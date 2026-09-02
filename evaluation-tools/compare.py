#!/usr/bin/env python3
"""
compare.py — one run + one reference -> one CSV row.

Usage:
  python3 evaluation-tools/compare.py output/bedroom_restir_spp64_20260805_143212 \
      references/bedroom.pfm evaluation-tools/bedroom_convergence.csv

Takes the run's full base path (no extension), the reference PFM's full path,
and the output CSV's full path — one CSV per comparison you are building.
Finds <base>.pfm, <base>_denoised.pfm (if present) and <base>.json by suffix
convention, computes every metric against the reference for both raw and
denoised, and appends one row carrying metrics + full provenance from the
run's json metadata.

All HDR metrics run on linear pre-tonemap float32, exactly as written by
FileHandler::writePFM. This script never renders, never overwrites, never
deletes. Tests: evaluation-tools/test_compare.py (pytest).
"""

import argparse
import csv
import json
import os

import flip_evaluator
import numpy as np
from skimage.metrics import structural_similarity

# Artifact-set suffix convention (must match FileHandler::saveOutputs).
RAW_PFM_SUFFIX      = ".pfm"
DENOISED_PFM_SUFFIX = "_denoised.pfm"
METADATA_SUFFIX     = ".json"

# PFM format tokens (must match FileHandler::writePFM).
PFM_COLOUR_HEADER = b"PF"

# CSV variant tags: metrics are emitted as <metric>_<tag>.
RAW_TAG      = "raw"
DENOISED_TAG = "dn"

# Extra provenance columns.
RUN_COLUMN       = "run"
REFERENCE_COLUMN = "reference"

# Sidecar keys that are annotations, not config axes — excluded from the CSV
# (they stay in the sidecar itself; a patched artifact would otherwise grow
# an extra column and trip the pinned-schema guard).
EXCLUDED_METADATA_KEYS = ("patch_note",)

# Part of relMSE's definition, not a tunable knob. Changing it invalidates
# comparability of every existing row.
REL_MSE_EPS = 1e-2

# HDR-FLIP consumes our linear PFMs directly (no tone-map choice needed).
FLIP_DYNAMIC_RANGE = "HDR"

# Hable filmic tone-map — exact port of Film::filmicCFunc/tonemap (film.cpp).
# SSIM is an LDR metric: both images go through THIS tone-map, always.
FILMIC_A = 0.15
FILMIC_B = 0.50
FILMIC_C = 0.10
FILMIC_D = 0.20
FILMIC_E = 0.02
FILMIC_F = 0.30
FILMIC_WHITE_POINT = 11.2
GAMMA = 1.0 / 2.2

# Rec. 709 luminance weights (matches Colour::lum in the renderer).
LUMINANCE_WEIGHTS = (0.2126, 0.7152, 0.0722)


# ---------------------------------------------------------------------------
# PFM input
# ---------------------------------------------------------------------------

def read_pfm(path):
    with open(path, "rb") as f:
        header = f.readline().strip()
        if header != PFM_COLOUR_HEADER:
            raise ValueError(f"{path}: not a colour PFM (header {header!r})")
        width, height = map(int, f.readline().split())
        scale = float(f.readline())
        dtype = "<f4" if scale < 0 else ">f4"
        data = np.fromfile(f, dtype, count=width * height * 3)
    if data.size != width * height * 3:
        raise ValueError(f"{path}: truncated body")
    return np.flipud(data.reshape(height, width, 3)).astype(np.float32)


# ---------------------------------------------------------------------------
# Metrics — tiny pure functions on (est, ref) float32 arrays.
# Later steps add entries to METRICS (ssim on tone-mapped output, flip via
# flip-evaluator);
# ---------------------------------------------------------------------------

def mse(est, ref):
    return float(np.mean((est - ref) ** 2))


def rmse(est, ref):
    return float(np.sqrt(mse(est, ref)))


def relmse(est, ref):
    return float(np.mean((est - ref) ** 2 / (ref ** 2 + REL_MSE_EPS)))


def flip(est, ref):
    # Mean HDR-FLIP error (0 = imperceptible, 1 = obvious). Maps: flipmap.py.
    _, mean_error, _ = flip_evaluator.evaluate(ref, est, FLIP_DYNAMIC_RANGE,
                                               applyMagma=False)
    return float(mean_error)


def filmic_curve(value):
    v1 = value * (FILMIC_A * value + FILMIC_C * FILMIC_B) + FILMIC_D * FILMIC_E
    v2 = value * (FILMIC_A * value + FILMIC_B) + FILMIC_D * FILMIC_F
    return v1 / v2 - FILMIC_E / FILMIC_F


def tonemap(image):
    # Clamp before gamma (the C++ clamps after; identical on [0, 1], and
    # clamping first avoids NaN on OIDN's occasional tiny negatives).
    mapped = filmic_curve(image) / filmic_curve(FILMIC_WHITE_POINT)
    return np.clip(mapped, 0.0, 1.0) ** GAMMA


def luminance(image):
    return image @ np.asarray(LUMINANCE_WEIGHTS, dtype=np.float32)


def ssim(est, ref):
    # Structural similarity (1 = identical structure) on tone-mapped luminance.
    return float(structural_similarity(luminance(tonemap(est)),
                                       luminance(tonemap(ref)),
                                       data_range=1.0))


METRICS = {
    "mse": mse,
    "rmse": rmse,
    "relmse": relmse,
    "flip": flip,
    "ssim": ssim
}


# ---------------------------------------------------------------------------
# Row assembly
# ---------------------------------------------------------------------------

def load_json_metadata(base):
    with open(base + METADATA_SUFFIX) as f:
        return json.load(f)


def flatten_json_metadata(metadata):
    flat = {}
    for key, value in metadata.items():
        if key in EXCLUDED_METADATA_KEYS:
            continue
        if isinstance(value, dict):
            for subkey, subvalue in value.items():
                flat[f"{key}_{subkey}"] = subvalue
        else:
            flat[key] = value
    return flat


def compare_run(base, reference_path, csv_path):
    reference = read_pfm(reference_path)

    row = flatten_json_metadata(load_json_metadata(base))
    row[RUN_COLUMN] = os.path.basename(base)
    row[REFERENCE_COLUMN] = os.path.basename(reference_path)

    variants = {RAW_TAG: base + RAW_PFM_SUFFIX}
    if os.path.exists(base + DENOISED_PFM_SUFFIX):
        variants[DENOISED_TAG] = base + DENOISED_PFM_SUFFIX

    for tag, path in variants.items():
        estimate = read_pfm(path)
        if estimate.shape != reference.shape:
            raise ValueError(f"{path}: shape {estimate.shape} != reference {reference.shape}")
        for name, fn in METRICS.items():
            row[f"{name}_{tag}"] = fn(estimate, reference)

    # Pinned schema: every metric column always exists (blank when the run has
    # no denoised pfm), so the header never depends on which row came first.
    metric_columns = [f"{name}_{tag}" for tag in (RAW_TAG, DENOISED_TAG) for name in METRICS]
    fieldnames = [key for key in row if key not in metric_columns] + metric_columns

    csv_exists = os.path.exists(csv_path)
    if csv_exists:
        with open(csv_path, newline="") as f:
            existing_header = next(csv.reader(f))
        if existing_header != fieldnames:
            raise SystemExit(
                f"{csv_path}: schema changed (existing header differs). "
                f"Move the old CSV aside and re-run compare.py on the runs you need.")
    with open(csv_path, "a", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames, restval="")
        if not csv_exists:
            writer.writeheader()
        writer.writerow(row)

    # Human-readable echo of what just landed in the CSV.
    print(f"run:       {row[RUN_COLUMN]}")
    print(f"reference: {row[REFERENCE_COLUMN]}")
    for tag in variants:
        print(f"  [{tag}]  " + "  ".join(f"{m}={row[f'{m}_{tag}']:.6g}" for m in METRICS))
    print(f"-> {csv_path}")

    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "run_base",
        help="full base path of the run, no extension "
        "(e.g. output/bedroom_restir_spp64_20260805_143212)"
    )
    parser.add_argument(
        "reference",
        help="full path of the reference PFM (e.g. references/bedroom.pfm)"
    )
    parser.add_argument(
        "csv",
        help="full path of the output CSV — one per comparison "
        "(e.g. evaluation-tools/bedroom_convergence.csv)"
    )
    args = parser.parse_args()

    compare_run(args.run_base, args.reference, args.csv)


if __name__ == "__main__":
    main()

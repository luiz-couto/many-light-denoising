#!/usr/bin/env python3
"""
errormap.py — per-pixel |estimate - reference| as a grayscale PNG.

Usage:
  python3 evaluation-tools/errormap.py <reference.pfm> <estimate.pfm> [<estimate.pfm> ...]

One PNG per estimate, written to FIGURES_DIR as <estimate-name>_errormap.png.
All maps share ONE normalization (the largest p99 across them), so brightness
is comparable between maps — black = matches reference, bright = wrong.
"""

import argparse
import os

import numpy as np
from PIL import Image

from compare import read_pfm

# ---------------------------------------------------------------------------
# Constants — no magic strings below this block.
# ---------------------------------------------------------------------------

EVALUATION_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
FIGURES_DIR = os.path.join(EVALUATION_TOOLS_DIR, "figures")

ERRORMAP_SUFFIX = "_errormap.png"

# Normalization percentile: p99 ignores the few hottest pixels so one firefly
# does not darken the entire map.
NORMALIZATION_PERCENTILE = 99


def error_map(estimate, reference):
    return np.abs(estimate - reference).mean(axis=2)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("reference", help="full path of the reference PFM")
    parser.add_argument("estimates", nargs="+", help="full paths of estimate PFMs")
    args = parser.parse_args()

    reference = read_pfm(args.reference)
    maps = {path: error_map(read_pfm(path), reference) for path in args.estimates}

    scale = max(np.percentile(m, NORMALIZATION_PERCENTILE) for m in maps.values())
    print(f"shared normalization: error {scale:.4g} -> white (p{NORMALIZATION_PERCENTILE})")

    os.makedirs(FIGURES_DIR, exist_ok=True)
    for path, errors in maps.items():
        name = os.path.basename(path).replace(".pfm", "")
        out = os.path.join(FIGURES_DIR, name + ERRORMAP_SUFFIX)
        pixels = (np.clip(errors / scale, 0.0, 1.0) * 255).astype(np.uint8)
        Image.fromarray(pixels).save(out)
        print(f"-> {out}")


if __name__ == "__main__":
    main()

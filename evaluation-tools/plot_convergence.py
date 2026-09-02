#!/usr/bin/env python3
"""
plot_convergence.py — convergence curves for one comparison CSV.

Usage:
  python3 evaluation-tools/plot_convergence.py evaluation-tools/bathroom_convergence.csv
  python3 evaluation-tools/plot_convergence.py evaluation-tools/bathroom_convergence.csv --metric rmse
  python3 evaluation-tools/plot_convergence.py <csv> --runs <run1> <run2>   # subset of the CSV

Takes the full path of a comparison CSV built by compare.py (one CSV per
comparison). Plots every row in it by default; --runs restricts to named rows
(the `run` column). Produces one chart per (variant, x-axis): raw-vs-spp,
raw-vs-seconds, denoised-vs-spp, denoised-vs-seconds — four PNGs per metric,
one line per integrator, log-log.
"""

import argparse
import csv
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import ticker

# ---------------------------------------------------------------------------
# Constants — no magic strings below this block.
# ---------------------------------------------------------------------------

EVALUATION_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
FIGURES_DIR = os.path.join(EVALUATION_TOOLS_DIR, "figures")

SCENE_COLUMN      = "scene"
INTEGRATOR_COLUMN = "integrator"
RUN_COLUMN        = "run"
SPP_COLUMN        = "spp"
SECONDS_COLUMN    = "render_seconds"

DEFAULT_METRIC = "relmse"
RAW_TAG        = "raw"
DENOISED_TAG   = "dn"

VARIANTS  = [RAW_TAG, DENOISED_TAG]
X_COLUMNS = [SPP_COLUMN, SECONDS_COLUMN]

INTEGRATOR_COLOURS = {"pt": "tab:blue", "ir": "tab:orange", "restir": "tab:green"}
INTEGRATOR_LABELS  = {"pt": "PT", "ir": "IR", "restir": "DWIR (Ours)"}
INTEGRATOR_ORDER   = ["pt", "ir", "restir"]
VARIANT_TITLES     = {RAW_TAG: "raw (no denoiser)", DENOISED_TAG: "denoised (OIDN)"}
X_AXIS_LABELS      = {SECONDS_COLUMN: "render time (s)", SPP_COLUMN: "samples per pixel"}

# Log-axis ticks at 1-2-5 per decade, labelled as plain numbers (max 3 digits).
LOG_TICK_SUBS = (1.0, 2.0, 5.0)


def format_log_axes(axis):
    for ax in (axis.xaxis, axis.yaxis):
        ax.set_major_locator(ticker.LogLocator(base=10, subs=LOG_TICK_SUBS))
        ax.set_major_formatter(ticker.FuncFormatter(lambda value, _: f"{value:.3g}"))
        ax.set_minor_locator(ticker.NullLocator())
        ax.set_minor_formatter(ticker.NullFormatter())


def load_rows(csv_path, run_names):
    with open(csv_path, newline="") as f:
        by_run = {row[RUN_COLUMN]: row for row in csv.DictReader(f)}
    if not by_run:
        raise SystemExit(f"{csv_path}: no rows")

    if not run_names:
        return list(by_run.values())

    missing = [name for name in run_names if name not in by_run]
    if missing:
        available = "\n  ".join(sorted(by_run))
        raise SystemExit(f"runs not found in {csv_path}:\n  " + "\n  ".join(missing)
                         + "\navailable runs:\n  " + available)
    return [by_run[name] for name in run_names]


def series_for(rows, integrator, metric, variant, x_column):
    """Sorted (x, metric) points for one line; skips rows lacking the column."""
    column = f"{metric}_{variant}"
    points = []
    for row in rows:
        if row[INTEGRATOR_COLUMN] != integrator:
            continue
        value = row.get(column, "")
        if value == "" or value is None:
            continue  # e.g. --no-denoise runs have no dn columns
        points.append((float(row[x_column]), float(value)))
    return sorted(points)


def plot_chart(rows, scene, figure_prefix, metric, variant, x_column):
    present = {row[INTEGRATOR_COLUMN] for row in rows}
    integrators = [i for i in INTEGRATOR_ORDER if i in present] \
                + sorted(present - set(INTEGRATOR_ORDER))

    fig, axis = plt.subplots(figsize=(6.5, 4.5))
    plotted = False
    for integrator in integrators:
        points = series_for(rows, integrator, metric, variant, x_column)
        if not points:
            continue
        xs, ys = zip(*points)
        axis.plot(xs, ys, "-o", color=INTEGRATOR_COLOURS.get(integrator, "tab:gray"),
                  label=INTEGRATOR_LABELS.get(integrator, integrator))
        plotted = True

    if not plotted:
        plt.close(fig)
        print(f"skip: no {metric}_{variant} data among the selected runs")
        return

    axis.set_xscale("log")
    axis.set_yscale("log")
    format_log_axes(axis)
    axis.set_xlabel(X_AXIS_LABELS[x_column])
    axis.set_ylabel(metric)
    axis.set_title(f"{scene} — {VARIANT_TITLES[variant]}")
    axis.grid(True, which="major", alpha=0.3)
    axis.legend()

    out = os.path.join(FIGURES_DIR, f"{figure_prefix}_{metric}_{variant}_{x_column}.png")
    fig.tight_layout()
    fig.savefig(out, dpi=150)
    plt.close(fig)
    print(f"-> {out}")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("csv", help="full path of the comparison CSV built by compare.py")
    parser.add_argument("--runs", nargs="*", default=[],
                        help="restrict to these run names (default: every row in the CSV)")
    parser.add_argument("--metric", default=DEFAULT_METRIC,
                        help=f"metric column prefix (default: {DEFAULT_METRIC})")
    args = parser.parse_args()

    rows = load_rows(args.csv, args.runs)
    scenes = sorted({row[SCENE_COLUMN] for row in rows})
    scene = "+".join(scenes)
    if len(scenes) > 1:
        print(f"warning: runs span multiple scenes ({scene}) — same chart, read with care")

    # Figures are named after the CSV, so a new comparison never overwrites
    # an older comparison's charts.
    figure_prefix = os.path.splitext(os.path.basename(args.csv))[0]

    os.makedirs(FIGURES_DIR, exist_ok=True)
    for variant in VARIANTS:
        for x_column in X_COLUMNS:
            plot_chart(rows, scene, figure_prefix, args.metric, variant, x_column)


if __name__ == "__main__":
    main()

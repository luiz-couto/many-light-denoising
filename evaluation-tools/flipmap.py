#!/usr/bin/env python3
"""
flipmap.py — per-pixel HDR-FLIP error map (magma colours) as a PNG.

Usage:
  python3 evaluation-tools/flipmap.py <reference.pfm> <estimate.pfm> [<estimate.pfm> ...]

One PNG per estimate, written to FIGURES_DIR as <estimate-name>_flipmap.png.
Dark = a viewer flipping between estimate and reference would not notice;
bright = they would. The mean FLIP value is printed per estimate.
"""

import argparse
import os

import flip_evaluator
import numpy as np
from PIL import Image

from compare import FLIP_DYNAMIC_RANGE, read_pfm

# ---------------------------------------------------------------------------
# Constants — no magic strings below this block.
# ---------------------------------------------------------------------------

EVALUATION_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
FIGURES_DIR = os.path.join(EVALUATION_TOOLS_DIR, "figures")

FLIPMAP_SUFFIX = "_flipmap.png"


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("reference", help="full path of the reference PFM")
    parser.add_argument("estimates", nargs="+", help="full paths of estimate PFMs")
    args = parser.parse_args()

    reference = read_pfm(args.reference)
    os.makedirs(FIGURES_DIR, exist_ok=True)

    for path in args.estimates:
        estimate = read_pfm(path)
        error_map, mean_error, _ = flip_evaluator.evaluate(reference, estimate,
                                                           FLIP_DYNAMIC_RANGE)
        name = os.path.basename(path).replace(".pfm", "")
        out = os.path.join(FIGURES_DIR, name + FLIPMAP_SUFFIX)
        Image.fromarray((np.clip(error_map, 0.0, 1.0) * 255).astype(np.uint8)).save(out)
        print(f"{name}: mean flip = {mean_error:.4f}")
        print(f"-> {out}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
ssimmap.py — per-pixel structural DISsimilarity (1 - local SSIM) as a PNG.

Usage:
  python3 evaluation-tools/ssimmap.py <reference.pfm> <estimate.pfm> [<estimate.pfm> ...]

One PNG per estimate, written to FIGURES_DIR as <estimate-name>_ssimmap.png.
Convention matches errormap.py and flipmap.py: black = structure matches the
reference, bright = structure damaged (blurred, lost, or invented). The mean
SSIM value is printed per estimate.

Computed on tone-mapped luminance, like the ssim column in compare.py.
"""

import argparse
import os

import numpy as np
from PIL import Image
from skimage.metrics import structural_similarity

from compare import luminance, read_pfm, tonemap

# ---------------------------------------------------------------------------
# Constants — no magic strings below this block.
# ---------------------------------------------------------------------------

EVALUATION_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
FIGURES_DIR = os.path.join(EVALUATION_TOOLS_DIR, "figures")

SSIMMAP_SUFFIX = "_ssimmap.png"

# Local SSIM lives in [-1, 1]; dissimilarity (1 - ssim) in [0, 2]. Values
# beyond 1 (anti-correlated structure) are clipped to white.
DISSIMILARITY_CLIP = 1.0


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("reference", help="full path of the reference PFM")
    parser.add_argument("estimates", nargs="+", help="full paths of estimate PFMs")
    args = parser.parse_args()

    reference = luminance(tonemap(read_pfm(args.reference)))
    os.makedirs(FIGURES_DIR, exist_ok=True)

    for path in args.estimates:
        estimate = luminance(tonemap(read_pfm(path)))
        mean_ssim, ssim_map = structural_similarity(estimate, reference,
                                                    data_range=1.0, full=True)
        dissimilarity = np.clip(1.0 - ssim_map, 0.0, DISSIMILARITY_CLIP)

        name = os.path.basename(path).replace(".pfm", "")
        out = os.path.join(FIGURES_DIR, name + SSIMMAP_SUFFIX)
        Image.fromarray((dissimilarity * 255).astype(np.uint8)).save(out)
        print(f"{name}: mean ssim = {mean_ssim:.4f}")
        print(f"-> {out}")


if __name__ == "__main__":
    main()

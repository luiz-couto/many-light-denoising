#!/usr/bin/env python3
"""
figuremaker.py — annotated full images + magnified crops for paper figures.

Usage:
  python3 evaluation-tools/figuremaker.py --crop X Y W H \
      selected-final-renders/bedroom_pt_spp1_*.png selected-final-renders/bedroom_restir_spp1_*.png

For each input PNG this writes two files next to each other in --out:
  <name>_annotated.png   the full image with a coloured rectangle at the crop
  <name>_crop.png        the crop region magnified with nearest-neighbour

One --crop rectangle applies to EVERY input, so compared methods always show
the pixel-exact same region — the convention comparison figures must follow.
Nearest-neighbour magnification preserves the noise character being shown;
bilinear smoothing would hide exactly what the figure is about.

This script never renders, never overwrites its inputs, never deletes.
Tests: evaluation-tools/test_figuremaker.py (pytest).
"""

import argparse
import os

from PIL import Image, ImageColor, ImageDraw

# ---------------------------------------------------------------------------
# Constants — no magic strings below this block.
# ---------------------------------------------------------------------------

EVALUATION_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
FIGURES_DIR = os.path.join(EVALUATION_TOOLS_DIR, "figures")

ANNOTATED_SUFFIX = "_annotated.png"
CROP_SUFFIX = "_crop.png"

DEFAULT_BOX_COLOR = "red"
DEFAULT_MAGNIFICATION = 4

# Rectangle outline thickness relative to image size, with a readable floor.
OUTLINE_FRACTION = 1.0 / 300.0
OUTLINE_MINIMUM_PIXELS = 2


def outline_width(image_width, image_height):
    return max(OUTLINE_MINIMUM_PIXELS,
               round(min(image_width, image_height) * OUTLINE_FRACTION))


def annotate(image, crop_box, color):
    # crop_box = (left, top, right, bottom), PIL convention.
    annotated = image.copy()
    draw = ImageDraw.Draw(annotated)
    draw.rectangle(crop_box, outline=color,
                   width=outline_width(image.width, image.height))
    return annotated


def magnify_crop(image, crop_box, magnification):
    crop = image.crop(crop_box)
    return crop.resize((crop.width * magnification, crop.height * magnification),
                       Image.NEAREST)


def output_paths(input_path, out_dir):
    stem = os.path.splitext(os.path.basename(input_path))[0]
    return (os.path.join(out_dir, stem + ANNOTATED_SUFFIX),
            os.path.join(out_dir, stem + CROP_SUFFIX))


def make_figure_pair(input_path, crop_box, color, magnification, out_dir):
    image = Image.open(input_path).convert("RGB")

    left, top, right, bottom = crop_box
    if not (0 <= left < right <= image.width and 0 <= top < bottom <= image.height):
        raise SystemExit(f"{input_path}: crop {crop_box} outside image "
                         f"{image.width}x{image.height}")

    annotated_path, crop_path = output_paths(input_path, out_dir)
    annotate(image, crop_box, color).save(annotated_path)
    magnify_crop(image, crop_box, magnification).save(crop_path)
    print(f"-> {annotated_path}")
    print(f"-> {crop_path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "images",
        nargs="+",
        help="input PNGs (renders, error maps); the same crop is applied to all"
    )
    parser.add_argument(
        "--crop",
        nargs=4,
        type=int,
        required=True,
        metavar=("X", "Y", "W", "H"),
        help="crop rectangle: left, top, width, height, in pixels of the input"
    )
    parser.add_argument(
        "--color",
        default=DEFAULT_BOX_COLOR,
        help=f"rectangle colour, any PIL colour name or #rrggbb (default {DEFAULT_BOX_COLOR})"
    )
    parser.add_argument(
        "--magnification",
        type=int,
        default=DEFAULT_MAGNIFICATION,
        help=f"nearest-neighbour upscale factor for the crop (default {DEFAULT_MAGNIFICATION})"
    )
    parser.add_argument(
        "--out",
        default=FIGURES_DIR,
        help="output directory (default evaluation-tools/figures)"
    )
    args = parser.parse_args()

    color = ImageColor.getrgb(args.color)
    x, y, w, h = args.crop
    crop_box = (x, y, x + w, y + h)

    os.makedirs(args.out, exist_ok=True)
    for path in args.images:
        make_figure_pair(path, crop_box, color, args.magnification, args.out)


if __name__ == "__main__":
    main()

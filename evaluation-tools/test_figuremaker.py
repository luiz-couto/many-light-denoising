"""Tests for figuremaker.py — pytest."""

import numpy as np
from PIL import Image

from figuremaker import (annotate, magnify_crop, outline_width,
                         output_paths, OUTLINE_MINIMUM_PIXELS,
                         ANNOTATED_SUFFIX, CROP_SUFFIX)

BOX_COLOR = (255, 0, 0)


def solid_image(width, height, value):
    return Image.fromarray(np.full((height, width, 3), value, dtype=np.uint8))


def test_crop_content_matches_source_region():
    # Distinct pixel values encode position; the magnified crop must contain
    # exactly the requested region, each pixel repeated magnification times.
    gradient = np.arange(16 * 16, dtype=np.uint8).reshape(16, 16)
    image = Image.fromarray(np.stack([gradient] * 3, axis=2))
    crop_box = (4, 2, 8, 6)

    crop = np.asarray(magnify_crop(image, crop_box, 2))

    expected = np.asarray(image)[2:6, 4:8].repeat(2, axis=0).repeat(2, axis=1)
    assert np.array_equal(crop, expected)


def test_annotate_draws_rectangle_at_crop_and_leaves_inside_untouched():
    image = solid_image(100, 100, 40)
    crop_box = (20, 30, 60, 70)

    annotated = np.asarray(annotate(image, crop_box, BOX_COLOR))
    width = outline_width(100, 100)

    # On the outline: box colour. Strictly inside the outline: original value.
    assert tuple(annotated[30, 40]) == BOX_COLOR
    assert tuple(annotated[50, 20]) == BOX_COLOR
    inside = annotated[30 + width:70 - width, 20 + width:60 - width]
    assert (inside == 40).all()


def test_annotate_does_not_modify_input_image():
    image = solid_image(50, 50, 40)
    annotate(image, (10, 10, 30, 30), BOX_COLOR)
    assert (np.asarray(image) == 40).all()


def test_outline_width_has_readable_floor_on_small_images():
    assert outline_width(64, 64) == OUTLINE_MINIMUM_PIXELS


def test_output_paths_use_stem_and_suffixes():
    annotated, crop = output_paths("output/bedroom_pt_spp1.png", "figs")
    assert annotated == "figs/bedroom_pt_spp1" + ANNOTATED_SUFFIX
    assert crop == "figs/bedroom_pt_spp1" + CROP_SUFFIX

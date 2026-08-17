"""Known-answer tests for compare.py. Run: pytest evaluation-tools/"""

import json

import numpy as np
import pytest

from compare import (METRICS, DENOISED_PFM_SUFFIX, METADATA_SUFFIX, RAW_PFM_SUFFIX,
                     REFERENCE_COLUMN, RUN_COLUMN, compare_run, flatten_json_metadata,
                     read_pfm, relmse, rmse)


def write_pfm(path, image):
    """Test-side twin of FileHandler::writePFM; keeps the round trip honest."""
    height, width, _ = image.shape
    with open(path, "wb") as f:
        f.write(f"PF\n{width} {height}\n-1.0\n".encode())
        np.flipud(image).astype("<f4").tofile(f)


def make_reference(rng, shape=(8, 8, 3)):
    return rng.uniform(0.1, 5.0, shape).astype(np.float32)


# ---------------------------------------------------------------------------
# PFM round trip
# ---------------------------------------------------------------------------

def test_pfm_roundtrip_bit_exact(tmp_path):
    # HDR, tiny, and awkward values must survive untouched — the C++ tests pin
    # the writer to this same layout, so agreement here is transitive to C++.
    rng = np.random.default_rng(7)
    image = rng.uniform(0, 10, (3, 2, 3)).astype(np.float32)
    image[0, 0] = (0.1, 1e30, 1e-30)
    path = tmp_path / "roundtrip.pfm"
    write_pfm(path, image)
    assert np.array_equal(read_pfm(path), image)


def test_read_pfm_rejects_non_pfm(tmp_path):
    path = tmp_path / "not_a_pfm.pfm"
    path.write_bytes(b"P6\n2 2\n255\n" + bytes(12))
    with pytest.raises(ValueError):
        read_pfm(path)


# ---------------------------------------------------------------------------
# Metrics — known answers
# ---------------------------------------------------------------------------

def test_metrics_perfect_on_identical():
    # Error metrics score 0 on identical images; similarity metrics score 1.
    perfect_scores = {"ssim": 1.0}
    ref = make_reference(np.random.default_rng(7))
    for name, fn in METRICS.items():
        expected = perfect_scores.get(name, 0.0)
        assert fn(ref, ref) == pytest.approx(expected, abs=1e-4), name


def test_rmse_exact_on_constant_offset():
    ref = make_reference(np.random.default_rng(7))
    assert rmse(ref + 0.5, ref) == pytest.approx(0.5, abs=1e-6)


def test_firefly_dominates_rmse():
    # ONE pixel off by 1000 outweighs EVERY pixel off by 0.1 — the reason
    # relMSE exists.
    black = np.zeros((100, 100, 3), np.float32)
    hot = black.copy()
    hot[0, 0, 0] = 1000.0
    assert rmse(hot, black) > rmse(black + 0.1, black)


def test_relmse_weighs_dark_errors_heavier():
    # Same absolute error: glaring in a dark region, invisible in a bright one.
    dark = np.full((2, 2, 3), 0.1, np.float32)
    bright = np.full((2, 2, 3), 10.0, np.float32)
    assert relmse(dark + 0.05, dark) > 100 * relmse(bright + 0.05, bright)


def test_ssim_decreases_with_noise():
    # NOTE (measured 2026-08-06, bathroom table): SSIM heavily punishes dense
    # noise — on RAW images it prefers ir's smooth-with-dots over pt's grain,
    # so ssim_raw is not a meaningful column; quote SSIM on DENOISED output
    # only, where it measures preserved detail vs false structure.
    from compare import ssim
    rng = np.random.default_rng(7)
    base = make_reference(rng, shape=(64, 64, 3))
    mild = base + rng.normal(0.0, 0.05, base.shape).astype(np.float32)
    heavy = base + rng.normal(0.0, 0.3, base.shape).astype(np.float32)
    assert 1.0 > ssim(mild, base) > ssim(heavy, base)


def test_tonemap_matches_filmic_anchors():
    # Exact port of Film::tonemap: black -> 0, the white point -> 1.
    from compare import FILMIC_WHITE_POINT, tonemap
    assert tonemap(np.float32(0.0)) == pytest.approx(0.0, abs=1e-6)
    assert tonemap(np.float32(FILMIC_WHITE_POINT)) == pytest.approx(1.0, abs=1e-5)


def test_flip_near_zero_on_identical_and_orders_by_visibility():
    from compare import flip
    rng = np.random.default_rng(7)
    ref = make_reference(rng, shape=(64, 64, 3))
    assert flip(ref, ref) < 1e-4
    # A grossly wrong image must score worse than a slightly wrong one.
    slightly = (ref * 1.02).astype(np.float32)
    grossly = (ref * 3.0).astype(np.float32)
    assert flip(grossly, ref) > flip(slightly, ref)


# ---------------------------------------------------------------------------
# Metadata flattening
# ---------------------------------------------------------------------------

def test_flatten_json_metadata_prefixes_nested_blocks():
    metadata = {"scene": "bedroom", "spp": 7, "pt": {"max_depth": 20}, "ir": {"m": 32}}
    flat = flatten_json_metadata(metadata)
    assert flat == {"scene": "bedroom", "spp": 7, "pt_max_depth": 20, "ir_m": 32}


# ---------------------------------------------------------------------------
# compare_run end to end (synthetic artifact set, no renderer needed)
# ---------------------------------------------------------------------------

def make_artifact_set(tmp_path, rng, with_denoised=True):
    reference = make_reference(rng)
    base = str(tmp_path / "scene_restir_spp4_20260805_120000")
    write_pfm(base + RAW_PFM_SUFFIX, reference + 0.5)
    if with_denoised:
        write_pfm(base + DENOISED_PFM_SUFFIX, reference + 0.25)
    metadata = {"scene": "scene", "integrator": "restir", "spp": 4,
                "pt": {"max_depth": 20}}
    with open(base + METADATA_SUFFIX, "w") as f:
        json.dump(metadata, f)
    reference_path = str(tmp_path / "reference.pfm")
    write_pfm(reference_path, reference)
    return base, reference_path


def test_compare_run_row_carries_metrics_and_provenance(tmp_path):
    base, reference_path = make_artifact_set(tmp_path, np.random.default_rng(7))
    row = compare_run(base, reference_path, str(tmp_path / "results.csv"))

    assert row["rmse_raw"] == pytest.approx(0.5, abs=1e-6)
    assert row["rmse_dn"] == pytest.approx(0.25, abs=1e-6)
    assert row["pt_max_depth"] == 20
    assert row[RUN_COLUMN] == "scene_restir_spp4_20260805_120000"
    assert row[REFERENCE_COLUMN] == "reference.pfm"
    assert (tmp_path / "results.csv").exists()


def test_compare_run_without_denoised_pfm(tmp_path):
    # --no-denoise runs produce no _denoised.pfm; the row simply has no dn columns.
    base, reference_path = make_artifact_set(tmp_path, np.random.default_rng(7),
                                             with_denoised=False)
    row = compare_run(base, reference_path, str(tmp_path / "results.csv"))

    assert "rmse_raw" in row
    assert "rmse_dn" not in row

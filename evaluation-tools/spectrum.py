#!/usr/bin/env python3
"""
spectrum.py — radially-averaged power spectrum of error images.

Usage:
  python3 evaluation-tools/spectrum.py --out figures/bathroom_spectra.png \
      <reference.pfm> <estimate.pfm> [<estimate.pfm> ...]

For each estimate: error = luminance(estimate) - luminance(reference), Hann
window (suppresses FFT edge leakage), 2D FFT, power |F|^2, averaged over rings
of equal spatial frequency. All curves overlaid on one log-log chart:
flat = white noise, low-frequency hump = correlated structures (blobs/dots),
rising = blue noise. Also prints spectral flatness per estimate
(geometric mean / arithmetic mean of the radial profile: 1 = perfectly white).
"""

import argparse
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from compare import luminance, read_pfm

# ---------------------------------------------------------------------------
# Constants — no magic strings below this block.
# ---------------------------------------------------------------------------

# Lowest ring plotted; ring 0 is the DC term (the mean error), which is about
# bias, not noise arrangement, and would dwarf the log plot.
MIN_FREQUENCY_RING = 1


def radial_power_spectrum(error_image):
    """Radially-averaged power spectrum of a 2D error image.

    Returns (frequencies, mean power per ring); frequency k = ring index,
    in cycles per image (k=1 -> structures the size of the whole image,
    k=N/2 -> pixel-scale structures).
    """
    height, width = error_image.shape
    window = np.outer(np.hanning(height), np.hanning(width))
    spectrum = np.fft.fftshift(np.fft.fft2(error_image * window))
    power = np.abs(spectrum) ** 2

    cy, cx = height // 2, width // 2
    ys, xs = np.indices(power.shape)
    ring = np.sqrt((ys - cy) ** 2 + (xs - cx) ** 2).astype(np.int64)

    max_ring = min(cy, cx)
    ring_power = np.bincount(ring.ravel(), weights=power.ravel())
    ring_count = np.bincount(ring.ravel())
    profile = ring_power[:max_ring] / ring_count[:max_ring]

    frequencies = np.arange(MIN_FREQUENCY_RING, max_ring)
    return frequencies, profile[MIN_FREQUENCY_RING:]


def spectral_flatness(profile):
    """Geometric mean / arithmetic mean of the radial profile: 1 = white."""
    positive = profile[profile > 0]
    return float(np.exp(np.mean(np.log(positive))) / np.mean(positive))


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", required=True,
                        help="full path of the output chart PNG")
    parser.add_argument("reference", help="full path of the reference PFM")
    parser.add_argument("estimates", nargs="+", help="full paths of estimate PFMs")
    args = parser.parse_args()

    reference = luminance(read_pfm(args.reference))

    fig, axis = plt.subplots(figsize=(6.5, 4.5))
    for path in args.estimates:
        estimate = luminance(read_pfm(path))
        if estimate.shape != reference.shape:
            raise SystemExit(f"{path}: shape {estimate.shape} != reference {reference.shape}")
        frequencies, profile = radial_power_spectrum(estimate - reference)
        label = os.path.basename(path).replace(".pfm", "")
        axis.plot(frequencies, profile, label=label)
        print(f"{label}: spectral flatness = {spectral_flatness(profile):.3f}  (1 = white)")

    axis.set_xscale("log")
    axis.set_yscale("log")
    axis.set_xlabel("spatial frequency (cycles per image)")
    axis.set_ylabel("error power (radial mean)")
    axis.set_title("error power spectrum")
    axis.grid(True, which="major", alpha=0.3)
    axis.legend(fontsize=8)

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    fig.tight_layout()
    fig.savefig(args.out, dpi=150)
    print(f"-> {args.out}")


if __name__ == "__main__":
    main()

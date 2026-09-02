"""Known-answer tests for spectrum.py. Run: pytest evaluation-tools/"""

import numpy as np

from spectrum import radial_power_spectrum, spectral_flatness


def test_white_noise_has_flat_spectrum():
    rng = np.random.default_rng(7)
    error = rng.normal(0.0, 1.0, (128, 128)).astype(np.float32)
    _, profile = radial_power_spectrum(error)
    assert spectral_flatness(profile) > 0.8   # near 1 = white


def test_large_blobs_concentrate_power_at_low_frequencies():
    # Smooth blobs the size of ~1/8 image -> energy in the lowest rings,
    # little at pixel scale. Built from heavily blurred noise.
    rng = np.random.default_rng(7)
    noise = rng.normal(0.0, 1.0, (128, 128))
    kernel = np.outer(np.hanning(33), np.hanning(33))
    blobs = np.fft.irfft2(np.fft.rfft2(noise) * np.fft.rfft2(kernel, noise.shape))
    frequencies, profile = radial_power_spectrum(blobs.astype(np.float32))
    low = profile[frequencies <= 8].mean()
    high = profile[frequencies >= 32].mean()
    assert low > 100 * high                    # the low-frequency hump
    assert spectral_flatness(profile) < 0.2    # decisively non-white


def test_sine_grating_spikes_at_its_frequency():
    # Vertical grating with 8 cycles across the image -> spike at ring ~8.
    # Calibrates the frequency axis.
    size = 128
    xs = np.arange(size)
    grating = np.sin(2.0 * np.pi * 8 * xs / size)
    error = np.tile(grating, (size, 1)).astype(np.float32)
    frequencies, profile = radial_power_spectrum(error)
    peak_frequency = frequencies[np.argmax(profile)]
    assert abs(int(peak_frequency) - 8) <= 1

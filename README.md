# many-light-denoising

Making many-light rendering algorithms denoiser-friendly. This repository contains the renderer and evaluation pipeline for **DWIR (Denoiser-Aware Instant Radiosity)**: a path tracer (`pt`), standard Instant Radiosity (`ir`), and our ReSTIR-based variant (`restir`).

## macOS Setup

### Dependencies

```bash
brew install sdl2 llvm
```

### Build

```bash
cmake -B build -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build build
```

### Run

```bash
./build/renderer <scene-name> <pt|ir|restir> [flags]
```

Scene folders live under `scenes/`. Pass just the folder name, not the full path:

```bash
./build/renderer kitchen restir --spp 64
./build/renderer bathroom pt --spp 256 --snapshot log
./build/renderer bedroom ir --spp 64 --ir-paths 128 --ir-depth 4
```

With no `--spp`, the renderer runs as an endless interactive loop. Press `S` at any time to save the current state as a full artifact set (see Outputs below).

### Flags

| Flag | Meaning | Default |
|---|---|---|
| `--spp N` | Render N samples per pixel, save the final artifacts, and exit | 0 (interactive) |
| `--no-denoise` | Disable OIDN; display and save only the raw output | denoiser on |
| `--snapshot N` | Additionally save artifacts every N spp | off |
| `--snapshot log` | Additionally save artifacts at power-of-two spp (1, 2, 4, ...) — log-spaced convergence data | off |
| `--ir-paths N` | Photon paths per pass for `ir`/`restir` (classical IR baseline: 128) | 8192 |
| `--ir-depth N` | Maximum photon bounce depth (classical IR baseline: 4) | 100 |
| `--ir-jitter on\|off` | Decoupled shading: jitter the shadow-ray target over the VPL's footprint disk (ablation flag) | on |
| `--restir-rounds N` | Spatial reuse rounds for `restir`; 0 disables spatial reuse (ablation flag) | 1 |

Remaining parameters (reservoir size M, reuse neighbours K and radius, G-clamp, footprint fraction, PT depth) are compile-time constants in `include/config.h`.

### Outputs

Every save writes a self-describing artifact set to `output/`, named `<scene>_<integrator>_spp<N>_<timestamp>`:

- `<base>.pfm` / `<base>.png` — raw accumulated output (linear HDR / tone-mapped)
- `<base>_denoised.pfm` / `<base>_denoised.png` — OIDN output (when the denoiser is on)
- `<base>.json` — sidecar recording the full configuration of the run (scene, integrator, spp, render seconds, and every algorithm parameter)

All metrics are computed on the linear PFMs. Runs are deterministic given the configuration.

## Evaluation tools

Python tools live in `evaluation-tools/` (see `requirements.txt`; metrics run on the PFMs):

- `compare.py <run-base> <reference.pfm> <out.csv>` — computes MSE, RMSE, relMSE, SSIM, and HDR-FLIP for the raw and denoised outputs of one run and appends a row (with full provenance) to a CSV.
- `plot_convergence.py <csv>` — convergence charts (raw/denoised × spp/render-time, log-log) from a comparison CSV.
- `spectrum.py` — radially averaged error power spectrum and spectral flatness (the whitening measurement).
- `errormap.py`, `flipmap.py`, `ssimmap.py` — per-pixel error, HDR-FLIP, and structural dissimilarity maps.
- `figuremaker.py` — annotated full images plus magnified crops for paper figures.

Ground-truth references (high-spp path-traced PFMs, one per scene) live in `evaluation-tools/references/`.

## Tests

The unit test suite (Catch2) builds alongside the renderer:

```bash
./build/tests
```

It includes analytical known-answer tests for the estimators, chi-square tests for the samplers, and energy-conservation tests for the photon pass.

## VS Code

Install the [clangd extension](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd) for correct C++23 IntelliSense. When prompted, disable IntelliSense from the Microsoft C/C++ extension.

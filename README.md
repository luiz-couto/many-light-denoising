# many-light-denoising
Making many-light rendering algorithms denoiser-friendly

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
./build/renderer <scene-name>
```

Scene folders live under `scenes/`. Pass just the folder name, not the full path:

```bash
./build/renderer kitchen
./build/renderer cornell-box
```

Press `S` to save the current frame as `output.png`.

Pass `--no-denoise` to disable OIDN denoising and display the raw path-traced instead:

```bash
./build/renderer kitchen --no-denoise
```

### VS Code

Install the [clangd extension](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd) for correct C++23 IntelliSense. When prompted, disable IntelliSense from the Microsoft C/C++ extension.

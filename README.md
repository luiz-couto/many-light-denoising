# many-light-denoising
Making many-light rendering algorithms denoiser-friendly

## macOS Setup

### Dependencies

```bash
brew install sdl2 openimagedenoise llvm
```

### Build

```bash
cmake -B build -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build build
```

### VS Code

Install the [clangd extension](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd) for correct C++23 IntelliSense. When prompted, disable IntelliSense from the Microsoft C/C++ extension.

clangd reads `build/compile_commands.json` automatically — no extra configuration needed.

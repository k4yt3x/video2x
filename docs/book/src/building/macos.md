# macOS

Instructions for building this project natively on macOS, including Apple
Silicon (M1/M2/M3/M4).

On macOS there is no native Vulkan driver; [MoltenVK](https://github.com/KhronosGroup/MoltenVK)
provides a Vulkan implementation on top of Apple's Metal API. ncnn is built from
the bundled submodule with Vulkan enabled so the Real-ESRGAN, Real-CUGAN, and
RIFE models run on the GPU.

## Dependencies

Install the build and runtime dependencies with [Homebrew](https://brew.sh):

```bash
brew update
brew install cmake ninja pkgconf ffmpeg boost \
    molten-vk vulkan-headers vulkan-loader glslang libomp
```

## Building

The repository's `.justfile` provides a `macos` recipe that installs the
dependencies, configures, and builds the project:

```bash
# Clone with submodules (ncnn, spdlog, etc.)
git clone --recurse-submodules https://github.com/k4yt3x/video2x.git
cd video2x

# Build (installs Homebrew dependencies automatically)
just macos
```

The built binaries are placed in `build/video2x-install`.

### Building manually

```bash
cmake -G Ninja -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DVIDEO2X_USE_EXTERNAL_NCNN=OFF \
    -DVIDEO2X_USE_EXTERNAL_SPDLOG=OFF \
    -DCMAKE_PREFIX_PATH="$(brew --prefix)" \
    -DOpenMP_ROOT="$(brew --prefix libomp)" \
    -DVulkan_INCLUDE_DIR="$(brew --prefix vulkan-headers)/include" \
    -DVulkan_LIBRARY="$(brew --prefix vulkan-loader)/lib/libvulkan.dylib" \
    -DCMAKE_INSTALL_PREFIX=build/video2x-install
cmake --build build --config Release --parallel --target install
```

## Running

The Vulkan loader must find the MoltenVK driver manifest (ICD). Point it at the
one Homebrew installs:

```bash
export VK_ICD_FILENAMES="$(brew --prefix molten-vk)/share/vulkan/icd.d/MoltenVK_icd.json"

# List detected GPUs (you should see your Apple GPU)
./build/video2x --list-devices

# Upscale a clip 4x with Real-ESRGAN
./build/video2x \
    -i input.mp4 -o output.mp4 \
    -p realesrgan -s 4 --realesrgan-model realesr-generalv3
```

Run the binary from the `build/` directory: CMake gives the build-tree
executable the rpaths it needs to find `libvideo2x.dylib` and the bundled ncnn
model libraries. (The `install` tree does not yet set macOS rpaths for a fully
relocatable bundle -- see *Known limitations*.)

> Note: MoltenVK implements Vulkan as a *portability* subset. Video2X requests
> the `VK_KHR_portability_enumeration` instance extension on macOS so the loader
> exposes the Metal-backed device.

## Troubleshooting

**`Failed to create Vulkan instance` / loader reports
`Failed to open JSON file ...MoltenVK_icd.json`.** Some Homebrew `molten-vk`
revisions ship a dangling symlink for the ICD manifest, so the loader finds the
path but cannot read it. Confirm with:

```bash
ls -lL "$(brew --prefix molten-vk)/share/vulkan/icd.d/MoltenVK_icd.json"  # "No such file" => broken
```

Fix it by writing a private ICD that points straight at the real dylib:

```bash
D="$(brew --prefix molten-vk)/lib/libMoltenVK.dylib"
mkdir -p ~/.config/vulkan/icd.d
printf '{"file_format_version":"1.0.0","ICD":{"library_path":"%s","api_version":"1.2.0"}}\n' "$D" \
    > ~/.config/vulkan/icd.d/MoltenVK_icd.json
export VK_ICD_FILENAMES="$HOME/.config/vulkan/icd.d/MoltenVK_icd.json"
```

`~/.config/vulkan/icd.d` is also a default loader search path, so this keeps
working in new shells. `brew reinstall molten-vk` may also restore the manifest.

## Known limitations

- The `install` target installs `libvideo2x.dylib` but does not yet rewrite macOS
  rpaths or bundle the ncnn model libraries and Homebrew dependencies, so the
  installed binary is not relocatable. Run from the `build/` directory, or set
  `DYLD_FALLBACK_LIBRARY_PATH` to the install `lib/` directory. A self-contained
  `.app`/bundle recipe (analogous to the Linux AppImage target) is future work.

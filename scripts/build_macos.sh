#!/usr/bin/env bash
# Build Video2X natively on Apple Silicon (ncnn over MoltenVK + optional Core ML).
# Usage: scripts/build_macos.sh   then   source build/env.sh   to run build/video2x
set -euo pipefail
cd "$(dirname "$0")/.."

brew install cmake ninja pkgconf ffmpeg molten-vk vulkan-loader vulkan-headers glslang \
    shaderc libplacebo boost libomp
git submodule update --init --recursive --depth 1

P=$(brew --prefix)
export PKG_CONFIG_PATH="$P/lib/pkgconfig"
# The project's CMake drops pkg-config -L/-I paths, so pass the Homebrew prefix explicitly
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DVIDEO2X_USE_EXTERNAL_NCNN=OFF -DVIDEO2X_USE_EXTERNAL_SPDLOG=OFF -DVIDEO2X_ENABLE_NATIVE=ON \
    -DCMAKE_PREFIX_PATH="$P" -DCMAKE_CXX_FLAGS="-I$P/include" \
    -DCMAKE_SHARED_LINKER_FLAGS="-L$P/lib" -DCMAKE_EXE_LINKER_FLAGS="-L$P/lib"
cmake --build build

# MoltenVK must be found by the Vulkan loader at runtime
cat > build/env.sh <<EOF
export DYLD_LIBRARY_PATH="$P/lib\${DYLD_LIBRARY_PATH:+:\$DYLD_LIBRARY_PATH}"
export VK_ICD_FILENAMES="$P/etc/vulkan/icd.d/MoltenVK_icd.json"
EOF
echo "Built build/video2x. Run: source build/env.sh && build/video2x -l"
echo "Optional Core ML models: see tools/coreml/convert_realesrgan.py"

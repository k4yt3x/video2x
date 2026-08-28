#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${VIDEO2X_BUILD_DIR:-"$project_dir/build-macos"}
install_dir=${VIDEO2X_INSTALL_DIR:-"$project_dir/dist/macos-$(uname -m)"}

cmake -G Ninja -S "$project_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$install_dir" \
    -DCMAKE_INSTALL_RPATH="@executable_path/../lib;/opt/homebrew/lib" \
    -DVIDEO2X_USE_EXTERNAL_NCNN=OFF \
    -DVIDEO2X_USE_EXTERNAL_SPDLOG=OFF \
    -DVIDEO2X_USE_EXTERNAL_BOOST=ON \
    -DVIDEO2X_ENABLE_NATIVE=ON

cmake --build "$build_dir" --parallel
cmake --install "$build_dir"

printf 'Video2X installed in %s\n' "$install_dir"

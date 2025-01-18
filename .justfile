# Use PowerShell to run recipes on Windows
set windows-shell := ['pwsh', '-Command']

# Default build directory, generator, and C++ compiler
bindir := "build"
generator := "Ninja"
cxx := "clang++"

# Test video and output paths
test_video := "data/standard-test.mp4"
test_output := "data/output.mp4"

[unix]
[group('build')]
build:
    cmake -G '{{generator}}' -S . -B {{bindir}} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_CXX_COMPILER={{cxx}} \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX={{bindir}}/video2x-install \
        -DVIDEO2X_ENABLE_NATIVE=ON
    cmake --build {{bindir}} --config Release --parallel --target install

[windows]
[group('build')]
build:
    cmake -S . -B {{bindir}} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX={{bindir}}/video2x-install \
        -DCMAKE_INSTALL_BINDIR="." \
        -DCMAKE_INSTALL_LIBDIR="." \
        -DCMAKE_INSTALL_INCLUDEDIR=include \
        -DCMAKE_INSTALL_DATADIR="." \
        -DVIDEO2X_USE_EXTERNAL_NCNN=OFF \
        -DVIDEO2X_USE_EXTERNAL_SPDLOG=OFF \
        -DVIDEO2X_USE_EXTERNAL_BOOST=OFF
    cmake --build {{bindir}} --config Release --parallel --target install

[unix]
[group('build')]
static:
    cmake -G '{{generator}}' -S . -B {{bindir}} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_CXX_COMPILER={{cxx}} \
        -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX={{bindir}}/video2x-install \
        -DVIDEO2X_USE_EXTERNAL_NCNN=OFF \
        -DVIDEO2X_USE_EXTERNAL_SPDLOG=OFF \
        -DVIDEO2X_USE_EXTERNAL_BOOST=OFF
    cmake --build {{bindir}} --config Release --parallel --target install

[unix]
[group('build')]
debug:
    cmake -G '{{generator}}' -S . -B {{bindir}} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_CXX_COMPILER={{cxx}} \
        -DCMAKE_BUILD_TYPE=Debug
    cmake --build {{bindir}} --config Debug --parallel

[windows]
[group('build')]
debug:
    cmake -S . -B {{bindir}} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_INSTALL_PREFIX={{bindir}}/video2x-install \
        -DVIDEO2X_USE_EXTERNAL_NCNN=OFF \
        -DVIDEO2X_USE_EXTERNAL_SPDLOG=OFF \
        -DVIDEO2X_USE_EXTERNAL_BOOST=OFF
    cmake --build {{bindir}} --config Debug --parallel

[unix]
[group('build')]
debian:
    apt-get update
    apt-get install -y --no-install-recommends \
        build-essential cmake clang pkg-config ninja-build \
        libavcodec-dev \
        libavdevice-dev \
        libavfilter-dev \
        libavformat-dev \
        libavutil-dev \
        libswscale-dev \
        libvulkan-dev \
        glslang-tools \
        libomp-dev \
        libspdlog-dev \
        libboost-program-options-dev
    cmake -G '{{generator}}' -B /tmp/build -S . \
        -DVIDEO2X_USE_EXTERNAL_NCNN=OFF \
        -DCMAKE_CXX_COMPILER={{cxx}} \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/tmp/install \
        -DINSTALL_BIN_DESTINATION=. \
        -DINSTALL_INCLUDE_DESTINATION=include \
        -DINSTALL_LIB_DESTINATION=. \
        -DINSTALL_MODEL_DESTINATION=.
    cmake --build /tmp/build --config Release --target install --parallel

[unix]
[group('build')]
ubuntu2404:
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential cmake pkg-config ninja-build \
        libavcodec-dev \
        libavdevice-dev \
        libavfilter-dev \
        libavformat-dev \
        libavutil-dev \
        libswscale-dev \
        libvulkan-dev \
        glslang-tools \
        libomp-dev \
        libboost-program-options-dev
    cmake -G '{{generator}}' -B build -S . \
        -DVIDEO2X_USE_EXTERNAL_NCNN=OFF \
        -DVIDEO2X_USE_EXTERNAL_SPDLOG=OFF \
        -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=video2x-linux-ubuntu-amd64/usr
    cmake --build build --config Release --target install --parallel
    mkdir -p video2x-linux-ubuntu-amd64/DEBIAN
    cp packaging/debian/control.ubuntu2404 video2x-linux-ubuntu-amd64/DEBIAN/control
    dpkg-deb --build video2x-linux-ubuntu-amd64

[unix]
[group('build')]
ubuntu2204:
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y software-properties-common
    add-apt-repository -y ppa:ubuntuhandbook1/ffmpeg7
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build \
        libavcodec-dev \
        libavdevice-dev \
        libavfilter-dev \
        libavformat-dev \
        libavutil-dev \
        libswscale-dev \
        libvulkan-dev \
        glslang-tools \
        libomp-dev \
        libboost-program-options-dev
    cmake -G '{{generator}}' -B build -S . \
        -DVIDEO2X_USE_EXTERNAL_NCNN=OFF \
        -DVIDEO2X_USE_EXTERNAL_SPDLOG=OFF \
        -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=video2x-linux-ubuntu-amd64/usr
    cmake --build build --config Release --target install --parallel
    mkdir -p video2x-linux-ubuntu-amd64/DEBIAN
    cp packaging/debian/control.ubuntu2204 video2x-linux-ubuntu-amd64/DEBIAN/control
    dpkg-deb --build video2x-linux-ubuntu-amd64

[unix]
[group('build')]
appimage:
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential cmake clang pkg-config ninja-build curl file fuse \
        libavcodec-dev \
        libavdevice-dev \
        libavfilter-dev \
        libavformat-dev \
        libavutil-dev \
        libswscale-dev \
        libvulkan-dev \
        glslang-tools \
        libomp-dev \
        libboost-program-options1.83-dev \
        libboost-program-options1.83.0 \
        libspdlog-dev
    cmake -G '{{generator}}' -B build -S . \
        -DVIDEO2X_USE_EXTERNAL_NCNN=OFF \
        -DNCNN_BUILD_SHARED_LIBS=ON \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_BUILD_TYPE=Release \
        -DNCNN_AVX512=OFF \
        -DCMAKE_INSTALL_PREFIX=AppDir/usr
    cmake --build build --config Release --target install --parallel
    rm -rf AppDir/usr/share/video2x/models/rife/rife \
        AppDir/usr/share/video2x/models/rife/rife-HD \
        AppDir/usr/share/video2x/models/rife/rife-UHD \
        AppDir/usr/share/video2x/models/rife/rife-anime \
        AppDir/usr/share/video2x/models/rife/rife-v2 \
        AppDir/usr/share/video2x/models/rife/rife-v2.3 \
        AppDir/usr/share/video2x/models/rife/rife-v2.4 \
        AppDir/usr/share/video2x/models/rife/rife-v3.0 \
        AppDir/usr/share/video2x/models/rife/rife-v3.1
    curl -Lo /usr/local/bin/linuxdeploy \
        https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
    chmod +x /usr/local/bin/linuxdeploy
    LD_LIBRARY_PATH=AppDir/usr/lib linuxdeploy \
        --appdir AppDir \
        --executable AppDir/usr/bin/video2x \
        --exclude-library "libvulkan.so.1" \
        --desktop-file packaging/appimage/video2x.desktop \
        --icon-file packaging/appimage/video2x.png \
        --output appimage

[unix]
[group('misc')]
clean:
    rm -vrf {{bindir}} data/output*.* heaptrack*.zst valgrind.log

[windows]
[group('misc')]
clean:
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue -Path build,data/output*.*

[unix]
[group('test')]
test-realesrgan:
    LD_LIBRARY_PATH={{bindir}} {{bindir}}/video2x \
        -i {{test_video}} -o {{test_output}} \
        -p realesrgan -s 4 --realesrgan-model realesr-animevideov3

[unix]
[group('test')]
test-realcugan:
    LD_LIBRARY_PATH={{bindir}} {{bindir}}/video2x \
        -i {{test_video}} -o {{test_output}} \
        -p realcugan -s 4 -n 0 --realcugan-model models-se

[unix]
[group('test')]
test-libplacebo:
    LD_LIBRARY_PATH={{bindir}} {{bindir}}/video2x \
        -i {{test_video}} -o {{test_output}} \
        -p libplacebo -w 1920 -h 1080 --libplacebo-shader anime4k-v4-a

[unix]
[group('test')]
test-rife:
    LD_LIBRARY_PATH={{bindir}} {{bindir}}/video2x \
        -i {{test_video}} -o {{test_output}} \
        -p rife -m 4 --rife-model rife-v4.6

[unix]
[group('test')]
memcheck-realesrgan:
    LD_LIBRARY_PATH={{bindir}} valgrind \
        --tool=memcheck \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --show-reachable=yes \
        --verbose --log-file="valgrind.log" \
        {{bindir}}/video2x \
        -i {{test_video}} -o {{test_output}} \
        -p realesrgan -s 2 --realesrgan-model realesr-animevideov3 \
        -e preset=veryfast -e crf=30

[unix]
[group('test')]
memcheck-realcugan:
    LD_LIBRARY_PATH={{bindir}} valgrind \
        --tool=memcheck \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --show-reachable=yes \
        --verbose --log-file="valgrind.log" \
        {{bindir}}/video2x \
        -i {{test_video}} -o {{test_output}} \
        -p realcugan -s 2 -n 0 --realcugan-model models-se \
        -e preset=veryfast -e crf=30

[unix]
[group('test')]
memcheck-libplacebo:
    LD_LIBRARY_PATH={{bindir}} valgrind \
        --tool=memcheck \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --show-reachable=yes \
        --verbose --log-file="valgrind.log" \
        {{bindir}}/video2x \
        -i {{test_video}} -o {{test_output}} \
        -p libplacebo -w 1920 -h 1080 --libplacebo-shader anime4k-v4-a \
        -e preset=veryfast -e crf=30

[unix]
[group('test')]
memcheck-rife:
    LD_LIBRARY_PATH={{bindir}} valgrind \
        --tool=memcheck \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --show-reachable=yes \
        --verbose --log-file="valgrind.log" \
        {{bindir}}/video2x \
        -i {{test_video}} -o {{test_output}} \
        -p rife -m 4 --rife-model rife-v4.6 \
        -e preset=veryfast -e crf=30

[unix]
[group('test')]
heaptrack-realesrgan:
    LD_LIBRARY_PATH={{bindir}} HEAPTRACK_ENABLE_DEBUGINFOD=1 heaptrack \
        {{bindir}}/video2x \
        -i {{test_video}} -o {{test_output}} \
        -p realesrgan -s 4 --realesrgan-model realesr-animevideov3 \
        -e preset=veryfast -e crf=30

[unix]
[group('test')]
heaptrack-realcugan:
    LD_LIBRARY_PATH={{bindir}} HEAPTRACK_ENABLE_DEBUGINFOD=1 heaptrack \
        {{bindir}}/video2x \
        -i {{test_video}} -o {{test_output}} \
        -p realcugan -s 4 -n 0 --realcugan-model models-se \
        -e preset=veryfast -e crf=30

[unix]
[group('test')]
heaptrack-libplacebo:
    LD_LIBRARY_PATH={{bindir}} HEAPTRACK_ENABLE_DEBUGINFOD=1 heaptrack \
        {{bindir}}/video2x \
        -i {{test_video}} -o {{test_output}} \
        -p libplacebo -w 1920 -h 1080 --libplacebo-shader anime4k-v4-a \
        -e preset=veryfast -e crf=30

[unix]
[group('test')]
heaptrack-rife:
    LD_LIBRARY_PATH={{bindir}} HEAPTRACK_ENABLE_DEBUGINFOD=1 heaptrack \
        {{bindir}}/video2x \
        -i {{test_video}} -o {{test_output}} \
        -p rife -m 4 --rife-model rife-v4.6 \
        -e preset=veryfast -e crf=30

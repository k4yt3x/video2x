BINDIR := "build"
CXX := "clang++"
TEST_VIDEO := "data/standard-test.mp4"
TEST_OUTPUT := "data/output.mp4"

build:
    cmake -G Ninja -S . -B {{BINDIR}} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_CXX_COMPILER={{CXX}} \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX={{BINDIR}}/video2x-install \
        -DVIDEO2X_ENABLE_NATIVE=ON
    cmake --build {{BINDIR}} --config Release --parallel --target install
    cp {{BINDIR}}/compile_commands.json .

static:
    cmake -G Ninja -S . -B {{BINDIR}} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_CXX_COMPILER={{CXX}} \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX={{BINDIR}}/video2x-install \
        -DBUILD_SHARED_LIBS=OFF \
        -DVIDEO2X_USE_EXTERNAL_NCNN=OFF \
        -DVIDEO2X_USE_EXTERNAL_SPDLOG=OFF \
        -DVIDEO2X_USE_EXTERNAL_BOOST=OFF
    cmake --build {{BINDIR}} --config Release --parallel --target install
    cp {{BINDIR}}/compile_commands.json .

debug:
    cmake -G Ninja -S . -B {{BINDIR}} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_CXX_COMPILER={{CXX}} \
        -DCMAKE_BUILD_TYPE=Debug
    cmake --build {{BINDIR}} --config Debug --parallel
    cp {{BINDIR}}/compile_commands.json .

windows:
    cmake -S . -B {{BINDIR}} \
        -DVIDEO2X_USE_EXTERNAL_NCNN=OFF \
        -DVIDEO2X_USE_EXTERNAL_SPDLOG=OFF \
        -DVIDEO2X_USE_EXTERNAL_BOOST=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX={{BINDIR}}/video2x-install
    cmake --build {{BINDIR}} --config Release --parallel --target install

windows-debug:
    cmake -S . -B {{BINDIR}} \
        -DVIDEO2X_USE_EXTERNAL_NCNN=OFF \
        -DVIDEO2X_USE_EXTERNAL_SPDLOG=OFF \
        -DVIDEO2X_USE_EXTERNAL_BOOST=OFF \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_INSTALL_PREFIX={{BINDIR}}/video2x-install
    cmake --build {{BINDIR}} --config Debug --parallel

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
    cmake -G Ninja -B /tmp/build -S . \
        -DVIDEO2X_USE_EXTERNAL_NCNN=OFF \
        -DCMAKE_CXX_COMPILER={{CXX}} \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/tmp/install \
        -DINSTALL_BIN_DESTINATION=. \
        -DINSTALL_INCLUDE_DESTINATION=include \
        -DINSTALL_LIB_DESTINATION=. \
        -DINSTALL_MODEL_DESTINATION=.
    cmake --build /tmp/build --config Release --target install --parallel

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
    cmake -G Ninja -B build -S . \
        -DVIDEO2X_USE_EXTERNAL_NCNN=OFF \
        -DVIDEO2X_USE_EXTERNAL_SPDLOG=OFF \
        -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=video2x-linux-ubuntu-amd64/usr
    cmake --build build --config Release --target install --parallel
    mkdir -p video2x-linux-ubuntu-amd64/DEBIAN
    cp packaging/debian/control.ubuntu2404 video2x-linux-ubuntu-amd64/DEBIAN/control
    dpkg-deb --build video2x-linux-ubuntu-amd64

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
    cmake -G Ninja -B build -S . \
        -DVIDEO2X_USE_EXTERNAL_NCNN=OFF \
        -DVIDEO2X_USE_EXTERNAL_SPDLOG=OFF \
        -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=video2x-linux-ubuntu-amd64/usr
    cmake --build build --config Release --target install --parallel
    mkdir -p video2x-linux-ubuntu-amd64/DEBIAN
    cp packaging/debian/control.ubuntu2204 video2x-linux-ubuntu-amd64/DEBIAN/control
    dpkg-deb --build video2x-linux-ubuntu-amd64

clean:
    rm -vrf {{BINDIR}} data/output*.* heaptrack*.zst valgrind.log

test-realesrgan:
    LD_LIBRARY_PATH={{BINDIR}} {{BINDIR}}/video2x \
        -i {{TEST_VIDEO}} -o {{TEST_OUTPUT}} \
        -p realesrgan -s 4 --realesrgan-model realesr-animevideov3

test-realcugan:
    LD_LIBRARY_PATH={{BINDIR}} {{BINDIR}}/video2x \
        -i {{TEST_VIDEO}} -o {{TEST_OUTPUT}} \
        -p realcugan -s 4 -n 0 --realcugan-model models-se

test-libplacebo:
    LD_LIBRARY_PATH={{BINDIR}} {{BINDIR}}/video2x \
        -i {{TEST_VIDEO}} -o {{TEST_OUTPUT}} \
        -p libplacebo -w 1920 -h 1080 --libplacebo-shader anime4k-v4-a

test-rife:
    LD_LIBRARY_PATH={{BINDIR}} {{BINDIR}}/video2x \
        -i {{TEST_VIDEO}} -o {{TEST_OUTPUT}} \
        -p rife -m 4 --rife-model rife-v4.6

memcheck-realesrgan:
    LD_LIBRARY_PATH={{BINDIR}} valgrind \
        --tool=memcheck \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --show-reachable=yes \
        --verbose --log-file="valgrind.log" \
        {{BINDIR}}/video2x \
        -i {{TEST_VIDEO}} -o {{TEST_OUTPUT}} \
        -p realesrgan -s 2 --realesrgan-model realesr-animevideov3 \
        -e preset=veryfast -e crf=30

memcheck-realcugan:
    LD_LIBRARY_PATH={{BINDIR}} valgrind \
        --tool=memcheck \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --show-reachable=yes \
        --verbose --log-file="valgrind.log" \
        {{BINDIR}}/video2x \
        -i {{TEST_VIDEO}} -o {{TEST_OUTPUT}} \
        -p realcugan -s 2 -n 0 --realcugan-model models-se \
        -e preset=veryfast -e crf=30

memcheck-libplacebo:
    LD_LIBRARY_PATH={{BINDIR}} valgrind \
        --tool=memcheck \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --show-reachable=yes \
        --verbose --log-file="valgrind.log" \
        {{BINDIR}}/video2x \
        -i {{TEST_VIDEO}} -o {{TEST_OUTPUT}} \
        -p libplacebo -w 1920 -h 1080 --libplacebo-shader anime4k-v4-a \
        -e preset=veryfast -e crf=30

memcheck-rife:
    LD_LIBRARY_PATH={{BINDIR}} valgrind \
        --tool=memcheck \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --show-reachable=yes \
        --verbose --log-file="valgrind.log" \
        {{BINDIR}}/video2x \
        -i {{TEST_VIDEO}} -o {{TEST_OUTPUT}} \
        -p rife -m 4 --rife-model rife-v4.6 \
        -e preset=veryfast -e crf=30

heaptrack-realesrgan:
    LD_LIBRARY_PATH={{BINDIR}} HEAPTRACK_ENABLE_DEBUGINFOD=1 heaptrack \
        {{BINDIR}}/video2x \
        -i {{TEST_VIDEO}} -o {{TEST_OUTPUT}} \
        -p realesrgan -s 4 --realesrgan-model realesr-animevideov3 \
        -e preset=veryfast -e crf=30

heaptrack-realcugan:
    LD_LIBRARY_PATH={{BINDIR}} HEAPTRACK_ENABLE_DEBUGINFOD=1 heaptrack \
        {{BINDIR}}/video2x \
        -i {{TEST_VIDEO}} -o {{TEST_OUTPUT}} \
        -p realcugan -s 4 -n 0 --realcugan-model models-se \
        -e preset=veryfast -e crf=30

heaptrack-libplacebo:
    LD_LIBRARY_PATH={{BINDIR}} HEAPTRACK_ENABLE_DEBUGINFOD=1 heaptrack \
        {{BINDIR}}/video2x \
        -i {{TEST_VIDEO}} -o {{TEST_OUTPUT}} \
        -p libplacebo -w 1920 -h 1080 --libplacebo-shader anime4k-v4-a \
        -e preset=veryfast -e crf=30

heaptrack-rife:
    LD_LIBRARY_PATH={{BINDIR}} HEAPTRACK_ENABLE_DEBUGINFOD=1 heaptrack \
        {{BINDIR}}/video2x \
        -i {{TEST_VIDEO}} -o {{TEST_OUTPUT}} \
        -p rife -m 4 --rife-model rife-v4.6 \
        -e preset=veryfast -e crf=30

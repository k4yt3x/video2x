.PHONY: build static debug windows windows-debug debian ubuntu clean \
	test-realesrgan test-realcugan test-libplacebo test-rife \
	memcheck-realesrgan memcheck-realcugan memcheck-libplacebo memcheck-rife \
	heaptrack-realesrgan heaptrack-realcugan heaptrack-libplacebo heaptrack-rife

BINDIR=build
CXX=clang++

TEST_VIDEO=data/standard-test.mp4
TEST_OUTPUT=data/output.mp4

build:
	cmake -S . -B $(BINDIR) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DCMAKE_CXX_COMPILER=$(CXX) \
		-DCMAKE_BUILD_TYPE=Release
	cmake --build $(BINDIR) --config Release --parallel
	cp $(BINDIR)/compile_commands.json .

static:
	cmake -S . -B $(BINDIR) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DCMAKE_CXX_COMPILER=$(CXX) \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_SHARED_LIBS=OFF \
		-DUSE_SYSTEM_NCNN=OFF \
		-DUSE_SYSTEM_SPDLOG=OFF \
		-DUSE_SYSTEM_BOOST=OFF
	cmake --build $(BINDIR) --config Release --parallel
	cp $(BINDIR)/compile_commands.json .

debug:
	cmake -S . -B $(BINDIR) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DCMAKE_CXX_COMPILER=$(CXX) \
		-DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BINDIR) --config Debug --parallel
	cp $(BINDIR)/compile_commands.json .

windows:
	cmake -S . -B $(BINDIR) \
		-DUSE_SYSTEM_NCNN=OFF \
		-DUSE_SYSTEM_SPDLOG=OFF \
		-DUSE_SYSTEM_BOOST=OFF \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=build/libvideo2x-shared
	cmake --build $(BINDIR) --config Release --parallel --target install

windows-debug:
	cmake -S . -B $(BINDIR) \
		-DUSE_SYSTEM_NCNN=OFF \
		-DUSE_SYSTEM_SPDLOG=OFF \
		-DUSE_SYSTEM_BOOST=OFF \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_INSTALL_PREFIX=build/libvideo2x-shared
	cmake --build $(BINDIR) --config Debug --parallel --target install

debian:
	apt-get update
	apt-get install -y --no-install-recommends \
		build-essential cmake clang pkg-config \
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
	cmake -B /tmp/build -S . -DUSE_SYSTEM_NCNN=OFF -DCMAKE_CXX_COMPILER=$(CXX) \
		-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/tmp/install \
		-DINSTALL_BIN_DESTINATION=. -DINSTALL_INCLUDE_DESTINATION=include \
		-DINSTALL_LIB_DESTINATION=. -DINSTALL_MODEL_DESTINATION=.
	cmake --build /tmp/build --config Release --target install --parallel

ubuntu2404:
	apt-get update
	DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
		build-essential cmake pkg-config \
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
	cmake -B build -S . -DUSE_SYSTEM_NCNN=OFF -DUSE_SYSTEM_SPDLOG=OFF -DSPDLOG_NO_EXCEPTIONS=ON \
		-DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release \
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
		build-essential cmake \
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
	cmake -B build -S . -DUSE_SYSTEM_NCNN=OFF -DUSE_SYSTEM_SPDLOG=OFF -DSPDLOG_NO_EXCEPTIONS=ON \
		-DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=video2x-linux-ubuntu-amd64/usr
	cmake --build build --config Release --target install --parallel
	mkdir -p video2x-linux-ubuntu-amd64/DEBIAN
	cp packaging/debian/control.ubuntu2204 video2x-linux-ubuntu-amd64/DEBIAN/control
	dpkg-deb --build video2x-linux-ubuntu-amd64

clean:
	rm -vrf $(BINDIR) data/output*.* heaptrack*.zst valgrind.log

test-realesrgan:
	LD_LIBRARY_PATH=$(BINDIR) $(BINDIR)/video2x -i $(TEST_VIDEO) -o $(TEST_OUTPUT) \
		-p realesrgan -s 4 --realesrgan-model realesr-animevideov3

test-realcugan:
	LD_LIBRARY_PATH=$(BINDIR) $(BINDIR)/video2x -i $(TEST_VIDEO) -o $(TEST_OUTPUT) \
		-p realcugan -s 4 -n 0 --realcugan-model models-se

test-libplacebo:
	LD_LIBRARY_PATH=$(BINDIR) $(BINDIR)/video2x -i $(TEST_VIDEO) -o $(TEST_OUTPUT) \
		-p libplacebo -w 1920 -h 1080 --libplacebo-shader anime4k-v4-a

test-rife:
	LD_LIBRARY_PATH=$(BINDIR) $(BINDIR)/video2x -i $(TEST_VIDEO) -o $(TEST_OUTPUT) \
		-p rife -m 4 --rife-model rife-v4.6

memcheck-realesrgan:
	LD_LIBRARY_PATH=$(BINDIR) valgrind \
		--tool=memcheck \
		--leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--show-reachable=yes \
		--verbose --log-file="valgrind.log" \
		$(BINDIR)/video2x \
		-i $(TEST_VIDEO) -o $(TEST_OUTPUT) \
		-p realesrgan -s 2 --realesrgan-model realesr-animevideov3 \
		-e preset=veryfast -e crf=30

memcheck-realcugan:
	LD_LIBRARY_PATH=$(BINDIR) valgrind \
		--tool=memcheck \
		--leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--show-reachable=yes \
		--verbose --log-file="valgrind.log" \
		$(BINDIR)/video2x \
		-i $(TEST_VIDEO) -o $(TEST_OUTPUT) \
		-p realcugan -s 2 -n 0 --realcugan-model models-se \
		-e preset=veryfast -e crf=30

memcheck-libplacebo:
	LD_LIBRARY_PATH=$(BINDIR) valgrind \
		--tool=memcheck \
		--leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--show-reachable=yes \
		--verbose --log-file="valgrind.log" \
		$(BINDIR)/video2x \
		-i $(TEST_VIDEO) -o $(TEST_OUTPUT) \
		-p libplacebo -w 1920 -h 1080 --libplacebo-shader anime4k-v4-a \
		-e preset=veryfast -e crf=30

memcheck-rife:
	LD_LIBRARY_PATH=$(BINDIR) valgrind \
		--tool=memcheck \
		--leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--show-reachable=yes \
		--verbose --log-file="valgrind.log" \
		$(BINDIR)/video2x \
		-i $(TEST_VIDEO) -o $(TEST_OUTPUT) \
		-p rife -m 4 --rife-model rife-v4.6 \
		-e preset=veryfast -e crf=30

heaptrack-realesrgan:
	LD_LIBRARY_PATH=$(BINDIR) HEAPTRACK_ENABLE_DEBUGINFOD=1 heaptrack \
		$(BINDIR)/video2x \
		-i $(TEST_VIDEO) -o $(TEST_OUTPUT) \
		-p realesrgan -s 4 --realesrgan-model realesr-animevideov3 \
		-e preset=veryfast -e crf=30

heaptrack-realcugan:
	LD_LIBRARY_PATH=$(BINDIR) HEAPTRACK_ENABLE_DEBUGINFOD=1 heaptrack \
		$(BINDIR)/video2x \
		-i $(TEST_VIDEO) -o $(TEST_OUTPUT) \
		-p realcugan -s 4 -n 0 --realcugan-model models-se \
		-e preset=veryfast -e crf=30

heaptrack-libplacebo:
	LD_LIBRARY_PATH=$(BINDIR) HEAPTRACK_ENABLE_DEBUGINFOD=1 heaptrack \
		$(BINDIR)/video2x \
		-i $(TEST_VIDEO) -o $(TEST_OUTPUT) \
		-p libplacebo -w 1920 -h 1080 --libplacebo-shader anime4k-v4-a \
		-e preset=veryfast -e crf=30

heaptrack-rife:
	LD_LIBRARY_PATH=$(BINDIR) HEAPTRACK_ENABLE_DEBUGINFOD=1 heaptrack \
		$(BINDIR)/video2x \
		-i $(TEST_VIDEO) -o $(TEST_OUTPUT) \
		-p rife -m 4 --rife-model rife-v4.6 \
		-e preset=veryfast -e crf=30

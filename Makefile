.PHONY: build static debug clean \
	test-realesrgan test-libplacebo \
	memcheck-realesrgan memcheck-libplacebo \
	heaptrack-realesrgan heaptrack-libplacebo

BINDIR=build
CC=clang
CXX=clang++

TEST_VIDEO=data/standard-test.mp4
TEST_OUTPUT=data/output.mp4

build:
	cmake -S . -B $(BINDIR) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DCMAKE_C_COMPILER=$(CC) \
		-DCMAKE_CXX_COMPILER=$(CXX) \
		-DCMAKE_BUILD_TYPE=Release
	cmake --build $(BINDIR) --config Release --parallel
	cp $(BINDIR)/compile_commands.json .

static:
	cmake -S . -B $(BINDIR) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DCMAKE_C_COMPILER=$(CC) \
		-DCMAKE_CXX_COMPILER=$(CXX) \
		-DCMAKE_BUILD_TYPE=Release \
		-DBUILD_SHARED_LIBS=OFF
	cmake --build $(BINDIR) --config Release --parallel
	cp $(BINDIR)/compile_commands.json .

debug:
	cmake -S . -B $(BINDIR) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DCMAKE_C_COMPILER=$(CC) \
		-DCMAKE_CXX_COMPILER=$(CXX) \
		-DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BINDIR) --config Debug --parallel
	cp $(BINDIR)/compile_commands.json .

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
		libopencv-dev
	cmake -B /tmp/build -S . -DUSE_SYSTEM_NCNN=OFF \
		-DCMAKE_C_COMPILER=$(CC) -DCMAKE_CXX_COMPILER=$(CXX) \
		-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/tmp/install \
		-DINSTALL_BIN_DESTINATION=. -DINSTALL_INCLUDE_DESTINATION=include \
		-DINSTALL_LIB_DESTINATION=. -DINSTALL_MODEL_DESTINATION=.
	cmake --build /tmp/build --config Release --target install --parallel

clean:
	rm -rf $(BINDIR)

test-realesrgan:
	LD_LIBRARY_PATH=$(BINDIR) $(BINDIR)/video2x -i $(TEST_VIDEO) -o $(TEST_OUTPUT) \
		-f realesrgan -r 4 -m realesr-animevideov3

test-libplacebo:
	LD_LIBRARY_PATH=$(BINDIR) $(BINDIR)/video2x -i $(TEST_VIDEO) -o $(TEST_OUTPUT) \
		-f libplacebo -w 1920 -h 1080 -s anime4k-mode-a

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
		-f realesrgan -r 2 -m realesr-animevideov3 \
		-p veryfast -b 1000000 -q 30

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
		-f libplacebo -w 1920 -h 1080 -s anime4k-mode-a \
		-p veryfast -b 1000000 -q 30

heaptrack-realesrgan:
	LD_LIBRARY_PATH=$(BINDIR) HEAPTRACK_ENABLE_DEBUGINFOD=1 heaptrack \
		$(BINDIR)/video2x \
		-i $(TEST_VIDEO) -o $(TEST_OUTPUT) \
		-f realesrgan -r 4 -m realesr-animevideov3 \
		-p veryfast -b 1000000 -q 30

heaptrack-libplacebo:
	LD_LIBRARY_PATH=$(BINDIR) HEAPTRACK_ENABLE_DEBUGINFOD=1 heaptrack \
		$(BINDIR)/video2x \
		-i $(TEST_VIDEO) -o $(TEST_OUTPUT) \
		-f libplacebo -w 1920 -h 1080 -s anime4k-mode-a \
		-p veryfast -b 1000000 -q 30

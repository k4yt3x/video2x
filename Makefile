.PHONY: build static debug windows test-realesrgan test-libplacebo leakcheck clean

BINDIR=build

build:
	cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -S . -B build
	cmake --build build --config Release --parallel
	cp build/compile_commands.json .

static:
	cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -S . -B build \
		-DBUILD_SHARED_LIBS=OFF -DUSE_SYSTEM_NCNN=OFF
	cmake --build build --config Release --parallel
	cp build/compile_commands.json .

debug:
	cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -S . -B build
	cmake --build build --config Debug --parallel
	cp build/compile_commands.json .

windows:
	cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -S . -B build \
		-DCMAKE_TOOLCHAIN_FILE=cmake/windows-x64-toolchain.cmake \
		-DUSE_SYSTEM_NCNN=OFF -DBUILD_VIDEO2X=OFF
	cmake --build build --config Release --parallel
	cp build/compile_commands.json .

test-realesrgan:
	LD_LIBRARY_PATH=$(BINDIR) $(BINDIR)/video2x -i data/standard-test.mp4 -o data/output.mp4 \
		-f realesrgan -r 4 --model realesr-animevideov3

test-libplacebo:
	LD_LIBRARY_PATH=$(BINDIR) $(BINDIR)/video2x -i data/standard-test.mp4 -o data/output.mp4 \
		-f libplacebo -w 1920 -h 1080 -s anime4k-mode-a

leakcheck-realesrgan:
	LD_LIBRARY_PATH=$(BINDIR) valgrind \
		--tool=memcheck \
		--leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--show-reachable=yes \
		--verbose --log-file="valgrind.log" \
		$(BINDIR)/video2x \
		-i data/standard-test.mp4 -o data/output.mp4 \
		-f realesrgan -r 2 --model realesr-animevideov3 \
		-p veryfast -b 1000000 -q 30

leakcheck-libplacebo:
	LD_LIBRARY_PATH=$(BINDIR) valgrind \
		--tool=memcheck \
		--leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--show-reachable=yes \
		--verbose --log-file="valgrind.log" \
		$(BINDIR)/video2x \
		-i data/standard-test.mp4 -o data/output.mp4 \
		-f libplacebo -w 1920 -h 1080 -s anime4k-mode-a \
		-p veryfast -b 1000000 -q 30

clean:
	rm -rf $(BINDIR)

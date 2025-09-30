FROM archlinux:base-devel
RUN pacman -Syu --noconfirm cmake ninja boost ffmpeg ncnn openmp spdlog vulkan-driver vulkan-headers
RUN mkdir /source
WORKDIR /source
COPY . /source
RUN cmake -B build -G Ninja -DCMAKE_INSTALL_PREFIX=/usr
RUN ninja -C build install
RUN tar -cf video2x.tar -T build/install_manifest.txt

FROM archlinux:base
RUN pacman -Suy --noconfirm boost-libs ffmpeg ncnn openmp spdlog vulkan-driver
COPY --from=0 /source/video2x.tar /
RUN tar -xf /video2x.tar && rm /video2x.tar
CMD ["/usr/bin/video2x"]

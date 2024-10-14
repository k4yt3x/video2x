# Name: Video2X Dockerfile
# Creator: K4YT3X
# Date Created: February 3, 2022
# Last Modified: October 14, 2024

# stage 1: build the python components into wheels
FROM docker.io/archlinux:latest AS builder

# Install dependencies and create a non-root user
RUN pacman -Syy --noconfirm \
        base-devel ffmpeg ncnn git cmake make clang pkgconf vulkan-headers openmp spdlog sudo \
        nvidia-utils vulkan-radeon vulkan-intel vulkan-swrast \
    && useradd -m builder \
    && echo 'builder ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/builder

# Switch to the non-root user and copy the source code
USER builder
COPY --chown=builder:builder . /video2x
WORKDIR /video2x

# Build the package
RUN makepkg -s --noconfirm \
    && find /video2x -maxdepth 1 -name 'video2x-*.pkg.tar.zst' ! -name '*-debug-*' | head -n 1 | \
        xargs -I {} cp {} /tmp/video2x.pkg.tar.zst

# stage 2: install wheels into the final image
FROM docker.io/archlinux:latest
LABEL maintainer="K4YT3X <i@k4yt3x.com>" \
      org.opencontainers.image.source="https://github.com/k4yt3x/video2x" \
      org.opencontainers.image.description="A lossless video super resolution framework"

ENV VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/nvidia_icd.json\
:/usr/share/vulkan/icd.d/radeon_icd.x86_64.json\
:/usr/share/vulkan/icd.d/intel_icd.x86_64.json

COPY --from=builder /tmp/video2x.pkg.tar.zst /video2x.pkg.tar.zst
RUN pacman -Sy --noconfirm ffmpeg ncnn spdlog \
        nvidia-utils vulkan-radeon vulkan-intel vulkan-swrast \
    && pacman -U --noconfirm /video2x.pkg.tar.zst \
    && rm -rf /video2x.pkg.tar.zst /var/cache/pacman/pkg/*

WORKDIR /host
ENTRYPOINT ["/usr/bin/video2x"]

# Name: Video2X Dockerfile (CUDA)
# Creator: K4YT3X
# Date Created: February 3, 2022
# Last Modified: February 4, 2022

# stage 1: build the python components into wheels
FROM nvidia/cuda:11.6.0-runtime-ubuntu20.04 AS builder
ENV DEBIAN_FRONTEND=noninteractive

COPY . /video2x
WORKDIR /video2x
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        python3-pip python3-opencv python3-pil python3-tqdm \
        python3-dev libvulkan-dev glslang-dev glslang-tools \
        build-essential swig git \
    && git config --global http.postBuffer 1048576000 \
    && git config --global https.postBuffer 1048576000 \
    && pip wheel -w /wheels \
        wheel setuptools setuptools_scm \
        rife-ncnn-vulkan-python@git+https://github.com/media2x/rife-ncnn-vulkan-python.git .

# stage 2: install wheels into the final image
FROM nvidia/cuda:11.6.0-runtime-ubuntu20.04
LABEL maintainer="K4YT3X <i@k4yt3x.com>" \
      org.opencontainers.image.source="https://github.com/k4yt3x/video2x" \
      org.opencontainers.image.description="A lossless video/GIF/image upscaler"
ENV DEBIAN_FRONTEND=noninteractive

COPY --from=builder /var/lib/apt/lists* /var/lib/apt/lists/
COPY --from=builder /wheels /wheels
COPY . /video2x
WORKDIR /video2x
RUN apt-get install -y --no-install-recommends \
        python3-pip python3-dev \
        python3-opencv python3-pil python3-tqdm \
        mesa-vulkan-drivers ffmpeg \
    && pip install --no-cache-dir --no-index -f /wheels . \
    && apt-get clean \
    && rm -rf /wheels /video2x /var/lib/apt/lists/*

WORKDIR /host
ENTRYPOINT ["/usr/bin/python3", "-m", "video2x"]

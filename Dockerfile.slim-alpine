# Name: Video2X Dockerfile (Slim Alpine)
# Creator: K4YT3X
# Date Created: February 1, 2022
# Last Modified: March 18, 2022

# stage: build python components into heels
FROM docker.io/library/python:3.10.4-alpine3.15 AS builder
COPY . /video2x
WORKDIR /video2x
RUN apk add --no-cache \
        cmake g++ gcc git ninja swig linux-headers \
        ffmpeg-dev glslang-dev jpeg-dev vulkan-loader-dev zlib-dev \
    && pip install -U pip \
    && CMAKE_ARGS="-DWITH_FFMPEG=YES" pip wheel -w /wheels opencv-python \
    && pip wheel -w /wheels wheel pdm-pep517 .

# stage 2: install wheels into final image
FROM docker.io/library/python:3.10.4-alpine3.15
LABEL maintainer="K4YT3X <i@k4yt3x.com>" \
      org.opencontainers.image.source="https://github.com/k4yt3x/video2x" \
      org.opencontainers.image.description="A lossless video/GIF/image upscaler"

COPY --from=builder /wheels /wheels
COPY . /video2x
WORKDIR /video2x
RUN apk add --no-cache --virtual .run-deps \
        ffmpeg libgomp libjpeg-turbo libstdc++ \
        vulkan-loader mesa-vulkan-ati \
    && pip install --no-cache-dir -U pip \
    && pip install --no-cache-dir --no-index -f /wheels . \
    && rm -rf /wheels /video2x

WORKDIR /host
ENTRYPOINT ["/usr/local/bin/python", "-m", "video2x"]

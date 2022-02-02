# Name: Video2X Dockerfile
# Creator: Danielle Douglas
# Date Created: Unknown
# Last Modified: January 14, 2020

# Editor: Lhanjian
# Last Modified: May 24, 2020

# Editor: K4YT3X
# Last Modified: February 1, 2022

# stage: build python components into heels
FROM python:3.10.2-alpine3.15 AS builder
COPY . /video2x
WORKDIR /video2x
RUN apk add --no-cache \
        cmake g++ gcc git ninja swig \
        ffmpeg-dev glslang-dev jpeg-dev vulkan-loader-dev zlib-dev \
        linux-headers \
    && pip install -U pip \
    && CMAKE_ARGS="-DWITH_FFMPEG=YES" pip wheel -w /wheels opencv-python \
    && pip wheel -w /wheels wheel setuptools setuptools_scm \
        rife-ncnn-vulkan-python@git+https://github.com/media2x/rife-ncnn-vulkan-python.git .

# stage 2: install wheels into final image
FROM python:3.10.2-alpine3.15
LABEL maintainer="Danielle Douglas <ddouglas87@gmail.com>" \
      maintainer="Lhanjian <lhjay1@foxmail.com>" \
      maintainer="K4YT3X <i@k4yt3x.com>"

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

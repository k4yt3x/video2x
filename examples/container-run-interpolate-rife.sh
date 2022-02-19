#!/bin/sh

set -euxo pipefail

sudo podman run \
    -it --rm --gpus all -v /dev/dri:/dev/dri \
    -v $PWD/data:/host \
    ghcr.io/k4yt3x/video2x:5.0.0-beta4-cuda \
    -i input.mp4 -o output.mp4 \
    interpolate

#!/bin/sh
# mount the current (video2x repo root) directory into a container
# with drivers installed so the code can be debugged in the container
# this one launches an interactive shell instead of Python

set -euo pipefail

sudo podman run -it --rm \
    --gpus all -v /dev/dri:/dev/dri \
    -v $PWD:/host \
    -m 15g \
    --cpus 0.9 \
    -v $HOME/projects/media2x/video2x:/video2x \
    -e PYTHONPATH=/video2x \
    -e PYTHONDONTWRITEBYTECODE=1 \
    --entrypoint=/bin/bash \
    ghcr.io/k4yt3x/video2x:5.0.0-beta4-cuda

# alias upscale='python3 -m video2x -i /host/input-large.mp4 -o /host/output-large.mp4 -p3 upscale -h 1440 -d waifu2x -n3'

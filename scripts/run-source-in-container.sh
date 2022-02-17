#!/bin/sh
# mount the current (video2x repo root) directory into a container
# with drivers installed so the code can be debugged in the container

set -euo pipefail

sudo podman run -it --rm \
    --gpus all -v /dev/dri:/dev/dri \
    -v $PWD:/host \
    -m 15g \
    --cpus 0.9 \
    -v $HOME/projects/media2x/video2x:/video2x \
    -e PYTHONPATH=/video2x \
    -e PYTHONDONTWRITEBYTECODE=1 \
    ghcr.io/k4yt3x/video2x:5.0.0-beta4-cuda \
    -i data/input.mp4 -o data/output.mp4 \
    -p3 \
    upscale \
    -h 1440 -a waifu2x -n3

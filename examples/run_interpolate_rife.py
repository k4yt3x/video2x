#!/usr/bin/python
# -*- coding: utf-8 -*-

# built-in imports
import pathlib

# import video2x
from video2x import Video2X

# create video2x object
video2x = Video2X()

# run upscale
# fmt: off
video2x.interpolate(
    pathlib.Path("input.mp4"),   # input video path
    pathlib.Path("output.mp4"),  # another
    3,                           # processes: number of parallel processors
    10,                          # threshold: adjacent frames with > n% diff won't be processed (100 == process all)
    "rife",                      # algorithm: the algorithm to use to process the video
)

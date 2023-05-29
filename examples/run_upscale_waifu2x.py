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
video2x.upscale(
    pathlib.Path("input.mp4"),   # input video path
    pathlib.Path("output.mp4"),  # another
    None,                        # width: width of output, None == auto
    720,                         # height: height of output, None == auto
    3,                           # noise: noise level, algorithm-dependent
    5,                           # processes: number of parallel processors
    0,                           # threshold: adjacent frames with < n% diff won't be processed (0 == process all)
    "waifu2x",                   # algorithm: the algorithm to use to process the video
)

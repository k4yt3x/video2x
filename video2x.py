#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2x Controller
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: Feb 24, 2018

Description: This is the main controller for Video2x

Version 1.0
"""

from ffmpeg import FFMPEG
from waifu2x import WAIFU2X
import os

fm = FFMPEG()
w2 = WAIFU2X()
if not os.path.isdir("frames"):
    os.mkdir("frames")
fm.strip_frames("testf.mp4", "frames")

if not os.path.isdir("upscaled"):
    os.mkdir("upscaled")
w2.upscale("frames", "upscaled")

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: FFMPEG Class
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: Feb 24, 2018

Description: This class handles all FFMPEG related
operations.

Version 1.0
"""

import os


class FFMPEG:

    def __init__(self):
        pass

    def strip_frames(self, videoin, outpath):
        os.system("ffmpeg -i {} -r 1/1 {}/extracted_%0d.png".format(videoin, outpath))

    def extract_audio(self, videoin, outpath):
        os.system("ffmpeg -i {} -vn -acodec copy {}/output-audio.aac".format(videoin, outpath))

    def to_vid(self, framerate, resolution, folder):
        os.system("ffmpeg -r {} -f image2 -s {} -i {}/extracted_%d.png -vcodec libx264 -crf 25 -pix_fmt yuv420p output.mp4".format(framerate, resolution, folder))

    def pressin_audio(self, videoin):
        os.system("ffmpeg -i {} -i audio.mp3 -codec copy -shortest output.mp4")

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

    def __init__(self, ffmpeg_path, outfile):
        self.ffmpeg_path = ffmpeg_path
        self.outfile = outfile

    def strip_frames(self, videoin, outpath):
        os.system("{} -i {} {}/extracted_%0d.png -y".format(self.ffmpeg_path, videoin, outpath))

    def extract_audio(self, videoin, outpath):
        os.system("{} -i {} -vn -acodec copy {}/output-audio.aac -y".format(self.ffmpeg_path, videoin, outpath))

    def to_vid(self, framerate, resolution, folder):
        os.system("{} -r {} -f image2 -s {} -i {}/extracted_%d.png -vcodec libx264 -crf 25 -pix_fmt yuv420p output.mp4 -y".format(self.ffmpeg_path, framerate, resolution, folder))

    def pressin_audio(self, videoin, outpath):
        os.system("{} -i {} -i {}/output-audio.aac -codec copy -shortest {} -y".format(self.ffmpeg_path, videoin, outpath, self.outfile))

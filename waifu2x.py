#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: FFMPEG Class
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: Feb 24, 2018

Description: This class controls waifu2x
engine

Version 1.0
"""

import os


class WAIFU2X:

    def __init__(self, waifu2x_path, method):
        self.waifu2x_path = waifu2x_path
        self.method = method

    def upscale(self, folderin, folderout, width, height):
        os.system("{} -p {} -I png -i {} -e png -o {} -w {} -h {}".format(self.waifu2x_path, self.method, folderin, folderout, width, height))

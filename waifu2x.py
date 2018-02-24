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

    def __init__(self):
        pass

    def upscale(self, factor, folderin, folderout):
        os.system("waifu2x-caffe-cui.exe -i {} -o {}".format(folderin, folderout))

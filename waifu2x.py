#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: FFMPEG Class
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: Feb 25, 2018

Description: This class controls waifu2x
engine

Version 1.1
"""

import os


class WAIFU2X:
    """This class communicates with waifu2x cui engine

    An object will be created for this class, containing information
    about the binary address and the processing method. When being called
    by the main program, other detailed information will be passed to
    the upscale function.

    TODO: Make enhancement model customizable
    """

    def __init__(self, waifu2x_path, method):
        self.waifu2x_path = waifu2x_path
        self.method = method

    def upscale(self, folderin, folderout, width, height):
        """This is the core function for WAIFU2X class

        [description]

        Arguments:
            folderin {string} -- source folder path
            folderout {string} -- output folder path
            width {int} -- output video width
            height {int} -- output video height
        """
        os.system("{} -p {} -I png -i {} -e png -o {} -w {} -h {} -n 3 -m noise_scale -y photo".format(
            self.waifu2x_path, self.method, folderin, folderout, width, height))

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: FFMPEG Class
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: May 19, 2018

Description: This class controls waifu2x
engine

Version 2.0
"""
import subprocess


class WAIFU2X:
    """This class communicates with waifu2x cui engine

    An object will be created for this class, containing information
    about the binary address and the processing method. When being called
    by the main program, other detailed information will be passed to
    the upscale function.
    """

    def __init__(self, waifu2x_path, method, model_type):
        self.waifu2x_path = waifu2x_path
        self.method = method
        self.model_type = model_type

    def upscale(self, file, upscaled, width, height):
        """This is the core function for WAIFU2X class

        [description]

        Arguments:
            file {string} -- input image
            upscaled {string} -- output folder path
            width {int} -- output video width
            height {int} -- output video height
            model_type {string} -- model to use for upscaling
        """
        file_id = file.split('extracted_')[-1].split('.png')[0]
        output_file = '{}\\{}{}{}'.format(upscaled, 'extracted_', file_id, '.png')
        execute = "{} -p {} -I png -i {} -e png -o {} -w {} -h {} -n 3 -m noise_scale -y {}".format(
            self.waifu2x_path, self.method, file, output_file, width, height, self.model_type)
        subprocess.call(execute, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Waifu2x Caffe Driver
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: February 22, 2020

Description: This class is a high-level wrapper
for waifu2x-caffe.
"""

# built-in imports
import os
import shlex
import subprocess
import threading

# third-party imports
from avalon_framework import Avalon


class Waifu2xCaffe:
    """This class communicates with waifu2x cui engine

    An object will be created for this class, containing information
    about the binary address and the processing method. When being called
    by the main program, other detailed information will be passed to
    the upscale function.
    """

    def __init__(self, driver_settings, process, model_dir, bit_depth):
        self.driver_settings = driver_settings
        self.driver_settings['process'] = process
        self.driver_settings['model_dir'] = model_dir
        self.driver_settings['output_depth'] = bit_depth

        # arguments passed through command line overwrites config file values
        self.process = process
        self.model_dir = model_dir
        self.print_lock = threading.Lock()

    def upscale(self, input_directory, output_directory, scale_ratio, scale_width, scale_height, image_format):
        """This is the core function for WAIFU2X class

        Arguments:
            input_directory {string} -- source directory path
            output_directory {string} -- output directory path
            width {int} -- output video width
            height {int} -- output video height
        """

        # overwrite config file settings
        self.driver_settings['input_path'] = input_directory
        self.driver_settings['output_path'] = output_directory

        if scale_ratio:
            self.driver_settings['scale_ratio'] = scale_ratio
        elif scale_width and scale_height:
            self.driver_settings['scale_width'] = scale_width
            self.driver_settings['scale_height'] = scale_height

        self.driver_settings['output_extention'] = image_format

        # list to be executed
        # initialize the list with waifu2x binary path as the first element
        execute = [str(self.driver_settings['path'])]

        for key in self.driver_settings.keys():

            value = self.driver_settings[key]

            # is executable key or null or None means that leave this option out (keep default)
            if key == 'path' or value is None or value is False:
                continue
            else:
                if len(key) == 1:
                    execute.append(f'-{key}')
                else:
                    execute.append(f'--{key}')
                execute.append(str(value))

        # return the Popen object of the new process created
        self.print_lock.acquire()
        Avalon.debug_info(f'[upscaler] Subprocess {os.getpid()} executing: {shlex.join(execute)}')
        self.print_lock.release()
        return subprocess.Popen(execute)

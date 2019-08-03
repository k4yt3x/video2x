#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Waifu2x Caffe Driver
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: August 3, 2019

Description: This class is a high-level wrapper
for waifu2x-caffe.
"""

# built-in imports
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

    def __init__(self, waifu2x_settings, process, model_dir, bit_depth):
        self.waifu2x_settings = waifu2x_settings
        self.waifu2x_settings['process'] = process
        self.waifu2x_settings['model_dir'] = model_dir
        self.waifu2x_settings['output_depth'] = bit_depth

        # arguments passed through command line overwrites config file values
        self.process = process
        self.model_dir = model_dir
        self.print_lock = threading.Lock()

    def upscale(self, input_directory, output_directory, scale_ratio, scale_width, scale_height, image_format, upscaler_exceptions):
        """This is the core function for WAIFU2X class

        Arguments:
            input_directory {string} -- source directory path
            output_directory {string} -- output directory path
            width {int} -- output video width
            height {int} -- output video height
        """

        try:
            # overwrite config file settings
            self.waifu2x_settings['input_path'] = input_directory
            self.waifu2x_settings['output_path'] = output_directory

            if scale_ratio:
                self.waifu2x_settings['scale_ratio'] = scale_ratio
            elif scale_width and scale_height:
                self.waifu2x_settings['scale_width'] = scale_width
                self.waifu2x_settings['scale_height'] = scale_height

            self.waifu2x_settings['output_extention'] = image_format

            # print thread start message
            self.print_lock.acquire()
            Avalon.debug_info(f'[upscaler] Thread {threading.current_thread().name} started')
            self.print_lock.release()

            # list to be executed
            # initialize the list with waifu2x binary path as the first element
            execute = [str(self.waifu2x_settings['waifu2x_caffe_path'])]

            for key in self.waifu2x_settings.keys():

                value = self.waifu2x_settings[key]

                # is executable key or null or None means that leave this option out (keep default)
                if key == 'waifu2x_caffe_path' or value is None or value is False:
                    continue
                else:
                    if len(key) == 1:
                        execute.append(f'-{key}')
                    else:
                        execute.append(f'--{key}')
                    execute.append(str(value))

            Avalon.debug_info(f'Executing: {execute}')
            completed_command = subprocess.run(execute, check=True)

            # print thread exiting message
            self.print_lock.acquire()
            Avalon.debug_info(f'[upscaler] Thread {threading.current_thread().name} exiting')
            self.print_lock.release()

            # return command execution return code
            return completed_command.returncode
        except Exception as e:
            upscaler_exceptions.append(e)

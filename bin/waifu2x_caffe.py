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

# local imports
import common

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

    def __init__(self, settings, process, model_dir, bit_depth):
        self.settings = settings
        self.settings['process'] = process
        self.settings['output_depth'] = bit_depth

        # arguments passed through command line overwrites config file values
        self.process = process
        self.print_lock = threading.Lock()

        if model_dir:
            self.settings['model_dir'] = model_dir

        # Searches for models directory
        if 'model_dir' in self.settings:
            model_dir = common.find_path(self.settings['model_dir'])

            # Search for model folder in waifu2x-caffe folder
            if model_dir[0] is None:
                model_dir = common.find_path(self.settings['path'] / self.settings['model_dir'])

            self.settings['model_dir'] = model_dir[0]

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
            self.settings['input_path'] = input_directory
            self.settings['output_path'] = output_directory

            if scale_ratio:
                self.settings['scale_ratio'] = scale_ratio
            elif scale_width and scale_height:
                self.settings['scale_width'] = scale_width
                self.settings['scale_height'] = scale_height

            self.settings['output_extention'] = image_format

            # print thread start message
            self.print_lock.acquire()
            Avalon.debug_info(f'[upscaler] Thread {threading.current_thread().name} started')
            self.print_lock.release()

            # list to be executed
            # initialize the list with waifu2x binary path as the first element
            execute = [self.settings['path'] / self.settings['binary']]

            for key, value in self.settings.items():
                if key in ['path', 'binary', 'win_binary']:
                    continue
                # is executable key or null or None means that leave this option out (keep default)
                if value is None or value is False:
                    continue

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

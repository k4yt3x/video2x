#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Waifu2x Converter CPP Driver
Author: K4YT3X
Date Created: February 8, 2019
Last Modified: February 22, 2020

Description: This class is a high-level wrapper
for waifu2x-converter-cpp.
"""

# built-in imports
import os
import pathlib
import shlex
import subprocess
import threading

# third-party imports
from avalon_framework import Avalon


class Waifu2xConverter:
    """This class communicates with waifu2x cui engine

    An object will be created for this class, containing information
    about the binary address and the processing method. When being called
    by the main program, other detailed information will be passed to
    the upscale function.
    """

    def __init__(self, driver_settings, model_dir):
        self.driver_settings = driver_settings
        self.driver_settings['model_dir'] = model_dir
        self.print_lock = threading.Lock()

    def upscale(self, input_directory, output_directory, scale_ratio, jobs, image_format):
        """ Waifu2x Converter Driver Upscaler
        This method executes the upscaling of extracted frames.

        Arguments:
            input_directory {string} -- source directory path
            output_directory {string} -- output directory path
            scale_ratio {int} -- frames' scale ratio
            threads {int} -- number of threads
        """

        # overwrite config file settings
        self.driver_settings['input'] = input_directory
        self.driver_settings['output'] = output_directory
        self.driver_settings['scale-ratio'] = scale_ratio
        self.driver_settings['jobs'] = jobs
        self.driver_settings['output-format'] = image_format

        # models_rgb must be specified manually for waifu2x-converter-cpp
        # if it's not specified in the arguments, create automatically
        if self.driver_settings['model-dir'] is None:
            self.driver_settings['model-dir'] = pathlib.Path(self.driver_settings['path']) / 'models_rgb'

        # list to be executed
        # initialize the list with waifu2x binary path as the first element
        execute = [str(pathlib.Path(self.driver_settings['path']) / 'waifu2x-converter-cpp.exe')]

        for key in self.driver_settings.keys():

            value = self.driver_settings[key]

            # the key doesn't need to be passed in this case
            if key == 'path':
                continue

            # null or None means that leave this option out (keep default)
            elif value is None or value is False:
                continue
            else:
                if len(key) == 1:
                    execute.append(f'-{key}')
                else:
                    execute.append(f'--{key}')

                # true means key is an option
                if value is True:
                    continue

                execute.append(str(value))

        # return the Popen object of the new process created
        self.print_lock.acquire()
        Avalon.debug_info(f'[upscaler] Subprocess {os.getpid()} executing: {shlex.join(execute)}')
        self.print_lock.release()
        return subprocess.Popen(execute)

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Waifu2x Caffe Driver
Author: K4YT3X
Date Created: May 3, 2020
Last Modified: May 3, 2020

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


class Anime4kCpp:
    """ Anime4K CPP wrapper
    """

    def __init__(self, driver_settings):
        self.driver_settings = driver_settings
        self.print_lock = threading.Lock()

    def upscale(self, input_file, output_file, zoom_factor, threads):
        """This is the core function for WAIFU2X class

        Arguments:
            input_file {string} -- source directory path
            output_file {string} -- output directory path
            width {int} -- output video width
            height {int} -- output video height
        """

        # overwrite config file settings
        self.driver_settings['input'] = input_file
        self.driver_settings['output'] = output_file
        self.driver_settings['zoomFactor'] = zoom_factor
        self.driver_settings['threads'] = threads

        # list to be executed
        # initialize the list with waifu2x binary path as the first element
        execute = [self.driver_settings.pop('path')]

        for key in self.driver_settings.keys():

            value = self.driver_settings[key]

            # is executable key or null or None means that leave this option out (keep default)
            if value is None or value is False:
                continue
            else:
                if len(key) == 1:
                    execute.append(f'-{key}')
                else:
                    execute.append(f'--{key}')

                # true means key is an option
                if value is not True:
                    execute.append(str(value))

        # return the Popen object of the new process created
        self.print_lock.acquire()
        Avalon.debug_info(f'[upscaler] Subprocess {os.getpid()} executing: {shlex.join(execute)}')
        self.print_lock.release()
        return subprocess.Popen(execute)

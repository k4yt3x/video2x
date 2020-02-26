#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Waifu2x NCNN Vulkan Driver
Creator: SAT3LL
Date Created: June 26, 2019
Last Modified: November 15, 2019

Editor: K4YT3X
Last Modified: February 22, 2020

Description: This class is a high-level wrapper
for waifu2x_ncnn_vulkan.
"""

# built-in imports
import os
import shlex
import subprocess
import threading

# third-party imports
from avalon_framework import Avalon


class Waifu2xNcnnVulkan:
    """This class communicates with waifu2x ncnn vulkan engine

    An object will be created for this class, containing information
    about the binary address and the processing method. When being called
    by the main program, other detailed information will be passed to
    the upscale function.
    """

    def __init__(self, driver_settings):
        self.driver_settings = driver_settings

        # arguments passed through command line overwrites config file values

        # waifu2x_ncnn_vulkan can't find its own model directory if its not in the current dir
        #   so change to it
        os.chdir(os.path.join(self.driver_settings['path'], '..'))

        self.print_lock = threading.Lock()

    def upscale(self, input_directory, output_directory, scale_ratio):
        """This is the core function for WAIFU2X class

        Arguments:
            input_directory {string} -- source directory path
            output_directory {string} -- output directory path
            ratio {int} -- output video ratio
        """

        # overwrite config file settings
        self.driver_settings['i'] = input_directory
        self.driver_settings['o'] = output_directory
        self.driver_settings['s'] = int(scale_ratio)

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

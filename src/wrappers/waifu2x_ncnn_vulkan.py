#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Waifu2x NCNN Vulkan Driver
Creator: SAT3LL
Date Created: June 26, 2019
Last Modified: May 4, 2020

Editor: K4YT3X
Last Modified: February 22, 2020

Description: This class is a high-level wrapper
for waifu2x_ncnn_vulkan.
"""

# built-in imports
import argparse
import os
import shlex
import subprocess
import threading

# third-party imports
from avalon_framework import Avalon


class WrapperMain:
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

    @staticmethod
    def parse_arguments(arguments):
        parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter, add_help=False)
        parser.add_argument('--help', action='help', help='show this help message and exit')
        parser.add_argument('-v', action='store_true', help='verbose output')
        # parser.add_argument('-i', type=pathlib.Path, help='input image path (jpg/png) or directory')
        # parser.add_argument('-o', type=pathlib.Path, help='output image path (png) or directory')
        parser.add_argument('-n', type=int, choices=range(-1, 4), help='denoise level')
        parser.add_argument('-s', type=int, choices=range(1, 3), help='upscale ratio')
        parser.add_argument('-t', type=int, help='tile size (>=32)')
        parser.add_argument('-m', type=str, help='waifu2x model path')
        parser.add_argument('-g', type=int, help='gpu device to use')
        parser.add_argument('-j', type=str, help='thread count for load/proc/save')
        parser.add_argument('-x', action='store_true', help='enable tta mode')
        return parser.parse_args(arguments)

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
        execute = [self.driver_settings.pop('path')]

        for key in self.driver_settings.keys():

            value = self.driver_settings[key]

            # null or None means that leave this option out (keep default)
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

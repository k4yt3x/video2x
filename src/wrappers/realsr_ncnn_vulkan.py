#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: RealSR NCNN Vulkan Driver
Creator: K4YT3X
Date Created: May 26, 2020
Last Modified: September 9, 2020

Description: This class is a high-level wrapper
for realsr_ncnn_vulkan.
"""

# built-in imports
import argparse
import os
import pathlib
import platform
import subprocess
import threading

# third-party imports
from avalon_framework import Avalon


class WrapperMain:
    """This class communicates with RealSR NCNN Vulkan engine

    An object will be created for this class, containing information
    about the binary address and the processing method. When being called
    by the main program, other detailed information will be passed to
    the upscale function.
    """

    def __init__(self, driver_settings):
        self.driver_settings = driver_settings
        self.print_lock = threading.Lock()

    @staticmethod
    def parse_arguments(arguments):
        parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter, add_help=False)
        parser.error = lambda message: (_ for _ in ()).throw(AttributeError(message))
        parser.add_argument('--help', action='help', help='show this help message and exit')
        parser.add_argument('-v', action='store_true', help='verbose output')
        parser.add_argument('-i', type=str, help=argparse.SUPPRESS)  # help='input image path (jpg/png) or directory')
        parser.add_argument('-o', type=str, help=argparse.SUPPRESS)  # help='output image path (png) or directory')
        parser.add_argument('-s', type=int, help='upscale ratio')
        parser.add_argument('-t', type=int, help='tile size (>=32/0=auto)')
        parser.add_argument('-m', type=str, help='realsr model path')
        parser.add_argument('-g', type=int, help='gpu device to use')
        parser.add_argument('-j', type=str, help='thread count for load/proc/save')
        parser.add_argument('-x', action='store_true', help='enable tta mode')
        return parser.parse_args(arguments)

    def load_configurations(self, upscaler):
        # self.driver_settings['s'] = int(upscaler.scale_ratio)
        self.driver_settings['j'] = '{}:{}:{}'.format(upscaler.processes, upscaler.processes, upscaler.processes)

    def set_scale_ratio(self, scale_ratio: int):
        self.driver_settings['s'] = int(scale_ratio)

    def upscale(self, input_directory, output_directory):
        """This is the core function for RealSR NCNN Vulkan class

        Arguments:
            input_directory {string} -- source directory path
            output_directory {string} -- output directory path
            ratio {int} -- output video ratio
        """

        # change the working directory to the binary's parent directory
        # so the binary can find shared object files and other files
        os.chdir(pathlib.Path(self.driver_settings['path']).parent)

        # overwrite config file settings
        self.driver_settings['i'] = input_directory
        self.driver_settings['o'] = output_directory

        # by default, realsr-ncnn-vulkan will look for the models under the current working directory
        # change the working directory to its containing folder if model directory not specified
        if self.driver_settings['m'] is None and platform.system() == 'Windows':
            os.chdir(pathlib.Path(self.driver_settings['path']).parent)

        # list to be executed
        # initialize the list with the binary path as the first element
        execute = [self.driver_settings['path']]

        for key in self.driver_settings.keys():

            value = self.driver_settings[key]

            # null or None means that leave this option out (keep default)
            if key == 'path' or value is None or value is False:
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
        Avalon.debug_info(f'[upscaler] Subprocess {os.getpid()} executing: {" ".join(execute)}')
        self.print_lock.release()
        return subprocess.Popen(execute)

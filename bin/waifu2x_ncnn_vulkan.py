#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Waifu2x NCNN Vulkan Driver
Author: SAT3LL
Date Created: June 26, 2019
Last Modified: August 3, 2019

Dev: K4YT3X

Description: This class is a high-level wrapper
for waifu2x_ncnn_vulkan.
"""

# built-in imports
import os
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

    def __init__(self, waifu2x_settings):
        self.waifu2x_settings = waifu2x_settings

        # arguments passed through command line overwrites config file values

        # waifu2x_ncnn_vulkan can't find its own model directory if its not in the current dir
        #   so change to it
        os.chdir(os.path.join(self.waifu2x_settings['waifu2x_ncnn_vulkan_path'], '..'))

        self.print_lock = threading.Lock()

    def upscale(self, input_directory, output_directory, scale_ratio, upscaler_exceptions):
        """This is the core function for WAIFU2X class

        Arguments:
            input_directory {string} -- source directory path
            output_directory {string} -- output directory path
            ratio {int} -- output video ratio
        """

        try:
            # overwrite config file settings
            self.waifu2x_settings['i'] = input_directory
            self.waifu2x_settings['o'] = output_directory
            self.waifu2x_settings['s'] = scale_ratio

            # print thread start message
            self.print_lock.acquire()
            Avalon.debug_info(f'[upscaler] Thread {threading.current_thread().name} started')
            self.print_lock.release()

            # list to be executed
            # initialize the list with waifu2x binary path as the first element
            execute = [str(self.waifu2x_settings['waifu2x_ncnn_vulkan_path'])]

            for key in self.waifu2x_settings.keys():

                value = self.waifu2x_settings[key]

                # is executable key or null or None means that leave this option out (keep default)
                if key == 'waifu2x_ncnn_vulkan_path' or value is None or value is False:
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

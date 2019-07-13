#!/usr/bin/env python3
# -*- coding: future_fstrings -*-


"""
Name: Waifu2x NCNN Vulkan Driver
Author: SAT3LL
Date Created: June 26, 2019
Last Modified: June 26, 2019

Dev: K4YT3X

Description: This class is a high-level wrapper
for waifu2x_ncnn_vulkan.
"""
from avalon_framework import Avalon
import os
import subprocess
import threading


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
        # os.chdir(os.path.join(self.waifu2x_settings['waifu2x_ncnn_vulkan_path'], '..'))

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
            self.waifu2x_settings['input'] = input_directory
            self.waifu2x_settings['output'] = output_directory

            # print thread start message
            self.print_lock.acquire()
            Avalon.debug_info(f'[upscaler] Thread {threading.current_thread().name} started')
            self.print_lock.release()

            # waifu2x_ncnn_vulkan does not have long-opts, we'll have a dictionary that maps "our" config long-opt
            # names to their short opts
            waifu2x_ncnn_vulkan_opt_flag = {
                'input': '-i',
                'output': '-o',
                'noise-level': '-n',
                'scale-ratio': '-s',
                'tile-size': '-t',
                'model-path': '-m',
                'gpu': '-g',
                'load-proc-save_threads': '-j',
                'verbose': '-v'
            }

            execute = [self.waifu2x_settings['waifu2x_ncnn_vulkan_path']]
            for key in self.waifu2x_settings.keys():
                value = self.waifu2x_settings[key]
                if key == 'waifu2x_ncnn_vulkan_path':
                    continue
                elif key == 'input':
                    execute.append(waifu2x_ncnn_vulkan_opt_flag[key])
                    execute.append(input_directory)
                elif key == 'output':
                    execute.append(waifu2x_ncnn_vulkan_opt_flag[key])
                    execute.append(output_directory)
                elif key == 'scale-ratio':
                    execute.append(waifu2x_ncnn_vulkan_opt_flag[key])
                    # waifu2x_ncnn_vulkan does not accept an arbitrary scale ratio, max is 2
                    if scale_ratio == 1:
                        execute.append('1')
                    else:
                        execute.append('2')
                # allow upper if cases to take precedence
                elif value is None or value is False:
                    continue
                else:
                    execute.append(waifu2x_ncnn_vulkan_opt_flag[key])
                    execute.append(str(value))

            Avalon.debug_info(f'Executing: {execute}')
            subprocess.run(execute, check=True, stderr=subprocess.DEVNULL)

            # print thread exiting message
            self.print_lock.acquire()
            Avalon.debug_info(f'[upscaler] Thread {threading.current_thread().name} exiting')
            self.print_lock.release()

            return 0
        except Exception as e:
            upscaler_exceptions.append(e)

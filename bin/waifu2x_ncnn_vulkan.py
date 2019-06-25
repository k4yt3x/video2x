#!/usr/bin/env python3
# -*- coding: future_fstrings -*-


"""
Name: Waifu2x NCNN Vulkan Driver
Author: K4YT3X, SAT3LL

Description: This class is a high-level wrapper
for waifu2x-ncnn-vulkan.
"""
from avalon_framework import Avalon
import subprocess
import threading
import os

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

        # waifu2x-ncnn-vulkan can't find its own model directory if its not in the current dir
        #   so change to it
        os.chdir(os.path.join(self.waifu2x_settings['waifu2x-ncnn-vulkan_path'], '..'))

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
            self.waifu2x_settings['input_path'] = input_directory
            self.waifu2x_settings['output_path'] = output_directory

            # print thread start message
            self.print_lock.acquire()
            Avalon.debug_info(f'[upscaler] Thread {threading.current_thread().name} started')
            self.print_lock.release()

            # waifu2x-ncnn-vulkan accepts arguments in a positional manner
            # See: https://github.com/nihui/waifu2x-ncnn-vulkan#usage
            # waifu2x-ncnn-vulkan.exe [input image] [output png] [noise=-1/0/1/2/3] [scale=1/2] [blocksize=400]
            #     noise = noise level, large value means strong denoise effect, -1=no effect
            #     scale = scale level, 1=no scale, 2=upscale 2x
            #     blocksize = tile size, use smaller value to reduce GPU memory usage, default is 400

            # waifu2x-ncnn-vulkan does not accept an arbitrary scale ratio, max is 2
            if scale_ratio == 1:
                for raw_frame in os.listdir(input_directory):
                    command = [
                        os.path.join(input_directory, raw_frame),
                        os.path.join(output_directory, raw_frame),
                        str(self.waifu2x_settings['noise-level']),
                        '1',
                        str(self.waifu2x_settings['block-size'])
                    ]
                    execute = [self.waifu2x_settings['waifu2x-ncnn-vulkan_path']]
                    execute.extend(command)

                    Avalon.debug_info(f'Executing: {execute}')
                    subprocess.run(execute, check=True, stderr=subprocess.DEVNULL)
            else:
                for raw_frame in os.listdir(input_directory):
                    command = [
                        os.path.join(input_directory, raw_frame),
                        os.path.join(output_directory, raw_frame),
                        str(self.waifu2x_settings['noise-level']),
                        '2',
                        str(self.waifu2x_settings['block-size'])
                    ]
                    execute = [self.waifu2x_settings['waifu2x-ncnn-vulkan_path']]
                    execute.extend(command)

                    Avalon.debug_info(f'Executing: {execute}')
                    subprocess.run(execute, check=True, stderr=subprocess.DEVNULL)

            # print thread exiting message
            self.print_lock.acquire()
            Avalon.debug_info(f'[upscaler] Thread {threading.current_thread().name} exiting')
            self.print_lock.release()

            return 0
        except Exception as e:
            upscaler_exceptions.append(e)

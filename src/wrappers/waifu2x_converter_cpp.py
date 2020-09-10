#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Waifu2x Converter CPP Driver
Author: K4YT3X
Date Created: February 8, 2019
Last Modified: September 9, 2020

Description: This class is a high-level wrapper
for waifu2x-converter-cpp.
"""

# built-in imports
import argparse
import os
import pathlib
import subprocess
import threading

# third-party imports
from avalon_framework import Avalon


class WrapperMain:
    """This class communicates with waifu2x cui engine

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
        parser.add_argument('--list-supported-formats', action='store_true', help='dump currently supported format list')
        parser.add_argument('--list-opencv-formats', action='store_true', help='(deprecated. Use --list-supported-formats) dump opencv supported format list')
        parser.add_argument('-l', '--list-processor', action='store_true', help='dump processor list')
        parser.add_argument('-f', '--output-format', choices=['png', 'jpg'], help='The format used when running in recursive/folder mode\nSee --list-supported-formats for a list of supported formats/extensions.')
        parser.add_argument('-c', '--png-compression', type=int, choices=range(10), help='Set PNG compression level (0-9), 9 = Max compression (slowest & smallest)')
        parser.add_argument('-q', '--image-quality', type=int, choices=range(-1, 102), help='JPEG & WebP Compression quality (0-101, 0 being smallest size and lowest quality), use 101 for lossless WebP')
        parser.add_argument('--block-size', type=int, help='block size')
        parser.add_argument('--disable-gpu', action='store_true', help='disable GPU')
        parser.add_argument('--force-OpenCL', action='store_true', help='force to use OpenCL on Intel Platform')
        parser.add_argument('-p', '--processor', type=int, help='set target processor')
        parser.add_argument('-j', '--jobs', type=int, help='number of threads launching at the same time')
        parser.add_argument('--model-dir', type=str, help='path to custom model directory (don\'t append last / )')
        parser.add_argument('--scale-ratio', type=float, help='custom scale ratio')
        parser.add_argument('--noise-level', type=int, choices=range(4), help='noise reduction level')
        parser.add_argument('-m', '--mode', choices=['noise', 'scale', 'noise-scale'], help='image processing mode')
        parser.add_argument('-v', '--log-level', type=int, choices=range(5), help='Set log level')
        parser.add_argument('-s', '--silent', action='store_true', help='Enable silent mode. (same as --log-level 1)')
        parser.add_argument('-t', '--tta', type=int, choices=range(2), help='Enable Test-Time Augmentation mode.')
        parser.add_argument('-g', '--generate-subdir', type=int, choices=range(2), help='Generate sub folder when recursive directory is enabled.')
        parser.add_argument('-a', '--auto-naming', type=int, choices=range(2), help='Add postfix to output name when output path is not specified.\nSet 0 to disable this.')
        parser.add_argument('-r', '--recursive-directory', type=int, choices=range(2), help='Search recursively through directories to find more images to process.')
        parser.add_argument('-o', '--output', type=str, help=argparse.SUPPRESS)  # help='path to output image file or directory  (you should use the full path)')
        parser.add_argument('-i', '--input', type=str, help=argparse.SUPPRESS)  # help='(required)  path to input image file or directory (you should use the full path)')
        parser.add_argument('--version', action='store_true', help='Displays version information and exits.')
        return parser.parse_args(arguments)

    def load_configurations(self, upscaler):
        # self.driver_settings['scale-ratio'] = upscaler.scale_ratio
        self.driver_settings['jobs'] = upscaler.processes
        self.driver_settings['output-format'] = upscaler.extracted_frame_format.lower()

    def set_scale_ratio(self, scale_ratio: float):
        self.driver_settings['scale-ratio'] = scale_ratio

    def upscale(self, input_directory, output_directory):
        """ Waifu2x Converter Driver Upscaler
        This method executes the upscaling of extracted frames.

        Arguments:
            input_directory {string} -- source directory path
            output_directory {string} -- output directory path
            scale_ratio {int} -- frames' scale ratio
            threads {int} -- number of threads
        """

        # change the working directory to the binary's parent directory
        # so the binary can find shared object files and other files
        os.chdir(pathlib.Path(self.driver_settings['path']).parent)

        # overwrite config file settings
        self.driver_settings['input'] = input_directory
        self.driver_settings['output'] = output_directory

        # models_rgb must be specified manually for waifu2x-converter-cpp
        # if it's not specified in the arguments, create automatically
        if self.driver_settings['model-dir'] is None:
            self.driver_settings['model-dir'] = pathlib.Path(self.driver_settings['path']).parent / 'models_rgb'

        # list to be executed
        # initialize the list with waifu2x binary path as the first element
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

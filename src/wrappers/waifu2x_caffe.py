#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Waifu2x Caffe Driver
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: September 12, 2020

Description: This class is a high-level wrapper
for waifu2x-caffe.
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
        parser.add_argument('-t', '--tta', type=int, choices=range(2), help='8x slower and slightly high quality')
        parser.add_argument('--gpu', type=int, help='gpu device no')
        parser.add_argument('-b', '--batch_size', type=int, help='input batch size')
        parser.add_argument('--crop_h', type=int, help='input image split size(height)')
        parser.add_argument('--crop_w', type=int, help='input image split size(width)')
        parser.add_argument('-c', '--crop_size', type=int, help='input image split size')
        parser.add_argument('-d', '--output_depth', type=int, help='output image chaneel depth bit')
        parser.add_argument('-q', '--output_quality', type=int, help='output image quality')
        parser.add_argument('-p', '--process', choices=['cpu', 'gpu', 'cudnn'], help='process mode')
        parser.add_argument('--model_dir', type=str, help='path to custom model directory (don\'t append last / )')
        parser.add_argument('-h', '--scale_height', type=int, help='custom scale height')
        parser.add_argument('-w', '--scale_width', type=int, help='custom scale width')
        parser.add_argument('-s', '--scale_ratio', type=float, help='custom scale ratio')
        parser.add_argument('-n', '--noise_level', type=int, choices=range(4), help='noise reduction level')
        parser.add_argument('-m', '--mode', choices=['noise', 'scale', 'noise_scale', 'auto_scale'], help='image processing mode')
        parser.add_argument('-e', '--output_extention', type=str, help='extention to output image file when output_path is (auto) or input_path is folder')
        parser.add_argument('-l', '--input_extention_list', type=str, help='extention to input image file when input_path is folder')
        parser.add_argument('-o', '--output_path', type=str, help=argparse.SUPPRESS)  # help='path to output image file (when input_path is folder, output_path must be folder)')
        parser.add_argument('-i', '--input_path', type=str, help=argparse.SUPPRESS)  # help='(required) path to input image file')
        return parser.parse_args(arguments)

    def load_configurations(self, upscaler):
        # use scale width and scale height if specified
        # self.driver_settings['scale_ratio'] = upscaler.scale_ratio
        self.driver_settings['output_extention'] = upscaler.extracted_frame_format

        # bit_depth will be 12 at this point
        # it will up updated later
        self.driver_settings['output_depth'] = 12

    def set_scale_resolution(self, width: int, height: int):
        self.driver_settings['scale_width'] = width
        self.driver_settings['scale_height'] = height
        self.driver_settings['scale_ratio'] = None

    def set_scale_ratio(self, scale_ratio: float):
        self.driver_settings['scale_width'] = None
        self.driver_settings['scale_height'] = None
        self.driver_settings['scale_ratio'] = scale_ratio

    def upscale(self, input_directory, output_directory):
        """ start upscaling process
        """

        # change the working directory to the binary's parent directory
        # so the binary can find shared object files and other files
        os.chdir(pathlib.Path(self.driver_settings['path']).parent)

        # overwrite config file settings
        self.driver_settings['input_path'] = input_directory
        self.driver_settings['output_path'] = output_directory

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

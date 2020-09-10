#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Anime4KCPP Driver
Author: K4YT3X
Date Created: May 3, 2020
Last Modified: September 9, 2020

Description: This class is a high-level wrapper
for Anime4KCPP.
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
    """ Anime4K CPP wrapper
    """

    def __init__(self, driver_settings):
        self.driver_settings = driver_settings
        self.print_lock = threading.Lock()

    @staticmethod
    def zero_to_one_float(value):
        value = float(value)
        if value < 0.0 or value > 1.0:
            raise argparse.ArgumentTypeError(f'{value} is not between 0.0 and 1.0')
        return value

    @staticmethod
    def parse_arguments(arguments):
        parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter, add_help=False)
        parser.error = lambda message: (_ for _ in ()).throw(AttributeError(message))
        parser.add_argument('--help', action='help', help='show this help message and exit')
        parser.add_argument('-i', '--input', type=str, help=argparse.SUPPRESS)  # help='File for loading')
        parser.add_argument('-o', '--output', type=str, help=argparse.SUPPRESS)  # help='File for outputting')
        parser.add_argument('-p', '--passes', type=int, help='Passes for processing')
        parser.add_argument('-n', '--pushColorCount', type=int, help='Limit the number of color pushes')
        parser.add_argument('-c', '--strengthColor', type=WrapperMain.zero_to_one_float, help='Strength for pushing color,range 0 to 1,higher for thinner')
        parser.add_argument('-g', '--strengthGradient', type=WrapperMain.zero_to_one_float, help='Strength for pushing gradient,range 0 to 1,higher for sharper')
        parser.add_argument('-z', '--zoomFactor', type=float, help='zoom factor for resizing')
        parser.add_argument('-t', '--threads', type=int, help='Threads count for video processing')
        parser.add_argument('-f', '--fastMode', action='store_true', help='Faster but maybe low quality')
        parser.add_argument('-v', '--videoMode', action='store_true', help='Video process')
        parser.add_argument('-s', '--preview', action='store_true', help='Preview image')
        parser.add_argument('-b', '--preprocessing', action='store_true', help='Enable pre processing')
        parser.add_argument('-a', '--postprocessing', action='store_true', help='Enable post processing')
        parser.add_argument('-r', '--preFilters', type=int, help='Enhancement filter, only working when preProcessing is true,there are 5 options by binary:Median blur=0000001, Mean blur=0000010, CAS Sharpening=0000100, Gaussian blur weak=0001000, Gaussian blur=0010000, Bilateral filter=0100000, Bilateral filter faster=1000000, you can freely combine them, eg: Gaussian blur weak + Bilateral filter = 0001000 | 0100000 = 0101000 = 40(D)')
        parser.add_argument('-e', '--postFilters', type=int, help='Enhancement filter, only working when postProcessing is true,there are 5 options by binary:Median blur=0000001, Mean blur=0000010, CAS Sharpening=0000100, Gaussian blur weak=0001000, Gaussian blur=0010000, Bilateral filter=0100000, Bilateral filter faster=1000000, you can freely combine them, eg: Gaussian blur weak + Bilateral filter = 0001000 | 0100000 = 0101000 = 40(D), so you can put 40 to enable Gaussian blur weak and Bilateral filter, which also is what I recommend for image that < 1080P, 48 for image that >= 1080P, and for performance I recommend to use 72 for video that < 1080P, 80 for video that >=1080P')
        parser.add_argument('-q', '--GPUMode', action='store_true', help='Enable GPU acceleration')
        parser.add_argument('-w', '--CNNMode', action='store_true', help='Enable ACNet')
        parser.add_argument('-H', '--HDN', action='store_true', help='Enable HDN mode for ACNet')
        parser.add_argument('-L', '--HDNLevel', type=int, help='Set HDN level')
        parser.add_argument('-l', '--listGPUs', action='store_true', help='list GPUs')
        parser.add_argument('-h', '--platformID', type=int, help='Specify the platform ID')
        parser.add_argument('-d', '--deviceID', type=int, help='Specify the device ID')
        parser.add_argument('-C', '--codec', type=str, help='Specify the codec for encoding from mp4v(recommended in Windows), dxva(for Windows), avc1(H264, recommended in Linux), vp09(very slow), hevc(not support in Windowds), av01(not support in Windowds) (string [=mp4v])')
        parser.add_argument('-F', '--forceFps', type=float, help='Set output video fps to the specifying number, 0 to disable')
        parser.add_argument('-D', '--disableProgress', action='store_true', help='disable progress display')
        parser.add_argument('-W', '--webVideo', type=str, help='process the video from URL')
        parser.add_argument('-A', '--alpha', action='store_true', help='preserve the Alpha channel for transparent image')
        return parser.parse_args(arguments)

    def load_configurations(self, upscaler):
        # self.driver_settings['zoomFactor'] = upscaler.scale_ratio
        self.driver_settings['threads'] = upscaler.processes

        # append FFmpeg path to the end of PATH
        # Anime4KCPP will then use FFmpeg to migrate audio tracks
        os.environ['PATH'] += f';{upscaler.ffmpeg_settings["ffmpeg_path"]}'

    def set_scale_ratio(self, scale_ratio: float):
        self.driver_settings['zoomFactor'] = scale_ratio

    def upscale(self, input_file, output_file):
        """This is the core function for WAIFU2X class

        Arguments:
            input_file {string} -- source directory path
            output_file {string} -- output directory path
            width {int} -- output video width
            height {int} -- output video height
        """

        # change the working directory to the binary's parent directory
        # so the binary can find shared object files and other files
        os.chdir(pathlib.Path(self.driver_settings['path']).parent)

        # overwrite config file settings
        self.driver_settings['input'] = input_file
        self.driver_settings['output'] = output_file

        # Anime4KCPP will look for Anime4KCPPKernel.cl under the current working directory
        # change the CWD to its containing directory so it will find it
        if platform.system() == 'Windows':
            os.chdir(pathlib.Path(self.driver_settings['path']).parent)

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

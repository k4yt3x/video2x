#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Waifu2x Caffe Driver
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: March 9, 2019

Description: This class controls waifu2x
engine
"""
from avalon_framework import Avalon
import subprocess
import threading


class Waifu2xCaffe:
    """This class communicates with waifu2x cui engine

    An object will be created for this class, containing information
    about the binary address and the processing method. When being called
    by the main program, other detailed information will be passed to
    the upscale function.
    """

    def __init__(self, waifu2x_settings, process, model_dir):
        self.waifu2x_settings = waifu2x_settings
        self.waifu2x_settings['process'] = process
        self.waifu2x_settings['model_dir'] = model_dir

        # arguments passed through command line overwrites config file values
        self.process = process
        self.model_dir = model_dir
        self.print_lock = threading.Lock()

    def upscale(self, input_folder, output_folder, scale_ratio, scale_width, scale_height):
        """This is the core function for WAIFU2X class

        Arguments:
            input_folder {string} -- source folder path
            output_folder {string} -- output folder path
            width {int} -- output video width
            height {int} -- output video height
        """

        # overwrite config file settings
        self.waifu2x_settings['input_path'] = input_folder
        self.waifu2x_settings['output_path'] = output_folder

        if scale_ratio:
            self.waifu2x_settings['scale_ratio'] = scale_ratio
        elif scale_width and scale_height:
            self.waifu2x_settings['scale_width'] = scale_width
            self.waifu2x_settings['scale_height'] = scale_height

        # Print thread start message
        self.print_lock.acquire()
        Avalon.debug_info('[upscaler] Thread {} started'.format(threading.current_thread().name))
        self.print_lock.release()

        """
        # Create string for execution
        execute = '\"{}\" -p {} -I png -i \"{}\" -e png -o {} -w {} -h {} -n 3 -m noise_scale -y {}'.format(
            self.waifu2x_path, self.process, input_folder, output_folder, width, height, self.model_dir)
        """

        execute = []

        for key in self.waifu2x_settings.keys():

            value = self.waifu2x_settings[key]

            # The key doesn't need to be passed in this case
            if key == 'waifu2x_caffe_path':
                execute.append(str(value))

            # Null or None means that leave this option out (keep default)
            elif value is None or value is False:
                continue

            else:
                if len(key) == 1:
                    execute.append('-{}'.format(key))
                else:
                    execute.append('--{}'.format(key))
                execute.append(str(value))
        
        Avalon.debug_info('Executing: {}'.format(execute))
        subprocess.run(execute, check=True)

        # Print thread exiting message
        self.print_lock.acquire()
        Avalon.debug_info('[upscaler] Thread {} exiting'.format(threading.current_thread().name))
        self.print_lock.release()

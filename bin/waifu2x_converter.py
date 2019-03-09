#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Waifu2x Converter CPP Driver
Author: K4YT3X
Date Created: February 8, 2019
Last Modified: March 9, 2019

Description: This class controls waifu2x
engine
"""
from avalon_framework import Avalon
import subprocess
import threading


class Waifu2xConverter:
    """This class communicates with waifu2x cui engine

    An object will be created for this class, containing information
    about the binary address and the processing method. When being called
    by the main program, other detailed information will be passed to
    the upscale function.
    """

    def __init__(self, waifu2x_settings):
        self.waifu2x_settings = waifu2x_settings
        self.print_lock = threading.Lock()

    def upscale(self, input_folder, output_folder, scale_ratio, jobs):
        """ Waifu2x Converter Driver Upscaler
        This method executes the upscaling of extracted frames.

        Arguments:
            input_folder {string} -- source folder path
            output_folder {string} -- output folder path
            scale_ratio {int} -- frames' scale ratio
            threads {int} -- number of threads
        """

        # overwrite config file settings
        self.waifu2x_settings['input'] = input_folder
        self.waifu2x_settings['output'] = output_folder
        self.waifu2x_settings['scale_ratio'] = scale_ratio
        self.waifu2x_settings['jobs'] = jobs

        # models_rgb must be specified manually
        self.waifu2x_settings['model_dir'] = '{}\\models_rgb'.format(self.waifu2x_settings['waifu2x_converter_path'])

        # Print thread start message
        self.print_lock.acquire()
        Avalon.debug_info('[upscaler] Thread {} started'.format(threading.current_thread().name))
        self.print_lock.release()

        # Create list for execution
        execute = []

        for key in self.waifu2x_settings.keys():

            value = self.waifu2x_settings[key]

            # The key doesn't need to be passed in this case
            if key == 'waifu2x_converter_path':
                execute.append('{}\\waifu2x-converter-cpp.exe'.format(str(value)))

            # Null or None means that leave this option out (keep default)
            elif value is None or value is False:
                continue
            else:
                if len(key) == 1:
                    execute.append('-{}'.format(key))
                else:
                    execute.append('--{}'.format(key))

                # True means key is an option
                if value is True:
                    continue

                execute.append(str(value))
        
        Avalon.debug_info('Executing: {}'.format(execute))
        subprocess.run(execute, check=True)

        # Print thread exiting message
        self.print_lock.acquire()
        Avalon.debug_info('[upscaler] Thread {} exiting'.format(threading.current_thread().name))
        self.print_lock.release()

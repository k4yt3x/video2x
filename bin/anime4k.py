#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Anime4K Driver
Author: K4YT3X
Date Created: August 15, 2019
Last Modified: August 15, 2019

Description: This class is a high-level wrapper
for Anime4k.
"""

# local imports
import common

# built-in imports
import subprocess
import threading

# third-party imports
from avalon_framework import Avalon


class Anime4k:
    """This class communicates with Anime4K engine

    An object will be created for this class, containing information
    about the binary address and the processing method. When being called
    by the main program, other detailed information will be passed to
    the upscale function.
    """

    def __init__(self, settings):
        self.settings = settings
        self.print_lock = threading.Lock()

    def upscale(self, input_directory, output_directory, scale_ratio, upscaler_exceptions, push_strength=None, push_grad_strength=None):
        """ Anime4K wrapper

        Arguments:
            file_in {string} -- input file path
            file_out {string} -- output file path

        Keyword Arguments:
            scale {int} -- scale ratio (default: {None})
            push_strength {int} -- residual push strength (default: {None})
            push_grad_strength {int} -- residual gradient push strength (default: {None})

        Returns:
            subprocess.Popen.returncode -- command line return value of execution
        """
        try:
            # return value is the sum of all execution return codes
            return_value = 0

            # get a list lof all image files in input_directory
            extracted_frame_files = [f for f in input_directory.iterdir() if str(f).lower().endswith('.png') or str(f).lower().endswith('.jpg')]

            # Only print debug_info on first iteration.
            logged = False

            # upscale each image in input_directory
            for image in extracted_frame_files:

                execute = [
                    self.settings['java_path'] / self.settings['java_binary'],
                    '-jar',
                    self.settings['path'] / self.settings['binary'],
                    image.absolute(),
                    output_directory / image.name,
                    scale_ratio
                ]

                # optional arguments
                kwargs = [
                    'push_strength',
                    'push_grad_strength'
                ]

                # if optional argument specified, append value to execution list
                for arg in kwargs:
                    if locals()[arg] is not None:
                        execute.extend([locals([arg])])

                # turn all list elements into string to avoid errors
                execute = [str(e) for e in execute]

                if not logged:
                    self.print_lock.acquire()
                    Avalon.debug_info(f'Executing: {execute}')
                    self.print_lock.release()
                    logged = True

                return_value += subprocess.run(execute, check=True).returncode

            # print thread exiting message
            self.print_lock.acquire()
            Avalon.debug_info(f'[upscaler] Thread {threading.current_thread().name} exiting')
            self.print_lock.release()

            # return command execution return code
            return return_value
        except Exception as e:
            upscaler_exceptions.append(e)

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Waifu2x Converter CPP Driver
Author: K4YT3X
Date Created: February 8, 2019
Last Modified: February 8, 2019

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

    def __init__(self, waifu2x_path):
        self.waifu2x_path = waifu2x_path
        self.print_lock = threading.Lock()

    def upscale(self, folderin, folderout, scale_ratio, threads):
        """ Waifu2x Converter Driver Upscaler
        This method executes the upscaling of extracted frames.

        Arguments:
            folderin {string} -- source folder path
            folderout {string} -- output folder path
            scale_ratio {int} -- frames' scale ratio
            threads {int} -- number of threads
        """

        # Print thread start message
        self.print_lock.acquire()
        Avalon.debug_info('[upscaler] Thread {} started'.format(threading.current_thread().name))
        self.print_lock.release()

        # Create string for execution
        execute = '\"{}\\waifu2x-converter-cpp.exe\" -i \"{}\" -o {} --scale_ratio {} --noise_level 3 -m noise_scale -j {} --model_dir {}\\models_rgb'.format(
            self.waifu2x_path, folderin, folderout, scale_ratio, threads, self.waifu2x_path)
        subprocess.call(execute)

        # Print thread exiting message
        self.print_lock.acquire()
        Avalon.debug_info('[upscaler] Thread {} exiting'.format(threading.current_thread().name))
        self.print_lock.release()

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Anime4K Driver
Author: K4YT3X
Date Created: August 15, 2019
Last Modified: February 26, 2020

Description: This class is a high-level wrapper
for Anime4k.
"""

# built-in imports
import os
import queue
import shlex
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

    def __init__(self, driver_settings):
        self.driver_settings = driver_settings
        self.print_lock = threading.Lock()

    def upscale(self, input_directory, output_directory, scale_ratio, processes, push_strength=None, push_grad_strength=None):
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

        # a list of all commands to be executed
        commands = queue.Queue()

        # get a list lof all image files in input_directory
        extracted_frame_files = [f for f in input_directory.iterdir() if str(f).lower().endswith('.png') or str(f).lower().endswith('.jpg')]

        # upscale each image in input_directory
        for image in extracted_frame_files:

            execute = [
                self.driver_settings['java_path'],
                '-jar',
                self.driver_settings['path'],
                str(image.absolute()),
                str(output_directory / image.name),
                str(scale_ratio)
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

            commands.put(execute)

        # initialize two lists to hold running and finished processes
        anime4k_running_processes = []
        anime4k_finished_processes = []

        # run all commands in queue
        while not commands.empty():

            # if any commands have completed
            # remove the subprocess.Popen project and move it into finished processes
            for process in anime4k_running_processes:
                if process.poll() is not None:
                    Avalon.debug_info(f'Subprocess {process.pid} exited with code {process.poll()}')
                    anime4k_finished_processes.append(process)
                    anime4k_running_processes.remove(process)

            # when number running processes is less than what's specified
            # create new processes and add to running process pool
            while len(anime4k_running_processes) < processes:
                next_in_queue = commands.get()
                new_process = subprocess.Popen(next_in_queue)
                anime4k_running_processes.append(new_process)

                self.print_lock.acquire()
                Avalon.debug_info(f'[upscaler] Subprocess {new_process.pid} executing: {shlex.join(next_in_queue)}')
                self.print_lock.release()

        # return command execution return code
        return anime4k_finished_processes

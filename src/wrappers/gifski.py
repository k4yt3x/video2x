#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Gifski Wrapper
Creator: K4YT3X
Date Created: May 11, 2020
Last Modified: September 13, 2020

Description: High-level wrapper for Gifski.
"""

# built-in imports
import pathlib
import subprocess

# third-party imports
from avalon_framework import Avalon


class Gifski:

    def __init__(self, gifski_settings):
        self.gifski_settings = gifski_settings

    def make_gif(self, upscaled_frames: pathlib.Path, output_path: pathlib.Path, framerate: float, extracted_frame_format: str, output_width: int, output_height: int) -> subprocess.Popen:
        execute = [
            self.gifski_settings['gifski_path'],
            '-o',
            output_path,
            '--fps',
            int(round(framerate, 0)),
            '--width',
            output_width,
            '--height',
            output_height
        ]

        # load configurations from config file
        execute.extend(self._load_configuration())

        # append frames location
        execute.extend([upscaled_frames / f'extracted_*.{extracted_frame_format}'])

        return(self._execute(execute))

    def _load_configuration(self):

        configuration = []

        for key in self.gifski_settings.keys():

            value = self.gifski_settings[key]

            # null or None means that leave this option out (keep default)
            if key == 'gifski_path' or value is None or value is False:
                continue
            else:
                if len(key) == 1:
                    configuration.append(f'-{key}')
                else:
                    configuration.append(f'--{key}')

                # true means key is an option
                if value is not True:
                    configuration.append(str(value))
        return configuration

    def _execute(self, execute: list) -> subprocess.Popen:
        # turn all list elements into string to avoid errors
        execute = [str(e) for e in execute]

        Avalon.debug_info(f'Executing: {execute}')

        return subprocess.Popen(execute)

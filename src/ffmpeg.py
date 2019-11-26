#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2X FFmpeg Controller
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: November 15, 2019

Description: This class handles all FFmpeg related operations.
"""

# built-in imports
import json
import pathlib
import subprocess

# third-party imports
from avalon_framework import Avalon


class Ffmpeg:
    """This class communicates with FFmpeg

    This class deals with FFmpeg. It handles extracting
    frames, stripping audio, converting images into videos
    and inserting audio tracks to videos.
    """

    def __init__(self, ffmpeg_settings, image_format):
        self.ffmpeg_settings = ffmpeg_settings

        self.ffmpeg_path = pathlib.Path(self.ffmpeg_settings['ffmpeg_path'])
        self.ffmpeg_binary = self.ffmpeg_path / 'ffmpeg'
        self.ffmpeg_probe_binary = self.ffmpeg_path / 'ffprobe'
        self.image_format = image_format
        self.pixel_format = None

    def get_pixel_formats(self):
        """ Get a dictionary of supported pixel formats

        List all supported pixel formats and their
        corresponding bit depth.

        Returns:
            dictionary -- JSON dict of all pixel formats to bit depth
        """
        execute = [
            self.ffmpeg_probe_binary,
            '-v',
            'quiet',
            '-pix_fmts'
        ]

        # turn elements into str
        execute = [str(e) for e in execute]

        Avalon.debug_info(f'Executing: {" ".join(execute)}')

        # initialize dictionary to store pixel formats
        pixel_formats = {}

        # record all pixel formats into dictionary
        for line in subprocess.run(execute, check=True, stdout=subprocess.PIPE).stdout.decode().split('\n'):
            try:
                pixel_formats[' '.join(line.split()).split()[1]] = int(' '.join(line.split()).split()[3])
            except (IndexError, ValueError):
                pass

        # print pixel formats for debugging
        Avalon.debug_info(str(pixel_formats))

        return pixel_formats

    def get_video_info(self, input_video):
        """ Gets input video information

        This method reads input video information
        using ffprobe in dictionary

        Arguments:
            input_video {string} -- input video file path

        Returns:
            dictionary -- JSON text of input video information
        """

        # this execution command needs to be hard-coded
        # since video2x only strictly recignizes this one format
        execute = [
            self.ffmpeg_probe_binary,
            '-v',
            'quiet',
            '-print_format',
            'json',
            '-show_format',
            '-show_streams',
            '-i',
            input_video
        ]

        # turn elements into str
        execute = [str(e) for e in execute]

        Avalon.debug_info(f'Executing: {" ".join(execute)}')
        json_str = subprocess.run(execute, check=True, stdout=subprocess.PIPE).stdout
        return json.loads(json_str.decode('utf-8'))

    def extract_frames(self, input_video, extracted_frames):
        """Extract every frame from original videos

        This method extracts every frame from input video using FFmpeg

        Arguments:
            input_video {string} -- input video path
            extracted_frames {string} -- video output directory
        """
        execute = [
            self.ffmpeg_binary
        ]

        execute.extend(self._read_configuration(phase='video_to_frames'))

        execute.extend([
            '-i',
            input_video
        ])

        execute.extend(self._read_configuration(phase='video_to_frames', section='output_options'))

        execute.extend([
            extracted_frames / f'extracted_%0d.{self.image_format}'
        ])

        self._execute(execute)

    def convert_video(self, framerate, resolution, upscaled_frames):
        """Converts images into videos

        This method converts a set of images into a video

        Arguments:
            framerate {float} -- target video framerate
            resolution {string} -- target video resolution
            upscaled_frames {string} -- source images directory
        """
        execute = [
            self.ffmpeg_binary,
            '-r',
            str(framerate),
            '-s',
            resolution
        ]

        # read other options
        execute.extend(self._read_configuration(phase='frames_to_video'))

        # read FFmpeg input options
        execute.extend(self._read_configuration(phase='frames_to_video', section='input_options'))

        # WORKAROUND FOR WAIFU2X-NCNN-VULKAN
        # Dev: SAT3LL
        # rename all .png.png suffixes to .png
        import re
        regex = re.compile(r'\.png\.png$', re.IGNORECASE)
        for frame_name in upscaled_frames.iterdir():
            (upscaled_frames / frame_name).rename(upscaled_frames / regex.sub('.png', str(frame_name)))
        # END WORKAROUND

        # append input frames path into command
        execute.extend([
            '-i',
            upscaled_frames / f'extracted_%d.{self.image_format}'
        ])

        # read FFmpeg output options
        execute.extend(self._read_configuration(phase='frames_to_video', section='output_options'))

        # specify output file location
        execute.extend([
            upscaled_frames / 'no_audio.mp4'
        ])

        self._execute(execute)

    def migrate_audio_tracks_subtitles(self, input_video, output_video, upscaled_frames):
        """ Migrates audio tracks and subtitles from input video to output video

        Arguments:
            input_video {string} -- input video file path
            output_video {string} -- output video file path
            upscaled_frames {string} -- directory containing upscaled frames
        """
        execute = [
            self.ffmpeg_binary
        ]

        execute.extend(self._read_configuration(phase='migrating_tracks'))

        execute.extend([
            '-i',
            upscaled_frames / 'no_audio.mp4',
            '-i',
            input_video
        ])

        execute.extend(self._read_configuration(phase='migrating_tracks', section='output_options'))

        execute.extend([
            output_video
        ])

        self._execute(execute)

    def _read_configuration(self, phase, section=None):
        """ read configuration from JSON

        Read the configurations (arguments) from the JSON
        configuration file and append them to the end of the
        FFmpeg command.

        Arguments:
            execute {list} -- list of arguments to be executed
            phase {str} -- phase of operation
        """

        configuration = []

        # if section is specified, read configurations or keys
        # from only that section
        if section:
            source = self.ffmpeg_settings[phase][section].keys()

            # if pixel format is not specified, use the source pixel format
            try:
                if self.ffmpeg_settings[phase][section].get('-pix_fmt') is None:
                    self.ffmpeg_settings[phase][section]['-pix_fmt'] = self.pixel_format
            except KeyError:
                pass
        else:
            source = self.ffmpeg_settings[phase].keys()

        for key in source:

            if section:
                value = self.ffmpeg_settings[phase][section][key]
            else:
                value = self.ffmpeg_settings[phase][key]

            # null or None means that leave this option out (keep default)
            if value is None or value is False or isinstance(value, dict):
                continue

            # if the value is a list, append the same argument and all values
            elif isinstance(value, list):

                for subvalue in value:
                    configuration.append(key)
                    if value is not True:
                        configuration.append(str(subvalue))

            # otherwise the value is typical
            else:
                configuration.append(key)

                # true means key is an option
                if value is True:
                    continue

                configuration.append(str(value))

        return configuration

    def _execute(self, execute):
        """ execute command

        Arguments:
            execute {list} -- list of arguments to be executed

        Returns:
            int -- execution return code
        """
        # turn all list elements into string to avoid errors
        execute = [str(e) for e in execute]

        Avalon.debug_info(f'Executing: {execute}')

        return subprocess.run(execute, check=True).returncode

#!/usr/bin/env python3
# -*- coding: future_fstrings -*-


"""
Name: FFMPEG Class
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: July 9, 2019

Description: This class handles all FFMPEG related
operations.
"""
from avalon_framework import Avalon
import json
import subprocess
import os
import sys

class Ffmpeg:
    """This class communicates with ffmpeg

    This class deals with ffmpeg. It handles extracitng
    frames, stripping audio, converting images into videos
    and inserting audio tracks to videos.
    """

    def __init__(self, ffmpeg_settings, image_format):
        self.ffmpeg_settings = ffmpeg_settings

        self.ffmpeg_path = self.ffmpeg_settings['ffmpeg_path']

        if sys.platform != 'win32':
            self.ffmpeg_binary = os.path.join(self.ffmpeg_path, 'ffmpeg')
            self.ffmpeg_probe_binary = os.path.join(self.ffmpeg_path, 'ffprobe')
        else:
            self.ffmpeg_binary = os.path.join(self.ffmpeg_path, 'ffmpeg.exe')
            self.ffmpeg_probe_binary = os.path.join(self.ffmpeg_path, 'ffprobe.exe')
            
        self.image_format = image_format
        self.pixel_format = None
    
    def get_pixel_formats(self):
        """ Get a dictionary of supported pixel formats

        List all supported pixel formats and their corresponding
        bit depth.

        Returns:
            dictionary -- JSON dict of all pixel formats to bit depth
        """
        execute = [
            self.ffmpeg_probe_binary,
            '-v',
            'quiet',
            '-pix_fmts'
        ]

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
        # this avalon command returns a code if it's not run in windows
        # so we'll catch to skip in linux until avalon is patched
        if sys.platform == 'win32':
            Avalon.debug_info(pixel_formats)

        return pixel_formats

    def get_video_info(self, input_video):
        """ Gets input video information

        This method reads input video information
        using ffprobe in dictionary.

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

        Avalon.debug_info(f'Executing: {" ".join(execute)}')
        json_str = subprocess.run(execute, check=True, stdout=subprocess.PIPE).stdout
        return json.loads(json_str.decode('utf-8'))

    def extract_frames(self, input_video, extracted_frames):
        """Extract every frame from original videos

        This method extracts every frame from videoin
        using ffmpeg

        Arguments:
            input_video {string} -- input video path
            extracted_frames {string} -- video output directory
        """
        execute = [
            self.ffmpeg_binary
        ]

        execute.extend([
            '-i',
            input_video
        ])

        execute.extend(self._read_configuration(phase='video_to_frames', section='output_options'))

        execute.extend([
            os.path.join(extracted_frames, f'extracted_%0d.{self.image_format}')
        ])

        execute.extend(self._read_configuration(phase='video_to_frames'))

        self._execute(execute)

    def convert_video(self, framerate, resolution, upscaled_frames):
        """Converts images into videos

        This method converts a set of images into a
        video.

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

        # read FFmpeg input options
        execute.extend(self._read_configuration(phase='frames_to_video', section='input_options'))

        # WORKAROUND FOR WAIFU2X-NCNN-VULKAN
        import re
        import shutil
        regex = re.compile(r'\.png\.png$')
        for raw_frame in os.listdir(upscaled_frames):
            shutil.move(os.path.join(upscaled_frames, raw_frame), os.path.join(upscaled_frames, regex.sub('.png', raw_frame)))
        # END WORKAROUND

        # append input frames path into command
        execute.extend([
            '-i',
            os.path.join(upscaled_frames, f'extracted_%d.{self.image_format}')
        ])

        # read FFmpeg output options
        execute.extend(self._read_configuration(phase='frames_to_video', section='output_options'))

        # read other options
        execute.extend(self._read_configuration(phase='frames_to_video'))

        # specify output file location
        execute.extend([
            os.path.join(upscaled_frames, 'no_audio.mp4')
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
            self.ffmpeg_binary,
            '-i',
            os.path.join(upscaled_frames, 'no_audio.mp4'),
            '-i',
            input_video
        ]

        execute.extend(self._read_configuration(phase='migrating_tracks', section='output_options'))

        execute.extend([
            output_video
        ])

        execute.extend(self._read_configuration(phase='migrating_tracks'))

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
            if value is None or value is False or isinstance(value, list) or isinstance(value, dict):
                continue
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
        Avalon.debug_info(f'Executing: {execute}')
        #remove shell=true becuase it isn't needed and causes issues in linux
        return subprocess.run(execute, check=True).returncode

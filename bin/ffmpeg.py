#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: FFMPEG Class
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: April 21, 2019

Description: This class handles all FFMPEG related
operations.
"""
from avalon_framework import Avalon
import json
import subprocess


class Ffmpeg:
    """This class communicates with ffmpeg

    This class deals with ffmpeg. It handles extracitng
    frames, stripping audio, converting images into videos
    and inserting audio tracks to videos.
    """

    def __init__(self, ffmpeg_settings, image_format):
        self.ffmpeg_settings = ffmpeg_settings

        self.ffmpeg_path = self.ffmpeg_settings['ffmpeg_path']
        # add a forward slash to directory if not present
        # otherwise there will be a format error
        if self.ffmpeg_path[-1] != '/' and self.ffmpeg_path[-1] != '\\':
            self.ffmpeg_path = f'{self.ffmpeg_path}\\'

        self.ffmpeg_binary = f'{self.ffmpeg_path}ffmpeg.exe'
        self.ffmpeg_probe_binary = f'{self.ffmpeg_path}ffprobe.exe'
        self.image_format = image_format

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
            extracted_frames {string} -- video output folder
        """
        execute = [
            self.ffmpeg_binary,
            '-i',
            input_video,
            f'{extracted_frames}\\extracted_%0d.{self.image_format}'
        ]
        self._execute(execute=execute, phase='video_to_frames')

    def convert_video(self, framerate, resolution, upscaled_frames):
        """Converts images into videos

        This method converts a set of images into a
        video.

        Arguments:
            framerate {float} -- target video framerate
            resolution {string} -- target video resolution
            upscaled_frames {string} -- source images folder
        """
        execute = [
            self.ffmpeg_binary,
            '-r',
            str(framerate),
            '-s',
            resolution,
            '-i',
            f'{upscaled_frames}\\extracted_%d.{self.image_format}',
            f'{upscaled_frames}\\no_audio.mp4'
        ]
        self._execute(execute=execute, phase='frames_to_video')

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
            f'{upscaled_frames}\\no_audio.mp4',
            '-i',
            input_video,
            output_video
        ]
        self._execute(execute=execute, phase='migrating_tracks')

    def _execute(self, execute, phase):

        for key in self.ffmpeg_settings[phase].keys():

            value = self.ffmpeg_settings[phase][key]

            # null or None means that leave this option out (keep default)
            if value is None or value is False:
                continue
            else:
                execute.append(key)

                # true means key is an option
                if value is True:
                    continue

                execute.append(str(value))

        Avalon.debug_info(f'Executing: {execute}')
        return subprocess.run(execute, shell=True, check=True).returncode

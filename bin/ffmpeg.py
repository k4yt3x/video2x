#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: FFMPEG Class
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: March 19, 2019

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

    def __init__(self, ffmpeg_settings):
        self.ffmpeg_settings = ffmpeg_settings
        self._parse_settings()
    
    def _parse_settings(self):
        """ Parse ffmpeg settings
        """
        self.ffmpeg_path = self.ffmpeg_settings['ffmpeg_path']
        # add a forward slash to directory if not present
        # otherwise there will be a format error
        if self.ffmpeg_path[-1] != '/' and self.ffmpeg_path[-1] != '\\':
            self.ffmpeg_path = '{}\\'.format(self.ffmpeg_path)

        self.ffmpeg_binary = '{}ffmpeg.exe'.format(self.ffmpeg_path)
        self.ffmpeg_hwaccel = self.ffmpeg_settings['ffmpeg_hwaccel']
        self.extra_arguments = self.ffmpeg_settings['extra_arguments']

    def get_video_info(self, input_video):
        """ Gets input video information

        This method reads input video information
        using ffprobe in dictionary.

        Arguments:
            input_video {string} -- input video file path

        Returns:
            dictionary -- JSON text of input video information
        """
        execute = [
            '{}ffprobe.exe'.format(self.ffmpeg_path),
            '-v',
            'quiet',
            '-print_format',
            'json',
            '-show_format',
            '-show_streams',
            '-i',
            '{}'.format(input_video)
        ]

        Avalon.debug_info('Executing: {}'.format(' '.join(execute)))
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
            '{}'.format(input_video),
            '{}\\extracted_%0d.png'.format(extracted_frames),
            '-y'
        ]
        execute += self.extra_arguments

        Avalon.debug_info('Executing: {}'.format(' '.join(execute)))
        subprocess.run(execute, shell=True, check=True)

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
            '-f',
            'image2',
            '-s',
            resolution,
            '-i',
            '{}\\extracted_%d.png'.format(upscaled_frames),
            '-vcodec',
            'libx264',
            '-crf',
            '25',
            '-pix_fmt',
            'yuv420p',
            '{}\\no_audio.mp4'.format(upscaled_frames),
            '-y'
        ]
        execute += self.extra_arguments

        Avalon.debug_info('Executing: {}'.format(' '.join(execute)))
        subprocess.run(execute, shell=True, check=True)

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
            '{}\\no_audio.mp4'.format(upscaled_frames),
            '-i',
            '{}'.format(input_video),
            '-map',
            '0:v:0?',
            '-map',
            '1?',
            '-c',
            'copy',
            '-map',
            '-1:v?',
            '{}'.format(output_video),
            '-y'
        ]
        execute += self.extra_arguments

        Avalon.debug_info('Executing: {}'.format(' '.join(execute)))
        subprocess.run(execute, shell=True, check=True)

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2X Upscaler
Author: K4YT3X
Date Created: December 10, 2018
Last Modified: December 19, 2018

Licensed under the GNU General Public License Version 3 (GNU GPL v3),
    available at: https://www.gnu.org/licenses/gpl-3.0.txt

(C) 2018-2019 K4YT3X
"""

from avalon_framework import Avalon
from exceptions import *
from ffmpeg import Ffmpeg
from fractions import Fraction
from waifu2x import Waifu2x
import json
import os
import shutil
import subprocess
import tempfile
import threading

MODELS_AVAILABLE = ['upconv_7_anime_style_art_rgb', 'upconv_7_photo',
                    'anime_style_art_rgb', 'photo', 'anime_style_art_y']


class Upscaler:

    def __init__(self, input_video, output_video, method, waifu2x_path, ffmpeg_path, ffmpeg_arguments=[], ffmpeg_hwaccel='gpu', output_width=False, output_height=False, factor=False, model_type='anime_style_art_rgb', threads=3):
        # Mandatory arguments
        self.input_video = input_video
        self.output_video = output_video
        self.method = method
        self.waifu2x_path = waifu2x_path
        self.ffmpeg_path = ffmpeg_path

        # Optional arguments
        self.ffmpeg_arguments = ffmpeg_arguments
        self.ffmpeg_hwaccel = ffmpeg_hwaccel
        self.output_width = output_width
        self.output_height = output_height
        self.factor = factor
        self.model_type = model_type
        self.threads = threads

        # Make temporary directories
        self.extracted_frames = tempfile.mkdtemp()
        self.upscaled_frames = tempfile.mkdtemp()

    def _get_video_info(self):
        """Gets input video information
        returns input video information using ffprobe in dictionary.
        """
        json_str = subprocess.check_output('\"{}ffprobe.exe\" -v quiet -print_format json -show_format -show_streams \"{}\"'.format(self.ffmpeg_path, self.input_video))
        return json.loads(json_str.decode('utf-8'))

    def _check_model_type(self, args):
        """ Validate upscaling model
        """
        if self.model_type not in MODELS_AVAILABLE:
            raise InvalidModelType('Specified model type not available')

    def _check_arguments(self):
        # Check if arguments are valid / all necessary argument
        # values are specified
        if not self.input_video:
            raise ArgumentError('You need to specify the video to process')
        elif (not self.output_width or not self.output_height) and not self.factor:
            raise ArgumentError('You must specify output video width and height or upscale factor')
        elif not self.output_video:
            raise ArgumentError('You need to specify the output video name')
        elif not self.method:
            raise ArgumentError('You need to specify the enlarging processing unit')

    def _upscale_frames(self, w2):
        """ Upscale video frames with waifu2x-caffe

        This function upscales all the frames extracted
        by ffmpeg using the waifu2x-caffe binary.

        Arguments:
            w2 {Waifu2x Object} -- initialized waifu2x object
        """

        # Create a container for all upscaler threads
        upscaler_threads = []

        # List all images in the extracted frames
        frames = [os.path.join(self.extracted_frames, f) for f in os.listdir(self.extracted_frames) if os.path.isfile(os.path.join(self.extracted_frames, f))]

        # If we have less images than threads,
        # create only the threads necessary
        if len(frames) < self.threads:
            self.threads = len(frames)

        # Create a folder for each thread and append folder
        # name into a list

        thread_pool = []
        for thread_id in range(self.threads):
            thread_folder = '{}\\{}'.format(self.extracted_frames, str(thread_id))

            # Delete old folders and create new folders
            if os.path.isdir(thread_folder):
                shutil.rmtree(thread_folder)
            os.mkdir(thread_folder)

            # Append folder path into list
            thread_pool.append((thread_folder, thread_id))

        # Evenly distribute images into each folder
        # until there is none left in the folder
        for image in frames:
            # Move image
            shutil.move(image, thread_pool[0][0])
            # Rotate list
            thread_pool = thread_pool[-1:] + thread_pool[:-1]

        # Create threads and start them
        for thread_info in thread_pool:
            # Create thread
            thread = threading.Thread(target=w2.upscale, args=(thread_info[0], self.upscaled_frames, self.output_width, self.output_height))
            thread.name = thread_info[1]

            # Add threads into the pool
            upscaler_threads.append(thread)

        # Start all threads
        for thread in upscaler_threads:
            thread.start()

        # Wait for threads to finish
        for thread in upscaler_threads:
            thread.join()

    def run(self):
        """Main controller for Video2X

        This function controls the flow of video conversion
        and handles all necessary functions.
        """

        # Parse arguments for waifu2x
        # Check argument sanity
        self._check_model_type(self.model_type)
        self._check_arguments()

        # Convert paths to absolute paths
        self.input_video = os.path.abspath(self.input_video)
        self.output_video = os.path.abspath(self.output_video)

        # Add a forward slash to directory if not present
        # otherwise there will be a format error
        if self.ffmpeg_path[-1] != '/' and self.ffmpeg_path[-1] != '\\':
            self.ffmpeg_path = '{}/'.format(self.ffmpeg_path)

        # Check if FFMPEG and waifu2x are present
        if not os.path.isdir(self.ffmpeg_path):
            raise FileNotFoundError(self.ffmpeg_path)
        if not os.path.isfile(self.waifu2x_path):
            raise FileNotFoundError(self.waifu2x_path)

        # Initialize objects for ffmpeg and waifu2x-caffe
        fm = Ffmpeg(self.ffmpeg_path, self.output_video, self.ffmpeg_arguments)
        w2 = Waifu2x(self.waifu2x_path, self.method, self.model_type)

        # Extract frames from video
        fm.extract_frames(self.input_video, self.extracted_frames)

        Avalon.info('Reading video information')
        info = self._get_video_info()
        # Analyze original video with ffprobe and retrieve framerate
        # width, height = info['streams'][0]['width'], info['streams'][0]['height']

        # Find index of video stream
        video_stream_index = None
        for stream in info['streams']:
            if stream['codec_type'] == 'video':
                video_stream_index = stream['index']
                break

        # Exit if no video stream found
        if video_stream_index is None:
            Avalon.error('Aborting: No video stream found')

        # Get average frame rate of video stream
        framerate = float(Fraction(info['streams'][video_stream_index]['avg_frame_rate']))
        Avalon.info('Framerate: {}'.format(framerate))

        # Width/height will be coded width/height x upscale factor
        if self.factor:
            coded_width = info['streams'][video_stream_index]['coded_width']
            coded_height = info['streams'][video_stream_index]['coded_height']
            self.output_width = self.factor * coded_width
            self.output_height = self.factor * coded_height

        # Upscale images one by one using waifu2x
        Avalon.info('Starting to upscale extracted images')
        self._upscale_frames(w2)
        Avalon.info('Upscaling completed')

        # Frames to Video
        Avalon.info('Converting extracted frames into video')

        # Use user defined output size
        fm.convert_video(framerate, '{}x{}'.format(self.output_width, self.output_height), self.upscaled_frames)
        Avalon.info('Conversion completed')

        # Extract and press audio in
        Avalon.info('Stripping audio track from original video')
        fm.extract_audio(self.input_video, self.upscaled_frames)
        Avalon.info('Inserting audio track into new video')
        fm.insert_audio_track(self.upscaled_frames)

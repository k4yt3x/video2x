#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2X Upscaler
Author: K4YT3X
Date Created: December 10, 2018
Last Modified: May 4, 2020

Description: This file contains the Upscaler class. Each
instance of the Upscaler class is an upscaler on an image or
a folder.
"""

# local imports
from exceptions import *
from image_cleaner import ImageCleaner
from wrappers.ffmpeg import Ffmpeg

# built-in imports
from fractions import Fraction
import contextlib
import copy
import importlib
import os
import pathlib
import re
import shutil
import sys
import tempfile
import threading
import time
import traceback

# third-party imports
from avalon_framework import Avalon
from tqdm import tqdm

# these names are consistent for
# - driver selection in command line
# - driver wrapper file names
# - config file keys
AVAILABLE_DRIVERS = ['waifu2x_caffe',
                     'waifu2x_converter_cpp',
                     'waifu2x_ncnn_vulkan',
                     'srmd_ncnn_vulkan',
                     'anime4kcpp']


class Upscaler:
    """ An instance of this class is a upscaler that will
    upscale all images in the given directory.

    Raises:
        Exception -- all exceptions
        ArgumentError -- if argument is not valid
    """

    def __init__(self, input_video, output_video, driver_settings, ffmpeg_settings):
        # mandatory arguments
        self.input_video = input_video
        self.output_video = output_video
        self.driver_settings = driver_settings
        self.ffmpeg_settings = ffmpeg_settings

        # optional arguments
        self.driver = 'waifu2x_caffe'
        self.scale_width = None
        self.scale_height = None
        self.scale_ratio = None
        self.processes = 1
        self.video2x_cache_directory = pathlib.Path(tempfile.gettempdir()) / 'video2x'
        self.image_format = 'png'
        self.preserve_frames = False

    def create_temp_directories(self):
        """create temporary directory
        """

        # create a new temp directory if the current one is not found
        if not self.video2x_cache_directory.exists():
            self.video2x_cache_directory = pathlib.Path(tempfile.gettempdir()) / 'video2x'

        # create temp directories for extracted frames and upscaled frames
        self.extracted_frames = pathlib.Path(tempfile.mkdtemp(dir=self.video2x_cache_directory))
        Avalon.debug_info(_('Extracted frames are being saved to: {}').format(self.extracted_frames))
        self.upscaled_frames = pathlib.Path(tempfile.mkdtemp(dir=self.video2x_cache_directory))
        Avalon.debug_info(_('Upscaled frames are being saved to: {}').format(self.upscaled_frames))

    def cleanup_temp_directories(self):
        """delete temp directories when done
        """
        if not self.preserve_frames:
            for directory in [self.extracted_frames, self.upscaled_frames, self.video2x_cache_directory]:
                try:
                    # avalon framework cannot be used if python is shutting down
                    # therefore, plain print is used
                    print(_('Cleaning up cache directory: {}').format(directory))
                    shutil.rmtree(directory)
                except (OSError, FileNotFoundError):
                    print(_('Unable to delete: {}').format(directory))
                    traceback.print_exc()

    def _check_arguments(self):
        # check if arguments are valid / all necessary argument
        # values are specified
        if not self.input_video:
            Avalon.error(_('You must specify input video file/directory path'))
            raise ArgumentError('input video path not specified')
        if not self.output_video:
            Avalon.error(_('You must specify output video file/directory path'))
            raise ArgumentError('output video path not specified')
        if (self.driver in ['waifu2x_converter', 'waifu2x_ncnn_vulkan', 'anime4k']) and self.scale_width and self.scale_height:
            Avalon.error(_('Selected driver accepts only scaling ratio'))
            raise ArgumentError('selected driver supports only scaling ratio')
        if self.driver == 'waifu2x_ncnn_vulkan' and self.scale_ratio is not None and (self.scale_ratio > 2 or not self.scale_ratio.is_integer()):
            Avalon.error(_('Scaling ratio must be 1 or 2 for waifu2x_ncnn_vulkan'))
            raise ArgumentError('scaling ratio must be 1 or 2 for waifu2x_ncnn_vulkan')
        if self.driver == 'srmd_ncnn_vulkan' and self.scale_ratio is not None and (self.scale_ratio not in [2, 3, 4]):
            Avalon.error(_('Scaling ratio must be one of 2, 3 or 4 for srmd_ncnn_vulkan'))
            raise ArgumentError('scaling ratio must be one of 2, 3 or 4 for srmd_ncnn_vulkan')
        if (self.scale_width or self.scale_height) and self.scale_ratio:
            Avalon.error(_('You can only specify either scaling ratio or output width and height'))
            raise ArgumentError('both scaling ration and width/height specified')
        if (self.scale_width and not self.scale_height) or (not self.scale_width and self.scale_height):
            Avalon.error(_('You must specify both width and height'))
            raise ArgumentError('only one of width or height is specified')

    def _progress_bar(self, extracted_frames_directories):
        """ This method prints a progress bar

        This method prints a progress bar by keeping track
        of the amount of frames in the input directory
        and the output directory. This is originally
        suggested by @ArmandBernard.
        """

        # get number of extracted frames
        self.total_frames = 0
        for directory in extracted_frames_directories:
            self.total_frames += len([f for f in directory.iterdir() if str(f).lower().endswith(self.image_format.lower())])

        with tqdm(total=self.total_frames, ascii=True, desc=_('Upscaling Progress')) as progress_bar:

            # tqdm update method adds the value to the progress
            # bar instead of setting the value. Therefore, a delta
            # needs to be calculated.
            previous_cycle_frames = 0
            while not self.progress_bar_exit_signal:

                with contextlib.suppress(FileNotFoundError):
                    self.total_frames_upscaled = len([f for f in self.upscaled_frames.iterdir() if str(f).lower().endswith(self.image_format.lower())])
                    delta = self.total_frames_upscaled - previous_cycle_frames
                    previous_cycle_frames = self.total_frames_upscaled

                    # if upscaling is finished
                    if self.total_frames_upscaled >= self.total_frames:
                        return

                    # adds the delta into the progress bar
                    progress_bar.update(delta)

                time.sleep(1)

    def _upscale_frames(self):
        """ Upscale video frames with waifu2x-caffe

        This function upscales all the frames extracted
        by ffmpeg using the waifu2x-caffe binary.

        Arguments:
            w2 {Waifu2x Object} -- initialized waifu2x object
        """

        # progress bar process exit signal
        self.progress_bar_exit_signal = False

        # initialize waifu2x driver
        if self.driver not in AVAILABLE_DRIVERS:
            raise UnrecognizedDriverError(_('Unrecognized driver: {}').format(self.driver))

        # create a container for all upscaler processes
        upscaler_processes = []

        # list all images in the extracted frames
        frames = [(self.extracted_frames / f) for f in self.extracted_frames.iterdir() if f.is_file]

        # if we have less images than processes,
        # create only the processes necessary
        if len(frames) < self.processes:
            self.processes = len(frames)

        # create a directory for each process and append directory
        # name into a list
        process_directories = []
        for process_id in range(self.processes):
            process_directory = self.extracted_frames / str(process_id)
            process_directories.append(process_directory)

            # delete old directories and create new directories
            if process_directory.is_dir():
                shutil.rmtree(process_directory)
            process_directory.mkdir(parents=True, exist_ok=True)

        # waifu2x-converter-cpp will perform multi-threading within its own process
        if self.driver == 'waifu2x_converter_cpp':
            process_directories = [self.extracted_frames]

        else:
            # evenly distribute images into each directory
            # until there is none left in the directory
            for image in frames:
                # move image
                image.rename(process_directories[0] / image.name)
                # rotate list
                process_directories = process_directories[-1:] + process_directories[:-1]

        # create threads and start them
        for process_directory in process_directories:

            DriverWrapperMain = getattr(importlib.import_module(f'wrappers.{self.driver}'), 'WrapperMain')
            driver = DriverWrapperMain(copy.deepcopy(self.driver_settings))

            # if the driver being used is waifu2x-caffe
            if self.driver == 'waifu2x_caffe':
                upscaler_processes.append(driver.upscale(process_directory,
                                                         self.upscaled_frames,
                                                         self.scale_ratio,
                                                         self.scale_width,
                                                         self.scale_height,
                                                         self.image_format,
                                                         self.bit_depth))

            # if the driver being used is waifu2x-converter-cpp
            elif self.driver == 'waifu2x_converter_cpp':
                upscaler_processes.append(driver.upscale(process_directory,
                                                         self.upscaled_frames,
                                                         self.scale_ratio,
                                                         self.processes,
                                                         self.image_format))

            # if the driver being used is waifu2x-ncnn-vulkan
            elif self.driver == 'waifu2x_ncnn_vulkan':
                upscaler_processes.append(driver.upscale(process_directory,
                                                         self.upscaled_frames,
                                                         self.scale_ratio))

            # if the driver being used is srmd_ncnn_vulkan
            elif self.driver == 'srmd_ncnn_vulkan':
                upscaler_processes.append(driver.upscale(process_directory,
                                                         self.upscaled_frames,
                                                         self.scale_ratio))

        # start progress bar in a different thread
        progress_bar = threading.Thread(target=self._progress_bar, args=(process_directories,))
        progress_bar.start()

        # create the clearer and start it
        Avalon.debug_info(_('Starting upscaled image cleaner'))
        image_cleaner = ImageCleaner(self.extracted_frames, self.upscaled_frames, len(upscaler_processes))
        image_cleaner.start()

        # wait for all process to exit
        try:
            Avalon.debug_info(_('Main process waiting for subprocesses to exit'))
            for process in upscaler_processes:
                Avalon.debug_info(_('Subprocess {} exited with code {}').format(process.pid, process.wait()))
        except (KeyboardInterrupt, SystemExit):
            Avalon.warning('Exit signal received')
            Avalon.warning('Killing processes')
            for process in upscaler_processes:
                process.terminate()

            # cleanup and exit with exit code 1
            Avalon.debug_info(_('Killing upscaled image cleaner'))
            image_cleaner.stop()
            self.progress_bar_exit_signal = True
            sys.exit(1)

        # if the driver is waifu2x-converter-cpp
        # images need to be renamed to be recognizable for FFmpeg
        if self.driver == 'waifu2x_converter_cpp':
            for image in [f for f in self.upscaled_frames.iterdir() if f.is_file()]:
                renamed = re.sub(f'_\\[.*\\]\\[x(\\d+(\\.\\d+)?)\\]\\.{self.image_format}', f'.{self.image_format}', str(image.name))
                (self.upscaled_frames / image).rename(self.upscaled_frames / renamed)

        # upscaling done, kill the clearer
        Avalon.debug_info(_('Killing upscaled image cleaner'))
        image_cleaner.stop()

        # pass exit signal to progress bar thread
        self.progress_bar_exit_signal = True

    def run(self):
        """ Main controller for Video2X

        This function controls the flow of video conversion
        and handles all necessary functions.
        """

        # parse arguments for waifu2x
        # check argument sanity
        self._check_arguments()

        # convert paths to absolute paths
        self.input_video = self.input_video.absolute()
        self.output_video = self.output_video.absolute()

        # drivers that have native support for video processing
        if self.driver == 'anime4kcpp':
            # append FFmpeg path to the end of PATH
            # Anime4KCPP will then use FFmpeg to migrate audio tracks
            os.environ['PATH'] += f';{self.ffmpeg_settings["ffmpeg_path"]}'
            Avalon.info(_('Starting to upscale extracted images'))
            driver = Anime4kCpp(self.driver_settings)
            driver.upscale(self.input_video, self.output_video, self.scale_ratio, self.processes).wait()
            Avalon.info(_('Upscaling completed'))

        else:
            self.create_temp_directories()

            # initialize objects for ffmpeg and waifu2x-caffe
            fm = Ffmpeg(self.ffmpeg_settings, self.image_format)

            Avalon.info(_('Reading video information'))
            video_info = fm.get_video_info(self.input_video)
            # analyze original video with ffprobe and retrieve framerate
            # width, height = info['streams'][0]['width'], info['streams'][0]['height']

            # find index of video stream
            video_stream_index = None
            for stream in video_info['streams']:
                if stream['codec_type'] == 'video':
                    video_stream_index = stream['index']
                    break

            # exit if no video stream found
            if video_stream_index is None:
                Avalon.error(_('Aborting: No video stream found'))
                raise StreamNotFoundError('no video stream found')

            # extract frames from video
            fm.extract_frames(self.input_video, self.extracted_frames)

            # get average frame rate of video stream
            framerate = float(Fraction(video_info['streams'][video_stream_index]['avg_frame_rate']))
            fm.pixel_format = video_info['streams'][video_stream_index]['pix_fmt']

            # get a dict of all pixel formats and corresponding bit depth
            pixel_formats = fm.get_pixel_formats()

            # try getting pixel format's corresponding bti depth
            try:
                self.bit_depth = pixel_formats[fm.pixel_format]
            except KeyError:
                Avalon.error(_('Unsupported pixel format: {}').format(fm.pixel_format))
                raise UnsupportedPixelError(f'unsupported pixel format {fm.pixel_format}')

            Avalon.info(_('Framerate: {}').format(framerate))

            # width/height will be coded width/height x upscale factor
            if self.scale_ratio:
                original_width = video_info['streams'][video_stream_index]['width']
                original_height = video_info['streams'][video_stream_index]['height']
                self.scale_width = int(self.scale_ratio * original_width)
                self.scale_height = int(self.scale_ratio * original_height)

            # upscale images one by one using waifu2x
            Avalon.info(_('Starting to upscale extracted images'))
            self._upscale_frames()
            Avalon.info(_('Upscaling completed'))

            # frames to Video
            Avalon.info(_('Converting extracted frames into video'))

            # use user defined output size
            fm.convert_video(framerate, f'{self.scale_width}x{self.scale_height}', self.upscaled_frames)
            Avalon.info(_('Conversion completed'))

            # migrate audio tracks and subtitles
            Avalon.info(_('Migrating audio tracks and subtitles to upscaled video'))
            fm.migrate_audio_tracks_subtitles(self.input_video, self.output_video, self.upscaled_frames)

            # destroy temp directories
            self.cleanup_temp_directories()

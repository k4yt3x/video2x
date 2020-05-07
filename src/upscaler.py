#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2X Upscaler
Author: K4YT3X
Date Created: December 10, 2018
Last Modified: May 7, 2020

Description: This file contains the Upscaler class. Each
instance of the Upscaler class is an upscaler on an image or
a folder.
"""

# local imports
from exceptions import *
from image_cleaner import ImageCleaner
from progress_monitor import ProgressMonitor
from wrappers.ffmpeg import Ffmpeg

# built-in imports
from fractions import Fraction
import contextlib
import copy
import gettext
import importlib
import locale
import os
import pathlib
import queue
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import traceback

# third-party imports
from avalon_framework import Avalon

# internationalization constants
DOMAIN = 'video2x'
LOCALE_DIRECTORY = pathlib.Path(__file__).parent.absolute() / 'locale'

# getting default locale settings
default_locale, encoding = locale.getdefaultlocale()
language = gettext.translation(DOMAIN, LOCALE_DIRECTORY, [default_locale], fallback=True)
language.install()
_ = language.gettext

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

    def __init__(self, input_path, output_path, driver_settings, ffmpeg_settings):
        # mandatory arguments
        self.input_path = input_path
        self.output_path = output_path
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

        # other internal members and signals
        self.stop_signal = False
        self.total_frames_upscaled = 0
        self.total_frames = 0

    def create_temp_directories(self):
        """create temporary directories
        """

        # if cache directory unspecified, use %TEMP%\video2x
        if self.video2x_cache_directory is None:
            self.video2x_cache_directory = pathlib.Path(tempfile.gettempdir()) / 'video2x'

        # if specified cache path exists and isn't a directory
        if self.video2x_cache_directory.exists() and not self.video2x_cache_directory.is_dir():
            Avalon.error(_('Specified or default cache directory is a file/link'))
            raise FileExistsError('Specified or default cache directory is a file/link')
        
        # if cache directory doesn't exist, try creating it
        if not self.video2x_cache_directory.exists():
            try:
                Avalon.debug_info(_('Creating cache directory {}').format(self.video2x_cache_directory))
                self.video2x_cache_directory.mkdir(parents=True, exist_ok=True)
            except Exception as exception:
                Avalon.error(_('Unable to create {}').format(self.video2x_cache_directory))
                raise exception

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
        # if input is a file
        if self.input_path.is_file():
            if self.output_path.is_dir():
                Avalon.error(_('Input and output path type mismatch'))
                Avalon.error(_('Input is single file but output is directory'))
                raise ArgumentError('input output path type mismatch')
            if not re.search(r'.*\..*$', str(self.output_path)):
                Avalon.error(_('No suffix found in output file path'))
                Avalon.error(_('Suffix must be specified for FFmpeg'))
                raise ArgumentError('no output video suffix specified')

        # if input is a directory
        elif self.input_path.is_dir():
            if self.output_path.is_file():
                Avalon.error(_('Input and output path type mismatch'))
                Avalon.error(_('Input is directory but output is existing single file'))
                raise ArgumentError('input output path type mismatch')

        # if input is neither
        else:
            Avalon.error(_('Input path is neither a file nor a directory'))
            raise FileNotFoundError(f'{self.input_path} is neither file nor directory')
        
        # check Fmpeg settings
        ffmpeg_path = pathlib.Path(self.ffmpeg_settings['ffmpeg_path'])
        if not ((pathlib.Path(ffmpeg_path / 'ffmpeg.exe').is_file() and
                pathlib.Path(ffmpeg_path / 'ffprobe.exe').is_file()) or
                (pathlib.Path(ffmpeg_path / 'ffmpeg').is_file() and
                pathlib.Path(ffmpeg_path / 'ffprobe').is_file())):
            Avalon.error(_('FFmpeg or FFprobe cannot be found under the specified path'))
            Avalon.error(_('Please check the configuration file settings'))
            raise FileNotFoundError(self.ffmpeg_settings['ffmpeg_path'])

        # check if driver settings
        driver_settings = copy.deepcopy(self.driver_settings)
        driver_path = driver_settings.pop('path')

        # check if driver path exists
        if not (pathlib.Path(driver_path).is_file() or pathlib.Path(f'{driver_path}.exe').is_file()):
            Avalon.error(_('Specified driver executable directory doesn\'t exist'))
            Avalon.error(_('Please check the configuration file settings'))
            raise FileNotFoundError(driver_path)

        # parse driver arguments using driver's parser
        # the parser will throw AttributeError if argument doesn't satisfy constraints
        try:
            driver_arguments = []
            for key in driver_settings.keys():

                value = driver_settings[key]

                if value is None or value is False:
                    continue

                else:
                    if len(key) == 1:
                        driver_arguments.append(f'-{key}')
                    else:
                        driver_arguments.append(f'--{key}')
                    # true means key is an option
                    if value is not True:
                        driver_arguments.append(str(value))

            DriverWrapperMain = getattr(importlib.import_module(f'wrappers.{self.driver}'), 'WrapperMain')
            DriverWrapperMain.parse_arguments(driver_arguments)
        except AttributeError as e:
            Avalon.error(_('Failed to parse driver argument: {}').format(e.args[0]))
            raise e

    def _upscale_frames(self):
        """ Upscale video frames with waifu2x-caffe

        This function upscales all the frames extracted
        by ffmpeg using the waifu2x-caffe binary.

        Arguments:
            w2 {Waifu2x Object} -- initialized waifu2x object
        """

        # initialize waifu2x driver
        if self.driver not in AVAILABLE_DRIVERS:
            raise UnrecognizedDriverError(_('Unrecognized driver: {}').format(self.driver))

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
                self.process_pool.append(driver.upscale(process_directory,
                                                         self.upscaled_frames,
                                                         self.scale_ratio,
                                                         self.scale_width,
                                                         self.scale_height,
                                                         self.image_format,
                                                         self.bit_depth))

            # if the driver being used is waifu2x-converter-cpp
            elif self.driver == 'waifu2x_converter_cpp':
                self.process_pool.append(driver.upscale(process_directory,
                                                         self.upscaled_frames,
                                                         self.scale_ratio,
                                                         self.processes,
                                                         self.image_format))

            # if the driver being used is waifu2x-ncnn-vulkan
            elif self.driver == 'waifu2x_ncnn_vulkan':
                self.process_pool.append(driver.upscale(process_directory,
                                                         self.upscaled_frames,
                                                         self.scale_ratio))

            # if the driver being used is srmd_ncnn_vulkan
            elif self.driver == 'srmd_ncnn_vulkan':
                self.process_pool.append(driver.upscale(process_directory,
                                                         self.upscaled_frames,
                                                         self.scale_ratio))

        # start progress bar in a different thread
        Avalon.debug_info(_('Starting progress monitor'))
        self.progress_monitor = ProgressMonitor(self, process_directories)
        self.progress_monitor.start()

        # create the clearer and start it
        Avalon.debug_info(_('Starting upscaled image cleaner'))
        self.image_cleaner = ImageCleaner(self.extracted_frames, self.upscaled_frames, len(self.process_pool))
        self.image_cleaner.start()

        # wait for all process to exit
        try:
            self._wait()
        except (Exception, KeyboardInterrupt, SystemExit) as e:
            # cleanup
            Avalon.debug_info(_('Killing progress monitor'))
            self.progress_monitor.stop()

            Avalon.debug_info(_('Killing upscaled image cleaner'))
            self.image_cleaner.stop()            
            raise e

        # if the driver is waifu2x-converter-cpp
        # images need to be renamed to be recognizable for FFmpeg
        if self.driver == 'waifu2x_converter_cpp':
            for image in [f for f in self.upscaled_frames.iterdir() if f.is_file()]:
                renamed = re.sub(f'_\\[.*\\]\\[x(\\d+(\\.\\d+)?)\\]\\.{self.image_format}',
                                 f'.{self.image_format}',
                                 str(image.name))
                (self.upscaled_frames / image).rename(self.upscaled_frames / renamed)

        # upscaling done, kill helper threads
        Avalon.debug_info(_('Killing progress monitor'))
        self.progress_monitor.stop()

        Avalon.debug_info(_('Killing upscaled image cleaner'))
        self.image_cleaner.stop()

    def _terminate_subprocesses(self):
        Avalon.warning(_('Terminating all processes'))
        for process in self.process_pool:
            process.terminate()

    def _wait(self):
        """ wait for subprocesses in process pool to complete
        """
        Avalon.debug_info(_('Main process waiting for subprocesses to exit'))

        try:
            # while process pool not empty
            while self.process_pool:
                
                # if stop signal received, terminate all processes
                if self.stop_signal is True:
                    raise SystemExit

                for process in self.process_pool:
                    process_status = process.poll()

                    # if process finished
                    if process_status is None:
                        continue

                    # if return code is not 0
                    elif process_status != 0:
                        Avalon.error(_('Subprocess {} exited with code {}').format(process.pid, process_status))
                        raise subprocess.CalledProcessError(process_status, process.args)

                    else:
                        Avalon.debug_info(_('Subprocess {} exited with code {}').format(process.pid, process_status))
                        self.process_pool.remove(process)

                time.sleep(0.1)

        except (KeyboardInterrupt, SystemExit) as e:
            Avalon.warning(_('Stop signal received'))
            self._terminate_subprocesses()
            raise e

        except (Exception, subprocess.CalledProcessError) as e:
            Avalon.error(_('Subprocess execution ran into an error'))
            self._terminate_subprocesses()
            raise e

    def run(self):
        """ Main controller for Video2X

        This function controls the flow of video conversion
        and handles all necessary functions.
        """
        
        # external stop signal when called in a thread
        self.stop_signal = False

        # define process pool to contain processes
        self.process_pool = []

        # parse arguments for waifu2x
        # check argument sanity
        self._check_arguments()

        # define processing queue
        processing_queue = queue.Queue()

        # if input specified is single file
        if self.input_path.is_file():
            Avalon.info(_('Upscaling single video file: {}').format(self.input_path))
            processing_queue.put((self.input_path.absolute(), self.output_path.absolute()))

        # if input specified is a directory
        elif self.input_path.is_dir():

            # make output directory if it doesn't exist
            self.output_path.mkdir(parents=True, exist_ok=True)
            for input_video in [f for f in self.input_path.iterdir() if f.is_file()]:
                output_video = self.output_path / input_video.name
                processing_queue.put((input_video.absolute(), output_video.absolute()))

        while not processing_queue.empty():
            input_video, output_video = processing_queue.get()
            # drivers that have native support for video processing
            if self.driver == 'anime4kcpp':
                # append FFmpeg path to the end of PATH
                # Anime4KCPP will then use FFmpeg to migrate audio tracks
                os.environ['PATH'] += f';{self.ffmpeg_settings["ffmpeg_path"]}'
                Avalon.info(_('Starting to upscale extracted images'))

                # import and initialize Anime4KCPP wrapper
                DriverWrapperMain = getattr(importlib.import_module('wrappers.anime4kcpp'), 'WrapperMain')
                driver = DriverWrapperMain(copy.deepcopy(self.driver_settings))

                # run Anime4KCPP
                self.process_pool.append(driver.upscale(input_video, output_video, self.scale_ratio, self.processes))
                self._wait()
                Avalon.info(_('Upscaling completed'))

            else:
                try:
                    self.create_temp_directories()

                    # initialize objects for ffmpeg and waifu2x-caffe
                    fm = Ffmpeg(self.ffmpeg_settings, self.image_format)

                    Avalon.info(_('Reading video information'))
                    video_info = fm.get_video_info(input_video)
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
                    self.process_pool.append((fm.extract_frames(input_video, self.extracted_frames)))
                    self._wait()

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
                    self.process_pool.append(fm.convert_video(framerate, f'{self.scale_width}x{self.scale_height}', self.upscaled_frames))
                    self._wait()
                    Avalon.info(_('Conversion completed'))

                    # migrate audio tracks and subtitles
                    Avalon.info(_('Migrating audio tracks and subtitles to upscaled video'))
                    self.process_pool.append(fm.migrate_audio_tracks_subtitles(input_video, output_video, self.upscaled_frames))
                    self._wait()

                    # destroy temp directories
                    self.cleanup_temp_directories()

                except (Exception, KeyboardInterrupt, SystemExit) as e:
                    with contextlib.suppress(ValueError):
                        self.cleanup_temp_directories()
                    raise e

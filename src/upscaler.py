#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2X Upscaler
Author: K4YT3X
Date Created: December 10, 2018
Last Modified: May 12, 2020

Description: This file contains the Upscaler class. Each
instance of the Upscaler class is an upscaler on an image or
a folder.
"""

# local imports
from exceptions import *
from image_cleaner import ImageCleaner
from progress_monitor import ProgressMonitor
from wrappers.ffmpeg import Ffmpeg
from wrappers.gifski import Gifski

# built-in imports
from fractions import Fraction
import contextlib
import copy
import gettext
import importlib
import locale
import pathlib
import queue
import re
import shutil
import subprocess
import tempfile
import time
import traceback

# third-party imports
from avalon_framework import Avalon
import magic

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

    def __init__(self, input_path, output_path, driver_settings, ffmpeg_settings, gifski_settings):
        # mandatory arguments
        self.input = input_path
        self.output = output_path
        self.driver_settings = driver_settings
        self.ffmpeg_settings = ffmpeg_settings
        self.gifski_settings = gifski_settings

        # optional arguments
        self.driver = 'waifu2x_caffe'
        self.scale_ratio = None
        self.processes = 1
        self.video2x_cache_directory = pathlib.Path(tempfile.gettempdir()) / 'video2x'
        self.image_format = 'png'
        self.preserve_frames = False

        # other internal members and signals
        self.running = False
        self.total_frames_upscaled = 0
        self.total_frames = 0
        self.total_files = 0
        self.total_processed = 0
        self.current_input_file = pathlib.Path()
        self.last_frame_upscaled = pathlib.Path()

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
        if isinstance(self.input, list):
            if self.output.exists() and not self.output.is_dir():
                Avalon.error(_('Input and output path type mismatch'))
                Avalon.error(_('Input is multiple files but output is not directory'))
                raise ArgumentError('input output path type mismatch')
            for input_path in self.input:
                if not input_path.is_file() and not input_path.is_dir():
                    Avalon.error(_('Input path {} is neither a file nor a directory').format(input_path))
                    raise FileNotFoundError(f'{input_path} is neither file nor directory')
                with contextlib.suppress(FileNotFoundError):
                    if input_path.samefile(self.output):
                        Avalon.error(_('Input directory and output directory cannot be the same'))
                        raise FileExistsError('input directory and output directory are the same')

        # if input is a file
        elif self.input.is_file():
            if self.output.is_dir():
                Avalon.error(_('Input and output path type mismatch'))
                Avalon.error(_('Input is single file but output is directory'))
                raise ArgumentError('input output path type mismatch')
            if self.output.suffix == '':
                Avalon.error(_('No suffix found in output file path'))
                Avalon.error(_('Suffix must be specified'))
                raise ArgumentError('no output file suffix specified')

        # if input is a directory
        elif self.input.is_dir():
            if self.output.is_file():
                Avalon.error(_('Input and output path type mismatch'))
                Avalon.error(_('Input is directory but output is existing single file'))
                raise ArgumentError('input output path type mismatch')
            with contextlib.suppress(FileNotFoundError):
                if self.input.samefile(self.output):
                    Avalon.error(_('Input directory and output directory cannot be the same'))
                    raise FileExistsError('input directory and output directory are the same')

        # if input is neither
        else:
            Avalon.error(_('Input path is neither a file nor a directory'))
            raise FileNotFoundError(f'{self.input} is neither file nor directory')

        # check FFmpeg settings
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

        # waifu2x-caffe scale_ratio, scale_width and scale_height check
        if self.driver == 'waifu2x_caffe':
            if (driver_settings['scale_width'] != 0 and driver_settings['scale_height'] == 0 or
                    driver_settings['scale_width'] == 0 and driver_settings['scale_height'] != 0):
                Avalon.error('Only one of scale_width and scale_height is specified for waifu2x-caffe')
                raise AttributeError('only one of scale_width and scale_height is specified for waifu2x-caffe')

            # if scale_width and scale_height are specified, ensure scale_ratio is None
            elif self.driver_settings['scale_width'] != 0 and self.driver_settings['scale_height'] != 0:
                self.driver_settings['scale_ratio'] = None

            # if scale_width and scale_height not specified
            # ensure they are None, not 0
            else:
                self.driver_settings['scale_width'] = None
                self.driver_settings['scale_height'] = None

        # temporary file type check for Anime4KCPP
        # it doesn't support GIF processing yet
        if self.driver == 'anime4kcpp':
            for task in self.processing_queue.queue:
                if task[0].suffix.lower() == '.gif':
                    Avalon.error(_('Anime4KCPP doesn\'t yet support GIF processing'))
                    raise AttributeError('Anime4KCPP doesn\'t yet support GIF file processing')

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
        if self.driver in ['waifu2x_converter_cpp', 'waifu2x_ncnn_vulkan', 'srmd_ncnn_vulkan']:
            process_directories = [self.extracted_frames]

        else:
            # evenly distribute images into each directory
            # until there is none left in the directory
            for image in frames:
                # move image
                image.rename(process_directories[0] / image.name)
                # rotate list
                process_directories = process_directories[-1:] + process_directories[:-1]

        # create driver processes and start them
        for process_directory in process_directories:
            self.process_pool.append(self.driver_object.upscale(process_directory, self.upscaled_frames))

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
                if self.running is False:
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
        self.running = True

        # define process pool to contain processes
        self.process_pool = []

        # load driver modules
        DriverWrapperMain = getattr(importlib.import_module(f'wrappers.{self.driver}'), 'WrapperMain')
        self.driver_object = DriverWrapperMain(self.driver_settings)

        # load options from upscaler class into driver settings
        self.driver_object.load_configurations(self)

        # initialize FFmpeg object
        self.ffmpeg_object = Ffmpeg(self.ffmpeg_settings, self.image_format)

        # define processing queue
        self.processing_queue = queue.Queue()

        # if input is a list of files
        if isinstance(self.input, list):
            # make output directory if it doesn't exist
            self.output.mkdir(parents=True, exist_ok=True)

            for input_path in self.input:

                if input_path.is_file():
                    output_path = self.output / input_path.name
                    self.processing_queue.put((input_path.absolute(), output_path.absolute()))

                elif input_path.is_dir():
                    for input_path in [f for f in input_path.iterdir() if f.is_file()]:
                        output_path = self.output / input_path.name
                        self.processing_queue.put((input_path.absolute(), output_path.absolute()))

        # if input specified is single file
        elif self.input.is_file():
            Avalon.info(_('Upscaling single file: {}').format(self.input))
            self.processing_queue.put((self.input.absolute(), self.output.absolute()))

        # if input specified is a directory
        elif self.input.is_dir():

            # make output directory if it doesn't exist
            self.output.mkdir(parents=True, exist_ok=True)
            for input_path in [f for f in self.input.iterdir() if f.is_file()]:
                output_path = self.output / input_path.name
                self.processing_queue.put((input_path.absolute(), output_path.absolute()))

        # check argument sanity before running
        self._check_arguments()

        # record file count for external calls
        self.total_files = self.processing_queue.qsize()

        try:
            while not self.processing_queue.empty():

                # reset current processing progress for new job
                self.total_frames_upscaled = 0
                self.total_frames = 0

                # get new job from queue
                self.current_input_file, output_path = self.processing_queue.get()

                # get file type
                input_file_mime_type = magic.from_file(str(self.current_input_file.absolute()), mime=True)
                input_file_type = input_file_mime_type.split('/')[0]
                input_file_subtype = input_file_mime_type.split('/')[1]

                # start handling input
                # if input file is a static image
                if input_file_type == 'image' and input_file_subtype != 'gif':
                    Avalon.info(_('Starting to upscale image'))
                    self.process_pool.append(self.driver_object.upscale(self.current_input_file, output_path))
                    self._wait()
                    Avalon.info(_('Upscaling completed'))

                    # static images don't require GIF or video encoding
                    # go to the next task
                    self.processing_queue.task_done()
                    self.total_processed += 1
                    continue

                # if input file is a image/gif file or a video
                elif input_file_mime_type == 'image/gif' or input_file_type == 'video':

                    # drivers that have native support for video processing
                    if input_file_type == 'video' and self.driver == 'anime4kcpp':
                        Avalon.info(_('Starting to upscale video with Anime4KCPP'))
                        # enable video processing mode for Anime4KCPP
                        self.driver_settings['videoMode'] = True
                        self.process_pool.append(self.driver_object.upscale(self.current_input_file, output_path))
                        self._wait()
                        Avalon.info(_('Upscaling completed'))
                        self.processing_queue.task_done()
                        self.total_processed += 1
                        continue

                    else:
                        self.create_temp_directories()

                        # get video information JSON using FFprobe
                        Avalon.info(_('Reading video information'))
                        video_info = self.ffmpeg_object.probe_file_info(self.current_input_file)
                        # analyze original video with FFprobe and retrieve framerate
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

                        # get average frame rate of video stream
                        framerate = float(Fraction(video_info['streams'][video_stream_index]['r_frame_rate']))
                        # self.ffmpeg_object.pixel_format = video_info['streams'][video_stream_index]['pix_fmt']

                        # extract frames from video
                        self.process_pool.append((self.ffmpeg_object.extract_frames(self.current_input_file, self.extracted_frames)))
                        self._wait()

                        # if driver is waifu2x-caffe
                        # pass pixel format output depth information
                        if self.driver == 'waifu2x_caffe':
                            # get a dict of all pixel formats and corresponding bit depth
                            pixel_formats = self.ffmpeg_object.get_pixel_formats()

                            # try getting pixel format's corresponding bti depth
                            try:
                                self.driver_settings['output_depth'] = pixel_formats[self.ffmpeg_object.pixel_format]
                            except KeyError:
                                Avalon.error(_('Unsupported pixel format: {}').format(self.ffmpeg_object.pixel_format))
                                raise UnsupportedPixelError(f'unsupported pixel format {self.ffmpeg_object.pixel_format}')

                        Avalon.info(_('Framerate: {}').format(framerate))

                        # width/height will be coded width/height x upscale factor
                        # original_width = video_info['streams'][video_stream_index]['width']
                        # original_height = video_info['streams'][video_stream_index]['height']
                        # scale_width = int(self.scale_ratio * original_width)
                        # scale_height = int(self.scale_ratio * original_height)

                        # upscale images one by one using waifu2x
                        Avalon.info(_('Starting to upscale extracted frames'))
                        self._upscale_frames()
                        Avalon.info(_('Upscaling completed'))

                # if file is none of: image, image/gif, video
                # skip to the next task
                else:
                    Avalon.error(_('File {} ({}) neither an image of a video').format(self.current_input_file, input_file_mime_type))
                    Avalon.warning(_('Skipping this file'))
                    self.processing_queue.task_done()
                    self.total_processed += 1
                    continue

                # start handling output
                # output can be either GIF or video

                # if the desired output is gif file
                if output_path.suffix.lower() == '.gif':
                    Avalon.info(_('Converting extracted frames into GIF image'))
                    gifski_object = Gifski(self.gifski_settings)
                    self.process_pool.append(gifski_object.make_gif(self.upscaled_frames, output_path, framerate, self.image_format))
                    self._wait()
                    Avalon.info(_('Conversion completed'))

                # if the desired output is video
                else:
                    # frames to video
                    Avalon.info(_('Converting extracted frames into video'))
                    self.process_pool.append(self.ffmpeg_object.assemble_video(framerate, self.upscaled_frames))
                    # f'{scale_width}x{scale_height}',
                    self._wait()
                    Avalon.info(_('Conversion completed'))

                    try:
                        # migrate audio tracks and subtitles
                        Avalon.info(_('Migrating audio, subtitles and other streams to upscaled video'))
                        self.process_pool.append(self.ffmpeg_object.migrate_streams(self.current_input_file,
                                                                                    output_path,
                                                                                    self.upscaled_frames))
                        self._wait()

                    # if failed to copy streams
                    # use file with only video stream
                    except subprocess.CalledProcessError:
                        traceback.print_exc()
                        Avalon.error(_('Failed to migrate streams'))
                        Avalon.warning(_('Trying to output video without additional streams'))

                        if input_file_mime_type == 'image/gif':
                            # copy will overwrite destination content if exists
                            shutil.copy(self.upscaled_frames / self.ffmpeg_object.intermediate_file_name, output_path)

                        else:
                            # construct output file path
                            output_file_name = f'{output_path.stem}{self.ffmpeg_object.intermediate_file_name.suffix}'
                            output_video_path = output_path.parent / output_file_name

                            # if output file already exists
                            # create temporary directory in output folder
                            # temporary directories generated by tempfile are guaranteed to be unique
                            # and won't conflict with other files
                            if output_video_path.exists():
                                Avalon.error(_('Output video file exists'))

                                temporary_directory = pathlib.Path(tempfile.mkdtemp(dir=output_path.parent))
                                output_video_path = temporary_directory / output_file_name
                                Avalon.info(_('Created temporary directory to contain file'))

                            # copy file to new destination
                            Avalon.info(_('Writing intermediate file to: {}').format(output_video_path.absolute()))
                            shutil.copy(self.upscaled_frames / self.ffmpeg_object.intermediate_file_name, output_video_path)

                # increment total number of files processed
                self.cleanup_temp_directories()
                self.processing_queue.task_done()
                self.total_processed += 1

        except (Exception, KeyboardInterrupt, SystemExit) as e:
            with contextlib.suppress(ValueError, AttributeError):
                self.cleanup_temp_directories()
                self.running = False
            raise e

        # signal upscaling completion
        self.running = False

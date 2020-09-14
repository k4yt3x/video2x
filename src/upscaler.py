#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2X Upscaler
Author: K4YT3X
Date Created: December 10, 2018
Last Modified: September 13, 2020

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
import math
import mimetypes
import pathlib
import queue
import re
import shutil
import subprocess
import tempfile
import time
import traceback

# third-party imports
from PIL import Image
from avalon_framework import Avalon
from tqdm import tqdm
import magic

# internationalization constants
DOMAIN = 'video2x'
LOCALE_DIRECTORY = pathlib.Path(__file__).parent.absolute() / 'locale'

# getting default locale settings
default_locale, encoding = locale.getdefaultlocale()
language = gettext.translation(DOMAIN, LOCALE_DIRECTORY, [default_locale], fallback=True)
language.install()
_ = language.gettext

# version information
UPSCALER_VERSION = '4.4.0'

# these names are consistent for
# - driver selection in command line
# - driver wrapper file names
# - config file keys
AVAILABLE_DRIVERS = ['waifu2x_caffe',
                     'waifu2x_converter_cpp',
                     'waifu2x_ncnn_vulkan',
                     'srmd_ncnn_vulkan',
                     'realsr_ncnn_vulkan',
                     'anime4kcpp']

# fixed scaling ratios supported by the drivers
# that only support certain fixed scale ratios
DRIVER_FIXED_SCALING_RATIOS = {
    'waifu2x_ncnn_vulkan': [1, 2],
    'srmd_ncnn_vulkan': [2, 3, 4],
    'realsr_ncnn_vulkan': [4],
}


class Upscaler:
    """ An instance of this class is a upscaler that will
    upscale all images in the given directory.

    Raises:
        Exception -- all exceptions
        ArgumentError -- if argument is not valid
    """

    def __init__(
        self,
        input_path: pathlib.Path or list,
        output_path: pathlib.Path,
        driver_settings: dict,
        ffmpeg_settings: dict,
        gifski_settings: dict,
        driver: str = 'waifu2x_caffe',
        scale_ratio: float = None,
        scale_width: int = None,
        scale_height: int = None,
        processes: int = 1,
        video2x_cache_directory: pathlib.Path = pathlib.Path(tempfile.gettempdir()) / 'video2x',
        extracted_frame_format: str = 'png',
        output_file_name_format_string: str = '{original_file_name}_output{extension}',
        image_output_extension: str = '.png',
        video_output_extension: str = '.mp4',
        preserve_frames: bool = False
    ):

        # required parameters
        self.input = input_path
        self.output = output_path
        self.driver_settings = driver_settings
        self.ffmpeg_settings = ffmpeg_settings
        self.gifski_settings = gifski_settings

        # optional parameters
        self.driver = driver
        self.scale_ratio = scale_ratio
        self.scale_width = scale_width
        self.scale_height = scale_height
        self.processes = processes
        self.video2x_cache_directory = video2x_cache_directory
        self.extracted_frame_format = extracted_frame_format
        self.output_file_name_format_string = output_file_name_format_string
        self.image_output_extension = image_output_extension
        self.video_output_extension = video_output_extension
        self.preserve_frames = preserve_frames

        # other internal members and signals
        self.running = False
        self.current_processing_starting_time = time.time()
        self.total_frames_upscaled = 0
        self.total_frames = 0
        self.total_files = 0
        self.total_processed = 0
        self.scaling_jobs = []
        self.current_pass = 0
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
                except FileNotFoundError:
                    pass
                except OSError:
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

    def _upscale_frames(self, input_directory: pathlib.Path, output_directory: pathlib.Path):
        """ Upscale video frames with waifu2x-caffe

        This function upscales all the frames extracted
        by ffmpeg using the waifu2x-caffe binary.

        Args:
            input_directory (pathlib.Path): directory containing frames to upscale
            output_directory (pathlib.Path): directory which upscaled frames should be exported to

        Raises:
            UnrecognizedDriverError: raised when the given driver is not recognized
            e: re-raised exception after an exception has been captured and finished processing in this scope
        """

        # initialize waifu2x driver
        if self.driver not in AVAILABLE_DRIVERS:
            raise UnrecognizedDriverError(_('Unrecognized driver: {}').format(self.driver))

        # list all images in the extracted frames
        frames = [(input_directory / f) for f in input_directory.iterdir() if f.is_file]

        # if we have less images than processes,
        # create only the processes necessary
        if len(frames) < self.processes:
            self.processes = len(frames)

        # create a directory for each process and append directory
        # name into a list
        process_directories = []
        for process_id in range(self.processes):
            process_directory = input_directory / str(process_id)
            process_directories.append(process_directory)

            # delete old directories and create new directories
            if process_directory.is_dir():
                shutil.rmtree(process_directory)
            process_directory.mkdir(parents=True, exist_ok=True)

        # waifu2x-converter-cpp will perform multi-threading within its own process
        if self.driver in ['waifu2x_converter_cpp', 'waifu2x_ncnn_vulkan', 'srmd_ncnn_vulkan', 'realsr_ncnn_vulkan', 'anime4kcpp']:
            process_directories = [input_directory]

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
            self.process_pool.append(self.driver_object.upscale(process_directory, output_directory))

        # start progress bar in a different thread
        Avalon.debug_info(_('Starting progress monitor'))
        self.progress_monitor = ProgressMonitor(self, process_directories)
        self.progress_monitor.start()

        # create the clearer and start it
        Avalon.debug_info(_('Starting upscaled image cleaner'))
        self.image_cleaner = ImageCleaner(input_directory, output_directory, len(self.process_pool))
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
            for image in [f for f in output_directory.iterdir() if f.is_file()]:
                renamed = re.sub(f'_\\[.*\\]\\[x(\\d+(\\.\\d+)?)\\]\\.{self.extracted_frame_format}',
                                 f'.{self.extracted_frame_format}',
                                 str(image.name))
                (output_directory / image).rename(output_directory / renamed)

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
        self.ffmpeg_object = Ffmpeg(self.ffmpeg_settings, extracted_frame_format=self.extracted_frame_format)

        # define processing queue
        self.processing_queue = queue.Queue()

        Avalon.info(_('Loading files into processing queue'))
        Avalon.debug_info(_('Input path(s): {}').format(self.input))

        # make output directory if the input is a list or a directory
        if isinstance(self.input, list) or self.input.is_dir():
            self.output.mkdir(parents=True, exist_ok=True)

        input_files = []

        # if input is single directory
        # put it in a list for compability with the following code
        if not isinstance(self.input, list):
            input_paths = [self.input]
        else:
            input_paths = self.input

        # flatten directories into file paths
        for input_path in input_paths:

            # if the input path is a single file
            # add the file's path object to input_files
            if input_path.is_file():
                input_files.append(input_path)

            # if the input path is a directory
            # add all files under the directory into the input_files (non-recursive)
            elif input_path.is_dir():
                input_files.extend([f for f in input_path.iterdir() if f.is_file()])

        output_paths = []

        for input_path in input_files:

            # get file type
            # try python-magic if it's available
            try:
                input_file_mime_type = magic.from_file(str(input_path.absolute()), mime=True)
                input_file_type = input_file_mime_type.split('/')[0]
                input_file_subtype = input_file_mime_type.split('/')[1]
            except Exception:

                # in case python-magic fails to detect file type
                # try guessing file mime type with mimetypes
                input_file_mime_type = mimetypes.guess_type(input_path.name)[0]
                input_file_type = input_file_mime_type.split('/')[0]
                input_file_subtype = input_file_mime_type.split('/')[1]

            Avalon.debug_info(_('File MIME type: {}').format(input_file_mime_type))

            # set default output file suffixes
            # if image type is GIF, default output suffix is also .gif
            if input_file_mime_type == 'image/gif':
                output_path = self.output / self.output_file_name_format_string.format(original_file_name=input_path.stem, extension='.gif')

            elif input_file_type == 'image':
                output_path = self.output / self.output_file_name_format_string.format(original_file_name=input_path.stem, extension=self.image_output_extension)

            elif input_file_type == 'video':
                output_path = self.output / self.output_file_name_format_string.format(original_file_name=input_path.stem, extension=self.video_output_extension)

            # if file is none of: image, image/gif, video
            # skip to the next task
            else:
                Avalon.error(_('File {} ({}) neither an image nor a video').format(input_path, input_file_mime_type))
                Avalon.warning(_('Skipping this file'))
                continue

            # if there is only one input file
            # do not modify output file suffix
            if isinstance(self.input, pathlib.Path) and self.input.is_file():
                output_path = self.output

            output_path_id = 0
            while str(output_path) in output_paths:
                output_path = output_path.parent / pathlib.Path(f'{output_path.stem}_{output_path_id}{output_path.suffix}')
                output_path_id += 1

            # record output path
            output_paths.append(str(output_path))

            # push file information into processing queue
            self.processing_queue.put((input_path.absolute(), output_path.absolute(), input_file_mime_type, input_file_type, input_file_subtype))

        # check argument sanity before running
        self._check_arguments()

        # record file count for external calls
        self.total_files = self.processing_queue.qsize()

        Avalon.info(_('Loaded files into processing queue'))
        # print all files in queue for debugging
        for job in self.processing_queue.queue:
            Avalon.debug_info(_('Input file: {}').format(job[0].absolute()))

        try:
            while not self.processing_queue.empty():

                # get new job from queue
                self.current_input_file, output_path, input_file_mime_type, input_file_type, input_file_subtype = self.processing_queue.get()

                # get current job starting time for GUI calculations
                self.current_processing_starting_time = time.time()

                # get video information JSON using FFprobe
                Avalon.info(_('Reading file information'))
                file_info = self.ffmpeg_object.probe_file_info(self.current_input_file)

                # create temporary directories for storing frames
                self.create_temp_directories()

                # start handling input
                # if input file is a static image
                if input_file_type == 'image' and input_file_subtype != 'gif':
                    Avalon.info(_('Starting upscaling image'))

                    # copy original file into the pre-processing directory
                    shutil.copy(self.current_input_file, self.extracted_frames / self.current_input_file.name)

                    width = int(file_info['streams'][0]['width'])
                    height = int(file_info['streams'][0]['height'])
                    framerate = self.total_frames = 1

                # elif input_file_mime_type == 'image/gif' or input_file_type == 'video':
                else:
                    Avalon.info(_('Starting upscaling video/GIF'))

                    # find index of video stream
                    video_stream_index = None
                    for stream in file_info['streams']:
                        if stream['codec_type'] == 'video':
                            video_stream_index = stream['index']
                            break

                    # exit if no video stream found
                    if video_stream_index is None:
                        Avalon.error(_('Aborting: No video stream found'))
                        raise StreamNotFoundError('no video stream found')

                    # get average frame rate of video stream
                    framerate = float(Fraction(file_info['streams'][video_stream_index]['r_frame_rate']))
                    width = int(file_info['streams'][video_stream_index]['width'])
                    height = int(file_info['streams'][video_stream_index]['height'])

                    # get total number of frames
                    Avalon.info(_('Getting total number of frames in the file'))

                    # if container stores total number of frames in nb_frames, fetch it directly
                    if 'nb_frames' in file_info['streams'][video_stream_index]:
                        self.total_frames = int(file_info['streams'][video_stream_index]['nb_frames'])

                    # otherwise call FFprobe to count the total number of frames
                    else:
                        self.total_frames = self.ffmpeg_object.get_number_of_frames(self.current_input_file, video_stream_index)

                # calculate scale width/height/ratio and scaling jobs if required
                Avalon.info(_('Calculating scaling parameters'))

                # create a local copy of the global output settings
                output_scale = self.scale_ratio
                output_width = self.scale_width
                output_height = self.scale_height

                # calculate output width and height if scale ratio is specified
                if output_scale is not None:
                    output_width = int(math.ceil(width * output_scale / 2.0) * 2)
                    output_height = int(math.ceil(height * output_scale / 2.0) * 2)

                else:
                    # scale keeping aspect ratio is only one of width/height is given
                    if output_width == 0 or output_width is None:
                        output_width = output_height / height * width

                    elif output_height == 0 or output_height is None:
                        output_height = output_width / width * height

                    output_width = int(math.ceil(output_width / 2.0) * 2)
                    output_height = int(math.ceil(output_height / 2.0) * 2)

                    # calculate required minimum scale ratio
                    output_scale = max(output_width / width, output_height / height)

                # if driver is one of the drivers that doesn't support arbitrary scaling ratio
                # TODO: more documentations on this block
                if self.driver in DRIVER_FIXED_SCALING_RATIOS:

                    # select the optimal driver scaling ratio to use
                    supported_scaling_ratios = sorted(DRIVER_FIXED_SCALING_RATIOS[self.driver])

                    remaining_scaling_ratio = math.ceil(output_scale)
                    self.scaling_jobs = []

                    while remaining_scaling_ratio > 1:
                        for ratio in supported_scaling_ratios:
                            if ratio >= remaining_scaling_ratio:
                                self.scaling_jobs.append(ratio)
                                remaining_scaling_ratio /= ratio
                                break

                        else:

                            found = False
                            for i in supported_scaling_ratios:
                                for j in supported_scaling_ratios:
                                    if i * j >= remaining_scaling_ratio:
                                        self.scaling_jobs.extend([i, j])
                                        remaining_scaling_ratio /= i * j
                                        found = True
                                        break
                                if found is True:
                                    break

                            if found is False:
                                self.scaling_jobs.append(supported_scaling_ratios[-1])
                                remaining_scaling_ratio /= supported_scaling_ratios[-1]

                else:
                    self.scaling_jobs = [output_scale]

                # print file information
                Avalon.debug_info(_('Framerate: {}').format(framerate))
                Avalon.debug_info(_('Width: {}').format(width))
                Avalon.debug_info(_('Height: {}').format(height))
                Avalon.debug_info(_('Total number of frames: {}').format(self.total_frames))
                Avalon.debug_info(_('Output width: {}').format(output_width))
                Avalon.debug_info(_('Output height: {}').format(output_height))
                Avalon.debug_info(_('Required scale ratio: {}').format(output_scale))
                Avalon.debug_info(_('Upscaling jobs queue: {}').format(self.scaling_jobs))

                # extract frames from video
                if input_file_mime_type == 'image/gif' or input_file_type == 'video':
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

                # upscale images one by one using waifu2x
                Avalon.info(_('Starting to upscale extracted frames'))
                upscale_begin_time = time.time()

                self.current_pass = 1
                if self.driver == 'waifu2x_caffe':
                    self.driver_object.set_scale_resolution(output_width, output_height)
                else:
                    self.driver_object.set_scale_ratio(self.scaling_jobs[0])
                self._upscale_frames(self.extracted_frames, self.upscaled_frames)
                for job in self.scaling_jobs[1:]:
                    self.current_pass += 1
                    self.driver_object.set_scale_ratio(job)
                    shutil.rmtree(self.extracted_frames)
                    shutil.move(self.upscaled_frames, self.extracted_frames)
                    self.upscaled_frames.mkdir(parents=True, exist_ok=True)
                    self._upscale_frames(self.extracted_frames, self.upscaled_frames)

                Avalon.info(_('Upscaling completed'))
                Avalon.info(_('Average processing speed: {} seconds per frame').format(self.total_frames / (time.time() - upscale_begin_time)))

                # downscale frames with Lanczos
                Avalon.info(_('Lanczos downscaling frames'))
                shutil.rmtree(self.extracted_frames)
                shutil.move(self.upscaled_frames, self.extracted_frames)
                self.upscaled_frames.mkdir(parents=True, exist_ok=True)

                for image in tqdm([i for i in self.extracted_frames.iterdir() if i.is_file() and i.name.endswith(self.extracted_frame_format)], ascii=True, desc=_('Downscaling')):
                    image_object = Image.open(image)

                    # if the image dimensions are not equal to the output size
                    # resize the image using Lanczos
                    if (image_object.width, image_object.height) != (output_width, output_height):
                        image_object.resize((output_width, output_height), Image.LANCZOS).save(self.upscaled_frames / image.name)
                        image_object.close()

                    # if the image's dimensions are already equal to the output size
                    # move image to the finished directory
                    else:
                        image_object.close()
                        shutil.move(image, self.upscaled_frames / image.name)

                # start handling output
                # output can be either GIF or video
                if input_file_type == 'image' and input_file_subtype != 'gif':

                    Avalon.info(_('Exporting image'))

                    # there should be only one image in the directory
                    shutil.move([f for f in self.upscaled_frames.iterdir() if f.is_file()][0], output_path)

                # elif input_file_mime_type == 'image/gif' or input_file_type == 'video':
                else:

                    # if the desired output is gif file
                    if output_path.suffix.lower() == '.gif':
                        Avalon.info(_('Converting extracted frames into GIF image'))
                        gifski_object = Gifski(self.gifski_settings)
                        self.process_pool.append(gifski_object.make_gif(self.upscaled_frames, output_path, framerate, self.extracted_frame_format, output_width, output_height))
                        self._wait()
                        Avalon.info(_('Conversion completed'))

                    # if the desired output is video
                    else:
                        # frames to video
                        Avalon.info(_('Converting extracted frames into video'))
                        self.process_pool.append(self.ffmpeg_object.assemble_video(framerate, self.upscaled_frames))
                        # f'{scale_width}x{scale_height}'
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

                                # move file to new destination
                                Avalon.info(_('Writing intermediate file to: {}').format(output_video_path.absolute()))
                                shutil.move(self.upscaled_frames / self.ffmpeg_object.intermediate_file_name, output_video_path)

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

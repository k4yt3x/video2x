#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""

__      __  _       _                  ___   __   __
\ \    / / (_)     | |                |__ \  \ \ / /
 \ \  / /   _    __| |   ___    ___      ) |  \ V /
  \ \/ /   | |  / _` |  / _ \  / _ \    / /    > <
   \  /    | | | (_| | |  __/ | (_) |  / /_   / . \
    \/     |_|  \__,_|  \___|  \___/  |____| /_/ \_\


Name: Video2X Controller
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: August 29, 2019

Dev: BrianPetkovsek
Dev: SAT3LL

Licensed under the GNU General Public License Version 3 (GNU GPL v3),
    available at: https://www.gnu.org/licenses/gpl-3.0.txt

(C) 2018-2019 K4YT3X

Video2X is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Video2X is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

Description: Video2X is an automation software based on waifu2x image
enlarging engine. It extracts frames from a video, enlarge it by a
number of times without losing any details or quality, keeping lines
smooth and edges sharp.
"""

# local imports
from exceptions import *
from upscaler import AVAILABLE_DRIVERS
from upscaler import Upscaler
import common

# built-in imports
import argparse
import contextlib
import json
import pathlib
import re
import shutil
import sys
import tempfile
import time
import traceback
from pathlib import Path

# third-party imports
from avalon_framework import Avalon
import GPUtil
import psutil

VERSION = '2.10.0'

LEGAL_INFO = f'''Video2X Version: {VERSION}
Author: K4YT3X
License: GNU GPL v3
Github Page: https://github.com/k4yt3x/video2x
Contact: k4yt3x@k4yt3x.com'''

LOGO = r'''
    __      __  _       _                  ___   __   __
    \ \    / / (_)     | |                |__ \  \ \ / /
     \ \  / /   _    __| |   ___    ___      ) |  \ V /
      \ \/ /   | |  / _` |  / _ \  / _ \    / /    > <
       \  /    | | | (_| | |  __/ | (_) |  / /_   / . \
        \/     |_|  \__,_|  \___|  \___/  |____| /_/ \_\
'''

# each thread might take up to 2.5 GB during initialization.
# (system memory, not to be confused with GPU memory)
SYS_MEM_PER_THREAD = 2.5
GPU_MEM_PER_THREAD = 3.5


def process_arguments():
    """Processes CLI arguments

    This function parses all arguments
    This allows users to customize options
    for the output video.
    """
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    # video options
    file_options = parser.add_argument_group('File Options')
    file_options.add_argument('-i', '--input', type=pathlib.Path, help='source video file/directory', action='store')
    file_options.add_argument('-o', '--output', type=pathlib.Path, help='output video file/directory', action='store')

    # upscaler options
    upscaler_options = parser.add_argument_group('Upscaler Options')
    upscaler_options.add_argument('-m', '--method', help='upscaling method', action='store', default='gpu', choices=['cpu', 'gpu', 'cudnn'])
    upscaler_options.add_argument('-d', '--driver', help='upscaling driver', action='store', default='waifu2x_caffe', choices=AVAILABLE_DRIVERS)
    upscaler_options.add_argument('-y', '--model_dir', type=pathlib.Path, help='directory containing model JSON files', action='store')
    upscaler_options.add_argument('-t', '--threads', help='number of threads to use for upscaling', action='store', type=int, default=1)
    upscaler_options.add_argument('-c', '--config', type=pathlib.Path, help='video2x config file location', action='store', default=pathlib.Path(sys.argv[0]).parent.absolute() / 'video2x.json')
    upscaler_options.add_argument('-b', '--batch', help='enable batch mode (select all default values to questions)', action='store_true')

    # scaling options
    scaling_options = parser.add_argument_group('Scaling Options')
    scaling_options.add_argument('--width', help='output video width', action='store', type=int)
    scaling_options.add_argument('--height', help='output video height', action='store', type=int)
    scaling_options.add_argument('-r', '--ratio', help='scaling ratio', action='store', type=float)

    # extra options
    extra_options = parser.add_argument_group('Extra Options')
    extra_options.add_argument('-v', '--version', help='display version, lawful information and exit', action='store_true')

    # parse arguments
    return parser.parse_args()


def print_logo():
    """print video2x logo"""
    print(LOGO)
    print(f'\n{"Video2X Video Enlarger".rjust(40, " ")}')
    print(f'\n{Avalon.FM.BD}{f"Version {VERSION}".rjust(36, " ")}{Avalon.FM.RST}\n')


def check_memory():
    """ Check usable system memory
    Warn the user if insufficient memory is available for
    the number of threads that the user have chosen.
    """

    memory_status = []
    # get system available memory
    system_memory_available = psutil.virtual_memory().available / (1024 ** 3)
    memory_status.append(('system', system_memory_available))

    # check if Nvidia-smi is available
    # GPUtil requires nvidia-smi.exe to interact with GPU
    if args.method == 'gpu' or args.method == 'cudnn':
        if not (shutil.which('nvidia-smi') or
                pathlib.Path(r'C:\Program Files\NVIDIA Corporation\NVSMI\nvidia-smi.exe').is_file()):
            # Nvidia System Management Interface not available
            Avalon.warning('Nvidia-smi not available, skipping available memory check')
            Avalon.warning('If you experience error \"cudaSuccess out of memory\", try reducing number of threads you\'re using')
        else:
            with contextlib.suppress(ValueError):
                # "0" is GPU ID. Both waifu2x drivers use the first GPU available, therefore only 0 makes sense
                gpu_memory_available = (GPUtil.getGPUs()[0].memoryTotal - GPUtil.getGPUs()[0].memoryUsed) / 1024
                memory_status.append(('GPU', gpu_memory_available))

    # go though each checkable memory type and check availability
    for memory_type, memory_available in memory_status:

        if memory_type == 'system':
            mem_per_thread = SYS_MEM_PER_THREAD
        else:
            mem_per_thread = GPU_MEM_PER_THREAD

        # if user doesn't even have enough memory to run even one thread
        if memory_available < mem_per_thread:
            Avalon.warning(f'You might have insufficient amount of {memory_type} memory available to run this program ({memory_available} GB)')
            Avalon.warning('Proceed with caution')
            if args.threads > 1:
                if Avalon.ask('Reduce number of threads to avoid crashing?', default=True, batch=args.batch):
                    args.threads = 1
        # if memory available is less than needed, warn the user
        elif memory_available < (mem_per_thread * args.threads):
            Avalon.warning(f'Each waifu2x-caffe thread will require up to {SYS_MEM_PER_THREAD} GB of system memory')
            Avalon.warning(f'You demanded {args.threads} threads to be created, but you only have {round(memory_available, 4)} GB {memory_type} memory available')
            Avalon.warning(f'{mem_per_thread * args.threads} GB of {memory_type} memory is recommended for {args.threads} threads')
            Avalon.warning(f'With your current amount of {memory_type} memory available, {int(memory_available // mem_per_thread)} threads is recommended')

            # ask the user if he / she wants to change to the recommended
            # number of threads
            if Avalon.ask('Change to the recommended value?', default=True, batch=args.batch):
                args.threads = int(memory_available // mem_per_thread)
            else:
                Avalon.warning('Proceed with caution')


def read_config(config_file):
    """ Reads configuration file

    Returns a dictionary read by JSON.
    """
    with open(config_file, 'r') as raw_config:
        config = json.load(raw_config)
        return config


# /////////////////// Execution /////////////////// #

# this is not a library
if __name__ != '__main__':
    Avalon.error('This file cannot be imported')
    raise ImportError(f'{__file__} cannot be imported')

# print video2x logo
print_logo()

# process CLI arguments
args = process_arguments()

# display version and lawful informaition
if args.version:
    print(LEGAL_INFO)
    exit(0)

# arguments sanity check
if not args.input:
    Avalon.error('You must specify input video file/directory path')
    raise ArgumentError('input video path not specified')
if not args.output:
    Avalon.error('You must specify output video file/directory path')
    raise ArgumentError('output video path not specified')
if (args.driver in ['waifu2x_converter', 'waifu2x_ncnn_vulkan', 'anime4k']) and args.width and args.height:
    Avalon.error('Selected driver accepts only scaling ratio')
    raise ArgumentError('selected driver supports only scaling ratio')
if args.driver == 'waifu2x_ncnn_vulkan' and (args.ratio > 2 or not args.ratio.is_integer()):
    Avalon.error('Scaling ratio must be 1 or 2 for waifu2x_ncnn_vulkan')
    raise ArgumentError('scaling ratio must be 1 or 2 for waifu2x_ncnn_vulkan')
if (args.width or args.height) and args.ratio:
    Avalon.error('You can only specify either scaling ratio or output width and height')
    raise ArgumentError('both scaling ration and width/height specified')
if (args.width and not args.height) or (not args.width and args.height):
    Avalon.error('You must specify both width and height')
    raise ArgumentError('only one of width or height is specified')

# check available memory if driver is waifu2x-based
if args.driver in ['waifu2x_caffe', 'waifu2x_converter', 'waifu2x_ncnn_vulkan']:
    check_memory()

# anime4k runs significantly faster with more threads
if args.driver == 'anime4k' and args.threads <= 1:
    Avalon.warning('Anime4K runs significantly faster with more threads')
    if Avalon.ask('Use more threads of Anime4K?', default=True, batch=args.batch):
        while True:
            try:
                threads = Avalon.gets('Amount of threads to use [5]: ', default=5, batch=args.batch)
                args.threads = int(threads)
                break
            except ValueError:
                if threads == '':
                    args.threads = 5
                    break
                else:
                    Avalon.error(f'{threads} is not a valid integer')

# read configurations from JSON
config = read_config(args.config)

# load waifu2x configuration
waifu2x_settings = config[args.driver]

# Search for valid java binary path
if args.driver == 'anime4k':
    path = common.find_path(waifu2x_settings['java_path'], 'java')
    if path[0] is None and path[1] is None:
        raise FileNotFoundError(Path(waifu2x_settings['java_path']).absolute() / 'java')

    waifu2x_settings['java_path'] = path[0]
    waifu2x_settings['java_binary'] = path[1]


# Search for valid waifu2x binary path
if 'win_binary' in waifu2x_settings and sys.platform == 'win32':
    waifu2x_settings['binary'] = waifu2x_settings['win_binary']

path = common.find_path(waifu2x_settings['path'], waifu2x_settings['binary'])
if path[0] is None and path[1] is None:
    raise FileNotFoundError(Path(waifu2x_settings['path']).absolute() / waifu2x_settings['binary'])

waifu2x_settings['path'] = path[0]
waifu2x_settings['binary'] = path[1]

# read FFmpeg configuration
ffmpeg_settings = config['ffmpeg']

# Search for valid ffmpeg path
ffmpeg_path = common.find_path(ffmpeg_settings['path'], 'ffmpeg')
ffprobe_path = common.find_path(ffmpeg_settings['path'], 'ffprobe')
ffmpeg_settings['path'] = ffmpeg_path[0]
ffmpeg_settings['ffmpeg_binary'] = ffmpeg_path[1]
ffmpeg_settings['ffprobe_binary'] = ffprobe_path[1]

# load video2x settings
image_format = config['video2x']['image_format'].lower()
preserve_frames = config['video2x']['preserve_frames']

# load cache directory
if isinstance(config['video2x']['video2x_cache_directory'], str):
    video2x_cache_directory = pathlib.Path(config['video2x']['video2x_cache_directory'])
else:
    video2x_cache_directory = pathlib.Path(tempfile.gettempdir()) / 'video2x'

if video2x_cache_directory.exists() and not video2x_cache_directory.is_dir():
    Avalon.error('Specified cache directory is a file/link')
    raise FileExistsError('Specified cache directory is a file/link')

elif not video2x_cache_directory.exists():
    # if destination file is a file or a symbolic link
    Avalon.warning(f'Specified cache directory {video2x_cache_directory} does not exist')

    # try creating the cache directory
    if Avalon.ask('Create directory?', default=True, batch=args.batch):
        try:
            video2x_cache_directory.mkdir(parents=True, exist_ok=True)
            Avalon.info(f'{video2x_cache_directory} created')

        # there can be a number of exceptions here
        # PermissionError, FileExistsError, etc.
        # therefore, we put a catch-them-all here
        except Exception as e:
            Avalon.error(f'Unable to create {video2x_cache_directory}')
            Avalon.error('Aborting...')
            raise e
    else:
        raise FileNotFoundError('Could not create cache directory')


# start execution
try:
    # start timer
    begin_time = time.time()

    # if input specified is a single file
    if args.input.is_file():

        # upscale single video file
        Avalon.info(f'Upscaling single video file: {args.input}')

        # check for input output format mismatch
        if args.output.is_dir():
            Avalon.error('Input and output path type mismatch')
            Avalon.error('Input is single file but output is directory')
            raise Exception('input output path type mismatch')
        if not re.search(r'.*\..*$', str(args.output)):
            Avalon.error('No suffix found in output file path')
            Avalon.error('Suffix must be specified for FFmpeg')
            raise Exception('No suffix specified')

        upscaler = Upscaler(input_video=args.input, output_video=args.output, method=args.method, waifu2x_settings=waifu2x_settings, ffmpeg_settings=ffmpeg_settings)

        # set optional options
        upscaler.waifu2x_driver = args.driver
        upscaler.scale_width = args.width
        upscaler.scale_height = args.height
        upscaler.scale_ratio = args.ratio
        upscaler.model_dir = args.model_dir
        upscaler.threads = args.threads
        upscaler.video2x_cache_directory = video2x_cache_directory
        upscaler.image_format = image_format
        upscaler.preserve_frames = preserve_frames

        # run upscaler
        upscaler.create_temp_directories()
        upscaler.run()
        upscaler.cleanup_temp_directories()

    # if input specified is a directory
    elif args.input.is_dir():
        # upscale videos in a directory
        Avalon.info(f'Upscaling videos in directory: {args.input}')

        # make output directory if it doesn't exist
        args.output.mkdir(parents=True, exist_ok=True)

        for input_video in [f for f in args.input.iterdir() if f.is_file()]:
            output_video = args.output / input_video.name
            upscaler = Upscaler(input_video=input_video, output_video=output_video, method=args.method, waifu2x_settings=waifu2x_settings, ffmpeg_settings=ffmpeg_settings)

            # set optional options
            upscaler.waifu2x_driver = args.driver
            upscaler.scale_width = args.width
            upscaler.scale_height = args.height
            upscaler.scale_ratio = args.ratio
            upscaler.model_dir = args.model_dir
            upscaler.threads = args.threads
            upscaler.video2x_cache_directory = video2x_cache_directory
            upscaler.image_format = image_format
            upscaler.preserve_frames = preserve_frames

            # run upscaler
            upscaler.create_temp_directories()
            upscaler.run()
            upscaler.cleanup_temp_directories()
    else:
        Avalon.error('Input path is neither a file nor a directory')
        raise FileNotFoundError(f'{args.input} is neither file nor directory')

    Avalon.info(f'Program completed, taking {round((time.time() - begin_time), 5)} seconds')
except Exception:
    Avalon.error('An exception has occurred')
    traceback.print_exc()
finally:
    # remove Video2X cache directory
    with contextlib.suppress(FileNotFoundError):
        if not preserve_frames:
            shutil.rmtree(video2x_cache_directory)

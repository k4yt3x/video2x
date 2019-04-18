#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""

__      __  _       _                  ___   __   __
\ \    / / (_)     | |                |__ \  \ \ / /
 \ \  / /   _    __| |   ___    ___      ) |  \ V /
  \ \/ /   | |  / _` |  / _ \  / _ \    / /    > <
   \  /    | | | (_| | |  __/ | (_) |  / /_   / . \
    \/     |_|  \__,_|  \___|  \___/  |____| /_/ \_\


Name: Video2X Controller
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: April 11, 2019

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
from avalon_framework import Avalon
from upscaler import Upscaler
import argparse
import GPUtil
import json
import os
import psutil
import re
import shutil
import sys
import tempfile
import time
import traceback

VERSION = '2.7.1'

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
    file_options.add_argument('-i', '--input', help='Source video file/directory', action='store', required=True)
    file_options.add_argument('-o', '--output', help='Output video file/directory', action='store', required=True)

    # upscaler options
    upscaler_options = parser.add_argument_group('Upscaler Options')
    upscaler_options.add_argument('-m', '--method', help='Upscaling method', action='store', default='gpu', choices=['cpu', 'gpu', 'cudnn'], required=True)
    upscaler_options.add_argument('-d', '--driver', help='Waifu2x driver', action='store', default='waifu2x_caffe', choices=['waifu2x_caffe', 'waifu2x_converter'])
    upscaler_options.add_argument('-y', '--model_dir', help='Folder containing model JSON files', action='store')
    upscaler_options.add_argument('-t', '--threads', help='Number of threads to use for upscaling', action='store', type=int, default=5)
    upscaler_options.add_argument('-c', '--config', help='Video2X config file location', action='store', default='{}\\video2x.json'.format(os.path.dirname(os.path.abspath(sys.argv[0]))))
    upscaler_options.add_argument('-b', '--batch', help='Enable batch mode (select all default values to questions)', action='store_true')

    # scaling options
    scaling_options = parser.add_argument_group('Scaling Options')
    scaling_options.add_argument('--width', help='Output video width', action='store', type=int)
    scaling_options.add_argument('--height', help='Output video height', action='store', type=int)
    scaling_options.add_argument('-r', '--ratio', help='Scaling ratio', action='store', type=float)

    # parse arguments
    return parser.parse_args()


def print_logo():
    print('__      __  _       _                  ___   __   __')
    print('\\ \\    / / (_)     | |                |__ \\  \\ \\ / /')
    print(' \\ \\  / /   _    __| |   ___    ___      ) |  \\ V /')
    print('  \\ \\/ /   | |  / _` |  / _ \\  / _ \\    / /    > <')
    print('   \\  /    | | | (_| | |  __/ | (_) |  / /_   / . \\')
    print('    \\/     |_|  \\__,_|  \\___|  \\___/  |____| /_/ \\_\\')
    print('\n               Video2X Video Enlarger')
    spaces = ((44 - len("Version {}".format(VERSION))) // 2) * " "
    print('{}\n{}    Version {}\n{}'.format(Avalon.FM.BD, spaces, VERSION, Avalon.FM.RST))


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
        if not os.path.isfile('C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvidia-smi.exe'):
            # Nvidia System Management Interface not available
            Avalon.warning('Nvidia-smi not available, skipping available memory check')
            Avalon.warning('If you experience error \"cudaSuccess out of memory\", try reducing number of threads you\'re using')
        else:
            try:
                # "0" is GPU ID. Both waifu2x drivers use the first GPU available, therefore only 0 makes sense
                gpu_memory_available = (GPUtil.getGPUs()[0].memoryTotal - GPUtil.getGPUs()[0].memoryUsed) / 1024
                memory_status.append(('GPU', gpu_memory_available))
            except ValueError:
                pass

    # go though each checkable memory type and check availability
    for memory_type, memory_available in memory_status:

        if memory_type == 'system':
            mem_per_thread = SYS_MEM_PER_THREAD
        else:
            mem_per_thread = GPU_MEM_PER_THREAD

        # if user doesn't even have enough memory to run even one thread
        if memory_available < mem_per_thread:
            Avalon.warning('You might have insufficient amount of {} memory available to run this program ({} GB)'.format(memory_type, memory_available))
            Avalon.warning('Proceed with caution')
            if args.threads > 1:
                if Avalon.ask('Reduce number of threads to avoid crashing?', default=True, batch=args.batch):
                    args.threads = 1
        # if memory available is less than needed, warn the user
        elif memory_available < (mem_per_thread * args.threads):
            Avalon.warning('Each waifu2x-caffe thread will require up to 2.5 GB of system memory')
            Avalon.warning('You demanded {} threads to be created, but you only have {} GB {} memory available'.format(args.threads, round(memory_available, 4), memory_type))
            Avalon.warning('{} GB of {} memory is recommended for {} threads'.format(mem_per_thread * args.threads, memory_type, args.threads))
            Avalon.warning('With your current amount of {} memory available, {} threads is recommended'.format(memory_type, int(memory_available // mem_per_thread)))

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


def absolutify_paths(config):
    """ Check to see if paths to binaries are absolute

    This function checks if paths to binary files are absolute.
    If not, then absolutify the path.

    Arguments:
        config {dict} -- configuration file dictionary

    Returns:
        dict -- configuration file dictionary
    """
    current_folder = os.path.dirname(os.path.abspath(sys.argv[0]))

    # check waifu2x-caffe path
    if not re.match('^[a-z]:', config['waifu2x_caffe']['waifu2x_caffe_path'], re.IGNORECASE):
        config['waifu2x_caffe']['waifu2x_caffe_path'] = '{}\\{}'.format(current_folder, config['waifu2x_caffe']['waifu2x_caffe_path'])

    # check waifu2x-converter-cpp path
    if not re.match('^[a-z]:', config['waifu2x_converter']['waifu2x_converter_path'], re.IGNORECASE):
        config['waifu2x_converter']['waifu2x_converter_path'] = '{}\\{}'.format(current_folder, config['waifu2x_converter']['waifu2x_converter_path'])

    # check ffmpeg path
    if not re.match('^[a-z]:', config['ffmpeg']['ffmpeg_path'], re.IGNORECASE):
        config['ffmpeg']['ffmpeg_path'] = '{}\\{}'.format(current_folder, config['ffmpeg']['ffmpeg_path'])

    # check video2x cache path
    if config['video2x']['video2x_cache_folder']:
        if not re.match('^[a-z]:', config['video2x']['video2x_cache_folder'], re.IGNORECASE):
            config['video2x']['video2x_cache_folder'] = '{}\\{}'.format(current_folder, config['video2x']['video2x_cache_folder'])

    return config


# /////////////////// Execution /////////////////// #

# this is not a library
if __name__ != '__main__':
    Avalon.error('This file cannot be imported')
    raise ImportError('{} cannot be imported'.format(__file__))

print_logo()

# process CLI arguments
args = process_arguments()

# arguments sanity check
if args.driver == 'waifu2x_converter' and args.width and args.height:
    Avalon.error('Waifu2x Converter CPP accepts only scaling ratio')
    exit(1)
if (args.width or args.height) and args.ratio:
    Avalon.error('You can only specify either scaling ratio or output width and height')
    exit(1)
if (args.width and not args.height) or (not args.width and args.height):
    Avalon.error('You must specify both width and height')
    exit(1)

# check available memory
check_memory()

# read configurations from JSON
config = read_config(args.config)
config = absolutify_paths(config)

# load waifu2x configuration
if args.driver == 'waifu2x_caffe':
    waifu2x_settings = config['waifu2x_caffe']
    if not os.path.isfile(waifu2x_settings['waifu2x_caffe_path']):
        Avalon.error('Specified waifu2x-caffe directory doesn\'t exist')
        Avalon.error('Please check the configuration file settings')
        raise FileNotFoundError(waifu2x_settings['waifu2x_caffe_path'])
elif args.driver == 'waifu2x_converter':
    waifu2x_settings = config['waifu2x_converter']
    if not os.path.isdir(waifu2x_settings['waifu2x_converter_path']):
        Avalon.error('Specified waifu2x-conver-cpp directory doesn\'t exist')
        Avalon.error('Please check the configuration file settings')
        raise FileNotFoundError(waifu2x_settings['waifu2x_converter_path'])

# check if waifu2x path is valid


# read FFMPEG configuration
ffmpeg_settings = config['ffmpeg']

# load video2x settings
video2x_cache_folder = config['video2x']['video2x_cache_folder']
image_format = config['video2x']['image_format'].lower()
preserve_frames = config['video2x']['preserve_frames']

# create temp directories if they don't exist
if not video2x_cache_folder:
    video2x_cache_folder = '{}\\video2x'.format(tempfile.gettempdir())

if video2x_cache_folder and not os.path.isdir(video2x_cache_folder):
    if not os.path.isfile(video2x_cache_folder) and not os.path.islink(video2x_cache_folder):
        Avalon.warning('Specified cache folder/directory {} does not exist'.format(video2x_cache_folder))
        if Avalon.ask('Create folder/directory?', default=True, batch=args.batch):
            if os.mkdir(video2x_cache_folder) is None:
                Avalon.info('{} created'.format(video2x_cache_folder))
            else:
                Avalon.error('Unable to create {}'.format(video2x_cache_folder))
                Avalon.error('Aborting...')
                exit(1)
    else:
        Avalon.error('Specified cache folder/directory is a file/link')
        Avalon.error('Unable to continue, exiting...')
        exit(1)


# start execution
try:
    # start timer
    begin_time = time.time()

    # if input specified is a single file
    if os.path.isfile(args.input):
        """ Upscale single video file """
        Avalon.info('Upscaling single video file: {}'.format(args.input))

        # check for input output format mismatch
        if os.path.isdir(args.output):
            Avalon.error('Input and output path type mismatch')
            Avalon.error('Input is single file but output is folder')
            raise Exception('input output path type mismatch')
        if not re.search('.*\..*$', args.output):
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
        upscaler.video2x_cache_folder = video2x_cache_folder
        upscaler.image_format = image_format
        upscaler.preserve_frames = preserve_frames

        # run upscaler
        upscaler.create_temp_folders()
        upscaler.run()
        upscaler.cleanup()

    # if input specified is a folder
    elif os.path.isdir(args.input):
        """ Upscale videos in a folder/directory """
        Avalon.info('Upscaling videos in folder/directory: {}'.format(args.input))
        for input_video in [f for f in os.listdir(args.input) if os.path.isfile(os.path.join(args.input, f))]:
            output_video = '{}\\{}'.format(args.output, input_video)
            upscaler = Upscaler(input_video=os.path.join(args.input, input_video), output_video=output_video, method=args.method, waifu2x_settings=waifu2x_settings, ffmpeg_settings=ffmpeg_settings)

            # set optional options
            upscaler.waifu2x_driver = args.driver
            upscaler.scale_width = args.width
            upscaler.scale_height = args.height
            upscaler.scale_ratio = args.ratio
            upscaler.model_dir = args.model_dir
            upscaler.threads = args.threads
            upscaler.video2x_cache_folder = video2x_cache_folder
            upscaler.image_format = image_format
            upscaler.preserve_frames = preserve_frames

            # run upscaler
            upscaler.create_temp_folders()
            upscaler.run()
            upscaler.cleanup()
    else:
        Avalon.error('Input path is neither a file nor a folder/directory')
        raise FileNotFoundError('{} is neither file nor folder/directory'.format(args.input))

    Avalon.info('Program completed, taking {} seconds'.format(round((time.time() - begin_time), 5)))
except Exception:
    Avalon.error('An exception has occurred')
    traceback.print_exc()
    Avalon.warning('If you experience error \"cudaSuccess out of memory\", try reducing number of threads you\'re using')
finally:
    # remove Video2X Cache folder
    try:
        if not preserve_frames:
            shutil.rmtree(video2x_cache_folder)
    except FileNotFoundError:
        pass

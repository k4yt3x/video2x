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
Last Modified: March 9, 2019

Licensed under the GNU General Public License Version 3 (GNU GPL v3),
    available at: https://www.gnu.org/licenses/gpl-3.0.txt

(C) 2018-2019 K4YT3X

Description: Video2X is an automation software based on
waifu2x image enlarging engine. It extracts frames from a
video, enlarge it by a number of times without losing any
details or quality, keeping lines smooth and edges sharp.
"""
from avalon_framework import Avalon
from upscaler import Upscaler
from upscaler import MODELS_AVAILABLE
import GPUtil
import argparse
import json
import os
import psutil
import shutil
import tempfile
import time
import traceback

VERSION = '2.6.0'

# Each thread might take up to 2.5 GB during initialization.
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

    # Video options
    basic_options = parser.add_argument_group('Basic Options')
    basic_options.add_argument('-i', '--input', help='Specify source video file/directory', action='store', default=False, required=True)
    basic_options.add_argument('-o', '--output', help='Specify output video file/directory', action='store', default=False, required=True)
    basic_options.add_argument('-m', '--method', help='Specify upscaling method', action='store', default='gpu', choices=['cpu', 'gpu', 'cudnn'], required=True)
    basic_options.add_argument('-d', '--driver', help='Waifu2x driver', action='store', default='waifu2x_caffe', choices=['waifu2x_caffe', 'waifu2x_converter'])
    basic_options.add_argument('-y', '--model_type', help='Specify model to use', action='store', default='models/cunet', choices=MODELS_AVAILABLE)
    basic_options.add_argument('-t', '--threads', help='Specify number of threads to use for upscaling', action='store', type=int, default=5)
    basic_options.add_argument('-c', '--config', help='Manually specify config file', action='store', default='{}\\video2x.json'.format(os.path.dirname(os.path.abspath(__file__))))

    # Scaling options
    # scaling_options = parser.add_argument_group('Scaling Options', required=True)  # TODO: (width & height) || (factor)
    scaling_options = parser.add_argument_group('Scaling Options')  # TODO: (width & height) || (factor)
    scaling_options.add_argument('--width', help='Output video width', action='store', type=int, default=False)
    scaling_options.add_argument('--height', help='Output video height', action='store', type=int, default=False)
    scaling_options.add_argument('-r', '--ratio', help='Scaling ratio', action='store', type=int, default=False)

    # Parse arguments
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
    # Get system available memory
    system_memory_available = psutil.virtual_memory().available / (1024 ** 3)
    memory_status.append(('system', system_memory_available))

    # Check if Nvidia-smi is available
    # GPUtil requires nvidia-smi.exe to interact with GPU
    if args.method == 'gpu' or args.method == 'cudnn':
        if not os.path.isfile('C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvidia-smi.exe'):
            # Nvidia System Management Interface not available
            Avalon.warning('Nvidia-smi not available, skipping available memory check')
            Avalon.warning('If you experience error \"cudaSuccess  out of memory, try reducing number of threads you\'re using\"')
        else:
            # "0" is GPU ID. Both waifu2x drivers use the first GPU available, therefore only 0 makes sense
            gpu_memory_available = (GPUtil.getGPUs()[0].memoryTotal - GPUtil.getGPUs()[0].memoryUsed) / 1024
            memory_status.append(('GPU', gpu_memory_available))

    # Go though each checkable memory type and check availability
    for memory_type, memory_available in memory_status:

        if memory_type == 'system':
            mem_per_thread = SYS_MEM_PER_THREAD
        else:
            mem_per_thread = GPU_MEM_PER_THREAD

        # If user doesn't even have enough memory to run even one thread
        if memory_available < mem_per_thread:
            Avalon.warning('You might have insufficient amount of {} memory available to run this program ({} GB)'.format(memory_type, memory_available))
            Avalon.warning('Proceed with caution')
            if args.threads > 1:
                if Avalon.ask('Reduce number of threads to avoid crashing?', True):
                    args.threads = 1
        # If memory available is less than needed, warn the user
        elif memory_available < (mem_per_thread * args.threads):
            Avalon.warning('Each waifu2x-caffe thread will require up to 2.5 GB of system memory')
            Avalon.warning('You demanded {} threads to be created, but you only have {} GB {} memory available'.format(args.threads, round(memory_available, 4), memory_type))
            Avalon.warning('{} GB of {} memory is recommended for {} threads'.format(mem_per_thread * args.threads, memory_type, args.threads))
            Avalon.warning('With your current amount of {} memory available, {} threads is recommended'.format(memory_type, int(memory_available // mem_per_thread)))

            # Ask the user if he / she wants to change to the recommended
            # number of threads
            if Avalon.ask('Change to the recommended value?', True):
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

# This is not a library
if __name__ != '__main__':
    Avalon.error('This file cannot be imported')
    raise ImportError('{} cannot be imported'.format(__file__))

print_logo()

# Process CLI arguments
args = process_arguments()

# Arguments sanity check
if args.driver == 'waifu2x_converter' and args.width and args.height:
    Avalon.error('Waifu2x Converter CPP accepts only scaling ratio')
    exit(1)
if (args.width or args.height) and args.ratio:
    Avalon.error('You can only specify either scaling ratio or output width and height')
    exit(1)
if (args.width and not args.height) or (not args.width and args.height):
    Avalon.error('You must specify both width and height')
    exit(1)

# Check available memory
check_memory()

# Read configurations from JSON
config = read_config(args.config)

# load waifu2x configuration
if args.driver == 'waifu2x_caffe':
    waifu2x_settings = config['waifu2x_caffe']
elif args.driver == 'waifu2x_converter':
    waifu2x_settings = config['waifu2x_converter']

# read FFMPEG configuration
ffmpeg_settings = config['ffmpeg']

# load video2x settings
video2x_cache_folder = config['video2x']['video2x_cache_folder']
preserve_frames = config['video2x']['preserve_frames']

# Create temp directories if they don't exist
if not video2x_cache_folder:
    video2x_cache_folder = '{}\\video2x'.format(tempfile.gettempdir())

if video2x_cache_folder and not os.path.isdir(video2x_cache_folder):
    if not os.path.isfile(video2x_cache_folder) and not os.path.islink(video2x_cache_folder):
        Avalon.warning('Specified cache folder/directory {} does not exist'.format(video2x_cache_folder))
        if Avalon.ask('Create folder/directory?', True):
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


# Start execution
try:
    # Start timer
    begin_time = time.time()

    if os.path.isfile(args.input):
        """ Upscale single video file """
        Avalon.info('Upscaling single video file: {}'.format(args.input))
        upscaler = Upscaler(input_video=args.input, output_video=args.output, method=args.method, waifu2x_settings=waifu2x_settings, ffmpeg_settings=ffmpeg_settings, waifu2x_driver=args.driver, scale_width=args.width, scale_height=args.height, scale_ratio=args.ratio, model_type=args.model_type, threads=args.threads, video2x_cache_folder=video2x_cache_folder)
        upscaler.run()
        upscaler.cleanup()
    elif os.path.isdir(args.input):
        """ Upscale videos in a folder/directory """
        Avalon.info('Upscaling videos in folder/directory: {}'.format(args.input))
        for input_video in [f for f in os.listdir(args.input) if os.path.isfile(os.path.join(args.input, f))]:
            output_video = '{}\\{}'.format(args.output, input_video)
            upscaler = Upscaler(input_video=os.path.join(args.input, input_video), output_video=output_video, method=args.method, waifu2x_settings=waifu2x_settings, ffmpeg_settings=ffmpeg_settings, waifu2x_driver=args.driver, scale_width=args.width, scale_height=args.height, scale_ratio=args.ratio, model_type=args.model_type, threads=args.threads, video2x_cache_folder=video2x_cache_folder)
            upscaler.run()
            upscaler.cleanup()
    else:
        Avalon.error('Input path is neither a file nor a folder/directory')
        raise FileNotFoundError('{} is neither file nor folder/directory'.format(args.input))

    Avalon.info('Program completed, taking {} seconds'.format(round((time.time() - begin_time), 5)))
except Exception:
    Avalon.error('An exception has occurred')
    traceback.print_exc()
finally:
    # Remove Video2X Cache folder
    try:
        if not preserve_frames:
            shutil.rmtree(video2x_cache_folder)
    except FileNotFoundError:
        pass

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
Last Modified: December 19, 2018

Licensed under the GNU General Public License Version 3 (GNU GPL v3),
    available at: https://www.gnu.org/licenses/gpl-3.0.txt

(C) 2018 K4YT3X

Description: Video2X is an automation software based on
waifu2x image enlarging engine. It extracts frames from a
video, enlarge it by a number of times without losing any
details or quality, keeping lines smooth and edges sharp.
"""
from avalon_framework import Avalon
from upscaler import Upscaler
from upscaler import MODELS_AVAILABLE
import argparse
import json
import os
import psutil
import time
import traceback

VERSION = '2.2.0'

# Each thread might take up to 2.5 GB during initialization.
# (system memory, not to be confused with GPU memory)
MEM_PER_THREAD = 2.5


def process_arguments():
    """Processes CLI arguments

    This function parses all arguments
    This allows users to customize options
    for the output video.
    """
    parser = argparse.ArgumentParser()

    # Video options
    basic_options = parser.add_argument_group('Basic Options')
    basic_options.add_argument('-i', '--input', help='Specify source video file/directory', action='store', default=False, required=True)
    basic_options.add_argument('-o', '--output', help='Specify output video file/directory', action='store', default=False, required=True)
    basic_options.add_argument('-m', '--method', help='Specify upscaling method', action='store', default='gpu', choices=['cpu', 'gpu', 'cudnn'], required=True)
    basic_options.add_argument('-y', '--model_type', help='Specify model to use', action='store', default='anime_style_art_rgb', choices=MODELS_AVAILABLE)
    basic_options.add_argument('-t', '--threads', help='Specify number of threads to use for upscaling', action='store', type=int, default=5)
    basic_options.add_argument('-c', '--config', help='Manually specify config file', action='store', default='video2x.json')

    # Scaling options
    # scaling_options = parser.add_argument_group('Scaling Options', required=True)  # TODO: (width & height) || (factor)
    scaling_options = parser.add_argument_group('Scaling Options')  # TODO: (width & height) || (factor)
    scaling_options.add_argument('--width', help='Output video width', action='store', type=int, default=False)
    scaling_options.add_argument('--height', help='Output video height', action='store', type=int, default=False)
    scaling_options.add_argument('-f', '--factor', help='Factor to upscale the videos by', action='store', type=int, default=False)

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


def check_system_memory():
    """ Check usable system memory
    Warn the user if insufficient memory is available for
    the number of threads that the user have chosen.
    """
    memory_available = psutil.virtual_memory().available / (1024 ** 3)

    # If user doesn't even have enough memory to run even one thread
    if memory_available < MEM_PER_THREAD:
        Avalon.warning('You might have an insufficient amount of memory available to run this program ({} GB)'.format(memory_available))
        Avalon.warning('Proceed with caution')
    # If memory available is less than needed, warn the user
    elif memory_available < (MEM_PER_THREAD * args.threads):
        Avalon.warning('Each waifu2x-caffe thread will require up to 2.5 GB during initialization')
        Avalon.warning('You demanded {} threads to be created, but you only have {} GB memory available'.format(args.threads, round(memory_available, 4)))
        Avalon.warning('{} GB of memory is recommended for {} threads'.format(MEM_PER_THREAD * args.threads, args.threads))
        Avalon.warning('With your current amount of memory available, {} threads is recommended'.format(int(memory_available // MEM_PER_THREAD)))

        # Ask the user if he / she wants to change to the recommended
        # number of threads
        if Avalon.ask('Change to the recommended value?', True):
            args.threads = int(memory_available // MEM_PER_THREAD)
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

# Check system available memory
check_system_memory()

# Read configurations from JSON
config = read_config(args.config)
waifu2x_path = config['waifu2x_path']
ffmpeg_path = config['ffmpeg_path']
ffmpeg_arguments = config['ffmpeg_arguments']
ffmpeg_hwaccel = config['ffmpeg_hwaccel']

# Start execution
try:
    # Start timer
    begin_time = time.time()

    if os.path.isfile(args.input):
        """ Upscale single video file """
        Avalon.info('Upscaling single video file: {}'.format(args.input))
        upscaler = Upscaler(input_video=args.input, output_video=args.output, method=args.method, waifu2x_path=waifu2x_path, ffmpeg_path=ffmpeg_path, ffmpeg_arguments=ffmpeg_arguments, ffmpeg_hwaccel=ffmpeg_hwaccel, output_width=args.width, output_height=args.height, factor=args.factor, model_type=args.model_type, threads=args.threads)
        upscaler.run()
    elif os.path.isdir(args.input):
        """ Upscale videos in a folder/directory """
        Avalon.info('Upscaling videos in folder: {}'.format(args.input))
        for input_video in [f for f in os.listdir(args.input) if os.path.isfile(os.path.join(args.input, f))]:
            output_video = '{}\\{}'.format(args.output, input_video)
            upscaler = Upscaler(input_video=os.path.join(args.input, input_video), output_video=output_video, method=args.method, waifu2x_path=waifu2x_path, ffmpeg_path=ffmpeg_path, ffmpeg_arguments=ffmpeg_arguments, ffmpeg_hwaccel=ffmpeg_hwaccel, output_width=args.width, output_height=args.height, factor=args.factor, model_type=args.model_type, threads=args.threads)
            upscaler.run()
    else:
        Avalon.error('Input path is neither a file nor a folder/directory')
        raise FileNotFoundError('{} is neither file nor folder/directory'.format(args.input))

    Avalon.info('Program completed, taking {} seconds'.format(round((time.time() - begin_time), 5)))
except Exception:
    Avalon.error('An exception occurred')
    traceback.print_exc()

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""

__      __  _       _                  ___   __   __
\ \    / / (_)     | |                |__ \  \ \ / /
 \ \  / /   _    __| |   ___    ___      ) |  \ V /
  \ \/ /   | |  / _` |  / _ \  / _ \    / /    > <
   \  /    | | | (_| | |  __/ | (_) |  / /_   / . \
    \/     |_|  \__,_|  \___|  \___/  |____| /_/ \_\


Name: Video2x Controller
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: December 10, 2018

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
import argparse
import inspect
import json
import os
import time
import traceback

VERSION = '2.1.7'

EXEC_PATH = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
FRAMES = '{}\\frames'.format(EXEC_PATH)  # Folder containing extracted frames
UPSCALED = '{}\\upscaled'.format(EXEC_PATH)  # Folder containing enlarges frames


def process_arguments():
    """Processes CLI arguments

    This function parses all arguments
    This allows users to customize options
    for the output video.
    """
    parser = argparse.ArgumentParser()

    # Video options
    basic_options = parser.add_argument_group('Basic Options')
    basic_options.add_argument('-i', '--input', help='Specify source video file', action='store', default=False, required=True)
    basic_options.add_argument('-o', '--output', help='Specify output video file', action='store', default=False, required=True)
    basic_options.add_argument('-y', '--model_type', help='Specify model to use', action='store', default='anime_style_art_rgb')
    basic_options.add_argument('-t', '--threads', help='Specify number of threads to use for upscaling', action='store', type=int, default=5)
    basic_options.add_argument('-c', '--config', help='Manually specify config file', action='store', default='video2x.json')

    # Scaling options
    scaling_options = parser.add_argument_group('Scaling Options', required=True)
    scaling_options.add_argument('--width', help='Output video width', action='store', type=int, default=False)
    scaling_options.add_argument('--height', help='Output video height', action='store', type=int, default=False)
    scaling_options.add_argument('-f', '--factor', help='Factor to upscale the videos by', action='store', type=int, default=False)

    # Render drivers, at least one option must be specified
    driver_group = parser.add_argument_group('Render Drivers', required=True)
    driver_group.add_argument('--cpu', help='Use CPU for enlarging', action='store_true', default=False)
    driver_group.add_argument('--gpu', help='Use GPU for enlarging', action='store_true', default=False)
    driver_group.add_argument('--cudnn', help='Use CUDNN for enlarging', action='store_true', default=False)

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
    spaces = ((44 - len("Version " + VERSION)) // 2) * " "
    print('{}\n{}    Version {}\n{}'.format(Avalon.FM.BD, spaces, VERSION, Avalon.FM.RST))


def read_config(config_file):
    """ Reads configuration file

    Returns a dictionary read by json.
    """
    with open(config_file, 'r') as raw_config:
        config = json.load(raw_config)
        return config


# /////////////////// Execution /////////////////// #

# This is not a library
if __name__ != '__main__':
    Avalon.error('This file cannot be imported')
    exit(1)

print_logo()

# Process cli arguments
args = process_arguments()

# Read configurations from JSON
config = read_config()
waifu2x_path = config['waifu2x_path']
ffmpeg_path = config['ffmpeg_path']
ffmpeg_arguments = config['ffmpeg_arguments']
ffmpeg_hwaccel = config['ffmpeg_hwaccel']

# Parse arguments for waifu2x
if args.cpu:
    method = 'cpu'
elif args.gpu:
    method = 'gpu'
    ffmpeg_arguments.append('-hwaccel {}'.format(ffmpeg_hwaccel))
elif args.cudnn:
    method = 'cudnn'
    ffmpeg_arguments.append('-hwaccel {}'.format(ffmpeg_hwaccel))

# Start execution
try:
    # Start timer
    begin_time = time.time()

    # Initialize and run upscaling
    upscaler = Upscaler(input_video=args.video, output_video=args.output, method=method, waifu2x_path=waifu2x_path, ffmpeg_path=ffmpeg_path, ffmpeg_arguments=ffmpeg_arguments, ffmpeg_hwaccel=ffmpeg_hwaccel, output_width=args.width, output_height=args.height, factor=args.factor, model_type=args.model_type, threads=args.threads)
    upscaler.run()

    Avalon.info('Program completed, taking {} seconds'.format(round((time.time() - begin_time), 5)))
except Exception:
    Avalon.error('An exception occurred')
    traceback.print_exc()

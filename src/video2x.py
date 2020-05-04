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
Creator: K4YT3X
Date Created: Feb 24, 2018
Last Modified: May 4, 2020

Editor: BrianPetkovsek
Last Modified: June 17, 2019

Editor: SAT3LL
Last Modified: June 25, 2019

Editor: 28598519a
Last Modified: March 23, 2020

Licensed under the GNU General Public License Version 3 (GNU GPL v3),
    available at: https://www.gnu.org/licenses/gpl-3.0.txt

(C) 2018 - 2020 K4YT3X

Video2X is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Video2X is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.

Description: Video2X is an automation software based on waifu2x image
enlarging engine. It extracts frames from a video, enlarge it by a
number of times without losing any details or quality, keeping lines
smooth and edges sharp.
"""

# local imports
from upscaler import AVAILABLE_DRIVERS
from upscaler import Upscaler

# built-in imports
import argparse
import contextlib
import importlib
import pathlib
import re
import shutil
import sys
import tempfile
import time
import traceback
import yaml

# third-party imports
from avalon_framework import Avalon


VERSION = '4.0.0'

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


def parse_arguments():
    """ parse CLI arguments
    """
    parser = argparse.ArgumentParser(prog='video2x', formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    # video options
    general_options = parser.add_argument_group('General Options')
    general_options.add_argument('-i', '--input', type=pathlib.Path, help='source video file/directory')
    general_options.add_argument('-o', '--output', type=pathlib.Path, help='output video file/directory')
    general_options.add_argument('-c', '--config', type=pathlib.Path, help='video2x config file location', action='store',
                                 default=pathlib.Path(__file__).parent.absolute() / 'video2x.yaml')
    general_options.add_argument('-d', '--driver', help='upscaling driver', choices=AVAILABLE_DRIVERS, default='waifu2x_caffe')
    general_options.add_argument('-p', '--processes', help='number of processes to use for upscaling', action='store', type=int, default=1)
    general_options.add_argument('-v', '--version', help='display version, lawful information and exit', action='store_true')

    # scaling options
    scaling_options = parser.add_argument_group('Scaling Options')
    scaling_options.add_argument('--width', help='output video width', action='store', type=int)
    scaling_options.add_argument('--height', help='output video height', action='store', type=int)
    scaling_options.add_argument('-r', '--ratio', help='scaling ratio', action='store', type=float)

    # if no driver arguments are specified
    if '--' not in sys.argv:
        video2x_args = parser.parse_args()
        return video2x_args, None

    # if driver arguments are specified
    else:
        video2x_args = parser.parse_args(sys.argv[1:sys.argv.index('--')])
        wrapper = getattr(importlib.import_module(f'wrappers.{video2x_args.driver}'), 'WrapperMain')
        driver_args = wrapper.parse_arguments(sys.argv[sys.argv.index('--') + 1:])
        return video2x_args, driver_args


def print_logo():
    """print video2x logo"""
    print(LOGO)
    print(f'\n{"Video2X Video Enlarger".rjust(40, " ")}')
    print(f'\n{Avalon.FM.BD}{f"Version {VERSION}".rjust(36, " ")}{Avalon.FM.RST}\n')


def read_config(config_file: pathlib.Path) -> dict:
    """ read video2x configurations from config file

    Arguments:
        config_file {pathlib.Path} -- video2x configuration file pathlib.Path

    Returns:
        dict -- dictionary of video2x configuration
    """

    with open(config_file, 'r') as config:
        return yaml.load(config, Loader=yaml.FullLoader)


# /////////////////// Execution /////////////////// #

# this is not a library
if __name__ != '__main__':
    Avalon.error('This file cannot be imported')
    raise ImportError(f'{__file__} cannot be imported')

# print video2x logo
print_logo()

# parse command line arguments
video2x_args, driver_args = parse_arguments()

# display version and lawful informaition
if video2x_args.version:
    print(LEGAL_INFO)
    sys.exit(0)

# read configurations from configuration file
config = read_config(video2x_args.config)

# load waifu2x configuration
driver_settings = config[video2x_args.driver]

# overwrite driver_settings with driver_args
if driver_args is not None:
    driver_args_dict = vars(driver_args)
    for key in driver_args_dict:
        if driver_args_dict[key] is not None:
            driver_settings[key] = driver_args_dict[key]

# check if driver path exists
if not pathlib.Path(driver_settings['path']).exists():
    if not pathlib.Path(f'{driver_settings["path"]}.exe').exists():
        Avalon.error('Specified driver executable directory doesn\'t exist')
        Avalon.error('Please check the configuration file settings')
        raise FileNotFoundError(driver_settings['path'])

# read FFmpeg configuration
ffmpeg_settings = config['ffmpeg']

# load video2x settings
image_format = config['video2x']['image_format'].lower()
preserve_frames = config['video2x']['preserve_frames']

# load cache directory
if config['video2x']['video2x_cache_directory'] is not None:
    video2x_cache_directory = pathlib.Path(config['video2x']['video2x_cache_directory'])
else:
    video2x_cache_directory = pathlib.Path(tempfile.gettempdir()) / 'video2x'

if video2x_cache_directory.exists() and not video2x_cache_directory.is_dir():
    Avalon.error('Specified cache directory is a file/link')
    raise FileExistsError('Specified cache directory is a file/link')

# if cache directory doesn't exist
# ask the user if it should be created
elif not video2x_cache_directory.exists():
    try:
        Avalon.debug_info(f'Creating cache directory {video2x_cache_directory}')
        video2x_cache_directory.mkdir(parents=True, exist_ok=True)
    # there can be a number of exceptions here
    # PermissionError, FileExistsError, etc.
    # therefore, we put a catch-them-all here
    except Exception as exception:
        Avalon.error(f'Unable to create {video2x_cache_directory}')
        raise exception


# start execution
try:
    # start timer
    begin_time = time.time()

    # if input specified is a single file
    if video2x_args.input.is_file():

        # upscale single video file
        Avalon.info(f'Upscaling single video file: {video2x_args.input}')

        # check for input output format mismatch
        if video2x_args.output.is_dir():
            Avalon.error('Input and output path type mismatch')
            Avalon.error('Input is single file but output is directory')
            raise Exception('input output path type mismatch')
        if not re.search(r'.*\..*$', str(video2x_args.output)):
            Avalon.error('No suffix found in output file path')
            Avalon.error('Suffix must be specified for FFmpeg')
            raise Exception('No suffix specified')

        upscaler = Upscaler(input_video=video2x_args.input,
                            output_video=video2x_args.output,
                            driver_settings=driver_settings,
                            ffmpeg_settings=ffmpeg_settings)

        # set optional options
        upscaler.driver = video2x_args.driver
        upscaler.scale_width = video2x_args.width
        upscaler.scale_height = video2x_args.height
        upscaler.scale_ratio = video2x_args.ratio
        upscaler.processes = video2x_args.processes
        upscaler.video2x_cache_directory = video2x_cache_directory
        upscaler.image_format = image_format
        upscaler.preserve_frames = preserve_frames

        # run upscaler
        upscaler.run()

    # if input specified is a directory
    elif video2x_args.input.is_dir():
        # upscale videos in a directory
        Avalon.info(f'Upscaling videos in directory: {video2x_args.input}')

        # make output directory if it doesn't exist
        video2x_args.output.mkdir(parents=True, exist_ok=True)

        for input_video in [f for f in video2x_args.input.iterdir() if f.is_file()]:
            output_video = video2x_args.output / input_video.name
            upscaler = Upscaler(input_video=input_video,
                                output_video=output_video,
                                driver_settings=driver_settings,
                                ffmpeg_settings=ffmpeg_settings)

            # set optional options
            upscaler.driver = video2x_args.driver
            upscaler.scale_width = video2x_args.width
            upscaler.scale_height = video2x_args.height
            upscaler.scale_ratio = video2x_args.ratio
            upscaler.processes = video2x_args.processes
            upscaler.video2x_cache_directory = video2x_cache_directory
            upscaler.image_format = image_format
            upscaler.preserve_frames = preserve_frames

            # run upscaler
            upscaler.run()
    else:
        Avalon.error('Input path is neither a file nor a directory')
        raise FileNotFoundError(f'{video2x_args.input} is neither file nor directory')

    Avalon.info(f'Program completed, taking {round((time.time() - begin_time), 5)} seconds')

except Exception:
    Avalon.error('An exception has occurred')
    traceback.print_exc()

    # try cleaning up temp directories
    with contextlib.suppress(Exception):
        upscaler.cleanup_temp_directories()

finally:
    # remove Video2X cache directory
    with contextlib.suppress(FileNotFoundError):
        if not preserve_frames:
            shutil.rmtree(video2x_cache_directory)

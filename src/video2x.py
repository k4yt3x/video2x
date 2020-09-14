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
Last Modified: September 13, 2020

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
from bilogger import BiLogger
from upscaler import AVAILABLE_DRIVERS
from upscaler import UPSCALER_VERSION
from upscaler import Upscaler

# built-in imports
import argparse
import gettext
import importlib
import locale
import os
import pathlib
import sys
import tempfile
import time
import traceback
import yaml

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

CLI_VERSION = '4.3.1'

LEGAL_INFO = _('''Video2X CLI Version: {}
Upscaler Version: {}
Author: K4YT3X
License: GNU GPL v3
Github Page: https://github.com/k4yt3x/video2x
Contact: k4yt3x@k4yt3x.com''').format(CLI_VERSION, UPSCALER_VERSION)

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
    parser = argparse.ArgumentParser(prog='video2x', formatter_class=argparse.ArgumentDefaultsHelpFormatter, add_help=False)

    # video options
    video2x_options = parser.add_argument_group(_('Video2X Options'))
    video2x_options.add_argument('--help', action='help', help=_('show this help message and exit'))

    # if help is in arguments list
    # do not require input and output path to be specified
    require_input_output = True
    if '-h' in sys.argv or '--help' in sys.argv:
        require_input_output = False
    video2x_options.add_argument('-i', '--input', type=pathlib.Path, help=_('source video file/directory'), required=require_input_output)
    video2x_options.add_argument('-o', '--output', type=pathlib.Path, help=_('output video file/directory'), required=require_input_output)

    video2x_options.add_argument('-c', '--config', type=pathlib.Path, help=_('Video2X config file path'), action='store',
                                 default=pathlib.Path(__file__).parent.absolute() / 'video2x.yaml')
    video2x_options.add_argument('--log', type=pathlib.Path, help=_('log file path'))
    video2x_options.add_argument('-v', '--version', help=_('display version, lawful information and exit'), action='store_true')

    # scaling options
    upscaling_options = parser.add_argument_group(_('Upscaling Options'))
    upscaling_options.add_argument('-r', '--ratio', help=_('scaling ratio'), action='store', type=float)
    upscaling_options.add_argument('-w', '--width', help=_('output width'), action='store', type=float)
    upscaling_options.add_argument('-h', '--height', help=_('output height'), action='store', type=float)
    upscaling_options.add_argument('-d', '--driver', help=_('upscaling driver'), choices=AVAILABLE_DRIVERS, default='waifu2x_ncnn_vulkan')
    upscaling_options.add_argument('-p', '--processes', help=_('number of processes to use for upscaling'), action='store', type=int, default=1)
    upscaling_options.add_argument('--preserve_frames', help=_('preserve extracted and upscaled frames'), action='store_true')

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
    print(f'\n{Avalon.FM.BD}{f"Version {CLI_VERSION}".rjust(36, " ")}{Avalon.FM.RST}\n')


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
    Avalon.error(_('This file cannot be imported'))
    raise ImportError(f'{__file__} cannot be imported')

# print video2x logo
print_logo()

# parse command line arguments
video2x_args, driver_args = parse_arguments()

# display version and lawful informaition
if video2x_args.version:
    print(LEGAL_INFO)
    sys.exit(0)

# additional checks on upscaling arguments
if video2x_args.ratio is not None and (video2x_args.width is not None or video2x_args.height is not None):
    Avalon.error(_('Specify either scaling ratio or scaling resolution, not both'))
    sys.exit(1)

# redirect output to both terminal and log file
if video2x_args.log is not None:
    log_file = video2x_args.log.open(mode='a+', encoding='utf-8')
else:
    log_file = tempfile.TemporaryFile(mode='a+', suffix='.log', prefix='video2x_', encoding='utf-8')

original_stdout = sys.stdout
original_stderr = sys.stderr
sys.stdout = BiLogger(sys.stdout, log_file)
sys.stderr = BiLogger(sys.stderr, log_file)

# read configurations from configuration file
config = read_config(video2x_args.config)

# load waifu2x configuration
driver_settings = config[video2x_args.driver]
driver_settings['path'] = os.path.expandvars(driver_settings['path'])

# read FFmpeg configuration
ffmpeg_settings = config['ffmpeg']
ffmpeg_settings['ffmpeg_path'] = os.path.expandvars(ffmpeg_settings['ffmpeg_path'])

# read Gifski configuration
gifski_settings = config['gifski']
gifski_settings['gifski_path'] = os.path.expandvars(gifski_settings['gifski_path'])

# load video2x settings
extracted_frame_format = config['video2x']['extracted_frame_format'].lower()
output_file_name_format_string = config['video2x']['output_file_name_format_string']
image_output_extension = config['video2x']['image_output_extension']
video_output_extension = config['video2x']['video_output_extension']
preserve_frames = config['video2x']['preserve_frames']

# if preserve frames specified in command line
# overwrite config file options
if video2x_args.preserve_frames is True:
    preserve_frames = True

# if cache directory not specified
# use default path: %TEMP%\video2x
if config['video2x']['video2x_cache_directory'] is None:
    video2x_cache_directory = (pathlib.Path(tempfile.gettempdir()) / 'video2x')
else:
    video2x_cache_directory = pathlib.Path(config['video2x']['video2x_cache_directory'])

# overwrite driver_settings with driver_args
if driver_args is not None:
    driver_args_dict = vars(driver_args)
    for key in driver_args_dict:
        if driver_args_dict[key] is not None:
            driver_settings[key] = driver_args_dict[key]

# start execution
try:
    # start timer
    begin_time = time.time()

    # initialize upscaler object
    upscaler = Upscaler(
        # required parameters
        input_path=video2x_args.input,
        output_path=video2x_args.output,
        driver_settings=driver_settings,
        ffmpeg_settings=ffmpeg_settings,
        gifski_settings=gifski_settings,

        # optional parameters
        driver=video2x_args.driver,
        scale_ratio=video2x_args.ratio,
        scale_width=video2x_args.width,
        scale_height=video2x_args.height,
        processes=video2x_args.processes,
        video2x_cache_directory=video2x_cache_directory,
        extracted_frame_format=extracted_frame_format,
        output_file_name_format_string=output_file_name_format_string,
        image_output_extension=image_output_extension,
        video_output_extension=video_output_extension,
        preserve_frames=preserve_frames
    )

    # run upscaler
    upscaler.run()

    Avalon.info(_('Program completed, taking {} seconds').format(round((time.time() - begin_time), 5)))

except Exception:

    Avalon.error(_('An exception has occurred'))
    traceback.print_exc()

    if video2x_args.log is not None:
        log_file_path = video2x_args.log.absolute()

    # if log file path is not specified, create temporary file as permanent log file
    # tempfile.TempFile does not have a name attribute and is not guaranteed to have
    # a visible name on the file system
    else:
        log_file_path = tempfile.mkstemp(suffix='.log', prefix='video2x_')[1]
        with open(log_file_path, 'w', encoding='utf-8') as permanent_log_file:
            log_file.seek(0)
            permanent_log_file.write(log_file.read())

    Avalon.error(_('The error log file can be found at: {}').format(log_file_path))

finally:
    sys.stdout = original_stdout
    sys.stderr = original_stderr
    log_file.close()

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
Last Modified: May 19, 2018

Licensed under the GNU General Public License Version 3 (GNU GPL v3),
    available at: https://www.gnu.org/licenses/gpl-3.0.txt

(C) 2018 K4YT3X

Description: Video2X is an automation software based on
waifu2x image enlarging engine. It extracts frames from a
video, enlarge it by a number of times without losing any
details or quality, keeping lines smooth and edges sharp.
"""
from ffmpeg import FFMPEG
from fractions import Fraction
from tqdm import tqdm
from waifu2x import WAIFU2X
import argparse
import avalon_framework as avalon
import inspect
import json
import os
import shutil
import subprocess
import traceback

VERSION = '2.0.4'

EXEC_PATH = os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
FRAMES = '{}\\frames'.format(EXEC_PATH)  # Folder containing extracted frames
UPSCALED = '{}\\upscaled'.format(EXEC_PATH)  # Folder containing enlarges frames

# FFMPEG bin folder. Mind that '/' at the end
FFMPEG_PATH = 'C:/Program Files (x86)/ffmpeg/bin/'
# waifu2x executable path. Mind all the forward slashes
WAIFU2X_PATH = '\"C:/Program Files (x86)/waifu2x-caffe/waifu2x-caffe-cui.exe\"'


def process_arguments():
    """Processes CLI arguments

    This function parses all arguments
    This allows users to customize options
    for the output video.
    """
    parser = argparse.ArgumentParser()
    control_group = parser.add_argument_group('Controls')
    control_group.add_argument('--width', help='Output video width', action='store', type=int, default=False)
    control_group.add_argument('--height', help='Output video height', action='store', type=int, default=False)
    control_group.add_argument('-v', '--video', help='Specify source video file', action='store', default=False)
    control_group.add_argument('-o', '--output', help='Specify output file', action='store', default=False)
    control_group.add_argument('-y', '--model_type', help='Specify model to use', action='store', default='anime_style_art_rgb')
    control_group.add_argument('--cpu', help='Use CPU for enlarging', action='store_true', default=False)
    control_group.add_argument('--gpu', help='Use GPU for enlarging', action='store_true', default=False)
    control_group.add_argument('--cudnn', help='Use CUDNN for enlarging', action='store_true', default=False)
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
    print(avalon.FM.BD + "\n" + spaces +
          '    Version ' + VERSION + '\n' + avalon.FM.RST)


def get_vid_info():
    """Gets original video information

    Retrieves original video information using
    ffprobe, then export it into json file.
    Finally it reads, parses the json file and
    returns a dictionary

    Returns:
        dictionary -- original video information
    """
    json_str = subprocess.check_output(
        '{} -v quiet -print_format json -show_format -show_streams \"{}\"'.format('\"' + FFMPEG_PATH + 'ffprobe.exe\"', args.video))
    return json.loads(json_str.decode('utf-8'))


def check_model_type(args):
    """
    Check if the model demanded from cli
    argument is legal.
    """
    models_available = ['upconv_7_anime_style_art_rgb', 'upconv_7_photo',
                        'anime_style_art_rgb', 'photo', 'anime_style_art_y']
    if args.model_type not in models_available:
        avalon.error('Specified model type not found!')
        avalon.info('Available models:')
        for model in models_available:
            print(model)
        exit(1)


def video2x():
    """Main controller for Video2X

    This function controls the flow of video conversion
    and handles all necessary functions.
    """

    check_model_type(args)

    if args.cpu:
        method = 'cpu'
    elif args.gpu:
        method = 'gpu'
    elif args.cudnn:
        method = 'cudnn'

    fm = FFMPEG('\"' + FFMPEG_PATH + 'ffmpeg.exe\"', args.output)
    w2 = WAIFU2X(WAIFU2X_PATH, method, args.model_type)

    # Clear and create directories
    if os.path.isdir(FRAMES):
        shutil.rmtree(FRAMES)
    if os.path.isdir(UPSCALED):
        shutil.rmtree(UPSCALED)
    os.mkdir(FRAMES)
    os.mkdir(UPSCALED)

    # Extract frames from video
    fm.extract_frames(args.video, FRAMES)

    info = get_vid_info()
    # Analyze original video with ffprobe and retrieve framerate
    # width, height = info['streams'][0]['width'], info['streams'][0]['height']
    framerate = float(Fraction(info['streams'][0]['avg_frame_rate']))
    avalon.info('Framerate: {}'.format(framerate))

    # Upscale images one by one using waifu2x
    avalon.info('Starting to upscale extracted images')
    w2.upscale(FRAMES, UPSCALED, args.width, args.height)
    avalon.info('Conversion complete')

    # Frames to Video
    avalon.info('Converting extracted frames into video')
    fm.convert_video(framerate, '{}x{}'.format(args.width, args.height), UPSCALED)

    # Extract and press audio in
    avalon.info('Stripping audio track from original video')
    fm.extract_audio(args.video, UPSCALED)
    avalon.info('Inserting audio track into new video')
    fm.insert_audio_track(UPSCALED)


# /////////////////// Execution /////////////////// #

args = process_arguments()
# Convert paths to absolute paths
args.video = os.path.abspath(args.video)
args.output = os.path.abspath(args.output)
print_logo()


# Check if FFMPEG and waifu2x are present
if not os.path.isdir(FFMPEG_PATH):
    avalon.error('FFMPEG binaries not found')
    avalon.error('Please specify FFMPEG binaries location in source code')
    print('Current value: {}\n'.format(FFMPEG_PATH))
    raise FileNotFoundError('FFMPEG binaries not found')
if not os.path.isfile(WAIFU2X_PATH.strip('\"')):
    avalon.error('Waifu2x CUI executable not found')
    avalon.error('Please specify Waifu2x CUI location in source code')
    print('Current value: {}\n'.format(WAIFU2X_PATH))
    raise FileNotFoundError('Waifu2x CUI executable not found')


# Check if arguments are valid / all necessary argument
# values are specified
if not args.video:
    avalon.error('You need to specify the video to process')
    exit(1)
elif not args.width or not args.height:
    avalon.error('You must specify output video width and height')
    exit(1)
elif not args.output:
    avalon.error('You need to specify the output video name')
    exit(1)
elif not args.cpu and not args.gpu and not args.cudnn:
    avalon.error('You need to specify the enlarging processing unit')
    exit(1)

if __name__ == '__main__':
    try:
        video2x()
    except Exception as e:
        avalon.error('An exception occurred')
        traceback.print_exc()

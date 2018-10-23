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
Last Modified: October 23, 2018

Licensed under the GNU General Public License Version 3 (GNU GPL v3),
    available at: https://www.gnu.org/licenses/gpl-3.0.txt

(C) 2018 K4YT3X

Description: Video2X is an automation software based on
waifu2x image enlarging engine. It extracts frames from a
video, enlarge it by a number of times without losing any
details or quality, keeping lines smooth and edges sharp.
"""
from avalon_framework import Avalon
from ffmpeg import Ffmpeg
from fractions import Fraction
from waifu2x import Waifu2x
import argparse
import inspect
import json
import os
import shutil
import subprocess
import threading
import time
import traceback

VERSION = '2.1.2'

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
    options_group = parser.add_argument_group('Options')
    options_group.add_argument('--width', help='Output video width', action='store', type=int, default=False)
    options_group.add_argument('--height', help='Output video height', action='store', type=int, default=False)
    options_group.add_argument('-v', '--video', help='Specify source video file', action='store', default=False)
    options_group.add_argument('-o', '--output', help='Specify output file', action='store', default=False)
    options_group.add_argument('-y', '--model_type', help='Specify model to use', action='store', default='anime_style_art_rgb')
    options_group.add_argument('-t', '--threads', help='Specify model to use', action='store', type=int, default=5)
    options_group.add_argument('-c', '--config', help='Manually specify config file', action='store', default='video2x.json')

    # Render drivers, at least one option must be specified
    driver_group = parser.add_argument_group('Render Drivers')
    driver_group.add_argument('--cpu', help='Use CPU for enlarging', action='store_true', default=False)
    driver_group.add_argument('--gpu', help='Use GPU for enlarging', action='store_true', default=False)
    driver_group.add_argument('--cudnn', help='Use CUDNN for enlarging', action='store_true', default=False)
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


def read_config():
    """ Reads configuration file

    Returns a dictionary read by json.
    """
    with open(args.config, 'r') as raw_config:
        config = json.load(raw_config)
    return config


def get_video_info():
    """Gets original video information

    Retrieves original video information using
    ffprobe, then export it into json file.
    Finally it reads, parses the json file and
    returns a dictionary

    Returns:
        dictionary -- original video information
    """
    json_str = subprocess.check_output(
        '\"{}ffprobe.exe\" -v quiet -print_format json -show_format -show_streams \"{}\"'.format(ffmpeg_path, args.video))
    return json.loads(json_str.decode('utf-8'))


def check_model_type(args):
    """
    Check if the model demanded from cli
    argument is legal.
    """
    models_available = ['upconv_7_anime_style_art_rgb', 'upconv_7_photo',
                        'anime_style_art_rgb', 'photo', 'anime_style_art_y']
    if args.model_type not in models_available:
        Avalon.error('Specified model type not found!')
        Avalon.info('Available models:')
        for model in models_available:
            print(model)
        exit(1)


def upscale_frames(w2):
    """ Upscale video frames with waifu2x-caffe

    This function upscales all the frames extracted
    by ffmpeg using the waifu2x-caffe binary.

    Arguments:
        w2 {Waifu2x Object} -- initialized waifu2x object
    """

    # Create a container for all upscaler threads
    upscaler_threads = []

    # List all images in the extracted frames
    frames = [os.path.join(FRAMES, f) for f in os.listdir(FRAMES) if os.path.isfile(os.path.join(FRAMES, f))]

    # If we have less images than threads,
    # create only the threads necessary
    if len(frames) < args.threads:
        args.threads = len(frames)

    # Move an equal amount of images into separate
    # folders for each thread
    images_per_thread = len(frames) // args.threads
    for thread_id in range(args.threads):
        thread_folder = '{}\\{}'.format(FRAMES, str(thread_id))

        # Delete old folders and create new folders
        if os.path.isdir(thread_folder):
            shutil.rmtree(thread_folder)
        os.mkdir(thread_folder)

        # Begin moving images into corresponding folders
        for _ in range(images_per_thread):
            try:
                shutil.move(frames.pop(0), thread_folder)
            except IndexError:
                pass

        # Create thread
        thread = threading.Thread(target=w2.upscale, args=(thread_folder, UPSCALED, args.width, args.height))
        thread.name = str(thread_id)

        # Add threads into the pool
        upscaler_threads.append(thread)

    # Start all threads
    for thread in upscaler_threads:
        thread.start()

    # Wait for threads to finish
    for thread in upscaler_threads:
        thread.join()


def video2x():
    """Main controller for Video2X

    This function controls the flow of video conversion
    and handles all necessary functions.
    """

    check_model_type(args)

    # Parse arguments for waifu2x
    if args.cpu:
        method = 'cpu'
    elif args.gpu:
        method = 'gpu'
    elif args.cudnn:
        method = 'cudnn'

    # Initialize objects for ffmpeg and waifu2x-caffe
    fm = Ffmpeg(ffmpeg_path, args.output, ffmpeg_arguments)
    w2 = Waifu2x(waifu2x_path, method, args.model_type)

    # Clear and create directories
    if os.path.isdir(FRAMES):
        shutil.rmtree(FRAMES)
    if os.path.isdir(UPSCALED):
        shutil.rmtree(UPSCALED)
    os.mkdir(FRAMES)
    os.mkdir(UPSCALED)

    # Extract frames from video
    fm.extract_frames(args.video, FRAMES)

    Avalon.info('Reading video information')
    info = get_video_info()
    # Analyze original video with ffprobe and retrieve framerate
    # width, height = info['streams'][0]['width'], info['streams'][0]['height']

    # Find index of video stream
    video_stream_index = None
    for stream in info['streams']:
        if stream['codec_type'] == 'video':
            video_stream_index = stream['index']
            break

    # Exit if no video stream found
    if video_stream_index is None:
        Avalon.error('Aborting: No video stream found')

    # Get average frame rate of video stream
    framerate = float(Fraction(info['streams'][video_stream_index]['avg_frame_rate']))
    Avalon.info('Framerate: {}'.format(framerate))

    # Upscale images one by one using waifu2x
    Avalon.info('Starting to upscale extracted images')
    upscale_frames(w2)
    Avalon.info('Upscaling completed')

    # Frames to Video
    Avalon.info('Converting extracted frames into video')
    fm.convert_video(framerate, '{}x{}'.format(args.width, args.height), UPSCALED)
    Avalon.info('Conversion completed')

    # Extract and press audio in
    Avalon.info('Stripping audio track from original video')
    fm.extract_audio(args.video, UPSCALED)
    Avalon.info('Inserting audio track into new video')
    fm.insert_audio_track(UPSCALED)


# /////////////////// Execution /////////////////// #

# This is not a library
if __name__ != '__main__':
    Avalon.error('This file cannot be imported')
    exit(1)

# Process cli arguments
args = process_arguments()

# Print video2x banner
print_logo()

# Check if arguments are valid / all necessary argument
# values are specified
if not args.video:
    Avalon.error('You need to specify the video to process')
    exit(1)
elif not args.width or not args.height:
    Avalon.error('You must specify output video width and height')
    exit(1)
elif not args.output:
    Avalon.error('You need to specify the output video name')
    exit(1)
elif not args.cpu and not args.gpu and not args.cudnn:
    Avalon.error('You need to specify the enlarging processing unit')
    exit(1)

# Convert paths to absolute paths
args.video = os.path.abspath(args.video)
args.output = os.path.abspath(args.output)

# Read configurations from config file
config = read_config()
waifu2x_path = config['waifu2x_path']
ffmpeg_path = config['ffmpeg_path']
ffmpeg_arguments = config['ffmpeg_arguments']

# Add a forward slash to directory if not present
# otherwise there will be a format error
if ffmpeg_path[-1] != '/' and ffmpeg_path[-1] != '\\':
    ffmpeg_path = '{}/'.format(ffmpeg_path)

# Check if FFMPEG and waifu2x are present
if not os.path.isdir(ffmpeg_path):
    Avalon.error('FFMPEG binaries not found')
    Avalon.error('Please specify FFMPEG binaries location in config file')
    Avalon.error('Current value: {}'.format(ffmpeg_path))
    raise FileNotFoundError(ffmpeg_path)
if not os.path.isfile(waifu2x_path):
    Avalon.error('Waifu2x CUI executable not found')
    Avalon.error('Please specify Waifu2x CUI location in config file')
    Avalon.error('Current value: {}'.format(waifu2x_path))
    raise FileNotFoundError(waifu2x_path)

# Start execution
try:
    begin_time = time.time()
    video2x()
    Avalon.info('Program completed, taking {} seconds'.format(round((time.time() - begin_time), 5)))
except Exception:
    Avalon.error('An exception occurred')
    traceback.print_exc()

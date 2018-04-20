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
Last Modified: Feb 25, 2018

Licensed under the GNU General Public License Version 3 (GNU GPL v3),
    available at: https://www.gnu.org/licenses/gpl-3.0.txt

(C) 2016 - 2017 K4YT3X

Description: Video2X is an automation software based on
waifu2x image enlarging engine. It extracts frames from a
video, enlarge it by a number of times without losing any
details or quality, keeping lines smooth and edges sharp.

Version 1.1 alpha
"""

from ffmpeg import FFMPEG
from fractions import Fraction
from waifu2x import WAIFU2X
import avalon_framework as avalon
import argparse
import json
import os
import traceback

# FFMPEG bin folder. Mind that "/" at the end
FFMPEG_PATH = "C:/Program Files (x86)/ffmpeg/bin/"
# waifu2x executable path. Mind all the forward slashes
WAIFU2X_PATH = "\"C:/Program Files (x86)/waifu2x-caffe/waifu2x-caffe-cui.exe\""

FOLDERIN = "frames"  # Folder containing extracted frames
FOLDEROUT = "upscaled"  # Folder contaning enlarges frames


def processArguments():
    """Processes CLI arguments

    This function parses all arguments
    This allows users to customize options
    for the output video.
    """
    global args
    parser = argparse.ArgumentParser()
    control_group = parser.add_argument_group('Controls')
    control_group.add_argument("-f", "--factor", help="Factor to enlarge video by", action="store", default=2)
    control_group.add_argument("-v", "--video", help="Specify video file", action="store", default=False)
    control_group.add_argument("-o", "--output", help="Specify output file", action="store", default=False)
    control_group.add_argument("-y", "--model_type", help="Specify model to use", action="store", default="anime_style_art_rgb")
    control_group.add_argument("--cpu", help="Use CPU for enlarging", action="store_true", default=False)
    control_group.add_argument("--gpu", help="Use GPU for enlarging", action="store_true", default=False)
    control_group.add_argument("--cudnn", help="Use CUDNN for enlarging", action="store_true", default=False)
    args = parser.parse_args()


def get_vid_info():
    """Gets original video information

    Retrieves original video information using
    ffprobe, then export it into json file.
    Finally it reads, parses the json file and
    returns a dictionary

    Returns:
        dictionary -- original video information
    """
    os.system("{} -v quiet -print_format json -show_format -show_streams {} > info.json".format("\"" + FFMPEG_PATH + "ffprobe.exe\"", args.video))
    json_file = open('info.json', 'r')
    json_str = json_file.read()
    print(json.loads(json_str))
    return json.loads(json_str)


def check_model_type(args):
    models_available = ["upconv_7_anime_style_art_rgb", "upconv_7_photo",
                        "anime_style_art_rgb", "photo", "anime_style_art_y"]
    if args.model_type not in models_available:
        avalon.error('Specified model type not found!')
        avalon.info("Available models:")
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
        method = "cpu"
    elif args.gpu:
        method = "gpu"
    elif args.cudnn:
        method = "cudnn"

    fm = FFMPEG("\"" + FFMPEG_PATH + "ffmpeg.exe\"", args.output)
    w2 = WAIFU2X(WAIFU2X_PATH, method)

    # Extract Frames
    if not os.path.isdir(FOLDERIN):
        os.mkdir(FOLDERIN)
    fm.extract_frames(args.video, FOLDERIN)

    info = get_vid_info()
    # Framerate is read as fraction from the json dictionary
    width, height, framerate = info["streams"][0]["width"], info["streams"][0]["height"], float(Fraction(info["streams"][0]["avg_frame_rate"]))
    print("Framerate: ", framerate)
    final_resolution = str(width * int(args.factor)) + "x" + str(height * int(args.factor))

    # Upscale Frames
    if not os.path.isdir(FOLDEROUT):
        os.mkdir(FOLDEROUT)
    w2.upscale(FOLDERIN, FOLDEROUT, int(args.factor) * width, int(args.factor) * height, args.model_type)

    # Frames to Video
    fm.to_vid(framerate, final_resolution, FOLDEROUT)

    # Extract and press audio in
    fm.extract_audio(args.video, FOLDEROUT)
    fm.insert_audio_track("output.mp4", FOLDEROUT)


processArguments()

# Check if arguments are valid / all necessary argument
# values are specified
if not args.video:
    print("Error: You need to specify the video to process")
    exit(1)
elif not args.output:
    print("Error: You need to specify the output video name")
    exit(1)
elif not args.cpu and not args.gpu and not args.cudnn:
    print("Error: You need to specify the enlarging processing unit")
    exit(1)

if __name__ == '__main__':
    try:
        video2x()
    except Exception as e:
        # This code block is reserved for future
        # fail-safe handlers
        traceback.print_exc()

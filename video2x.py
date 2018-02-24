#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2x Controller
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: Feb 24, 2018

Description: This is the main controller for Video2x

Version 1.0
"""

from ffmpeg import FFMPEG
from fractions import Fraction
from waifu2x import WAIFU2X
import argparse
import json
import os

FFMPEG_PATH = "C:/Program Files (x86)/ffmpeg/bin/"
WAIFU2X_PATH = "\"C:/Program Files (x86)/waifu2x-caffe/waifu2x-caffe-cui.exe\""

FOLDERIN = "frames"
FOLDEROUT = "upscaled"


def processArguments():
    """This function parses all arguments
    This allows users to customize options
    for the output video.
    """
    global args
    parser = argparse.ArgumentParser()
    control_group = parser.add_argument_group('Controls')
    control_group.add_argument("-f", "--factor", help="Factor to enlarge video by", action="store", default=2)
    control_group.add_argument("-v", "--video", help="Specify video file", action="store", default=False)
    control_group.add_argument("-o", "--output", help="Specify output file", action="store", default=False)
    args = parser.parse_args()


def get_vid_info():
    """Gets original video information
    This function uses ffprobe to determine the
    properties of the original video.

    It returns a dict
    """
    os.system("{} -v quiet -print_format json -show_format -show_streams {} > info.json".format("\"" + FFMPEG_PATH + "ffprobe.exe\"", args.video))
    json_file = open('info.json', 'r')
    json_str = json_file.read()
    print(json.loads(json_str))
    return json.loads(json_str)


def main():
    """Main flow control function for video2x.
    This function takes care of the order of operation.
    """
    fm = FFMPEG("\"" + FFMPEG_PATH + "ffmpeg.exe\"", args.output)
    w2 = WAIFU2X(WAIFU2X_PATH)

    # Extract Frames
    if not os.path.isdir(FOLDERIN):
        os.mkdir(FOLDERIN)
    fm.strip_frames(args.video, FOLDERIN)

    info = get_vid_info()
    width, height, framerate = info["streams"][0]["width"], info["streams"][0]["height"], float(Fraction(info["streams"][0]["avg_frame_rate"]))
    print("Framerate: ", framerate)
    final_resolution = str(width * int(args.factor)) + "x" + str(height * int(args.factor))

    # Upscale Frames
    if not os.path.isdir(FOLDEROUT):
        os.mkdir(FOLDEROUT)
    w2.upscale(FOLDERIN, FOLDEROUT, int(args.factor) * width, int(args.factor) * height)

    # Frames to Video
    fm.to_vid(framerate, final_resolution, FOLDEROUT)

    # Extract and press audio in
    fm.extract_audio(args.video, FOLDEROUT)
    fm.pressin_audio("output.mp4", FOLDEROUT)


processArguments()
main()

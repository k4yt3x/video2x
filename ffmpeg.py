#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: FFMPEG Class
Author: K4YT3X
Date Created: Feb 24, 2018
Last Modified: October 22, 2018

Description: This class handles all FFMPEG related
operations.

Version 2.0.3
"""
import subprocess


class Ffmpeg:
    """This class communicates with ffmpeg

    This class deals with ffmpeg. It handles extracitng
    frames, stripping audio, converting images into videos
    and inserting audio tracks to videos.
    """

    def __init__(self, ffmpeg_path, outfile):
        self.ffmpeg_path = ffmpeg_path
        self.outfile = outfile

    def extract_frames(self, videoin, outpath):
        """Extract every frame from original videos

        This method extracts every frame from videoin
        using ffmpeg

        Arguments:
            videoin {string} -- input video path
            outpath {string} -- video output folder
        """
        execute = "{} -i \"{}\" {}\\extracted_%0d.png -y".format(self.ffmpeg_path, videoin, outpath)
        print(execute)
        subprocess.call(execute)

    def extract_audio(self, videoin, outpath):
        """Strips audio tracks from videos

        This method strips audio tracks from videos
        into the output folder in aac format.

        Arguments:
            videoin {string} -- input video path
            outpath {string} -- video output folder
        """
        execute = "{} -i \"{}\" -vn -acodec copy {}\\output-audio.aac -y".format(self.ffmpeg_path, videoin, outpath)
        print(execute)
        subprocess.call(execute)

    def convert_video(self, framerate, resolution, upscaled, ):
        """Converts images into videos

        This method converts a set of images into a
        video.

        Arguments:
            framerate {float} -- target video framerate
            resolution {string} -- target video resolution
            upscaled {string} -- source images folder
        """
        execute = "{} -r {} -f image2 -s {} -i {}\\extracted_%d.png -vcodec libx264 -crf 25 -pix_fmt yuv420p {}\\no_audio.mp4 -y".format(
            self.ffmpeg_path, framerate, resolution, upscaled, upscaled)
        print(execute)
        subprocess.call(execute)

    def insert_audio_track(self, upscaled):
        """Insert audio into video

        Inserts the WAV audio track stripped from
        the original video into final video.

        Arguments:
            upscaled {string} -- upscaled image folder
        """
        execute = "{} -i {}\\no_audio.mp4 -i {}\\output-audio.aac -shortest -codec copy {} -y".format(self.ffmpeg_path, upscaled, upscaled, self.outfile)
        print(execute)
        subprocess.call(execute)

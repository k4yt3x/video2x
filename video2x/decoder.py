#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Copyright (C) 2018-2023 K4YT3X and contributors.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.

Name: Video Decoder
Author: K4YT3X <i@k4yt3x.com>
"""

import contextlib
import os
import pathlib
import signal
import subprocess
from multiprocessing import Queue
from queue import Full
from threading import Thread

import ffmpeg
from PIL import Image

from .pipe_printer import PipePrinter

# map Loguru log levels to FFmpeg log levels
LOGURU_FFMPEG_LOGLEVELS = {
    "trace": "trace",
    "debug": "debug",
    "info": "info",
    "success": "info",
    "warning": "warning",
    "error": "error",
    "critical": "fatal",
}


class VideoDecoder:
    """
    A video decoder that generates frames read from FFmpeg.

    :param input_path pathlib.Path: the input file's path
    :param input_width int: the input file's width
    :param input_height int: the input file's height
    :param frame_rate float: the input file's frame rate
    :param pil_ignore_max_image_pixels bool: setting this to True
        disables PIL's "possible DDoS" warning
    """

    def __init__(
        self,
        input_path: pathlib.Path,
        input_width: int,
        input_height: int,
        frame_rate: float,
        pil_ignore_max_image_pixels: bool = True,
    ) -> None:
        self.input_path = input_path
        self.input_width = input_width
        self.input_height = input_height

        # this disables the "possible DDoS" warning
        if pil_ignore_max_image_pixels is True:
            Image.MAX_IMAGE_PIXELS = None

        self.decoder = subprocess.Popen(
            ffmpeg.compile(
                ffmpeg.input(input_path, r=frame_rate)["v"]
                .output("pipe:1", format="rawvideo", pix_fmt="rgb24")
                .global_args("-hide_banner")
                .global_args("-nostats")
                .global_args("-nostdin")
                .global_args(
                    "-loglevel",
                    LOGURU_FFMPEG_LOGLEVELS.get(
                        os.environ.get("LOGURU_LEVEL", "INFO").lower()
                    ),
                ),
                overwrite_output=True,
            ),
            env=dict(AV_LOG_FORCE_COLOR="TRUE", **os.environ),
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

        # start the PIPE printer to start printing FFmpeg logs
        self.pipe_printer = PipePrinter(self.decoder.stderr)
        self.pipe_printer.start()

    def __iter__(self):
        # continue yielding while FFmpeg continues to produce output
        # it is possible to use := for this block to be more concise
        # but it is purposefully avoided to remain compatible with Python 3.7
        buffer = self.decoder.stdout.read(3 * self.input_width * self.input_height)

        while len(buffer) > 0:
            # convert raw bytes into image object
            frame = Image.frombytes(
                "RGB", (self.input_width, self.input_height), buffer
            )

            # return this frame
            yield frame

            # read the next frame
            buffer = self.decoder.stdout.read(3 * self.input_width * self.input_height)

        # automatically self-join and clean up after iterations are done
        self.join()

    def kill(self):
        self.decoder.send_signal(signal.SIGKILL)

    def join(self):
        # close PIPEs to prevent process from getting stuck
        self.decoder.stdout.close()
        self.decoder.stderr.close()

        # wait for process to exit
        self.decoder.wait()

        # wait for PIPE printer to exit
        self.pipe_printer.stop()
        self.pipe_printer.join()


class VideoDecoderThread(Thread):
    def __init__(
        self, tasks_queue: Queue, decoder: VideoDecoder, processing_settings: tuple
    ):
        super().__init__()

        self.tasks_queue = tasks_queue
        self.decoder = decoder
        self.processing_settings = processing_settings
        self.running = False

    def run(self):
        self.running = True
        previous_frame = None
        for frame_index, frame in enumerate(self.decoder):
            while True:
                # check for the stop signal
                if self.running is False:
                    self.decoder.join()
                    return

                with contextlib.suppress(Full):
                    self.tasks_queue.put(
                        (frame_index, previous_frame, frame, self.processing_settings),
                        timeout=0.1,
                    )
                    break

            previous_frame = frame

    def stop(self):
        self.running = False

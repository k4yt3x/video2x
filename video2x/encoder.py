#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Copyright (C) 2018-2022 K4YT3X and contributors.

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

Name: Video Encoder
Author: K4YT3X
Date Created: June 17, 2021
Last Modified: June 30, 2021
"""

# built-in imports
import multiprocessing
import multiprocessing.managers
import multiprocessing.sharedctypes
import os
import pathlib
import signal
import subprocess
import threading
import time

# third-party imports
from loguru import logger
import ffmpeg


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


class VideoEncoder(threading.Thread):
    def __init__(
        self,
        input_path: pathlib.Path,
        frame_rate: float,
        output_path: pathlib.Path,
        output_width: int,
        output_height: int,
        total_frames: int,
        processed_frames: multiprocessing.managers.ListProxy,
        processed: multiprocessing.sharedctypes.Synchronized,
    ):
        threading.Thread.__init__(self)
        self.running = False
        self.input_path = input_path
        self.output_path = output_path
        self.total_frames = total_frames
        self.processed_frames = processed_frames
        self.processed = processed

        self.original = ffmpeg.input(input_path)

        # define frames as input
        frames = ffmpeg.input(
            "pipe:0",
            format="rawvideo",
            pix_fmt="rgb24",
            vsync="1",
            s=f"{output_width}x{output_height}",
            r=frame_rate,
        )

        # map additional streams from original file
        """
        additional_streams = [
            # self.original["v?"],
            self.original["a?"],
            self.original["s?"],
            self.original["d?"],
            self.original["t?"],
        ]
        """

        # run FFmpeg and produce final output
        self.encoder = subprocess.Popen(
            ffmpeg.compile(
                ffmpeg.output(
                    frames,
                    str(self.output_path),
                    pix_fmt="yuv420p",
                    vcodec="libx264",
                    acodec="copy",
                    r=frame_rate,
                    crf=17,
                    vsync="1",
                    # map_metadata=1,
                    # metadata="comment=Upscaled with Video2X",
                )
                .global_args("-hide_banner")
                .global_args("-nostats")
                .global_args(
                    "-loglevel",
                    LOGURU_FFMPEG_LOGLEVELS.get(
                        os.environ.get("LOGURU_LEVEL", "INFO").lower()
                    ),
                ),
                overwrite_output=True,
            ),
            stdin=subprocess.PIPE,
            # stdout=subprocess.DEVNULL,
            # stderr=subprocess.DEVNULL,
        )

    def run(self):
        self.running = True
        frame_index = 0
        while self.running and frame_index < self.total_frames:
            try:
                image = self.processed_frames[frame_index]
                if image is None:
                    time.sleep(0.1)
                    continue

                # send the image to FFmpeg for encoding
                self.encoder.stdin.write(image.tobytes())

                # remove the image from memory
                self.processed_frames[frame_index] = None

                with self.processed.get_lock():
                    self.processed.value += 1

                frame_index += 1

            # send exceptions into the client connection pipe
            except Exception as e:
                logger.exception(e)
                break

        # flush the remaining data in STDIN and close PIPE
        logger.debug("Encoding queue depleted")
        self.encoder.stdin.flush()
        self.encoder.stdin.close()

        # send SIGINT (2) to FFmpeg
        # this instructs it to finalize and exit
        self.encoder.send_signal(signal.SIGINT)

        # wait for process to terminate
        self.encoder.wait()
        logger.info("Encoder thread exiting")

        self.running = False
        return super().run()

    def stop(self):
        self.running = False

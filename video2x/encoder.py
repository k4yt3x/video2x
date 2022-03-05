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
Last Modified: March 1, 2022
"""

import multiprocessing
import multiprocessing.managers
import multiprocessing.sharedctypes
import os
import pathlib
import signal
import subprocess
import threading
import time

import ffmpeg
from loguru import logger

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
        copy_audio: bool = True,
        copy_subtitle: bool = True,
        copy_data: bool = False,
        copy_attachments: bool = False,
    ) -> None:
        threading.Thread.__init__(self)
        self.running = False
        self.input_path = input_path
        self.output_path = output_path
        self.total_frames = total_frames
        self.processed_frames = processed_frames
        self.processed = processed

        # stores exceptions if the thread exits with errors
        self.exception = None

        # create FFmpeg input for the original input video
        self.original = ffmpeg.input(input_path)

        # define frames as input
        frames = ffmpeg.input(
            "pipe:0",
            format="rawvideo",
            pix_fmt="rgb24",
            vsync="cfr",
            s=f"{output_width}x{output_height}",
            r=frame_rate,
        )

        # copy additional streams from original file
        # https://ffmpeg.org/ffmpeg.html#Stream-specifiers-1
        additional_streams = [
            # self.original["1:v?"],
            self.original["a?"] if copy_audio is True else None,
            self.original["s?"] if copy_subtitle is True else None,
            self.original["d?"] if copy_data is True else None,
            self.original["t?"] if copy_attachments is True else None,
        ]

        # run FFmpeg and produce final output
        self.encoder = subprocess.Popen(
            ffmpeg.compile(
                ffmpeg.output(
                    frames,
                    *[s for s in additional_streams if s is not None],
                    str(self.output_path),
                    vcodec="libx264",
                    vsync="cfr",
                    pix_fmt="yuv420p",
                    crf=17,
                    preset="veryslow",
                    # acodec="libfdk_aac",
                    # cutoff=20000,
                    r=frame_rate,
                    map_metadata=1,
                    metadata="comment=Processed with Video2X",
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
            env={"AV_LOG_FORCE_COLOR": "TRUE"},
            stdin=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

        # start the PIPE printer to start printing FFmpeg logs
        self.pipe_printer = PipePrinter(self.encoder.stderr)
        self.pipe_printer.start()

    def run(self) -> None:
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
                self.exception = e
                logger.exception(e)
                break
        else:
            logger.debug("Encoding queue depleted")

        # flush the remaining data in STDIN and STDERR
        self.encoder.stdin.flush()
        self.encoder.stderr.flush()

        # send SIGINT (2) to FFmpeg
        # this instructs it to finalize and exit
        self.encoder.send_signal(signal.SIGINT)

        # close PIPEs to prevent process from getting stuck
        self.encoder.stdin.close()
        self.encoder.stderr.close()

        # wait for process to exit
        self.encoder.wait()

        # wait for PIPE printer to exit
        self.pipe_printer.stop()
        self.pipe_printer.join()

        logger.info("Encoder thread exiting")
        return super().run()

    def stop(self) -> None:
        self.running = False

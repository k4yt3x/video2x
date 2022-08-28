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
Last Modified: August 28, 2022
"""

import os
import pathlib
import signal
import subprocess

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


class VideoEncoder:
    def __init__(
        self,
        input_path: pathlib.Path,
        frame_rate: float,
        output_path: pathlib.Path,
        output_width: int,
        output_height: int,
        copy_audio: bool = True,
        copy_subtitle: bool = True,
        copy_data: bool = False,
        copy_attachments: bool = False,
    ) -> None:

        # create FFmpeg input for the original input video
        original = ffmpeg.input(input_path)

        # define frames as input
        frames = ffmpeg.input(
            "pipe:0",
            format="rawvideo",
            pix_fmt="rgb24",
            s=f"{output_width}x{output_height}",
            r=frame_rate,
        )

        # copy additional streams from original file
        # https://ffmpeg.org/ffmpeg.html#Stream-specifiers-1
        additional_streams = [
            # original["1:v?"],
            original["a?"] if copy_audio is True else None,
            original["s?"] if copy_subtitle is True else None,
            original["d?"] if copy_data is True else None,
            original["t?"] if copy_attachments is True else None,
        ]

        # run FFmpeg and produce final output
        self.encoder = subprocess.Popen(
            ffmpeg.compile(
                ffmpeg.output(
                    frames,
                    *[s for s in additional_streams if s is not None],
                    str(output_path),
                    vcodec="libx264",
                    scodec="copy",
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
            env=dict(AV_LOG_FORCE_COLOR="TRUE", **os.environ),
            stdin=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

        # start the PIPE printer to start printing FFmpeg logs
        self.pipe_printer = PipePrinter(self.encoder.stderr)
        self.pipe_printer.start()

    def kill(self):
        self.encoder.send_signal(signal.SIGKILL)

    def write(self, frame: Image.Image) -> None:
        """
        write a frame into FFmpeg encoder's STDIN

        :param frame Image.Image: the Image object to use for writing
        """
        self.encoder.stdin.write(frame.tobytes())

    def join(self) -> None:
        """
        signal the encoder that all frames have been sent and the FFmpeg
        should be instructed to wrap-up the processing
        """
        # flush the remaining data in STDIN and STDERR
        self.encoder.stdin.flush()
        self.encoder.stderr.flush()

        # close PIPEs to prevent process from getting stuck
        self.encoder.stdin.close()
        self.encoder.stderr.close()

        # wait for process to exit
        self.encoder.wait()

        # wait for PIPE printer to exit
        self.pipe_printer.stop()
        self.pipe_printer.join()

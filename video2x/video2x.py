#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
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

__      __  _       _                  ___   __   __
\ \    / / (_)     | |                |__ \  \ \ / /
 \ \  / /   _    __| |   ___    ___      ) |  \ V /
  \ \/ /   | |  / _` |  / _ \  / _ \    / /    > <
   \  /    | | | (_| | |  __/ | (_) |  / /_   / . \
    \/     |_|  \__,_|  \___|  \___/  |____| /_/ \_\


Name: Video2X
Creator: K4YT3X
Date Created: Feb 24, 2018
Last Modified: February 12, 2022

Editor: BrianPetkovsek
Last Modified: June 17, 2019

Editor: SAT3LL
Last Modified: June 25, 2019

Editor: 28598519a
Last Modified: March 23, 2020
"""

# local imports
from . import __version__
from .decoder import VideoDecoder
from .encoder import VideoEncoder
from .interpolator import Interpolator
from .upscaler import Upscaler

# built-in imports
import argparse
import math
import multiprocessing
import os
import pathlib
import sys
import time

# third-party imports
from loguru import logger
from rich import print
from rich.console import Console
from rich.file_proxy import FileProxy
from rich.progress import (
    BarColumn,
    Progress,
    ProgressColumn,
    Task,
    TimeElapsedColumn,
    TimeRemainingColumn,
)
from rich.text import Text
import cv2
import ffmpeg


LEGAL_INFO = """Video2X {}
Author: K4YT3X
License: GNU GPL v3
Github Page: https://github.com/k4yt3x/video2x
Contact: k4yt3x@k4yt3x.com""".format(
    __version__
)

# algorithms available for upscaling tasks
UPSCALING_ALGORITHMS = [
    "waifu2x",
    "srmd",
    "realsr",
]

# algorithms available for frame interpolation tasks
INTERPOLATION_ALGORITHMS = ["rife"]

# progress bar labels for different modes
MODE_LABELS = {"upscale": "Upscaling", "interpolate": "Interpolating"}

# format string for Loguru loggers
LOGURU_FORMAT = (
    "<green>{time:HH:mm:ss.SSSSSS!UTC}</green> | "
    "<level>{level: <8}</level> | "
    "<level>{message}</level>"
)


class ProcessingSpeedColumn(ProgressColumn):
    """Custom progress bar column that displays the processing speed"""

    def render(self, task: Task) -> Text:
        speed = task.finished_speed or task.speed
        return Text(
            f"{round(speed, 2) if isinstance(speed, float) else '?'} FPS",
            style="progress.data.speed",
        )


class Video2X:
    """
    Video2X class

    provides two vital functions:
        - upscale: perform upscaling on a file
        - interpolate: perform motion interpolation on a file
    """

    def __init__(self):
        self.version = "5.0.0"

    def _get_video_info(self, path: pathlib.Path):
        """
        get video file information with FFmpeg

        :param path pathlib.Path: video file path
        :raises RuntimeError: raised when video stream isn't found
        """
        # probe video file info
        logger.info("Reading input video information")
        for stream in ffmpeg.probe(path)["streams"]:
            if stream["codec_type"] == "video":
                video_info = stream
                break
        else:
            raise RuntimeError("unable to find video stream")

        # get total number of frames to be processed
        capture = cv2.VideoCapture(str(path))

        # check if file is opened successfully
        if not capture.isOpened():
            raise RuntimeError("OpenCV has failed to open the input file")

        total_frames = int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
        frame_rate = capture.get(cv2.CAP_PROP_FPS)

        return video_info["width"], video_info["height"], total_frames, frame_rate

    def _run(
        self,
        input_path: pathlib.Path,
        width: int,
        height: int,
        total_frames: int,
        frame_rate: float,
        output_path: pathlib.Path,
        output_width: int,
        output_height: int,
        Processor: object,
        mode: str,
        processes: int,
        processing_settings: tuple,
    ):

        # record original STDOUT and STDERR for restoration
        original_stdout = sys.stdout
        original_stderr = sys.stderr

        # create console for rich's Live display
        console = Console()

        # redirect STDOUT and STDERR to console
        sys.stdout = FileProxy(console, sys.stdout)
        sys.stderr = FileProxy(console, sys.stderr)

        # re-add Loguru to point to the new STDERR
        logger.remove()
        logger.add(sys.stderr, colorize=True, format=LOGURU_FORMAT)

        # initialize values
        self.processor_processes = []
        self.processing_queue = multiprocessing.Queue(maxsize=processes * 10)
        processed_frames = multiprocessing.Manager().list([None] * total_frames)
        self.processed = multiprocessing.Value("I", 0)

        # set up and start decoder thread
        logger.info("Starting video decoder")
        self.decoder = VideoDecoder(
            input_path,
            width,
            height,
            frame_rate,
            self.processing_queue,
            processing_settings,
        )
        self.decoder.start()

        # set up and start encoder thread
        logger.info("Starting video encoder")
        self.encoder = VideoEncoder(
            input_path,
            frame_rate * 2 if mode == "interpolate" else frame_rate,
            output_path,
            output_width,
            output_height,
            total_frames,
            processed_frames,
            self.processed,
        )
        self.encoder.start()

        # create processor processes
        for process_name in range(processes):
            process = Processor(self.processing_queue, processed_frames)
            process.name = str(process_name)
            process.daemon = True
            process.start()
            self.processor_processes.append(process)

        # a temporary variable that stores the exception
        exception = []

        try:
            # create progress bar
            with Progress(
                "[progress.description]{task.description}",
                BarColumn(finished_style="green"),
                "[progress.percentage]{task.percentage:>3.0f}%",
                "[color(240)]({task.completed}/{task.total})",
                ProcessingSpeedColumn(),
                TimeElapsedColumn(),
                "<",
                TimeRemainingColumn(),
                console=console,
                disable=True,
            ) as progress:
                task = progress.add_task(
                    f"[cyan]{MODE_LABELS.get(mode, 'Unknown')}", total=total_frames
                )

                # wait for jobs in queue to deplete
                while self.processed.value < total_frames - 1:
                    time.sleep(0.5)
                    for process in self.processor_processes:
                        if not process.is_alive():
                            raise Exception("process died unexpectedly")

                    # show progress bar when upscale starts
                    if progress.disable is True and self.processed.value > 0:
                        progress.disable = False
                        progress.start()

                    # update progress
                    progress.update(task, completed=self.processed.value)

                progress.update(task, completed=total_frames)
            logger.info("Processing has completed")

        # if SIGTERM is received or ^C is pressed
        # TODO: pause and continue here
        except (SystemExit, KeyboardInterrupt) as e:
            logger.warning("Exit signal received, terminating")
            exception.append(e)

        except Exception as e:
            logger.exception(e)
            exception.append(e)

        # if no exceptions were produced
        else:
            logger.success("Processing completed successfully")

        finally:
            # mark processing queue as closed
            self.processing_queue.close()

            # stop upscaler processes
            logger.info("Stopping upscaler processes")
            for process in self.processor_processes:
                process.terminate()

            # wait for processes to finish
            for process in self.processor_processes:
                process.join()

            # ensure both the decoder and the encoder have exited
            self.decoder.stop()
            self.encoder.stop()
            self.decoder.join()
            self.encoder.join()

            # raise the error if there is any
            if len(exception) > 0:
                raise exception[0]

            # restore original STDOUT and STDERR
            sys.stdout = original_stdout
            sys.stderr = original_stderr

            # re-add Loguru to point to the restored STDERR
            logger.remove()
            logger.add(sys.stderr, colorize=True, format=LOGURU_FORMAT)

    def upscale(
        self,
        input_path: pathlib.Path,
        output_path: pathlib.Path,
        output_width: int,
        output_height: int,
        noise: int,
        processes: int,
        threshold: float,
        algorithm: str,
    ) -> None:

        # get basic video information
        width, height, total_frames, frame_rate = self._get_video_info(input_path)

        # automatically calculate output width and height if only one is given
        if output_width == 0 or output_width is None:
            output_width = output_height / height * width

        elif output_height == 0 or output_height is None:
            output_height = output_width / width * height

        # sanitize output width and height to be divisible by 2
        output_width = int(math.ceil(output_width / 2.0) * 2)
        output_height = int(math.ceil(output_height / 2.0) * 2)

        # start processing
        self._run(
            input_path,
            width,
            height,
            total_frames,
            frame_rate,
            output_path,
            output_width,
            output_height,
            Upscaler,
            "upscale",
            processes,
            (
                output_width,
                output_height,
                noise,
                threshold,
                algorithm,
            ),
        )

    def interpolate(
        self,
        input_path: pathlib.Path,
        output_path: pathlib.Path,
        processes: int,
        threshold: float,
        algorithm: str,
    ) -> None:

        # get video basic information
        width, height, original_frames, frame_rate = self._get_video_info(input_path)

        # calculate the number of total output frames
        total_frames = original_frames * 2 - 1

        # start processing
        self._run(
            input_path,
            width,
            height,
            total_frames,
            frame_rate,
            output_path,
            width,
            height,
            Interpolator,
            "interpolate",
            processes,
            (threshold, algorithm),
        )


def parse_arguments() -> argparse.Namespace:
    """
    parse command line arguments

    :rtype argparse.Namespace: command parsing results
    """
    parser = argparse.ArgumentParser(
        prog="video2x",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "-v", "--version", help="show version information and exit", action="store_true"
    )
    parser.add_argument(
        "-i",
        "--input",
        type=pathlib.Path,
        help="input file/directory path",
        required=True,
    )
    parser.add_argument(
        "-o",
        "--output",
        type=pathlib.Path,
        help="output file/directory path",
        required=True,
    )
    parser.add_argument(
        "-p", "--processes", type=int, help="number of processes to launch", default=1
    )
    parser.add_argument(
        "-l",
        "--loglevel",
        choices=["trace", "debug", "info", "success", "warning", "error", "critical"],
        default="info",
    )

    # upscaler arguments
    action = parser.add_subparsers(
        help="action to perform", dest="action", required=True
    )

    upscale = action.add_parser("upscale", help="upscale a file", add_help=False)
    upscale.add_argument(
        "--help", action="help", help="show this help message and exit"
    )
    upscale.add_argument("-w", "--width", type=int, help="output width")
    upscale.add_argument("-h", "--height", type=int, help="output height")
    upscale.add_argument("-n", "--noise", type=int, help="denoise level", default=3)
    upscale.add_argument(
        "-a",
        "--algorithm",
        choices=UPSCALING_ALGORITHMS,
        help="algorithm to use for upscaling",
        default=UPSCALING_ALGORITHMS[0],
    )
    upscale.add_argument(
        "-t",
        "--threshold",
        type=float,
        help="skip if the % difference between two adjacent frames is below this value; set to 0 to process all frames",
        default=0,
    )

    # interpolator arguments
    interpolate = action.add_parser(
        "interpolate", help="interpolate frames for file", add_help=False
    )
    interpolate.add_argument(
        "--help", action="help", help="show this help message and exit"
    )
    interpolate.add_argument(
        "-a",
        "--algorithm",
        choices=UPSCALING_ALGORITHMS,
        help="algorithm to use for upscaling",
        default=INTERPOLATION_ALGORITHMS[0],
    )
    interpolate.add_argument(
        "-t",
        "--threshold",
        type=float,
        help="skip if the % difference between two adjacent frames exceeds this value; set to 100 to interpolate all frames",
        default=10,
    )

    return parser.parse_args()


def main():
    """
    command line direct invocation
    program entry point
    """

    try:

        # parse command line arguments
        args = parse_arguments()

        # set logger level
        if os.environ.get("LOGURU_LEVEL") is None:
            os.environ["LOGURU_LEVEL"] = args.loglevel.upper()

        # remove default handler
        logger.remove()

        # add new sink with custom handler
        logger.add(sys.stderr, colorize=True, format=LOGURU_FORMAT)

        # display version and lawful informaition
        if args.version:
            print(LEGAL_INFO)
            sys.exit(0)

        # print package version and copyright notice
        logger.opt(colors=True).info(f"<magenta>Video2X {__version__}</magenta>")
        logger.opt(colors=True).info(
            "<magenta>Copyright (C) 2018-2022 K4YT3X and contributors.</magenta>"
        )

        # initialize upscaler object
        video2x = Video2X()

        if args.action == "upscale":
            video2x.upscale(
                args.input,
                args.output,
                args.width,
                args.height,
                args.noise,
                args.processes,
                args.threshold,
                args.algorithm,
            )

        elif args.action == "interpolate":
            video2x.interpolate(
                args.input,
                args.output,
                args.processes,
                args.threshold,
                args.algorithm,
            )

    # don't print the traceback for manual terminations
    except (SystemExit, KeyboardInterrupt) as e:
        raise SystemExit(e)

    except Exception as e:
        logger.exception(e)
        raise SystemExit(e)

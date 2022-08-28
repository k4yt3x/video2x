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
Date Created: February 24, 2018
Last Modified: August 28, 2022

Editor: BrianPetkovsek
Last Modified: June 17, 2019

Editor: SAT3LL
Last Modified: June 25, 2019

Editor: 28598519a
Last Modified: March 23, 2020
"""

import ctypes
import math
import signal
import sys
import time
from enum import Enum
from multiprocessing import Manager, Pool, Queue, Value
from pathlib import Path

import ffmpeg
from cv2 import cv2
from loguru import logger
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

from video2x.processor import Processor

from . import __version__
from .decoder import VideoDecoder, VideoDecoderThread
from .encoder import VideoEncoder
from .interpolator import Interpolator
from .upscaler import UpscalerProcessor

# for desktop environments only
# if pynput can be loaded, enable global pause hotkey support
try:
    from pynput.keyboard import HotKey, Listener
except ImportError:
    ENABLE_HOTKEY = False
else:
    ENABLE_HOTKEY = True

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


class ProcessingMode(Enum):
    UPSCALE = {"label": "Upscaling", "processor": UpscalerProcessor}
    INTERPOLATE = {"label": "Interpolating", "processor": Interpolator}


class Video2X:
    """
    Video2X class

    provides two vital functions:
        - upscale: perform upscaling on a file
        - interpolate: perform motion interpolation on a file
    """

    def __init__(self) -> None:
        self.version = __version__

    @staticmethod
    def _get_video_info(path: Path) -> tuple:
        """
        get video file information with FFmpeg

        :param path Path: video file path
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
        input_path: Path,
        width: int,
        height: int,
        total_frames: int,
        frame_rate: float,
        output_path: Path,
        output_width: int,
        output_height: int,
        mode: ProcessingMode,
        processes: int,
        processing_settings: tuple,
    ) -> None:

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

        # TODO: add docs
        tasks_queue = Queue(maxsize=processes * 10)
        processed_frames = Manager().dict()
        pause_flag = Value(ctypes.c_bool, False)

        # set up and start decoder thread
        logger.info("Starting video decoder")
        decoder = VideoDecoder(
            input_path,
            width,
            height,
            frame_rate,
        )
        decoder_thread = VideoDecoderThread(tasks_queue, decoder, processing_settings)
        decoder_thread.start()

        # set up and start encoder thread
        logger.info("Starting video encoder")
        encoder = VideoEncoder(
            input_path,
            frame_rate * 2 if mode == "interpolate" else frame_rate,
            output_path,
            output_width,
            output_height,
        )

        # create a pool of processor processes to process the queue
        processor: Processor = mode.value["processor"](
            tasks_queue, processed_frames, pause_flag
        )
        processor_pool = Pool(processes, processor.process)

        # create progress bar
        progress = Progress(
            "[progress.description]{task.description}",
            BarColumn(complete_style="blue", finished_style="green"),
            "[progress.percentage]{task.percentage:>3.0f}%",
            "[color(240)]({task.completed}/{task.total})",
            ProcessingSpeedColumn(),
            TimeElapsedColumn(),
            "<",
            TimeRemainingColumn(),
            console=console,
            speed_estimate_period=300.0,
            disable=True,
        )
        task = progress.add_task(f"[cyan]{mode.value['label']}", total=total_frames)

        def _toggle_pause(_signal_number: int = -1, _frame=None):

            # allow the closure to modify external immutable flag
            nonlocal pause_flag

            # print console messages and update the progress bar's status
            if pause_flag.value is False:
                progress.update(
                    task, description=f"[cyan]{mode.value['label']} (paused)"
                )
                progress.stop_task(task)
                logger.warning("Processing paused, press Ctrl+Alt+V again to resume")

            # the lock is already acquired
            elif pause_flag.value is True:
                progress.update(task, description=f"[cyan]{mode.value['label']}")
                logger.warning("Resuming processing")
                progress.start_task(task)

            # invert the flag
            with pause_flag.get_lock():
                pause_flag.value = not pause_flag.value

        # allow sending SIGUSR1 to pause/resume processing
        signal.signal(signal.SIGUSR1, _toggle_pause)

        # enable global pause hotkey if it's supported
        if ENABLE_HOTKEY is True:

            # create global pause hotkey
            pause_hotkey = HotKey(HotKey.parse("<ctrl>+<alt>+v"), _toggle_pause)

            # create global keyboard input listener
            keyboard_listener = Listener(
                on_press=(
                    lambda key: pause_hotkey.press(keyboard_listener.canonical(key))
                ),
                on_release=(
                    lambda key: pause_hotkey.release(keyboard_listener.canonical(key))
                ),
            )

            # start monitoring global key presses
            keyboard_listener.start()

        # a temporary variable that stores the exception
        exceptions = []

        try:

            # let the context manager automatically stop the progress bar
            with progress:

                frame_index = 0
                while frame_index < total_frames:

                    current_frame = processed_frames.get(frame_index)

                    if pause_flag.value is True or current_frame is None:
                        time.sleep(0.1)
                        continue

                    # show the progress bar after the processing starts
                    # reduces speed estimation inaccuracies and print overlaps
                    if frame_index == 0:
                        progress.disable = False
                        progress.start()

                    if current_frame is True:
                        encoder.write(processed_frames.get(frame_index - 1))

                    else:
                        encoder.write(current_frame)

                        if frame_index > 0:
                            del processed_frames[frame_index - 1]

                    progress.update(task, completed=frame_index + 1)
                    frame_index += 1

        # if SIGTERM is received or ^C is pressed
        except (SystemExit, KeyboardInterrupt) as error:
            logger.warning("Exit signal received, exiting gracefully")
            logger.warning("Press ^C again to force terminate")
            exceptions.append(error)

        except Exception as error:
            logger.exception(error)
            exceptions.append(error)

        else:
            logger.info("Processing has completed")

        finally:

            # stop keyboard listener
            if ENABLE_HOTKEY is True:
                keyboard_listener.stop()
                keyboard_listener.join()

            # if errors have occurred, kill the FFmpeg processes
            if len(exceptions) > 0:
                decoder.kill()
                encoder.kill()

            # stop the decoder
            decoder_thread.stop()
            decoder_thread.join()

            # clear queue and signal processors to exit
            # multiprocessing.Queue has no Queue.queue.clear
            while tasks_queue.empty() is not True:
                tasks_queue.get()
            for _ in range(processes):
                tasks_queue.put(None)

            # close and join the process pool
            processor_pool.close()
            processor_pool.join()

            # stop the encoder
            encoder.join()

            # restore original STDOUT and STDERR
            sys.stdout = original_stdout
            sys.stderr = original_stderr

            # re-add Loguru to point to the restored STDERR
            logger.remove()
            logger.add(sys.stderr, colorize=True, format=LOGURU_FORMAT)

            # raise the first collected error
            if len(exceptions) > 0:
                raise exceptions[0]

    def upscale(
        self,
        input_path: Path,
        output_path: Path,
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
            ProcessingMode.UPSCALE,
            processes,
            (
                output_width,
                output_height,
                algorithm,
                noise,
                threshold,
            ),
        )

    def interpolate(
        self,
        input_path: Path,
        output_path: Path,
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
            ProcessingMode.INTERPOLATE,
            processes,
            (threshold, algorithm),
        )
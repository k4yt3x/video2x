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

Name: Interpolator
Author: K4YT3X
Date Created: May 27, 2021
Last Modified: February 2, 2022
"""

# local imports
from rife_ncnn_vulkan_python.rife_ncnn_vulkan import Rife

# built-in imports
import multiprocessing
import multiprocessing.managers
import multiprocessing.sharedctypes
import queue
import signal
import time

# third-party imports
from PIL import ImageChops, ImageStat
from loguru import logger


ALGORITHM_CLASSES = {"rife": Rife}


class Interpolator(multiprocessing.Process):
    def __init__(
        self,
        processing_queue: multiprocessing.Queue,
        processed_frames: multiprocessing.managers.ListProxy,
    ):
        multiprocessing.Process.__init__(self)
        self.running = False
        self.processing_queue = processing_queue
        self.processed_frames = processed_frames

        signal.signal(signal.SIGTERM, self._stop)

    def run(self):
        self.running = True
        logger.info(f"Interpolator process {self.name} initiating")
        processor_objects = {}
        while self.running:
            try:
                try:
                    # get new job from queue
                    (
                        frame_index,
                        (image0, image1),
                        (difference_threshold, algorithm),
                    ) = self.processing_queue.get(False)
                except queue.Empty:
                    time.sleep(0.1)
                    continue

                # if image0 is None, image1 is the first frame
                # skip this round
                if image0 is None:
                    continue

                difference = ImageChops.difference(image0, image1)
                difference_stat = ImageStat.Stat(difference)
                difference_ratio = (
                    sum(difference_stat.mean) / (len(difference_stat.mean) * 255) * 100
                )

                # if the difference is lower than threshold
                # process the interpolation
                if difference_ratio < difference_threshold:

                    # select a processor object with the required settings
                    # create a new object if none are available
                    processor_object = processor_objects.get(algorithm)
                    if processor_object is None:
                        processor_object = ALGORITHM_CLASSES[algorithm](0)
                        processor_objects[algorithm] = processor_object
                    interpolated_image = processor_object.process(image0, image1)

                # if the difference is greater than threshold
                # there's a change in camera angle, ignore
                else:
                    interpolated_image = image0

                if frame_index == 1:
                    self.processed_frames[0] = image0
                self.processed_frames[frame_index * 2 - 1] = interpolated_image
                self.processed_frames[frame_index * 2] = image1

            # send exceptions into the client connection pipe
            except (SystemExit, KeyboardInterrupt):
                break

            except Exception as e:
                logger.exception(e)
                break

        logger.info(f"Interpolator process {self.name} terminating")
        self.running = False
        return super().run()

    def _stop(self, _signal_number, _frame):
        self.running = False

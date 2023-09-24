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

Name: Interpolator
Author: K4YT3X <i@k4yt3x.com>
"""

import time
from importlib import import_module

from loguru import logger
from PIL import ImageChops, ImageStat

from .processor import Processor


class Interpolator:
    ALGORITHM_CLASSES = {"rife": "rife_ncnn_vulkan_python.rife_ncnn_vulkan.Rife"}

    processor_objects = {}

    def interpolate_image(self, image0, image1, difference_threshold, algorithm):
        difference = ImageChops.difference(image0, image1)
        difference_stat = ImageStat.Stat(difference)
        difference_ratio = (
            sum(difference_stat.mean) / (len(difference_stat.mean) * 255) * 100
        )

        if difference_ratio < difference_threshold:
            processor_object = self.processor_objects.get(algorithm)

            if processor_object is None:
                module_name, class_name = self.ALGORITHM_CLASSES[algorithm].rsplit(
                    ".", 1
                )
                processor_module = import_module(module_name)
                processor_class = getattr(processor_module, class_name)
                processor_object = processor_class(0)
                self.processor_objects[algorithm] = processor_object

            interpolated_image = processor_object.process(image0, image1)

        else:
            interpolated_image = image0

        return interpolated_image


class InterpolatorProcessor(Processor, Interpolator):
    def process(self) -> None:
        task = self.tasks_queue.get()
        while task is not None:
            try:
                if self.pause_flag.value is True:
                    time.sleep(0.1)
                    continue

                (
                    frame_index,
                    image0,
                    image1,
                    (difference_threshold, algorithm),
                ) = task

                if image0 is None:
                    task = self.tasks_queue.get()
                    continue

                interpolated_image = self.interpolate_image(
                    image0, image1, difference_threshold, algorithm
                )

                if frame_index == 1:
                    self.processed_frames[0] = image0
                self.processed_frames[frame_index * 2 - 1] = interpolated_image
                self.processed_frames[frame_index * 2] = image1

                task = self.tasks_queue.get()

            except (SystemExit, KeyboardInterrupt):
                break

            except Exception as error:
                logger.exception(error)
                break

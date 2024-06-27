#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Copyright (C) 2018-2024 K4YT3X and contributors.

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

Name: Upscaler
Author: K4YT3X <i@k4yt3x.com>
"""

import math
import time
from importlib import import_module

from PIL import Image

from .processor import Processor

import os
import sys
from contextlib import contextmanager

def fileno(file_or_fd):
    fd = getattr(file_or_fd, 'fileno', lambda: file_or_fd)()
    if not isinstance(fd, int):
        raise ValueError("Expected a file (`.fileno()`) or a file descriptor")
    return fd

@contextmanager
def stdout_redirected(to=os.devnull, stdout=None):
    if stdout is None:
       stdout = sys.stdout

    stdout_fd = fileno(stdout)
    # copy stdout_fd before it is overwritten
    # NOTE: `copied` is inheritable on Windows when duplicating a standard stream
    with os.fdopen(os.dup(stdout_fd), 'wb') as copied:
        stdout.flush()  # flush library buffers that dup2 knows nothing about
        try:
            os.dup2(fileno(to), stdout_fd)  # $ exec >&to
        except ValueError:  # filename
            with open(to, 'wb') as to_file:
                os.dup2(to_file.fileno(), stdout_fd)  # $ exec > to
        try:
            yield stdout # allow code to be run with the redirected stdout
        finally:
            # restore stdout to its previous value
            #NOTE: dup2 makes stdout_fd inheritable unconditionally
            stdout.flush()
            os.dup2(copied.fileno(), stdout_fd)  # $ exec >&copied


class Upscaler:
    # fixed scaling ratios supported by the algorithms
    # that only support certain fixed scale ratios
    ALGORITHM_FIXED_SCALING_RATIOS = {
        "anime4k": [-1],
        "realcugan": [1, 2, 3, 4],
        "realsr": [4],
        "srmd": [2, 3, 4],
        "waifu2x": [1, 2],
        "realesr-animevideov3":[2, 3, 4],
        "realesrgan-x4plus-anime":[4],
        "realesrgan-x4plus":[4],
    }

    ALGORITHM_CLASSES = {
        "anime4k": "anime4k_python.Anime4K.process",
        "realcugan": "realcugan_ncnn_vulkan_python.Realcugan.process",
        "realsr": "realsr_ncnn_vulkan_python.Realsr.process",
        "srmd": "srmd_ncnn_vulkan_python.Srmd.process",
        "waifu2x": "waifu2x_ncnn_vulkan_python.Waifu2x.process",
        "realesr-animevideov3":"realesrgan_ncnn_py.Realesrgan.process_pil",
        "realesrgan-x4plus-anime":"realesrgan_ncnn_py.Realesrgan.process_pil",
        "realesrgan-x4plus":"realesrgan_ncnn_py.Realesrgan.process_pil",
    }

    # compute the actual model id by adding the scale to it
    REALESRGAN_MODEL_BASE = {
        "realesr-animevideov3":-2,
        "realesrgan-x4plus-anime":-1,
        "realesrgan-x4plus":0,
    }

    processor_functions = {}

    @staticmethod
    def _get_scaling_tasks(
        input_width: int,
        input_height: int,
        output_width: int,
        output_height: int,
        algorithm: str,
    ) -> list:
        """
        Get the required tasks for upscaling the image until it is larger than
        or equal to the desired output dimensions. For example, SRMD only supports
        2x, 3x, and 4x, so upsclaing an image from 320x240 to 3840x2160 will
        require the SRMD to run 3x then 4x. In this case, this function will
        return [3, 4].

        :param input_width int: input image width
        :param input_height int: input image height
        :param output_width int: desired output image width
        :param output_height int: desired output image size
        :param algorithm str: upsclaing algorithm
        :rtype list: the list of upsclaing tasks required
        """
        # calculate required minimum scale ratio
        output_scale = max(output_width / input_width, output_height / input_height)

        # select the optimal algorithm scaling ratio to use
        supported_scaling_ratios = sorted(
            Upscaler.ALGORITHM_FIXED_SCALING_RATIOS[algorithm]
        )

        remaining_scaling_ratio = math.ceil(output_scale)

        # if the scaling ratio is 1.0
        # apply the smallest scaling ratio available
        if remaining_scaling_ratio == 1:
            return [supported_scaling_ratios[0]]

        # if the processor supports arbitrary scales
        # return only one job
        if supported_scaling_ratios[0] == -1:
            return [remaining_scaling_ratio]

        scaling_jobs = []
        while remaining_scaling_ratio > 1:
            for ratio in supported_scaling_ratios:
                if ratio >= remaining_scaling_ratio:
                    scaling_jobs.append(ratio)
                    remaining_scaling_ratio /= ratio
                    break

            else:
                found = False
                for i in supported_scaling_ratios:
                    for j in supported_scaling_ratios:
                        if i * j >= remaining_scaling_ratio:
                            scaling_jobs.extend([i, j])
                            remaining_scaling_ratio /= i * j
                            found = True
                            break
                    if found is True:
                        break

                if found is False:
                    scaling_jobs.append(supported_scaling_ratios[-1])
                    remaining_scaling_ratio /= supported_scaling_ratios[-1]
        return scaling_jobs

    def upscale_image(
        self,
        image: Image.Image,
        output_width: int,
        output_height: int,
        algorithm: str,
        noise: int,
    ) -> Image.Image:
        """
        upscale an image

        :param image Image.Image: the image to upscale
        :param output_width int: the desired output width
        :param output_height int: the desired output height
        :param algorithm str: the algorithm to use
        :param noise int: the noise level (available only for some algorithms)
        :rtype Image.Image: the upscaled image
        """
        width, height = image.size

        for task in self._get_scaling_tasks(
            width, height, output_width, output_height, algorithm
        ):
            # select a processor object with the required settings
            # create a new object if none are available
            processor_function = self.processor_functions.get((algorithm, task))
            if processor_function is None:
                module_name, class_name, function_name = self.ALGORITHM_CLASSES[algorithm].rsplit(
                    ".", 2
                )
                processor_module = import_module(module_name)
                processor_class = getattr(processor_module, class_name)
                processor_object = (
                    processor_class(gpuid=0, model=self.REALESRGAN_MODEL_BASE[algorithm] + task)
                    if algorithm in self.REALESRGAN_MODEL_BASE else
                    processor_class(noise=noise, scale=task)
                )
                processor_function = getattr(processor_object, function_name)
                self.processor_functions[(algorithm, task)] = processor_function

            # process the image with the selected algorithm
            image = processor_function(image)

        # downscale the image to the desired output size and
        # save the image to disk
        return image.resize((output_width, output_height), Image.Resampling.LANCZOS)


class UpscalerProcessor(Processor, Upscaler):
    def process(self) -> None:
        task = self.tasks_queue.get()
        # some precessors do output a lot of nonsense that messes the
        # output and the progress bars... so redirect it in this
        # process.  We use file level redirection, instead of just
        # replacing sys.stdout, to make sure it is effective also in
        # subprocesses that are in C++ or what not
        with stdout_redirected() as stdout, \
             stdout_redirected(stdout=sys.stderr) as stderr:
            while task is not None:
                try:
                    task = self._do_process(task)
                except KeyboardInterrupt:
                    break

    def _do_process(self, task) -> None:
        if self.pause_flag.value is True:
            time.sleep(0.1)
            return task

        # unpack the task's values
        (
            frame_index,
            previous_frame,
            current_frame,
            (output_width, output_height, algorithm, noise, threshold),
        ) = task

        # calculate the %diff between the current frame and the previous frame
        difference_ratio = 0
        if previous_frame is not None:
            difference_ratio = self.get_image_diff(
                previous_frame, current_frame
            )

        if difference_ratio < threshold:
            # if the difference is lower than threshold, skip this frame,
            # make the current image the same as the previous result
            self.processed_frames[frame_index] = True
        else:
            # if the difference is greater than threshold
            # process this frame
            self.processed_frames[frame_index] = self.upscale_image(
                current_frame, output_width, output_height, algorithm, noise
            )

        return self.tasks_queue.get()

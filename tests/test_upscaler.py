#!/usr/bin/python
# -*- coding: utf-8 -*-

from pathlib import Path

import utils
from PIL import Image

from video2x import Upscaler, Video2X


def test_upscaling():
    video2x = Video2X()

    output_path = Path("tests/data/test_video_output.mp4")

    video2x.upscale(
        Path("tests/data/test_video.mp4"),
        output_path,
        None,
        720,
        3,
        5,
        0,
        "waifu2x",
    )
    output_path.unlink()


def test_upscale_image():
    # initialize upscaler instance
    upscaler = Upscaler()

    image = Image.open("tests/data/test_image.png")
    upscaled_image = upscaler.upscale_image(image, 1680, 960, "waifu2x", 3)
    reference_image = Image.open("tests/data/test_image_ref.png")
    assert utils.get_image_diff(upscaled_image, reference_image) < 0.5


def test_get_scaling_tasks():
    dimensions = [320, 240, 3840, 2160]

    for algorithm, correct_answer in [
        ("waifu2x", [2, 2, 2, 2]),
        ["srmd", [3, 4]],
        ("realsr", [4, 4]),
        ("realcugan", [3, 4]),
    ]:
        assert Upscaler._get_scaling_tasks(*dimensions, algorithm) == correct_answer

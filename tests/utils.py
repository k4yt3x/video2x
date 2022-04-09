#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from PIL import Image, ImageChops, ImageStat


def get_image_diff(image0: Image.Image, image1: Image.Image) -> float:
    """
    calculate the percentage of differences between two images

    :param image0 Image.Image: the first frame
    :param image1 Image.Image: the second frame
    :rtype float: the percent difference between the two images
    """
    difference = ImageChops.difference(image0, image1)
    difference_stat = ImageStat.Stat(difference)
    percent_diff = sum(difference_stat.mean) / (len(difference_stat.mean) * 255) * 100
    return percent_diff

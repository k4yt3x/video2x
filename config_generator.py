#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2x Config Generator
Author: K4YT3X
Date Created: October 23, 2018
Last Modified: October 23, 2018

Licensed under the GNU General Public License Version 3 (GNU GPL v3),
    available at: https://www.gnu.org/licenses/gpl-3.0.txt

(C) 2018 K4YT3X
"""
from avalon_framework import Avalon
import json


def enroll_settings():
    settings = {}
    settings['waifu2x_path'] = Avalon.gets('waifu2x-caffe-cui.exe path: ')
    settings['ffmpeg_path'] = Avalon.gets('ffmpeg binaries directory: ')
    settings['ffmpeg_arguments'] = Avalon.gets('Extra arguments passed to ffmpeg: ')
    return settings


def write_config(settings):
    with open('video2x.json', 'w') as config:
        json.dump(settings, config, indent=2)
        config.close()


write_config(enroll_settings())

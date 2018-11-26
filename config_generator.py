#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2x Config Generator
Author: K4YT3X
Date Created: October 23, 2018
Last Modified: November 26, 2018

Licensed under the GNU General Public License Version 3 (GNU GPL v3),
    available at: https://www.gnu.org/licenses/gpl-3.0.txt

(C) 2018 K4YT3X
"""
from avalon_framework import Avalon
import json
import os

VERSION = '1.0.0'


def get_path(text):
    """ Get path and validate
    """
    while True:
        path = Avalon.gets(text)
        if os.path.isdir(path):
            return path
        Avalon.error('{} id not a directory / folder'.format(path))


def enroll_settings():
    settings = {}

    settings['waifu2x_path'] = get_path('waifu2x-caffe-cui.exe path: ')
    settings['ffmpeg_path'] = get_path('ffmpeg binaries directory: ')
    settings['ffmpeg_arguments'] = Avalon.gets('Extra arguments passed to ffmpeg: ')

    settings['ffmpeg_hwaccel'] = Avalon.gets('ffmpeg hardware acceleration method (cuda): ')
    if settings['ffmpeg_hwaccel'] == '':
        settings['ffmpeg_hwaccel'] = 'cuda'

    return settings


def write_config(settings):
    with open('video2x.json', 'w') as config:
        json.dump(settings, config, indent=2)
        config.close()


try:
    print('Video2X Config Generator {}'.format(VERSION))
    write_config(enroll_settings())
except KeyboardInterrupt:
    Avalon.warning('Exiting...')

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2x Config Generator
Author: K4YT3X
Date Created: October 23, 2018
Last Modified: March 4, 2019

Licensed under the GNU General Public License Version 3 (GNU GPL v3),
    available at: https://www.gnu.org/licenses/gpl-3.0.txt

(C) 2018-2019 K4YT3X
"""
from avalon_framework import Avalon
import json
import os

VERSION = '1.0.3'


def get_path(text):
    """ Get path and validate
    """
    while True:
        path = Avalon.gets(text)
        if os.path.isdir(path):
            return path
        Avalon.error('{} id not a directory/folder'.format(path))


def enroll_settings():
    settings = {}

    settings['waifu2x_path'] = get_path('waifu2x-caffe-cui.exe path: ')
    settings['ffmpeg_path'] = get_path('ffmpeg binaries directory: ')

    settings['ffmpeg_arguments'] = []
    while True:
        argument = Avalon.gets('Extra arguments passed to ffmpeg (empty=none): ')
        if argument:
            settings['ffmpeg_arguments'].append(argument)
        else:
            break

    settings['ffmpeg_hwaccel'] = Avalon.gets('ffmpeg hardware acceleration method (empty=auto): ')
    if settings['ffmpeg_hwaccel'] == '':
        settings['ffmpeg_hwaccel'] = 'auto'

    settings['video2x_cache_folder'] = Avalon.gets('Video2X cache folder (empty=system default): ')
    if settings['video2x_cache_folder'] == '':
        settings['video2x_cache_folder'] = False

    settings['preserve_frames'] = Avalon.ask('Preserve extracted or upscaled frames')

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

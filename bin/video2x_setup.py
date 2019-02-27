#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2X Setup Script
Author: K4YT3X
Date Created: November 28, 2018
Last Modified: February 26, 2019

Licensed under the GNU General Public License Version 3 (GNU GPL v3),
    available at: https://www.gnu.org/licenses/gpl-3.0.txt

(C) 2018-2019 K4YT3X

Description: This script helps installing all dependencies of video2x
and generates a configuration for it.

Installation Details:
- ffmpeg: %LOCALAPPDATA%\\video2x\\ffmpeg
- waifu2x-caffe: %LOCALAPPDATA%\\video2x\\waifu2x-caffe

"""
import json
import os
import subprocess
import tempfile
import traceback
import zipfile

# Requests doesn't come with windows, therefore
# it will be installed as a dependency and imported
# later in the script.
# import requests

VERSION = '1.0.2'


class Video2xSetup:
    """ Install dependencies for video2x video enlarger

    This library is meant to be executed as a stand-alone
    script. All files will be installed under %LOCALAPPDATA%\\video2x.
    """

    def __init__(self):
        self.trash = []

    def run(self):

        print('\nInstalling Python libraries')
        self._install_python_requirements()

        print('\nInstalling FFMPEG')
        self._install_ffmpeg()

        print('\nInstalling waifu2x-caffe')
        self._install_waifu2x_caffe()

        print('\nGenerating Video2X configuration file')
        self._generate_config()

        print('\nCleaning up temporary files')
        self._cleanup()

    def _install_python_requirements(self):
        """ Read requirements.txt and return its content
        """
        with open('requirements.txt', 'r') as req:
            for line in req:
                package = line.split(' ')[0]
                pip_install(package)

    def _cleanup(self):
        """ Cleanup all the temp files downloaded
        """
        for file in self.trash:
            try:
                print('Deleting: {}'.format(file))
                os.remove(file)
            except FileNotFoundError:
                pass

    def _install_ffmpeg(self):
        """ Install FFMPEG
        """
        latest_release = 'https://ffmpeg.zeranoe.com/builds/win64/static/ffmpeg-4.1-win64-static.zip'

        ffmpeg_zip = download(latest_release, tempfile.gettempdir())
        self.trash.append(ffmpeg_zip)

        with zipfile.ZipFile(ffmpeg_zip) as zipf:
            zipf.extractall('{}\\video2x'.format(os.getenv('localappdata')))

    def _install_waifu2x_caffe(self):
        """ Install waifu2x_caffe
        """
        import requests

        # Get latest release of waifu2x-caffe via GitHub API
        latest_release = json.loads(requests.get('https://api.github.com/repos/lltcggie/waifu2x-caffe/releases/latest').content)

        for a in latest_release['assets']:
            if 'waifu2x-caffe.zip' in a['browser_download_url']:
                waifu2x_caffe_zip = download(a['browser_download_url'], tempfile.gettempdir())
                self.trash.append(waifu2x_caffe_zip)

        with zipfile.ZipFile(waifu2x_caffe_zip) as zipf:
            zipf.extractall('{}\\video2x'.format(os.getenv('localappdata')))

    def _generate_config(self):
        """ Generate video2x config
        """
        settings = {}

        settings['waifu2x_path'] = '{}\\video2x\\waifu2x-caffe\\waifu2x-caffe-cui.exe'.format(os.getenv('localappdata'))
        settings['ffmpeg_path'] = '{}\\video2x\\ffmpeg-4.1-win64-static\\bin'.format(os.getenv('localappdata'))
        settings['ffmpeg_arguments'] = []
        settings['ffmpeg_hwaccel'] = 'auto'
        settings['extracted_frames'] = False
        settings['upscaled_frames'] = False
        settings['preserve_frames'] = False

        with open('video2x.json', 'w') as config:
            json.dump(settings, config, indent=2)
            config.close()


def download(url, save_path, chunk_size=4096):
    """ Download file to local with requests library
    """
    import requests

    output_file = '{}\\{}'.format(save_path, url.split('/')[-1])
    print('Downloading: {}'.format(url))
    print('Chunk size: {}'.format(chunk_size))
    print('Saving to: {}'.format(output_file))

    stream = requests.get(url, stream=True)

    # Write content into file
    with open(output_file, 'wb') as output:
        for chunk in stream.iter_content(chunk_size=chunk_size):
            if chunk:
                print('!', end='')
                output.write(chunk)
    print()

    return output_file


def pip_install(package):
    """ Install python package via python pip module

    pip.main() is not available after pip 9.0.1, thus
    pip module is not used in this case.
    """
    return subprocess.run(['python', '-m', 'pip', 'install', '-U', package]).returncode


if __name__ == "__main__":
    try:
        print('Video2x Setup Script')
        print('Version: {}'.format(VERSION))
        setup = Video2xSetup()
        setup.run()
        print('\n Script finished successfully')
    except Exception:
        traceback.print_exc()
        print('An error has occurred')
        print('Video2X Automatic Setup has failed')
        exit(1)

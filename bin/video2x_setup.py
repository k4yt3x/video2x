#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2X Setup Script
Author: K4YT3X
Date Created: November 28, 2018
Last Modified: June 15, 2019

Licensed under the GNU General Public License Version 3 (GNU GPL v3),
    available at: https://www.gnu.org/licenses/gpl-3.0.txt

(C) 2018-2019 K4YT3X

Description: This script helps installing all dependencies of video2x
and generates a configuration for it.

Installation Details:
- ffmpeg: %LOCALAPPDATA%\\video2x\\ffmpeg
- waifu2x-caffe: %LOCALAPPDATA%\\video2x\\waifu2x-caffe

"""
import argparse
import json
import os
import subprocess
import sys
import tempfile
import traceback
import zipfile

# Requests doesn't come with windows, therefore
# it will be installed as a dependency and imported
# later in the script.
# import requests

VERSION = '1.2.1'


def process_arguments():
    """Processes CLI arguments
    """
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    # video options
    general_options = parser.add_argument_group('General Options')
    general_options.add_argument('-d', '--driver', help='driver to download and configure', action='store', choices=['all', 'waifu2x_caffe', 'waifu2x_converter'], default='all')

    # parse arguments
    return parser.parse_args()


class Video2xSetup:
    """ Install dependencies for video2x video enlarger

    This library is meant to be executed as a stand-alone
    script. All files will be installed under %LOCALAPPDATA%\\video2x.
    """

    def __init__(self, driver, download_python_modules):
        self.driver = driver
        self.download_python_modules = download_python_modules
        self.trash = []

    def run(self):
        if self.download_python_modules:
            print('\nInstalling Python libraries')
            self._install_python_requirements()

        print('\nInstalling FFMPEG')
        self._install_ffmpeg()

        if self.driver == 'all':
            self._install_waifu2x_caffe()
            self._install_waifu2x_converter_cpp()
        elif self.driver == 'waifu2x_caffe':
            self._install_waifu2x_caffe()
        elif self.driver == 'waifu2x_converter':
            self._install_waifu2x_converter_cpp()

        print('\nGenerating Video2X configuration file')
        self._generate_config()

        print('\nCleaning up temporary files')
        self._cleanup()

    def _install_python_requirements(self):
        """ Read requirements.txt and return its content
        """
        with open('requirements.txt', 'r') as req:
            for line in req:
                package = line.split('==')[0]
                pip_install(package)

    def _cleanup(self):
        """ Cleanup all the temp files downloaded
        """
        for file in self.trash:
            try:
                print(f'Deleting: {file}')
                os.remove(file)
            except FileNotFoundError:
                pass

    def _install_ffmpeg(self):
        """ Install FFMPEG
        """
        latest_release = 'https://ffmpeg.zeranoe.com/builds/win64/static/ffmpeg-latest-win64-static.zip'

        ffmpeg_zip = download(latest_release, tempfile.gettempdir())
        self.trash.append(ffmpeg_zip)

        with zipfile.ZipFile(ffmpeg_zip) as zipf:
            zipf.extractall(os.path.join(os.getenv('localappdata'), 'video2x'))

    def _install_waifu2x_caffe(self):
        """ Install waifu2x_caffe
        """
        print('\nInstalling waifu2x-caffe')
        import requests

        # Get latest release of waifu2x-caffe via GitHub API
        latest_release = json.loads(requests.get('https://api.github.com/repos/lltcggie/waifu2x-caffe/releases/latest').content)

        for a in latest_release['assets']:
            if 'waifu2x-caffe.zip' in a['browser_download_url']:
                waifu2x_caffe_zip = download(a['browser_download_url'], tempfile.gettempdir())
                self.trash.append(waifu2x_caffe_zip)

        with zipfile.ZipFile(waifu2x_caffe_zip) as zipf:
            zipf.extractall(os.path.join(os.getenv('localappdata'), 'video2x'))

    def _install_waifu2x_converter_cpp(self):
        """ Install waifu2x_caffe
        """
        print('\nInstalling waifu2x-converter-cpp')
        import re
        import requests

        # Get latest release of waifu2x-caffe via GitHub API
        latest_release = json.loads(requests.get('https://api.github.com/repos/DeadSix27/waifu2x-converter-cpp/releases/latest').content)

        for a in latest_release['assets']:
            if re.search(r'waifu2x-DeadSix27-win64_v[0-9]*\.zip', a['browser_download_url']):
                waifu2x_converter_cpp_zip = download(a['browser_download_url'], tempfile.gettempdir())
                self.trash.append(waifu2x_converter_cpp_zip)

        with zipfile.ZipFile(waifu2x_converter_cpp_zip) as zipf:
            zipf.extractall(os.path.join(os.getenv('localappdata'), 'video2x', 'waifu2x-converter-cpp'))

    def _generate_config(self):
        """ Generate video2x config
        """
        # Open current video2x.json file as template
        with open('video2x.json', 'r') as template:
            template_dict = json.load(template)
            template.close()

        local_app_data = os.getenv('localappdata')

        # configure only the specified drivers
        if self.driver == 'all':
            template_dict['waifu2x_caffe']['waifu2x_caffe_path'] = os.path.join(local_app_data, 'video2x', 'waifu2x-caffe', 'waifu2x-caffe-cui.exe')
            template_dict['waifu2x_converter']['waifu2x_converter_path'] = os.path.join(local_app_data, 'video2x', 'waifu2x-converter-cpp')
        elif self.driver == 'waifu2x_caffe':
            template_dict['waifu2x_caffe']['waifu2x_caffe_path'] = os.path.join(local_app_data, 'video2x', 'waifu2x-caffe', 'waifu2x-caffe-cui.exe')
        elif self.driver == 'waifu2x_converter':
            template_dict['waifu2x_converter']['waifu2x_converter_path'] = os.path.join(local_app_data, 'video2x', 'waifu2x-converter-cpp')

        template_dict['ffmpeg']['ffmpeg_path'] = os.path.join(local_app_data, 'video2x', 'ffmpeg-latest-win64-static', 'bin')
        template_dict['video2x']['video2x_cache_directory'] = None
        template_dict['video2x']['preserve_frames'] = False

        # Write configuration into file
        with open('video2x.json', 'w') as config:
            json.dump(template_dict, config, indent=4)
            config.close()


def download(url, save_path, chunk_size=4096):
    """ Download file to local with requests library
    """
    from tqdm import tqdm
    import requests

    output_file = os.path.join(save_path, url.split('/')[-1])
    print(f'Downloading: {url}')
    print(f'Chunk size: {chunk_size}')
    print(f'Saving to: {output_file}')

    stream = requests.get(url, stream=True)
    total_size = int(stream.headers['content-length'])

    # Write content into file
    with open(output_file, 'wb') as output:
        with tqdm(total=total_size, ascii=True) as progress_bar:
            for chunk in stream.iter_content(chunk_size=chunk_size):
                if chunk:
                    output.write(chunk)
                    progress_bar.update(len(chunk))

    return output_file


def pip_install(package):
    """ Install python package via python pip module

    pip.main() is not available after pip 9.0.1, thus
    pip module is not used in this case.
    """
    return subprocess.run(['python', '-m', 'pip', 'install', '-U', package]).returncode


if __name__ == '__main__':
    try:
        args = process_arguments()
        print('Video2X Setup Script')
        print(f'Version: {VERSION}')

        # do not install pip modules if script
        # is packaged in exe format
        download_python_modules = True
        if sys.argv[0].endswith('.exe'):
            print('\nScript is packaged as exe, skipping pip module download')
            download_python_modules = False

        setup = Video2xSetup(args.driver, download_python_modules)
        setup.run()
        print('\nScript finished successfully')
    except Exception:
        traceback.print_exc()
        print('An error has occurred')
        print('Video2X Automatic Setup has failed')
        exit(1)

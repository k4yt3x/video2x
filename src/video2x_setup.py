#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2X Setup Script
Creator: K4YT3X
Date Created: November 28, 2018
Last Modified: January 4, 2020

Editor: BrianPetkovsek
Editor: SAT3LL

Description: This script helps installing all dependencies of video2x
and generates a configuration for it.

Installation Details:
- ffmpeg: %LOCALAPPDATA%\\video2x\\ffmpeg
- waifu2x-caffe: %LOCALAPPDATA%\\video2x\\waifu2x-caffe
- waifu2x-cpp-converter: %LOCALAPPDATA%\\video2x\\waifu2x-converter-cpp
- waifu2x_ncnn_vulkan: %LOCALAPPDATA%\\video2x\\waifu2x-ncnn-vulkan
- anime4k: %LOCALAPPDATA%\\video2x\\anime4k
"""

# built-in imports
from datetime import timedelta
import argparse
import contextlib
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time
import traceback
import urllib
import zipfile

# Requests doesn't come with windows, therefore
# it will be installed as a dependency and imported
# later in the script.
# import requests

VERSION = '1.6.1'

# global static variables
LOCALAPPDATA = pathlib.Path(os.getenv('localappdata'))
VIDEO2X_CONFIG = pathlib.Path(sys.argv[0]).parent.absolute() / 'video2x.yaml'
DRIVER_OPTIONS = ['all', 'waifu2x_caffe', 'waifu2x_converter', 'waifu2x_ncnn_vulkan', 'anime4k']


def parse_arguments():
    """Processes CLI arguments
    """
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    # video options
    general_options = parser.add_argument_group('General Options')
    general_options.add_argument('-d', '--driver', help='driver to download and configure', action='store', choices=DRIVER_OPTIONS, default='all')

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

        print('\nInstalling FFmpeg')
        self._install_ffmpeg()

        if self.driver == 'all':
            self._install_waifu2x_caffe()
            self._install_waifu2x_converter_cpp()
            self._install_waifu2x_ncnn_vulkan()
            self._install_anime4k()
        elif self.driver == 'waifu2x_caffe':
            self._install_waifu2x_caffe()
        elif self.driver == 'waifu2x_converter':
            self._install_waifu2x_converter_cpp()
        elif self.driver == 'waifu2x_ncnn_vulkan':
            self._install_waifu2x_ncnn_vulkan()
        elif self.driver == 'anime4k':
            self._install_anime4k()

        print('\nGenerating Video2X configuration file')
        self._generate_config()

        print('\nCleaning up temporary files')
        self._cleanup()

    def _install_python_requirements(self):
        """ Read requirements.txt and return its content
        """
        pip_install('requirements.txt')

    def _cleanup(self):
        """ Cleanup all the temp files downloaded
        """
        for file in self.trash:
            try:
                if file.is_dir():
                    print(f'Deleting directory: {file}')
                    shutil.rmtree(file)
                else:
                    print(f'Deleting file: {file}')
                    file.unlink()
            except Exception:
                print(f'Error deleting: {file}')
                traceback.print_exc()

    def _install_ffmpeg(self):
        """ Install FFMPEG
        """
        latest_release = 'https://ffmpeg.zeranoe.com/builds/win64/static/ffmpeg-latest-win64-static.zip'

        ffmpeg_zip = download(latest_release, tempfile.gettempdir())
        self.trash.append(ffmpeg_zip)

        with zipfile.ZipFile(ffmpeg_zip) as zipf:
            zipf.extractall(LOCALAPPDATA / 'video2x')

    def _install_waifu2x_caffe(self):
        """ Install waifu2x_caffe
        """
        print('\nInstalling waifu2x-caffe')
        import requests

        # Get latest release of waifu2x-caffe via GitHub API
        latest_release = requests.get('https://api.github.com/repos/lltcggie/waifu2x-caffe/releases/latest').json()

        for a in latest_release['assets']:
            if 'waifu2x-caffe.zip' in a['browser_download_url']:
                waifu2x_caffe_zip = download(a['browser_download_url'], tempfile.gettempdir())
                self.trash.append(waifu2x_caffe_zip)

        with zipfile.ZipFile(waifu2x_caffe_zip) as zipf:
            zipf.extractall(LOCALAPPDATA / 'video2x')

    def _install_waifu2x_converter_cpp(self):
        """ Install waifu2x_caffe
        """
        print('\nInstalling waifu2x-converter-cpp')
        import requests

        # Get latest release of waifu2x-caffe via GitHub API
        latest_release = requests.get('https://api.github.com/repos/DeadSix27/waifu2x-converter-cpp/releases/latest').json()

        for a in latest_release['assets']:
            if re.search(r'waifu2x-DeadSix27-win64_v[0-9]*\.zip', a['browser_download_url']):
                waifu2x_converter_cpp_zip = download(a['browser_download_url'], tempfile.gettempdir())
                self.trash.append(waifu2x_converter_cpp_zip)

        with zipfile.ZipFile(waifu2x_converter_cpp_zip) as zipf:
            zipf.extractall(LOCALAPPDATA / 'video2x' / 'waifu2x-converter-cpp')

    def _install_waifu2x_ncnn_vulkan(self):
        """ Install waifu2x-ncnn-vulkan
        """
        print('\nInstalling waifu2x-ncnn-vulkan')
        import requests

        # Get latest release of waifu2x-ncnn-vulkan via Github API
        latest_release = requests.get('https://api.github.com/repos/nihui/waifu2x-ncnn-vulkan/releases/latest').json()

        for a in latest_release['assets']:
            if re.search(r'waifu2x-ncnn-vulkan-\d*\.zip', a['browser_download_url']):
                waifu2x_ncnn_vulkan_zip = download(a['browser_download_url'], tempfile.gettempdir())
                self.trash.append(waifu2x_ncnn_vulkan_zip)

        # extract and rename
        waifu2x_ncnn_vulkan_directory = LOCALAPPDATA / 'video2x' / 'waifu2x-ncnn-vulkan'
        with zipfile.ZipFile(waifu2x_ncnn_vulkan_zip) as zipf:
            zipf.extractall(LOCALAPPDATA / 'video2x')

            # if directory already exists, remove it
            if waifu2x_ncnn_vulkan_directory.exists():
                shutil.rmtree(waifu2x_ncnn_vulkan_directory)

            # rename the newly extracted directory
            (LOCALAPPDATA / 'video2x' / zipf.namelist()[0]).rename(waifu2x_ncnn_vulkan_directory)

    def _install_anime4k(self):
        """ Install Anime4K
        """
        print('\nInstalling Anime4K')

        """
        import requests

        # get latest release of Anime4K via Github API
        # at the time of writing this portion, Anime4K doesn't yet have a stable release
        # therefore releases/latest won't work
        latest_release = requests.get('https://api.github.com/repos/bloc97/Anime4K/releases').json()[0]

        for a in latest_release['assets']:
            if 'Anime4K_Java.zip' in a['browser_download_url']:
                anime4k_zip = download(a['browser_download_url'], tempfile.gettempdir())
                self.trash.append(anime4k_zip)
        """

        # since Java pre-compiled release has been removed from download
        # page, we use this cached version as a temporary solution
        anime4k_zip = download('https://files.flexio.org/Resources/anime4k.zip', tempfile.gettempdir())
        self.trash.append(anime4k_zip)

        # extract and rename
        with zipfile.ZipFile(anime4k_zip) as zipf:
            zipf.extractall(LOCALAPPDATA / 'video2x' / 'anime4k')

    def _generate_config(self):
        """ Generate video2x config
        """
        import yaml

        # open current video2x configuration file as template
        with open(VIDEO2X_CONFIG, 'r') as template:
            template_dict = yaml.load(template, Loader=yaml.FullLoader)
            template.close()

        # configure only the specified drivers
        if self.driver == 'all':
            template_dict['waifu2x_caffe']['path'] = str(LOCALAPPDATA / 'video2x' / 'waifu2x-caffe' / 'waifu2x-caffe-cui.exe')
            template_dict['waifu2x_converter']['path'] = str(LOCALAPPDATA / 'video2x' / 'waifu2x-converter-cpp')
            template_dict['waifu2x_ncnn_vulkan']['path'] = str(LOCALAPPDATA / 'video2x' / 'waifu2x-ncnn-vulkan' / 'waifu2x-ncnn-vulkan.exe')
            template_dict['anime4k']['path'] = str(LOCALAPPDATA / 'video2x' / 'anime4k' / 'Anime4K.jar')
        elif self.driver == 'waifu2x_caffe':
            template_dict['waifu2x_caffe']['path'] = str(LOCALAPPDATA / 'video2x' / 'waifu2x-caffe' / 'waifu2x-caffe-cui.exe')
        elif self.driver == 'waifu2x_converter':
            template_dict['waifu2x_converter']['path'] = str(LOCALAPPDATA / 'video2x' / 'waifu2x-converter-cpp')
        elif self.driver == 'waifu2x_ncnn_vulkan':
            template_dict['waifu2x_ncnn_vulkan']['path'] = str(LOCALAPPDATA / 'video2x' / 'waifu2x-ncnn-vulkan' / 'waifu2x-ncnn-vulkan.exe')
        elif self.driver == 'anime4k':
            template_dict['anime4k']['path'] = str(LOCALAPPDATA / 'video2x' / 'anime4k' / 'Anime4K.jar')

        template_dict['ffmpeg']['ffmpeg_path'] = str(LOCALAPPDATA / 'video2x' / 'ffmpeg-latest-win64-static' / 'bin')
        template_dict['video2x']['video2x_cache_directory'] = None
        template_dict['video2x']['preserve_frames'] = False

        # write configuration into file
        with open(VIDEO2X_CONFIG, 'w') as config:
            yaml.dump(template_dict, config)


def download(url, save_path, chunk_size=4096):
    """ Download file to local with requests library
    """
    from tqdm import tqdm
    import requests

    save_path = pathlib.Path(save_path)

    # create target folder if it doesn't exist
    save_path.mkdir(parents=True, exist_ok=True)

    # create requests stream for steaming file
    stream = requests.get(url, stream=True, allow_redirects=True)

    # get file name
    file_name = None
    if 'content-disposition' in stream.headers:
        disposition = stream.headers['content-disposition']
        with contextlib.suppress(IndexError):
            file_name = re.findall("filename=(.+)", disposition)[0].strip('"')

    if file_name is None:
        # output_file = f'{save_path}\\{stream.url.split("/")[-1]}'
        output_file = save_path / stream.url.split('/')[-1]
    else:
        output_file = save_path / file_name

    # decode url encoding
    output_file = pathlib.Path(urllib.parse.unquote(str(output_file)))

    # get total size for progress bar if provided in headers
    total_size = 0
    if 'content-length' in stream.headers:
        total_size = int(stream.headers['content-length'])

    # print download information summary
    print(f'Downloading: {url}')
    print(f'Total size: {total_size}')
    print(f'Chunk size: {chunk_size}')
    print(f'Saving to: {output_file}')

    # Write content into file
    with open(output_file, 'wb') as output:
        with tqdm(total=total_size, ascii=True) as progress_bar:
            for chunk in stream.iter_content(chunk_size=chunk_size):
                if chunk:
                    output.write(chunk)
                    progress_bar.update(len(chunk))

    # return the full path of saved file
    return output_file


def pip_install(file):
    """ Install python package via python pip module

    pip.main() is not available after pip 9.0.1, thus
    pip module is not used in this case.
    """
    return subprocess.run([sys.executable, '-m', 'pip', 'install', '-U', '-r', file]).returncode


if __name__ == '__main__':
    try:
        # set default exit code
        EXIT_CODE = 0

        # get start time
        start_time = time.time()

        # check platform
        if sys.platform != 'win32':
            print('This script is currently only compatible with Windows')
            EXIT_CODE = 1
            sys.exit(1)

        # parse command line arguments
        args = parse_arguments()
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

    except SystemExit:
        pass

    # if PermissionError is raised
    # user needs to run this with higher privilege
    except PermissionError:
        traceback.print_exc()
        print('You might have insufficient privilege for this script to run')
        print('Try running this script with Administrator privileges')
        EXIT_CODE = 1

    # for any exception in the script
    except Exception:
        traceback.print_exc()
        print('An error has occurred')
        print('Video2X Automatic Setup has failed')

        # in case of a failure, try cleaning up temp files
        try:
            setup._cleanup()
        except Exception:
            traceback.print_exc()
            print('An error occurred while trying to cleanup files')

        EXIT_CODE = 1

    # regardless if script finishes successfully or not
    # print script execution summary
    finally:
        print('Script finished')
        print(f'Time taken: {timedelta(seconds=round(time.time() - start_time))}')
        input('Press [ENTER] to exit script')
        sys.exit(EXIT_CODE)

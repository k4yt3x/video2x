#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Name: Video2X Setup Script
Creator: K4YT3X
Date Created: November 28, 2018
Last Modified: May 30, 2020

Editor: BrianPetkovsek
Editor: SAT3LL

Description: This script helps installing all dependencies of video2x
and generates a configuration for it.

Installation Details:
- ffmpeg: %LOCALAPPDATA%\\video2x\\ffmpeg
- waifu2x-caffe: %LOCALAPPDATA%\\video2x\\waifu2x-caffe
- waifu2x-cpp-converter: %LOCALAPPDATA%\\video2x\\waifu2x-converter-cpp
- waifu2x_ncnn_vulkan: %LOCALAPPDATA%\\video2x\\waifu2x-ncnn-vulkan
- srmd_ncnn_vulkan: %LOCALAPPDATA%\\video2x\\srmd-ncnn-vulkan
- realsr_ncnn_vulkan: %LOCALAPPDATA%\\video2x\\realsr-ncnn-vulkan
- anime4kcpp: %LOCALAPPDATA%\\video2x\\anime4kcpp
"""

# built-in imports
from datetime import timedelta
import argparse
import contextlib
import os
import pathlib
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import traceback
import urllib
import zipfile

# Some libraries don't come with default Python installation.
# Therefore, they will be installed during the Python dependency
#   installation step and imported later in the script.

SETUP_VERSION = '2.2.1'

# global static variables
LOCALAPPDATA = pathlib.Path(os.getenv('localappdata'))
DRIVER_OPTIONS = ['all',
                  'ffmpeg',
                  'gifski',
                  'waifu2x_caffe',
                  'waifu2x_converter_cpp',
                  'waifu2x_ncnn_vulkan',
                  'srmd_ncnn_vulkan',
                  'realsr_ncnn_vulkan',
                  'anime4kcpp']


def parse_arguments():
    """ parse command line arguments
    """
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-d', '--driver', help='driver to download and configure', choices=DRIVER_OPTIONS, default='all')
    parser.add_argument('-u', '--uninstall', help='uninstall Video2X dependencies from default location', action='store_true')
    # parse arguments
    return parser.parse_args()


class Video2xSetup:
    """ install dependencies for video2x video enlarger

    This library is meant to be executed as a stand-alone
    script. All files will be installed under %LOCALAPPDATA%\\video2x.
    """

    def __init__(self, driver, download_python_modules):
        self.driver = driver
        self.download_python_modules = download_python_modules
        self.trash = []

    def run(self):
        # regardless of which driver to install
        # always ensure Python modules are installed and up-to-date
        if self.download_python_modules:
            print('\nInstalling Python libraries')
            self._install_python_requirements()

        # if all drivers are to be installed
        if self.driver == 'all':
            DRIVER_OPTIONS.remove('all')
            for driver in DRIVER_OPTIONS:
                getattr(self, f'_install_{driver}')()

        # install only the selected driver
        else:
            getattr(self, f'_install_{self.driver}')()

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
        print('\nInstalling FFmpeg')

        latest_release = 'https://ffmpeg.zeranoe.com/builds/win64/static/ffmpeg-latest-win64-static.zip'

        ffmpeg_zip = download(latest_release, tempfile.gettempdir())
        self.trash.append(ffmpeg_zip)

        with zipfile.ZipFile(ffmpeg_zip) as zipf:
            zipf.extractall(LOCALAPPDATA / 'video2x')

    def _install_gifski(self):
        print('\nInstalling Gifski')
        import requests

        # Get latest release of Gifski via Github API
        latest_release = requests.get('https://api.github.com/repos/ImageOptim/gifski/releases/latest').json()

        for a in latest_release['assets']:
            if re.search(r'gifski-.*\.tar\.xz', a['browser_download_url']):
                gifski_tar_gz = download(a['browser_download_url'], tempfile.gettempdir())
                self.trash.append(gifski_tar_gz)

        # extract and rename
        with tarfile.open(gifski_tar_gz) as archive:
            archive.extractall(LOCALAPPDATA / 'video2x' / 'gifski')

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
            if re.search(r'waifu2x-ncnn-vulkan-\d*-windows\.zip', a['browser_download_url']):
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

    def _install_srmd_ncnn_vulkan(self):
        """ Install srmd-ncnn-vulkan
        """
        print('\nInstalling srmd-ncnn-vulkan')
        import requests

        # Get latest release of srmd-ncnn-vulkan via Github API
        latest_release = requests.get('https://api.github.com/repos/nihui/srmd-ncnn-vulkan/releases/latest').json()

        for a in latest_release['assets']:
            if re.search(r'srmd-ncnn-vulkan-\d*-windows\.zip', a['browser_download_url']):
                srmd_ncnn_vulkan_zip = download(a['browser_download_url'], tempfile.gettempdir())
                self.trash.append(srmd_ncnn_vulkan_zip)

        # extract and rename
        srmd_ncnn_vulkan_directory = LOCALAPPDATA / 'video2x' / 'srmd-ncnn-vulkan'
        with zipfile.ZipFile(srmd_ncnn_vulkan_zip) as zipf:
            zipf.extractall(LOCALAPPDATA / 'video2x')

            # if directory already exists, remove it
            if srmd_ncnn_vulkan_directory.exists():
                shutil.rmtree(srmd_ncnn_vulkan_directory)

            # rename the newly extracted directory
            (LOCALAPPDATA / 'video2x' / zipf.namelist()[0]).rename(srmd_ncnn_vulkan_directory)

    def _install_realsr_ncnn_vulkan(self):
        """ Install realsr-ncnn-vulkan
        """
        print('\nInstalling realsr-ncnn-vulkan')
        import requests

        # Get latest release of realsr-ncnn-vulkan via Github API
        latest_release = requests.get('https://api.github.com/repos/nihui/realsr-ncnn-vulkan/releases/latest').json()

        for a in latest_release['assets']:
            if re.search(r'realsr-ncnn-vulkan-\d*-windows\.zip', a['browser_download_url']):
                realsr_ncnn_vulkan_zip = download(a['browser_download_url'], tempfile.gettempdir())
                self.trash.append(realsr_ncnn_vulkan_zip)

        # extract and rename
        realsr_ncnn_vulkan_directory = LOCALAPPDATA / 'video2x' / 'realsr-ncnn-vulkan'
        with zipfile.ZipFile(realsr_ncnn_vulkan_zip) as zipf:
            zipf.extractall(LOCALAPPDATA / 'video2x')

            # if directory already exists, remove it
            if realsr_ncnn_vulkan_directory.exists():
                shutil.rmtree(realsr_ncnn_vulkan_directory)

            # rename the newly extracted directory
            (LOCALAPPDATA / 'video2x' / zipf.namelist()[0]).rename(realsr_ncnn_vulkan_directory)

    def _install_anime4kcpp(self):
        """ Install Anime4KCPP
        """
        print('\nInstalling Anime4KCPP')

        import patoolib
        import requests

        # get latest release of Anime4KCPP via Github API
        # at the time of writing this portion, Anime4KCPP doesn't yet have a stable release
        # therefore releases/latest won't work
        latest_release = requests.get('https://api.github.com/repos/TianZerL/Anime4KCPP/releases/latest').json()

        for a in latest_release['assets']:
            if re.search(r'Anime4KCPP_CLI-.*-Win64-msvc\.7z', a['browser_download_url']):
                anime4kcpp_7z = download(a['browser_download_url'], tempfile.gettempdir())
                self.trash.append(anime4kcpp_7z)

        # if running in PyInstaller, add sys._MEIPASS\7z to path
        # this directory contains 7za.exe and its DLL files
        with contextlib.suppress(AttributeError):
            os.environ['PATH'] += f';{sys._MEIPASS}\\7z'

        # (LOCALAPPDATA / 'video2x' / 'anime4kcpp').mkdir(parents=True, exist_ok=True)
        # pyunpack.Archive(anime4kcpp_7z).extractall(LOCALAPPDATA / 'video2x' / 'anime4kcpp')
        if (LOCALAPPDATA / 'video2x' / 'anime4kcpp').exists():
            shutil.rmtree(LOCALAPPDATA / 'video2x' / 'anime4kcpp')
        patoolib.extract_archive(str(anime4kcpp_7z), outdir=str(LOCALAPPDATA / 'video2x' / 'anime4kcpp'))


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


if __name__ != '__main__':
    raise ImportError('video2x_setup cannot be imported')

try:
    # set default exit code
    EXIT_CODE = 0

    # get start time
    start_time = time.time()

    # check platform
    if platform.system() != 'Windows':
        print('This script is currently only compatible with Windows')
        EXIT_CODE = 1
        sys.exit(1)

    # parse command line arguments
    args = parse_arguments()
    print('Video2X Setup Script')
    print(f'Version: {SETUP_VERSION}')

    # uninstall video2x dependencies
    if args.uninstall:
        if input('Are you sure you want to uninstall all Video2X dependencies? [y/N]: ').lower() == 'y':
            try:
                print(f'Removing: {LOCALAPPDATA / "video2x"}')
                shutil.rmtree(LOCALAPPDATA / 'video2x')
                print('Successfully uninstalled all dependencies')
            except FileNotFoundError:
                print(f'Dependency folder does not exist: {LOCALAPPDATA / "video2x"}')
        else:
            print('Uninstallation aborted')

    # run installation
    else:
        # do not install pip modules if script
        # is packaged in exe format
        download_python_modules = True
        if sys.argv[0].endswith('.exe'):
            print('\nScript is packaged as exe, skipping pip module download')
            download_python_modules = False

        # create setup install instance and run installer
        setup = Video2xSetup(args.driver, download_python_modules)
        setup.run()

        print('\nScript finished successfully')

# let SystemExit signals pass through
except SystemExit as e:
    raise e

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

    # if the program is launched as an Windows PE file
    # it might be launched from double clicking
    # pause the window before it closes automatically
    if sys.argv[0].endswith('.exe'):
        input('Press [ENTER] to exit script')

    sys.exit(EXIT_CODE)

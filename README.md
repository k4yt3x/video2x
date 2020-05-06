![Master Branch Version](https://img.shields.io/badge/master-v4.0.0-9cf?style=flat-square)
![GitHub release (latest by date)](https://img.shields.io/github/v/release/k4yt3x/video2x?style=flat-square)
![GitHub All Releases](https://img.shields.io/github/downloads/k4yt3x/video2x/total?style=flat-square)
![GitHub](https://img.shields.io/github/license/k4yt3x/video2x?style=flat-square)
![Platforms](https://img.shields.io/badge/Platforms-Windows%20%7C%20Linux%20%7C%20macOS-blue?style=flat-square)
<img alt="Become a Patron!"
src="https://c5.patreon.com/external/logo/become_a_patron_button@2x.png"
href="https://www.patreon.com/bePatron?u=34970782" 
height=20 />

# Video2X Lossless Video Enlarger

### Official Discussion Group (Telegram): https://t.me/video2x

## Download Builds (Windows)

You can go to the [releases page](https://github.com/k4yt3x/video2x/releases) to download the latest builds of `Video2X`. The exe files will require no Python or Python module installation.

The **`full`** package provides all packages that will possibly be needed by `Video2X`, including `FFmpeg`, `waifu2x-caffe`, `waifu2x-converter-cpp`, `waifu2x-ncnn-vulkan`, `srmd-ncnn-vulkan` and `Anime4KCPP`. The config file (`video2x.yaml`) is also already configured for the environment. All you need to do is just to launch `video2x.exe`.

The **`light`** package provides only the most basic functions of `Video2X`. Only `video2x.exe`, `video2x_setup.exe` and `video2x.yaml` are included. To setup dependencies (e.g. `FFmpeg` and `Waifu2X`) automatically, simply launch `video2x_setup.exe`.

## Prerequisites

Component names that are **bolded** can be automatically downloaded and configured with the `video2x_setup.py` script.

1. Operating System: Windows / Linux
2. AMD GPU / Nvidia GPU
3. AMD GPU driver / Nvidia GPU driver / Nvidia CUDNN
4. [**FFmpeg**](https://ffmpeg.zeranoe.com/builds/)
5. One of the following drivers
   - [**waifu2x-caffe**](https://github.com/lltcggie/waifu2x-caffe/releases)
   - [**waifu2x-converter-cpp**](https://github.com/DeadSix27/waifu2x-converter-cpp/releases)
   - [**waifu2x-ncnn-vulkan**](https://github.com/nihui/waifu2x-ncnn-vulkan)
   - [**srmd-ncnn-vulkan**](https://github.com/nihui/srmd-ncnn-vulkan)
   - [**Anime4KCPP**](https://github.com/TianZerL/Anime4KCPP)

## Recent Changes

### 4.0.0 (May 5, 2020)

- Added internationalization support
  - Added language zh_CN (简体中文)
  - Language will change automatically according to system locale settings
- Added support for [Anime4KCPP](https://github.com/TianZerL/Anime4KCPP) in replacement for Anime4K (Java)
- Driver-specific settings can now be specified in the command line by specifying them after a `--`
- All driver-specific settings are parsed by the corresponding driver
- Modularized driver wrappers in Video2X
- Cleaned up some clutters in the code

### 3.2.0 (April 26, 2020)

- Added support for [SRMD-NCNN-Vulkan](https://github.com/nihui/srmd-ncnn-vulkan)

### 3.1.0 (February 26, 2020)

- Removed the redundant layer of multi-threading since multi-process has to be implemented for launching Windows PE files in sub-processes
- Added support for graceful exit upon `KeyboardInterrupt` or termination signals
- Other minor improvements such as replacing `' '.join(execute)` with `shlex.join(execute)`

### Setup Script 1.8.0 (May 5, 2020)

- Added support for Anime4KCPP

## Description

Video2X is an automation software based on waifu2x image enlarging engine. It extracts frames from a video, enlarge it by a number of times without losing any details or quality, keeping lines smooth and edges sharp.

For short: **Video2X enlarges your video without losing details**.

Watch for the sharper edges in this screenshot around the shadows:

![preview](https://user-images.githubusercontent.com/21986859/49412428-65083280-f73a-11e8-8237-bb34158a545e.png)

**You can also watch the YouTube video Demo: https://www.youtube.com/watch?v=PG94iPoeoZk**

Clip is from trailer of animated movie "千と千尋の神隠し". Copyright belongs to "株式会社スタジオジブリ (STUDIO GHIBLI INC.)". Will delete immediately if use of clip is in violation of copyright.

## Screenshots

### Video2X GUI

![Video2X GUI Screenshot](https://user-images.githubusercontent.com/21986859/81157039-19346b00-8f76-11ea-965f-4d7edf6a818e.png)

### Video2X CLI

![Video2X CLI Screenshot](https://user-images.githubusercontent.com/21986859/81039711-4fe88380-8e99-11ea-9846-175f72100a76.png)

---

## Documentations

### [Video2X Wiki](https://github.com/k4yt3x/video2x/wiki)

You can find all detailed user-facing and developer-facing documentations in the [Video2X Wiki](https://github.com/k4yt3x/video2x/wiki). It covers everything from step-by-step instructions for beginners, to the code structure of this program for advanced users and developers. If this README page doesn't answer all your questions, the wiki page is where you should head to.

### [Step-By-Step Tutorial](https://github.com/k4yt3x/video2x/wiki/Step-By-Step-Tutorial)

For those who want a detailed walk-through of how to use `Video2X`, you can head to the [Step-By-Step Tutorial](https://github.com/k4yt3x/video2x/wiki/Step-By-Step-Tutorial) wiki page. It includes almost every step you need to perform in order to enlarge your first video.

### [Drivers](https://github.com/k4yt3x/video2x/wiki/Drivers)

Go to the [Drivers](https://github.com/k4yt3x/video2x/wiki/Drivers) wiki page if you want to see a detailed description on the different types of drivers implemented by `Video2X`. This wiki page contains detailed difference between different drivers, and how to download and set each of them up for `Video2X`.

### [Q&A](https://github.com/k4yt3x/video2x/wiki/Q&A)

If you have any questions, first try visiting our [Q&A](https://github.com/k4yt3x/video2x/wiki/Q&A) page to see if your question is answered there. If not, open an issue and we will respond to your questions ASAP.

---

## Quick Start

### Prerequisites

- **Python 3.8**
Download: https://www.python.org/downloads/windows/
- **FFmpeg Windows Build**
Download: https://ffmpeg.org/download.html
- **waifu2x-caffe** (designed for Nvidia CUDA/cuDNN)
Download: https://github.com/lltcggie/waifu2x-caffe/releases
- **waifu2x-converter-cpp**
Download: https://github.com/DeadSix27/waifu2x-converter-cpp/releases
- **waifu2x-ncnn-vulkan**
Download: https://github.com/nihui/waifu2x-ncnn-vulkan/releases
- **Anime4KCPP**
Download: https://github.com/TianZerL/Anime4KCPP/releases
- **srmd-ncnn-vulkan**
Download: https://github.com/nihui/srmd-ncnn-vulkan/releases

### Installing Dependencies

First, clone the video2x repository.

```shell
git clone https://github.com/k4yt3x/video2x.git
cd video2x/src
```

Then you may run the `video2x_setup.py` script to install and configure the dependencies automatically. This script is designed and tested on Windows 10.

This script will install the newest version of `ffmpeg`, and all upscaling drivers to `%LOCALAPPDATA%\\video2x` and all required python libraries.

```shell
python video2x_setup.py
```

Alternatively, you can also install the dependencies manually. Please refer to the prerequisites section to see what's needed.

Then you'll need to install python dependencies before start using video2x. Install simply by executing the following command.

```shell
pip install -r requirements.txt
```

### Sample Videos

If you can't find a video clip to begin with, or if you want to see a before-after comparison, we have prepared some sample clips for you. The quick start guide down below will also be based on the name of the sample clips.

![sample_video](https://user-images.githubusercontent.com/21986859/52905766-d5512b00-3236-11e9-9aea-077636539679.png)

- [Sample Video Original (240P) 1.7MB](https://files.k4yt3x.com/Resources/Videos/sample_input.mp4)
- [Sample Video Upscaled (1080P) 4.8MB](https://files.k4yt3x.com/Resources/Videos/sample_output.mp4)

Clip is from anime "さくら荘のペットな彼女". Copyright belongs to "株式会社アニプレックス (Aniplex Inc.)". Will delete immediately if use of clip is in violation of copyright.

### Basic Upscale Example

This example command below uses `waifu2x-caffe` to enlarge the video `sample-input.mp4` two double its original size.

```shell
python video2x.py -i sample-input.mp4 -o sample-output.mp4 -r 2 -d waifu2x_caffe
```

### Advanced Upscale Example

If you would like to tweak engine-specific settings, either specify the corresponding argument after `--`, or edit the corresponding field in the configuration file `video2x.yaml`. **Command line arguments will overwrite default values in the config file.**

This example below adds enables TTA for `waifu2x-caffe`.

```shell
python video2x.py -i sample-input.mp4 -o sample-output.mp4 -r 2 -d waifu2x_caffe -- --tta 1
```

To see a help page for driver-specific settings, use `-d` to select the driver and append `-- --help` as demonstrated below. This will print all driver-specific settings and descriptions.

```shell
python video2x.py -d waifu2x_caffe -- --help
```

---

# Full Usage

## General Options:

### -h, --help
    show this help message and exit

### -i INPUT, --input INPUT
    source video file/directory

### -o OUTPUT, --output OUTPUT
    output video file/directory

### -c CONFIG, --config CONFIG
    video2x config file path

### -d {waifu2x_caffe,waifu2x_converter_cpp,waifu2x_ncnn_vulkan,srmd_ncnn_vulkan,anime4kcpp}, --driver {waifu2x_caffe,waifu2x_converter_cpp,waifu2x_ncnn_vulkan,srmd_ncnn_vulkan,anime4kcpp}
    upscaling driver (default: waifu2x_caffe)

### -p PROCESSES, --processes PROCESSES
    number of processes to use for upscaling (default: 1)

### -v, --version
    display version, lawful information and exit

## Scaling Options

### --width WIDTH
    output video width

### --height HEIGHT
    output video height

### -r RATIO, --ratio RATIO
    scaling ratio

---

## License

Licensed under the GNU General Public License Version 3 (GNU GPL v3)
https://www.gnu.org/licenses/gpl-3.0.txt

![GPLv3 Icon](https://www.gnu.org/graphics/gplv3-127x51.png)

(C) 2018-2020 K4YT3X

## Credits

This project relies on the following software and projects.

- [FFmpeg](https://www.ffmpeg.org/)
- [waifu2x-caffe](https://github.com/lltcggie/waifu2x-caffe)
- [waifu2x-converter-cpp](https://github.com/DeadSix27/waifu2x-converter-cpp)
- [waifu2x-ncnn-vulkan](https://github.com/nihui/waifu2x-ncnn-vulkan)
- [srmd-ncnn-vulkan](https://github.com/nihui/srmd-ncnn-vulkan)
- [Anime4KCPP](https://github.com/TianZerL/Anime4KCPP)

## Special Thanks

Appreciations given to the following code contributors:

- @BrianPetkovsek
- @SAT3LL

## Related Projects

- [Dandere2x](https://github.com/CardinalPanda/dandere2x): A lossy video upscaler also built around `waifu2x`, but with video compression techniques to shorten the time needed to process a video.
- [Waifu2x-Extension-GUI](https://github.com/AaronFeng753/Waifu2x-Extension-GUI): A similar project that focuses more and only on building a better graphical user interface. It is built using C++ and Qt5, and currently only supports the Windows platform.

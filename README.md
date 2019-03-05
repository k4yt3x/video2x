# Video2X Lossless Video Enlarger

### Official Discussion Group (Telegram): https://t.me/video2x

## Prerequisites

Component names that are **bolded** are mandatory.

Component names that are *italicized* can be automatically downloaded and configured with the `video2x_setup.py` script.

1. Operating System: Windows
1. AMD GPU / Nvidia GPU
1. AMD GPU driver / Nvidia GPU driver / Nvidia CUDNN
1. [***FFMPEG***](https://ffmpeg.zeranoe.com/builds/)
1. [***waifu2x-caffe***](https://github.com/lltcggie/waifu2x-caffe/releases) / [**waifu2x-converter-cpp**](https://github.com/DeadSix27/waifu2x-converter-cpp/releases)

## Recent Changes

### 2.5.0 (March 4, 2019)

- Added progress bar according to @ArmandBernard 's suggestion.
- Cache folders are now created under `'{}\\video2x'.format(tempfile.gettempdir())` by default, and the default key for cache folder location in configuration file has been changed to `video2x_cache_folder`. This makes it easier to manage cache folders.
- Cache folders are now more likely to be deleted successfully even if the program crashes. This is made possible with more try-catch blocks.
- Updated FFMPEG stream migration grammar according to @cr08 's suggestions. All streams (including attachment streams) will now be copied over to the new video.
- A lot of minor upgrades and adjustments.

### 2.4.3 (February 26, 2019)

- Fixed the bug where ffmpeg arguments (hwaccel) are not passed on to ffmpeg.

### 2.4.2 (February 26, 2019)

- Added the function to detect insufficient GPU memory.
- Added the function to create temporary folders automatically.
- Fixed configuration file path error.

## Description

Video2X is an automation software based on waifu2x image enlarging engine. It extracts frames from a video, enlarge it by a number of times without losing any details or quality, keeping lines smooth and edges sharp.

For short: **Video2X enlarges your video without losing details**

Watch for the sharper edges in this screenshot around the shadows:

![preview](https://user-images.githubusercontent.com/21986859/49412428-65083280-f73a-11e8-8237-bb34158a545e.png)

**You can also watch the YouTube video Demo: https://www.youtube.com/watch?v=PG94iPoeoZk**

Clip is from trailer of animated movie "千と千尋の神隠し". Copyright belongs to "株式会社スタジオジブリ (STUDIO GHIBLI INC.)". Will delete immediately if use of clip is in violation of copyright.

## Screenshot
![screenshot](https://user-images.githubusercontent.com/21986859/40265170-39c0caae-5b01-11e8-8371-8b6c24769639.png)

## Installation

### Prerequisites

- **Python 3**  
Download: https://www.python.org/downloads/windows/
- **FFMPEG Windows Build**  
Download: https://ffmpeg.org/download.html  
- **waifu2x-caffe for Windows**  
Download: https://github.com/lltcggie/waifu2x-caffe/releases
- **waifu2x-converter-cpp**  
Download: https://github.com/DeadSix27/waifu2x-converter-cpp/releases

### Install Dependencies

First, clone the video2x repository.

```bash
$ git clone https://github.com/K4YT3X/video2x.git
$ cd video2x/bin
```

Then you may run the `video2x_setup.py` script to install and configure the depencies automatically. This script is designed and tested on Windows 10.

This script will install `ffmpeg`, `waifu2x-caffe` to `%LOCALAPPDATA%\\video2x` and all python libraries.

```bash
$ python video2x_setup.py
```

Alternatively, you can also install the dependencies manually. Please refer to the prerequisites section to see what's needed.

Then you'll need to install python dependencies before start using video2x. Install simply by executing the following command.

```bash
$ pip install -r requirements.txt
```

## Quick Start

### Sample Videos

If you can't find a video clip to begin with, or if you want to see a before-after comparison, we have prepared some sample clips for you. The quick start guide down below will also be based on the name of the sample clips.

![sample_video](https://user-images.githubusercontent.com/21986859/52905766-d5512b00-3236-11e9-9aea-077636539679.png)

- [Sample Video Original (240P) 1.7MB](https://files.flexio.org/Resources/Videos/sample_input.mp4)
- [Sample Video Upscaled (1080P) 4.8MB](https://files.flexio.org/Resources/Videos/sample_output.mp4)

Clip is from anime "さくら荘のペットな彼女". Copyright belongs to "株式会社アニプレックス (Aniplex Inc.)". Will delete immediately if use of clip is in violation of copyright.

### For Command Line Beginners

If you're unfamiliar of directories in command lines, then here's a short section that might help you to get started.

For example, if you downloaded the sample input video to `C:\Users\[YourUsername]\Downloads`, then the full path of your input video will be `C:\Users\[YourUsername]\Downloads\sample_input.mp4`, vice versa. The output path is also relative. If you want to export the output video to the current directory, just specify the output video name such as `output.mp4`. However, if you want to put the output video in a different directory, you should use relative or absolute path, such as `C:\Users\[YourUsername]\Desktop\output.mp4`.

If you're tired typing everything in, you can also drag the video file directly into the command line window, and Windows will fill in the full path of the video for you.

### Nvidia CUDA (waifu2x-caffe)

Enlarge the video to 1920x1080 using CUDA. You may also use the `-r/--ratio` option.

```bash
$ python video2x.py -i sample_input.mp4 -o sample_output.mp4 -m gpu --width=1920 --height=1080
```

### Nvidia CNDNN

Enlarge the video to 1920x1080 using CUDNN. You may also use the `-r/--ratio` option.

```bash
$ python video2x.py -i sample_input.mp4 -o sample_output.mp4 -m cudnn --width=1920 --height=1080
```

### AMD or Nvidia (waifu2x-converter-cpp OpenCL)

Enlarge the video by 2 times using OpenCL. Note that `waifu2x-converter-cpp` doesn't support width and height.

```bash
$ python video2x.py -i sample_input.mp4 -o sample_output.mp4 -m gpu -r 2
```

### CPU

Enlarge the video to 1920x1080 using the CPU. You may also use the `-r/--ratio` option. This is potentially much slower than using a GPU.

```bash
$ python video2x.py -i sample_input.mp4 -o sample_output.mp4 -m cpu --width=1920 --height=1080
```

# Full Usage

## General Options

### -h, --help
    show this help message and exit

## File Input and Output

### -i INPUT, --input INPUT
    Specify source video file/directory

### -o OUTPUT, --output OUTPUT
    Specify output video file/directory

## Upscaler Options

### -m {cpu,gpu,cudnn}, --method {cpu,gpu,cudnn}
    Specify upscaling method

### -d {waifu2x_caffe,waifu2x_converter}, --driver {waifu2x_caffe,waifu2x_converter}
    Waifu2x driver

### -y {upconv_7_anime_style_art_rgb,upconv_7_photo,anime_style_art_rgb,photo,anime_style_art_y}, --model_type {upconv_7_anime_style_art_rgb,upconv_7_photo,anime_style_art_rgb,photo,anime_style_art_y}
    Specify model to use

### -t THREADS, --threads THREADS
    Specify number of threads to use for upscaling

### -c CONFIG, --config CONFIG
    Manually specify config file

### --width WIDTH
    Output video width

### --height HEIGHT
    Output video height

### -r RATIO, --ratio RATIO
    Scaling ratio

## License

Licensed under the GNU General Public License Version 3 (GNU GPL v3)
https://www.gnu.org/licenses/gpl-3.0.txt

![GPLv3 Icon](https://www.gnu.org/graphics/gplv3-127x51.png)

(C) 2018-2019 K4YT3X


## Credits

This project relies on the following software and projects.

- [FFMPEG]('https://www.ffmpeg.org/')
- [waifu2x-caffe](https://github.com/lltcggie/waifu2x-caffe)
- [waifu2x-converter-cpp](https://github.com/DeadSix27/waifu2x-converter-cpp)
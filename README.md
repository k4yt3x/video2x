# Video2X Video Enlarger

### This software is currently designed for Windows.

## 2.2.1 (February 1, 2019)

- Fixed AAC codec error discovered by @meguerreroa

## 2.2.0 (December 21, 2018)

- Rewritten main file to organize project structure. All executables have been moved into the `bin` folder.
- Bulk enlarge videos in a folder function has been added.
- Rewritten command line arguments parser to make arguments more clear.
- Other minor improvements.

## Setup Script (November 29, 2018)

- Added setup script. Now you can install dependencies and generate video2x configuraiton automatically by running the `video2x_setup.py` script.

## Description

Video2X is an automation software based on waifu2x image enlarging engine. It extracts frames from a video, enlarge it by a number of times without losing any details or quality, keeping lines smooth and edges sharp.

For short: **Video2X enlarges your video without losing details**

Watch for the sharper edges in this screenshot around the shadows:

[![preview](https://user-images.githubusercontent.com/21986859/49412428-65083280-f73a-11e8-8237-bb34158a545e.png)

**Or you can watch the YouTube video Demo: https://www.youtube.com/watch?v=PG94iPoeoZk**

## Screenshot
![screenshot](https://user-images.githubusercontent.com/21986859/40265170-39c0caae-5b01-11e8-8371-8b6c24769639.png)

</br>

## Installation

### Prerequisites

- **Python 3**  
Download: https://www.python.org/downloads/windows/
- **FFMPEG Windows Build**  
Download: https://ffmpeg.org/download.html  
- **waifu2x-caffe for Windows**  
Download: https://github.com/lltcggie/waifu2x-caffe/releases

### Install Dependencies

You can run the `video2x_setup.py` script to install and configure the depencies automatically. This script is designed and tested on Windows 10.

This script will install `ffmpeg`, `waifu2x-caffe` to `%LOCALAPPDATA%\\video2x` and all python libraries.

```bash
$ python bin/video2x_setup.py
```

After downloading the dependencies, clone the video2x package.

```bash
$ git clone https://github.com/K4YT3X/video2x.git
$ cd video2x/bin
```
Then you'll need to install python dependencies before start using video2x. Install simply by executing the following command.

```bash
$ pip install -r requirements.txt
```

## Quick Start

To enlarge a video on a computer with NVIDIA GPU

```bash
$ python video2x.py -i video.mp4 -o video.mp4 -m gpu --width=1920 --height=1080
```

To enlarge a video on a computer with CPU

```bash
$ python video2x.py -i video.mp4 -o video.mp4 -m cpu --width=1920 --height=1080
```


## Full Usage
```
usage: video2x.py [-h] -i INPUT -o OUTPUT -m {cpu,gpu,cudnn}
                  [-y {upconv_7_anime_style_art_rgb,upconv_7_photo,anime_style_art_rgb,photo,anime_style_art_y}]
                  [-t THREADS] [-c CONFIG] [--width WIDTH] [--height HEIGHT]
                  [-f FACTOR]

optional arguments:
  -h, --help            show this help message and exit

Basic Options:
  -i INPUT, --input INPUT
                        Specify source video file/directory
  -o OUTPUT, --output OUTPUT
                        Specify output video file/directory
  -m {cpu,gpu,cudnn}, --method {cpu,gpu,cudnn}
                        Specify upscaling method
  -y {upconv_7_anime_style_art_rgb,upconv_7_photo,anime_style_art_rgb,photo,anime_style_art_y}, --model_type {upconv_7_anime_style_art_rgb,upconv_7_photo,anime_style_art_rgb,photo,anime_style_art_y}
                        Specify model to use
  -t THREADS, --threads THREADS
                        Specify number of threads to use for upscaling
  -c CONFIG, --config CONFIG
                        Manually specify config file

Scaling Options:
  --width WIDTH         Output video width
  --height HEIGHT       Output video height
  -f FACTOR, --factor FACTOR
                        Factor to upscale the videos by

```

This project is based on the following softwares and projects.
- [FFMPEG]('https://www.ffmpeg.org/')
- [waifu2x caffe](https://github.com/lltcggie/waifu2x-caffe)

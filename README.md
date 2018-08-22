# Video2X Video Enlarger

### This software is currently designed for Windows.

## 2.0.2 (Aug 10, 2018)
- Changed enlarging mechanism from factor to resolution
- Fixed audio merging errors

## Description

Video2X is an automation software based on waifu2x image enlarging engine. It extracts frames from a video, enlarge it by a number of times without losing any details or quality, keeping lines smooth and edges sharp.

For short: **Video2X enlarges your video without losing details**

Watch for the sharper edges in this screenshot around the shadows:
![snapshot](https://user-images.githubusercontent.com/18014964/36638068-19cdb78c-19b8-11e8-8dfb-406b7015d30c.png)

**Or you can watch the YouTube video Demo: https://www.youtube.com/watch?v=PG94iPoeoZk**

## Screenshot
![screenshot](https://user-images.githubusercontent.com/21986859/40265170-39c0caae-5b01-11e8-8371-8b6c24769639.png)

</br>

## Installation

### Prerequisites

- **FFMPEG Windows Build**  
Download: https://ffmpeg.org/download.html  
- **waifu2x-caffe for Windows**  
Download: https://github.com/lltcggie/waifu2x-caffe/releases


After downloading the dependencies, clone the video2x package.

```bash
$ git clone https://github.com/K4YT3X/video2x.git
$ cd video2x
```
Then you'll need to install python dependencies before start using video2x. Install simply by executing the following command.

```bash
$ pip install -r requirements.txt
```


## Quick Start

To enlarge a video on a computer with NVIDIA GPU

```bash
$ python video2x.py -v VIDEO_FILE -o OUTPUT_FILENAME --width OUTPUT_WIDTH --height OUTPUT_HEIGHT --gpu
```

To enlarge a video on a computer without NVIDIA GPU

```bash
$ python video2x.py -v VIDEO_FILE -o OUTPUT_FILENAME --width OUTPUT_WIDTH --height OUTPUT_HEIGHT --cpu
```


## Full Usage
```
usage: video2x.py [-h] [--width WIDTH] [--height HEIGHT] [-v VIDEO]
                  [-o OUTPUT] [-y MODEL_TYPE] [--cpu] [--gpu] [--cudnn]

optional arguments:
  -h, --help            show this help message and exit

Controls:
  --width WIDTH         Output video width
  --height HEIGHT       Output video height
  -v VIDEO, --video VIDEO
                        Specify source video file
  -o OUTPUT, --output OUTPUT
                        Specify output file
  -y MODEL_TYPE, --model_type MODEL_TYPE
                        Specify model to use
  --cpu                 Use CPU for enlarging
  --gpu                 Use GPU for enlarging
  --cudnn               Use CUDNN for enlarging
```

This project is based on the following softwares and projects.
- [FFMPEG]('https://www.ffmpeg.org/')
- [waifu2x caffe](https://github.com/lltcggie/waifu2x-caffe)

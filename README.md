# Video2X Video Enlarger

**Current Version: 2.0 beta**

## 2.0 Beta Changes
1. Fixed contents meant for demonstration
1. Fixed audio issues
1. Better CUI
1. Now using absolute path for every file
1. Added progress bar for showing waifu2x enlarging progress

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
```
$ git clone https://github.com/K4YT3X/video2x.git
$ cd video2x
```

## Quick Start

To enlarge a video on a computer with NVIDIA GPU
```
$ python video2x.py -v VIDEO_FILE -o OUTPUT_FILENAME -f TIMES_TO_ENLARGE --gpu
```

To enlarge a video on a computer without NVIDIA GPU
```
$ python video2x.py -v VIDEO_FILE -o OUTPUT_FILENAME -f TIMES_TO_ENLARGE --cpu
```


## Full Usage
```
usage: video2x.py [-h] [-f FACTOR] [-v VIDEO] [-o OUTPUT] [-y MODEL_TYPE]
                  [--cpu] [--gpu] [--cudnn]

optional arguments:
  -h, --help            show this help message and exit

Controls:
  -f FACTOR, --factor FACTOR
                        Factor to enlarge video by
  -v VIDEO, --video VIDEO
                        Specify video file
  -o OUTPUT, --output OUTPUT
                        Specify output file
  -y MODEL_TYPE, --model_type MODEL_TYPE
                        Specify model to use
  --cpu                 Use CPU for enlarging
  --gpu                 Use GPU for enlarging
  --cudnn               Use CUDNN for enlarging
```
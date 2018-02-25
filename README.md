# Video2X Video Enlarger
[![Join the chat at https://gitter.im/K4YT3X-DEV/SCUTUM](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/K4YT3X-DEV/SCUTUM?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![status](https://travis-ci.org/K4YT3X/SCUTUM.svg)](https://travis-ci.org/K4YT3X/SCUTUM)
## Description

Video2X is an automation software based on waifu2x image enlarging engine. It extracts frames from a video, enlarge it by a number of times without losing any details or quality, keeping lines smooth and edges sharp.

Watch for the sharper edges in this screenshot around the shadows
![vlcsnap-2018-02-24-23h09m19s297](https://user-images.githubusercontent.com/18014964/36638068-19cdb78c-19b8-11e8-8dfb-406b7015d30c.png)

## Video Demo (YouTube)

<p align="center">
<a href="https://www.youtube.com/watch?v=PG94iPoeoZk">
<img border="0" alt="Video2X Demo" src="https://img.youtube.com/vi/PG94iPoeoZk/0.jpg" width="480" height="360">
</a>
</p>

## Installation

### Prerequisites

+ **FFMPEG Windows Build**  
Download: https://ffmpeg.org/download.html  
+ **waifu2x-caffe for Windows**  
Download: https://github.com/lltcggie/waifu2x-caffe/releases


After downloading the dependencies, clone the video2x package.
~~~~
$ git clone https://github.com/K4YT3X/video2x.git
$ cd video2x
~~~~

Now follow the in-line description and modify **FFMPEG_PATH** and **WAIFU2X_PATH**.  
You can also change **FOLDERIN** and **FOLDERIN** if you want.

<br>

## Quick Start

To enlarge a video on a computer with NVIDIA GPU
~~~~
$ python video2x.py -v VIDEO_FILE -o OUTPUT_FILENAME -f TIMES_TO_ENLARGE --gpu
~~~~

To enlarge a video on a computer without NVIDIA GPU
~~~~
$ python video2x.py -v VIDEO_FILE -o OUTPUT_FILENAME -f TIMES_TO_ENLARGE --cpu
~~~~

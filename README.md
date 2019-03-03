# Video2X Lossless Video Enlarger

### Official Discussion Group (Telegram): https://t.me/video2x

### Prerequisites

Component names that are **bolded** are mandatory.

Component names that are *italicized* can be automatically downloaded and configured with the `video2x_setup.py` script.

1. Operating System: Windows
1. AMD GPU / Nvidia GPU
1. AMD GPU driver / Nvidia GPU driver / Nvidia CUDNN
1. [***FFMPEG***](https://ffmpeg.zeranoe.com/builds/)
1. [***waifu2x-caffe***](https://github.com/lltcggie/waifu2x-caffe/releases) / [**waifu2x-converter-cpp**](https://github.com/DeadSix27/waifu2x-converter-cpp/releases)

## 2.4.3 (February 26, 2019)

- Fixed the bug where ffmpeg arguments (hwaccel) are not passed on to ffmpeg

## 2.4.2 (February 26, 2019)

- Added the function to detect insufficient GPU memory
- Added the function to create temporary folders automatically
- Fixed configuration file path error

## 2.4.1 (February 21, 2019)

- Video2X will now migrate all the audio tracks and subtitles to the output video.

## 2.4.0 (February 8, 2019)

- **Added AMD Support**. You can now use `-d/--driver waifu2x_converter` to specify the waifu2x driver to be `waifu2x-converter-cpp`. Note that you'll have to download and configure [waifu2x-converter-cpp](https://github.com/DeadSix27/waifu2x-converter-cpp/releases) first.

## Description

Video2X is an automation software based on waifu2x image enlarging engine. It extracts frames from a video, enlarge it by a number of times without losing any details or quality, keeping lines smooth and edges sharp.

For short: **Video2X enlarges your video without losing details**

Watch for the sharper edges in this screenshot around the shadows:

![preview](https://user-images.githubusercontent.com/21986859/49412428-65083280-f73a-11e8-8237-bb34158a545e.png)

**You can also watch the YouTube video Demo: https://www.youtube.com/watch?v=PG94iPoeoZk**

Clip is from trailer of animated movie "千と千尋の神隠し". Copyright belongs to "株式会社スタジオジブリ (STUDIO GHIBLI INC.)". Will delete immediately if use of clip is in violation of copyright.

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

### Step-by-Step Installation

First of all, you need Python to be installed on your OS. Go to [python.org](https://www.python.org/downloads/) and get it. Installer may ask you about putting python into your PATH, you should agree. After installation run command prompt and type:

```python```

Press \[ENTER\] to submit your input. If you did everything right you'll see output like that:

```
Python 3.7.2 (tags/v3.7.2:9a3ffc0492, Dec 23 2018, 22:20:52) [MSC v.1916 32 bit (Intel)] on win32
Type "help", "copyright", "credits" or "license" for more information.
>>>
```

Tripple angle brackets means that python interpreter awaits your input. We don't need it now, so exit it by typing:

```exit()```

Now you should be return in command prompt. Next step is installing video2x and its dependesies. If you not familiar with command-line interface of git (I suppose you are) just download as zip archive from green button at the top of this page. Unpack it in somewhere you want to. For example, at your HOME directory (`C:\Users\[your_username]\`). Next, return to the command prompt. Change your working directory to that place where you unpacked video2x before.

For HOME directory: 

```cd  video2x\bin```

For any other location:

```cd [absolute_path_to_unpacked_video2x]\bin```

As you can see, we changed directory to `bin\` sub-folder in video2x. To be sure that you did it right, type:

```dir```

This command lists files and folders in current working directory. If everything is OK and you see such files as "video2x.py" or "video2x_setup.py", you can continue:

```python video2x_setup.py```

Wait while python installed all needed components. Some issues may appear in installation process. If it completed with failure run command prompt with administrator rights and repeat previos steps (except Python installation). If it succesed, my congratulations - you may start to use video2x.

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

If you would like to interrupt process of video enlarging use CTRL+C.

Use \[TAB\] for command-line completion. This may be useful for avoiding incorrect paths or commands.

If your command is too long and you would like go to new line and continue there just complete it with "\\" symbol at the end (without double quotes), press \[ENTER\] and continue to type your command.

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

## Credits

This project is based on the following software and projects.

- [FFMPEG]('https://www.ffmpeg.org/')
- [waifu2x-caffe](https://github.com/lltcggie/waifu2x-caffe)
- [waifu2x-converter-cpp](https://github.com/DeadSix27/waifu2x-converter-cpp)
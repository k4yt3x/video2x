# Video2X Lossless Video Enlarger

### Official Discussion Group (Telegram): https://t.me/video2x

## Download Builds (Windows)

You can go to the [releases page](https://github.com/k4yt3x/video2x/releases) to download the latest builds of `Video2X`. The exe files will require no Python or Python module installation.

The **`full`** package provides all packages that will possibly be needed by `Video2X`, including `FFmpeg`, `waifu2x-caffe`, `waifu2x-converter-cpp`, `waifu2x-ncnn-vulkan`, and `Anime4K`. The config file (`video2x.json`) is also already configured for the environment. All you need to do is just to launch `video2x.exe`.

The **`light`** package provides only the most basic functions of `Video2X`. Only `video2x.exe`, `video2x_setup.exe` and `video2x.json` are included. To setup dependencies (e.g. `FFmpeg` and `Waifu2X`) automatically, simply launch `video2x_setup.exe`.

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
   - [**Anime4K**](https://github.com/bloc97/Anime4K)

## Recent Changes

### 3.1.0 (February 26, 2020)

- Removed the redundant layer of multi-threading since multi-process has to be implemented for launching Windows PE files in sub-processes
- Added support for graceful exit upon `KeyboardInterrupt` or termination signals
- Other minor improvements such as replacing `' '.join(execute)` with `shlex.join(execute)`

### 3.0.0 (November 26, 2019)

- Linux compatibility
- Configuration file changed to YAML format
  - You may still use a JSON-formatted config file. To do so, please specify `-c video2x.json`.
- Other code clean-up and optimization

### 2.10.0 (August 16, 2019)

- **Added support for [Anime4K](https://github.com/bloc97/Anime4K)**

### Setup Script 1.6.0 (November 26, 2019)

- Added compatibility for new YAML configuration file
- Added better exception handling

## Description

Video2X is an automation software based on waifu2x image enlarging engine. It extracts frames from a video, enlarge it by a number of times without losing any details or quality, keeping lines smooth and edges sharp.

For short: **Video2X enlarges your video without losing details**

Watch for the sharper edges in this screenshot around the shadows:

![preview](https://user-images.githubusercontent.com/21986859/49412428-65083280-f73a-11e8-8237-bb34158a545e.png)

**You can also watch the YouTube video Demo: https://www.youtube.com/watch?v=PG94iPoeoZk**

Clip is from trailer of animated movie "千と千尋の神隠し". Copyright belongs to "株式会社スタジオジブリ (STUDIO GHIBLI INC.)". Will delete immediately if use of clip is in violation of copyright.

## Screenshot

![video2x_gui](https://user-images.githubusercontent.com/21986859/63219691-ee478400-c166-11e9-8c15-dcf59228bd96.png)

![screenshot](https://user-images.githubusercontent.com/21986859/40265170-39c0caae-5b01-11e8-8371-8b6c24769639.png)

---

## Documentations

### [Video2X Wiki](https://github.com/k4yt3x/video2x/wiki)

You can find all detailed user-facing and developer-facing documentations in the [Video2X Wiki](https://github.com/k4yt3x/video2x/wiki). It covers everything from step-by-step instructions for beginners, to the code structure of this program for advanced users and developers. If this README page doesn't answer all your questions, the wiki page is where you should head to.

### [Step-By-Step Tutorial](https://github.com/k4yt3x/video2x/wiki/Step-By-Step-Tutorial)

For those who want a detailed walk-through of how to use `Video2X`, you can head to the [Step-By-Step Tutorial](https://github.com/k4yt3x/video2x/wiki/Step-By-Step-Tutorial) wiki page. It includes almost every step you need to perform in order to enlarge your first video.

### [Waifu2X Drivers](https://github.com/k4yt3x/video2x/wiki/Waifu2X-Drivers)

Go to the [Waifu2X Drivers](https://github.com/k4yt3x/video2x/wiki/Waifu2X-Drivers) wiki page if you want to see a detailed description on the different types of `waifu2x` drivers implemented by `Video2X`. This wiki page contains detailed difference between different drivers, and how to download and set each of them up for `Video2X`.

### [Q&A](https://github.com/k4yt3x/video2x/wiki/Q&A)

If you have any questions, first try visiting our [Q&A](https://github.com/k4yt3x/video2x/wiki/Q&A) page to see if your question is answered there. If not, open an issue and we will respond to your questions ASAP.

---

## Quick Start

### Prerequisites

- **Python 3.8**
Download: https://www.python.org/downloads/windows/
- **FFmpeg Windows Build**
Download: https://ffmpeg.org/download.html
- **waifu2x-caffe** (for Nvidia CUDA/CUDNN)
Download: https://github.com/lltcggie/waifu2x-caffe/releases
- **waifu2x-converter-cpp** (required for AMD, OpenCL and OpenGL processing)
Download: https://github.com/DeadSix27/waifu2x-converter-cpp/releases
- **waifu2x-ncnn-vulkan**
Download: https://github.com/nihui/waifu2x-ncnn-vulkan/releases
- **Anime4K**
Download: https://github.com/bloc97/Anime4K/releases

### Installing Dependencies

First, clone the video2x repository.

```shell
git clone https://github.com/k4yt3x/video2x.git
cd video2x/bin
```

Then you may run the `video2x_setup.py` script to install and configure the dependencies automatically. This script is designed and tested on Windows 10.

This script will install the newest version of `ffmpeg`, any one or all `waifu2x-caffe`, `waifu2x-converter-cpp`, and `waifu2x-ncnn-vulkan` to `%LOCALAPPDATA%\\video2x` and all required python libraries.

```shell
python video2x_setup.py
```

Alternatively, you can also install the dependencies manually. Please refer to the prerequisites section to see what's needed.

Then you'll need to install python dependencies before start using video2x. Install simply by executing the following command.

```shell
pip install -r requirements.txt
```

**Note that all command line arguments/options overwrite configuration file settings.**

### Sample Videos

If you can't find a video clip to begin with, or if you want to see a before-after comparison, we have prepared some sample clips for you. The quick start guide down below will also be based on the name of the sample clips.

![sample_video](https://user-images.githubusercontent.com/21986859/52905766-d5512b00-3236-11e9-9aea-077636539679.png)

- [Sample Video Original (240P) 1.7MB](https://files.flexio.org/Resources/Videos/sample_input.mp4)
- [Sample Video Upscaled (1080P) 4.8MB](https://files.flexio.org/Resources/Videos/sample_output.mp4)

Clip is from anime "さくら荘のペットな彼女". Copyright belongs to "株式会社アニプレックス (Aniplex Inc.)". Will delete immediately if use of clip is in violation of copyright.

### Nvidia CUDA (waifu2x-caffe)

Enlarge the video to 1920x1080 using CUDA. You may also use the `-r/--ratio` option.

```shell
python video2x.py -i sample_input.mp4 -o sample_output.mp4 -m gpu --width=1920 --height=1080
```

### Nvidia CUDNN

Enlarge the video to 1920x1080 using CUDNN. You may also use the `-r/--ratio` option.

```shell
python video2x.py -i sample_input.mp4 -o sample_output.mp4 -m cudnn --width=1920 --height=1080
```

### AMD or Nvidia (waifu2x-converter-cpp OpenCL)

Enlarge the video by 2 times using OpenCL. Note that `waifu2x-converter-cpp` doesn't support width and height. You'll also have to explicitly specify that the driver to be used is `waifu2x_converter`.

```shell
python video2x.py -i sample_input.mp4 -o sample_output.mp4 -m gpu -r 2 -d waifu2x_converter
```

### AMD or Nvidia (waifu2x-ncnn-vulkan Vulkan)

```shell
python video2x.py -i sample_input.mp4 -o sample_output.mp4 -m gpu -r 2 -d waifu2x_ncnn_vulkan
```

### Anime4K

Enlarge the video by 2 times using Anime4K. **Remember to install and configure [JRE 12](https://www.oracle.com/technetwork/java/javase/downloads/java-archive-javase12-5440181.html) path in the configuration file.**

```shell
python video2x.py -i sample_input.mp4 -o sample_output.mp4 -r 2 -d anime4k
```

### CPU

Enlarge the video to 1920x1080 using the CPU. You may also use the `-r/--ratio` option. **waifu2x-based upscalers potentially run much slower than using a GPU, but Anime4K is more CPU-dependant**. The configuration file for this method is similar to the previous methods.

```shell
python video2x.py -i sample_input.mp4 -o sample_output.mp4 -m cpu --width=1920 --height=1080
```

---

# Full Usage

## General Options

### -h, --help
    show this help message and exit

## File Options

### -i INPUT, --input INPUT
    Source video file/directory (default: None)

### -o OUTPUT, --output OUTPUT
    Output video file/directory (default: None)

## Upscaling Options

### -m {cpu,gpu,cudnn}, --method {cpu,gpu,cudnn}
    Upscaling method (default: gpu)

### -d {waifu2x_caffe,waifu2x_converter}, --driver {waifu2x_caffe,waifu2x_converter}
    Waifu2x driver (default: waifu2x_caffe)

### -y MODEL_DIR, --model_dir MODEL_DIR
    Folder containing model JSON files

### -t THREADS, --threads THREADS
    Number of threads to use for upscaling (default: 5)

### -c CONFIG, --config CONFIG
    Video2X config file location (default: video2x\bin\video2x.json)

### -b, --batch
    Enable batch mode (select all default values to questions)

## Scaling Options

### --width WIDTH
    Output video width

### --height HEIGHT
    Output video height

### -r RATIO, --ratio RATIO
    Scaling ratio

---

## License

Licensed under the GNU General Public License Version 3 (GNU GPL v3)
https://www.gnu.org/licenses/gpl-3.0.txt

![GPLv3 Icon](https://www.gnu.org/graphics/gplv3-127x51.png)

(C) 2018-2020 K4YT3X

## Credits

This project relies on the following software and projects.

- [FFmpeg]('https://www.ffmpeg.org/')
- [waifu2x-caffe](https://github.com/lltcggie/waifu2x-caffe)
- [waifu2x-converter-cpp](https://github.com/DeadSix27/waifu2x-converter-cpp)
- [waifu2x-ncnn-vulkan](https://github.com/nihui/waifu2x-ncnn-vulkan)
- [Anime4K](https://github.com/bloc97/Anime4K)

## Special Thanks

Appreciations given to the following code contributors:

- @BrianPetkovsek
- @SAT3LL

## Related Resources

- [Dandere2x](https://github.com/CardinalPanda/dandere2x): `Dandere2x` is a lossy video upscaler also built around `waifu2x`, but with video compression techniques to shorten the time needed to process a video.

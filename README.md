<p align="center">
   <img src="https://user-images.githubusercontent.com/21986859/81626588-ae5ab800-93eb-11ea-918f-ebe98c2de40a.png"/>
</p>

![GitHub release (latest by date)](https://img.shields.io/github/v/release/k4yt3x/video2x?style=flat-square)
![GitHub Workflow Status](https://img.shields.io/github/workflow/status/k4yt3x/video2x/Video2X%20Nightly%20Build?label=Nightly&style=flat-square)
![Docker Cloud Build Status](https://img.shields.io/docker/cloud/build/k4yt3x/video2x?style=flat-square)
![GitHub All Releases](https://img.shields.io/github/downloads/k4yt3x/video2x/total?style=flat-square)
![GitHub](https://img.shields.io/github/license/k4yt3x/video2x?style=flat-square)
![Platforms](https://img.shields.io/badge/Platforms-Windows%20%7C%20Linux%20%7C%20macOS-blue?style=flat-square)
![Become A Patron!](https://img.shields.io/endpoint.svg?url=https%3A%2F%2Fshieldsio-patreon.herokuapp.com%2Fk4yt3x&style=flat-square)

<!--# Video2X Lossless Video Enlarger-->

### Official Discussion Group (Telegram): https://t.me/video2x

## [Download Stable/Beta Builds](https://github.com/k4yt3x/video2x/releases/latest) (Windows)

- **`Full`**: full package comes pre-configured with **all** dependencies like `FFmpeg` and `waifu2x-caffe`.
- **`Light`**: ligt package comes with only Video2X binaries and a template configuration file. The user will either have to run the setup script or install and configure dependencies themselves.

Go to the **[Quick Start](#quick-start)** section for usages.

### [Download From Mirror](https://files.k4yt3x.com/Projects/Video2X/latest)

In case you're unable to download the releases directly from GitHub, you can try downloading from the mirror site hosted by the author. Only releases will be updated in this directory, not nightly builds.

## [Download Nightly Builds](https://github.com/k4yt3x/video2x/actions) (Windows)

**You need to be logged into GitHub to be able to download GitHub Actions artifacts.**

Nightly builds are built automatically every time a new commit is pushed to the master branch. The lates nightly build is always up-to-date with the latest version of the code, but is less stable and may contain bugs. Nightly builds are handled by GitHub's integrated CI/CD tool, GitHub Actions.

To download the latest nightly build, go to the [GitHub Actions](https://github.com/k4yt3x/video2x/actions) tab, enter the last run of workflow "Video2X Nightly Build, and download the artifacts generated from the run.

## [Docker Image](https://hub.docker.com/r/k4yt3x/video2x)

Video2X Docker images are available on Docker Hub for easy and rapid Video2X deployment on Linux and macOS. If you already have Docker installed, then only one command is needed to start upscaling a video. For more information on how to use Video2X's Docker image, please refer to the [documentations](https://github.com/K4YT3X/video2x/wiki/Docker).

## Introduction

Video2X is a video/GIF/image upscaling software based on Waifu2X, Anime4K, SRMD and RealSR written in Python 3. It upscales videos, GIFs and images, restoring details from low-resolution inputs. Video2X also accepts GIF input to video output and video input to GIF output.

Currently, Video2X supports the following drivers (implementations of algorithms).

- **Waifu2X Caffe**: Caffe implementation of waifu2x
- **Waifu2X Converter CPP**: CPP implementation of waifu2x based on OpenCL and OpenCV
- **Waifu2X NCNN Vulkan**: NCNN implementation of waifu2x based on Vulkan API
- **SRMD NCNN Vulkan**: NCNN implementation of SRMD based on Vulkan API
- **RealSR NCNN Vulkan**: NCNN implementation of RealSR based on Vulkan API
- **Anime4KCPP**: CPP implementation of Anime4K

### Video Upscaling

![Spirited Away Demo](https://user-images.githubusercontent.com/21986859/49412428-65083280-f73a-11e8-8237-bb34158a545e.png)\
*Upscale Comparison Demonstration*

**You can watch the whole demo video on YouTube: https://youtu.be/mGEfasQl2Zo**

Clip is from trailer of animated movie "千と千尋の神隠し". Copyright belongs to "株式会社スタジオジブリ (STUDIO GHIBLI INC.)". Will delete immediately if use of clip is in violation of copyright.

### GIF Upscaling

This original input GIF is 160x120 in size. This image is downsized and accelerated to 20 FPS from its [original image](https://gfycat.com/craftyeasygoingankole-capoo-bug-cat).

![catfru](https://user-images.githubusercontent.com/21986859/81631069-96d4fc80-93f6-11ea-92fb-33d6545055e7.gif)\
*Catfru original 160x120 GIF image*

Below is what it looks like after getting upscaled to 640x480 (4x) using Video2X.

![catfru4x](https://user-images.githubusercontent.com/21986859/81631070-976d9300-93f6-11ea-9137-072a3b386110.gif)\
*Catfru 4x upscaled GIF*

### Image Upscaling

![jill_comparison](https://user-images.githubusercontent.com/21986859/81631903-79a12d80-93f8-11ea-9c3c-f340240cf08c.png)\
*Image upscaling example*

[Original image](https://72915.tumblr.com/post/173793265673) from [nananicu@twitter](https://twitter.com/nananicu/status/994546266968281088), edited by K4YT3X.

## All Demo Videos

Below is a list of all the demo videos available.
The list is sorted from new to old.

- **Bad Apple!!**
  - YouTube: https://youtu.be/-RKLdCELgkQ
  - Bilibili: https://www.bilibili.com/video/BV1s5411s7xV/
- **The Pet Girl of Sakurasou 240P to 1080P 60FPS**
  - Original name: さくら荘のペットな彼女
  - YouTube: https://youtu.be/M0vDI1HH2_Y
  - Bilibili: https://www.bilibili.com/video/BV14k4y167KP/
- **Spirited Away (360P to 4K)**
  - Original name: 千と千尋の神隠し
  - YouTube: https://youtu.be/mGEfasQl2Zo
  - Bilibili: https://www.bilibili.com/video/BV1V5411471i/

---

## Screenshots

### Video2X GUI

![GUI Preview](https://user-images.githubusercontent.com/21986859/82119295-bc526500-976c-11ea-9ea8-53264689023e.png)\
*Video2X GUI Screenshot*

### Video2X CLI

![Video2X CLI Screenshot](https://user-images.githubusercontent.com/21986859/81662415-0c5bbf80-942d-11ea-8aa6-aacf813f9368.png)\
*Video2X CLI Screenshot*

---

### Sample Videos

If you can't find a video clip to begin with, or if you want to see a before-after comparison, we have prepared some sample clips for you. The quick start guide down below will also be based on the name of the sample clips.

![sample_video](https://user-images.githubusercontent.com/21986859/52905766-d5512b00-3236-11e9-9aea-077636539679.png)\
*Sample Upscale Videos*

- [Sample Video (240P) 4.54MB](https://files.k4yt3x.com/Resources/Videos/sample_input.mp4)
- [Sample Video Upscaled (1080P) 4.54MB](https://files.k4yt3x.com/Resources/Videos/sample_output.mp4)
- [Sample Video Original (1080P) 22.2MB](https://files.k4yt3x.com/Resources/Videos/sample_original.mp4)

Clip is from anime "さくら荘のペットな彼女". Copyright belongs to "株式会社アニプレックス (Aniplex Inc.)". Will delete immediately if use of clip is in violation of copyright.

---

## Quick Start

### Prerequisites

Before running Video2X, you'll need to ensure you have installed the drivers' external dependencies such as GPU drivers.

- waifu2x-caffe
  - GPU mode: Nvidia graphics card driver
  - cuDNN mode: Nvidia CUDA and [cuDNN](https://docs.nvidia.com/deeplearning/sdk/cudnn-install/index.html#install-windows)
- Other Drivers
  - GPU driver if you want to use GPU for processing

### Running Video2X (GUI)

The easiest way to run Video2X is to use the full build. Extract the full release zip file and you'll get these files.

![Video2X Release Files](https://user-images.githubusercontent.com/21986859/81489846-28633380-926a-11ea-9e81-fb92f492e14c.png)\
*Video2X release files*

Simply double click on video2x_gui.exe to launch the GUI.

![Video2X GUI Main Tab](https://user-images.githubusercontent.com/21986859/81489858-4c267980-926a-11ea-9ab2-38ec738f2fb6.png)\
*Video2X GUI main tab*

Then, drag the videos you wish to upscale into the window and select the appropriate output path.

![drag-drop](https://user-images.githubusercontent.com/21986859/81489880-7bd58180-926a-11ea-85ae-b72d2f4f5e72.png)\
*Drag and drop file into Video2X GUI*

Tweak the settings if you want to, then hit the start button at the bottom and the upscale will start. Now you'll just have to wait for it to complete.

![upscale-started](https://user-images.githubusercontent.com/21986859/81489924-ce16a280-926a-11ea-831c-6c66b950f957.png)\
*Video2X started processing input files*

### Running Video2X (CLI)

#### Basic Upscale Example

This example command below uses `waifu2x-caffe` to enlarge the video `sample-input.mp4` two double its original size.

```shell
python video2x.py -i sample-input.mp4 -o sample-output.mp4 -r 2 -d waifu2x_caffe
```

#### Advanced Upscale Example

If you would like to tweak engine-specific settings, either specify the corresponding argument after `--`, or edit the corresponding field in the configuration file `video2x.yaml`. **Command line arguments will overwrite default values in the config file.**

This example below adds enables TTA for `waifu2x-caffe`.

```shell
python video2x.py -i sample-input.mp4 -o sample-output.mp4 -r 2 -d waifu2x_caffe -- --tta 1
```

To see a help page for driver-specific settings, use `-d` to select the driver and append `-- --help` as demonstrated below. This will print all driver-specific settings and descriptions.

```shell
python video2x.py -d waifu2x_caffe -- --help
```

### Running Video2X (Docker)

Video2X can be deployed via Docker. The following command upscales the video `sample_input.mp4` two times with Waifu2X NCNN Vulkan and outputs the upscaled video to `sample_output.mp4`. For more details on Video2X Docker image usages, please refer to the [documentations](https://github.com/K4YT3X/video2x/wiki/Docker).

```shell
docker run --rm -it --gpus all -v /dev/dri:/dev/dri -v $PWD:/host k4yt3x/video2x:4.6.0 -d waifu2x_ncnn_vulkan -r 2 -i sample_input.mp4 -o sample_output.mp4
```

---

## Documentations

### [Video2X Wiki](https://github.com/k4yt3x/video2x/wiki)

You can find all detailed user-facing and developer-facing documentations in the [Video2X Wiki](https://github.com/k4yt3x/video2x/wiki). It covers everything from step-by-step instructions for beginners, to the code structure of this program for advanced users and developers. If this README page doesn't answer all your questions, the wiki page is where you should head to.

### [Step-By-Step Tutorial](https://github.com/k4yt3x/video2x/wiki/Step-By-Step-Tutorial)

For those who want a detailed walk-through of how to use Video2X, you can head to the [Step-By-Step Tutorial](https://github.com/k4yt3x/video2x/wiki/Step-By-Step-Tutorial) wiki page. It includes almost every step you need to perform in order to enlarge your first video.

### [Run From Source Code](https://github.com/k4yt3x/video2x/wiki/Run-From-Source-Code)

This wiki page contains all instructions for how you can run Video2X directly from Python source code.

### [Drivers](https://github.com/k4yt3x/video2x/wiki/Drivers)

Go to the [Drivers](https://github.com/k4yt3x/video2x/wiki/Drivers) wiki page if you want to see a detailed description on the different types of drivers implemented by Video2X. This wiki page contains detailed difference between different drivers, and how to download and set each of them up for Video2X.

### [Q&A](https://github.com/k4yt3x/video2x/wiki/Q&A)

If you have any questions, first try visiting our [Q&A](https://github.com/k4yt3x/video2x/wiki/Q&A) page to see if your question is answered there. If not, open an issue and we will respond to your questions ASAP. Alternatively, you can also join our [Telegram discussion group](https://t.me/video2x) and ask your questions there.

### [History](https://github.com/k4yt3x/video2x/wiki/History)

Are you interested in how the idea of Video2X was born? Do you want to know the stories and histories behind Video2X's development? Come into this page.

---

# Full Usage

## Video2X Options

### -h, --help
    show this help message and exit

### -i INPUT, --input INPUT
    source video file/directory

### -o OUTPUT, --output OUTPUT
    output video file/directory

### -c CONFIG, --config CONFIG
    video2x config file path

### --log LOG
    log file path (default: program_directory\video2x_%Y-%m-%d_%H-%M-%S.log)

### --disable_logging
    disable logging (default: False)

### -v, --version
    display version, lawful information and exit

## Upscaling Options

### -d DRIVER, --driver DRIVER
    upscaling driver (default: waifu2x_caffe)

Available options are:

- waifu2x_caffe
- waifu2x_converter_cpp
- waifu2x_ncnn_vulkan
- srmd_ncnn_vulkan
- realsr_ncnn_vulkan
- anime4kcpp

### -r RATIO, --ratio RATIO
    scaling ratio

### -p PROCESSES, --processes PROCESSES
    number of processes to use for upscaling (default: 1)

### --preserve_frames
    preserve extracted and upscaled frames (default: False)

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
- [realsr-ncnn-vulkan](https://github.com/nihui/realsr-ncnn-vulkan)
- [Anime4K](https://github.com/bloc97/Anime4K)
- [Anime4KCPP](https://github.com/TianZerL/Anime4KCPP)
- [Gifski](https://github.com/ImageOptim/gifski)

## Special Thanks

Appreciations given to the following personnel who have contributed significantly to the project (specifically the technical perspective).

- [@BrianPetkovsek](https://github.com/BrianPetkovsek)
- [@sat3ll](https://github.com/sat3ll)
- [@ddouglas87](https://github.com/ddouglas87)
- [@lhanjian](https://github.com/lhanjian)

## Related Projects

- [Dandere2x](https://github.com/CardinalPanda/dandere2x): A lossy video upscaler also built around `waifu2x`, but with video compression techniques to shorten the time needed to process a video.
- [Waifu2x-Extension-GUI](https://github.com/AaronFeng753/Waifu2x-Extension-GUI): A similar project that focuses more and only on building a better graphical user interface. It is built using C++ and Qt5, and currently only supports the Windows platform.

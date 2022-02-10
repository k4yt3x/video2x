<p align="center">
   <img src="https://user-images.githubusercontent.com/21986859/102733190-872a7880-4334-11eb-8e9e-0ca747f130b1.png"/>
   </br>
   <img src="https://img.shields.io/github/v/release/k4yt3x/video2x?style=flat-square"/>
   <img src="https://img.shields.io/github/workflow/status/k4yt3x/video2x/CI?label=CI&style=flat-square"/>
   <img src="https://img.shields.io/github/downloads/k4yt3x/video2x/total?style=flat-square"/>
   <img src="https://img.shields.io/github/license/k4yt3x/video2x?style=flat-square"/>
   <img src="https://img.shields.io/badge/dynamic/json?color=%23e85b46&label=Patreon&query=data.attributes.patron_count&suffix=%20patrons&url=https%3A%2F%2Fwww.patreon.com%2Fapi%2Fcampaigns%2F4507807&style=flat-square"/>
</p>

### Official [Telegram Discussion Group](https://t.me/video2x)

Join the Telegram discussion group to

## [Download Windows Releases](https://github.com/k4yt3x/video2x/releases/tag/4.8.1)

The latest Windows update is built based on version 4.8.1. GUI is not available for 5.0.0 yet, but is already under development. Go to the **[Quick Start](#quick-start)** section for usages. Try the [mirror](https://files.k4yt3x.com/Projects/Video2X/lates) if you can't download releases directly from GitHub.

## [Google Colab](https://colab.research.google.com/drive/1gWEwcA9y57EsxwOjmLNmNMXPsafw0kGo)

You can use Video2X on [Google Colab](https://colab.research.google.com/) **for free** if you don't have a powerful GPU of your own. You can borrow a powerful GPU (Tesla K80, T4, P4, or P100) on Google's server for free for a maximum of 12 hours per session. **Please use the free resource fairly** and do not create sessions back-to-back and run upscaling 24/7. This might result in you getting banned. You can get [Colab Pro/Pro+](https://colab.research.google.com/signup/pricing) if you'd like to use better GPUs and get longer runtimes. Usage instructions are embedded in the [Colab Notebook](https://github.com/k4yt3x/video2x/actions/workflows/ci.yml).

## [Download Nightly Releases](https://github.com/k4yt3x/video2x/actions/workflows/ci.yml)

Nightly releases are automatically created by the GitHub Actions CI/CD pipelines. They usually contain more experimental features and bug fixes. However, they are much less stable to the stable releases. **You must log in to GitHub to download CI build artifacts.**

## [Container Image](https://github.com/k4yt3x/video2x/pkgs/container/video2x)

Video2X container images are available on the GitHub Container Registry for easy deployment on Linux and macOS. If you already have Docker/Podman installed, only one command is needed to start upscaling a video. For more information on how to use Video2X's Docker image, please refer to the [documentations (outdated)](https://github.com/K4YT3X/video2x/wiki/Docker).

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
_Upscale Comparison Demonstration_

**You can watch the whole demo video on YouTube: https://youtu.be/mGEfasQl2Zo**

Clip is from trailer of animated movie "千と千尋の神隠し". Copyright belongs to "株式会社スタジオジブリ (STUDIO GHIBLI INC.)". Will delete immediately if use of clip is in violation of copyright.

### GIF Upscaling

This original input GIF is 160x120 in size. This image is downsized and accelerated to 20 FPS from its [original image](https://gfycat.com/craftyeasygoingankole-capoo-bug-cat).

![catfru](https://user-images.githubusercontent.com/21986859/81631069-96d4fc80-93f6-11ea-92fb-33d6545055e7.gif)\
_Catfru original 160x120 GIF image_

Below is what it looks like after getting upscaled to 640x480 (4x) using Video2X.

![catfru4x](https://user-images.githubusercontent.com/21986859/81631070-976d9300-93f6-11ea-9137-072a3b386110.gif)\
_Catfru 4x upscaled GIF_

### Image Upscaling

![jill_comparison](https://user-images.githubusercontent.com/21986859/81631903-79a12d80-93f8-11ea-9c3c-f340240cf08c.png)\
_Image upscaling example_

[Original image](https://72915.tumblr.com/post/173793265673) from [nananicu@twitter](https://twitter.com/nananicu/status/994546266968281088), edited by K4YT3X.

## All Demo Videos

Below is a list of all the demo videos available.
The list is sorted from new to old.

- **Bad Apple!!**
  - YouTube: https://youtu.be/A81rW_FI3cw
  - Bilibili: https://www.bilibili.com/video/BV16K411K7ue
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
_Video2X GUI Screenshot_

### Video2X CLI

![Video2X CLI Screenshot](https://user-images.githubusercontent.com/21986859/81662415-0c5bbf80-942d-11ea-8aa6-aacf813f9368.png)\
_Video2X CLI Screenshot_

---

### Sample Videos

If you can't find a video clip to begin with, or if you want to see a before-after comparison, we have prepared some sample clips for you. The quick start guide down below will also be based on the name of the sample clips.

![sample_video](https://user-images.githubusercontent.com/21986859/52905766-d5512b00-3236-11e9-9aea-077636539679.png)\
_Sample Upscale Videos_

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
_Video2X release files_

Simply double click on video2x_gui.exe to launch the GUI.

![Video2X GUI Main Tab](https://user-images.githubusercontent.com/21986859/81489858-4c267980-926a-11ea-9ab2-38ec738f2fb6.png)\
_Video2X GUI main tab_

Then, drag the videos you wish to upscale into the window and select the appropriate output path.

![drag-drop](https://user-images.githubusercontent.com/21986859/81489880-7bd58180-926a-11ea-85ae-b72d2f4f5e72.png)\
_Drag and drop file into Video2X GUI_

Tweak the settings if you want to, then hit the start button at the bottom and the upscale will start. Now you'll just have to wait for it to complete.

![upscale-started](https://user-images.githubusercontent.com/21986859/81489924-ce16a280-926a-11ea-831c-6c66b950f957.png)\
_Video2X started processing input files_

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

Video2X can be deployed via Docker. The following command upscales the video `sample_input.mp4` with Waifu2X ncnn Vulkan and outputs the upscaled video to `output.mp4`. For more details on Video2X Docker image usages, please refer to the [documentations (outdated)](https://github.com/K4YT3X/video2x/wiki/Docker).

```shell
docker run -it --rm \                           # temporary container, delete after run
    --gpus all -v /dev/dri:/dev/dri \           # mount GPUs
    -v $PWD:/host \                             # bind mount the current directory as the container's /host
    ghcr.io/k4yt3x/video2x:5.0.0-beta1-cuda \   # the URL of the docker image
    -i sample_input.mp4 \                       # path of the input file
    -o output.mp4 \                             # the path to write the output
    -p5 \                                       # launch 5 processes
    upscale \                                   # set action to upscale
    -h 720 \                                    # set output hight to 720 pixels
    -d waifu2x \                                # use driver waifu2x
    -n3                                         # noise level 3
```

To interpolate a video, set the action to `interpolate`. Right now, only 2x framerate is supported.

```shell
docker run -it --rm \
    --gpus all -v /dev/dri:/dev/dri \
    -v $PWD:/host \
    ghcr.io/k4yt3x/video2x:5.0.0-beta1-cuda \
    -i sample_input.mp4 \
    -o output.mp4 \
    interpolate                                 # set action to interpolate
```

---

## Documentations

### [Video2X Wiki](https://github.com/k4yt3x/video2x/wiki)

You can find all detailed user-facing and developer-facing documentations in the [Video2X Wiki](https://github.com/k4yt3x/video2x/wiki). It covers everything from step-by-step instructions for beginners, to the code structure of this program for advanced users and developers. If this README page doesn't answer all your questions, the wiki page is where you should head to.

### [Drivers](https://github.com/k4yt3x/video2x/wiki/Drivers)

Go to the [Drivers](https://github.com/k4yt3x/video2x/wiki/Drivers) wiki page if you want to see a detailed description on the different types of drivers implemented by Video2X. This wiki page contains detailed difference between different drivers, and how to download and set each of them up for Video2X.

### [Q&A](https://github.com/k4yt3x/video2x/wiki/Q&A)

If you have any questions, first try visiting our [Q&A](https://github.com/k4yt3x/video2x/wiki/Q&A) page to see if your question is answered there. If not, open an issue and we will respond to your questions ASAP. Alternatively, you can also join our [Telegram discussion group](https://t.me/video2x) and ask your questions there.

### [History](https://github.com/k4yt3x/video2x/wiki/History)

Are you interested in how the idea of Video2X was born? Do you want to know the stories and histories behind Video2X's development? Come into this page.

---

## License

This project is licensed under the [GNU Affero General Public License Version 3 (GNU AGPL v3)](https://www.gnu.org/licenses/agpl-3.0.txt)\
Copyright (c) 2018-2022 K4YT3X and contributors.

![AGPLv3](https://www.gnu.org/graphics/agplv3-155x51.png)

This project includes or depends on these following projects:

| Project                                                             | License              |
| ------------------------------------------------------------------- | -------------------- |
| [FFmpeg](https://www.ffmpeg.org/)                                   | LGPLv2.1, GPLv2      |
| [waifu2x-ncnn-vulkan](https://github.com/nihui/waifu2x-ncnn-vulkan) | MIT License          |
| [srmd-ncnn-vulkan](https://github.com/nihui/srmd-ncnn-vulkan)       | MIT License          |
| [realsr-ncnn-vulkan](https://github.com/nihui/realsr-ncnn-vulkan)   | MIT License          |
| [rife-ncnn-vulkan](https://github.com/nihui/rife-ncnn-vulkan)       | MIT License          |
| [ffmpeg-python](https://github.com/kkroening/ffmpeg-python)         | Apache-2.0           |
| [Loguru](https://github.com/Delgan/loguru)                          | MIT License          |
| [opencv-python](https://github.com/opencv/opencv-python)            | MIT License          |
| [Pillow](https://github.com/python-pillow/Pillow)                   | HPND License         |
| [Rich](https://github.com/Textualize/rich)                          | MIT License          |
| [tqdm](https://github.com/tqdm/tqdm)                                | MPLv2.0, MIT License |

Legacy versions of this project includes or depends on these following projects:

| Project                                                                     | License     |
| --------------------------------------------------------------------------- | ----------- |
| [waifu2x-caffe](https://github.com/lltcggie/waifu2x-caffe)                  | MIT License |
| [waifu2x-converter-cpp](https://github.com/DeadSix27/waifu2x-converter-cpp) | MIT License |
| [Anime4K](https://github.com/bloc97/Anime4K)                                | MIT License |
| [Anime4KCPP](https://github.com/TianZerL/Anime4KCPP)                        | MIT License |
| [Gifski](https://github.com/ImageOptim/gifski)                              | AGPLv3      |

More licensing information can be found in the [NOTICES](NOTICES) file.

## Special Thanks

Appreciations given to the following personnel who have contributed significantly to the project.

- [@BrianPetkovsek](https://github.com/BrianPetkovsek)
- [@sat3ll](https://github.com/sat3ll)
- [@ddouglas87](https://github.com/ddouglas87)
- [@lhanjian](https://github.com/lhanjian)
- [@ArchieMeng](https://github.com/archiemeng)

## Similar Projects

- [Dandere2x](https://github.com/CardinalPanda/dandere2x): A lossy video upscaler also built around `waifu2x`, but with video compression techniques to shorten the time needed to process a video.
- [Waifu2x-Extension-GUI](https://github.com/AaronFeng753/Waifu2x-Extension-GUI): A similar project that focuses more and only on building a better graphical user interface. It is built using C++ and Qt5, and currently only supports the Windows platform.

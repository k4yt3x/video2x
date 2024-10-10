<p align="center">
   <img src="https://github.com/user-attachments/assets/5cd63373-e806-474f-94ec-6e04963bf90f"/>
   </br>
   <img src="https://img.shields.io/github/v/release/k4yt3x/video2x?style=flat-square"/>
   <img src="https://img.shields.io/github/actions/workflow/status/k4yt3x/video2x/build.yml?label=Build&style=flat-square"/>
   <img src="https://img.shields.io/github/downloads/k4yt3x/video2x/total?style=flat-square"/>
   <img src="https://img.shields.io/github/license/k4yt3x/video2x?style=flat-square"/>
   <img src="https://img.shields.io/badge/dynamic/json?color=%23e85b46&label=Patreon&query=data.attributes.patron_count&suffix=%20patrons&url=https%3A%2F%2Fwww.patreon.com%2Fapi%2Fcampaigns%2F4507807&style=flat-square"/>
</p>

> [!IMPORTANT]
> Versions 4 and 5 have reached end-of-life (EOL) status. Due to limited development resources, issues related to any version earlier than 6 will no longer be addressed.

## 🌟 Version 6.0.0 Preview

**[Direct download link for Windows](https://github.com/k4yt3x/video2x/releases/download/6.0.0-beta.2/video2x-qt6-windows-amd64.zip)**

![6.0.0-beta-screenshot](https://github.com/user-attachments/assets/bde4e4e2-2f97-412f-8e34-848f384be720)

Version 6.0.0 is a complete rewrite of this project in C/C++. It:

- actually works this time, with less pain (in comparison to 5.0.0 beta);
- is blazing fast, thanks to the redesigned efficient pipeline and the speed of C/C++;
- is cross-platform, available right now for both Windows and Linux;
- provides much better output quality with Anime4K v4 and RealESRGAN;
- supports Anime4K v4 and all other custom MPV-compatible GLSL shaders;
- supports RealESRGAN (all three models) via ncnn and Vulkan;
- requires 0 disk space for processing the video, just space for storing the final output; and
- exports a standard C function that can be easily integrated in your own projects!

These are available for download now:

- **6.0.0 beta Qt6-based GUI for Windows** is on the [releases page](https://github.com/k4yt3x/video2x/releases).
- **6.0.0 beta CLI preview builds for Windows and Linux** are on the [releases page](https://github.com/k4yt3x/video2x/releases).
  - You will need to install the dependencies and set `LD_LIBRARY_PATH` for the Linux build to work. Refer to the [PKGBUILD](PKGBUILD) file to see what needs to be installed.
  - Alternatively, you can build it from source. Take a look at the [Makefile](Makefile).
- 6.0.0 beta AUR package for Arch Linux (`video2x-git`).
- 6.0.0 beta [container image](https://github.com/k4yt3x/video2x/pkgs/container/video2x).
- A new Colab will be made for 6.0.0 at a later time.

There is still much to be done and optimize. Stay tuned for more updates. As for why the 5.0.0 branch was abandoned, here are some of the reasons:

- Wrapped C++ libraries for Python are too painful to build for cross-platform distribution.
- Some wrapped C++ libraires exhibited unexpected behaviors.
- Running FFmpeg via commands and piping data through stdin/stdout are inefficient.
- C/C++ native binaries are much smaller and much more efficient.

## [💬 Telegram Discussion Group](https://t.me/video2x)

Join our Telegram discussion group to ask any questions you have about Video2X, chat directly with the developers, or discuss about super resolution technologies and the future of Video2X in general.

## [🪟 Download Windows Releases](https://github.com/k4yt3x/video2x/releases/tag/4.8.1)

The latest Windows release build based on version 4.8.1. Go to the [GUI](https://github.com/k4yt3x/video2x/wiki/GUI) page to see the basic usages of the GUI. Try the [mirror](https://files.k4yt3x.com/Projects/Video2X/latest) if you can't download releases directly from GitHub.

## [📔 Google Colab](https://colab.research.google.com/drive/1gWEwcA9y57EsxwOjmLNmNMXPsafw0kGo)

You can use Video2X on [Google Colab](https://colab.research.google.com/) **for free** if you don't have a powerful GPU of your own. You can borrow a powerful GPU (Tesla K80, T4, P4, or P100) on Google's server for free for a maximum of 12 hours per session. **Please use the free resource fairly** and do not create sessions back-to-back and run upscaling 24/7. This might result in you getting banned. You can get [Colab Pro/Pro+](https://colab.research.google.com/signup/pricing) if you'd like to use better GPUs and get longer runtimes. Usage instructions are embedded in the [Colab Notebook](https://colab.research.google.com/drive/1gWEwcA9y57EsxwOjmLNmNMXPsafw0kGo).

## [📦 Container Image](https://github.com/k4yt3x/video2x/pkgs/container/video2x)

Video2X container images are available on the GitHub Container Registry for easy deployment on Linux and macOS. If you already have Docker/Podman installed, only one command is needed to start upscaling a video. For more information on how to use Video2X's Docker image, please refer to the [documentations](https://github.com/K4YT3X/video2x/wiki/Container).

## [📖 Documentations](https://github.com/k4yt3x/video2x/wiki)

Video2X's documentations are hosted on this repository's [Wiki page](https://github.com/k4yt3x/video2x/wiki). It includes comprehensive explanations for how to use the [GUI](https://github.com/k4yt3x/video2x/wiki/GUI), the [CLI](https://github.com/k4yt3x/video2x/wiki/CLI), the [container image](https://github.com/K4YT3X/video2x/wiki/Container), the [library](https://github.com/k4yt3x/video2x/wiki/Library), and more. The Wiki is open to edits by the community, so you, yes you, can also correct errors or add new contents to the documentations.

## Introduction

Video2X is a video/GIF/image upscaling and frame interpolation software written in Python. It can use these following state-of-the-art algorithms to increase the resolution and frame rate of your video/GIF/image. More information about the algorithms that it supports can be found in [the documentations](https://github.com/k4yt3x/video2x/wiki/Algorithms).

### Video Upscaling

![Spirited Away Demo](https://user-images.githubusercontent.com/21986859/49412428-65083280-f73a-11e8-8237-bb34158a545e.png)\
_Upscale demo: Spirited Away's movie trailer_

- **Spirited Away**: [YouTube](https://youtu.be/mGEfasQl2Zo) | [Bilibili](https://www.bilibili.com/video/BV1V5411471i/)
  - 360P to 4K
  - The [original video](https://www.youtube.com/watch?v=ByXuk9QqQkk)'s copyright belongs to 株式会社スタジオジブリ
- **Bad Apple!!**: [YouTube](https://youtu.be/A81rW_FI3cw) | [Bilibili](https://www.bilibili.com/video/BV16K411K7ue)
  - 384P to 4K 120FPS
  - The [original video](https://www.nicovideo.jp/watch/sm8628149)'s copyright belongs to あにら
- **The Pet Girl of Sakurasou**: [YouTube](https://youtu.be/M0vDI1HH2_Y) | [Bilibili](https://www.bilibili.com/video/BV14k4y167KP/)
  - 240P to 1080P 60FPS
  - The original video's copyright belongs to ASCII Media Works

### Standard Test Clip

The following clip can be used to test if your setup works properly. This is also the standard clip used for running performance benchmarks.

- [Standard Test Clip (240P)](https://files.k4yt3x.com/Resources/Videos/standard-test.mp4) 4.54 MiB
- [waifu2x Upscaled Sample (1080P)](https://files.k4yt3x.com/Resources/Videos/standard-waifu2x.mp4) 4.54 MiB
- [Ground Truth (1080P)](https://files.k4yt3x.com/Resources/Videos/standard-original.mp4) 22.2 MiB

The original clip came from the anime "さくら荘のペットな彼女."\
Copyright of this clip belongs to 株式会社アニプレックス.

## License

This project is licensed under the [GNU Affero General Public License Version 3 (GNU AGPL v3)](https://www.gnu.org/licenses/agpl-3.0.txt)\
Copyright (C) 2018-2024 K4YT3X and [contributors](https://github.com/k4yt3x/video2x/graphs/contributors).

![AGPLv3](https://www.gnu.org/graphics/agplv3-155x51.png)

This project (`libvideo2x`) includes or depends on these following projects:

| Project                                                                       | License         |
| ----------------------------------------------------------------------------- | --------------- |
| [Anime4K](https://github.com/bloc97/Anime4K)                                  | MIT License     |
| [FFmpeg](https://www.ffmpeg.org/)                                             | LGPLv2.1, GPLv2 |
| [Real-ESRGAN ncnn Vulkan](https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan) | MIT License     |
| [ncnn](https://github.com/Tencent/ncnn)                                       | BSD 3-Clause    |

More licensing information can be found in the [NOTICE](NOTICE) file.

## Special Thanks

Appreciations are given to the following personnel who have contributed significantly to the project.

- [@ArchieMeng](https://github.com/archiemeng)
- [@BrianPetkovsek](https://github.com/BrianPetkovsek)
- [@ddouglas87](https://github.com/ddouglas87)
- [@lhanjian](https://github.com/lhanjian)
- [@nihui](https://github.com/nihui)
- [@sat3ll](https://github.com/sat3ll)

## Similar Projects

- [Dandere2x](https://github.com/CardinalPanda/dandere2x): A lossy video upscaler also built around `waifu2x`, but with video compression techniques to shorten the time needed to process a video.
- [Waifu2x-Extension-GUI](https://github.com/AaronFeng753/Waifu2x-Extension-GUI): A similar project that focuses more and only on building a better graphical user interface. It is built using C++ and Qt5, and currently only supports the Windows platform.

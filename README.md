<p align="center">
   <img src="https://github.com/user-attachments/assets/5cd63373-e806-474f-94ec-6e04963bf90f"/>
   </br>
   <img src="https://img.shields.io/github/v/release/k4yt3x/video2x?style=flat-square"/>
   <img src="https://img.shields.io/github/actions/workflow/status/k4yt3x/video2x/build.yml?label=Build&style=flat-square"/>
   <img src="https://img.shields.io/github/downloads/k4yt3x/video2x/total?style=flat-square"/>
   <img src="https://img.shields.io/github/license/k4yt3x/video2x?style=flat-square"/>
   <img src="https://img.shields.io/github/sponsors/k4yt3x?style=flat-square&link=https%3A%2F%2Fgithub.com%2Fsponsors%2Fk4yt3x"/>
   <img src="https://img.shields.io/badge/dynamic/json?color=%23e85b46&label=Patreon&query=data.attributes.patron_count&suffix=%20patrons&url=https%3A%2F%2Fwww.patreon.com%2Fapi%2Fcampaigns%2F4507807&style=flat-square"/>
</p>

> [!IMPORTANT]
> Versions 4 and 5 have reached end-of-life (EOL) status. Due to limited development resources, issues related to any version earlier than 6 will no longer be addressed.

## 🌟 Version 6.0.0

**[Download Windows Installer](https://github.com/k4yt3x/video2x/releases/download/6.1.1/video2x-qt6-windows-amd64-installer.exe)**

**TL;DR: Version 6.0.0 is a complete rewrite of the Video2X project in C/C++, featuring a faster, more efficient architecture, cross-platform support, vastly improved output quality, and a new GUI and installer for easy setup on Windows.**

![6.1.0-screenshot](https://github.com/user-attachments/assets/57aa11d0-dd01-49e9-b6b0-2d2f21a363ac)

Version 6.0.0 is a complete rewrite of this project in C/C++. It:

- genuinely works this time, with much less hassle compared to the 5.0.0 beta;
- is blazing fast, thanks to the new optimized pipeline and the efficiency of C/C++;
- is cross-platform, available now for both Windows and Linux;
- offers significantly better output quality with Anime4K v4 and RealESRGAN;
- supports Anime4K v4 and all custom MPV-compatible GLSL shaders;
- includes support for RealESRGAN (all three models) via ncnn and Vulkan;
- requires zero additional disk space during processing, just space for the final output; and
- exports a standard C function for easy integration into other projects! (documentations are on the way)

Support for RealCUGAN and frame interpolation with RIFE are coming soon.

## [🪟 Download for Windows](https://github.com/k4yt3x/video2x/releases/latest)

You can download the latest Windows release from the [releases page](https://github.com/k4yt3x/video2x/releases/latest). For basic GUI usage, refer to the [GUI wiki page](https://github.com/k4yt3x/video2x/wiki/GUI). If you're unable to download directly from GitHub, try the [mirror](https://files.k4yt3x.com/Projects/Video2X/latest). The GUI currently supports the following languages:

- English (United States)
- 简体中文（中国）
- 日本語（日本）
- Português (Portugal)

## [🐧 Install on Linux](https://aur.archlinux.org/packages/video2x-git)

You can install Video2X on Arch Linux using the [video2x-git](https://aur.archlinux.org/packages/video2x-git) AUR package or on Ubuntu/Debian using the `.deb` package from the [releases page](https://github.com/k4yt3x/video2x/releases/latest). If you'd like to build from source, refer to the [PKGBUILD](packaging/arch/PKGBUILD) file for a general overview of the required packages and commands. If you'd prefer not to compile the program from source, consider using the container image below.

## [📦 Container Image](https://github.com/k4yt3x/video2x/pkgs/container/video2x)

Video2X container images are available on the GitHub Container Registry for easy deployment on Linux and macOS. If you already have Docker/Podman installed, only one command is needed to start upscaling a video. For more information on how to use Video2X's Docker image, please refer to the [documentations](https://github.com/K4YT3X/video2x/wiki/Container).

## [📔 Google Colab](https://colab.research.google.com/drive/1gWEwcA9y57EsxwOjmLNmNMXPsafw0kGo)

You can use Video2X on [Google Colab](https://colab.research.google.com/) **for free** if you don't have a powerful GPU of your own. You can borrow a powerful GPU (NVIDIA T4, L4, or A100) on Google's server for free for a maximum of 12 hours per session. **Please use the free resource fairly** and do not create sessions back-to-back and run upscaling 24/7. This might result in you getting banned. You can get [Colab Pro/Pro+](https://colab.research.google.com/signup/pricing) if you'd like to use better GPUs and get longer runtimes. Usage instructions are embedded in the [Colab Notebook](https://colab.research.google.com/drive/1gWEwcA9y57EsxwOjmLNmNMXPsafw0kGo).

## [💬 Telegram Discussion Group](https://t.me/video2x)

Join our Telegram discussion group to ask any questions you have about Video2X, chat directly with the developers, or discuss about super resolution technologies and the future of Video2X in general.

## [📖 Documentations](https://github.com/k4yt3x/video2x/wiki)

Video2X's documentations are hosted on this repository's [Wiki page](https://github.com/k4yt3x/video2x/wiki). It includes comprehensive explanations for how to use the [GUI](https://github.com/k4yt3x/video2x/wiki/GUI), the [CLI](https://github.com/k4yt3x/video2x/wiki/CLI), the [container image](https://github.com/K4YT3X/video2x/wiki/Container), the [library](https://github.com/k4yt3x/video2x/wiki/Library), and more. The Wiki is open to edits by the community, so you, yes you, can also correct errors or add new contents to the documentations.

## Introduction

Video2X is a machine-learning-powered framework for video upscaling and frame interpolation, built around three main components:

- [libvideo2x](https://github.com/k4yt3x/video2x/blob/master/src/libvideo2x.cpp): The core C++ library providing upscaling and frame interpolation capabilities.
- [Video2X CLI](https://github.com/k4yt3x/video2x/blob/master/src/video2x.c): A command-line interface that utilizes `libvideo2x` for video processing.
- [Video2X Qt6](https://github.com/k4yt3x/video2x-qt6): A Qt6-based graphical interface that utilizes `libvideo2x` for video processing.

### Video Demos

![Spirited Away Demo](https://user-images.githubusercontent.com/21986859/49412428-65083280-f73a-11e8-8237-bb34158a545e.png)\
_Upscale demo: Spirited Away's movie trailer_

- **Spirited Away**: [YouTube](https://youtu.be/mGEfasQl2Zo) | [Bilibili](https://www.bilibili.com/video/BV1V5411471i/)
  - 360P to 4K
  - The [original video](https://www.youtube.com/watch?v=ByXuk9QqQkk)'s copyright belongs to 株式会社スタジオジブリ
- **Bad Apple!!**: [YouTube](https://youtu.be/A81rW_FI3cw) | [Bilibili](https://www.bilibili.com/video/BV16K411K7ue)
  - 384P 30 FPS to 4K 120 FPS with waifu2x and DAIN
  - The [original video](https://www.nicovideo.jp/watch/sm8628149)'s copyright belongs to あにら
- **The Pet Girl of Sakurasou**: [YouTube](https://youtu.be/M0vDI1HH2_Y) | [Bilibili](https://www.bilibili.com/video/BV14k4y167KP/)
  - 240P 29.97 to 1080P 60 FPS with waifu2x and DAIN
  - The original video's copyright belongs to ASCII Media Works

### Standard Test Clip

The following clip can be used to test if your setup works properly. This is also the standard clip used for running performance benchmarks.

- [Standard Test Clip (240P)](https://files.k4yt3x.com/Resources/Videos/standard-test.mp4) 4.54 MiB
- [waifu2x Upscaled Sample (1080P)](https://files.k4yt3x.com/Resources/Videos/standard-waifu2x.mp4) 4.54 MiB
- [Ground Truth (1080P)](https://files.k4yt3x.com/Resources/Videos/standard-original.mp4) 22.2 MiB

The original clip came from the anime "さくら荘のペットな彼女."\
Copyright of this clip belongs to 株式会社アニプレックス.

## License

This project is licensed under [GNU AGPL version 3](https://www.gnu.org/licenses/agpl-3.0.txt).\
Copyright (C) 2018-2024 K4YT3X and [contributors](https://github.com/k4yt3x/video2x/graphs/contributors).

![AGPLv3](https://www.gnu.org/graphics/agplv3-155x51.png)

This project includes or depends on these following projects:

| Project                                                                               | License         |
| ------------------------------------------------------------------------------------- | --------------- |
| [bloc97/Anime4K](https://github.com/bloc97/Anime4K)                                   | MIT License     |
| [FFmpeg/FFmpeg](https://www.ffmpeg.org/)                                              | LGPLv2.1, GPLv2 |
| [xinntao/Real-ESRGAN-ncnn-vulkan](https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan) | MIT License     |
| [Tencent/ncnn](https://github.com/Tencent/ncnn)                                       | BSD 3-Clause    |

More licensing information can be found in the [NOTICE](NOTICE) file.

## Special Thanks

Special thanks to the following individuals for their significant contributions to the project, listed in alphabetical order.

- [@ArchieMeng](https://github.com/archiemeng)
- [@BrianPetkovsek](https://github.com/BrianPetkovsek)
- [@ddouglas87](https://github.com/ddouglas87)
- [@lhanjian](https://github.com/lhanjian)
- [@nihui](https://github.com/nihui)
- [@sat3ll](https://github.com/sat3ll)

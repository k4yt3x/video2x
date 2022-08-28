<p align="center">
   <img src="https://user-images.githubusercontent.com/21986859/102733190-872a7880-4334-11eb-8e9e-0ca747f130b1.png"/>
   </br>
   <img src="https://img.shields.io/github/v/release/k4yt3x/video2x?style=flat-square"/>
   <img src="https://img.shields.io/github/workflow/status/k4yt3x/video2x/CI?label=CI&style=flat-square"/>
   <img src="https://img.shields.io/github/downloads/k4yt3x/video2x/total?style=flat-square"/>
   <img src="https://img.shields.io/github/license/k4yt3x/video2x?style=flat-square"/>
   <img src="https://img.shields.io/badge/dynamic/json?color=%23e85b46&label=Patreon&query=data.attributes.patron_count&suffix=%20patrons&url=https%3A%2F%2Fwww.patreon.com%2Fapi%2Fcampaigns%2F4507807&style=flat-square"/>
</p>

## [💬 Telegram Discussion Group](https://t.me/video2x)

Join our Telegram discussion group to ask any questions you have about Video2X, chat directly with the developers, or discuss about upscaling technologies and the future of Video2X in general.

## [🪟 Download Windows Releases](https://github.com/k4yt3x/video2x/releases/tag/4.8.1)

The latest Windows update is built based on version 4.8.1. GUI is not available for 5.0.0 yet, but is already under development. Go to the [GUI](https://github.com/k4yt3x/video2x/wiki/GUI) page to see the basic usages of the GUI. Try the [mirror](https://files.k4yt3x.com/Projects/Video2X/latest) if you can't download releases directly from GitHub.

## [📔 Google Colab](https://colab.research.google.com/drive/1gWEwcA9y57EsxwOjmLNmNMXPsafw0kGo)

You can use Video2X on [Google Colab](https://colab.research.google.com/) **for free** if you don't have a powerful GPU of your own. You can borrow a powerful GPU (Tesla K80, T4, P4, or P100) on Google's server for free for a maximum of 12 hours per session. **Please use the free resource fairly** and do not create sessions back-to-back and run upscaling 24/7. This might result in you getting banned. You can get [Colab Pro/Pro+](https://colab.research.google.com/signup/pricing) if you'd like to use better GPUs and get longer runtimes. Usage instructions are embedded in the [Colab Notebook](https://colab.research.google.com/drive/1gWEwcA9y57EsxwOjmLNmNMXPsafw0kGo).

## [🌙 Download Nightly Releases](https://github.com/k4yt3x/video2x/actions/workflows/ci.yml)

Nightly releases are automatically created by the GitHub Actions CI/CD pipelines. They usually contain more experimental features and bug fixes. However, they are much less stable to the stable releases. **You must log in to GitHub to download CI build artifacts.**

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

### GIF Upscaling

![catfru](https://user-images.githubusercontent.com/21986859/81631069-96d4fc80-93f6-11ea-92fb-33d6545055e7.gif)
![catfru4x](https://user-images.githubusercontent.com/21986859/81631070-976d9300-93f6-11ea-9137-072a3b386110.gif)\
_Catfru scaled up to 4x its original size using waifu2x [(original image)](https://gfycat.com/craftyeasygoingankole-capoo-bug-cat)_

### Image Upscaling

![Jill Comparison](https://user-images.githubusercontent.com/21986859/81631903-79a12d80-93f8-11ea-9c3c-f340240cf08c.png)\
_Image 8x upscaling demo ([original image](https://72915.tumblr.com/post/173793265673) by [nananicu](https://twitter.com/nananicu))_

### Standard Test Clip

The following clip can be used to test if your setup works properly. This is also the standard clip used for running performance benchmarks.

- [Standard Test Clip (240P)](https://files.k4yt3x.com/Resources/Videos/standard-test.mp4) 4.54 MiB
- [waifu2x Upscaled Sample (1080P)](https://files.k4yt3x.com/Resources/Videos/standard-waifu2x.mp4) 4.54 MiB
- [Original Ground Truth (1080P)](https://files.k4yt3x.com/Resources/Videos/standard-original.mp4) 22.2 MiB

The original clip came from the anime "さくら荘のペットな彼女."\
Copyright of this clip belongs to 株式会社アニプレックス.

## License

This project is licensed under the [GNU Affero General Public License Version 3 (GNU AGPL v3)](https://www.gnu.org/licenses/agpl-3.0.txt)\
Copyright (c) 2018-2022 K4YT3X and contributors.

![AGPLv3](https://www.gnu.org/graphics/agplv3-155x51.png)

This project includes or depends on these following projects:

| Project                                                                 | License         |
| ----------------------------------------------------------------------- | --------------- |
| [FFmpeg](https://www.ffmpeg.org/)                                       | LGPLv2.1, GPLv2 |
| [waifu2x-ncnn-vulkan](https://github.com/nihui/waifu2x-ncnn-vulkan)     | MIT License     |
| [srmd-ncnn-vulkan](https://github.com/nihui/srmd-ncnn-vulkan)           | MIT License     |
| [realsr-ncnn-vulkan](https://github.com/nihui/realsr-ncnn-vulkan)       | MIT License     |
| [rife-ncnn-vulkan](https://github.com/nihui/rife-ncnn-vulkan)           | MIT License     |
| [realcugan-ncnn-vulkan](https://github.com/nihui/realcugan-ncnn-vulkan) | MIT License     |
| [ffmpeg-python](https://github.com/kkroening/ffmpeg-python)             | Apache-2.0      |
| [Loguru](https://github.com/Delgan/loguru)                              | MIT License     |
| [opencv-python](https://github.com/opencv/opencv-python)                | MIT License     |
| [Pillow](https://github.com/python-pillow/Pillow)                       | HPND License    |
| [Rich](https://github.com/Textualize/rich)                              | MIT License     |
| [pynput](https://github.com/moses-palmer/pynput)                        | LGPLv3.0        |

Legacy versions of this project includes or depends on these following projects:

| Project                                                                     | License              |
| --------------------------------------------------------------------------- | -------------------- |
| [waifu2x-caffe](https://github.com/lltcggie/waifu2x-caffe)                  | MIT License          |
| [waifu2x-converter-cpp](https://github.com/DeadSix27/waifu2x-converter-cpp) | MIT License          |
| [Anime4K](https://github.com/bloc97/Anime4K)                                | MIT License          |
| [Anime4KCPP](https://github.com/TianZerL/Anime4KCPP)                        | MIT License          |
| [Gifski](https://github.com/ImageOptim/gifski)                              | AGPLv3               |
| [tqdm](https://github.com/tqdm/tqdm)                                        | MPLv2.0, MIT License |

More licensing information can be found in the [NOTICES](NOTICES) file.

## Special Thanks

Appreciations given to the following personnel who have contributed significantly to the project.

- [@BrianPetkovsek](https://github.com/BrianPetkovsek)
- [@sat3ll](https://github.com/sat3ll)
- [@ddouglas87](https://github.com/ddouglas87)
- [@lhanjian](https://github.com/lhanjian)
- [@ArchieMeng](https://github.com/archiemeng)
- [@nihui](https://github.com/nihui)

## Similar Projects

- [Dandere2x](https://github.com/CardinalPanda/dandere2x): A lossy video upscaler also built around `waifu2x`, but with video compression techniques to shorten the time needed to process a video.
- [Waifu2x-Extension-GUI](https://github.com/AaronFeng753/Waifu2x-Extension-GUI): A similar project that focuses more and only on building a better graphical user interface. It is built using C++ and Qt5, and currently only supports the Windows platform.

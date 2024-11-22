# Windows

Instructions for building this project on Windows.

## 1. Prerequisites

The following tools must be installed manually:

- [Visual Studio 2022](https://visualstudio.microsoft.com/vs/)
  - Workload: Desktop development with C++
- [winget-cli](https://github.com/microsoft/winget-cli)

## 2. Clone the Repository

```bash
# Install Git if not already installed
winget install -e --id=Git.Git

# Clone the repository
git clone --recurse-submodules https://github.com/k4yt3x/video2x.git
cd video2x
```

## 3. Install Dependencies

```bash
# Install CMake
winget install -e --id=Kitware.CMake

# Install Vulkan SDK
winget install -e --id=KhronosGroup.VulkanSDK

# Versions of manually installed dependencies
$ffmpegVersion = "7.1"
$ncnnVersion = "20240820"

# Download and extract FFmpeg
curl -Lo ffmpeg-shared.zip "https://github.com/GyanD/codexffmpeg/releases/download/$ffmpegVersion/ffmpeg-$ffmpegVersion-full_build-shared.zip"
Expand-Archive -Path ffmpeg-shared.zip -DestinationPath third_party
Rename-Item -Path "third_party/ffmpeg-$ffmpegVersion-full_build-shared" -NewName ffmpeg-shared

# Download and extract ncnn
curl -Lo ncnn-shared.zip "https://github.com/Tencent/ncnn/releases/download/$ncnnVersion/ncnn-$ncnnVersion-windows-vs2022-shared.zip"
Expand-Archive -Path ncnn-shared.zip -DestinationPath third_party
Rename-Item -Path "third_party/ncnn-$ncnnVersion-windows-vs2022-shared" -NewName ncnn-shared
```

## 4. Build the Project

```bash
cmake -S . -B build -DUSE_SYSTEM_NCNN=OFF -DUSE_SYSTEM_SPDLOG=OFF -DUSE_SYSTEM_BOOST=OFF `
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=build/libvideo2x-shared
cmake --build build --config Release --parallel --target install
```

The built binaries will be located in `build/libvideo2x-shared`.

# Windows (Qt6)

Instructions for building the Qt6 GUI of this project on Windows.

## 1. Prerequisites

These dependencies must be installed before building the project. This tutorial assumes that Qt6 has been installed to the default location (`C:\Qt`).

- [Visual Studio 2022](https://visualstudio.microsoft.com/vs/)
  - Workload: Desktop development with C++
- [winget-cli](https://github.com/microsoft/winget-cli)
- [Qt6](https://www.qt.io/download)
  - Component: Qt6 with MSVC 2022 64-bit
  - Component: Qt Creator

## 1. Clone the Repository

```bash
# Install Git if not already installed
winget install -e --id=Git.Git

# Clone the repository
git clone --recurse-submodules https://github.com/k4yt3x/video2x-qt6.git
cd video2x-qt6
```

## 2. Install Dependencies

You need to have the `libvideo2x` shared library built before building the Qt6 GUI. Put the built binaries in `third_party/libvideo2x-shared`.

```bash
# Versions of manually installed dependencies
$ffmpegVersion = "7.1"

# Download and extract FFmpeg
curl -Lo ffmpeg-shared.zip "https://github.com/GyanD/codexffmpeg/releases/download/$ffmpegVersion/ffmpeg-$ffmpegVersion-full_build-shared.zip"
Expand-Archive -Path ffmpeg-shared.zip -DestinationPath third_party
Rename-Item -Path "third_party/ffmpeg-$ffmpegVersion-full_build-shared" -NewName ffmpeg-shared
```

## 3. Build the Project

1. Open the `CMakeLists.txt` file in Qt Creator as the project file.
2. Click on the hammer icon at the bottom left of the window to build the project.
3. Built binaries will be located in the `build` directory.

After the build finishes, you will need to copy the Qt6 DLLs and other dependencies to the build directory to run the application. Before you run the following commands, remove everything in the release directory except for `video2x-qt6.exe` and the `.qm` files as they are not required for running the application. Then, run the following command to copy the Qt6 runtime DLLs:

```bash
C:\Qt\6.8.0\msvc2022_64\bin\windeployqt.exe --release --compiler-runtime .\build\Desktop_Qt_6_8_0_MSVC2022_64bit-Release\video2x-qt6.exe
```

You will also need to copy the `libvideo2x` shared library to the build directory. Copy all files under `third_party/libvideo2x-shared` to the release directory except for `include`, `libvideo2x.lib`, and `video2x.exe`.

Now you should be able to run the application by double-clicking on `video2x-qt6.exe`.

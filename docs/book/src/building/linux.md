# Linux

Instructions for building this project on Linux.

## Arch Linux

Arch users can build the latest version of the project from the AUR package `video2x-git`. The project's repository also contains another PKGBUILD example at `packaging/arch/PKGBUILD`.

```bash
# Build only
git clone https://aur.archlinux.org/video2x-git.git
cd video2x-git
makepkg -s
```

To build manually from the source, follow the instructions below.

```bash
# Install build and runtime dependencies
# See the PKGBUILD file for the list of up-to-date dependencies
pacman -Sy ffmpeg ncnn vulkan-driver opencv spdlog boost-libs
pacman -Sy git cmake make clang pkgconf vulkan-headers openmp boost

# Clone the repository
git clone --recurse-submodules https://github.com/k4yt3x/video2x.git
cd video2x

# Build the project
make build
```

The built binaries will be located in the `build` directory.

## Ubuntu

Ubuntu users can use the `Makefile` to build the project automatically. The `ubuntu2404` and `ubuntu2204` targets are available for Ubuntu 24.04 and 22.04, respectively. `make` will automatically install the required dependencies, build the project, and package it into a `.deb` package file. It is recommended to perform the build in a container to ensure the environment's consistency and to avoid leaving extra build packages on your system.

```bash
# make needs to be installed manually
sudo apt-get update && sudo apt-get install make

# Clone the repository
git clone --recurse-submodules https://github.com/k4yt3x/video2x.git
cd video2x

# Build the project
make ubuntu2404
```

The built `.deb` package will be located under the current directory.

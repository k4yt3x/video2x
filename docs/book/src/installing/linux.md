# Linux

Video2X packages are available for the Linux distros listed below. If you'd like to build it from source code, refer to the [PKGBUILD](https://github.com/k4yt3x/video2x/tree/master/packaging/arch/PKGBUILD) file for a general overview of the required dependencies and commands. If a package is not available for your distro and you prefer not to compile the program from source code, consider using the [container image](running/container.md).

## Arch Linux

- AUR packages, maintained by [@K4YT3X](https://github.com/k4yt3x).
  - [aur/video2x](https://aur.archlinux.org/packages/video2x)
  - [aur/video2x-git](https://aur.archlinux.org/packages/video2x-git)
  - [aur/video2x-qt6](https://aur.archlinux.org/packages/video2x-qt6)
  - [aur/video2x-qt6-git](https://aur.archlinux.org/packages/video2x-qt6-git)
- Chinese Mainland: archlinuxcn packages, maintained by [@Integral-Tech](https://github.com/Integral-Tech).
  - [archlinuxcn/video2x](https://github.com/archlinuxcn/repo/tree/master/archlinuxcn/video2x)
  - [archlinuxcn/video2x-git](https://github.com/archlinuxcn/repo/tree/master/archlinuxcn/video2x-git)
  - [archlinuxcn/video2x-qt6](https://github.com/archlinuxcn/repo/tree/master/archlinuxcn/video2x-qt6)
  - [archlinuxcn/video2x-qt6-git](https://github.com/archlinuxcn/repo/tree/master/archlinuxcn/video2x-qt6-git)

## Gentoo

Gentoo users can enable `gentooplusplus` repository, maintained by [@Eugeniusz-Gienek](https://github.com/Eugeniusz-Gienek), and install the packages from there:

```
# If not done before - install eselect-repository:
emerge --ask app-eselect/eselect-repository
# Enabling gentooplusplus repo
eselect repository enable gentooplusplus
emerge --sync
# (optional) check the use flags and enable the ones which make sense for you.
equery u media-video/video2x::gentooplusplus
# Installing of a CLI version - resulting executable is /usr/bin/video2x
emerge -av media-video/video2x::gentooplusplus
# If GUI is needed (resulting executable is /usr/bin/video2x-qt6), you can install it as well:
emerge -av media-video/video2x-x11::gentooplusplus
###
# P.S. if you don't have any other repositories which have video2x inside them,
# the repo postfix can be skipped - e.g. after enabling the repo you can just do:
# equery u media-video/video2x
# emerge -av media-video/video2x
# emerge -av media-video/video2x-x11
# Which might be a bit more convenient.
```

## Other Distros

Users of other distros can download and use the AppImage from the [releases page](https://github.com/k4yt3x/video2x/releases/latest).

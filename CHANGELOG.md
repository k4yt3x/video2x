# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Automatic selection of the most suitable pixel format for the output video.
- Frame interpolation processing mode.
- More `AVCodecContext` options.
- Support for RIFE ncnn Vulkan.
- Support for specifying arbitrary `AVOptions` for the encoder (#1232).

### Changed

- Improve error handling and error messages.
- Improve the CLI help message structure and clarity.
- Improve CLI argument validation.

### Removed

- The C API for easier maintenance and development.

### Fixed

- Timestamp errors processing frames with PTS equal to 0 (#1222).

## [6.1.1] - 2024-11-07

### Added

- Time remaining, and processing speed to the status bar.

### Fixed

- Stream mapping for cases where the video stream is not the first stream in the input file (#1217).
- The encoder using the wrong color space for the output video.

## [6.1.0] - 2024-11-04

### Added

- A better timer that gets paused when the processing is paused.
- Detection for the validity of the provided GPU ID.
- Status bar and processing statistics. (Video2X Qt6)
- The `--listgpus` option to list available Vulkan GPU devices.
- Vulkan device selection for libplacebo.

### Changed

- Automatically detect if options `colorspace` and `range` are supported by the buffer filter.
- Resource file missing error messages.
- Rewritten the CLI with C++.

### Fixed

- Wide character string paths support on Windows systems without UTF-8 support enabled (#1201).

## [6.0.0] - 2024-10-29

### Added

- The initial release of the 6.0.0 version of Video2X.

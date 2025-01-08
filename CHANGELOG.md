# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Multi-versioning to critical functions to enhance performance in generic architecture builds.
- The feature to copy input streams' metadata to the output streams (#1282).

### Changed

- Improve the CMake optimization flags and option names.

## [6.3.1] - 2024-12-21

### Fixed

- The issue of decreasing PTS precision with increasing video duration (#1269).
- (Video2X Qt6) Errors restoring Real-CUGAN configs in the Edit Task dialog.
- (Video2X Qt6) The incorrect comparison of version numbers.

## [6.3.0] - 2024-12-20

### Added

- A logger manager to provide unified logging across the application.
- Support for Real-CUGAN ncnn Vulkan (#1198).
- (Video2X Qt6) A check to ensure the required VC++ Redistributable version is installed.
- (Video2X Qt6) A configuration manager to save user preferences like the last selected language.
- (Video2X Qt6) A new logging widget to display logs in the UI.
- (Video2X Qt6) Custom options `crf=20` and `preset=slow` to the default encoder options.
- (Video2X Qt6) French translation.
- (Video2X Qt6) The ability to check for available updates and prompt the user to update.
- (Video2X Qt6) Tooltips for processor and encoder options.

### Changed

- Improve optimization flags and add namespaces for better code organization.
- (Video2X Qt6) Add processor names to processed videos instead of `.processed`.
- (Video2X Qt6) The output video suffix from auto-generated to `.mkv`.

### Fixed

- Make the encoder always use the calculated PTS with corrected math.
- (Video2X Qt6) The issue where task configs are being restored incorrectly in the UI.

## [6.2.0] - 2024-12-11

### Added

- Automatic selection of the most suitable pixel format for the output video.
- Frame interpolation processing mode.
- More `AVCodecContext` options.
- Support for RIFE ncnn Vulkan.
- Support for specifying arbitrary `AVOptions` for the encoder (#1232).
- (Video2X Qt6) Visual C++ Redistributable version check to the installer.

### Changed

- Improve CLI argument validation.
- Improve error handling and error messages.
- Improve the CLI help message structure and clarity.
- (Video2X Qt6) Improve the UI with a complete redesign.

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
- The `--listgpus` option to list available Vulkan GPU devices.
- Vulkan device selection for libplacebo.
- (Video2X Qt6) Status bar and processing statistics.

### Changed

- Automatically detect if options `colorspace` and `range` are supported by the buffer filter.
- Resource file missing error messages.
- Rewritten the CLI with C++.

### Fixed

- Wide character string paths support on Windows systems without UTF-8 support enabled (#1201).

## [6.0.0] - 2024-10-29

### Added

- The initial release of the 6.0.0 version of Video2X.

# Architecture

The basic working principals of Video2X and its historical architectures.

## Video2X <=4.0.0 (Legacy)

Below is the earliest architecture of Video2X. It extracts all of the frames from the video using FFmpeg, processes all frames, and stores them into a folder before running FFmpeg again to convert all of the frames back into a video. The drawbacks of this approach are apparent:

- Storing all frames of the video on disk twice requires a huge amount of storage, often hundreds of gigabytes.
- A lot of disk I/O (reading from/writing to disks) operations occur, which is inefficient. Each step stores its processing results to disk, and the next step has to read them from disk again.

![Video2Xv4](https://github.com/user-attachments/assets/976a93ff-efad-418f-a3e2-272e84db2d74)\
_Video2X architecture before version 5.0.0_

## Video2X 5.0.0 (Legacy)

Video2X 5.0.0's architecture was designed to address the inefficient disk I/O issues. This version uses frame serving and streamlines the process. All stages are started simultaneously, and frames are passed between stages through stdin/stdout pipes. However, this architecture also has several issues:

- At least two instances of FFmpeg will be started, three in the case of Anime4K.
- Passing frames through stdin/stdout is unstable. If frame sizes are incorrect, FFmpeg will hang waiting for the next frame.
- The frames entering and leaving each stage must be RGB24, even if they don't need to be. For instance, if the upscaler used is Anime4K, yuv420p is acceptable, but the frame is first converted by the decoder to RGB24, then converted back into YUV colorspace for libplacebo.

![Video2Xv5](https://github.com/user-attachments/assets/d1f38034-a5d3-4c7e-92bf-a5b30fa9ac72)\
_Video2X 5.x.x architecture_

## Video2X 6.0.0 (Current)

Video2X 6.0.0 (Current)

The newest version of Video2X's architecture addresses the issues of the previous architecture while improving efficiency.

- Frames are only decoded once and encoded once with FFmpeg's libavformat.
- Frames are passed as `AVFrame` structs. Their pixel formats are only converted when needed.
- Frames always stay in RAM, avoiding bottlenecks from disk I/O and pipes.
- Frames always stay in the hardware (GPU) unless they need to be downloaded to be processed by software (partially implemented).

![Video2Xv6 drawio](https://github.com/user-attachments/assets/c5d5fc3b-8688-4d50-b7c0-3b5d825a8c69)\
_Video2X 6.0.0 architecture_

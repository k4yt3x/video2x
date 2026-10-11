# Command Line

Instructions for running Video2X from the command line.

This page does not cover all the options available. For help with more options available, run Video2X with the `--help` argument.

## Basics

Use the following command to upscale a video by 4x with RealESRGAN:

```bash
video2x -i input.mp4 -o output.mp4 -p realesrgan -s 4 --realesrgan-model realesr-animevideov3
```

Use the following command to upscale a video to with libplacebo + Anime4Kv4 Mode A+A:

```bash
video2x -i input.mp4 -o output.mp4 -w 3840 -h 2160 -p libplacebo --libplacebo-shader anime4k-v4-a+a
```

## Output Resolution

How the output resolution is set depends on the processor:

- **Real-ESRGAN and Real-CUGAN** upscale by a fixed integer factor set with `--scaling-factor, -s`, which must be 2, 3, or 4. The output is always the input size multiplied by this factor, since each model is trained for a specific scale. The `--width, -w` and `--height, -h` arguments are ignored by these processors.
- **libplacebo** scales to any output size, which must be set with the `--width, -w` and `--height, -h` arguments.

For example, Real-ESRGAN with `-s 2` upscales a 1920x1080 video to 3840x2160. There is no way to make it output 2560x1440 (QHD) directly. To get a resolution that is not an integer multiple of the input, either:

- Use libplacebo with the desired output size:

  ```bash
  video2x -i input.mp4 -o output.mp4 -w 2560 -h 1440 -p libplacebo --libplacebo-shader anime4k-v4-a+a
  ```

- Or upscale with Real-ESRGAN or Real-CUGAN using the smallest factor that reaches the target size, then downscale the result with FFmpeg:

  ```bash
  video2x -i input.mp4 -o upscaled.mp4 -p realesrgan -s 2 --realesrgan-model realesr-animevideov3
  ffmpeg -i upscaled.mp4 -vf scale=2560:1440:flags=lanczos -c:a copy output.mp4
  ```

## Advanced

It is possible to specify custom MPV-compatible GLSL shader files with the `--libplacebo-shader` argument:

```bash
video2x -i input.mp4 -o output.mp4 -p libplacebo -w 3840 -h 2160 --libplacebo-shader path/to/custom/shader.glsl
```

List the available GPUs with `--list-gpus, -l`:

```bash
$ video2x --list-gpus
0. NVIDIA RTX A6000
        Type: Discrete GPU
        Vulkan API Version: 1.3.289
        Driver Version: 565.228.64
```

Select which GPU to use with the `--gpu, -g` argument:

```bash
video2x -i input.mp4 -o output.mp4 -p realesrgan -s 4 --realesrgan-model realesr-animevideov3 -g 1
```

Specify arbitrary extra FFmpeg encoder options with the `--extra-encoder-options, -e` argument:

```bash
video2x -i input.mkv -o output.mkv -p realesrgan --realesrgan-model realesrgan-plus -s 4 -c libx264rgb -e crf=17 -e preset=veryslow -e tune=film
```

## Encoder Options

Video2X uses FFmpeg's C libraries to encode videos. Encoder options are specified in two ways:

- **Common options** shared by all encoders are stored in a [`AVCodecContext`](https://ffmpeg.org/doxygen/trunk/structAVCodecContext.html) struct. Below are some options set through `AVCodecContext`:
  - Codec
  - Pixel format
  - Bitrate
  - Keyframe interval
  - Minimum and maximum quantizer
  - GOP size
- **Encoder-specific** options are stored in [`AVOption`](https://ffmpeg.org/doxygen/trunk/structAVOption.html) structs and set with the [`av_opt_set`](https://ffmpeg.org/doxygen/trunk/group__opt__set__funcs.html#ga5fd4b92bdf4f392a2847f711676a7537) function. Below are some encoder-specific options for `libx264`:
  - CRF
  - Preset
  - Tune
  - Profile

Common options can only be set through Video2X's command line arguments. You can run `video2x --help` and see the `Encoder options` section to see the supported options.

You can specify encoder-specific options in Video2X using the `--extra-encoder-option` (`-e`) argument. To view the available options for a particular codec, run:

```bash
ffmpeg -h encoder=$ENCODER
```

For example, to view the available options for `libx264`, run:

```console
$ ffmpeg -h encoder=libx264
Encoder libx264 [libx264 H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10]:
    General capabilities: dr1 delay threads
    Threading capabilities: other
    Supported pixel formats: yuv420p yuvj420p yuv422p yuvj422p yuv444p yuvj444p nv12 nv16 nv21 yuv420p10le yuv422p10le yuv444p10le nv20le gray gray10le
libx264 AVOptions:
  -preset            <string>     E..V....... Set the encoding preset (cf. x264 --fullhelp) (default "medium")
  -tune              <string>     E..V....... Tune the encoding params (cf. x264 --fullhelp)
  -profile           <string>     E..V....... Set profile restrictions (cf. x264 --fullhelp)
  -fastfirstpass     <boolean>    E..V....... Use fast settings when encoding first pass (default true)
  -level             <string>     E..V....... Specify level (as defined by Annex A)
  -passlogfile       <string>     E..V....... Filename for 2 pass stats
  -wpredp            <string>     E..V....... Weighted prediction for P-frames
  -a53cc             <boolean>    E..V....... Use A53 Closed Captions (if available) (default true)
  -x264opts          <string>     E..V....... x264 options
  -crf               <float>      E..V....... Select the quality for constant quality mode (from -1 to FLT_MAX) (default -1)
  -crf_max           <float>      E..V....... In CRF mode, prevents VBV from lowering quality beyond this point. (from -1 to FLT_MAX) (default -1)
  -qp                <int>        E..V....... Constant quantization parameter rate control method (from -1 to INT_MAX) (default -1)
...
```

You can then set the encoder-specific options with the `-e` argument. The `-e` argument can be used multiple times to set multiple options. For example, the following arguments set the CRF to 17, the preset to `veryslow`, and the tune to `film` for `libx264`:

```console
-e crf=17 -e preset=veryslow -e tune=film
```

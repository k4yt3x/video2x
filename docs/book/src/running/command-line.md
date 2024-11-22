# Command Line

Instructions for running Video2X from the command line.

This page does not cover all the options available. For help with more options available, run Video2X with the `--help` argument.

## Basics

Use the following command to upscale a video by 4x with RealESRGAN:

```bash
video2x -i input.mp4 -o output.mp4 -f realesrgan -r 4 -m realesr-animevideov3
```

Use the following command to upscale a video to with libplacebo + Anime4Kv4 Mode A+A:

```bash
video2x -i input.mp4 -o output.mp4 -f libplacebo -s anime4k-v4-a+a -w 3840 -h 2160
```

## Advanced

It is possible to specify custom MPV-compatible GLSL shader files with the `--shader, -s` argument:

```bash
video2x -i input.mp4 -o output.mp4 -f libplacebo -s path/to/custom/shader.glsl -w 3840 -h 2160
```

List the available GPUs with `--list-gpus, -l`:

```bash
$video2x --list-gpus
0. NVIDIA RTX A6000
        Type: Discrete GPU
        Vulkan API Version: 1.3.289
        Driver Version: 565.228.64
```

Select which GPU to use with the `--gpu, -g` argument:

```bash
video2x -i input.mp4 -o output.mp4 -f realesrgan -r 4 -m realesr-animevideov3 -g 1
```

Specify arbitrary extra FFmepg encoder options with the `--extra-encoder-options, -e` argument:

```bash
video2x -i input.mkv -o output.mkv -f realesrgan -m realesrgan-plus -r 4 -c libx264rgb -e crf=17 -e preset=veryslow -e tune=film
```

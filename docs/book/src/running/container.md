# Container

Instructions for running the Video2X container.

## Prerequisites

- Docker, Podman, or another OCI-compatible runtime
- A GPU that supports the Vulkan API
  - Check the [Vulkan Hardware Database](https://vulkan.gpuinfo.org/) to see if your GPU supports Vulkan

## Upscaling a Video

This section documents how to upscale a video. Replace `$TAG` with an appropriate container tag. A list of available tags can be found [here](https://github.com/k4yt3x/video2x/pkgs/container/video2x) (e.g., `6.1.1`).

### AMD GPUs

Make sure your host has the proper GPU and Vulkan libraries and drivers, then use the following command to launch the container:

```shell
docker run --gpus all -it --rm -v $PWD/data:/host ghcr.io/k4yt3x/video2x:$TAG -i standard-test.mp4 -o output.mp4 -p realesrgan -s 4 --realesrgan-model realesr-animevideov3
```

### NVIDIA GPUs

In addition to installing the proper drivers on your host, `nvidia-docker2` (NVIDIA Container Toolkit) must also be installed on the host to use NVIDIA GPUs in containers. Below are instructions for how to install it on some popular Linux distributions:

- Debian/Ubuntu
  - Follow the [official guide](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/install-guide.html#setting-up-nvidia-container-toolkit) to install `nvidia-docker2`
- Arch/Manjaro
  - Install `nvidia-container-toolkit` from the AUR
  - E.g., `yay -S nvidia-container-toolkit`

Once all the prerequisites are installed, you can launch the container:

```shell
docker run --gpus all -it --rm -v $PWD:/host ghcr.io/k4yt3x/video2x:$TAG -i standard-test.mp4 -o output.mp4 -p realesrgan -s 4 --realesrgan-model realesr-animevideov3
```

Depending on the version of your nvidia-docker and some other mysterious factors, you can also try setting `no-cgroups = true` in `/etc/nvidia-container-runtime/config.toml` and adding the NVIDIA devices into the container if the command above doesn't work:

```shell
docker run --gpus all --device=/dev/nvidia0 --device=/dev/nvidiactl --runtime nvidia -it --rm -v $PWD:/host ghcr.io/k4yt3x/video2x:$TAG -i standard-test.mp4 -o output.mp4 -p realesrgan -s 4 --realesrgan-model realesr-animevideov3
```

If you are still getting a `vkEnumeratePhysicalDevices failed -3` error at this point, try adding the `--privileged` flag to give the container the same level of permissions as the host:

```shell
docker run --gpus all --privileged -it --rm -v $PWD:/host ghcr.io/k4yt3x/video2x:$TAG -i standard-test.mp4 -o output.mp4 -p realesrgan -s 4 --realesrgan-model realesr-animevideov3
```

### Intel GPUs

Similar to NVIDIA GPUs, you can add `--gpus all` or `--device /dev/dri` to pass the GPU into the container. Adding `--privileged` might help with the performance (thanks @NukeninDark).

```shell
docker run --gpus all --privileged -it --rm -v $PWD:/host ghcr.io/k4yt3x/video2x:$TAG -i standard-test.mp4 -o output.mp4 -p realesrgan -s 4 --realesrgan-model realesr-animevideov3
```

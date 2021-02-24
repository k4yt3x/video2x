# Name: Video2X Dockerfile
# Creator: Danielle Douglas
# Date Created: Unknown
# Last Modified: January 14, 2020

# Editor: Lhanjian
# Last Modified: May 24, 2020

# Editor: K4YT3X
# Last Modified: June 13, 2020

# using Ubuntu LTS 19.10
# Ubuntu 20.x is incompatible with Nvidia libraries
FROM ubuntu:19.10

# file mainainter labels
LABEL maintainer="Danielle Douglas <ddouglas87@gmail.com>"
LABEL maintainer="Lhanjian <lhjay1@foxmail.com>"
LABEL maintainer="K4YT3X <k4yt3x@k4yt3x.com>"

RUN sed -i 's/archive.ubuntu.com/old-releases.ubuntu.com/g' /etc/apt/sources.list
RUN sed -i 's/security.ubuntu.com/old-releases.ubuntu.com/g' /etc/apt/sources.list

# run installation
RUN apt-get update \
    && apt-get install -y git-core \
    && git clone --recurse-submodules --progress https://github.com/k4yt3x/video2x.git /tmp/video2x/video2x \
    && bash -e /tmp/video2x/video2x/src/video2x_setup_ubuntu.sh /

WORKDIR /host
ENTRYPOINT ["python3.8", "/video2x/src/video2x.py"]

ENV NVIDIA_DRIVER_CAPABILITIES all
ENV DEBIAN_FRONTEND teletype

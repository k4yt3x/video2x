#!/usr/bin/bash -e
# Name: Video2X Setup Script (Ubuntu)
# Creator: K4YT3X
# Date Created: June 5, 2020
# Last Modified: June 5, 2020

# help message if input is incorrect of if -h/--help is specified
if [ "$1" == "-h" ] || [ "$1" == "--help" ] || [ "$#" -gt 2 ]; then
    echo "usage: $0 INSTALLATION_PATH TEMP"
    exit 0
fi

# set intallation path if specified
if [ ! -z "$1" ]; then
    INSTALLATION_PATH=$1
else
    INSTALLATION_PATH="$HOME/.local/share"
fi

# set temp directory location if specified
if [ ! -z "$2" ]; then
    TEMP=$2
else
    TEMP="/tmp/video2x"
fi

# environment variables
export DEBIAN_FRONTEND="noninteractive"

# install basic utilities and add PPAs
apt-get update
apt-get install -y --no-install-recommends apt-utils software-properties-common

# add PPAs and sources
add-apt-repository -y ppa:apt-fast/stable
add-apt-repository -y ppa:graphics-drivers/ppa
apt-get install -y --no-install-recommends apt-fast
apt-fast update

# install runtime packages
apt-fast install -y --no-install-recommends ffmpeg libmagic1 nvidia-cuda-toolkit nvidia-driver-440 python3.8

# install compilation packages
apt-fast install -y --no-install-recommends git-core curl wget ca-certificates gnupg2 python3-dev python3-pip python3-setuptools python3-wheel

# add Nvidia sources
curl -fsSL https://developer.download.nvidia.com/compute/cuda/repos/ubuntu1804/x86_64/7fa2af80.pub | apt-key add -
echo "deb https://developer.download.nvidia.com/compute/machine-learning/repos/ubuntu1804/x86_64 /" >/etc/apt/sources.list.d/nvidia-ml.list
apt-fast update

# install python3 packages
git clone --recurse-submodules --progress https://github.com/k4yt3x/video2x.git --depth=1 $INSTALLATION_PATH/video2x
python3.8 -m pip install -U pip
python3.8 -m pip install -U -r $INSTALLATION_PATH/video2x/src/requirements.txt
mkdir -v -p $INSTALLATION_PATH/video2x/src/dependencies

# install gifski
apt-fast install -y --no-install-recommends cargo
cargo install gifski

# install waifu2x-caffe
apt-fast install -y --no-install-recommends autoconf build-essential cmake gcc-8 libatlas-base-dev libboost-atomic-dev libboost-chrono-dev libboost-date-time-dev libboost-filesystem-dev libboost-iostreams-dev libboost-python-dev libboost-system-dev libboost-thread-dev libcudnn7 libcudnn7-dev libgflags-dev libgoogle-glog-dev libhdf5-dev libleveldb-dev liblmdb-dev libopencv-dev libprotobuf-dev libsnappy-dev protobuf-compiler python-numpy texinfo yasm zlib1g-dev

git clone --recurse-submodules --depth=1 --progress --recurse-submodules https://github.com/nagadomi/waifu2x-caffe-ubuntu.git $TEMP/waifu2x-caffe-ubuntu
git clone --recurse-submodules --progress --depth=1 https://github.com/nagadomi/caffe.git $TEMP/waifu2x-caffe-ubuntu/caffe

apt-get purge -y gcc g++
ln -s /usr/bin/gcc-8 /usr/bin/gcc
ln -s /usr/bin/g++-8 /usr/bin/g++
mkdir -v -p $TEMP/waifu2x-caffe-ubuntu/build
cd $TEMP/waifu2x-caffe-ubuntu/build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr
make -j$(nproc) install

mv -v /tmp/video2x/waifu2x-caffe-ubuntu/bin $INSTALLATION_PATH/video2x/src/dependencies/waifu2x-caffe
mv -v /tmp/video2x/waifu2x-caffe-ubuntu/build/waifu2x-caffe $INSTALLATION_PATH/video2x/src/dependencies/waifu2x-caffe/waifu2x-caffe

# install waifu2x-converter-cpp
apt-fast install -y --no-install-recommends build-essential cmake libopencv-dev ocl-icd-opencl-dev
git clone --recurse-submodules --depth=1 --progress https://github.com/DeadSix27/waifu2x-converter-cpp $TEMP/waifu2x-converter-cpp
mkdir -v $TEMP/waifu2x-converter-cpp/build
cd $TEMP/waifu2x-converter-cpp/build
cmake ..
make -j$(nproc)
mv -v $TEMP/waifu2x-converter-cpp/build $INSTALLATION_PATH/video2x/src/dependencies/waifu2x-converter-cpp
mv -v $TEMP/waifu2x-converter-cpp/models_rgb $INSTALLATION_PATH/video2x/src/dependencies/waifu2x-converter-cpp/models_rgb

# install waifu2x-ncnn-vulkan
# download libvulkan1
apt-fast install -y --no-install-recommends libvulkan1 unzip jq

# get latest release JSON as a string
echo "Fetching latest waifu2x-ncnn-vulkan release information using GitHub API"
waifu2x_ncnn_vulkan_latest_release=$(curl -s https://api.github.com/repos/nihui/waifu2x-ncnn-vulkan/releases/latest)

# count the number of assets in this release
assets=$(echo "$waifu2x_ncnn_vulkan_latest_release" | jq -r '.assets | length')

# iterate through each of the assets and see if the name of the asset matches what we're looking for
for i in $(seq $assets $END); do
    if echo "$waifu2x_ncnn_vulkan_latest_release" | jq -r ".assets["$(($i - 1))"].name" | egrep "^waifu2x-ncnn-vulkan-[0-9]*-linux\.zip$"; then
        download_link=$(echo "$waifu2x_ncnn_vulkan_latest_release" | jq -r ".assets["$(($i - 1))"].browser_download_url")
        break
    fi
done

# check if download_link variable is set
if [ -z "$download_link" ]; then
    echo "$waifu2x_ncnn_vulkan_latest_release"
    echo "Error: unable to find waifu2x-ncnn-vulkan download link or GitHub API rate limit exceeded"
    exit 1
fi

waifu2x_ncnn_vulkan_zip="$TEMP/waifu2x-ncnn-vulkan-linux.zip"
echo "Downloading $download_link to $waifu2x_ncnn_vulkan_zip"
wget "$download_link" -O "$waifu2x_ncnn_vulkan_zip"
unzip "$waifu2x_ncnn_vulkan_zip" -d $TEMP/waifu2x-ncnn-vulkan
mv -v $TEMP/waifu2x-ncnn-vulkan/waifu2x-ncnn-vulkan-*-linux $INSTALLATION_PATH/video2x/src/dependencies/waifu2x-ncnn-vulkan

# install srmd-ncnn-vulkan
# download libvulkan1
apt-fast install -y --no-install-recommends libvulkan1 unzip jq

# get latest release JSON as a string
echo "Fetching latest srmd-ncnn-vulkan release information using GitHub API"
srmd_ncnn_vulkan_latest_release=$(curl -s https://api.github.com/repos/nihui/srmd-ncnn-vulkan/releases/latest)

# count the number of assets in this release
assets=$(echo "$srmd_ncnn_vulkan_latest_release" | jq -r '.assets | length')

# iterate through each of the assets and see if the name of the asset matches what we're looking for
for i in $(seq $assets $END); do
    if echo "$srmd_ncnn_vulkan_latest_release" | jq -r ".assets["$(($i - 1))"].name" | egrep "^srmd-ncnn-vulkan-[0-9]*-linux\.zip$"; then
        download_link=$(echo "$srmd_ncnn_vulkan_latest_release" | jq -r ".assets["$(($i - 1))"].browser_download_url")
        break
    fi
done

# check if download_link variable is set
if [ -z "$download_link" ]; then
    echo "$srmd_ncnn_vulkan_latest_release"
    echo "Error: unable to find srmd-ncnn-vulkan download link or GitHub API rate limit exceeded"
    exit 1
fi

srmd_ncnn_vulkan_zip="$TEMP/srmd-ncnn-vulkan-linux.zip"
echo "Downloading $download_link to $srmd_ncnn_vulkan_zip"
wget "$download_link" -O "$srmd_ncnn_vulkan_zip"
unzip "$srmd_ncnn_vulkan_zip" -d $TEMP/srmd-ncnn-vulkan
mv -v $TEMP/srmd-ncnn-vulkan/srmd-ncnn-vulkan-*-linux $INSTALLATION_PATH/video2x/src/dependencies/srmd-ncnn-vulkan

# install realsr-ncnn-vulkan
# download libvulkan1
apt-fast install -y --no-install-recommends libvulkan1 unzip jq

# get latest release JSON as a string
echo "Fetching latest realsr-ncnn-vulkan release information using GitHub API"
realsr_ncnn_vulkan_latest_release=$(curl -s https://api.github.com/repos/nihui/realsr-ncnn-vulkan/releases/latest)

# count the number of assets in this release
assets=$(echo "$realsr_ncnn_vulkan_latest_release" | jq -r '.assets | length')

# iterate through each of the assets and see if the name of the asset matches what we're looking for
for i in $(seq $assets $END); do
    if echo "$realsr_ncnn_vulkan_latest_release" | jq -r ".assets["$(($i - 1))"].name" | egrep "^realsr-ncnn-vulkan-[0-9]*-linux\.zip$"; then
        download_link=$(echo "$realsr_ncnn_vulkan_latest_release" | jq -r ".assets["$(($i - 1))"].browser_download_url")
        break
    fi
done

# check if download_link variable is set
if [ -z "$download_link" ]; then
    echo "$realsr_ncnn_vulkan_latest_release"
    echo "Error: unable to find realsr-ncnn-vulkan download link or GitHub API rate limit exceeded"
    exit 1
fi

realsr_ncnn_vulkan_zip="$TEMP/realsr-ncnn-vulkan-linux.zip"
echo "Downloading $download_link to $realsr_ncnn_vulkan_zip"
wget "$download_link" -O "$realsr_ncnn_vulkan_zip"
unzip "$realsr_ncnn_vulkan_zip" -d $TEMP/realsr-ncnn-vulkan
mv -v $TEMP/realsr-ncnn-vulkan/realsr-ncnn-vulkan-*-linux $INSTALLATION_PATH/video2x/src/dependencies/realsr-ncnn-vulkan

# rewrite config file values
python3.8 - << EOF
import yaml

with open('/video2x/src/video2x.yaml', 'r') as template:
    template_dict = yaml.load(template, Loader=yaml.FullLoader)
    template.close()

template_dict['ffmpeg']['ffmpeg_path'] = '/usr/bin'
template_dict['gifski']['gifski_path'] = '/root/.cargo/bin/gifski'
template_dict['waifu2x_caffe']['path'] = '/video2x/src/dependencies/waifu2x-caffe/waifu2x-caffe'
template_dict['waifu2x_converter_cpp']['path'] = '/video2x/src/dependencies/waifu2x-converter-cpp/waifu2x-converter-cpp'
template_dict['waifu2x_ncnn_vulkan']['path'] = '/video2x/src/dependencies/waifu2x-ncnn-vulkan/waifu2x-ncnn-vulkan'
template_dict['srmd_ncnn_vulkan']['path'] = '/video2x/src/dependencies/srmd-ncnn-vulkan/srmd-ncnn-vulkan'
template_dict['realsr_ncnn_vulkan']['path'] = '/video2x/src/dependencies/realsr-ncnn-vulkan/realsr-ncnn-vulkan'
template_dict['anime4kcpp']['path'] = '/video2x/src/dependencies/anime4kcpp/anime4kcpp'

# write configuration into file
with open('/video2x/src/video2x.yaml', 'w') as config:
    yaml.dump(template_dict, config)
EOF

# clean up temp directory
# purge default utilities
apt-get purge -y git-core curl wget ca-certificates gnupg2 python3-dev python3-pip python3-setuptools

# purge waifu2x-caffe build dependencies
apt-get purge -y autoconf build-essential cmake gcc-8 libatlas-base-dev libboost-atomic-dev libboost-chrono-dev libboost-date-time-dev libboost-filesystem-dev libboost-iostreams-dev libboost-python-dev libboost-system-dev libboost-thread-dev libcudnn7 libcudnn7-dev libgflags-dev libgoogle-glog-dev libhdf5-dev libleveldb-dev liblmdb-dev libopencv-dev libprotobuf-dev libsnappy-dev protobuf-compiler python-numpy texinfo yasm zlib1g-dev

# purge waifu2x-converter-cpp build dependencies
apt-get purge -y libopencv-dev ocl-icd-opencl-dev

# purge waifu2x/srmd/realsr-ncnn-vulkan build dependencies
apt-get purge -y libvulkan1 unzip jq

# run autoremove and purge all unused packages
apt-get autoremove --purge -y

# remove temp directory
rm -vrf $TEMP

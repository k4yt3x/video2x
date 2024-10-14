pkgname=video2x
pkgver=r840.ecbc512
pkgrel=1
pkgdesc="A machine learning-based lossless video super resolution framework"
arch=('x86_64')
url="https://github.com/k4yt3x/video2x"
license=('AGPL3')
depends=('ffmpeg' 'ncnn' 'vulkan-driver')
makedepends=('git' 'cmake' 'make' 'clang' 'pkgconf' 'vulkan-headers' 'openmp' 'spdlog')

pkgver() {
    printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

prepare() {
    git submodule update --init --recursive
}

build() {
    cmake -B build -S .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build --config Release --parallel
}

package() {
    DESTDIR="$pkgdir" cmake --install build
}


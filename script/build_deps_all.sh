#!/bin/bash
set -e

mkdir -p libs
cd libs

if [ -z $cmake ]; then
  cmake="cmake"
fi
if [ -z $deps ]; then
  deps="deps"
fi

mkdir -p $deps
cd $deps
INSTALL_PREFIX=$PWD/built
rm -rf $INSTALL_PREFIX
mkdir -p $INSTALL_PREFIX

#### clean ####
clean() {
  rm -rf dl.zip yaml-* zxing-* protobuf
}

#### ZXing v2.3.0 ####
curl -L -o dl.zip https://github.com/zxing-cpp/zxing-cpp/archive/refs/tags/v2.3.0.zip
unzip dl.zip

cd zxing-*
mkdir -p build
cd build

$cmake .. -GNinja -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF -DBUILD_BLACKBOX_TESTS=OFF -DCMAKE_OSX_ARCHITECTURES=$1 -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX
ninja && ninja install

cd ../..

#### yaml-cpp ####
curl -L -o dl.zip https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.7.0.zip
unzip dl.zip

cd yaml-*
mkdir -p build
cd build

$cmake .. -GNinja -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=$1 -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX
ninja && ninja install

cd ../..

#### protobuf ####
git clone --recurse-submodules -b v30.2 --depth 1 --shallow-submodules https://github.com/parhelia512/protobuf

mkdir -p protobuf/build
cd protobuf/build

$cmake .. -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -Dprotobuf_MSVC_STATIC_RUNTIME=OFF \
  -Dprotobuf_BUILD_TESTS=OFF \
  -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX \
  -Dprotobuf_BUILD_PROTOBUF_BINARIES=ON \
  -Dprotobuf_BUILD_LIBUPB=OFF \
  -DCMAKE_OSX_ARCHITECTURES=$1 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 \
  -DCMAKE_CXX_STANDARD=17
ninja && ninja install

cd ../..

####
clean

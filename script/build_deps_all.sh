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
  rm -rf protobuf
}

#### protobuf ####
git clone --recurse-submodules -b v31.0 --depth 1 --shallow-submodules https://github.com/protocolbuffers/protobuf

mkdir -p protobuf/build
cd protobuf/build

$cmake .. -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -Dprotobuf_BUILD_TESTS=OFF \
  -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX \
  -Dprotobuf_BUILD_PROTOBUF_BINARIES=ON \
  -DCMAKE_OSX_ARCHITECTURES=$1 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 \
  -DCMAKE_CXX_STANDARD=17
ninja && ninja install

cd ../..

####
clean

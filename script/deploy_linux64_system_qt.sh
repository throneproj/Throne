#!/bin/bash
set -e

rm -rf $DEST
mkdir -p $DEST

#### copy binary ####
cp $GITHUB_WORKSPACE/build/Throne $DEST

#### copy Throne.png ####
cp $GITHUB_WORKSPACE/res/public/Throne.png $DEST

#### copy Core ####
cd download-artifact
cd *linux-amd64
tar xvzf artifacts.tgz -C ../../
cd ../..
cp deployment/linux-amd64/ThroneCore $DEST
cp deployment/linux-amd64/libcronet.so $DEST
rm -rf deployment/linux-amd64

# handle debug info
objcopy --only-keep-debug $DEST/Throne $DEST/Throne.debug
strip --strip-debug --strip-unneeded $DEST/Throne
objcopy --add-gnu-debuglink=$DEST/Throne.debug $DEST/Throne

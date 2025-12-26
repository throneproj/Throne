#!/bin/bash
set -e

if [[ $(uname -m) == 'aarch64' || $(uname -m) == 'arm64' ]]; then
  ARCH="arm64"
else
  ARCH="amd64"
fi

source script/env_deploy.sh
DEST=$DEPLOYMENT/linux-system-qt-$ARCH
rm -rf $DEST
mkdir -p $DEST

#### copy binary ####
cp $BUILD/Throne $DEST

#### copy Throne.png ####
cp ./res/public/Throne.png $DEST

#### copy Core ####
cd download-artifact
cd *linux-$ARCH
tar xvzf artifacts.tgz -C ../../
cd ../..
cp deployment/linux-$ARCH/Core $DEST
rm -rf deployment/linux-$ARCH

# handle debug info
objcopy --only-keep-debug $DEST/Throne $DEST/Throne.debug
strip --strip-debug --strip-unneeded $DEST/Throne
objcopy --add-gnu-debuglink=$DEST/Throne.debug $DEST/Throne

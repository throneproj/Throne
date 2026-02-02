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
if [ -d "download-artifact" ]; then
  cd download-artifact
  cd *linux-$ARCH
  tar xvzf artifacts.tgz -C ../../
  cd ../..
  cp deployment/linux-$ARCH/Core $DEST
  rm -rf deployment/linux-$ARCH
else
  # локальная сборка: Core уже в deployment/linux-$ARCH от build_go.sh
  if [ -f "deployment/linux-$ARCH/Core" ]; then
    cp deployment/linux-$ARCH/Core $DEST
  else
    echo "Предупреждение: deployment/linux-$ARCH/Core не найден. Соберите Core: ./script/build_go.sh"
  fi
fi

# handle debug info
objcopy --only-keep-debug $DEST/Throne $DEST/Throne.debug
strip --strip-debug --strip-unneeded $DEST/Throne
objcopy --add-gnu-debuglink=$DEST/Throne.debug $DEST/Throne

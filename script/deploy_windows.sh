#!/bin/bash
set -e

source script/env_deploy.sh
if [[ $1 == 'i686' ]]; then
  ARCH="windowslegacy-386"
  DEST=$DEPLOYMENT/windows32
else
  if [[ $1 == 'x86_64' ]]; then
    ARCH="windowslegacy-amd64"
    DEST=$DEPLOYMENT/windowslegacy64
  else
    ARCH="windows-amd64"
    DEST=$DEPLOYMENT/windows64
  fi
fi
rm -rf $DEST
mkdir -p $DEST

#### copy exe ####
cp $BUILD/Throne.exe $DEST
cp $BUILD/*pdb $DEST || true

cd download-artifact
cd *$ARCH
tar xvzf artifacts.tgz -C ../../
cd ../..

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

#### get the pdb ####
curl -fLJO https://github.com/rainers/cv2pdb/releases/download/v0.53/cv2pdb-0.53.zip
7z x cv2pdb-0.53.zip -ocv2pdb
./cv2pdb/cv2pdb64.exe ./build/Throne.exe ./tmp.exe ./Throne.pdb
rm -rf cv2pdb-0.53.zip cv2pdb
cd build
strip -s Throne.exe
cd ..
rm tmp.exe
mv Throne.pdb $DEST

#### download srslist ####
curl -fLso $DEST/srslist "https://raw.githubusercontent.com/throneproj/routeprofiles/rule-set/list"

#### copy exe ####
cp $BUILD/Throne.exe $DEST

cd download-artifact
cd *$ARCH
tar xvzf artifacts.tgz -C ../../
cd ../..

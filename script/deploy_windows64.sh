#!/bin/bash
set -e

source script/env_deploy.sh
DEST=$DEPLOYMENT/windows64
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

#### copy exe ####
cp $BUILD/Throne.exe $DEST

cd download-artifact
cd *windows-amd64
tar xvzf artifacts.tgz -C ../../
cd ..
cd *public_res
tar xvzf artifacts.tgz -C ../../
cd ../..

mv $DEPLOYMENT/public_res/* $DEST

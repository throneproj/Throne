#!/bin/bash
set -e

source script/env_deploy.sh
DEST=$DEPLOYMENT/windows64
rm -rf $DEST
mkdir -p $DEST

#### copy exe ####
cp $BUILD/nekoray.exe $DEST

cd download-artifact
cd *windows-amd64
tar xvzf artifacts.tgz -C ../../
cd ..
cd *public_res
tar xvzf artifacts.tgz -C ../../
cd ../..

mv $DEPLOYMENT/public_res/* $DEST

#### deploy qt & DLL runtime ####
pushd $DEST
windeployqt nekoray.exe --no-translations --no-system-d3d-compiler --no-opengl-sw --no-svg --verbose 2
popd

rm -rf $DEST/dxcompiler.dll $DEST/dxil.dll

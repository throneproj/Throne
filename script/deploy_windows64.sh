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

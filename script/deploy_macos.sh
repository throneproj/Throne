#!/bin/bash
set -e

rm -rf $DEST
mkdir -p $DEST

#### copy golang => .app ####
cd download-artifact
cd *$DEST_SUFFIX
tar xvzf artifacts.tgz -C ../../
cd ../..

mv deployment/$DEST_SUFFIX/* $GITHUB_WORKSPACE/build/Throne.app/Contents/MacOS

#### deploy qt & Dylib runtime => .app ####
pushd $GITHUB_WORKSPACE/build
macdeployqt Throne.app -verbose=3
popd

codesign --force --deep --sign - $GITHUB_WORKSPACE/build/Throne.app

dsymutil $GITHUB_WORKSPACE/build/Throne.app/Contents/MacOS/Throne
strip -S $GITHUB_WORKSPACE/build/Throne.app/Contents/MacOS/Throne

mv $GITHUB_WORKSPACE/build/Throne.app $DEST

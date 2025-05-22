#!/bin/bash
set -e

if [[ $(uname -m) == 'aarch64' || $(uname -m) == 'arm64' ]]; then
  ARCH="arm64"
else
  ARCH="amd64"
fi

source script/env_deploy.sh
DEST=$DEPLOYMENT/linux64
rm -rf $DEST
mkdir -p $DEST

#### copy binary ####
cp $BUILD/nekoray $DEST

cd download-artifact
cd *linux-$ARCH
tar xvzf artifacts.tgz -C ../../
cd ..
cd *public_res
tar xvzf artifacts.tgz -C ../../
cd ../..

mv $DEPLOYMENT/public_res/* $DEST

sudo add-apt-repository universe
sudo apt install libfuse2
sudo apt install patchelf
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20240109-1/linuxdeploy-x86_64.AppImage
wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20240109-1/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage

export EXTRA_QT_PLUGINS="iconengines;wayland-shell-integration;wayland-decoration-client;"
export EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so;"
./linuxdeploy-x86_64.AppImage --appdir $DEST --executable $DEST/nekoray --plugin qt
rm linuxdeploy-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage
cd $DEST
rm -r ./usr/translations ./usr/bin ./usr/share ./apprun-hooks

# fix plugins rpath
rm -r ./usr/plugins
mkdir ./usr/plugins
mkdir ./usr/plugins/platforms
cp $QT_PLUGIN_PATH/platforms/libqxcb.so ./usr/plugins/platforms
cp $QT_PLUGIN_PATH/platforms/libqwayland-generic.so ./usr/plugins/platforms
cp -r $QT_PLUGIN_PATH/platformthemes ./usr/plugins
cp -r $QT_PLUGIN_PATH/imageformats ./usr/plugins
cp -r $QT_PLUGIN_PATH/iconengines ./usr/plugins
cp -r $QT_PLUGIN_PATH/wayland-shell-integration ./usr/plugins
cp -r $QT_PLUGIN_PATH/wayland-decoration-client ./usr/plugins
cp -r $QT_PLUGIN_PATH/tls ./usr/plugins
patchelf --set-rpath '$ORIGIN/../../lib' ./usr/plugins/platforms/libqxcb.so
patchelf --set-rpath '$ORIGIN/../../lib' ./usr/plugins/platforms/libqwayland-generic.so
patchelf --set-rpath '$ORIGIN/../../lib' ./usr/plugins/platformthemes/libqgtk3.so
patchelf --set-rpath '$ORIGIN/../../lib' ./usr/plugins/platformthemes/libqxdgdesktopportal.so

# fix extra libs...
mkdir ./usr/lib2
ls ./usr/lib/
cp ./usr/lib/libQt* ./usr/lib/libxcb-cursor* ./usr/lib/libxcb-util* ./usr/lib/libicuuc* ./usr/lib/libicui18n* ./usr/lib/libicudata* ./usr/lib2
rm -r ./usr/lib
mv ./usr/lib2 ./usr/lib

# fix lib rpath
cd $DEST
patchelf --set-rpath '$ORIGIN/usr/lib' ./nekoray

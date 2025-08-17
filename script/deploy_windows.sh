#!/bin/bash
set -e

source script/env_deploy.sh

# --- This logic determines the correct Go artifact to use ---
# It is based on your original script, with a specific addition for v3.
if [[ $1 == 'i686' ]]; then
  GO_ARCH_ARTIFACT="windowslegacy-386"
elif [[ $1 == 'x86_64' ]]; then
  GO_ARCH_ARTIFACT="windowslegacy-amd64"
elif [[ $1 == 'x86_64-v3' ]]; then
  GO_ARCH_ARTIFACT="windows-amd64-v3"
else
  GO_ARCH_ARTIFACT="windows-amd64"
fi

# --- This logic determines the final output directory ---
if [[ $1 == 'i686' ]]; then
  DEST=$DEPLOYMENT/windows32
elif [[ $1 == 'x86_64' ]]; then
  DEST=$DEPLOYMENT/windowslegacy64
elif [[ $1 == 'x86_64-v3' ]]; then
  DEST=$DEPLOYMENT/windows64-v3
else
  DEST=$DEPLOYMENT/windows64
fi


echo "---> Cleaning and creating destination: $DEST"
rm -rf $DEST
mkdir -p $DEST

#### get the pdb ####
# This part is for Debug builds, can be removed if only using Release.
if [ -f "./build/Throne.exe" ]; then
    echo "---> Processing PDB files..."
    curl -fLJO https://github.com/rainers/cv2pdb/releases/download/v0.53/cv2pdb-0.53.zip
    7z x cv2pdb-0.53.zip -ocv2pdb
    ./cv2pdb/cv2pdb64.exe ./build/Throne.exe ./tmp.exe ./Throne.pdb
    rm -rf cv2pdb-0.53.zip cv2pdb
    cd build
    strip -s Throne.exe
    cd ..
    rm tmp.exe
    mv Throne.pdb $DEST
fi


#### download srslist ####
curl -fLso $DEST/srslist "https://raw.githubusercontent.com/throneproj/routeprofiles/rule-set/list"

#### copy exe ####
echo "---> Copying C++ executable..."
cp $BUILD/Throne.exe $DEST

# --- This is your original, reliable extraction logic ---
echo "---> Extracting Go core for arch: $GO_ARCH_ARTIFACT"
cd download-artifact
cd *$GO_ARCH_ARTIFACT
tar xvzf artifacts.tgz -C ../../
cd ../..


# --- This is the new, targeted fix ONLY for the v3 build ---
# It merges the Go core (extracted to a default location) into the correct v3 directory.
if [[ $1 == 'x86_64-v3' ]]; then
  echo "--> Merging v3 Go core into v3 C++ directory..."
  # The v3 Go artifact extracts to './deployment/windows64' by default. We move its contents.
  mv $DEPLOYMENT/windows64/* $DEST
  # Then, remove the now-empty directory.
  rm -rf $DEPLOYMENT/windows64
fi

echo "---> Merging public resources..."
mv $DEPLOYMENT/public_res/* $DEST

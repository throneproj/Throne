#!/bin/bash
set -e

# Android AAR of core/mobile via the sagernet gomobile fork (gomobile + gobind on PATH, JDK 17,
# ANDROID_HOME/ANDROID_NDK_HOME set). No protoc step: core/mobile does not import gen.
TAGS="with_gvisor,with_quic,with_wireguard,with_utls,with_clash_api,with_openvpn,with_openconnect,with_naive_outbound,badlinkname,tfogo_checklinkname0"
TARGET="android/arm64,android/arm,android/amd64"
DEST=${DEST:-$PWD/deployment/android}

mkdir -p "$DEST"

#### Go: core/mobile -> ThroneCore.aar ####
pushd core
VERSION_SINGBOX=$(go list -m -f '{{.Version}}' github.com/sagernet/sing-box)
gomobile bind -v -o "$DEST/ThroneCore.aar" -target "$TARGET" -androidapi 24 -javapkg=io.throneproj -libname=throne -trimpath \
  -ldflags "-s -w -checklinkname=0 -X github.com/sagernet/sing-box/constant.Version=${VERSION_SINGBOX} -X runtime.godebugDefault=multipathtcp=0" \
  -tags "$TAGS" ./mobile
popd

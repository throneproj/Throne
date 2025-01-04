#!/bin/bash
set -e

source script/env_deploy.sh
[ "$GOOS" == "windows" ] && [ "$GOARCH" == "amd64" ] && DEST=$DEPLOYMENT/windows64 || true
[ "$GOOS" == "windows" ] && [ "$GOARCH" == "arm64" ] && DEST=$DEPLOYMENT/windows-arm64 || true
[ "$GOOS" == "linux" ] && [ "$GOARCH" == "amd64" ] && DEST=$DEPLOYMENT/linux64 || true
[ "$GOOS" == "linux" ] && [ "$GOARCH" == "arm64" ] && DEST=$DEPLOYMENT/linux-arm64 || true
[ "$GOOS" == "darwin" ] && [ "$GOARCH" == "amd64" ] && DEST=$DEPLOYMENT/macos-amd64 || true
[ "$GOOS" == "darwin" ] && [ "$GOARCH" == "arm64" ] && DEST=$DEPLOYMENT/macos-arm64 || true

if [ -z $DEST ]; then
  echo "Please set GOOS GOARCH"
  exit 1
fi
rm -rf $DEST
mkdir -p $DEST

export CGO_ENABLED=0

#### Go: updater ####
pushd core/cmd/updater
[ "$GOOS" == "darwin" ] || go build -o $DEST -trimpath -ldflags "-w -s"
popd

#### Go: nekobox_core ####
pushd core/cmd/nekobox_core
go build -v -o $DEST -trimpath -ldflags "-w -s" -tags "with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls,with_ech,with_dhcp"
popd

#!/bin/bash
set -e

source script/env_deploy.sh
# По умолчанию для Linux: текущая ОС и архитектура
if [ -z "$GOOS" ] && [ -z "$GOARCH" ]; then
  case "$(uname -s)" in
    Linux)  export GOOS=linux; export GOARCH=$(uname -m | sed 's/x86_64/amd64/;s/aarch64/arm64/');;
    Darwin) export GOOS=darwin; export GOARCH=$(uname -m | sed 's/x86_64/amd64/;s/arm64/arm64/');;
  esac
fi
[ "$GOOS" == "windows" ] && [ "$GOARCH" == "amd64" ] && DEST=$DEPLOYMENT/windows64 || true
[ "$GOOS" == "windows" ] && [ "$GOARCH" == "386" ] && DEST=$DEPLOYMENT/windows32 || true
[ "$GOOS" == "windows" ] && [ "$GOARCH" == "arm64" ] && DEST=$DEPLOYMENT/windows-arm64 || true
[ "$GOOS" == "linux" ] && [ "$GOARCH" == "amd64" ] && DEST=$DEPLOYMENT/linux-amd64 || true
[ "$GOOS" == "linux" ] && [ "$GOARCH" == "arm64" ] && DEST=$DEPLOYMENT/linux-arm64 || true
[ "$GOOS" == "darwin" ] && [ "$GOARCH" == "amd64" ] && DEST=$DEPLOYMENT/macos-amd64 || true
[ "$GOOS" == "darwin" ] && [ "$GOARCH" == "arm64" ] && DEST=$DEPLOYMENT/macos-arm64 || true

if [[ "$GOOS" =~ legacy$ ]]; then
  GOCMD="$PWD/go/bin/go"
  if [[ "$GOOS" == "windowslegacy" ]]; then
    GOOS="windows"
    if [[ $GOARCH == 'amd64' ]]; then
      DEST=$DEPLOYMENT/windowslegacy64
    else
      DEST=$DEPLOYMENT/windows32
    fi
  else
    GOOS="darwin"
    DEST=$DEPLOYMENT/macos-legacy-amd64
  fi
else
  GOCMD="go"
fi

if [ -z $DEST ]; then
  echo "Please set GOOS GOARCH"
  exit 1
fi

# PATH: плагины protoc (go install кладёт в GOPATH/bin)
export PATH="$(go env GOPATH 2>/dev/null)/bin:$PATH"

# Проект требует Go 1.25+ (core/server и core/protorpc)
need_go_minor=25
go_minor=$(go version 2>/dev/null | sed -n 's/.*go1\.\([0-9]*\).*/\1/p')
if [ -z "$go_minor" ] || [ "$go_minor" -lt "$need_go_minor" ]; then
  echo "Требуется Go 1.25+, в PATH сейчас: $(go version 2>/dev/null || echo 'go не найден')."
  echo "Установите с https://go.dev/dl/ и добавьте bin в PATH (например export PATH=\"/usr/local/go/bin:\$PATH\")."
  exit 1
fi

if ! command -v protoc-gen-go &>/dev/null; then
  echo "protoc-gen-go не найден. Установите:"
  echo "  go install google.golang.org/protobuf/cmd/protoc-gen-go@latest"
  echo "  (и protoc-gen-protorpc: cd core/protorpc && go install ./protoc-gen-protorpc)"
  exit 1
fi
if ! command -v protoc-gen-protorpc &>/dev/null; then
  echo "protoc-gen-protorpc не найден. Соберите из репозитория:"
  echo "  cd core/protorpc && go install ./protoc-gen-protorpc"
  exit 1
fi

rm -rf $DEST
mkdir -p $DEST

if [[ "$GOOS" == "windows" ]]; then
  if [[ "$GOARCH" == "386" ]]; then
    curl -fLso $DEST/updater.exe "https://github.com/throneproj/updater/releases/latest/download/updater-windows32.exe"
  else
    curl -fLso $DEST/updater.exe "https://github.com/throneproj/updater/releases/latest/download/updater-windows64.exe"
  fi
fi
if [[ "$GOOS" == "linux" ]]; then
  if [[ "$GOARCH" == "arm64" ]]; then
    curl -fLso $DEST/updater "https://github.com/throneproj/updater/releases/latest/download/updater-linux-arm64"
  else
    curl -fLso $DEST/updater "https://github.com/throneproj/updater/releases/latest/download/updater-linux-amd64"
  fi
  chmod +x $DEST/updater
fi

export CGO_ENABLED=0

#### Go: core ####
pushd core/server
pushd gen
protoc -I . --go_out=. --protorpc_out=. libcore.proto
popd
VERSION_SINGBOX=$(go list -m -f '{{.Version}}' github.com/sagernet/sing-box)
$GOCMD build -v -o $DEST -trimpath -ldflags "-w -s -X 'github.com/sagernet/sing-box/constant.Version=${VERSION_SINGBOX}'" -tags "with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls,with_dhcp,with_tailscale"
popd

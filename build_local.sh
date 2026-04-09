#!/bin/bash
# Local development build script for Throne (Linux x86_64)
# Usage: ./build_local.sh [--skip-deps] [--skip-go] [--skip-cpp]
set -e

REPO="$(cd "$(dirname "$0")" && pwd)"
DEST="$REPO/deployment/linux-amd64"
BUILD_DIR="$REPO/build"

SKIP_DEPS=0
SKIP_GO=0
SKIP_CPP=0

for arg in "$@"; do
  case $arg in
    --skip-deps) SKIP_DEPS=1 ;;
    --skip-go)   SKIP_GO=1   ;;
    --skip-cpp)  SKIP_CPP=1  ;;
  esac
done

# ── Colours ────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()    { echo -e "${GREEN}[INFO]${NC} $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC} $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

# ── Step 1: Install build dependencies ─────────────────────────────────────
if [ "$SKIP_DEPS" -eq 0 ]; then
  info "Installing build dependencies..."
  sudo apt-get update -q
  sudo apt-get install -y \
    build-essential ninja-build cmake git curl \
    qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
    libqt6svg6-dev libqt6dbus6 \
    protobuf-compiler
else
  warn "Skipping dependency installation (--skip-deps)"
fi

# ── Locate Go ───────────────────────────────────────────────────────────────
GO_CMD=""
for candidate in go /usr/local/go/bin/go; do
  if command -v "$candidate" &>/dev/null; then
    GO_CMD="$candidate"
    break
  fi
done
[ -z "$GO_CMD" ] && error "Go not found. Install it from https://go.dev/dl/"
info "Using Go: $GO_CMD ($($GO_CMD version))"
export PATH="$PATH:$($GO_CMD env GOPATH)/bin"

# ── Step 2: Build Go core ───────────────────────────────────────────────────
if [ "$SKIP_GO" -eq 0 ]; then
  info "Generating protobuf Go bindings..."
  # Install protoc plugins if missing
  if ! command -v protoc-gen-go &>/dev/null; then
    info "Installing protoc-gen-go..."
    $GO_CMD install github.com/golang/protobuf/protoc-gen-go@latest
    $GO_CMD install google.golang.org/grpc/cmd/protoc-gen-go-grpc@latest
  fi

  cd "$REPO/core/server/gen"
  protoc \
    --plugin=protoc-gen-go="$($GO_CMD env GOPATH)/bin/protoc-gen-go" \
    --plugin=protoc-gen-go-grpc="$($GO_CMD env GOPATH)/bin/protoc-gen-go-grpc" \
    -I . --go_out=. --go-grpc_out=. libcore.proto
  cd "$REPO/core/server"

  info "Building Go core binary..."
  TAGS="with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls"
  TAGS="$TAGS,with_dhcp,with_tailscale,badlinkname,tfogo_checklinkname0"
  TAGS="$TAGS,with_purego,with_naive_outbound"

  VERSION_SINGBOX=$($GO_CMD list -m -f '{{.Version}}' github.com/sagernet/sing-box)
  info "  sing-box version: $VERSION_SINGBOX"

  mkdir -p "$DEST"
  GOOS=linux GOARCH=amd64 CGO_ENABLED=0 \
    $GO_CMD build -v \
    -o "$DEST" \
    -trimpath \
    -ldflags "-w -s -checklinkname=0 \
      -X 'github.com/sagernet/sing-box/constant.Version=${VERSION_SINGBOX}' \
      -X 'internal/godebug.defaultGODEBUG=multipathtcp=0'" \
    -tags "$TAGS"
  info "Go core built → $DEST/ThroneCore"
else
  warn "Skipping Go build (--skip-go)"
fi

# ── Step 3: Build Qt/C++ app ────────────────────────────────────────────────
if [ "$SKIP_CPP" -eq 0 ]; then
  info "Building Qt/C++ app..."

  # Fetch the route-set header required by CMake
  mkdir -p "$BUILD_DIR"
  if [ ! -f "$BUILD_DIR/srslist.h" ]; then
    info "Downloading srslist.h..."
    curl -fLso "$BUILD_DIR/srslist.h" \
      "https://raw.githubusercontent.com/throneproj/routeprofiles/rule-set/srslist.h"
  fi

  cd "$BUILD_DIR"
  cmake -GNinja \
    -DCMAKE_BUILD_TYPE=Debug \
    ..
  ninja

  info "Qt app built → $BUILD_DIR/Throne"

  # Copy Go core binary next to the Qt app (must be named ThroneCore)
  info "Copying core binary to build dir..."
  cp "$DEST/ThroneCore" "$BUILD_DIR/ThroneCore"
  chmod +x "$BUILD_DIR/ThroneCore"
else
  warn "Skipping C++ build (--skip-cpp)"
fi

# ── Done ────────────────────────────────────────────────────────────────────
echo ""
info "Build complete!"
info "Run with:  $BUILD_DIR/Throne"

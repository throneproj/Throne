#!/bin/bash
set -e

TAGS="with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls,with_dhcp,with_tailscale,badlinkname,tfogo_checklinkname0"

rm -rf $DEST
mkdir -p $DEST

[[ "$GOOS" =~ legacy$ ]] && IS_LEGACY=true && GOCMD="$PWD/golang.org/go/bin/go" && GOOS="${GOOS%legacy}" || { IS_LEGACY=false; GOCMD="go"; }

# --- Supply-chain integrity for downloaded prebuilt binaries ---------------
# Every binary fetched from a release page must match a sha256 pinned in
# script/deps.sha256 (format: "<remote-filename>  <sha256>"). This fails the
# build closed if an artifact was tampered with, replaced, or silently bumped.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPS_SHA_FILE="$SCRIPT_DIR/deps.sha256"

sha256_of() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    else
        shasum -a 256 "$1" | awk '{print $1}'
    fi
}

# verify_sha256 <downloaded-file> <pin-key>
verify_sha256() {
    local file="$1" key="$2" expected actual
    expected=$(awk -v k="$key" '$1==k {print $2; exit}' "$DEPS_SHA_FILE" 2>/dev/null)
    if [[ -z "$expected" || "$expected" == "PUT_REAL_SHA256_HERE" ]]; then
        echo "ERROR: no pinned sha256 for '$key' in $DEPS_SHA_FILE" >&2
        echo "       download verified hash and add a line: '$key  <sha256>'" >&2
        echo "       actual sha256 of fetched file: $(sha256_of "$file")" >&2
        exit 1
    fi
    actual=$(sha256_of "$file")
    if [[ "$actual" != "$expected" ]]; then
        echo "ERROR: checksum mismatch for '$key'" >&2
        echo "       expected: $expected" >&2
        echo "       actual:   $actual" >&2
        exit 1
    fi
    echo "verified $key ($actual)"
}

if [[ "$GOOS" == "windows" || "$GOOS" == "linux" ]]; then
    FILE=$([[ "$GOOS" == "windows" ]] && echo "updater-windows-x${GOARCH: -2}.exe" || echo "updater-linux-$GOARCH")
    UPDATER_OUT="$DEST/updater$([[ "$GOOS" == "windows" ]] && echo ".exe")"
    curl -fLso "$UPDATER_OUT" "https://github.com/throneproj/updater/releases/latest/download/$FILE"
    verify_sha256 "$UPDATER_OUT" "$FILE"
    [[ "$GOOS" == "linux" ]] && chmod +x "$DEST/updater"
fi

case "$GOOS" in
  windows)
    export CGO_ENABLED=0
    if ! $IS_LEGACY; then
      TAGS+=",with_purego,with_naive_outbound"
      curl -fLso $DEST/libcronet.dll "https://github.com/SagerNet/cronet-go/releases/latest/download/libcronet-windows-$GOARCH.dll"
      verify_sha256 "$DEST/libcronet.dll" "libcronet-windows-$GOARCH.dll"
    fi
    ;;
  darwin)
    TAGS+=",with_naive_outbound"
    export CGO_ENABLED=1 CGO_LDFLAGS="-weak_framework UniformTypeIdentifiers"
    ;;
  linux)
    TAGS+=",with_naive_outbound"
    export CGO_ENABLED=1
    ;;
esac

#### Go: core ####
pushd core/server
pushd gen
protoc -I . --go_out=. --go-grpc_out=. libcore.proto
popd
VERSION_SINGBOX=$(go list -m -f '{{.Version}}' github.com/sagernet/sing-box)
$GOCMD build -v -o $DEST -trimpath -ldflags "-w -s -X 'github.com/sagernet/sing-box/constant.Version=${VERSION_SINGBOX}' -X 'internal/godebug.defaultGODEBUG=multipathtcp=0' -checklinkname=0" -tags "$TAGS"
popd

#!/bin/bash
set -e

TAGS="with_clash_api,with_quic,with_wireguard,with_utls,with_dhcp,with_tailscale,with_openvpn,with_openconnect,badlinkname,tfogo_checklinkname0"

rm -rf $DEST
mkdir -p $DEST

[[ "$GOOS" =~ legacy$ ]] && IS_LEGACY=true && GOCMD="$PWD/golang.org/go/bin/go" && GOOS="${GOOS%legacy}" || { IS_LEGACY=false; GOCMD="go"; }
# a go.mod newer than the patched toolchain must fail the build, not auto-fetch an unpatched official Go
if $IS_LEGACY; then
  export GOTOOLCHAIN=local
fi

if [[ "$GOOS" == "windows" || "$GOOS" == "linux" ]]; then
    FILE=$([[ "$GOOS" == "windows" ]] && echo "updater-windows-x${GOARCH: -2}.exe" || echo "updater-linux-$GOARCH")
    curl -fLso "$DEST/updater$([[ "$GOOS" == "windows" ]] && echo ".exe")" "https://github.com/throneproj/updater/releases/latest/download/$FILE"
    [[ "$GOOS" == "linux" ]] && chmod +x "$DEST/updater"
fi

case "$GOOS" in
  windows)
    export CGO_ENABLED=0
    if ! $IS_LEGACY; then
      TAGS+=",with_purego,with_naive_outbound"
    fi
    ;;
  darwin)
    TAGS+=",with_naive_outbound"
    export CGO_ENABLED=1 CGO_LDFLAGS="-weak_framework UniformTypeIdentifiers"
    # cgo otherwise builds for the runner's SDK and hard-links every API newer than the app's floor
    if $IS_LEGACY; then
      export MACOSX_DEPLOYMENT_TARGET=10.15
    else
      export MACOSX_DEPLOYMENT_TARGET=14.4
    fi
    ;;
  linux)
    TAGS+=",with_naive_outbound"
    export CGO_ENABLED=1
    ;;
esac

#### Go: core ####
pushd core
# Modern Xray's Chrome fingerprint needs uTLS with Go 1.27. Keep the legacy
# OS builds on their existing uTLS and patched Go 1.26 without editing the
# checked-in module files or fetching an unpatched replacement toolchain.
MOD_FLAGS=()
if $IS_LEGACY; then
  LEGACY_MOD_DIR=$(mktemp -d "${TMPDIR:-/tmp}/throne-legacy-mod.XXXXXX")
  trap 'rm -rf -- "$LEGACY_MOD_DIR"' EXIT
  cp go.mod "$LEGACY_MOD_DIR/go.mod"
  cp go.sum "$LEGACY_MOD_DIR/go.sum"
  go mod edit -modfile="$LEGACY_MOD_DIR/go.mod" -go=1.26.0 \
    -require=github.com/refraction-networking/utls@v1.8.3-0.20260301010127-aa6edf4b11af \
    -droprequire=github.com/molecule-man/go-brrr
  MOD_FLAGS=(-modfile="$LEGACY_MOD_DIR/go.mod")
fi
pushd gen
protoc -I . --go_out=. --go-grpc_out=. libcore.proto
popd
if [[ "$GOOS" == "windows" ]] && ! $IS_LEGACY; then
  # The lib module ships the DLL, so it is always the binding's generation.
  CRONET_LIB=github.com/sagernet/cronet-go/lib/windows_$GOARCH
  go mod download "${MOD_FLAGS[@]}" $CRONET_LIB
  install -m 644 "$(go list "${MOD_FLAGS[@]}" -m -f '{{.Dir}}' $CRONET_LIB)/libcronet.dll" $DEST/
fi
if [[ "$GOOS" == "darwin" ]]; then
  # cgo cannot detect a binding/lib ABI skew, so compare the headers the darwin lib was built against.
  CRONET_LIB=github.com/sagernet/cronet-go/lib/darwin_$GOARCH
  go mod download "${MOD_FLAGS[@]}" github.com/sagernet/cronet-go $CRONET_LIB
  CRONET_LIB_INCLUDE="$(go list "${MOD_FLAGS[@]}" -m -f '{{.Dir}}' $CRONET_LIB)/include"
  for header in "$(go list "${MOD_FLAGS[@]}" -m -f '{{.Dir}}' github.com/sagernet/cronet-go)"/include/*.h; do
    cmp -s "$header" "$CRONET_LIB_INCLUDE/${header##*/}" || { echo "cronet-go binding and $CRONET_LIB differ in ${header##*/}" >&2; exit 1; }
  done
fi
VERSION_SINGBOX=$(go list "${MOD_FLAGS[@]}" -m -f '{{.Version}}' github.com/sagernet/sing-box)
$GOCMD build "${MOD_FLAGS[@]}" -v -o $DEST -trimpath -ldflags "-w -s -X 'github.com/sagernet/sing-box/constant.Version=${VERSION_SINGBOX}' -X 'internal/godebug.defaultGODEBUG=multipathtcp=0' -checklinkname=0" -tags "$TAGS"
popd

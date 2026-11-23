#!/bin/bash
set -e

TAGS="with_clash_api,with_quic,with_wireguard,with_utls,with_dhcp,with_tailscale,with_openvpn,with_openconnect,with_tor_transports,with_psiphon,PSIPHON_DISABLE_QUIC,PSIPHON_DISABLE_INPROXY,badlinkname,tfogo_checklinkname0"

rm -rf $DEST
mkdir -p $DEST

[[ "$GOOS" =~ legacy$ ]] && IS_LEGACY=true && GOCMD="$PWD/golang.org/go/bin/go" && GOOS="${GOOS%legacy}" || { IS_LEGACY=false; GOCMD="go"; }
# never auto-fetch an unpatched official Go: legacy_modfile lowers the go lines the patched toolchain is older than
if $IS_LEGACY; then
  export GOTOOLCHAIN=local
fi

# The patched toolchain refuses every module whose go line is newer than itself, so it builds against a temporary
# go.mod with its go line lowered, where each such module is replaced by a copy with its go line lowered as well.
legacy_modfile() {
  local version_pattern='^go([0-9]+\.[0-9]+(\.[0-9]+)?)' legacy modules path go_version version dir n=0
  local stale=()
  [[ "$("$GOCMD" env GOVERSION)" =~ $version_pattern ]] || { echo "cannot read the legacy Go version" >&2; return 1; }
  legacy=${BASH_REMATCH[1]}
  LEGACY_WORK=$(mktemp -d)
  trap 'rm -rf "$LEGACY_WORK"' EXIT
  cp go.mod go.sum "$LEGACY_WORK/"
  go mod edit -go="$legacy" -toolchain=none "$LEGACY_WORK/go.mod"
  modules=$(go list -m -f '{{if not .Main}}{{.Path}} {{.GoVersion}}{{end}}' all)
  while read -r path go_version; do
    if [[ -n "$go_version" ]] && ! printf '%s\n%s\n' "$go_version" "$legacy" | sort -V -C; then
      stale+=("$path")
    fi
  done <<< "$modules"
  [[ ${#stale[@]} -gt 0 ]] || return 0
  go mod download "${stale[@]}"
  modules=$(go list -m -f '{{.Path}}|{{.Version}}|{{.Dir}}' "${stale[@]}")
  while IFS='|' read -r path version dir; do
    n=$((n + 1))
    cp -R "$dir" "$LEGACY_WORK/mod$n"
    chmod -R u+w "$LEGACY_WORK/mod$n"
    go mod edit -go="$legacy" -toolchain=none "$LEGACY_WORK/mod$n/go.mod"
    go mod edit -replace="$path@$version=$LEGACY_WORK/mod$n" "$LEGACY_WORK/go.mod"
    echo "$path@$version needs a newer Go than $legacy, building a copy with its go line lowered"
  done <<< "$modules"
}

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
pushd gen
protoc -I . --go_out=. --go-grpc_out=. libcore.proto
popd
if [[ "$GOOS" == "windows" ]] && ! $IS_LEGACY; then
  # The lib module ships the DLL, so it is always the binding's generation.
  CRONET_LIB=github.com/sagernet/cronet-go/lib/windows_$GOARCH
  go mod download $CRONET_LIB
  install -m 644 "$(go list -m -f '{{.Dir}}' $CRONET_LIB)/libcronet.dll" $DEST/
fi
if [[ "$GOOS" == "darwin" ]]; then
  # cgo cannot detect a binding/lib ABI skew, so compare the headers the darwin lib was built against.
  CRONET_LIB=github.com/sagernet/cronet-go/lib/darwin_$GOARCH
  go mod download github.com/sagernet/cronet-go $CRONET_LIB
  CRONET_LIB_INCLUDE="$(go list -m -f '{{.Dir}}' $CRONET_LIB)/include"
  for header in "$(go list -m -f '{{.Dir}}' github.com/sagernet/cronet-go)"/include/*.h; do
    cmp -s "$header" "$CRONET_LIB_INCLUDE/${header##*/}" || { echo "cronet-go binding and $CRONET_LIB differ in ${header##*/}" >&2; exit 1; }
  done
fi
VERSION_SINGBOX=$(go list -m -f '{{.Version}}' github.com/sagernet/sing-box)
BUILD_FLAGS=()
if $IS_LEGACY; then
  legacy_modfile
  BUILD_FLAGS=(-modfile="$LEGACY_WORK/go.mod")
fi
$GOCMD build -v "${BUILD_FLAGS[@]}" -o $DEST -trimpath -ldflags "-w -s -X 'github.com/sagernet/sing-box/constant.Version=${VERSION_SINGBOX}' -X 'internal/godebug.defaultGODEBUG=multipathtcp=0' -checklinkname=0" -tags "$TAGS"
popd

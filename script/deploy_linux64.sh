#!/bin/bash
set -euo pipefail

# The relocatable Qt prefix from throneproj/buildqt that Throne was built against.
QT_PREFIX="${QT_PREFIX:-$GITHUB_WORKSPACE/Qt}"

rm -rf "$DEST"
mkdir -p "$DEST/usr/lib" "$DEST/usr/plugins/platforms"

#### copy binary ####
cp "$GITHUB_WORKSPACE/build/Throne" "$DEST"

#### copy Throne.png ####
cp "$GITHUB_WORKSPACE/res/public/Throne.png" "$DEST"

source "$(dirname "$0")/extract_core_artifact.sh"

#### copy plugins ####
cp "$QT_PREFIX/plugins/platforms/libqxcb.so" "$QT_PREFIX/plugins/platforms/libqwayland.so" "$DEST/usr/plugins/platforms"
for dir in platformthemes imageformats iconengines wayland-shell-integration wayland-decoration-client tls; do
    cp -r "$QT_PREFIX/plugins/$dir" "$DEST/usr/plugins"
done

#### copy libs: what Throne and the plugins load from Qt/lib, followed through DT_NEEDED ####
mapfile -t queue < <(find "$DEST/usr/plugins" -type f -name '*.so' | sort)
queue=("$DEST/Throne" "${queue[@]}")
declare -A copied=()
for ((i = 0; i < ${#queue[@]}; i++)); do
    for lib in $(readelf -d -W "${queue[i]}" | sed -n 's/.*(NEEDED).*\[\(.*\)\]$/\1/p'); do
        case "$lib" in
        libQt6*.so.* | libxcb-cursor.so.* | libxcb-util.so.*) ;;
        *) continue ;;
        esac
        [ -z "${copied[$lib]:-}" ] || continue
        copied[$lib]=1
        cp -L "$QT_PREFIX/lib/$lib" "$DEST/usr/lib/$lib"
        queue+=("$DEST/usr/lib/$lib")
    done
done
ls "$DEST/usr/lib"

# Strip before patchelf: strip can break a file patchelf had to grow.
strip --strip-unneeded "$DEST"/usr/lib/* "$DEST"/usr/plugins/*/*.so

#### fix rpath ####
# Only where it differs: Qt's installed RUNPATHs already resolve in this layout.
set_runpath() {
    [ "$(patchelf --print-rpath "$2")" = "$1" ] || patchelf --set-rpath "$1" "$2"
}
for lib in "$DEST"/usr/lib/*; do
    set_runpath '$ORIGIN' "$lib"
done
for plugin in "$DEST"/usr/plugins/*/*.so; do
    set_runpath '$ORIGIN/../../lib' "$plugin"
done
set_runpath '$ORIGIN/usr/lib' "$DEST/Throne"

# handle debug info
objcopy --only-keep-debug "$DEST/Throne" "$DEST/Throne.debug"
strip --strip-debug --strip-unneeded "$DEST/Throne"
objcopy --add-gnu-debuglink="$DEST/Throne.debug" "$DEST/Throne"

#!/bin/bash
set -e

rm -rf $DEST
mkdir -p $DEST

#### copy golang => .app ####
source "$(dirname "$0")/extract_core_artifact.sh"

mv deployment/$DEST_SUFFIX/* $GITHUB_WORKSPACE/build/Throne.app/Contents/MacOS

#### deploy qt & Dylib runtime => .app ####
pushd $GITHUB_WORKSPACE/build
macdeployqt Throne.app -verbose=3
popd

APP=$GITHUB_WORKSPACE/build/Throne.app

# install-qt-action's Qt is universal; keep only the architecture this build targets.
find "$APP/Contents/Frameworks" "$APP/Contents/PlugIns" -type f | while read -r f; do
    [ "$(lipo -archs "$f" 2>/dev/null | wc -w)" -gt 1 ] || continue
    lipo "$f" -thin "$ARCH" -output "$f.thin"
    mv "$f.thin" "$f"
done

# Outside the bundle: pack_release.sh leaves it out of the zip, which must not break the seal.
dsymutil "$APP/Contents/MacOS/Throne" -o "$DEST/Throne.dSYM"
strip -S "$APP/Contents/MacOS/Throne"

# Last: rewriting any binary after this invalidates its signature.
codesign --force --deep --sign - "$APP"

mv "$APP" $DEST

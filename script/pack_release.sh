#!/bin/bash
set -euo pipefail

USAGE="usage: pack_release.sh <windows|linux-amd64|linux-arm64|macos|debug> <tag>"
TARGET="${1:?$USAGE}"
export TAG="${2:?$USAGE}"
export ROOT="$PWD"
export DEPLOY="$ROOT/deployment"
export WORK="$ROOT/.pack"
JOBS="${PACK_JOBS:-$(nproc)}"

rm -rf "$WORK"
mkdir -p "$WORK/logs" "$DEPLOY"

# Common tarballs are skipped: every build tarball already carries its core.
untar() {
    local glob="$1"
    shift
    find download-artifact -path "*/Throne-*-$glob/artifacts.tgz" -not -path '*-Common-*' -print0 |
        xargs -0 -r -I{} -P "$JOBS" tar xzf {} "$@"
}

sign_windows_artifacts() {
    # Authenticode: no-op unless WINDOWS_CERT_* is configured (see script/windows_sign.ps1).
    local -a files=()
    local f
    for f in \
        "$DEPLOY/windows-amd64/Throne.exe" \
        "$DEPLOY/windows-amd64/ThroneCore.exe" \
        "$DEPLOY/windows-arm64/Throne.exe" \
        "$DEPLOY/windows-arm64/ThroneCore.exe" \
        "$DEPLOY/windowslegacy-amd64/Throne.exe" \
        "$DEPLOY/windowslegacy-amd64/ThroneCore.exe" \
        "$DEPLOY/windowslegacy-386/Throne.exe" \
        "$DEPLOY/windowslegacy-386/ThroneCore.exe" \
        "$DEPLOY/Throne-$TAG-windows-universal-installer.exe" \
        "$DEPLOY/Throne-$TAG-windows-amd64.msix" \
        "$DEPLOY/Throne-$TAG-windows-arm64.msix"
    do
        [[ -f $f ]] && files+=("$(cygpath -w "$f")")
    done
    ((${#files[@]})) || return 0
    local strict=()
    [[ ${WINDOWS_SIGN_STRICT:-} == 1 ]] && strict=(-Strict)
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$ROOT/script/windows_sign.ps1")" \
        -Path @files "${strict[@]}"
}

build_msix_packages() {
    # Full-trust desktop MSIX for modern Windows 11/10 (amd64 + arm64). Requires Windows SDK.
    local arch dir
    for arch in amd64:x64 arm64:arm64; do
        dir="${arch%%:*}"
        arch="${arch##*:}"
        [[ -f $DEPLOY/windows-$dir/Throne.exe ]] || continue
        powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$ROOT/script/pack_msix.ps1")" \
            -SourceDir "$(cygpath -w "$DEPLOY/windows-$dir")" \
            -Tag "$TAG" \
            -Architecture "$arch" \
            -OutDir "$(cygpath -w "$DEPLOY")"
    done
}

build_winget_manifests() {
    local installer="$DEPLOY/Throne-$TAG-windows-universal-installer.exe"
    [[ -f $installer ]] || return 0
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$ROOT/script/generate_winget_manifest.ps1")" \
        -Tag "$TAG" \
        -InstallerPath "$(cygpath -w "$installer")" \
        -OutDir "$(cygpath -w "$DEPLOY/Throne-$TAG-winget")"
}

build_installer() {
    local version="${TAG#v}"
    version="${version#V}"
    # Fork tags look like al4al-0.1.0 — strip the non-numeric prefix for Inno VersionInfo.
    if [[ $version == *.* && $version != [0-9]* ]]; then
        version="${version#*-}"
    fi
    local parts
    IFS='.' read -r -a parts <<<"${version%%-*}"
    local iscc="${ISCC:-}"
    if [[ -z $iscc ]]; then
        if command -v iscc.exe >/dev/null 2>&1; then
            iscc="$(command -v iscc.exe)"
        elif [[ -x /c/Program\ Files\ \(x86\)/Inno\ Setup\ 6/ISCC.exe ]]; then
            iscc='/c/Program Files (x86)/Inno Setup 6/ISCC.exe'
        elif [[ -x /c/Program\ Files/Inno\ Setup\ 6/ISCC.exe ]]; then
            iscc='/c/Program Files/Inno Setup 6/ISCC.exe'
        elif [[ -x "$LOCALAPPDATA/Programs/Inno Setup 6/ISCC.exe" ]]; then
            iscc="$LOCALAPPDATA/Programs/Inno Setup 6/ISCC.exe"
        else
            iscc='/c/Program Files (x86)/Inno Setup 6/ISCC.exe'
        fi
    fi
    if [[ ! -x $iscc && ! -f $iscc ]]; then
        echo "ISCC not found at: $iscc" >&2
        exit 1
    fi
    # ISCC is a native Windows program: stop MSYS from rewriting its /-switches as paths.
    MSYS_NO_PATHCONV=1 "$iscc" /Q \
        "/DAppVersion=$version" \
        "/DAppVersionMajor=${parts[0]:-0}" \
        "/DAppVersionMinor=${parts[1]:-0}" \
        "/DAppVersionPatch=${parts[2]:-0}" \
        "/DAppVersionBuild=${parts[3]:-0}" \
        "/O$(cygpath -w "$DEPLOY")" \
        "/FThrone-$TAG-windows-universal-installer" \
        "$(cygpath -w "$ROOT/script/windows_installer.iss")"
}

# Hard links give the archive its Throne/ root without moving a dir other tasks still read.
zip_dir() {
    mkdir -p "$WORK/$2"
    cp -al "$DEPLOY/$1" "$WORK/$2/Throne"
    cd "$WORK/$2"
    if command -v zip >/dev/null; then
        zip -q -r "$DEPLOY/Throne-$TAG-$2.zip" Throne
    else
        # Windows runners ship 7-Zip but not Info-ZIP.
        7z a -tzip -mx=5 -bd -bso0 "$DEPLOY/Throne-$TAG-$2.zip" Throne
    fi
}

zip_app() {
    mkdir -p "$WORK/$2/Throne"
    mv "$DEPLOY/$1/Throne.app" "$WORK/$2/Throne/"
    cd "$WORK/$2"
    zip -q --symlinks -r "$DEPLOY/Throne-$TAG-$2.zip" Throne
}

zip_debug() {
    cd "$DEPLOY"
    zip -q -r debug-symbols.zip debug
}

pack_deb() {
    cd "$DEPLOY"
    bash "$ROOT/script/pack_debian.sh" "$TAG" "$@"
}

pack_rpm() {
    cd "$DEPLOY"
    bash "$ROOT/script/pack_rpm.sh" "$TAG" "$@"
}

run_task() {
    local name="$1"
    shift
    # No `|| rc=$?` here: errexit is ignored for any command on the left of || or inside an if test.
    (set -eo pipefail; "$@") >"$WORK/logs/$name.log" 2>&1
    local rc=$?
    printf '%4ds  %s%s\n' "$SECONDS" "$name" "$([[ $rc == 0 ]] || echo ' FAILED')" >>"$WORK/logs/times"
    return $rc
}

export -f build_installer build_msix_packages build_winget_manifests sign_windows_artifacts zip_dir zip_app zip_debug pack_deb pack_rpm run_task

case "$TARGET" in
windows)
    untar 'windows*' --exclude='*.pdb'
    # Sign payload EXEs before Inno/MSIX consume them; packages are signed after TASKS.
    sign_windows_artifacts
    TASKS=(
        "installer build_installer"
        "msix build_msix_packages"
        "zip-windows64 zip_dir windows-amd64 windows64"
        "zip-windows-arm64 zip_dir windows-arm64 windows-arm64"
        "zip-windows32 zip_dir windowslegacy-386 windows32"
        "zip-windowslegacy64 zip_dir windowslegacy-amd64 windowslegacy64"
    )
    ;;
linux-amd64 | linux-arm64)
    arch="${TARGET#linux-}"
    untar "linux-$arch*" --exclude=Throne.debug
    TASKS=(
        "deb-$arch pack_deb $arch"
        "rpm-$arch pack_rpm $arch"
        "deb-$arch-system-qt pack_deb $arch systemqt"
        "rpm-$arch-system-qt pack_rpm $arch systemqt"
        "zip-linux-$arch zip_dir linux-$arch linux-$arch"
    )
    ;;
macos)
    untar 'darwin*' --exclude=Throne.dSYM
    TASKS=(
        "zip-macos-arm64 zip_app darwin-arm64 macos-arm64"
        "zip-macos-amd64 zip_app darwin-amd64 macos-amd64"
        "zip-macoslegacy-amd64 zip_app darwinlegacy-amd64 macoslegacy-amd64"
    )
    ;;
debug)
    untar linux-amd64 --wildcards '*/Throne.debug'
    untar linux-arm64 --wildcards '*/Throne.debug'
    untar 'windows*' --wildcards '*/Throne.pdb'
    untar 'darwin*' --wildcards '*/Throne.dSYM/*'
    cd "$DEPLOY"
    mkdir -p debug
    mv linux-amd64/Throne.debug "debug/Throne-$TAG-linux-amd64.debug"
    mv linux-arm64/Throne.debug "debug/Throne-$TAG-linux-arm64.debug"
    for dir in windows-amd64 windows-arm64 windowslegacy-386 windowslegacy-amd64; do
        mv "$dir/Throne.pdb" "debug/Throne-$TAG-$dir.pdb"
    done
    mv darwin-arm64/Throne.app/Contents/MacOS/Throne.dSYM "debug/Throne-$TAG-macos-arm64.dSYM"
    mv darwin-amd64/Throne.app/Contents/MacOS/Throne.dSYM "debug/Throne-$TAG-macos-amd64.dSYM"
    mv darwinlegacy-amd64/Throne.app/Contents/MacOS/Throne.dSYM "debug/Throne-$TAG-macoslegacy-amd64.dSYM"
    TASKS=("debug-symbols zip_debug")
    ;;
*)
    echo "$USAGE" >&2
    exit 1
    ;;
esac

rc=0
printf '%s\n' "${TASKS[@]}" | xargs -L1 -P "$JOBS" bash -c 'run_task "$@"' _ || rc=$?

for log in "$WORK"/logs/*.log; do
    echo "::group::$(basename "$log" .log)"
    cat "$log"
    echo "::endgroup::"
done
echo "Pack task durations:"
sort -rn "$WORK/logs/times"

if [[ $TARGET == windows && $rc -eq 0 ]]; then
    # Sign installer/MSIX (payload EXEs were signed before packaging) and emit winget YAML.
    sign_windows_artifacts
    build_winget_manifests
fi

rm -rf "$WORK"
exit "$rc"

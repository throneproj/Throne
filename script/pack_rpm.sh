#!/bin/bash
set -e

TAG="$1"
ARCH="$2"
VARIANT="$3"

RPM_VERSION="${TAG#v}"
RPM_VERSION="${RPM_VERSION#V}"
#RPM Version and Release fields both reject '-'
RPM_VERSION="${RPM_VERSION//-/_}"

# rpm and this project name arches differently from the amd64/arm64 used elsewhere.
RPM_ARCH=$([[ "$ARCH" == "amd64" ]] && echo "x86_64" || echo "aarch64")

SUFFIX=""
[[ "$VARIANT" == "systemqt" ]] && SUFFIX="-system-qt"
SRC_DIR="$PWD/linux-$ARCH$SUFFIX"
DIST="$(cd "$(dirname "$0")/../dist/linux" && pwd)"

DEPENDS=""
if [[ "$VARIANT" == "systemqt" ]]; then
    DEPENDS=$(cat <<'EOF'
Requires: (qt6-qtbase-gui >= 6.5 or libQt6Gui6 >= 6.5)
Requires: (qt6-qtwayland >= 6.5 or libQt6WaylandClient6 >= 6.5)
Requires: (google-noto-emoji-color-fonts or google-noto-coloremoji-fonts or noto-coloremoji-fonts)
EOF
)
else
    # Fedora/RHEL name first, then openSUSE. usr/lib bundles only Qt, ICU and xcb-cursor/xcb-util; Throne links libxcb
    # itself, and the bundled Qt loads the rest from the system even headless.
    DEPENDS=$(cat <<'EOF'
Requires: (libglvnd-egl or libEGL1)
Requires: (libglvnd-opengl or libOpenGL0)
Requires: (libglvnd-glx or libGL1)
Requires: (libglvnd-glx or libGLX0)
Requires: (fontconfig or libfontconfig1)
Requires: (freetype or libfreetype6)
Requires: (libxkbcommon or libxkbcommon0)
Requires: (libX11 or libX11-6)
Requires: (libX11-xcb or libX11-xcb1)
Requires: (libxcb or libxcb1)
Requires: (dbus-libs or libdbus-1-3)
Requires: (glib2 or libglib-2_0-0)
Requires: (openssl-libs or libopenssl3)
Requires: (krb5-libs or krb5)
Requires: ca-certificates
EOF
)
fi

# AutoReqProv is off, so the glibc floor comes from the highest GLIBC_ symbol version the packed files need.
GLIBC_NEED=$( (find "$SRC_DIR" -type f \( -name Throne -o -name ThroneCore -o -name '*.so*' \) -exec readelf -V -W {} + 2>/dev/null || true) \
    | { grep -o 'GLIBC_[0-9][0-9.]*' || true; } | sed 's/^GLIBC_//' | sort -Vu | tail -n1)
if [ -n "$GLIBC_NEED" ]; then
    DEPENDS="Requires: glibc >= $GLIBC_NEED
$DEPENDS"
fi

# Private work dir so pack_release.sh can build every package concurrently.
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS}

cat >"$WORK/Throne.desktop" <<-EOF
[Desktop Entry]
Name=Throne
Comment=Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
Exec=sh -c "PATH=/opt/Throne:\$PATH /opt/Throne/Throne -appdata"
Icon=/opt/Throne/Throne.png
Terminal=false
Type=Application
Categories=Network;Application;
EOF

# Scriptlets: POSIX sh, never fail the transaction, never enable or start throne.service, and touch systemd only
# when it is running (not in a chroot or container). On upgrade the new %post runs after the new files are in place.
cat >"$WORK/Throne.spec" <<-EOF
Name: throne
Version: ${RPM_VERSION}
Release: 1
Summary: Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
License: GPL-3.0-or-later
# deps should be declared by hand when AutoReqProv: no (couldnt be trusted on multi arch)
AutoReqProv: no
%define debug_package %{nil}
%define __os_install_post %{nil}
${DEPENDS}
Requires(post): desktop-file-utils
Requires(postun): desktop-file-utils

%description
Qt based cross-platform GUI proxy configuration manager (backend: sing-box).

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/opt/Throne
cp -a ${SRC_DIR}/. %{buildroot}/opt/Throne/
rm -f %{buildroot}/opt/Throne/Throne.debug
mkdir -p %{buildroot}/usr/share/applications
cp ${WORK}/Throne.desktop %{buildroot}/usr/share/applications/Throne.desktop
mkdir -p %{buildroot}/usr/lib/systemd/system %{buildroot}/etc/throne
install -m 0644 ${DIST}/throne.service %{buildroot}/usr/lib/systemd/system/throne.service
install -m 0644 ${DIST}/throne.env %{buildroot}/etc/throne/throne.env

%files
/opt/Throne
/usr/share/applications/Throne.desktop
/usr/lib/systemd/system/throne.service
%dir /etc/throne
%config(noreplace) /etc/throne/throne.env

%post
update-desktop-database >/dev/null 2>&1 || :
if [ -d /run/systemd/system ]; then
    systemctl daemon-reload >/dev/null 2>&1 || :
    if [ "\$1" -gt 1 ]; then
        systemctl try-restart throne.service >/dev/null 2>&1 || :
    fi
fi

%preun
if [ "\$1" -eq 0 ]; then
    if [ -d /run/systemd/system ]; then
        systemctl stop throne.service >/dev/null 2>&1 || :
    fi
    if command -v systemctl >/dev/null 2>&1; then
        systemctl --no-reload disable throne.service >/dev/null 2>&1 || :
    fi
fi

%postun
update-desktop-database >/dev/null 2>&1 || :
if [ -d /run/systemd/system ]; then
    systemctl daemon-reload >/dev/null 2>&1 || :
fi
EOF

# zstd -19 like the .deb files; T0 lets rpm use one worker per core.
rpmbuild -bb \
  --define "_topdir $WORK" \
  --define "_binary_payload w19T0.zstdio" \
  --target "$RPM_ARCH" \
  "$WORK/Throne.spec"

mv "$WORK/RPMS/$RPM_ARCH/throne-${RPM_VERSION}-1.${RPM_ARCH}.rpm" "Throne-$TAG-fedora-$ARCH$SUFFIX.rpm"

#!/bin/bash
set -e

VERSION="$1"
ARCH="$2"
SUFFIX=""
[[ $3 == "systemqt" ]] && SUFFIX="-system-qt"
DIST="$(cd "$(dirname "$0")/../dist/linux" && pwd)"

if [[ $3 == "systemqt" ]]; then
    DEPENDS="desktop-file-utils, libqt6core6, libqt6gui6, libqt6network6, libqt6widgets6, qt6-qpa-plugins, qt6-wayland, qt6-gtk-platformtheme, qt6-xdgdesktopportal-platformtheme, libxcb-cursor0, fonts-noto-color-emoji"
else
    # usr/lib bundles only Qt, ICU and xcb-cursor/xcb-util. Throne links libxcb itself, and the bundled Qt loads the
    # rest from the system even headless, so slim images and Pi OS Lite fail in the loader without them.
    DEPENDS="desktop-file-utils, libegl1, libgl1, libopengl0, libglx0, libfontconfig1, libfreetype6, libxkbcommon0, libx11-6, libx11-xcb1, libxcb1, libdbus-1-3, libglib2.0-0 | libglib2.0-0t64, libssl3 | libssl3t64, libgssapi-krb5-2, ca-certificates"
fi

# No dpkg-shlibdeps here, so the glibc floor comes from the highest GLIBC_ symbol version the packed files need:
# apt then refuses a system whose loader would reject them.
GLIBC_NEED=$( (find "linux-$ARCH$SUFFIX" -type f \( -name Throne -o -name ThroneCore -o -name '*.so*' \) -exec readelf -V -W {} + 2>/dev/null || true) \
    | { grep -o 'GLIBC_[0-9][0-9.]*' || true; } | sed 's/^GLIBC_//' | sort -Vu | tail -n1)
if [ -n "$GLIBC_NEED" ]; then
    DEPENDS="libc6 (>= $GLIBC_NEED), $DEPENDS"
fi

# Private staging dir so pack_release.sh can build every package concurrently.
PKG=$(mktemp -d)
trap 'rm -rf "$PKG"' EXIT
chmod 0755 "$PKG"

mkdir -p "$PKG/DEBIAN" "$PKG/opt" "$PKG/usr/lib/systemd/system" "$PKG/etc/throne"
cp -r "linux-$ARCH$SUFFIX" "$PKG/opt/Throne"
rm -f "$PKG/opt/Throne/Throne.debug"

# headless service: shipped disabled, never enabled or started by the package
install -m 0644 "$DIST/throne.service" "$PKG/usr/lib/systemd/system/throne.service"
install -m 0644 "$DIST/throne.env" "$PKG/etc/throne/throne.env"
echo /etc/throne/throne.env >"$PKG/DEBIAN/conffiles"

# basic
cat >"$PKG/DEBIAN/control" <<-EOF
Package: throne
Version: $VERSION
Architecture: $ARCH
Maintainer: Mahdi <Mahdi.zrei@gmail.com>
Depends: $DEPENDS
Description: Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
EOF

# Maintainer scripts never fail the transaction, and touch systemd only when it is running (not in a chroot or container).
cat >"$PKG/DEBIAN/postinst" <<-'EOF'
#!/bin/sh
mkdir -p /usr/share/applications
cat >/usr/share/applications/Throne.desktop<<-END
[Desktop Entry]
Name=Throne
Comment=Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
Exec=sh -c "PATH=/opt/Throne:$PATH /opt/Throne/Throne -appdata"
Icon=/opt/Throne/Throne.png
Terminal=false
Type=Application
Categories=Network;Application;
END

update-desktop-database >/dev/null 2>&1 || true

# $2 is the previously configured version: set on upgrade, empty on a fresh install.
if [ "$1" = configure ] && [ -d /run/systemd/system ]; then
    systemctl daemon-reload >/dev/null 2>&1 || true
    if [ -n "$2" ]; then
        systemctl try-restart throne.service >/dev/null 2>&1 || true
    fi
fi
exit 0
EOF

# An upgrade keeps the service running on the old files until postinst restarts it.
cat >"$PKG/DEBIAN/prerm" <<-'EOF'
#!/bin/sh
if [ "$1" = remove ]; then
    if [ -d /run/systemd/system ]; then
        systemctl stop throne.service >/dev/null 2>&1 || true
    fi
    if command -v systemctl >/dev/null 2>&1; then
        systemctl --no-reload disable throne.service >/dev/null 2>&1 || true
    fi
fi
exit 0
EOF

cat >"$PKG/DEBIAN/postrm" <<-'EOF'
#!/bin/sh
# postinst writes the launcher outside the package's file list, so it goes with the package.
if [ "$1" = remove ] || [ "$1" = purge ]; then
    rm -f /usr/share/applications/Throne.desktop
    command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database >/dev/null 2>&1 || true
fi
if [ -d /run/systemd/system ]; then
    systemctl daemon-reload >/dev/null 2>&1 || true
fi
exit 0
EOF

chmod 0755 "$PKG/DEBIAN/postinst" "$PKG/DEBIAN/prerm" "$PKG/DEBIAN/postrm"

# desktop && PATH

dpkg-deb --root-owner-group --build "$PKG" "Throne-$VERSION-debian-$ARCH$SUFFIX.deb"

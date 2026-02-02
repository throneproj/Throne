#!/bin/bash
set -e

version="${1:-0.1}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

ARCH="amd64"
[[ $(uname -m) == aarch64 || $(uname -m) == arm64 ]] && ARCH="arm64"
DEPLOY_DIR="$ROOT/deployment/linux-system-qt-$ARCH"

if [ ! -d "$DEPLOY_DIR" ]; then
  echo "Каталог $DEPLOY_DIR не найден. Сначала выполните:"
  echo "  ./script/build_local_linux.sh"
  echo "  ./script/deploy_linux64_system_qt.sh"
  exit 1
fi

rm -rf Throne
mkdir -p Throne/DEBIAN
mkdir -p Throne/opt
cp -r "$DEPLOY_DIR" Throne/opt
mv "Throne/opt/linux-system-qt-$ARCH" Throne/opt/Throne
rm -f Throne/opt/Throne/Throne.debug

# basic
cat >Throne/DEBIAN/control <<-EOF
Package: Throne
Version: $version
Architecture: $ARCH
Maintainer: Mahdi Mahdi.zrei@gmail.com
Depends: desktop-file-utils, libqt6core6, libqt6gui6, libqt6network6, libqt6widgets6, qt6-qpa-plugins, qt6-wayland, qt6-gtk-platformtheme, qt6-xdgdesktopportal-platformtheme, libxcb-cursor0, fonts-noto-color-emoji
Description: Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
EOF

cat >Throne/DEBIAN/postinst <<-EOF
cat >/usr/share/applications/Throne.desktop<<-END
[Desktop Entry]
Name=Throne
Comment=Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
Exec=sh -c "PATH=/opt/Throne:\$PATH /opt/Throne/Throne -appdata"
Icon=/opt/Throne/Throne.png
Terminal=false
Type=Application
Categories=Network;Application;
END

update-desktop-database
EOF

chmod 0755 Throne/DEBIAN/postinst

dpkg-deb --build Throne
echo "Собран пакет: $ROOT/Throne.deb"

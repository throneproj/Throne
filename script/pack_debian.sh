#!/bin/bash
set -e

version="$1"

mkdir -p Throne/DEBIAN
mkdir -p Throne/opt
cp -r linux-amd64 Throne/opt
mv Throne/opt/linux-amd64 Throne/opt/Throne

# basic
cat >Throne/DEBIAN/control <<-EOF
Package: Throne
Version: $version
Architecture: amd64
Maintainer: Mahdi Mahdi.zrei@gmail.com
Depends: desktop-file-utils
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

sudo chmod 0755 Throne/DEBIAN/postinst

# desktop && PATH

sudo dpkg-deb --build Throne

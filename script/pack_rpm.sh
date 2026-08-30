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

DEPENDS=""
[[ "$VARIANT" == "systemqt" ]] && DEPENDS="Requires: qt6-qtbase qt6-qtbase-gui qt6-qtwayland xcb-util-cursor google-noto-emoji-color-fonts"

WORK="$PWD/rpmwork"
rm -rf "$WORK"
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

cat >"$WORK/Throne.spec" <<-EOF
Name: Throne
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

%files
/opt/Throne
/usr/share/applications/Throne.desktop

%post
update-desktop-database &> /dev/null || :

%postun
update-desktop-database &> /dev/null || :
EOF

rpmbuild -bb \
  --define "_topdir $WORK" \
  --target "$RPM_ARCH" \
  "$WORK/Throne.spec"

mv "$WORK/RPMS/$RPM_ARCH/Throne-${RPM_VERSION}-1.${RPM_ARCH}.rpm" ./Throne.rpm

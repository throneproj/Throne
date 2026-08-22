#!/bin/bash
set -e

TAG="$1"
ARCH="$2"

RPM_VERSION="${TAG#v}"
RPM_VERSION="${RPM_VERSION#V}"
# RPM release fields reject '-', which tags like "v1.2.3-beta1" would otherwise contain.
RPM_VERSION="${RPM_VERSION//-/_}"

# rpm and this project name arches differently from the amd64/arm64 used elsewhere.
RPM_ARCH=$([[ "$ARCH" == "amd64" ]] && echo "x86_64" || echo "aarch64")

WORK="$PWD/rpmwork"
BUILDROOT="$WORK/buildroot"
rm -rf "$WORK"
mkdir -p "$BUILDROOT/opt" "$BUILDROOT/usr/share/applications" "$WORK/RPMS"

cp -r "linux-$ARCH$([[ $3 == "systemqt" ]] && echo "-system-qt")" "$BUILDROOT/opt/Throne"
rm -f "$BUILDROOT/opt/Throne/Throne.debug"

cat >"$BUILDROOT/usr/share/applications/Throne.desktop" <<-EOF
[Desktop Entry]
Name=Throne
Comment=Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
Exec=sh -c "PATH=/opt/Throne:\$PATH /opt/Throne/Throne -appdata"
Icon=/opt/Throne/Throne.png
Terminal=false
Type=Application
Categories=Network;Application;
EOF

DEPENDS=""
[[ $3 == "systemqt" ]] && DEPENDS="Requires: qt6-qtbase qt6-qtbase-gui qt6-qtwayland xcb-util-cursor google-noto-emoji-color-fonts"

# %install is a no-op on purpose: the buildroot above is already fully populated,
# and rpmbuild does not touch existing buildroot contents unless %install does.
cat >"$WORK/Throne.spec" <<-EOF
Name: Throne
Version: ${RPM_VERSION}
Release: 1
Summary: Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
License: Custom
BuildArch: ${RPM_ARCH}
${DEPENDS}
Requires(post): desktop-file-utils
Requires(postun): desktop-file-utils

%description
Qt based cross-platform GUI proxy configuration manager (backend: sing-box).

%install
true

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
  --buildroot "$BUILDROOT" \
  --define "_rpmdir $WORK/RPMS" \
  --define "_build_id_links none" \
  --target "$RPM_ARCH" \
  "$WORK/Throne.spec"

find "$WORK/RPMS" -name '*.rpm' -exec mv {} ./Throne.rpm \;
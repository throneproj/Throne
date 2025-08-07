Name: Throne
Version: 1.0.0
Release: 0%{?autorelease}
Summary: Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
URL: https://github.com/huakim/nekoray
License: GPLv3

Source0: %{url}/releases/download/%{version}/%{name}-%{version}.tar.gz
Source1: %{url}/releases/download/%{version}/vendor-%{version}.tar.gz
Source2: %{url}/releases/download/%{version}/%{name}.Sagernet.SingBox.Version.txt
Source3: %{url}/releases/download/%{version}/protorpc_generated-%{version}.tar.gz

BuildRequires: rpm_macro(cmake)
BuildRequires: rpm_macro(cmake_build)
BuildRequires: rpm_macro(cmake_install)
BuildRequires: cmake
BuildRequires: gcc-c++
BuildRequires: pkgconfig(protobuf)
BuildRequires: pkgconfig(libcurl)
BuildRequires: cmake(yaml-cpp)
BuildRequires: cmake(ZXing)
BuildRequires: cmake(absl)
BuildRequires: cmake(cpr)
BuildRequires: cmake(Qt6)
BuildRequires: cmake(Qt6Network)
BuildRequires: cmake(Qt6Svg)
BuildRequires: cmake(Qt6Linguist)
BuildRequires: cmake(Qt6Charts)
BuildRequires: patchelf

BuildRequires: sed
BuildRequires: golang

%package -n throne
Summary: %{summary}
Requires: throne-core
%define core Core

%package -n throne-core
Summary: %{summary}

%description
%{summary}.

%description -n throne
%{summary}.

%description -n throne-core
%{summary}.

%prep
%autosetup -p1 -n %{name}-%{version}
sed -i 's~add_library(qhotkey 3rdparty/QHotkey/qhotkey.cpp)~add_library(qhotkey STATIC 3rdparty/QHotkey/qhotkey.cpp)~' cmake/QHotkey.cmake

%build
%{?!__cmake_builddir:%define __cmake_builddir %__builddir}

(
DEST=$PWD/%{__cmake_builddir}
GOARCH=""
GOOS=darwin
GOCMD='go --mod vendor %{?gobuildflags}'

pushd core/server
%{_rpmconfigdir}/rpmuncompress -xv %{SOURCE1}
%{_rpmconfigdir}/rpmuncompress -xv %{SOURCE3}
popd

sed -i "s~protoc.*~~g;s~\(VERSION_SINGBOX=\).*~\1$(cat %{SOURCE2})~g;" script/build_go.sh

. script/build_go.sh
)

(
export CXXFLAGS="$CXXFLAGS -Wno-error=return-type"
export CFLAGS="$CFLAGS -Wno-error=return-type"
%cmake
%cmake_build
)

%install
mkdir -p %{buildroot}%{_libdir}/%{name}
mkdir -p %{buildroot}%{_bindir}
mkdir -p %{buildroot}%{_datadir}/applications
mkdir -p %{buildroot}%{_datadir}/icons

regex='s~/lib64~%{_libdir}~g;s~/bin~%{_bindir}~g;s~/usr/share~%{_datadir}~g;s~nekoray~%{name}~g'

cat << EOF > %{buildroot}%{_bindir}/throne
#!%{_bindir}/sh
%{_libdir}/%{name}/%{name} -appdata "${@}"
EOF

cat << EOF > %{buildroot}%{_datadir}/applications/throne.desktop
[Desktop Entry]
Version=1.0
Terminal=false
Type=Application
Name=throne
Categories=Network;
Comment=Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
Comment[zh_CN]=基于 Qt 的跨平台代理配置管理器 (后端 sing-box)
Keywords=Internet;VPN;Proxy;sing-box;
Exec=%{_bindir}/throne
Icon=%{_datadir}/icons/%{name}.ico
EOF

cp %{__cmake_builddir}/%{name} %{buildroot}%{_libdir}/%{name}/%{name}
cp %{__cmake_builddir}/%{core} %{buildroot}%{_libdir}/%{name}/%{core}
cp res/%{name}.ico %{buildroot}%{_datadir}/icons/%{name}.ico
patchelf --remove-rpath %{buildroot}%{_libdir}/%{name}/%{name}
#patchelf --remove-rpath %{buildroot}%{_libdir}/%{name}/%{core}

%files -n throne
%attr(0755, -, -) %{_bindir}/throne
%attr(0755, -, -) %{_libdir}/%{name}/%{name}
%attr(0644, -, -) %{_datadir}/icons/%{name}.ico
%attr(0644, -, -) %{_datadir}/applications/%{name}.desktop

%files -n throne-core
%dir %{_libdir}/%{name}
%attr(0755, -, -) %{_libdir}/%{name}/%{core}


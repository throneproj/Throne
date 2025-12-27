git clone https://code.qt.io/qt/qt5.git qt6
cd qt6

git switch %1
mkdir build
CALL .\configure.bat -no-schannel -openssl-linked -no-dtls -no-ocsp -release -static -platform win32-msvc -prefix ./build -static-runtime -submodules qtbase,qtimageformats,qtsvg,qttranslations -skip tests -skip examples -gui -widgets -init-submodules -- -D OPENSSL_ROOT_DIR="%VCPKG_ROOT%/installed/x64-windows-static"
echo on branch %1
echo config complete, building...
cmake --build . --parallel
echo build one, installing...
ninja install
echo installed Qt %1 in static mode

cd ..

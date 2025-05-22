git clone https://code.qt.io/qt/qt5.git qt6
cd qt6

git switch %1
mkdir build
CALL .\configure.bat -release -static -prefix ./build -static-runtime -submodules qtbase,qtimageformats,qtsvg,qttranslations -skip tests -skip examples -gui -widgets -init-submodules
echo on branch %1
echo config complete, building...
cmake --build . --parallel
echo build one, installing...
ninja install
echo installed Qt %1 in static mode

cd ..

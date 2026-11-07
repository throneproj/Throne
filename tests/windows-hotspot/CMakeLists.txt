cmake_minimum_required(VERSION 3.20)
project(WindowsHotspotTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Test)
get_filename_component(THRONE_ROOT "../.." ABSOLUTE)
qt_add_executable(windows_hotspot_tests
    windows_hotspot_tests.cpp
    "${THRONE_ROOT}/src/sys/windows/WindowsHotspot.cpp"
    "${THRONE_ROOT}/include/sys/WindowsHotspot.h")
target_include_directories(windows_hotspot_tests PRIVATE "${THRONE_ROOT}")
target_link_libraries(windows_hotspot_tests PRIVATE Qt6::Core Qt6::Test)
if(MSVC)
    target_compile_options(windows_hotspot_tests PRIVATE /utf-8)
endif()
enable_testing()
add_test(NAME windows_hotspot_tests COMMAND windows_hotspot_tests)

option(HOTSPOT_BUILD_LIVE_SMOKE "Build opt-in test that changes the REAL Windows hotspot" OFF)
if(HOTSPOT_BUILD_LIVE_SMOKE)
    qt_add_executable(hotspot_live_smoke live_smoke.cpp
        "${THRONE_ROOT}/src/sys/windows/WindowsHotspot.cpp"
        "${THRONE_ROOT}/include/sys/WindowsHotspot.h")
    target_include_directories(hotspot_live_smoke PRIVATE "${THRONE_ROOT}")
    target_link_libraries(hotspot_live_smoke PRIVATE Qt6::Core)
    set_source_files_properties("${THRONE_ROOT}/script/windows_hotspot_ui.ps1" PROPERTIES QT_RESOURCE_ALIAS windows_hotspot_ui.ps1)
    qt_add_resources(hotspot_live_smoke hotspot_script PREFIX /windows FILES "${THRONE_ROOT}/script/windows_hotspot_ui.ps1")
    if(MSVC)
        target_compile_options(hotspot_live_smoke PRIVATE /utf-8)
    endif()
endif()

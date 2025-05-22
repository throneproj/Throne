cmake_minimum_required(VERSION 3.10)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Gui REQUIRED)

# General settings
set(CPACK_PACKAGE_VENDOR "Skycoder42")
set(CPACK_PACKAGE_CONTACT "Shatur")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
# CPACK: DEB Specific Settings
set(CPACK_DEBIAN_PACKAGE_NAME "libqhotkey")
set(CPACK_DEBIAN_PACKAGE_SECTION "Libraries")
# Set dependencies
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6x11extras6 (>= 6.2.0)")
include(CPack)

add_library(qhotkey 3rdparty/QHotkey/qhotkey.cpp)
add_library(QHotkey::QHotkey ALIAS qhotkey)
target_link_libraries(qhotkey PUBLIC Qt6::Core Qt6::Gui)

if(APPLE)
    find_library(CARBON_LIBRARY Carbon)
    mark_as_advanced(CARBON_LIBRARY)

    target_sources(qhotkey PRIVATE 3rdparty/QHotkey/qhotkey_mac.cpp)
    target_link_libraries(qhotkey PRIVATE ${CARBON_LIBRARY})
elseif(WIN32)
    target_sources(qhotkey PRIVATE 3rdparty/QHotkey/qhotkey_win.cpp)
else()
    find_package(X11 REQUIRED)
    target_link_libraries(qhotkey PRIVATE ${X11_LIBRARIES})

    include_directories(${X11_INCLUDE_DIR})
    target_sources(qhotkey PRIVATE 3rdparty/QHotkey/qhotkey_x11.cpp)
endif()

include(GNUInstallDirs)

target_include_directories(qhotkey
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/QHotkey>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

include(CMakePackageConfigHelpers)

set_target_properties(qhotkey PROPERTIES
    SOVERSION ${PROJECT_VERSION_MAJOR}
    VERSION ${PROJECT_VERSION}
    INTERFACE_QHotkey_MAJOR_VERSION ${PROJECT_VERSION_MAJOR}
    COMPATIBLE_INTERFACE_STRING QHotkey_MAJOR_VERSION)

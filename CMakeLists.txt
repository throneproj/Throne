cmake_minimum_required(VERSION 3.10)

project(Throne VERSION 0.1)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif ()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network LinguistTools DBus)

if (NKR_CROSS)
    set_property(TARGET Qt6::moc PROPERTY IMPORTED_LOCATION /usr/bin/moc)
    set_property(TARGET Qt6::uic PROPERTY IMPORTED_LOCATION /usr/bin/uic)
    set_property(TARGET Qt6::rcc PROPERTY IMPORTED_LOCATION /usr/bin/rcc)
    set_property(TARGET Qt6::lrelease PROPERTY IMPORTED_LOCATION /usr/bin/lrelease)
    set_property(TARGET Qt6::lupdate PROPERTY IMPORTED_LOCATION /usr/bin/lupdate)
endif ()

#### Platform Variables ####
if (APPLE)
    include("cmake/macos/macos.cmake")
elseif (WIN32)
    include("cmake/windows/windows.cmake")
else ()
    include("cmake/linux/linux.cmake")
endif ()

#### default prefix path ####

message("[CMAKE_PREFIX_PATH] ${CMAKE_PREFIX_PATH}")

# for some cross toolchain
list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_PREFIX_PATH})
message("[CMAKE_FIND_ROOT_PATH] ${CMAKE_FIND_ROOT_PATH}")

#### NKR ####

include("cmake/print.cmake")
include("cmake/nkr.cmake")
include("cmake/QHotkey.cmake")

find_package(Threads)

if (NKR_PACKAGE OR NKR_PACKAGE_MACOS)
    nkr_add_compile_definitions(NKR_CPP_USE_APPDATA)
endif ()

include("cmake/myproto.cmake")
list(APPEND NKR_EXTERNAL_TARGETS myproto)

set(BUILD_SHARED_LIBS OFF)
list(APPEND NKR_EXTERNAL_TARGETS qhotkey)

# Sources
set(PROJECT_SOURCES
        ${PLATFORM_SOURCES}

        src/main.cpp
        src/global/Configs.cpp
        src/global/Utils.cpp
        src/global/HTTPRequestHelper.cpp

        3rdparty/base64.cpp
        3rdparty/qrcodegen.cpp
        3rdparty/QtExtKeySequenceEdit.cpp
        3rdparty/QrDecoder.cpp

        3rdparty/qv2ray/v2/ui/LogHighlighter.cpp
        3rdparty/qv2ray/v2/ui/QvAutoCompleteTextEdit.cpp
        3rdparty/qv2ray/v2/ui/widgets/common/QJsonModel.cpp
        3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.cpp
        3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp
        3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.ui
        3rdparty/qv2ray/v2/ui/widgets/speedchart/SpeedWidget.cpp
        3rdparty/qv2ray/v2/ui/widgets/speedchart/SpeedWidget.hpp
        3rdparty/qv2ray/v2/proxy/QvProxyConfigurator.cpp

        3rdparty/quirc/decode.c
        3rdparty/quirc/identify.c
        3rdparty/quirc/quirc.c
        3rdparty/quirc/version_db.c

        3rdparty/protorpc/rpc_client.cc
        3rdparty/protorpc/rpc_conn.cc
        3rdparty/protorpc/rpc_wire.cc
        3rdparty/protorpc/wire.pb/wire.pb.cc

        src/api/RPC.cpp

        src/dataStore/Database.cpp
        src/stats/traffic/TrafficLooper.cpp
        src/dataStore/ProfileFilter.cpp
        src/configs/ConfigBuilder.cpp

        src/configs/proxy/AbstractBean.cpp
        src/configs/proxy/Bean2CoreObj_box.cpp
        src/configs/proxy/Bean2Link.cpp
        src/configs/proxy/Link2Bean.cpp
        include/configs/proxy/ChainBean.hpp # translate

        include/configs/sub/GroupUpdater.hpp
        src/configs/sub/GroupUpdater.cpp

        include/sys/Process.hpp
        src/sys/Process.cpp
        src/sys/AutoRun.cpp

        include/ui/setting/ThemeManager.hpp
        src/ui/setting/ThemeManager.cpp
        src/ui/setting/Icon.cpp

        src/ui/mainwindow_grpc.cpp
        src/ui/mainwindow.cpp
        include/ui/mainwindow.h
        include/ui/mainwindow.ui

        include/ui/profile/dialog_edit_profile.h
        src/ui/profile/dialog_edit_profile.cpp
        include/ui/profile/dialog_edit_profile.ui
        include/ui/group/dialog_edit_group.h
        src/ui/group/dialog_edit_group.cpp
        include/ui/group/dialog_edit_group.ui

        include/ui/profile/edit_chain.h
        src/ui/profile/edit_chain.cpp
        include/ui/profile/edit_chain.ui
        include/ui/profile/edit_socks_http.h
        src/ui/profile/edit_socks_http.cpp
        include/ui/profile/edit_socks_http.ui
        include/ui/profile/edit_shadowsocks.h
        src/ui/profile/edit_shadowsocks.cpp
        include/ui/profile/edit_shadowsocks.ui
        include/ui/profile/edit_vmess.h
        src/ui/profile/edit_vmess.cpp
        include/ui/profile/edit_vmess.ui
        include/ui/profile/edit_trojan_vless.h
        src/ui/profile/edit_trojan_vless.cpp
        include/ui/profile/edit_trojan_vless.ui
        include/ui/profile/edit_anytls.h
        src/ui/profile/edit_anytls.cpp
        include/ui/profile/edit_anytls.ui

        include/ui/profile/edit_quic.h
        src/ui/profile/edit_quic.cpp
        include/ui/profile/edit_quic.ui

        include/ui/profile/edit_custom.h
        src/ui/profile/edit_custom.cpp
        include/ui/profile/edit_custom.ui

        include/ui/profile/edit_extra_core.h
        src/ui/profile/edit_extra_core.cpp
        include/ui/profile/edit_extra_core.ui

        include/ui/profile/edit_wireguard.h
        src/ui/profile/edit_wireguard.cpp
        include/ui/profile/edit_wireguard.ui

        src/ui/setting/dialog_basic_settings.cpp
        include/ui/setting/dialog_basic_settings.h
        include/ui/setting/dialog_basic_settings.ui

        src/ui/group/dialog_manage_groups.cpp
        include/ui/group/dialog_manage_groups.h
        include/ui/group/dialog_manage_groups.ui

        src/ui/setting/dialog_manage_routes.cpp
        include/ui/setting/dialog_manage_routes.h
        include/ui/setting/dialog_manage_routes.ui

        src/ui/setting/dialog_vpn_settings.cpp
        include/ui/setting/dialog_vpn_settings.h
        include/ui/setting/dialog_vpn_settings.ui

        src/ui/setting/dialog_hotkey.cpp
        include/ui/setting/dialog_hotkey.h
        include/ui/setting/dialog_hotkey.ui

        src/ui/profile/ProxyItem.cpp
        include/ui/profile/ProxyItem.h
        include/ui/profile/ProxyItem.ui
        src/ui/group/GroupItem.cpp
        include/ui/group/GroupItem.h
        include/ui/group/GroupItem.ui
        src/ui/setting/RouteItem.cpp
        include/ui/setting/RouteItem.h
        include/ui/setting/RouteItem.ui

        res/Throne.qrc
        ${QV2RAY_RC}
        include/dataStore/RouteEntity.h
        src/dataStore/RouteEntity.cpp
        res/darkstyle.qrc
        src/ui/profile/edit_ssh.cpp
        include/ui/profile/edit_ssh.h
        include/ui/profile/edit_ssh.ui
        include/configs/proxy/SSHBean.h
        include/sys/macos/MacOS.h

        src/sys/linux/LinuxCap.cpp

        src/sys/macos/MacOS.cpp

        src/sys/AutoRun.cpp
        src/sys/Process.cpp
        include/ui/mainwindow_interface.h
        include/stats/connections/connectionLister.hpp
        src/stats/connectionLister/connectionLister.cpp
        src/configs/proxy/Json2Bean.cpp
        include/sys/windows/eventHandler.h
        src/dataStore/Group.cpp
        src/dataStore/ProxyEntity.cpp
)

# Qt exe
qt_add_executable(Throne
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
)

# Target

set_property(TARGET Throne PROPERTY AUTOUIC ON)
set_property(TARGET Throne PROPERTY AUTOMOC ON)
set_property(TARGET Throne PROPERTY AUTORCC ON)


# Target Source macOS

set(MACOSX_ICON ${CMAKE_SOURCE_DIR}/res/Throne.icns)
if (APPLE)
    target_sources(Throne PRIVATE ${MACOSX_ICON})
endif ()

set_target_properties(Throne PROPERTIES
        MACOSX_BUNDLE_ICON_FILE "Throne.icns"
        RESOURCE ${MACOSX_ICON}
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/res/MacOSXBundleInfo.plist
        MACOSX_BUNDLE_GUI_IDENTIFIER moe.Throne.macosx
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE

)

# Target Source Translations

set(TS_FILES
        res/translations/zh_CN.ts
        res/translations/fa_IR.ts
        res/translations/ru_RU.ts
)
set(LUPDATE_OPTIONS
        -locations none -no-obsolete
)
qt_add_lupdate(Throne TS_FILES ${TS_FILES} OPTIONS ${LUPDATE_OPTIONS})
qt_add_lrelease(Throne TS_FILES ${TS_FILES} QM_FILES_OUTPUT_VARIABLE QM_FILES)

configure_file(res/translations/translations.qrc ${CMAKE_BINARY_DIR} COPYONLY)
target_sources(Throne PRIVATE ${CMAKE_BINARY_DIR}/translations.qrc)

# Target Link

target_link_libraries(Throne PRIVATE
        Qt6::Widgets Qt6::Network Qt6::DBus
        Threads::Threads
        ${NKR_EXTERNAL_TARGETS}
        ${PLATFORM_LIBRARIES}
)

qt_finalize_executable(Throne)

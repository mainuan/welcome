cmake_minimum_required(VERSION 3.24)

project(mainuan-welcome VERSION 2.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

option(MAINUAN_DEVELOPER_MODE "Allow MAINUAN_DEV_* variables to replace the installer backends" OFF)
set(MAINUAN_HELPER_PATH "${CMAKE_INSTALL_FULL_LIBEXECDIR}/mainuan-welcome/mainuan-welcome-helper")

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Widgets
    DBus
    Network
    Multimedia
)

find_package(Qt6 6.5 QUIET COMPONENTS Test)
# KConfig reads Plasma settings through the same cascade Plasma uses
# (~/.config/kdedefaults holds the values written by global themes).
find_package(KF6Config 6.0 REQUIRED)
# QR codes for Contribuir are generated locally.
find_package(PkgConfig REQUIRED)
pkg_check_modules(QRENCODE REQUIRED IMPORTED_TARGET libqrencode)
find_package(KF6Kirigami QUIET CONFIG)

qt_standard_project_setup(REQUIRES 6.5)
if (COMMAND qt_policy)
    qt_policy(SET QTP0004 NEW)
endif()

add_library(mainuan-core STATIC
    src/services/SystemService.cpp
    src/services/SystemService.h
    src/services/VisualStyle.cpp
    src/services/AccentProfile.cpp
    src/services/AccentProfile.h
    src/services/VisualStyle.h
    src/services/PackageService.cpp
    src/services/PackageService.h
    src/services/InstallService.cpp
    src/services/InstallService.h
    src/services/InstallProgressParser.cpp
    src/services/InstallProgressParser.h
    src/services/SingleInstance.cpp
    src/services/SingleInstance.h
    src/services/LayoutService.cpp
    src/services/PlasmaInfo.cpp
    src/services/PlasmaInfo.h
    src/services/LayoutService.h
    src/services/SystemReportService.cpp
    src/services/SystemReportService.h
    src/services/QrCode.cpp
    src/services/QrCode.h
    src/services/StartupPreference.cpp
    src/services/StartupPreference.h
    src/models/ApplicationModel.cpp
    src/models/ApplicationModel.h
    src/models/VideoModel.cpp
    src/models/VideoModel.h
    src/models/LayoutModel.cpp
    src/models/LayoutModel.h
)

target_include_directories(mainuan-core PUBLIC src)
# Plasma and KWin scripts used by LayoutService.
qt_add_resources(mainuan-core layout_scripts
    PREFIX "/layouts"
    BASE src/layouts
    FILES src/layouts/desktop-layouts.js src/layouts/kwin-tiling.js
)
target_compile_definitions(mainuan-core PRIVATE MAINUAN_HELPER_PATH="${MAINUAN_HELPER_PATH}")
target_link_libraries(mainuan-core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::DBus
    Qt6::Network
    KF6::ConfigCore
    PkgConfig::QRENCODE
)

target_compile_options(mainuan-core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wshadow>
)

qt_add_executable(mainuan-welcome
    src/main.cpp
)

qt_add_qml_module(mainuan-welcome
    URI Mainuan.Welcome
    VERSION 1.0
    # KDE's Kirigami plugins are runtime QML modules on Debian/Kubuntu.  Do not
    # try to link private plugin targets that are not exported by every KF6 release.
    NO_IMPORT_SCAN
    QML_FILES
        qml/Main.qml
        qml/components/AppCard.qml
        qml/components/StatusRow.qml
        qml/components/SidebarButton.qml
        qml/components/SettingRow.qml
        qml/components/PageContainer.qml
        qml/components/LayoutPreview.qml
        qml/components/InfoTile.qml
        qml/components/ActionCard.qml
        qml/components/ReportDialog.qml
        qml/components/SegmentedChoice.qml
        qml/components/MessageBanner.qml
        qml/components/InstallDialog.qml
        qml/pages/HomePage.qml
        qml/pages/AppearancePage.qml
        qml/pages/AppsPage.qml
        qml/pages/TutorialsPage.qml
        qml/pages/AboutPage.qml
        qml/pages/ContributePage.qml
)

set(MAINUAN_ASSETS
    rootfs/usr/share/welcome/com.brave.Browser.png
    rootfs/usr/share/welcome/com.google.Chrome.png
    rootfs/usr/share/welcome/com.microsoft.Edge.png
    rootfs/usr/share/welcome/com.opera.opera-gx.png
    rootfs/usr/share/welcome/com.wps.Office.png
    rootfs/usr/share/welcome/io.gitlab.librewolf-community.png
    rootfs/usr/share/welcome/org.libreoffice.LibreOffice.png
    rootfs/usr/share/welcome/org.torproject.torbrowser-launcher.png
    rootfs/usr/share/welcome/webapp.gdocs.png
    rootfs/usr/share/welcome/webapp.office365.png
    rootfs/usr/share/welcome/icone-videoaulas_g204.svg
    rootfs/usr/share/welcome/icone-videoaulas_g205.svg
    rootfs/usr/share/welcome/icone-videoaulas_g206.svg
    rootfs/usr/share/welcome/icone-videoaulas_g207.svg
    rootfs/usr/share/welcome/icone-videoaulas_g208.svg
    rootfs/usr/share/welcome/icone-videoaulas_g209.svg
)
foreach(asset IN LISTS MAINUAN_ASSETS)
    get_filename_component(asset_name "${asset}" NAME)
    set_source_files_properties("${asset}" PROPERTIES QT_RESOURCE_ALIAS "${asset_name}")
endforeach()

qt_add_resources(mainuan-welcome welcome_assets
    PREFIX "/assets"
    FILES ${MAINUAN_ASSETS}
)

# Line icons used across the interface (Lucide, ISC; see qml/icons/LICENSE.lucide).
file(GLOB MAINUAN_ICONS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/qml/icons/*.svg")
foreach(icon IN LISTS MAINUAN_ICONS)
    get_filename_component(icon_name "${icon}" NAME)
    set_source_files_properties("${icon}" PROPERTIES QT_RESOURCE_ALIAS "${icon_name}")
endforeach()
qt_add_resources(mainuan-welcome welcome_icons
    PREFIX "/icons"
    FILES ${MAINUAN_ICONS}
)

target_link_libraries(mainuan-welcome PRIVATE
    mainuan-core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
)

if (TARGET KF6::Kirigami)
    target_link_libraries(mainuan-welcome PRIVATE KF6::Kirigami)
endif()

target_compile_definitions(mainuan-welcome PRIVATE MAINUAN_VERSION="${PROJECT_VERSION}")
if (MAINUAN_DEVELOPER_MODE)
    target_compile_definitions(mainuan-welcome PRIVATE MAINUAN_DEVELOPER_MODE)
endif()

target_compile_options(mainuan-welcome PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wshadow>
)

set_target_properties(mainuan-welcome PROPERTIES
    OUTPUT_NAME mainuan-welcome
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

install(TARGETS mainuan-welcome
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    rootfs/usr/share/applications/org.mainuan.Welcome.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

# Configuration directory: /etc for the /usr prefix (as GNUInstallDirs does)
# or an absolute CMAKE_INSTALL_SYSCONFDIR (Debian passes /etc); otherwise
# relative, so `cmake --install --prefix <stage>` stays inside the stage.
if(IS_ABSOLUTE "${CMAKE_INSTALL_SYSCONFDIR}")
    set(MAINUAN_SYSCONFDIR "${CMAKE_INSTALL_SYSCONFDIR}")
elseif(CMAKE_INSTALL_PREFIX STREQUAL "/usr")
    set(MAINUAN_SYSCONFDIR "/etc")
else()
    set(MAINUAN_SYSCONFDIR "${CMAKE_INSTALL_SYSCONFDIR}")
endif()

# System-wide login entry; each user can hide it from the Welcome itself
# (Hidden=true in $XDG_CONFIG_HOME/autostart).
install(FILES
    rootfs/etc/xdg/autostart/org.mainuan.Welcome.desktop
    DESTINATION ${MAINUAN_SYSCONFDIR}/xdg/autostart
)

install(FILES
    rootfs/etc/mainuan/welcome.conf
    DESTINATION ${MAINUAN_SYSCONFDIR}/mainuan
)

install(FILES
    rootfs/usr/share/metainfo/org.mainuan.Welcome.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)

install(FILES doc/mainuan-welcome.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

install(PROGRAMS
    rootfs/usr/libexec/mainuan-welcome/mainuan-welcome-helper
    DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/mainuan-welcome
)

# The polkit action authorizes exactly the installed helper path.
configure_file(
    rootfs/usr/share/polkit-1/actions/org.mainuan.welcome.policy.in
    ${CMAKE_CURRENT_BINARY_DIR}/org.mainuan.welcome.policy
    @ONLY
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/org.mainuan.welcome.policy
    DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions
)

if (Qt6Test_FOUND)
    enable_testing()
    add_executable(tst_mainuan
        tests/tst_mainuan.cpp
    )
    target_link_libraries(tst_mainuan PRIVATE mainuan-core Qt6::Test)
    target_compile_options(tst_mainuan PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
    )
    target_compile_definitions(tst_mainuan PRIVATE
        MAINUAN_TEST_HELPER="${CMAKE_CURRENT_SOURCE_DIR}/rootfs/usr/libexec/mainuan-welcome/mainuan-welcome-helper"
        MAINUAN_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME tst_mainuan COMMAND tst_mainuan)
    add_test(NAME smoke_startup COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke.sh $<TARGET_FILE:mainuan-welcome>)
    set_tests_properties(smoke_startup PROPERTIES SKIP_RETURN_CODE 77)
endif()

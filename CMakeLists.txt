cmake_minimum_required(VERSION 3.24)

project(mainuan-welcome VERSION 1.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Match the distribution's oldest amd64 CPUs, including during LTO linking.
# The build environment/ELF gates also reject incompatible prebuilt inputs.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    add_compile_options(-march=x86-64 -mtune=generic)
    add_link_options(-march=x86-64 -mtune=generic)
endif()

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
# Translations: KI18n for C++ (i18n()) and KI18nQml for QML (KLocalization, KF >= 6.8).
find_package(KF6I18n 6.8 REQUIRED)
# QR codes for Contribuir are generated locally.
find_package(PkgConfig REQUIRED)
pkg_check_modules(QRENCODE REQUIRED IMPORTED_TARGET libqrencode)
find_package(KF6Kirigami QUIET CONFIG)

qt_standard_project_setup(REQUIRES 6.5)
if (COMMAND qt_policy)
    qt_policy(SET QTP0004 NEW)
endif()

add_library(mainuan-core STATIC
    src/CatalogLayoutDirection.h
    src/WindowGeometry.h
    src/services/SystemService.cpp
    src/services/SystemService.h
    src/services/VisualStyle.cpp
    src/services/AccentProfile.cpp
    src/services/AccentProfile.h
    src/services/UserAvatar.cpp
    src/services/UserAvatar.h
    src/services/VisualStyle.h
    src/services/PackageService.cpp
    src/services/PackageService.h
    src/services/InstallService.cpp
    src/services/InstallService.h
    src/services/FreeOfficeProfile.cpp
    src/services/FreeOfficeProfile.h
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
qt_add_resources(mainuan-core installation_profiles
    PREFIX "/installation"
    BASE rootfs/usr/share/mainuan-welcome
    FILES rootfs/usr/share/mainuan-welcome/freeoffice.profile
)
# Plasma and KWin scripts used by LayoutService.
qt_add_resources(mainuan-core layout_scripts
    PREFIX "/layouts"
    BASE src/layouts
    FILES src/layouts/desktop-layouts.js src/layouts/reference-layouts.js src/layouts/kwin-tiling.js
)
target_compile_definitions(mainuan-core PRIVATE MAINUAN_HELPER_PATH="${MAINUAN_HELPER_PATH}")
# Every i18n() call in the project resolves against the package's own catalog,
# also when the code runs inside a test binary that never sets an application domain.
set(MAINUAN_TRANSLATION_DOMAIN "mainuan-welcome")
target_compile_definitions(mainuan-core PUBLIC TRANSLATION_DOMAIN="${MAINUAN_TRANSLATION_DOMAIN}")
target_link_libraries(mainuan-core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::DBus
    Qt6::Network
    KF6::ConfigCore
    KF6::I18n
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
    rootfs/usr/share/welcome/freeOffice.png
    rootfs/usr/share/welcome/vivaldi.png
    rootfs/usr/share/welcome/zenBrowser.png
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
    KF6::I18n
    KF6::I18nQml
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

# --- Translations ------------------------------------------------------------
# Catalogs use the flat layout po/<lang>.po (languages listed in po/LINGUAS).
# Each one is compiled to <build>/share/locale/<lang>/LC_MESSAGES/<domain>.mo,
# which the executable also finds when it runs from the build tree, and is
# installed to /usr/share/locale. The .desktop, AppStream and polkit files are
# generated from their templates with msgfmt so they carry every language.
find_program(MAINUAN_MSGFMT NAMES msgfmt REQUIRED)
set(MAINUAN_PO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/po")
set(MAINUAN_LOCALE_BUILD_DIR "${CMAKE_BINARY_DIR}/share/locale")
file(STRINGS "${MAINUAN_PO_DIR}/LINGUAS" MAINUAN_LINGUAS REGEX "^[^#]")
string(REGEX REPLACE "[ \t]+" ";" MAINUAN_LINGUAS "${MAINUAN_LINGUAS}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MAINUAN_PO_DIR}/LINGUAS")
set(MAINUAN_PO_FILES)
set(MAINUAN_MO_FILES)
foreach(lang IN LISTS MAINUAN_LINGUAS)
    set(po "${MAINUAN_PO_DIR}/${lang}.po")
    set(mo "${MAINUAN_LOCALE_BUILD_DIR}/${lang}/LC_MESSAGES/${MAINUAN_TRANSLATION_DOMAIN}.mo")
    add_custom_command(OUTPUT "${mo}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MAINUAN_LOCALE_BUILD_DIR}/${lang}/LC_MESSAGES"
        COMMAND ${MAINUAN_MSGFMT} --check -o "${mo}" "${po}"
        DEPENDS "${po}"
        COMMENT "Compiling ${lang} catalog"
        VERBATIM)
    install(FILES "${mo}" DESTINATION "${CMAKE_INSTALL_LOCALEDIR}/${lang}/LC_MESSAGES")
    list(APPEND MAINUAN_PO_FILES "${po}")
    list(APPEND MAINUAN_MO_FILES "${mo}")
endforeach()

# msgfmt finds the ITS rules for the XML templates through po/its.
set(MAINUAN_GETTEXT_ENV ${CMAKE_COMMAND} -E env "GETTEXTDATADIRS=${MAINUAN_PO_DIR}")
file(GLOB MAINUAN_ITS_FILES CONFIGURE_DEPENDS "${MAINUAN_PO_DIR}/its/*")
list(APPEND MAINUAN_ITS_FILES "${MAINUAN_PO_DIR}/polkit-lang.cmake")
function(mainuan_translate_metadata mode template output)
    get_filename_component(output_dir "${output}" DIRECTORY)
    add_custom_command(OUTPUT "${output}"
        # msgfmt does not create the output directory (clean build trees)
        COMMAND ${CMAKE_COMMAND} -E make_directory "${output_dir}"
        COMMAND ${MAINUAN_GETTEXT_ENV} ${MAINUAN_MSGFMT} --${mode} --template "${template}" -d "${MAINUAN_PO_DIR}" -o "${output}"
        ${ARGN}
        DEPENDS "${template}" ${MAINUAN_PO_FILES} "${MAINUAN_PO_DIR}/LINGUAS" ${MAINUAN_ITS_FILES}
        COMMENT "Translating ${output}"
        VERBATIM)
endfunction()

set(MAINUAN_DESKTOP_FILE "${CMAKE_CURRENT_BINARY_DIR}/applications/org.mainuan.Welcome.desktop")
set(MAINUAN_AUTOSTART_FILE "${CMAKE_CURRENT_BINARY_DIR}/autostart/org.mainuan.Welcome.desktop")
set(MAINUAN_METAINFO_FILE "${CMAKE_CURRENT_BINARY_DIR}/org.mainuan.Welcome.metainfo.xml")
set(MAINUAN_POLICY_FILE "${CMAKE_CURRENT_BINARY_DIR}/org.mainuan.welcome.policy")
mainuan_translate_metadata(desktop
    "${CMAKE_CURRENT_SOURCE_DIR}/rootfs/usr/share/applications/org.mainuan.Welcome.desktop.in"
    "${MAINUAN_DESKTOP_FILE}")
mainuan_translate_metadata(desktop
    "${CMAKE_CURRENT_SOURCE_DIR}/rootfs/etc/xdg/autostart/org.mainuan.Welcome.desktop.in"
    "${MAINUAN_AUTOSTART_FILE}")
mainuan_translate_metadata(xml
    "${CMAKE_CURRENT_SOURCE_DIR}/rootfs/usr/share/metainfo/org.mainuan.Welcome.metainfo.xml.in"
    "${MAINUAN_METAINFO_FILE}")
# The polkit action authorizes exactly the installed helper path: substitute
# @MAINUAN_HELPER_PATH@ first, then merge the translations into the result.
configure_file(
    rootfs/usr/share/polkit-1/actions/org.mainuan.welcome.policy.in
    ${CMAKE_CURRENT_BINARY_DIR}/policy-template/org.mainuan.welcome.policy
    @ONLY
)
mainuan_translate_metadata(xml
    "${CMAKE_CURRENT_BINARY_DIR}/policy-template/org.mainuan.welcome.policy"
    "${MAINUAN_POLICY_FILE}"
    COMMAND ${CMAKE_COMMAND} -DFILE=${MAINUAN_POLICY_FILE} -P ${MAINUAN_PO_DIR}/polkit-lang.cmake)

add_custom_target(mainuan-translations ALL DEPENDS
    ${MAINUAN_MO_FILES}
    "${MAINUAN_DESKTOP_FILE}" "${MAINUAN_AUTOSTART_FILE}" "${MAINUAN_METAINFO_FILE}" "${MAINUAN_POLICY_FILE}")

install(FILES
    ${MAINUAN_DESKTOP_FILE}
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
    ${MAINUAN_AUTOSTART_FILE}
    DESTINATION ${MAINUAN_SYSCONFDIR}/xdg/autostart
)

install(FILES
    rootfs/etc/mainuan/welcome.conf
    DESTINATION ${MAINUAN_SYSCONFDIR}/mainuan
)

install(FILES
    ${MAINUAN_METAINFO_FILE}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)

install(FILES doc/mainuan-welcome.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

install(PROGRAMS
    rootfs/usr/libexec/mainuan-welcome/mainuan-welcome-helper
    DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/mainuan-welcome
)
install(FILES rootfs/usr/share/mainuan-welcome/freeoffice.profile
    DESTINATION ${CMAKE_INSTALL_DATADIR}/mainuan-welcome)

install(FILES
    ${MAINUAN_POLICY_FILE}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions
)

if (Qt6Test_FOUND)
    enable_testing()
    find_program(MAINUAN_DBUS_RUN_SESSION NAMES dbus-run-session REQUIRED)
    # A disconnected bus leaves the appearance wallpaper query unfinished.
    # Give each native suite its own bus, without Plasma service activation,
    # both for ordinary CTest and for debhelper's isolated/parallel test run.
    function(mainuan_add_isolated_test target)
        add_test(NAME ${target} COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-isolated.sh
            ${MAINUAN_DBUS_RUN_SESSION} ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/session-bus.conf
            $<TARGET_FILE:${target}>)
    endfunction()
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
    mainuan_add_isolated_test(tst_mainuan)
    add_executable(tst_appinstallation tests/tst_appinstallation.cpp)
    target_link_libraries(tst_appinstallation PRIVATE mainuan-core Qt6::Test)
    target_compile_definitions(tst_appinstallation PRIVATE
        MAINUAN_TEST_HELPER="${CMAKE_CURRENT_SOURCE_DIR}/rootfs/usr/libexec/mainuan-welcome/mainuan-welcome-helper"
        MAINUAN_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    mainuan_add_isolated_test(tst_appinstallation)
    add_executable(tst_layouts tests/tst_layouts.cpp)
    target_link_libraries(tst_layouts PRIVATE mainuan-core Qt6::Test Qt6::Qml)
    target_compile_definitions(tst_layouts PRIVATE
        MAINUAN_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    mainuan_add_isolated_test(tst_layouts)
    # i18n: the functional suites assert the English source text. LANGUAGE=en
    # keeps an installed pt_BR catalog (or a Portuguese session) out of them;
    # tst_i18n/i18n_qml below cover the translated interface.
    set(MAINUAN_TEST_I18N_ENV "LANGUAGE=en")
    set_tests_properties(tst_mainuan tst_appinstallation tst_layouts PROPERTIES ENVIRONMENT "${MAINUAN_TEST_I18N_ENV}")
    add_executable(tst_i18n tests/tst_i18n.cpp)
    target_link_libraries(tst_i18n PRIVATE mainuan-core Qt6::Test)
    add_dependencies(tst_i18n mainuan-translations)
    # glibc ignores LANGUAGE under the C locale, so both runs use a Brazilian
    # session locale (Build-Depends: locales-all); LANGUAGE=en must still win.
    foreach(lang IN ITEMS pt_BR en)
        add_test(NAME i18n_${lang} COMMAND $<TARGET_FILE:tst_i18n>)
        set_tests_properties(i18n_${lang} PROPERTIES ENVIRONMENT
            "LC_ALL=pt_BR.UTF-8;LANGUAGE=${lang};MAINUAN_EXPECT_LANGUAGE=${lang};MAINUAN_LOCALE_DIR=${MAINUAN_LOCALE_BUILD_DIR};QT_QPA_PLATFORM=offscreen")
    endforeach()
    # QML suites need KI18n's i18n() in the engine: a QtQuickTest runner with
    # the application's localization setup replaces the stock qmltestrunner.
    find_package(Qt6 6.5 QUIET COMPONENTS QuickTest)
    if(Qt6QuickTest_FOUND)
        add_executable(mainuan-qmltest tests/qmltest_main.cpp)
        target_link_libraries(mainuan-qmltest PRIVATE Qt6::QuickTest Qt6::Qml KF6::I18n KF6::I18nQml)
        target_compile_definitions(mainuan-qmltest PRIVATE TRANSLATION_DOMAIN="${MAINUAN_TRANSLATION_DOMAIN}")
        add_dependencies(mainuan-qmltest mainuan-translations)
        set(MAINUAN_QML_I18N_ENV "LANGUAGE=en")
        add_test(NAME layouts_qml COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/layouts-qml.sh
            $<TARGET_FILE:mainuan-qmltest> ${CMAKE_CURRENT_SOURCE_DIR})
        set_tests_properties(layouts_qml PROPERTIES SKIP_RETURN_CODE 77 ENVIRONMENT "${MAINUAN_QML_I18N_ENV}")
        add_test(NAME applications_qml COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/apps-qml.sh
            $<TARGET_FILE:mainuan-qmltest> ${CMAKE_CURRENT_SOURCE_DIR})
        set_tests_properties(applications_qml PROPERTIES SKIP_RETURN_CODE 77 ENVIRONMENT "${MAINUAN_QML_I18N_ENV}")
        foreach(lang IN ITEMS pt_BR en)
            add_test(NAME i18n_qml_${lang} COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/i18n-qml.sh
                $<TARGET_FILE:mainuan-qmltest> ${CMAKE_CURRENT_SOURCE_DIR} ${MAINUAN_LOCALE_BUILD_DIR} ${lang})
            set_tests_properties(i18n_qml_${lang} PROPERTIES SKIP_RETURN_CODE 77)
        endforeach()
    endif()
    add_test(NAME smoke_startup COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke.sh $<TARGET_FILE:mainuan-welcome>)
    set_tests_properties(smoke_startup PROPERTIES SKIP_RETURN_CODE 77)
    # The catalog and the metadata templates stay in sync with the sources.
    find_program(MAINUAN_XGETTEXT NAMES xgettext)
    if(MAINUAN_XGETTEXT)
        add_test(NAME i18n_catalogs COMMAND bash -c "'${MAINUAN_PO_DIR}/i18n.sh' check-pot && '${MAINUAN_PO_DIR}/i18n.sh' validate")
    endif()
    add_test(NAME i18n_metadata COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/i18n-metadata.sh
        ${MAINUAN_DESKTOP_FILE} ${MAINUAN_AUTOSTART_FILE} ${MAINUAN_METAINFO_FILE} ${MAINUAN_POLICY_FILE})
endif()

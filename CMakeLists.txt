cmake_minimum_required(VERSION 3.16)
# The release workflow passes the version; local builds use the default.
set(KDETODO_VERSION "0.1.0" CACHE STRING "Application version (MAJOR.MINOR.PATCH)")
set(KDETODO_VERSION_SUFFIX "" CACHE STRING "Suffix of the package version, e.g. +git1a2b3c4")
project(kdetodo VERSION ${KDETODO_VERSION} LANGUAGES CXX)

set(QT_MIN_VERSION 5.15.0)
set(KF5_MIN_VERSION 5.100.0)

find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KDETODO_WERROR "Treat compiler warnings as errors" OFF)
option(KDETODO_SANITIZE "Build with address and undefined behaviour sanitizers" OFF)

find_package(Qt5 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 Test)
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS CoreAddons I18n Config DBusAddons)

set(CMAKE_AUTORCC ON)

add_compile_options(-Wall -Wextra -Wpedantic)
if(KDETODO_WERROR)
    add_compile_options(-Werror)
endif()
if(KDETODO_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
    # KDE's default linker flags reject the sanitizer runtime's undefined symbols.
    string(REPLACE "-Wl,--no-undefined" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
    string(REPLACE "-Wl,--no-undefined" "" CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS}")
endif()

add_definitions(-DTRANSLATION_DOMAIN="kdetodo" -DKDETODO_VERSION="${PROJECT_VERSION}")

add_subdirectory(src)

install(FILES data/org.anjankow.kdetodo.desktop DESTINATION ${KDE_INSTALL_APPDIR})
# The application icon in the sizes the desktop asks for.
foreach(size 16 22 24 32 48 64 128 256 512)
    install(FILES data/icons/${size}/org.anjankow.kdetodo.png DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/${size}x${size}/apps)
endforeach()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# ---- Debian package (cpack -G DEB)
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "kdetodo")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}${KDETODO_VERSION_SUFFIX}")
set(CPACK_PACKAGE_CONTACT "anjankow <anjankow@protonmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A simple todo list stored in a text file (KDE)")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/anjankow/todo-ubuntu-kde")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_STRIP_FILES ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# Not found by shlibdeps: the QML modules the interface imports, and the
# SVG support for the icons of the buttons.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt5svg5, qml-module-qtquick2, qml-module-qtquick-window2, qml-module-qtquick-controls2, qml-module-qtquick-templates2, qml-module-qtquick-layouts, qml-module-qt-labs-platform, qml-module-org-kde-kirigami2, qml-module-org-kde-qqc2desktopstyle")
include(CPack)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

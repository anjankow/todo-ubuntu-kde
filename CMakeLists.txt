cmake_minimum_required(VERSION 3.16)
project(kdetodo VERSION 0.1.0 LANGUAGES CXX)

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

find_package(Qt5 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Widgets Test)
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS CoreAddons I18n Config WidgetsAddons XmlGui)

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

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

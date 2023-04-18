cmake_minimum_required(VERSION 3.15)
project(libzynthbox VERSION 0.1.0)
if (POLICY CMP0063)
    cmake_policy(SET CMP0063 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(QT_MIN_VERSION "5.11.0")
set(KF5_MIN_VERSION "5.54.0")

set(ENV{PKG_CONFIG_PATH} "/usr/lib/pkgconfig:/usr/local/lib/pkgconfig")

find_package(ECM ${KF5_MIN_VERSION} REQUIRED CONFIG)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${CMAKE_CURRENT_LIST_DIR}/cmake)

find_package(Qt5 ${QT_MIN_VERSION} REQUIRED CONFIG COMPONENTS Core Gui Quick Qml)
find_package(KF5Kirigami2 2.7.0)
find_package(Python 3.5 REQUIRED COMPONENTS Interpreter)
find_program(SHIBOKEN_GENERATOR shiboken2)

include(FindPkgConfig)
include(KDEInstallDirs)
include(ECMInstallIcons)

pkg_check_modules(RTMIDI REQUIRED rtmidi)
pkg_check_modules(Jack REQUIRED jack)

execute_process(
  COMMAND "${Python_EXECUTABLE}" -c "import sys; print(f'{sys.version_info[0]}.{sys.version_info[1]}')"
  OUTPUT_VARIABLE Python_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE)

include_directories(tracktion_engine/modules/)
add_subdirectory(tracktion_engine/modules/juce)
add_subdirectory(tracktion_engine/modules)

add_subdirectory(src)
add_subdirectory(pyside_bindings)

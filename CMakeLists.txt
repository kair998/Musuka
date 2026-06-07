cmake_minimum_required(VERSION 3.20)

project(musuka LANGUAGES CXX)

if(NOT MSVC)
    message(FATAL_ERROR "musuka currently supports the verified MSVC + NMake build only. Run build.bat from a Windows environment with Visual Studio Build Tools.")
endif()

if(NOT CMAKE_GENERATOR STREQUAL "NMake Makefiles")
    message(FATAL_ERROR "musuka currently supports only the verified NMake Makefiles generator. Run build.bat instead of selecting another CMake generator.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Qt 6 (optional) ──────────────────────────────────────────────
option(MUSUKA_USE_QT "Enable Qt 6 Widgets UI support" OFF)

if(MUSUKA_USE_QT)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
endif()

set(MUSUKA_SOURCES
    src/main.cpp
    src/App.cpp
    src/ConfigStore.cpp
    src/DesktopScanner.cpp
    src/DesktopWindow.cpp
    src/ImageUtil.cpp
    src/Json.cpp
    src/SettingsWindow.cpp
    src/Util.cpp
    src/WinUtil.cpp
    src/Musuka.rc
)

add_executable(musuka WIN32 ${MUSUKA_SOURCES})

target_compile_definitions(musuka PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(musuka PRIVATE /W4 /permissive- /utf-8)
endif()

target_link_libraries(musuka PRIVATE
    gdiplus
    comctl32
    comdlg32
    shell32
    ole32
    uuid
    shlwapi
    user32
    gdi32
)

if(MUSUKA_USE_QT)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)

    target_sources(musuka PRIVATE
        src/QtSettingsWindow.cpp
        src/QtSettingsWindow.h
    )
    target_link_libraries(musuka PRIVATE Qt6::Widgets)
    target_compile_definitions(musuka PRIVATE MUSUKA_USE_QT)

    message(STATUS "musuka: Qt 6 UI support ENABLED")
else()
    message(STATUS "musuka: building without Qt (native Win32 UI only)")
endif()

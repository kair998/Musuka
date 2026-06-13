cmake_minimum_required(VERSION 3.20)

project(musuka LANGUAGES CXX)

include(CTest)

if(NOT MSVC)
    message(FATAL_ERROR "musuka currently supports the verified MSVC + NMake build only. Run build.bat from a Windows environment with Visual Studio Build Tools.")
endif()

if(NOT CMAKE_GENERATOR STREQUAL "NMake Makefiles")
    message(FATAL_ERROR "musuka currently supports only the verified NMake Makefiles generator. Run build.bat instead of selecting another CMake generator.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(MUSUKA_SOURCES
    src/main.cpp
    src/App.cpp
    src/ConfigStore.cpp
    src/DesktopScanner.cpp
    src/DesktopWindow.cpp
    src/ImageUtil.cpp
    src/Json.cpp
    src/QtSettingsWindow.cpp
    src/QtSettingsWindow.h
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
    Qt6::Widgets
)

message(STATUS "musuka: Qt 6 Widgets UI enabled")

if(BUILD_TESTING)
    add_executable(wallpaper_engine_smoke
        tests/WallpaperEngineSmoke.cpp
        src/App.cpp
        src/ConfigStore.cpp
        src/DesktopScanner.cpp
        src/DesktopWindow.cpp
        src/ImageUtil.cpp
        src/Json.cpp
        src/QtSettingsWindow.cpp
        src/QtSettingsWindow.h
        src/Util.cpp
        src/WinUtil.cpp
        src/Musuka.rc
    )
    target_compile_definitions(wallpaper_engine_smoke PRIVATE
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    if(MSVC)
        target_compile_options(wallpaper_engine_smoke PRIVATE /W4 /permissive- /utf-8)
    endif()
    target_include_directories(wallpaper_engine_smoke PRIVATE src)
    target_link_libraries(wallpaper_engine_smoke PRIVATE
        gdiplus
        comctl32
        comdlg32
        shell32
        ole32
        uuid
        shlwapi
        user32
        gdi32
        Qt6::Widgets
    )
    add_test(NAME wallpaper_engine_smoke COMMAND wallpaper_engine_smoke)
endif()

cmake_minimum_required(VERSION 3.20)

project(musuka LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(musuka WIN32
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
)

target_compile_definitions(musuka PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(musuka PRIVATE /W4 /permissive- /utf-8)
endif()

if(MINGW)
    target_link_options(musuka PRIVATE -municode)
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

@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
    echo CMake was not found in PATH.
    echo Install CMake first, then add it to PATH.
    exit /b 1
)

where g++.exe >nul 2>nul
if errorlevel 1 (
    echo g++.exe was not found in PATH.
    echo Install MinGW-w64, then add its bin directory to PATH.
    echo Example: C:\msys64\mingw64\bin
    exit /b 1
)

set "BUILD_DIR=build-mingw"
set "GENERATOR="

where ninja.exe >nul 2>nul
if not errorlevel 1 (
    set "GENERATOR=Ninja"
    echo Using CMake generator: Ninja
    cmake -S . -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release
    if errorlevel 1 exit /b 1

    cmake --build "%BUILD_DIR%" --config Release
    if errorlevel 1 exit /b 1

    echo.
    echo Build complete: %BUILD_DIR%\musuka.exe
    exit /b 0
)

where mingw32-make.exe >nul 2>nul
if errorlevel 1 (
    echo Neither ninja.exe nor mingw32-make.exe was found in PATH.
    echo Install Ninja or make sure MinGW-w64 bin is in PATH.
    exit /b 1
)

set "GENERATOR=MinGW Makefiles"
echo Using CMake generator: MinGW Makefiles
cmake -S . -B "%BUILD_DIR%" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 exit /b 1

echo.
echo Build complete: %BUILD_DIR%\musuka.exe


@echo off
setlocal EnableExtensions
cd /d "%~dp0"

if not defined QT_PREFIX_PATH (
    echo QT_PREFIX_PATH is not set.
    echo Example:
    echo   set QT_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
    echo   build-qt.bat
    exit /b 1
)

if not exist "%QT_PREFIX_PATH%\lib\cmake\Qt6\Qt6Config.cmake" (
    echo Qt 6 was not found at:
    echo   %QT_PREFIX_PATH%
    exit /b 1
)

set "USE_QT=1"
set "BUILD_DIR=build-nmake-qt"
call build.bat
if errorlevel 1 exit /b 1

if not exist "%QT_PREFIX_PATH%\bin\windeployqt.exe" (
    echo windeployqt.exe was not found at:
    echo   %QT_PREFIX_PATH%\bin\windeployqt.exe
    exit /b 1
)

"%QT_PREFIX_PATH%\bin\windeployqt.exe" --release --no-translations "%BUILD_DIR%\musuka.exe"
if errorlevel 1 exit /b 1

echo.
echo Qt runtime deployed. Run: %BUILD_DIR%\musuka.exe

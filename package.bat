@echo off
setlocal EnableExtensions
cd /d "%~dp0"

call build.bat
if errorlevel 1 exit /b 1

if not exist release mkdir release

copy /y build-nmake\musuka.exe release\musuka.exe >nul

if exist default_image (
    if exist release\default_image rmdir /s /q release\default_image
    xcopy /e /i /y default_image release\default_image >nul
) else (
    echo default_image directory does not exist. The release will run without built-in images.
)

copy /y README.md release\README.md >nul

:: --- Qt deployment (windeployqt) ---
:: Detect whether this build linked Qt by checking CMake cache for MUSUKA_USE_QT.
set "USE_QT_DEPLOY=0"
if exist "build-nmake\CMakeCache.txt" (
    findstr /C:"MUSUKA_USE_QT:BOOL=ON" "build-nmake\CMakeCache.txt" >nul 2>nul
    if not errorlevel 1 set "USE_QT_DEPLOY=1"
)

if "%USE_QT_DEPLOY%"=="1" call :DeployQt
if errorlevel 1 exit /b 1
if "%USE_QT_DEPLOY%"=="0" echo [package] Native Win32 build (no Qt deployment needed).

echo.
echo Package complete: release\
exit /b 0

:DeployQt
echo [package] Qt build detected, running windeployqt...
set "WINDEPLOYQT="

if defined QT_PREFIX_PATH if exist "%QT_PREFIX_PATH%\bin\windeployqt.exe" (
    set "WINDEPLOYQT=%QT_PREFIX_PATH%\bin\windeployqt.exe"
)

if not defined WINDEPLOYQT (
    for /f "delims=" %%I in ('where windeployqt.exe 2^>nul') do if not defined WINDEPLOYQT set "WINDEPLOYQT=%%I"
)

if not defined WINDEPLOYQT (
    for /d %%D in ("C:\Qt\6.*\msvc2022_64") do if exist "%%D\bin\windeployqt.exe" set "WINDEPLOYQT=%%D\bin\windeployqt.exe"
)

if not defined WINDEPLOYQT (
    echo [package] ERROR: windeployqt.exe not found.
    echo Set QT_PREFIX_PATH or add the Qt bin directory to PATH.
    exit /b 1
)

echo [package] Using %WINDEPLOYQT%
"%WINDEPLOYQT%" --release --no-translations release\musuka.exe
if errorlevel 1 (
    echo [package] ERROR: windeployqt failed.
    exit /b 1
)

echo [package] Qt DLLs and plugins deployed to release\
exit /b 0

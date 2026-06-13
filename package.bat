@echo off
setlocal EnableExtensions
cd /d "%~dp0"

if exist release (
    rmdir /s /q release
    if exist release (
        echo [package] ERROR: release directory could not be cleared.
        echo Close any running release\musuka.exe process and try again.
        exit /b 1
    )
)

call build.bat
if errorlevel 1 exit /b 1

mkdir release
if errorlevel 1 exit /b 1

copy /y build-nmake\musuka.exe release\musuka.exe >nul
if errorlevel 1 exit /b 1

if exist default_image (
    xcopy /e /i /y default_image release\default_image >nul
    if errorlevel 2 exit /b 1
) else (
    mkdir release\default_image
    if errorlevel 1 exit /b 1
)

if exist data (
    xcopy /e /i /y data release\data >nul
    if errorlevel 2 exit /b 1
) else (
    mkdir release\data
    if errorlevel 1 exit /b 1
)

"%QT_PREFIX_PATH%\bin\windeployqt.exe" --release --no-translations release\musuka.exe
if errorlevel 1 (
    echo [package] ERROR: windeployqt failed.
    exit /b 1
)

for %%F in (dxcompiler.dll dxil.dll concrt140.dll msvcp140.dll msvcp140_1.dll msvcp140_2.dll msvcp140_atomic_wait.dll msvcp140_codecvt_ids.dll vccorlib140.dll vcruntime140.dll vcruntime140_1.dll vcruntime140_threads.dll) do (
    if exist "build-nmake\%%F" copy /y "build-nmake\%%F" "release\%%F" >nul
)

for %%F in (Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll platforms\qwindows.dll msvcp140.dll msvcp140_1.dll msvcp140_2.dll vcruntime140.dll vcruntime140_1.dll) do (
    if not exist "release\%%F" (
        echo [package] ERROR: required Qt runtime is missing: %%F
        exit /b 1
    )
)

echo.
echo Portable Qt package complete: release\
exit /b 0

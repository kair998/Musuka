@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

if not defined BUILD_DIR set "BUILD_DIR=build-nmake"

where cmake.exe >nul 2>nul
if errorlevel 1 (
    echo CMake was not found in PATH.
    echo Install CMake 3.20 or newer and add cmake.exe to PATH.
    exit /b 1
)

call :FindVcVars
if not defined VCVARSALL_PATH (
    echo Visual Studio Build Tools vcvarsall.bat was not found.
    echo Install "Desktop development with C++", or set VCVARSALL to the full vcvarsall.bat path.
    echo Example:
    echo   set VCVARSALL=D:\DevTools\VSBuildTools2022\VC\Auxiliary\Build\vcvarsall.bat
    exit /b 1
)

call "%VCVARSALL_PATH%" x64
if errorlevel 1 exit /b 1

where cl.exe >nul 2>nul
if errorlevel 1 (
    echo cl.exe was not found after loading the MSVC environment.
    exit /b 1
)

where nmake.exe >nul 2>nul
if errorlevel 1 (
    echo nmake.exe was not found after loading the MSVC environment.
    exit /b 1
)

set "CMAKE_EXTRA_ARGS=-DMUSUKA_USE_QT=OFF"
if "%USE_QT%"=="1" (
    set "CMAKE_EXTRA_ARGS=-DMUSUKA_USE_QT=ON"
    if defined QT_PREFIX_PATH (
        set "CMAKE_EXTRA_ARGS=!CMAKE_EXTRA_ARGS! -DCMAKE_PREFIX_PATH=%QT_PREFIX_PATH%"
    )
)

cmake -S . -B "%BUILD_DIR%" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release !CMAKE_EXTRA_ARGS!
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%"
if errorlevel 1 exit /b 1

if exist default_image (
    if exist "%BUILD_DIR%\default_image" rmdir /s /q "%BUILD_DIR%\default_image"
    xcopy /e /i /y default_image "%BUILD_DIR%\default_image" >nul
) else (
    echo default_image directory does not exist. The program will run without built-in images.
)

echo.
echo Build complete: %BUILD_DIR%\musuka.exe
exit /b 0

:FindVcVars
if defined VCVARSALL (
    if exist "%VCVARSALL%" (
        set "VCVARSALL_PATH=%VCVARSALL%"
        exit /b 0
    )
)

if exist "D:\DevTools\VSBuildTools2022\VC\Auxiliary\Build\vcvarsall.bat" (
    set "VCVARSALL_PATH=D:\DevTools\VSBuildTools2022\VC\Auxiliary\Build\vcvarsall.bat"
    exit /b 0
)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        if exist "%%I\VC\Auxiliary\Build\vcvarsall.bat" (
            set "VCVARSALL_PATH=%%I\VC\Auxiliary\Build\vcvarsall.bat"
            exit /b 0
        )
    )
)

for %%P in (
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
    "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Community"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
) do (
    if exist "%%~P\VC\Auxiliary\Build\vcvarsall.bat" (
        set "VCVARSALL_PATH=%%~P\VC\Auxiliary\Build\vcvarsall.bat"
        exit /b 0
    )
)

exit /b 0


@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "BUILD_DIR=build-nmake"

where cmake.exe >nul 2>nul
if errorlevel 1 (
    echo CMake was not found in PATH.
    echo Install CMake 3.20 or newer and add cmake.exe to PATH.
    exit /b 1
)

if not defined QT_PREFIX_PATH (
    echo QT_PREFIX_PATH is not set.
    echo Install Qt 6 for MSVC 2022 64-bit and set its installation path.
    echo Example:
    echo   set QT_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
    exit /b 1
)

if not exist "%QT_PREFIX_PATH%\lib\cmake\Qt6\Qt6Config.cmake" (
    echo Qt 6 was not found at:
    echo   %QT_PREFIX_PATH%
    exit /b 1
)

if not exist "%QT_PREFIX_PATH%\bin\windeployqt.exe" (
    echo windeployqt.exe was not found at:
    echo   %QT_PREFIX_PATH%\bin\windeployqt.exe
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

cmake -U MUSUKA_USE_QT -S . -B "%BUILD_DIR%" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_PREFIX_PATH%"
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%"
if errorlevel 1 exit /b 1

if exist default_image (
    if exist "%BUILD_DIR%\default_image" rmdir /s /q "%BUILD_DIR%\default_image"
    xcopy /e /i /y default_image "%BUILD_DIR%\default_image" >nul
) else (
    echo default_image directory does not exist. The program will run without built-in images.
)

for %%F in (vc_redist.x64.exe opengl32sw.dll d3dcompiler_47.dll dxcompiler.dll dxil.dll Qt6Network.dll Qt6Svg.dll) do (
    if exist "%BUILD_DIR%\%%F" del /q "%BUILD_DIR%\%%F"
)
for %%D in (generic iconengines networkinformation styles tls) do (
    if exist "%BUILD_DIR%\%%D" rmdir /s /q "%BUILD_DIR%\%%D"
)
for %%F in (qgif.dll qicns.dll qico.dll qsvg.dll qtga.dll qtiff.dll qwbmp.dll qwebp.dll) do (
    if exist "%BUILD_DIR%\imageformats\%%F" del /q "%BUILD_DIR%\imageformats\%%F"
)

"%QT_PREFIX_PATH%\bin\windeployqt.exe" --release --no-translations --no-compiler-runtime --no-opengl-sw --no-system-d3d-compiler --no-system-dxc-compiler --skip-plugin-types generic,iconengines,networkinformation,styles,tls --exclude-plugins qgif,qicns,qico,qsvg,qtga,qtiff,qwbmp,qwebp "%BUILD_DIR%\musuka.exe"
if errorlevel 1 exit /b 1

set "VC_RUNTIME_DIR=%VCToolsRedistDir%x64\Microsoft.VC143.CRT"
if not exist "%VC_RUNTIME_DIR%\vcruntime140.dll" (
    echo MSVC x64 runtime DLLs were not found at:
    echo   %VC_RUNTIME_DIR%
    exit /b 1
)
xcopy /y "%VC_RUNTIME_DIR%\*.dll" "%BUILD_DIR%\" >nul
if errorlevel 2 exit /b 1

echo.
echo Qt build complete: %BUILD_DIR%\musuka.exe
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


@echo off
setlocal
cd /d "%~dp0"

call build_mingw.bat
if errorlevel 1 exit /b 1

if exist release-mingw rmdir /s /q release-mingw
mkdir release-mingw

copy /y build-mingw\musuka.exe release-mingw\musuka.exe >nul

set "MINGW_BIN="
for /f "delims=" %%G in ('where g++.exe 2^>nul') do (
    if not defined MINGW_BIN set "MINGW_BIN=%%~dpG"
)

if defined MINGW_BIN (
    for %%D in (
        libgcc_s_seh-1.dll
        libgcc_s_sjlj-1.dll
        libgcc_s_dw2-1.dll
        libstdc++-6.dll
        libwinpthread-1.dll
    ) do (
        if exist "%MINGW_BIN%%%D" copy /y "%MINGW_BIN%%%D" release-mingw\%%D >nul
    )
)

if exist default_image (
    xcopy /e /i /y default_image release-mingw\default_image >nul
) else (
    echo default_image directory does not exist. The release will run without built-in images.
)

copy /y README.md release-mingw\README.md >nul
copy /y build_mingw.bat release-mingw\build_mingw.bat >nul

echo.
echo Package complete: release-mingw\

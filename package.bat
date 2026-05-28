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

echo.
echo Package complete: release\

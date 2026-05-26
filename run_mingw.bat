@echo off
setlocal
cd /d "%~dp0"

if not exist build-mingw\musuka.exe (
    call build_mingw.bat
    if errorlevel 1 exit /b 1
)

start "" build-mingw\musuka.exe


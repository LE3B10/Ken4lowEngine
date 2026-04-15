@echo off
setlocal

set "PROJECT_DIR=%~1"
set "CONFIGURATION=%~2"

if "%PROJECT_DIR%"=="" set "PROJECT_DIR=%~dp0..\.."
for %%I in ("%PROJECT_DIR%") do set "PROJECT_DIR=%%~fI"
if "%PROJECT_DIR:~-1%"=="\" set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"

if "%CONFIGURATION%"=="" set "CONFIGURATION=Debug"

echo [RunBuildFonts] PROJECT_DIR=%PROJECT_DIR%
echo [RunBuildFonts] CONFIGURATION=%CONFIGURATION%

powershell -ExecutionPolicy Bypass -NoProfile -File "%~dp0BuildFonts.ps1" -ProjectDir "%PROJECT_DIR%" -Configuration "%CONFIGURATION%"
exit /b %ERRORLEVEL%
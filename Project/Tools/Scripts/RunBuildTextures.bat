@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "PROJECT_DIR=%%~fI"

echo [RunBuildTextures] SCRIPT_DIR=%SCRIPT_DIR%
echo [RunBuildTextures] PROJECT_DIR=%PROJECT_DIR%

if not exist "%SCRIPT_DIR%BuildTextures.ps1" (
    echo [RunBuildTextures] ERROR: BuildTextures.ps1 not found
    exit /b 1
)

if not exist "%PROJECT_DIR%\Tools\Bin\TextureConverter.exe" (
    echo [RunBuildTextures] ERROR: TextureConverter.exe not found
    exit /b 1
)

"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -ExecutionPolicy Bypass -NoProfile -File "%SCRIPT_DIR%BuildTextures.ps1" -ProjectDir "%PROJECT_DIR%"
set "ERR=%ERRORLEVEL%"

echo [RunBuildTextures] PowerShell ExitCode=%ERR%
exit /b %ERR%
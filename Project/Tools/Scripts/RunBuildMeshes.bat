@echo off
setlocal

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..\..

powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%BuildMeshes.ps1" -ProjectDir "%PROJECT_DIR%" -Config "%Configuration%" -Platform "%Platform%"

if errorlevel 1 (
    echo [RunBuildMeshes] failed.
    exit /b 1
)

exit /b 0
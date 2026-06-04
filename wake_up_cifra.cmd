@echo off
REM CifraSync Launcher - "wake up cifra"
REM This script builds (if needed) and launches CifraSync in interactive mode.

setlocal enabledelayedexpansion
set PROJECT_DIR=%~dp0
cd /d "%PROJECT_DIR%"

REM Check if the binary exists, if not build it
if not exist "bin\cifrasync.exe" (
    echo.
    echo  [CifraSync] Building project...
    mingw32-make release
    if errorlevel 1 (
        echo.
        echo  [ERROR] Build failed. Make sure GCC and make (MSYS2 MinGW-w64) are installed.
        pause
        exit /b 1
    )
    echo  [CifraSync] Build complete.
)

echo.
echo  +-----------------------------------------+
echo  |        C I F R A S Y N C                |
echo  |    Encrypted Incremental Backup ^& Sync  |
echo  +-----------------------------------------+
echo.
echo  Waking up CifraSync...
echo.
bin\cifrasync.exe

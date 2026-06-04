@echo off
cd /d "%~dp0"
if not exist "bin\cifrasync.exe" mingw32-make release >nul 2>&1
bin\cifrasync.exe

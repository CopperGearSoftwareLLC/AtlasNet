@echo off
REM =====================================
REM Run ImguiBase inside WSL
REM =====================================

REM Get current directory (Windows format)
setlocal
set "CURR_DIR=%~dp0"
set "CURR_DIR=%CURR_DIR:~0,-1%"

REM Convert to WSL path (e.g. C:\Projects\Tool -> /mnt/c/Projects/Tool)
for /f "delims=" %%i in ('wsl wslpath "%CURR_DIR%"') do set "WSL_PATH=%%i"

REM Launch inside WSL
wsl --cd "%WSL_PATH%" ./ImguiBase

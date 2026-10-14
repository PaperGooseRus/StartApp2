@echo off
setlocal
cd /d "%~dp0"
if not exist PixelSandbox.exe (
    echo PixelSandbox.exe is missing. Extract the entire archive first.
    pause
    exit /b 1
)
PixelSandbox.exe %*
set "game_result=%errorlevel%"
if not "%game_result%"=="0" (
    echo Game exited with code %game_result%. Keep SDL2.dll beside PixelSandbox.exe.
    pause
)
exit /b %game_result%

@echo off
setlocal
cd /d "%~dp0"
if not exist PixelEditor.exe (
    echo PixelEditor.exe is missing. Extract the entire archive first.
    pause
    exit /b 1
)
PixelEditor.exe %*
set "editor_result=%errorlevel%"
if not "%editor_result%"=="0" (
    echo Editor exited with code %editor_result%. Keep assets and SDL2.dll beside PixelEditor.exe.
    pause
)
exit /b %editor_result%

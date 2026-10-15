@echo off
setlocal
cd /d "%~dp0"
pixel_tests.exe
if errorlevel 1 goto failed
soil_tests.exe
if errorlevel 1 goto failed
scene_tests.exe
if errorlevel 1 goto failed
snapshot_tests.exe
if errorlevel 1 goto failed
studio_tests.exe
if errorlevel 1 goto failed
set "SDL_VIDEODRIVER=dummy"
if exist PixelSandbox.exe (
    PixelSandbox.exe --smoke-test
    if errorlevel 1 goto failed
)
if exist PixelEditor.exe (
    PixelEditor.exe --smoke-test
    if errorlevel 1 goto failed
)
echo.
echo All checks passed. StartEditor.bat opens the IDE; StartGame.bat opens the game.
pause
exit /b 0
:failed
echo.
echo A check failed. Copy the output above when reporting the problem.
pause
exit /b 1

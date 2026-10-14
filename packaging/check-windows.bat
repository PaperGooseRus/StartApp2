@echo off
setlocal
cd /d "%~dp0"
pixel_tests.exe
if errorlevel 1 goto failed
snapshot_tests.exe
if errorlevel 1 goto failed
set "SDL_VIDEODRIVER=dummy"
PixelSandbox.exe --smoke-test
if errorlevel 1 goto failed
echo.
echo All checks passed. You can start the game with StartGame.bat.
pause
exit /b 0
:failed
echo.
echo A check failed. Copy the output above when reporting the problem.
pause
exit /b 1

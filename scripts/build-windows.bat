@echo off
setlocal
cd /d "%~dp0.."
where cmake >nul 2>nul
if errorlevel 1 (
    echo CMake not found. Install CMake and Visual Studio C++ Build Tools; see README.md.
    exit /b 1
)
rem Forward options, e.g. -G Ninja or -DSDL2_DIR=C:/SDK/SDL2/cmake
cmake -S . -B build\windows -DCMAKE_BUILD_TYPE=Release %*
if errorlevel 1 exit /b 1
cmake --build build\windows --config Release --parallel 2
if errorlevel 1 exit /b 1
ctest --test-dir build\windows -C Release --output-on-failure
if errorlevel 1 exit /b 1
cmake --install build\windows --config Release --prefix build\package-windows
if errorlevel 1 exit /b 1
echo Ready: build\package-windows\StartGame.bat

@echo off
rem EH3D-NODE-BUNGARD: builds the host tests of the core. The firmware itself is built with tools\build_node.sh (WSL or Linux, Docker).
rem   build.bat            runs the host tests
setlocal
cd /d "%~dp0"
cmake -S tests -B build\host
if errorlevel 1 goto :end
cmake --build build\host --config Debug
if errorlevel 1 goto :end
ctest --test-dir build\host -C Debug --output-on-failure
:end
if not defined NOPAUSE pause
endlocal

@echo off
rem S7 probe build (Release, Ninja, TESTS=ON, APP=ON, local D113-patched JUCE). Reproducible recipe.
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set VSLANG=1033
chcp 65001 >nul
cmake -S D:\CORE\CORE -B D:\Core-build\s7\build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCORE_BUILD_TESTS=ON -DCORE_BUILD_APP=ON -DFETCHCONTENT_SOURCE_DIR_JUCE=D:/Core-deps/JUCE-9.0.5
if errorlevel 1 exit /b 1
cmake --build D:\Core-build\s7\build --parallel 6

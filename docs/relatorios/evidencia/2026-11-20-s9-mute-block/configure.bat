@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set VSLANG=1033
chcp 65001 >nul
cmake -S D:\CORE\CORE -B D:\Core-build\s9\build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCORE_BUILD_TESTS=ON -DCORE_BUILD_APP=ON -DFETCHCONTENT_SOURCE_DIR_JUCE=D:/Core-deps/JUCE-9.0.5

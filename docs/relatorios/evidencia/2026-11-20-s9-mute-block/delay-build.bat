@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set VSLANG=1033
chcp 65001 >nul
cmake -S D:\Core-build\s9\delay-proj -B D:\Core-build\s9\delay-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build D:\Core-build\s9\delay-build --parallel 6

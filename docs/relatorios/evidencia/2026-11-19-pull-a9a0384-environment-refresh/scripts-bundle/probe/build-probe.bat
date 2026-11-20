@echo off
set VSLANG=1033
chcp 65001 >nul
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cl /nologo /MTd /Zi /EHsc /utf-8 /W3 /I "D:\CORE\CORE\engine\src" "D:\Core-build\core-review\probe\stack_probe.cpp" /Fo"D:\Core-build\core-review\probe\stack_probe.obj" /Fe:"D:\Core-build\core-review\probe\stack_probe.exe" /link /DEBUG "D:\Core-build\core-review\debug\engine\core_engine_lib.lib" dbghelp.lib
echo COMPILE_EXIT=%ERRORLEVEL%
"D:\Core-build\core-review\probe\stack_probe.exe"
echo RUN_EXIT=%ERRORLEVEL%
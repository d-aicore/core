# S11 clean-build notes (coordinator round 2)

Task: prove what a CLEAN Windows build does today — FetchContent downloads pristine JUCE 9.0.5
(no patch applied by the repo) while the host calls markCrashedByHost(); expect a COMPILE FAILURE
in juce_host.cpp. Repo D:\CORE\CORE @ a9a0384 kept READ-ONLY (never edited). D:\Core-deps untouched.

## Step 1 — Configure (2026-11-20 00:20 UTC)

- Bat: %TEMP%\s11-clean-0a48bde5c9ef43f49293704009657473.bat
  (@echo off / set VSLANG=1033 / chcp 65001 / call vcvars64.bat VS18 BuildTools /
  cmake -S D:\CORE\CORE -B D:\Core-build\s11-clean\build -G Ninja -DCMAKE_BUILD_TYPE=Debug
  -DCORE_BUILD_TESTS=ON -DFETCHCONTENT_BASE_DIR=D:/Core-build/s11-clean/deps)
- NO FETCHCONTENT_SOURCE_DIR_JUCE was set — JUCE was DOWNLOADED, not taken from D:\Core-deps.
- **rc = 0**. Output: D:\Core-build\s11-clean\configure.txt (34 lines).
- Toolchain: MSVC 19.51.36260.0 (cl 14.51.36231, VS18 BuildTools), CMake 4.4.4, Ninja.
  Configuring done (29.3s), Generating done (0.2s). juceaide configured/built/exported/tested OK.
- Only warning: CMP0135 DOWNLOAD_EXTRACT_TIMESTAMP author warning at CMakeLists.txt:64 (benign).
- Download evidence (deps\juce-subbuild\juce-populate-prefix\src\juce-populate-stamp\juce-populate-urlinfo.txt):
  method=url, url=https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.5.tar.gz
  Tarball: ...\juce-populate-prefix\src\9.0.5.tar.gz, 24,944,491 bytes (~24.9 MB).
- **URL_HASH check PASSED**: success stamp verify-juce-populate.cmake (0 bytes) present, and
  independent Get-FileHash of the tarball:
  SHA256=d92207f51ba728d3d195b6d5e93ecd049717a915fc250ff6844c32ea7272315a
  == pinned URL_HASH SHA256 in D:\CORE\CORE\CMakeLists.txt:66. MATCH=True.
- Extracted JUCE dir: **D:\Core-build\s11-clean\deps\juce-src**
  Version **9.0.5**: juce-src\CMakeLists.txt:37 `project(JUCE VERSION 9.0.5 LANGUAGES C CXX)`;
  modules\juce_core\system\juce_StandardHeader.h:42-44 JUCE_MAJOR_VERSION 9 / JUCE_MINOR_VERSION 0 / JUCE_BUILDNUMBER 5.

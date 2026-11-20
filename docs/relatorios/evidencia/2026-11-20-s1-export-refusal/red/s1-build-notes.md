# S1 build notes (s1-red)
Repo SHA: a9a038430d781959829f8510067ce59afffaad62

## Step 1 configure
- rc: 0
- bat: C:\Users\vk_do\AppData\Local\Temp\s1cfg-9122720bf8684eb8bc865d9f3e80109c.bat
- log: D:\Core-build\s1-red\configure.txt
- tail:
-- Testing juceaide
-- Finished setting up juceaide
-- Configuring done (24.9s)
-- Generating done (0.2s)
-- Build files have been written to: D:/Core-build/s1-red/build

## Step 2 build
- rc: 0
- bat: C:\Users\vk_do\AppData\Local\Temp\s1bld-698202729dd04c9c90aed4ed486f35b3.bat
- log: D:\Core-build\s1-red\build.txt
- tail:
[550/555] Building CXX object tests\CMakeFiles\engine_process_tests.dir\engine_process_tests.cpp.obj
[551/555] Linking CXX executable tests\window_tests.exe
[552/555] Linking CXX executable tests\hunt_tests.exe
[553/555] Linking CXX executable tests\calls_tests.exe
[554/555] Linking CXX executable tests\engine_process_tests.exe

## Step 3 verification
- tests/engine_tests.exe exists: True
- VST3 artefacts:
  - D:\Core-build\s1-red\build\app\selftest-plugins\CoreTestCrashRestart_artefacts\Debug\VST3\Core Test Crash Restart.vst3
  - D:\Core-build\s1-red\build\app\selftest-plugins\CoreTestCrashRestart_artefacts\Debug\VST3\Core Test Crash Restart.vst3\Contents\x86_64-win\Core Test Crash Restart.vst3
  - D:\Core-build\s1-red\build\app\selftest-plugins\CoreTestCrash_artefacts\Debug\VST3\Core Test Crash.vst3
  - D:\Core-build\s1-red\build\app\selftest-plugins\CoreTestCrash_artefacts\Debug\VST3\Core Test Crash.vst3\Contents\x86_64-win\Core Test Crash.vst3
  - D:\Core-build\s1-red\build\app\selftest-plugins\CoreTestFault_artefacts\Debug\VST3\Core Test Fault.vst3
  - D:\Core-build\s1-red\build\app\selftest-plugins\CoreTestFault_artefacts\Debug\VST3\Core Test Fault.vst3\Contents\x86_64-win\Core Test Fault.vst3
  - D:\Core-build\s1-red\build\app\selftest-plugins\CoreTestGain_artefacts\Debug\VST3\Core Test Gain.vst3
  - D:\Core-build\s1-red\build\app\selftest-plugins\CoreTestGain_artefacts\Debug\VST3\Core Test Gain.vst3\Contents\x86_64-win\Core Test Gain.vst3
  - D:\Core-build\s1-red\build\app\selftest-plugins\CoreTestSynth_artefacts\Debug\VST3\Core Test Synth.vst3
  - D:\Core-build\s1-red\build\app\selftest-plugins\CoreTestSynth_artefacts\Debug\VST3\Core Test Synth.vst3\Contents\x86_64-win\Core Test Synth.vst3
  - D:\Core-build\s1-red\build\app\selftest-plugins\CoreTestThrow_artefacts\Debug\VST3\Core Test Throw.vst3
  - D:\Core-build\s1-red\build\app\selftest-plugins\CoreTestThrow_artefacts\Debug\VST3\Core Test Throw.vst3\Contents\x86_64-win\Core Test Throw.vst3
- ctest -N -L merge output:
```nTest project D:/Core-build/s1-red/build
  Test  #1: engine_tests
  Test  #2: domain_tests
  Test  #3: modules_tests
  Test  #4: role_tests
  Test  #5: midi_tests
  Test  #6: bug_tests
  Test  #7: bitwig_tests
  Test  #8: audit_tests
  Test  #9: audit2_tests
  Test #10: hunt_tests
  Test #11: round3_tests
  Test #12: calls_tests
  Test #13: menu_tests
  Test #14: window_tests
  Test #15: cli_version
  Test #16: cli_demo_render
  Test #17: cli_measure
  Test #18: cli_unicode
  Test #19: cli_package_stale
  Test #20: engine_process_tests
  Test #21: cli_plugin_crash_render

Total Tests: 21
```

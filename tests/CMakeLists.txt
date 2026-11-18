# Core's tests (2026-11-16). Two levels:
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#   label "merge":   fast, every build and before every merge (ctest -L merge)
#   label "desktop": opens Core's window (the app's --self-test); only where the app is built
# Signals are generated in the tests; no audio fixtures and no bit-exact goldens tied to one compiler.


add_executable(engine_tests engine_tests.cpp ${CMAKE_SOURCE_DIR}/engine/src/app/measure.cpp ${CMAKE_SOURCE_DIR}/engine/src/io/device.cpp)
target_include_directories(engine_tests PRIVATE ${CMAKE_SOURCE_DIR}/engine/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(engine_tests PRIVATE core_engine_lib miniaudio_impl)
if(WIN32)
  target_link_libraries(engine_tests PRIVATE dbghelp)
endif()
add_test(NAME engine_tests COMMAND engine_tests)

add_executable(domain_tests domain_tests.cpp)
target_include_directories(domain_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(domain_tests PRIVATE core_domain core_engine_lib)
target_compile_definitions(domain_tests PRIVATE CORE_TEMPLATES_DIR="${CMAKE_SOURCE_DIR}/templates")
add_test(NAME domain_tests COMMAND domain_tests)

# Characterization guards for the L1/L2 dead-code removal (2026-11-18): the live legacy behavior
# (old songs, stored built-in data, the five visible templates, legacy engine renders) stays green.
add_executable(legacy_guard_tests legacy_guard_tests.cpp)
target_include_directories(legacy_guard_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(legacy_guard_tests PRIVATE core_domain core_engine_lib)
target_compile_definitions(legacy_guard_tests PRIVATE CORE_TEMPLATES_DIR="${CMAKE_SOURCE_DIR}/templates" CORE_PCORE_CATALOG="${CMAKE_SOURCE_DIR}/pcore/styles.json")
add_test(NAME legacy_guard_tests COMMAND legacy_guard_tests)

# The role map (Settings > Roles) and the tools that play through it (SUCK, PUM, build-up effects).
add_executable(role_tests role_tests.cpp)
target_include_directories(role_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(role_tests PRIVATE core_domain core_engine_lib)
add_test(NAME role_tests COMMAND role_tests)

# MIDI files in and out (export with the rolls expanded as they play; import into tracks and patterns).
add_executable(midi_tests midi_tests.cpp)
target_include_directories(midi_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(midi_tests PRIVATE core_domain core_engine_lib)
add_test(NAME midi_tests COMMAND midi_tests)

# The medium and small bugs fixed from the reviewed sweep (2026-11-16): each failed before its fix.
add_executable(bug_tests bug_tests.cpp)
target_include_directories(bug_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bug_tests PRIVATE core_domain core_engine_lib)
target_compile_definitions(bug_tests PRIVATE CORE_LANG_DIR="${CMAKE_SOURCE_DIR}/lang")
add_test(NAME bug_tests COMMAND bug_tests)

# The export for the Bitwig: the .mid and the .vstpreset of each plugin, and which goes on which track.
add_executable(bitwig_tests bitwig_tests.cpp)
target_include_directories(bitwig_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bitwig_tests PRIVATE core_domain core_engine_lib)
add_test(NAME bitwig_tests COMMAND bitwig_tests)

# The bugs of the external audit (2026-11-16, RELATORIO-BUGS-MELHORIAS.md): each failed before its fix.
add_executable(audit_tests audit_tests.cpp)
target_include_directories(audit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/src)
target_link_libraries(audit_tests PRIVATE core_domain core_engine_lib)
add_test(NAME audit_tests COMMAND audit_tests)

# The second external audit (2026-11-16, code sweep): engine-client lines, sync retry, export disk space, autosave.
add_executable(audit2_tests audit2_tests.cpp)
target_include_directories(audit2_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/src ${CMAKE_SOURCE_DIR}/app/engine-client/src)
target_link_libraries(audit2_tests PRIVATE core_domain core_engine_lib)
add_test(NAME audit2_tests COMMAND audit2_tests)

# The deep bug hunt (2026-11-16, RELATORIO-CAÇA-PROFUNDA.md, P01-P06): each case of the report.
add_executable(hunt_tests hunt_tests.cpp)
target_include_directories(hunt_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/src ${CMAKE_SOURCE_DIR}/app/engine-client/src)
target_link_libraries(hunt_tests PRIVATE core_domain core_engine_lib core_previews core_sampling)
target_compile_definitions(hunt_tests PRIVATE CORE_TEMPLATES_DIR="${CMAKE_SOURCE_DIR}/templates" CORE_LANG_DIR="${CMAKE_SOURCE_DIR}/lang")
add_test(NAME hunt_tests COMMAND hunt_tests)

# Ronda 3 do audit (R3-A a R3-G).
add_executable(round3_tests round3_tests.cpp)
target_include_directories(round3_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(round3_tests PRIVATE core_domain core_engine_lib)
target_compile_definitions(round3_tests PRIVATE CORE_LANG_DIR="${CMAKE_SOURCE_DIR}/lang")
add_test(NAME round3_tests COMMAND round3_tests)

# O control: as propostas de chamadas do AICORE vistas pelo Core (o "on" e a "section" na canção, o que é recusado).
add_executable(calls_tests calls_tests.cpp)
target_include_directories(calls_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(calls_tests PRIVATE core_control core_domain)
add_test(NAME calls_tests COMMAND calls_tests)

# The command line: what the app and the scripts call.
add_test(NAME cli_version COMMAND core-engine --version)
set_tests_properties(cli_version PROPERTIES PASS_REGULAR_EXPRESSION "core-engine ${CORE_VERSION}")
add_test(NAME cli_demo_render COMMAND ${CMAKE_COMMAND} -DENGINE=$<TARGET_FILE:core-engine> -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli
                                      -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_demo_render.cmake)
add_test(NAME cli_measure COMMAND ${CMAKE_COMMAND} -DENGINE=$<TARGET_FILE:core-engine> -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli
                                  -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_measure.cmake)

add_test(NAME cli_unicode COMMAND ${CMAKE_COMMAND} -DENGINE=$<TARGET_FILE:core-engine> -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli
                                  -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_unicode.cmake)

# The development zip (BUG-SWEEP-14): a file removed from the repo must not survive in package/Core or
# Core-windows-x64.zip. Runs the real core-package target, so it is registered only where the app is
# built (the Windows gate build). RUN_SERIAL: it repacks the build's shared package folder and zip.
if(TARGET core-package)
  add_test(NAME cli_package_stale COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR} -DSRC=${CMAKE_SOURCE_DIR}
                                          -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_package_stale.cmake)
  set_tests_properties(cli_package_stale PROPERTIES LABELS merge TIMEOUT 300 RUN_SERIAL TRUE)
endif()

set_tests_properties(engine_tests domain_tests role_tests midi_tests bug_tests audit_tests audit2_tests hunt_tests round3_tests calls_tests bitwig_tests legacy_guard_tests cli_version cli_demo_render cli_measure cli_unicode
                     PROPERTIES LABELS merge TIMEOUT 300)

# The engine process driven through the app's client (core-engine serve, null audio backend): messages larger than
# 8 MB both ways, a message over the limit. Windows only, as the client is.
if(TARGET core_engine_client)
  add_executable(engine_process_tests engine_process_tests.cpp)
  target_include_directories(engine_process_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(engine_process_tests PRIVATE core_engine_client)
  target_compile_definitions(engine_process_tests PRIVATE CORE_ENGINE_EXE="$<TARGET_FILE:core-engine>")
  add_dependencies(engine_process_tests core-engine)
  add_test(NAME engine_process_tests COMMAND engine_process_tests)
  set_tests_properties(engine_process_tests PROPERTIES LABELS merge TIMEOUT 120)
endif()

# A plugin that crashes in processBlock (app/selftest-plugins, CORE_TEST_CRASH) must not hang the engine: the offline
# render ends, and the served engine goes on answering and exits. These need the test plugins, which are built with
# JUCE (CORE_BUILD_APP on Windows, as the gate builds): without them the render test is not registered and
# engine_process_tests is built without its crash case.
if(TARGET CoreTestCrash_VST3 AND TARGET CoreTestSynth_VST3 AND TARGET engine_process_tests)
  set(CORE_TEST_CRASH_VST3 "$<GENEX_EVAL:$<TARGET_PROPERTY:CoreTestCrash_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")
  set(CORE_TEST_SYNTH_VST3 "$<GENEX_EVAL:$<TARGET_PROPERTY:CoreTestSynth_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")
  add_test(NAME cli_plugin_crash_render COMMAND ${CMAKE_COMMAND} -DENGINE=$<TARGET_FILE:core-engine> -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cli
                                                -DCRASH_PLUGIN=${CORE_TEST_CRASH_VST3} -DSYNTH_PLUGIN=${CORE_TEST_SYNTH_VST3}
                                                -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_plugin_crash_render.cmake)
  target_compile_definitions(engine_process_tests PRIVATE
    CORE_TEST_CRASH_VST3="${CORE_TEST_CRASH_VST3}"
    CORE_TEST_SYNTH_VST3="${CORE_TEST_SYNTH_VST3}")
  add_dependencies(engine_process_tests CoreTestCrash_VST3 CoreTestSynth_VST3)
  set_tests_properties(cli_plugin_crash_render PROPERTIES LABELS merge TIMEOUT 120)
endif()

# The app's own end-to-end test (play, undo, save/reopen, export... on the null device, with its test plugins).
# It opens Core's window for ~20 s, so it has its own label.
if(TARGET CoreApp)
  add_test(NAME desktop_self_test COMMAND CoreApp --self-test ${CMAKE_CURRENT_BINARY_DIR}/self-test.png)
  set_tests_properties(desktop_self_test PROPERTIES LABELS desktop TIMEOUT 600)
endif()

# Canaries: run before the suite in a sanitized build; each must trip its sanitizer, otherwise the
# suite's green means nothing. Kept out of ctest on purpose (they are expected to fail).
if(CORE_SANITIZE MATCHES "address")
  add_executable(asan_canary canary.cpp)
  target_compile_definitions(asan_canary PRIVATE CORE_CANARY_ASAN=1)
endif()
if(CORE_SANITIZE MATCHES "undefined" AND NOT MSVC)
  add_executable(ubsan_canary canary.cpp)
  target_compile_definitions(ubsan_canary PRIVATE CORE_CANARY_UBSAN=1)
endif()
if(CORE_SANITIZE MATCHES "thread")
  add_executable(tsan_canary canary.cpp)
  target_compile_definitions(tsan_canary PRIVATE CORE_CANARY_TSAN=1)
endif()

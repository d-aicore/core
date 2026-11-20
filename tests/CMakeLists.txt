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

# S4 (DATA priority): invalid float WAV samples (NaN, +/-Inf, float64 values that overflow the float
# cast) must be refused at import with a message naming the file, frame and kind, and nothing may be
# registered in the AssetStore (red measured at a9a0384). Fixtures are generated in the test; valid
# files must import bit-identical. Runs in Debug AND Release: engine/CMakeLists.txt applies /fp:fast
# to every MSVC config, so the validation is integer bit-pattern based and this suite guards both.
add_executable(wav_float_tests wav_float_tests.cpp)
target_include_directories(wav_float_tests PRIVATE ${CMAKE_SOURCE_DIR}/engine/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wav_float_tests PRIVATE core_engine_lib)
add_test(NAME wav_float_tests COMMAND wav_float_tests)
set_tests_properties(wav_float_tests PROPERTIES LABELS merge TIMEOUT 300)

add_executable(domain_tests domain_tests.cpp)
target_include_directories(domain_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(domain_tests PRIVATE core_domain core_engine_lib)
add_test(NAME domain_tests COMMAND domain_tests)

# The legacy module cleanup (2026-11-18, PROD-02/04/06): the Tools cards lose the W/D/F/R-Core titles, the old
# ids and titles stay lookup aliases, legacy buses are reused, and the bridge's "open" takes the old ids.
add_executable(modules_tests modules_tests.cpp)
target_include_directories(modules_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(modules_tests PRIVATE core_control core_domain)
add_test(NAME modules_tests COMMAND modules_tests)

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
target_include_directories(bug_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/app/desktop/src)
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

# S1 (DATA priority): an export whose needed plugin failed to load or failed during the render is refused
# before publish (plugin/track/why named, previous WAV byte-identical); bypass is not failure. In-process
# offline path (RenderJobs + renderProject, fake providers); the real VST3 SEH crash is covered by
# cli_plugin_crash_render. Red measured at a9a0384 (coordinator-round2/s1-repro-notes.md).
add_executable(export_plugin_failure_tests export_plugin_failure_tests.cpp)
target_include_directories(export_plugin_failure_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/src)
target_link_libraries(export_plugin_failure_tests PRIVATE core_engine_lib)
add_test(NAME export_plugin_failure_tests COMMAND export_plugin_failure_tests)

# The deep bug hunt (2026-11-16, RELATORIO-CAÇA-PROFUNDA.md, P01-P06): each case of the report.
add_executable(hunt_tests hunt_tests.cpp)
target_include_directories(hunt_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/engine/src ${CMAKE_SOURCE_DIR}/app/engine-client/src)
target_link_libraries(hunt_tests PRIVATE core_domain core_engine_lib)
target_compile_definitions(hunt_tests PRIVATE CORE_LANG_DIR="${CMAKE_SOURCE_DIR}/lang")
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

# BUG-SWEEP-32 (2026-11-18): the plugin catalog menu ids must never collide with the fixed action ids,
# at any catalog size (the routing extracted from MainComponent.cpp's two menus into MenuIds.h).
add_executable(menu_tests menu_tests.cpp)
target_include_directories(menu_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/app/desktop/src)
add_test(NAME menu_tests COMMAND menu_tests)

# The detachable windows' layout rules (UX detachable-windows slice, 2026-11-18): the settings form, the
# monitor validation on restore and the "bring windows back to screen" cascade. Pure logic, no JUCE.
add_executable(window_tests window_tests.cpp ${CMAKE_SOURCE_DIR}/app/desktop/src/WindowLayout.cpp)
target_include_directories(window_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/app/desktop/src)
target_link_libraries(window_tests PRIVATE core_domain)  # nlohmann/json.hpp (engine/third_party)
add_test(NAME window_tests COMMAND window_tests)

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

# S5 (DATA priority): the installer must never overwrite user-edited/user-owned samples at distributed
# paths, and uninstall must never delete user content through samples.txt (red measured by execution
# at a9a0384). Drives the real Core Setup.exe silent flow with synthetic fixtures in a temp dir under
# the build tree; registered only where the installer is built (WIN32 + CORE_BUILD_APP). RUN_SERIAL:
# it runs the one installer exe through sequential install/reinstall/uninstall steps.
if(WIN32 AND CORE_BUILD_APP AND TARGET CoreSetup)
  add_test(NAME installer_samples COMMAND ${CMAKE_COMMAND} -DSETUP=$<TARGET_FILE:CoreSetup>
                                          -DWORK=${CMAKE_CURRENT_BINARY_DIR}/installer-samples
                                          -P ${CMAKE_CURRENT_SOURCE_DIR}/installer_samples.cmake)
  set_tests_properties(installer_samples PROPERTIES LABELS merge TIMEOUT 300 RUN_SERIAL TRUE)
endif()

set_tests_properties(engine_tests domain_tests role_tests midi_tests bug_tests audit_tests audit2_tests export_plugin_failure_tests hunt_tests round3_tests calls_tests bitwig_tests menu_tests modules_tests window_tests cli_version cli_demo_render cli_measure cli_unicode
                     PROPERTIES LABELS merge TIMEOUT 300)

# The engine process driven through the app's client (core-engine serve, null audio backend): messages larger than
# 8 MB both ways, a message over the limit. Windows only, as the client is.
if(TARGET core_engine_client)
  add_executable(engine_process_tests engine_process_tests.cpp)
  target_include_directories(engine_process_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(engine_process_tests PRIVATE core_engine_client core_domain)
  target_compile_definitions(engine_process_tests PRIVATE CORE_ENGINE_EXE="$<TARGET_FILE:core-engine>")
  add_dependencies(engine_process_tests core-engine)
  add_test(NAME engine_process_tests COMMAND engine_process_tests)
  set_tests_properties(engine_process_tests PROPERTIES LABELS merge TIMEOUT 120)
endif()

# A plugin that crashes in processBlock (app/selftest-plugins, CORE_TEST_CRASH) must not hang the engine, and
# (S1, DATA priority) an export whose needed plugin crashed or could not load is REFUSED before publish:
# cli_plugin_crash_render drives the real CLI through the five refusal cases (previous WAV byte-identical,
# stems whole set, bypass is not failure) with a healthy green control. These need the test plugins, which are
# built with JUCE (CORE_BUILD_APP on Windows, as the gate builds): without them the render test is not
# registered and engine_process_tests is built without its crash case.
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

# The fault plugin (CORE_TEST_FAULT): crashes on demand outside processBlock (state, reset, editor) or in a
# parameter getter, so the host's guards around those entry points are tested. Built with the app, as above.
if(TARGET CoreTestFault_VST3 AND TARGET CoreTestSynth_VST3 AND TARGET engine_process_tests)
  set(CORE_TEST_FAULT_VST3 "$<GENEX_EVAL:$<TARGET_PROPERTY:CoreTestFault_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")
  target_compile_definitions(engine_process_tests PRIVATE CORE_TEST_FAULT_VST3="${CORE_TEST_FAULT_VST3}")
  add_dependencies(engine_process_tests CoreTestFault_VST3)
endif()

# The throwing effect (CORE_TEST_THROW): a plugin that fails while playing must be retryable into a new instance
# that plays (BUG-SWEEP-05). Built with the app, as above.
if(TARGET CoreTestThrow_VST3 AND TARGET CoreTestSynth_VST3 AND TARGET engine_process_tests)
  set(CORE_TEST_THROW_VST3 "$<GENEX_EVAL:$<TARGET_PROPERTY:CoreTestThrow_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")
  target_compile_definitions(engine_process_tests PRIVATE CORE_TEST_THROW_VST3="${CORE_TEST_THROW_VST3}")
  add_dependencies(engine_process_tests CoreTestThrow_VST3)
endif()

# The crash-restart plugin (CORE_TEST_CRASH_RESTART, D restart deadlock): asks the host to restart the component and
# then crashes in processBlock, so the vendored JUCE gate + Core's markCrashedByHost() wiring are tested. The synth
# definition comes from the CoreTestCrash block above (both are built with the app). Built with the app, as above.
if(TARGET CoreTestCrashRestart_VST3 AND TARGET CoreTestSynth_VST3 AND TARGET engine_process_tests)
  set(CORE_TEST_CRASH_RESTART_VST3 "$<GENEX_EVAL:$<TARGET_PROPERTY:CoreTestCrashRestart_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")
  target_compile_definitions(engine_process_tests PRIVATE CORE_TEST_CRASH_RESTART_VST3="${CORE_TEST_CRASH_RESTART_VST3}")
  add_dependencies(engine_process_tests CoreTestCrashRestart_VST3)
endif()

# S6 (engine RT proofs): the M5 live multicore path must be BIT-EXACT with the serial path (quality bar 4),
# measured with the deterministic test plugins through the real VST3 host: in-process Server + JuceHost +
# null device (the serve architecture), the existing Engine::startCapture seam (it copies the output AFTER
# the live-worker mix), and RtWorkerPool::workerJobs() as the parallel-usage proof (workers really ran, not
# just configured). M5 stays OFF by default (D51): every parallel run opts in explicitly, RAII env guard.
# The reset watchdog's 2 s bound is measured with timing evidence in engine_process_tests (Fault seams).
if(TARGET CoreTestSynth_VST3 AND TARGET CoreTestGain_VST3 AND TARGET core_plugin_host)
  add_executable(m5_bitexact_tests m5_bitexact_tests.cpp ${CMAKE_SOURCE_DIR}/engine/src/io/device.cpp)
  target_include_directories(m5_bitexact_tests PRIVATE ${CMAKE_SOURCE_DIR}/engine/src ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(m5_bitexact_tests PRIVATE core_plugin_host core_engine_lib miniaudio_impl)
  target_compile_definitions(m5_bitexact_tests PRIVATE
    CORE_TEST_SYNTH_VST3="$<GENEX_EVAL:$<TARGET_PROPERTY:CoreTestSynth_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
    CORE_TEST_GAIN_VST3="$<GENEX_EVAL:$<TARGET_PROPERTY:CoreTestGain_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
    JUCE_MODAL_LOOPS_PERMITTED=1)
  add_dependencies(m5_bitexact_tests CoreTestSynth_VST3 CoreTestGain_VST3)
  if(WIN32)
    target_link_libraries(m5_bitexact_tests PRIVATE dbghelp)
  endif()
  add_test(NAME m5_bitexact_tests COMMAND m5_bitexact_tests)
  set_tests_properties(m5_bitexact_tests PROPERTIES LABELS merge TIMEOUT 600)
endif()

# S7 (engine RT measurement, owner round-1 item 6): quality bar 3 proven by TOOLING — alloc/lock/IO
# detection in the audio callback (global operator new/delete overrides + IAT hooks on the exe's
# kernel32/kernelbase/api-set imports: file IO, CriticalSection/SRWLOCK, blocking waits, HeapAlloc family
# = the static CRT's malloc coverage), one test-only seam in device.cpp (core::rtProbe: in-callback window
# + thread attribution + directed-fault inject + exact per-block timing). The negative controls MUST fire
# (allocation, CreateFileW/ReadFile, EnterCriticalSection inside the callback); the clean engine path
# (serial + M5(4), real VST3 test plugins, null backend) must record ZERO audio-thread violations.
# --baseline mode: Release real-device load ramp (Phase B), same instrumentation. D51 default untouched.
if(TARGET CoreTestSynth_VST3 AND TARGET CoreTestGain_VST3 AND TARGET core_plugin_host)
  add_executable(s7_rt_probe_tests s7_rt_probe_tests.cpp ${CMAKE_SOURCE_DIR}/engine/src/io/device.cpp)
  target_include_directories(s7_rt_probe_tests PRIVATE ${CMAKE_SOURCE_DIR}/engine/src ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(s7_rt_probe_tests PRIVATE core_plugin_host core_engine_lib miniaudio_impl)
  target_compile_definitions(s7_rt_probe_tests PRIVATE
    CORE_TEST_SYNTH_VST3="$<GENEX_EVAL:$<TARGET_PROPERTY:CoreTestSynth_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
    CORE_TEST_GAIN_VST3="$<GENEX_EVAL:$<TARGET_PROPERTY:CoreTestGain_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
    JUCE_MODAL_LOOPS_PERMITTED=1)
  add_dependencies(s7_rt_probe_tests CoreTestSynth_VST3 CoreTestGain_VST3)
  if(WIN32)
    target_link_libraries(s7_rt_probe_tests PRIVATE dbghelp)
    # The probe symbolizes violation stacks with dbghelp: it needs its own PDB (test target only;
    # /DEBUG adds debug info, it changes no code). Without it every stack is raw addresses.
    target_link_options(s7_rt_probe_tests PRIVATE /DEBUG)
  endif()
  add_test(NAME s7_rt_probe_tests COMMAND s7_rt_probe_tests)
  set_tests_properties(s7_rt_probe_tests PROPERTIES LABELS merge TIMEOUT 600)
  # The STRICT zero-lock gate: quality bar 3 as written, measured RED at a9a0384 + round-1 (JUCE
  # MidiEventList's Array<Vst::Event, CriticalSection> locks on the audio thread / M5 workers). Kept out
  # of merge on purpose — the ENV-03 precedent: a known measured failure is excluded explicitly and run
  # separately, never a silent pass; it goes green only by an authorized fix or an explicit owner
  # acceptance (owner-decision gate; see the S7 report). Nothing is relaxed: the full criterion lives here.
  add_test(NAME s7_rt_strict_tests COMMAND s7_rt_probe_tests "S7 STRICT")
  set_tests_properties(s7_rt_strict_tests PROPERTIES LABELS rt_strict TIMEOUT 600 ENVIRONMENT "CORE_S7_STRICT=1")
endif()

# S9 (mute CPU + fixed-128 block, owner round-2 «Restantes etapas», MEASUREMENT-FIRST): two separate
# problems characterized, not fixed. (A) mute is a fader-only gate (graph.cpp:1329-1331): muted tracks pay
# full instrument+insert CPU — measured per track via engine.cpu + the S7 seam's exact per-block timing,
# serial AND M5(4), null backend; the non-zero muted-track CPU pins are the DEFECT itself (CSV-09), to be
# updated consciously when an authorized suspend change lands. (B) the live plugin block is compile-time
# 128 (juce_host.cpp:23, export-only 256/512 knob): device-block sweep 64/128/256/512 with latency
# invariance. (C) the four suspend preconditions: tails (mute = ramped silence, tail computed not heard),
# sidechain (M7c key path dormant for plugins, pre-mute capture by design), latency (PDC invariant under
# mute rebuilds), click-free resume (capture seam: ramped, bit-exact warm steady state vs reference).
# --measure/--covers modes write the JSON/raw evidence; the delay-tail probe uses an OUT-OF-REPO
# CoreTestDelay build (no repo target). Additive block; the six plugin registration blocks above preserved.
if(TARGET CoreTestSynth_VST3 AND TARGET CoreTestGain_VST3 AND TARGET core_plugin_host)
  add_executable(s9_mute_block_tests s9_mute_block_tests.cpp ${CMAKE_SOURCE_DIR}/engine/src/io/device.cpp)
  target_include_directories(s9_mute_block_tests PRIVATE ${CMAKE_SOURCE_DIR}/engine/src ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(s9_mute_block_tests PRIVATE core_plugin_host core_engine_lib miniaudio_impl)
  target_compile_definitions(s9_mute_block_tests PRIVATE
    CORE_TEST_SYNTH_VST3="$<GENEX_EVAL:$<TARGET_PROPERTY:CoreTestSynth_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
    CORE_TEST_GAIN_VST3="$<GENEX_EVAL:$<TARGET_PROPERTY:CoreTestGain_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
    JUCE_MODAL_LOOPS_PERMITTED=1)
  add_dependencies(s9_mute_block_tests CoreTestSynth_VST3 CoreTestGain_VST3)
  add_test(NAME s9_mute_block_tests COMMAND s9_mute_block_tests)
  set_tests_properties(s9_mute_block_tests PROPERTIES LABELS merge TIMEOUT 600)
endif()

# STAGE 2 timing / latency-compensation / sound-integrity PROOF (owner round-2, gates Chain parallel).
# Additive block; the seven plugin registration blocks above are preserved unchanged. Permanent regression:
# a deterministic fake delay/click provider exercises the graph's PDC (before/after compensation), sample-
# accurate MIDI scheduling (EventLog + click onset), automation sub-block landing, M7a bus recombination,
# block-size independence (64/128/256/512) at 48k/44.1k, and the Server::checkPluginLatency change seam —
# offline, no device, no JUCE at runtime. --evidence <outDir> [delayVst3] [clickVst3] runs the real-VST3
# matrix (serve architecture + null device + the existing Engine::startCapture seam) with the OUT-OF-REPO
# CoreTimingDelay/CoreTimingClick probes and writes probe songs, captures (.f32/.wav) and a JSON deviation
# table. Method + results: docs/relatorios/2026-11-20-timing-latency-proof.md.
if(TARGET CoreTestSynth_VST3 AND TARGET CoreTestGain_VST3 AND TARGET core_plugin_host)
  add_executable(timing_proof_tests timing_proof_tests.cpp ${CMAKE_SOURCE_DIR}/engine/src/io/device.cpp)
  target_include_directories(timing_proof_tests PRIVATE ${CMAKE_SOURCE_DIR}/engine/src ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(timing_proof_tests PRIVATE core_plugin_host core_engine_lib miniaudio_impl)
  target_compile_definitions(timing_proof_tests PRIVATE
    CORE_TEST_SYNTH_VST3="$<GENEX_EVAL:$<TARGET_PROPERTY:CoreTestSynth_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
    CORE_TEST_GAIN_VST3="$<GENEX_EVAL:$<TARGET_PROPERTY:CoreTestGain_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
    JUCE_MODAL_LOOPS_PERMITTED=1)
  add_dependencies(timing_proof_tests CoreTestSynth_VST3 CoreTestGain_VST3)
  if(WIN32)
    target_link_libraries(timing_proof_tests PRIVATE dbghelp)
  endif()
  add_test(NAME timing_proof_tests COMMAND timing_proof_tests)
  set_tests_properties(timing_proof_tests PROPERTIES LABELS merge TIMEOUT 600)
endif()

# S2+S3 (DATA priority): a song with missing plugins must be announced (name + why, engine answer AND the
# app session's notice), relink must identify the instance and preserve state + automation (no-match and
# duplicate cases recoverable), and an invalid plugin state must be reported with the document's ORIGINAL
# blob preserved across the live-state refresh and save/reopen (never factory defaults in silence).
# Serve harness as engine_process_tests, plus the app's own EngineSession (juce_events, null backend) and
# document save/reopen path. Red measured at a9a0384 + round-1 (coordinator-round2/s2s3-notes.md).
if(TARGET core_engine_client AND TARGET juce_events)
  add_executable(state_recovery_tests state_recovery_tests.cpp ${CMAKE_SOURCE_DIR}/app/desktop/src/EngineSession.cpp)
  target_include_directories(state_recovery_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/app/desktop/src)
  target_link_libraries(state_recovery_tests PRIVATE core_engine_client core_domain juce::juce_events)
  # The wait loop pumps the JUCE message queue (runDispatchLoopUntil is a modal-loop API, off by default in JUCE 9).
  target_compile_definitions(state_recovery_tests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 CORE_ENGINE_EXE="$<TARGET_FILE:core-engine>")
  add_dependencies(state_recovery_tests core-engine)
  add_test(NAME state_recovery_tests COMMAND state_recovery_tests)
  set_tests_properties(state_recovery_tests PROPERTIES LABELS merge TIMEOUT 300)
  if(TARGET CoreTestSynth_VST3)
    target_compile_definitions(state_recovery_tests PRIVATE
      CORE_TEST_SYNTH_VST3="$<GENEX_EVAL:$<TARGET_PROPERTY:CoreTestSynth_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")
    add_dependencies(state_recovery_tests CoreTestSynth_VST3)
  endif()
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

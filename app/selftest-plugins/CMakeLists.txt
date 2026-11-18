# Small VST3 plugins made here, for Core.exe --self-test only (2026-11-16: Core's sound comes from plugins, so the
# self-test needs one it knows): an instrument (a sine per note) and an effect (gain). Built into the build folder;
# never copied into the system's VST3 folder, never shipped.
juce_add_plugin(CoreTestSynth
  PRODUCT_NAME "Core Test Synth"
  COMPANY_NAME "Core"
  VERSION 1.0.0
  PLUGIN_MANUFACTURER_CODE Core
  PLUGIN_CODE CTsy
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  FORMATS VST3
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_AUTO_MANIFEST TRUE)
target_sources(CoreTestSynth PRIVATE TestPlugins.cpp)
target_compile_definitions(CoreTestSynth PUBLIC CORE_TEST_SYNTH=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(CoreTestSynth PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)

juce_add_plugin(CoreTestGain
  PRODUCT_NAME "Core Test Gain"
  COMPANY_NAME "Core"
  VERSION 1.0.0
  PLUGIN_MANUFACTURER_CODE Core
  PLUGIN_CODE CTgn
  IS_SYNTH FALSE
  FORMATS VST3
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_AUTO_MANIFEST TRUE)
target_sources(CoreTestGain PRIVATE TestPlugins.cpp)
target_compile_definitions(CoreTestGain PUBLIC CORE_TEST_GAIN=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(CoreTestGain PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)

# An instrument that crashes in processBlock after 0.1 s (an access violation), for the engine's tests only
# (tests/cli_plugin_crash_render.cmake, tests/engine_process_tests.cpp): a crashed plugin must not hang the engine.
juce_add_plugin(CoreTestCrash
  PRODUCT_NAME "Core Test Crash"
  COMPANY_NAME "Core"
  VERSION 1.0.0
  PLUGIN_MANUFACTURER_CODE Core
  PLUGIN_CODE CTcr
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  FORMATS VST3
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_AUTO_MANIFEST TRUE)
target_sources(CoreTestCrash PRIVATE TestPlugins.cpp)
target_compile_definitions(CoreTestCrash PUBLIC CORE_TEST_CRASH=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(CoreTestCrash PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)

# An instrument that asks the host to restart the component (restartComponent(kReloadComponent)) and then crashes in
# the same processBlock frame (CORE_TEST_CRASH_RESTART, D restart deadlock): the engine must skip the crashed
# instance's internal restart, keep answering and play on. Built for the tests; never shipped.
juce_add_plugin(CoreTestCrashRestart
  PRODUCT_NAME "Core Test Crash Restart"
  COMPANY_NAME "Core"
  VERSION 1.0.0
  PLUGIN_MANUFACTURER_CODE Core
  PLUGIN_CODE CTrs
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  FORMATS VST3
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_AUTO_MANIFEST TRUE)
target_sources(CoreTestCrashRestart PRIVATE TestPlugins.cpp)
target_compile_definitions(CoreTestCrashRestart PUBLIC CORE_TEST_CRASH_RESTART=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(CoreTestCrashRestart PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)

# A gain that throws a C++ exception in processBlock after 0.1 s (CORE_TEST_THROW, audit 6.4): the engine must
# silence its track and play the others on. Built for the tests; never shipped.
juce_add_plugin(CoreTestThrow
  PRODUCT_NAME "Core Test Throw"
  COMPANY_NAME "Core"
  VERSION 1.0.0
  PLUGIN_MANUFACTURER_CODE Core
  PLUGIN_CODE CTth
  IS_SYNTH FALSE
  FORMATS VST3
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_AUTO_MANIFEST TRUE)
target_sources(CoreTestThrow PRIVATE TestPlugins.cpp)
target_compile_definitions(CoreTestThrow PUBLIC CORE_TEST_THROW=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(CoreTestThrow PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)

# A gain that crashes on demand in an entry point outside processBlock (state, reset, editor) or in a parameter
# getter, chosen by an environment variable (CORE_TEST_FAULT_*): the crash tests for the host's guards. Never shipped.
juce_add_plugin(CoreTestFault
  PRODUCT_NAME "Core Test Fault"
  COMPANY_NAME "Core"
  VERSION 1.0.0
  PLUGIN_MANUFACTURER_CODE Core
  PLUGIN_CODE CTfl
  IS_SYNTH FALSE
  FORMATS VST3
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_AUTO_MANIFEST TRUE)
target_sources(CoreTestFault PRIVATE TestPlugins.cpp)
target_compile_definitions(CoreTestFault PUBLIC CORE_TEST_FAULT=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(CoreTestFault PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)

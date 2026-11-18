# A plugin that crashes in processBlock must not hang an offline render (2026-11-18). The host catches the crash with
# structured exception handling, so no destructor runs and JUCE's process lock stays held; releasing or deleting that
# instance afterwards waited for the lock forever and `core-engine render` never ended. Here: a song with the crashing
# test plugin on one track and the test synth on another renders to the end, exits 0, writes the file, and the synth
# is still heard.
file(MAKE_DIRECTORY "${WORK}")

# The plugin as the engine describes it ({format, id, name, file, ...}): what a song's track carries.
function(scan_plugin file out)
  execute_process(COMMAND "${ENGINE}" scan "${file}" OUTPUT_VARIABLE found ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 60)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "core-engine scan ${file} exited with ${rc}: ${err}${found}")
  endif()
  string(JSON count LENGTH "${found}")
  if(count LESS 1)
    message(FATAL_ERROR "core-engine scan found no plugin in ${file}: ${err}${found}")
  endif()
  string(JSON first GET "${found}" 0)
  set(${out} "${first}" PARENT_SCOPE)
endfunction()
scan_plugin("${CRASH_PLUGIN}" crash)
scan_plugin("${SYNTH_PLUGIN}" synth)

set(note "{\"tick\": 0, \"length\": 960, \"pitch\": 60, \"velocity\": 0.8}")
file(WRITE "${WORK}/plugin-crash.json" "{\"format\": \"core-project\", \"version\": 2, \"name\": \"plugin crash\", \"bpm\": 120,
 \"lengthTicks\": 7680, \"masterGainDb\": 0, \"tracks\": [
  {\"id\": \"crash\", \"name\": \"Crash\", \"instrument\": \"plugin\", \"plugin\": ${crash}, \"patternTicks\": 3840, \"notes\": [${note}]},
  {\"id\": \"synth\", \"name\": \"Synth\", \"instrument\": \"plugin\", \"plugin\": ${synth}, \"patternTicks\": 3840, \"notes\": [${note}]}]}")

file(REMOVE "${WORK}/plugin-crash.wav")
# 4 s of song render in well under a second; 45 s is the hang, reported as such (ctest's own timeout is the backstop).
execute_process(COMMAND "${ENGINE}" render "${WORK}/plugin-crash.json" "${WORK}/plugin-crash.wav"
                OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 45)
if(rc MATCHES "timeout")
  message(FATAL_ERROR "core-engine render did not end within 45 s after a plugin crashed in processBlock (the engine hangs): ${err}")
endif()
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "core-engine render exited with ${rc}: ${err}${out}")
endif()
string(JSON ok GET "${out}" ok)
if(NOT ok)
  message(FATAL_ERROR "render without ok: ${out}")
endif()
string(JSON missing LENGTH "${out}" result missing)
if(NOT missing EQUAL 0)
  message(FATAL_ERROR "a test plugin did not load, so nothing was tested: ${out}")
endif()
# The crash really happened: the host says it left that instance alone.
if(NOT err MATCHES "crashed plugin")
  message(FATAL_ERROR "the engine's log does not say that it left a crashed plugin alone, so the crash was not tested: ${err}")
endif()
if(NOT EXISTS "${WORK}/plugin-crash.wav")
  message(FATAL_ERROR "render did not write ${WORK}/plugin-crash.wav")
endif()
file(SIZE "${WORK}/plugin-crash.wav" size)
if(size LESS 1000000)
  message(FATAL_ERROR "plugin-crash.wav has ${size} bytes: expected about 1.7 MB (4 s + tail, 48 kHz, stereo, 24 bits)")
endif()
# The other track plays on: the synth's note is in the file (velocity 0.8 x gain 0.5 = 0.4 before the master).
string(JSON peak GET "${out}" result peak)
if(peak LESS 0.05)
  message(FATAL_ERROR "the song is silent (peak ${peak}): the crashed plugin took the other track down")
endif()
message(STATUS "plugin-crash.wav ${size} bytes, peak ${peak}")

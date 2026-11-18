<!-- coderabbit-ai-generated:v1 -->

# Report — 2026-11-18 — Native waveform and inactive Rise cleanup

## What was measured

- Repository `d-aicore/core`, branch `coderabbit/audit-fix-web-ui/029b227d`, committed base `ed4642e61b78e30fc4f7d950a28a214d0ac7c801`. Previous removal changes were already uncommitted and were retained. This follow-up is also uncommitted; before/after file hashes distinguish it in the accompanying evidence.
- Authorization: owner's «mais» in this task on 2026-11-18 continues the instruction to remove old native instrument/effect code. MIDI import must remain. Product contract: `docs/DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md`, PROD-03/04/05/06: retain patterns/alignment/user audio and retire own sound generation.
- Environment: Ubuntu 26.04.1, GCC 15.2.0, CMake 4.2.3; existing Release/C++20 Unix Makefiles build, app disabled, tests enabled. No setup, dependency or service changes.

## Results

| ID | Result | Evidence | Classification / state |
| --- | --- | --- | --- |
| CLEAN-07 | Removed `core_previews`, the offline pattern waveform renderer limited to retired native instruments, its background cache, rendering helper and CMake links | Deleted `app/previews/src/pattern_sound.cpp`, `.hpp` and CMakeLists.txt; removed `PatternWaves` from Waveforms and MainComponent | Code; headless build passed |
| BUG-LEGACY-RISE | Removed an active button whose implementation always returned zero and instructed users to add retired FX Core | Deleted `scoreRises`, SCorePanel button, handler, tooltip and layout entry | Confirmed in pre-change code; native UI execution unavailable |

Before removal, a compiled probe supplied a four-note retired Wave pattern to the preview module. It created a render request, rendered it and returned an empty waveform. Plugin tracks were rejected before rendering. Thus the pipeline could not display working instruments' waveforms and spent work rendering silence for the retired ones. The arrangement now directly uses its existing MIDI note drawing path. The file-based audio waveform cache, audio clip rendering, library waveforms and pattern note playback were preserved.

The obsolete `Sound.seconds()` test (one case, two checks) was removed with its deleted datatype. No replacement empty implementation or weakened assertion was introduced. All other hunt cases and the complete merge label remain. This coverage change is limited to a removed feature, not a test fix to hide a failure.

## Verification

- Before-removal probe: `c++ -std=c++20 -pthread -Iapp/domain/src -Iapp/previews/src -Iengine/src -Iengine/third_party /tmp/native-preview-probe.cpp /tmp/core-audit-build/app/previews/libcore_previews.a /tmp/core-audit-build/app/domain/libcore_domain.a /tmp/core-audit-build/engine/libcore_engine_lib.a -o /tmp/native-preview-probe && /tmp/native-preview-probe`, exit **0**: `request created; waveform empty=1; notes=4`. Source accompanies the output report.
- `cmake --build /tmp/core-audit-build --parallel 4`, exit **0**. CMake regenerated successfully after removing the target and its links.
- `ctest --test-dir /tmp/core-audit-build -L merge --output-on-failure --timeout 600 --no-tests=error`, exit **8**, **14/15 suites pass**, including MIDI, domain, roles, hunt and CLI rendering. Only the previously recorded FL-1 null-device initialization fails (`Cannot open the audio device: Invalid operation`). The gate is not green.
- Source search: no remaining references to core_previews, pattern_sound, preview::, PatternWaves, patternWaves_, paintClipWave, scoreRises or riseDot_ in app/engine/tests/build configuration. `git diff --check` exit **0**.
- Existing unrelated passing checks were read from the validation record. Full gate rerun was selected because CMake target deletion changes test/desktop linkage. No browser or native Windows runtime test was claimed: this repository has no web application entry point and the JUCE app/client targets require Windows.

## Pending work and ledger handoff

- Windows build and native visual checks remain required: verify MIDI clips show notes, audio clips/library show file waveforms, and arrangement controls still work. The Linux build does not compile those UI changes.
- Existing FL-1 audio-device failure remains open; previous audit findings are not all resolved by these removals.
- Pending ledger entry, owner Diogo / ledger scribe: record removal of the retired preview pipeline and inactive Rise action, probe result, build pass and merge 14/15; keep Windows qualification open. Source: owner's «mais» in this task, 2026-11-18. Do not assign a decision number or claim release qualification. `ESTADO.md` was not changed.

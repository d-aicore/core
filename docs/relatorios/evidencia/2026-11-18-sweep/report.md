<!-- coderabbit-ai-generated:v1 -->

<!-- coderabbit-ai-generated-section:v1:d289182a6a909d22284adc7abf2f12e1ea543960cb9a07ac05921b0354e8ac26 -->

# Deep code sweep — 2026-11-18

## Measurement scope

- Repository: d-aicore/core; branch `coderabbit/audit-fix-web-ui/029b227d`; baseline and product-source revision `ed4642e61b78e30fc4f7d950a28a214d0ac7c801` (Core 0.7.0). Initially clean checkout.
- Authorization: owner's request, 2026-11-18, current Coding Agent conversation: “Need a deep sweep, check if there are any bugs, or code needs to be better.” Audit only under AGENTS.md and docs/AUDITORIA.md. No product behavior, version, or ledger changes.
- Ubuntu 26.04.1, GCC 15.2.0, CMake 4.2.3, Unix Makefiles. Ninja absent; used the gate's equivalent CMake options and merge label with the available generator.
- Read product/boundary documents, ledger, existing audit reports, build/test configuration, persistence/autosave/history, export/publication, server/engine/workers, sampling/MIDI, add-on bridge and relevant desktop/plugin call paths. This is a risk-focused sweep, not a claim that every source line was reviewed. No knowledge-base directory or mapped repo-knowledge content was available.
- Native Windows desktop, JUCE plugin hosting, engine-client runtime, real devices and real plugins were not exercised. No browser verification: this checkout provides a Windows native application, with no web app to preview.

## Findings

| ID | Priority | Finding | Evidence and classification | Status |
| --- | --- | --- | --- | --- |
| BUG-SWEEP-01 | High | A restarted save can destroy a retained recovery copy. | `app/domain/src/storage.cpp:174`: the process-local counter starts at zero, and the temporary file is opened with truncation. Earlier replacement failures deliberately preserve this same temporary file (`:126–139`). A new process saving the same path reuses `.saving-0`; if validation fails, cleanup deletes it. Synthetic probe: seeded recovery file, rejected save, `preservedRecoveryExists=0`. Mechanism proven on Linux; original Windows replacement failure simulated by preexisting retained file. | Open |
| BUG-SWEEP-02 | High | Export does not own or reserve its working filenames. | `engine/src/core/render_job.cpp:276`, `:280`, `:301`; `engine/src/io/wav.cpp:40`: fixed `<target>.part` opens with truncation and cleanup removes every listed working path. Successful synthetic export consumed a preexisting partial export. Separate engine processes exporting the same target can also share this path (concurrency consequence confirmed by code, not separately executed). | Open |
| BUG-SWEEP-03 | Medium | Failed export rollback can strand the original and hide its recovery location. | `engine/src/core/render_job.cpp:92`: restore rename errors are ignored, and `published` counts only new files that could not be removed. Injected placement failure with a directory obstructing the original path leaves old bytes in `.previous`, target a directory, and `published=0`. `app/desktop/src/MainComponent.cpp:6454` unconditionally adds “No existing file was changed.” Recovery bytes survived this probe; no claim of irreversible loss. Engine outcome proven by injected filesystem test; UI wording confirmed statically, Windows dialog unverified. | Open |
| BUG-SWEEP-04 | Medium | Malformed add-on fields throw instead of returning an unusable-add-on error. | `app/control/src/bridge.cpp:209`, `:216`, `:217` call typed `json::value` without checking field types. `readAddon` promises optional failure in `bridge.hpp`. Both `{"name":null}` and a numeric `version` with otherwise valid fields throw `type_error.302` in the probe. Desktop handlers at `MainComponent.cpp:3824`, `:3847`, `:3856` have no local catch. Failed action/exception escape confirmed; process termination on Windows not exercised. | Open |
| BUG-SWEEP-05 | Medium | Failed plugins remain silent despite the instruction to make another edit. | Existing FL-2 revalidated: `MainComponent.cpp:6474` promises recovery on the next change; `engine/src/plugins/juce_host.cpp:351` returns the cached processor even when failed. The desktop never sends `plugin.retry`; server supports it at `server.cpp:444`. Editing with unchanged plugin identity keeps that failed instance. Static confirmation only; real plugin failure/recovery not executed. | Open; existing FL-2 |
| BUG-SWEEP-06 | Medium, Linux portability | The Linux gate requires realtime scheduling privileges to open null audio. | `engine/src/io/device.cpp:110` requests realtime priority unconditionally. Merge test FL-1 fails before its first assertion with “Cannot open the audio device: Invalid operation”. Isolated miniaudio probe: same null device at normal priority opens (0), realtime priority fails (-3); both contexts initialize successfully. Vendored POSIX thread creation returns errors rather than retrying with normal priority (`miniaudio.h:16262`). This establishes the priority dependency here; the exact OS permission/capability causing the rejection was not traced. | Open; Windows impact not established |

IDs above are report-local BUG identifiers, not ledger decisions.

## Verification

All commands ran from the repository root unless stated otherwise. Full logs and reproducible synthetic probe sources accompany the output copy of this report.

1. `cmake -S . -B /tmp/core-audit-build -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Release -DCORE_BUILD_APP=OFF -DCORE_BUILD_TESTS=ON` — exit 0.
2. `cmake --build /tmp/core-audit-build --parallel 4` — exit 0; compiler warnings remain. Warnings alone were not promoted into bug findings.
3. `ctest --test-dir /tmp/core-audit-build -L merge --output-on-failure --timeout 600 --no-tests=error` — exit 8; **14/15 suites passed**, engine_tests failed. That suite reported 25 cases, 127 checks, one failure. Other suites passed; their case counts were not collected. Elapsed 8.83 seconds.
4. `/tmp/core-audit-build/tests/engine_tests M5` — exit 1; reproducible null-device failure, one case and zero assertions reached. No test was relaxed or skipped to obtain a green gate.
5. Compiled and ran `core-audit-probes.cpp` against the baseline engine library and bridge source — exit 0: reproduced existing-part consumption, two malformed metadata exceptions, and unsuccessful rollback recovery. A probe exit of zero confirms the reproduction ran; it does not mean the product behavior is correct.
6. Compiled and ran `core-save-probe.cpp` against baseline domain/engine libraries — exit 0: rejected save deleted seeded `.saving-0`.
7. Compiled and ran `core-audio-probe.cpp` against baseline miniaudio library — exit 0: normal-priority null device succeeds, realtime-priority null device fails.

No fixes were applied, so there is no red/green repair claim. Sanitizers, Windows application startup/play/save/reopen/export and plugin load tests were not run in this sweep. Prior historical passes were not reused as current evidence.

## Recommended implementation scope

1. Address BUG-SWEEP-01 and -02 first: reserve unique working files atomically across processes, never truncate an existing candidate, and clean up only files created by the operation. Preserve previously retained recovery files. Add restart/collision regression cases before changing the implementation.
2. Return rollback outcomes and preserved backup locations explicitly through engine status and the desktop error message; stop promising unchanged files when restoration failed. Keep all recoverable copies.
3. Validate add-on fields by type and return precise parse errors. Verify malformed first-choice manifests do not prevent discovery of later usable folders.
4. Complete FL-2 with an explicit retry action and accurate wording. Plugin recovery is an existing behavior gap, not permission to redesign plugin fault policy.
5. For Linux null-device verification, decide an explicit scheduling fallback or use a controlled test output for the worker-default test while separately testing real device initialization. Do not silently reduce Windows audio priority.
6. Follow-up qualification: Windows startup/play/save/reopen/export, real plugin fault recovery, audio-thread allocation/locking instrumentation, and measured serial/multicore equivalence under plugin load. Static reading and this Linux test gate cannot establish those quality targets.

## Changes and ledger handoff

Only this report and the reports index are repository changes. Synthetic probes and logs are output artifacts. ESTADO.md and existing historical reports were preserved.

Pending handoff to the ledger scribe (owner: Diogo): record the measured baseline, 14/15 merge-suite outcome, six open findings (including revalidated FL-2), and Windows qualification remaining unverified. Source: owner's audit request quoted above and this report's executed probes/logs. No numbered decision is proposed or ratified; implementation authorization/scope is the next action.


<!-- /coderabbit-ai-generated:v1 -->
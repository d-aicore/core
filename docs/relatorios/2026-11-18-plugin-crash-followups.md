# Report — 2026-11-18 — Plugin crash follow-ups (work-plan item 1, fixes A–D)

## What was measured

- Repository / branch / SHA: `D:\CORE\CORE\.agents\worktrees\flawless-crash`, branch `flawless-crash`.
  Base `ed4642e`; A/B/C landed as `d434720` (A), `81f835a` (B), `8fcce35` (C). This delivery adds only the
  docs commit that carries this report and its index row (D is a diagnosis, not a code change). The main
  checkout `D:\CORE\CORE` was never edited; nothing was pushed or merged; the pinned JUCE at
  `D:\Core-deps\JUCE-9.0.5` was never edited.
- Scope and authorization: work-plan `docs/plan/README.md` item 1 ("Plugin crash, follow-ups"). Fixes A, B and C
  were completed by the prior engineer in this stream; fix D was investigated and is reported here by the
  continuing engineer. D turned out to need a vendored change, which is out of scope, so D is reported as a
  diagnosis with a measured red and options, not a code fix.
- Environment: Windows 10.0.26300 (owner's PC, Ryzen 7 7800X3D), MSVC VS 18 BuildTools, Release, Ninja,
  JUCE 9.0.5 pinned (`-DFETCHCONTENT_SOURCE_DIR_JUCE=D:/Core-deps/JUCE-9.0.5`), build folder
  `D:\Core-build\flawless-crash`, null audio backend for the serve tests. Evidence in
  `D:\Core-build\flawless-crash-evidence\`.
- Tests executed: `engine_process_tests` (the real `core-engine serve` driven through the app's own client),
  `CoreApp --self-test`, and the full gate `ci\gate.ps1` in both modes (`-L merge`, then `-Desktop`).
- Limitations: D is not fixed in code (the only correct fix is vendored; see below). The C green evidence was
  measured on a `.modified` (dirty) build; the clean-tree gate below re-runs the desktop self-test on a clean
  committed tree with `--version == commit sha` and no `.modified` suffix.

## Results

| ID | Problem / result | Evidence (SHA, file:line or run) | Classification | State |
| --- | --- | --- | --- | --- |
| A | Crash outside `processBlock` (state, reset, editor) had no test; the paths were already guarded. Added `CORE_TEST_THROW` target + `CORE_TEST_FAULT` plugin and 4 serve tests as regression guards. | `d434720`; `A-engine_process_tests-green.txt`: 8 tests / 93 checks / 0 failures (was 4 tests). Measured GREEN on the prior state (guards already present), not a fake red. | proven by test | validated |
| B | A crash in a parameter getter (`plugin.params`) or in the plugin-window surface took the whole engine down (quality bar 2). Guarded each parameter read and the window's close/resize/paint with the existing `guardedCall` pattern; the window knows its owner so a crash there marks it crashed. | `81f835a`; red `B-param-red.txt`: `plugin.params` → "The engine exited (code 3221225477)" (0xC0000005), 8 failures. Green `B-engine_process_tests-green.txt`: 9 tests / 104 checks / 0 failures. | proven by test (red→green) | corrected |
| C | Restart was offered only when Stopped/NoDevice, not after repeated engine timeouts while playing. Exposed `syncRetriesGaveUp` in the session snapshot and show Restart for Diverged + gave-up; self-test steps 112–114 with a `timeOutSyncForTest` hook. | `8fcce35`; red `C-selftest-red.txt`: "failed: no Restart offer after repeated engine timeouts". Green `C-selftest-green.txt` + `.png`: "restart offered; restarted after timeouts", full self-test ok, loudest=0.735. Green was on a `.modified` build; re-verified clean by the gate below. | proven by test (red→green) | corrected |
| D | A crashed instance (SEH catch leaves JUCE's `processMutex` held) whose plugin calls `restartComponent` reaches JUCE's internal message-thread path `reset()`/`releaseResources()`, which spin on the held lock → message-thread deadlock; the engine answers nothing. | Reachable path confirmed in code (below); deterministic red measured `D-restart-red.txt`: `plugins.states` and `engine.stats` time out, engine terminated (exit 1), 4 failures. No Core-side fix possible without vendored JUCE. | confirmed in code + proven by test (red); fix not implemented | open — needs vendored decision |

## The D reachable path (confirmed in code, JUCE 9.0.5 pinned)

Line numbers re-confirmed at the pinned copy; in JUCE 9.0.5 the VST3 implementation lives under
`modules/juce_audio_processors_headless/format_types/` (the task's `juce_audio_processors/...` anchors map here
with the same line numbers).

1. `engine/src/plugins/juce_host.cpp:199-205` — `processBlock` is run under `guardedCall` (SEH). On an access
   violation `markCrashed()` fires and the instance is silenced and leaked on purpose (`~JuceProcessor`,
   `juce_host.cpp:111-114`). Under `/EHsc` the SEH catch runs no C++ destructors, so every lock held around the
   call stays held.
2. The wrapper takes that lock in `processBlock`: `juce_VST3PluginFormatImpl.h:2422`
   (`const SpinLock::ScopedLockType processLock (processMutex);`). After the crash it is held forever.
3. Core's own `AudioProcessorListener` is gated (`juce_host.cpp:225-227`, `!failed()`), and every Core call into
   a failed instance is skipped/guarded (B). But JUCE's INTERNAL path is not gated: a plugin calling
   `IComponentHandler::restartComponent` → `VST3HostContextHeadless::restartComponent`
   (`juce_VST3PluginFormatImpl.h:3509-3517`) → `ComponentRestarter::restart`
   (`juce_VST3Common.h:1625-1662`, an `AsyncUpdater`) → on the message thread
   `restartComponentOnMessageThread` (`juce_VST3PluginFormatImpl.h:3528-3566`):
   - `kReloadComponent` → `plugin->reset()` (`:2809-2811`) takes `processMutex`;
   - `kIoChanged` → `plugin->releaseResources()` (`:2391-2393`) takes `processMutex`;
   - `kMidiCCAssignmentChanged` → `updateMidiMappings()` (`:2277`) takes `processMutex`.
   `juce::SpinLock::enter()` spins until acquired (`juce_SpinLock.h`), so the message thread spins forever.
4. The crashed instance is leaked alive and its editor stays open until replace/keepOnly/releaseAll
   (`juce_host.cpp:382/416/420`), so the plugin (editor or any of its threads) can still call `restartComponent`.
   From then on the engine's message thread is deadlocked and it answers no command (all plugin commands go
   through `runSync` on that thread).

### The measured red (deterministic)

`CORE_TEST_CRASH` was extended with an env-triggered variant (`CORE_TEST_CRASH_RESTART`): the plugin implements
`juce::VST3ClientExtensions`, captures the host's `IComponentHandler` via the public
`setIComponentHandler(Steinberg::FUnknown*)` hook (the wrapper calls it, `juce_audio_plugin_client_VST3.cpp:1432`),
and in the crashing `processBlock` calls `handler->restartComponent(Steinberg::Vst::kReloadComponent)` immediately
before the access violation. The request is queued to the message thread; the crash then leaves `processMutex`
held; the message thread dispatches the restart and deadlocks. A serve test loads it beside the synth, plays, and
asserts the message thread still answers.

Command (Release, `D:\Core-build\flawless-crash`, engine at `8fcce35`):
`tests\engine_process_tests.exe "restart the component"` → exit 1. Output (`D-restart-red.txt`):

```
plugins.states: No reply to 'plugins.states' within 5000 ms.
engine.stats: No reply to 'engine.stats' within 2000 ms.
FALHOU ... end.get() == "The engine exited (code 0).": medido The engine exited (code 1).
FAIL serve: a crashed plugin that asks to restart the component does not deadlock the message thread
1 testes, 7 verificacoes, 4 falhas
```

This red was measured with a temporary test-plugin variant + serve test; those edits were reverted afterwards so
the gate runs on a clean tree (a hanging test must not be left in the suite). The variant is reproducible from the
description above; it is the regression test to land together with the vendored fix.

### Why there is no Core-side fix (options instead)

Core cannot reach any of the three things that would block the path: the internal `VST3HostContextHeadless`
(its `plugin` pointer and `ComponentRestarter` are private; it is a `friend` of the wrapper, not a public type),
the `processMutex` (no public API releases it; every public method that touches it ENTERS it), or the plugin's
`IComponentHandler` (Core cannot revoke what JUCE handed the plugin). The wrapper's public surface is only
`AudioPluginInstance` + `VST3Client` (getPreset/setPreset) + ARA. Deleting the wrapper is not an escape either:
its destructor calls `cleanup()` → `releaseResources()` → the same held lock (`:2171-2181`). Options, in order of
correctness:

1. Vendored gate (the correct fix): in `restartComponentOnMessageThread`, skip the lock-taking calls for an
   instance the host has marked crashed. Needs a small public way for Core to mark the wrapper crashed (or a
   host callback). This is a `D:\Core-deps` edit — pinned, out of scope here; needs an owner/numbered decision.
2. Vendored crash-recoverable lock: make the wrapper's process lock recoverable after an SEH crash (or catch the
   crash inside the wrapper's lock scope so the `ScopedLockType` destructor runs). Also a vendored edit.
3. Core-side `/EHa` for `juce_host.cpp` + C++ `try/catch` in `guardedCall`: the unwind would run the wrapper's
   `ScopedLockType` destructor and release the lock. Rejected as non-minimal and unsafe: it changes the whole
   crash mechanism B builds on, and running destructors through a crashed plugin's frames can call back into it
   (a second crash), against quality bar 2. Not done.
4. Core-side editor disarm on `markCrashed` (hide/delete the crashed plugin's window): only removes the
   editor-triggered case, not a `restartComponent` from `processBlock` or a plugin thread (the measured red), and
   deleting a crashed plugin's editor itself calls plugin code. Partial mitigation at best; not a fix, not done.

Per the task's instruction ("if you conclude the only correct fix is vendored, STOP and report options"), D is
left open with the red measured and the options above; no vendored file was touched and no partial fix is claimed.

## Changes

- A (`d434720`, prior engineer): `app/selftest-plugins/CMakeLists.txt`, `app/selftest-plugins/TestPlugins.cpp`,
  `tests/CMakeLists.txt`, `tests/engine_process_tests.cpp` — `CORE_TEST_THROW` target, `CORE_TEST_FAULT` plugin,
  4 serve regression tests.
- B (`81f835a`, prior engineer): `engine/src/plugins/juce_host.cpp`, `app/selftest-plugins/TestPlugins.cpp`,
  `tests/engine_process_tests.cpp` — parameter-getter and window-surface guards.
- C (`8fcce35`, prior engineer): `app/desktop/src/EngineSession.{h,cpp}`, `app/desktop/src/MainComponent.cpp` —
  Restart after repeated timeouts; self-test steps 112–114.
- D (this session): no code change. Diagnosis + measured red + options (above). The temporary red-test edits were
  reverted; no vendored file was touched.
- Documentation (this session): this report + one index row in `docs/relatorios/README.md`.

## Verification

- A: `engine_process_tests` 8 tests / 93 checks / 0 failures (`A-engine_process_tests-green.txt`); regression
  guards measured green on the prior state (the paths were already guarded).
- B: red `plugin.params` → engine exit 0xC0000005, 8 failures (`B-param-red.txt`); green 9 tests / 104 checks /
  0 failures (`B-engine_process_tests-green.txt`).
- C: red self-test exit 1 "no Restart offer after repeated engine timeouts" (`C-selftest-red.txt`); green exit 0,
  "restart offered; restarted after timeouts", loudest=0.735 (`C-selftest-green.txt`, `.png`). Both on a
  `.modified` build; the clean-tree re-verification is the gate below.
- D: red measured as above (`D-restart-red.txt`), exit 1, 4 failures; no green (fix is vendored, not done).
- Full clean gate, both modes, on the clean committed tree (no `.modified`):
  - `ci\gate.ps1 -Build D:\Core-build\flawless-crash -Jobs 6` (merge): **VERDE, 17/17 tests passed**, 57 s
    (`gate-merge-prelim.log`; re-run as the final gate at the docs commit, `gate-merge-final.log`).
  - same `-Desktop` (all tests + desktop_self_test): **VERDE, 18/18 tests passed** (17 merge + desktop_self_test
    21.5 s), 41 s (`gate-desktop-prelim.log`; final `gate-desktop-final.log`). The desktop self-test passes on the
    clean tree, re-verifying C without the `.modified` suffix.
  - `core-engine --version` equals the commit sha with no `.modified` suffix (checked at the final gate).
- Per-suite case counts (current vs `baseline-suite-counts.txt`), coverage NOT shrunk:
  engine_tests 25/130, domain_tests 19/148, role_tests 21/216, midi_tests 8/115, bug_tests 22/138,
  audit_tests 7/65, audit2_tests 6/80, hunt_tests 14/649, round3_tests 7/82, calls_tests 10/44, bitwig_tests 2/25
  (all equal to baseline); **engine_process_tests 9 tests / 104 checks vs baseline 8/93 — grew by 1 test / 11
  checks (B's parameter-getter test; A had grown it 4→8).** 0 failures in every suite.

## Open items and handoff (proposed ESTADO.md lines — the scribe writes them, D72/D80)

Bugs use the `BUG-` namespace (D75). Proposed lines for the ledger scribe (owner = Diogo):

- `BUG-` (item 1, A): "Crash outside processBlock (state/reset/editor) now has serve regression tests
  (CORE_TEST_FAULT); the paths were already guarded. Proven by test." Source: `d434720`,
  `A-engine_process_tests-green.txt`.
- `BUG-` (item 1, B): "A crash in a parameter getter or plugin window no longer takes the engine down; guarded
  like processBlock. Red 0xC0000005 → green." Source: `81f835a`, `B-param-red.txt`, `B-engine_process_tests-green.txt`.
- `BUG-` (item 1, C): "Restart is offered after repeated engine timeouts, not only when stopped. Red → green in
  the self-test (steps 112–114); re-verified on a clean tree." Source: `8fcce35`, `C-selftest-red.txt`,
  `C-selftest-green.txt`, the clean gate.
- `BUG-` (item 1, D, OPEN): "A crashed plugin that calls restartComponent deadlocks the engine's message thread
  on JUCE's held process lock. Red measured; the only correct fix is vendored JUCE (pinned) — needs an owner
  decision. Options in the report." Source: this report, `D-restart-red.txt`. Pending: owner decision on a
  vendored JUCE change (or an approved Core-side mechanism) before D can be closed.
- Measured-state line: "Full clean gate (merge 17/17 + desktop 18/18) green, version==sha, coverage not shrunk
  vs baseline-suite-counts.txt (engine_process_tests 8→9)." Source: the gate logs.

Residual risk / next task: D stays open until a vendored (or otherwise approved) fix lands; the regression test to
land with it is described above. Not verified in the running app with real third-party plugins: the D path with a
real plugin that calls `restartComponent` (the red uses the test plugin); A/B/C GUI flows beyond the self-test.
Recommendation: take option 1 (vendored gate) to the owner as a numbered decision; do not edit `D:\Core-deps`
without it.

Note on process: while this delivery was gating, a cross-worktree `git stash` collision briefly applied another
stream's (`flawless-deadcode`) domain edits into this worktree. They were preserved (patch
`RECOVERED-flawless-deadcode-domain-edits.patch` + a labeled re-stash) and removed from this worktree; this
delivery's tree contains only the report + index row. No other stream's committed work was altered.
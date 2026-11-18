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
| D | A crashed instance (SEH catch leaves JUCE's `processMutex` held) whose plugin calls `restartComponent` reaches JUCE's internal message-thread path `reset()`/`releaseResources()`, which spin on the held lock → message-thread deadlock; the engine answers nothing. | Reachable path confirmed in code (below); deterministic red measured `D-restart-red.txt`: `plugins.states` and `engine.stats` time out, engine terminated (exit 1), 4 failures. Fix now authorized («sim», 2026-11-18) and the minimal vendored patch prepared: `ci/patches/juce-failed-plugin-restart.patch` (applies clean, read-only `git apply --check` rc 0). **APPLIED + GREEN this apply session** (`cc5b874`): a race in the designed entry-only gate was measured and corrected (crash-aware acquire); D test 1/17/0 exit 0, gate merge 17/17 + desktop 18/18, version==sha. See "D update (apply session)" below. | confirmed in code + proven by test (red cited → green measured) + gated VERDE | fixed — vendored patch applied (working-tree mod) + Core wiring + D test committed `cc5b874`; healthy suites unchanged |

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

## D update (this session, 2026-11-18): fix authorized, minimal vendored patch prepared, apply/gate BLOCKED

This section continues the D diagnosis above. The prior session left D open pending an owner decision; the owner
has now authorized the vendored fix. The patch is designed and saved in-repo, but it is **NOT applied, built,
measured green, or gated** — the shared pinned JUCE is under continuous active build by parallel streams, so the
task's precondition for touching it ("no gate/build active") is not met. The vendored tree is left **pristine**.

### Authorization record (for the ledger; D-number is the scribe's to assign)

Owner «sim», 2026-11-18, this session (PM team session), in response to the proposal that the only correct fix
requires editing the pinned JUCE: approves a **numbered vendored-JUCE modification for the `restartComponent`
deadlock**. Verbatim owner quote: «sim». This is the sole open item-1 follow-up (A/B/C already delivered on this
branch). The decision line and the `BUG-` line are proposed below; only the scribe writes `ESTADO.md` (D72/D80).

### The chosen fix (least-invasive correct option) and why

**Gate JUCE's internal restart path for an instance the host has marked crashed.** Core already knows the crash
(`JuceProcessor::markCrashed()`, `engine/src/plugins/juce_host.cpp:128`); the wrapper does not. The fix gives the
wrapper a crashed flag, set by Core through the public `VST3Client` surface Core already uses
(`getVST3Client()`, `juce_host.cpp:585`), and makes `VST3HostContextHeadless::restartComponentOnMessageThread`
(`juce_VST3PluginFormatImpl.h:3528`) return early for a crashed instance — so it never calls `reset()` /
`releaseResources()` / `updateMidiMappings()`, which would spin on the held `processMutex`.

Why a host signal is necessary (the alternatives are not correct/minimal):
- A crashed audio frame holding `processMutex` is **indistinguishable** from a healthy frame mid-`processBlock`;
  only Core's SEH catch knows the crash. A purely-vendored `tryEnter()` gate would drop legitimate restarts
  whenever the audio thread is momentarily processing (a healthy-hosting behavior change — forbidden).
- A bounded-wait (timeout) gate is heuristic: it could drop a slow-but-healthy plugin's restart and blocks the
  message thread up to the timeout (the red's `engine.stats` budget is 2000 ms). Not provably healthy-identical.
- Catching the SEH **inside** the wrapper's `processBlock` touches the healthy audio hot path and changes the
  crash mechanism B relies on (Core's `guardedCall` would no longer see the crash). Rejected (report option 2/3).
- Releasing/unlocking `processMutex` alone is insufficient: `reset()` would then re-enter the **crashed** plugin's
  code (`setProcessing`/`setActive`) → a second crash on the message thread, against quality bar 2. The restart
  must be **skipped**, not just unlocked.

The chosen gate leaves healthy hosting **bit-identical**: the flag is only ever set on a crash, so for a healthy
instance the gate never fires; the audio hot path, the lock type, and the SEH mechanism are untouched.

### The patch footprint (saved, not applied)

`ci/patches/juce-failed-plugin-restart.patch` — 2 vendored files, 4 hunks, +25 lines, 0 deletions:
1. `modules/juce_audio_processors_headless/utilities/juce_AudioPluginExtensions.h` — add a default-no-op
   `virtual void markCrashedByHost() noexcept {}` to the public `VST3Client` interface (the minimal reachable
   setter; Core only sees `AudioPluginInstance` + `VST3Client`, the concrete wrapper type is internal).
2. `modules/juce_audio_processors_headless/format_types/juce_VST3PluginFormatImpl.h` — (a) override
   `markCrashedByHost()` in the wrapper to set the flag; (b) add the private `std::atomic<bool> hostCrashed{false}`
   member next to `processMutex` (`VST3HostContextHeadless` is already a `friend`, `:2988`); (c) the early-return
   gate in `restartComponentOnMessageThread`.

Verified: `git apply --check` against the pristine `D:/Core-deps/JUCE-9.0.5` returns rc 0 (read-only; the tree was
NOT modified). **Not build-verified** (no compile/gate run — blocked). `<atomic>` is available (the included
`juce_VST3Common.h` already uses `std::atomic`).

### Remaining wiring to land WITH the patch (documented, not committed — would break the build/gate against pristine JUCE)

- Core, `engine/src/plugins/juce_host.cpp`, in `markCrashed()` (after `markFailed(Failure::Crash)`):
  `if (auto* c = instance_->getVST3Client()) c->markCrashedByHost();` — null for non-VST3; sets only an atomic
  (no plugin code, no lock), safe on the audio thread (quality bar 3).
- The D regression test (the prior session's env variant, reverted then; re-add with the patch):
  `CORE_TEST_CRASH_RESTART` extends the `Crash` test plugin (`app/selftest-plugins/TestPlugins.cpp:179`) to
  implement `juce::VST3ClientExtensions`, capture the host `IComponentHandler` via
  `setIComponentHandler(Steinberg::FUnknown*)`, and call `handler->restartComponent(Steinberg::Vst::kReloadComponent)`
  immediately before the access violation in `processBlock`. A serve test (`tests/engine_process_tests.cpp`) loads
  it beside the synth, plays, and asserts the message thread still answers (`plugins.states` ≤ 5000 ms,
  `engine.stats` ≤ 2000 ms), the song keeps playing, and retry/rebuild works — bounded timeouts that **fail on
  hang**, never an infinite wait. With the patch + Core wiring it must pass; without them it reproduces the red.

### Re-application on a future JUCE update

`D:/Core-deps/JUCE-9.0.5` is a plain directory (NOT a git repo); `git apply` works outside a repo. From the JUCE
root, only when `D:/Core-build/GATE-LOCK` is free and no cmake/ninja/cl is compiling the shared JUCE:
```
cd D:/Core-deps/JUCE-9.0.5
git apply --check <core>/ci/patches/juce-failed-plugin-restart.patch
git apply         <core>/ci/patches/juce-failed-plugin-restart.patch
```
If a JUCE bump drifts the anchors, re-locate the four sites (the `VST3Client` struct, `getVST3Client`, the
`processMutex` member, `restartComponentOnMessageThread`) and regenerate the diff. The patch is the durable
record; the vendored tree carries it as a working-tree modification (never git-committed in `D:/Core-deps`).

### The blocker (why nothing was applied/built/gated this session)

The task requires applying the vendored patch only when no gate/build is active. The shared pinned JUCE was under
**continuous active build by parallel streams** throughout this session — `D:/Core-build/GATE-LOCK` cycled
`flawless-pattern` (06:34:56Z) → brief gap (06:42) → `flawless-preservation` (06:43:07Z) → `flawless-randomize`
(06:47:48Z), with 12+ `cl.exe`, 2 `cmake`, 2 `ninja`, 1 `ctest` and active `ci/gate.ps1` runs compiling
`D:/Core-deps/JUCE-9.0.5`. Editing the header mid-build risks the documented false-green corruption (AGENTS.md:
a changed header during a build broke Ninja's dependency tracking). `ci/gate.ps1` also uses a fixed
`%TEMP%/core-gate.bat`, so a concurrent gate collides. There was no reliable safe window. Per the task's
precondition and the shared-dependency caution, the patch was **not applied**, no build/gate was run, and the
vendored tree is **pristine** (both target files byte-identical to their originals, verified with `cmp`).

### Red → green status

- RED: prior session's deterministic measurement, cited (`D-restart-red.txt`): `engine_process_tests`
  "restart the component", 1 test / 7 checks / 4 failures; `plugins.states` no reply 5000 ms, `engine.stats` no
  reply 2000 ms, engine exit 1 (deadlock). Not re-measured this session (build blocked).
- GREEN: **NOT measured** (patch not applied; build/gate blocked). The patch applies clean (read-only check) and
  is designed to make the red pass, but this is a design claim, not a measurement.
- Full gate (merge 17/17 + desktop 18/18): **NOT run this session** (blocked). The branch gate at `85ef0ca` was
  VERDE; this delivery commits only an inert patch file + docs (no compiled code or test change), so it does not
  alter the build/gate by construction — but that is not a re-measurement.

Classification: design **confirmed in code**; patch **prepared** (applies clean, read-only); **NOT** applied,
build-verified, measured green, or gated. D stays open until the patch + Core wiring + test land together on a
free shared JUCE and the gate is VERDE.

## D update (apply session, 2026-11-18): APPLIED + GREEN measured; a race in the designed gate found and corrected

This section supersedes the BLOCKED status of the "D update (this session)" section above **by reference** (that
prior record is kept as history, not erased). The PM relaxed the apply precondition: apply when MY Core streams'
`D:/Core-build/GATE-LOCK` is free and no MY-stream cmake/ninja/cl is compiling the shared JUCE; a concurrent SUR
build is ACCEPTED because the patch is healthy-identical and the unique-bat gate fix (`fffbb41`) prevents gate-bat
corruption (worst case a transient compile retry, not healthy-code corruption). In a verified MY-streams lull the
patch was applied, the Core wiring + D test landed, green was measured, and the full gate ran VERDE in both modes.

### Applied + committed
- Vendored patch applied to the pinned `D:/Core-deps/JUCE-9.0.5` (a PLAIN dir, not a git repo) with `git apply`
  from the JUCE root (`git apply --check` rc 0 first). Verified by grep: `markCrashedByHost` in both headers, the
  `hostCrashed` member and the gate in `juce_VST3PluginFormatImpl.h`. The vendored tree carries it as a
  **working-tree modification, never git-committed in `D:/Core-deps`**.
- Core wiring + D test + variant committed as **`cc5b874`** (branch flawless-crash): `engine/src/plugins/juce_host.cpp`
  (`markCrashed()` → `if (auto* c = instance_->getVST3Client()) c->markCrashedByHost();`, atomic only, audio-thread-safe),
  `app/selftest-plugins/TestPlugins.cpp` + `CMakeLists.txt` (`CORE_TEST_CRASH_RESTART` / `CoreTestCrashRestart`,
  implements `juce::VST3ClientExtensions`, captures `IComponentHandler` via `setIComponentHandler`, calls
  `restartComponent(kReloadComponent)` immediately before the access violation), `tests/engine_process_tests.cpp` +
  `tests/CMakeLists.txt` (the D serve test, bounded timeouts that fail on hang). The unique-bat gate fix `fffbb41`
  was cherry-picked as **`c61bcd1`** so the gate is collision-independent under the accepted concurrent builds.
- The in-repo patch record `ci/patches/juce-failed-plugin-restart.patch` was **regenerated to match the corrected
  vendored state** (10 hunks, was 4). Verified: `git apply --check` rc 0 on a reconstructed pristine tree (the
  original 4-hunk patch also applies clean to that pristine, confirming the reconstruction), applying the new patch
  reproduces the live vendored files **byte-identically** (`cmp`), and the **committed blob** applies clean (rc 0).
  Derivation scripts + pristine/cur copies kept in `D:/Core-build/flawless-crash-evidence/patchwork/`.

### A race in the designed gate (measured) and the correction
The originally-designed gate (early-return in `restartComponentOnMessageThread` when `hostCrashed`) was applied and
built first, and the D test **still deadlocked**. Tracing the engine (`D-diag.log`, `D-engine-stderr.txt`) showed the
order: `restart gate hostCrashed=0` → `calling reset()` → `markCrashedByHost: flag set`. The plugin asks to restart
(async `triggerAsyncUpdate`) and then crashes in the same frame; the message thread dispatches the restart and enters
`reset()`'s `processMutex` spin **before** the audio thread finishes the SEH unwind and sets the flag. An entry-only
gate loses that race (the flag is set too late to stop an already-spinning `reset()`), so it deadlocks just like the red.

Correction (still healthy-identical, still only the failed-plugin path): the restart path's lock-taking calls
`reset()`, `releaseResources()`, `updateMidiMappings()` acquire through `enterProcessMutexUnlessCrashed()` — a
`tryEnter` loop that re-checks `hostCrashed` and returns (skips) the moment the host marks the crash, instead of
spinning forever on the orphaned lock. A healthy instance never sets the flag and acquires exactly when a blocking
enter would, so **healthy hosting is unchanged**; the audio hot path (`processBlock`), the lock type (`SpinLock`) and
the SEH mechanism are untouched. The entry gate is kept as a fast skip, and `prepareToPlay`/`resetParameters` in the
restart path get a `hostCrashed` re-check. This is a **non-additive** change to three control-path functions — the
designed "additive-only" gate was measured insufficient — but it is behavior-identical for healthy plugins. The
owner's authorization («sim», 2026-11-18) is for the `restartComponent` deadlock fix; this correction serves that
goal with healthy hosting unchanged.

### Red → green (measured)
- **RED — cited** (prior session's deterministic measurement, `D-restart-red.txt`): 1 test / 7 checks / 4 failures;
  `plugins.states` no reply 5000 ms, `engine.stats` no reply 2000 ms, engine exit 1 (deadlock). Not re-measured
  against pristine this session (the Core wiring needs the patched header to compile, so a pristine build of the
  wired test is not possible); the race was instead measured directly on the applied designed gate (above).
- **GREEN — measured fresh** with the corrected patch + wiring. The D test `serve: a crashed plugin that asks to
  restart the component does not deadlock the message thread`: **1 test / 17 checks / 0 failures, exit 0**
  (`D-restart-green.txt`). The engine answers (`plugins.states` ≤ 5000 ms, `engine.stats` ≤ 2000 ms), the song plays
  on (callbacks advance), retry/rebuild works, and the engine exits by itself (code 0). Full `engine_process_tests`:
  **10 tests / 121 checks / 0 failures** (`D-engine_process_tests-green.txt`); the healthy crash/fault suites unchanged.

### Full gate (clean committed tree, both modes, GATE-LOCK held)
- merge: **VERDE, 17/17 tests passed**, 50 s (`gate-merge-D.log`, `gate-merge-D-resumo.txt`), @ `cc5b874`.
- desktop: **VERDE, 18/18 tests passed** (17 merge + `desktop_self_test` 21.21 s), 41 s (`gate-desktop-D.log`,
  `gate-desktop-D-resumo.txt`), @ `cc5b874`.
- `core-engine --version` = `0.7.0+cc5b874fa9` == HEAD `cc5b874`, **no `.modified`** suffix (clean tree).
- Coverage vs `baseline-suite-counts.txt`: `engine_process_tests` 8/93 (baseline) → **10/121** (grew by the D test;
  +1 test / +17 checks vs the 9/104 at `85ef0ca`); every other suite equal to baseline; 0 failures everywhere.

Classification: **confirmed in code + proven by test (red cited → green measured) + gated VERDE both modes**. D is
fixed. The vendored decision and the `BUG-` D line are proposed below for the scribe (D72/D80).

## Changes

- A (`d434720`, prior engineer): `app/selftest-plugins/CMakeLists.txt`, `app/selftest-plugins/TestPlugins.cpp`,
  `tests/CMakeLists.txt`, `tests/engine_process_tests.cpp` — `CORE_TEST_THROW` target, `CORE_TEST_FAULT` plugin,
  4 serve regression tests.
- B (`81f835a`, prior engineer): `engine/src/plugins/juce_host.cpp`, `app/selftest-plugins/TestPlugins.cpp`,
  `tests/engine_process_tests.cpp` — parameter-getter and window-surface guards.
- C (`8fcce35`, prior engineer): `app/desktop/src/EngineSession.{h,cpp}`, `app/desktop/src/MainComponent.cpp` —
  Restart after repeated timeouts; self-test steps 112–114.
- D (prior session): no code change. Diagnosis + measured red + options (above). The temporary red-test edits were
  reverted; no vendored file was touched.
- D (continuing session, this delivery): `ci/patches/juce-failed-plugin-restart.patch` (new, inert — the prepared
  vendored diff, NOT applied) + this report's "D update" section + the index row. No Core code or test is committed
  (the `markCrashed()` wiring + the `CORE_TEST_CRASH_RESTART` serve test are documented as remaining work;
  committing them against the pristine JUCE would break the build/gate). The vendored tree was NOT modified.
- D (apply session, this delivery, `cc5b874`): `engine/src/plugins/juce_host.cpp` (`markCrashed()` →
  `markCrashedByHost()`), `app/selftest-plugins/TestPlugins.cpp` + `CMakeLists.txt` (`CORE_TEST_CRASH_RESTART` /
  `CoreTestCrashRestart`), `tests/engine_process_tests.cpp` + `tests/CMakeLists.txt` (the D serve test), and
  `ci/patches/juce-failed-plugin-restart.patch` (regenerated to the corrected 10-hunk vendored diff, APPLIED to the
  pinned JUCE as a working-tree modification). Gate unique-bat fix `fffbb41` cherry-picked as `c61bcd1`. The designed
  entry-only gate raced (measured) and was corrected to a crash-aware acquire; green measured, gate VERDE both modes.
- Documentation (prior session): this report + one index row in `docs/relatorios/README.md`.

## Verification

- A: `engine_process_tests` 8 tests / 93 checks / 0 failures (`A-engine_process_tests-green.txt`); regression
  guards measured green on the prior state (the paths were already guarded).
- B: red `plugin.params` → engine exit 0xC0000005, 8 failures (`B-param-red.txt`); green 9 tests / 104 checks /
  0 failures (`B-engine_process_tests-green.txt`).
- C: red self-test exit 1 "no Restart offer after repeated engine timeouts" (`C-selftest-red.txt`); green exit 0,
  "restart offered; restarted after timeouts", loudest=0.735 (`C-selftest-green.txt`, `.png`). Both on a
  `.modified` build; the clean-tree re-verification is the gate below.
- D: red cited (`D-restart-red.txt`), exit 1, 4 failures; **green measured this apply session** (`D-restart-green.txt`
  1 test / 17 checks / 0 failures, exit 0; `D-engine_process_tests-green.txt` 10 / 121 / 0). The designed entry-only gate
  raced (measured) and was corrected to a crash-aware acquire; vendored patch applied + Core wiring `cc5b874`; gate merge
  17/17 + desktop 18/18, version==sha. See "D update (apply session)".
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
- `BUG-` (item 1, D, FIXED): "A crashed plugin that calls restartComponent no longer deadlocks the engine's message
  thread on JUCE's held process lock. The owner authorized a vendored-JUCE fix («sim», 2026-11-18); the patch
  (`ci/patches/juce-failed-plugin-restart.patch`, host-crashed gate + crash-aware acquire on the restart path) is
  applied to the pinned `D:/Core-deps/JUCE-9.0.5` as a working-tree modification (never git-committed there), with the
  Core `markCrashed()` → `markCrashedByHost()` wiring and the `CORE_TEST_CRASH_RESTART` serve test. A race in the
  designed entry-only gate was measured and corrected. Red cited → green measured (D test 1/17/0 exit 0;
  engine_process 10/121/0); gate merge 17/17 + desktop 18/18, version==sha. Healthy hosting unchanged." Source:
  `cc5b874`, this report (D update apply session), `D-restart-red.txt`, `D-restart-green.txt`, the gate logs.
- Vendored-JUCE DECISION (D-number the scribe assigns): "Owner «sim», 2026-11-18 (this session), approves a
  numbered vendored-JUCE modification for the `restartComponent` message-thread deadlock (item 1, D). The pinned
  `D:/Core-deps/JUCE-9.0.5` carries a host-crashed gate as a working-tree modification, tracked in-repo at
  `ci/patches/juce-failed-plugin-restart.patch` (never git-committed in `D:/Core-deps`); re-apply with `git apply`
  on a JUCE update. Only the failed-plugin restart path is affected; healthy hosting is unchanged." Source: owner
  verbatim «sim», 2026-11-18, this session; the patch file. (Proposed — the scribe writes `ESTADO.md`, D72/D80.)
- Measured-state line: "Full clean gate (merge 17/17 + desktop 18/18) green, version==sha (`0.7.0+cc5b874fa9`),
  coverage not shrunk vs baseline-suite-counts.txt (engine_process_tests 8→10, the D restart test added; 10/121)."
  Source: the gate logs (`gate-merge-D.log`, `gate-desktop-D.log` @ `cc5b874`).

Residual risk / next task: D is FIXED (vendored patch applied + Core `markCrashed()` wiring + the
`CORE_TEST_CRASH_RESTART` serve test, `cc5b874`; gate VERDE merge 17/17 + desktop 18/18, green measured, version==sha).
The vendored tree carries the patch as a working-tree modification — on a JUCE bump, re-apply
`ci/patches/juce-failed-plugin-restart.patch` (regenerated to the corrected 10-hunk form; re-locate the sites if the
anchors drift). Not verified in the running app with real third-party plugins: the D path with a real plugin that
calls `restartComponent` (the green uses the test plugin); A/B/C GUI flows beyond the self-test. The correction makes
three control-path functions (`reset`/`releaseResources`/`updateMidiMappings`) acquire crash-aware — behavior-identical
for healthy plugins (the flag is never set), but a real-plugin load sweep would further confirm healthy hosting under
contention. The owner decision is given («sim», 2026-11-18); the apply/build window was coordinated via `GATE-LOCK`
and the MY-streams lull (SUR-concurrent accepted per the PM decision; unique-bat gate `fffbb41`/`c61bcd1`).

Note on process: while this delivery was gating, a cross-worktree `git stash` collision briefly applied another
stream's (`flawless-deadcode`) domain edits into this worktree. They were preserved (patch
`RECOVERED-flawless-deadcode-domain-edits.patch` + a labeled re-stash) and removed from this worktree; this
delivery's tree contains only the report + index row. No other stream's committed work was altered.
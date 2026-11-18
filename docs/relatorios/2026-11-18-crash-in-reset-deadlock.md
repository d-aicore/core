# Report — 2026-11-18 — Crash-in-reset audio-thread deadlock (option a: crash-aware audio acquire)

## What was measured

- Repository: `d-aicore/core`, worktree `D:/CORE/CORE/.agents/worktrees/flawless-crashreset`, branch
  `flawless-crashreset`. Base `f6b86dc` (clean; consolidation tip `fcf38e0` + self-test convention doc).
  Fix commit `40bad88` (patch + regression + injection); report commit recorded in the index row.
- Scope and authorization: the vendored crash-isolation work the owner authorized («sim», 2026-11-18,
  DSH coordinator session `session-2ff54f03-8344-40bb-8cb4-54df0601eb69`), extended to the audio-path
  acquire by the decided design (option a) dispatched to this session. The vendored JUCE
  (`D:/Core-deps/JUCE-9.0.5`, pinned, plain directory — not a git repo) carries the change as a
  working-tree modification, never git-committed there; the durable record is
  `ci/patches/juce-failed-plugin-restart.patch`, regenerated here.
- Environment: Windows 10.0.26300, MSVC VS 18 BuildTools (v18.10.2), Ninja Release,
  build `D:/Core-build/flawless-crashreset`, null audio backend for every runtime measurement.
  Evidence: `D:/Core-build/flawless-crashreset-evidence/` (loop logs, renders, perf JSON, patch
  reconstruction + verification, gate logs).
- Tests executed: base merge gate; red loops (natural race 30+60, wide-window 5); green loops
  (wide-window 30, natural race 50); healthy null renders before/after (sha256 + cmp); perf under
  load before/after (2 runs each); full gate merge + desktop; per-suite case/verification counts.
- Limitations: the natural race did not reproduce on this machine today (0/90); the red is proven by
  the deterministic wide-window injection, which widens the same window. The demo song renders silent
  by construction (builtins removed 16/11, L10) — its sha256 is a stability fixture, not an amplitude
  proof; the sounding fixture is the 8-track Core Test Synth render. Perf is 2 samples per side on a
  shared machine; the uncontended-path identity is additionally proven by code-path construction.

## Mechanism (confirmed in code, JUCE 9.0.5 pinned)

A plugin crash inside `reset()` is caught by the host's SEH (`guardedCall`,
`engine/src/plugins/juce_host.cpp:64`), which under `/EHsc` runs no C++ destructors: the vendored
wrapper's `reset()` had acquired `processMutex` and never releases it. `JuceHost::resetAll()`
(`juce_host.cpp:430`) marks the instance crashed (`markCrashed()` → `markCrashedByHost()`,
`juce_host.cpp:441`), setting the wrapper's `hostCrashed` atomic (vendored
`juce_VST3PluginFormatImpl.h:3068`). The D patch already made the control-path acquires crash-aware
(`enterProcessMutexUnlessCrashed`, `reset()` at :2844, `releaseResources()`, `updateMidiMappings()`),
and gated `restartComponentOnMessageThread`.

But the four audio-path sites — `processBlock(float/double)` and `processBlockBypassed(float/double)`
— still took a plain `SpinLock::ScopedLockType`, which never consults `hostCrashed`. While stopped,
the audio thread still renders (tails/meters), so it can be waiting on `processMutex` when `reset()`
crashes. `SpinLock::enter` (`juce_Thread.cpp:432`) is one `tryEnter`, then 20 bounded spins, then an
unbounded yield-loop: with the lock held forever, the audio thread spins forever. Callbacks freeze,
the engine never exits, the client's `closeTimeout` (10 s) terminates it — exit code 1 (~11–14 s per
run). This is the residual of the D fix: the message thread was rescued, the audio thread was not.

### Why option (a), not (b)

A Core-side exclusion (b) — not calling the crashed instance from the graph — cannot close the window
where the audio thread is already inside or blocked on `processBlock` when `reset()` crashes: the
graph swap happens after the crash, and the blocked thread never returns to observe it. Only making
the audio-path acquire itself crash-aware (a) releases a thread already waiting on the held lock.
Healthy plugins never set `hostCrashed`, so (a) is behavior- and cost-identical for them on the
uncontended path (measured below: null renders bit-identical, load unchanged).

## Implementation (option a)

Vendored `modules/juce_audio_processors_headless/format_types/juce_VST3PluginFormatImpl.h`
(working-tree modification; line numbers after the change):

- `enterProcessMutexForAudio()` (:3097): the fast path is `SpinLock::enter()`'s own — one successful
  `tryEnter()`, no `hostCrashed` read. Only when the lock is held does it consult `hostCrashed`,
  mirroring `enter()`'s structure (20 bounded spins, then yield-loop) and giving up the moment the
  host marks the crash. Returns false only for a crashed instance.
- `ScopedProcessLockUnlessCrashed` (:3122): RAII guard over that acquire — releases on normal return
  and on a C++ exception unwind exactly like `SpinLock::ScopedLockType`, skips the release when the
  lock was not acquired. Same lock, same scope, same release for a healthy instance.
- The four audio sites (:2436, :2452, :2468, :2491) now use the guard and return early when it is not
  held (the crashed instance is silent; the host's graph silences it from then on).

Out of scope, unchanged: `prepareToPlay` (:2333) and `isBusesLayoutSupported` (:2543) still take the
plain scoped lock — message-thread control-path calls, host-guarded, and the restart gate already
skips crashed instances. The lock type, `processAudio`, and every healthy code path are untouched.

Core-side regression (committed `40bad88`):

- `app/selftest-plugins/TestPlugins.cpp`: `CORE_TEST_FAULT_RESET_DELAY_MS` (optional, with
  `CORE_TEST_FAULT_RESET`) makes the fault plugin wait N ms inside `reset()` — under the wrapper's
  `processMutex` — before crashing, widening the sub-millisecond race to a deterministic window.
  `envInt()` helper follows the existing `_dupenv_s` pattern. With the variable unset the fault
  plugin is byte-for-byte the same behavior as before.
- `tests/engine_process_tests.cpp:397`: new serve test "a plugin that crashes in reset while the
  audio thread waits on its lock is silenced (wide window)" — 300 ms delay, stop, every command
  answered within bounded timeouts, crashed plugin reported, song plays on, engine exits code 0.

## Red → green (measured)

Loop harness: `D:/Core-build/flawless-crashreset-evidence/loop.sh` (exit code + wall ms per run;
a hang = exit 1 after >5 s, the client's 10 s closeTimeout + TerminateProcess).

| Phase | Filter | Runs | Failures | Hangs | Log |
| --- | --- | --- | --- | --- | --- |
| RED natural race, base `f6b86dc` | "crashes in reset" | 30 | 0 | 0 | `red-loop-base.log` |
| RED natural race, base (extended) | "crashes in reset" | 60 | 0 | 0 | `red-loop-base-60.log` |
| RED wide-window, base + injection, vendored fix NOT applied | "wide window" | 5 | 5 | 5 | `red-wide-window-base.log` |
| GREEN wide-window, fix `40bad88` | "wide window" | 30 | 0 | 0 | `green-wide-window.log` |
| GREEN natural race, fix | "crashes in reset" | 50 | 0 | 0 | `green-natural-race.log` |

- The natural race (~1/30 reported by the prep agent) did not fire in 90 base runs on this machine
  today — it is a sub-millisecond timing window; reported, not reproduced here.
- The wide-window injection is the deterministic red: 5/5 runs exit 1 at 10.8–11.4 s, failing
  `engine_process_tests.cpp:424` (`playingOn` after re-play — the audio thread is frozen) and `:426`
  (`The engine exited (code 1).` instead of code 0). Same deadlock signature as the natural race
  (:385/:387 in the base numbering). Built with the injection but WITHOUT the vendored fix
  (`red-injection-build.log`, 39 steps), then the fix was applied and everything rebuilt
  (`green-build.log`: the headless module TU recompiles, `core_plugin_host.lib` + `core-engine.exe`
  relink; the test plugins do not include the headless module and correctly did not rebuild).
- Green: 30/30 wide-window + 50/50 natural race, 0 failures, 0 hangs, every run ~0.6–0.9 s.

## Healthy bit-identical (null renders, sha256 + cmp)

Offline renders, 48 kHz, 24-bit, same project JSON before/after (`demo-300.json`, `load-render.json`
= 8 tracks of Core Test Synth, one note each, master −20 dB so the export does not refuse clipping):

| Fixture | Base sha256 | Fix sha256 | cmp |
| --- | --- | --- | --- |
| demo 300 (silent by construction, L10) | `c23746f0d78436e36454d1d1512f17e08ea53a6369b090c053481e815532c35a` | identical | byte-identical |
| 8× Core Test Synth (peak 0.32126) | `ca9bc3fc86ea0a65b1acf291c2069f83cd22424daaee909a85d5ddacc7c5bcb0` | identical | byte-identical |

Logs: `base/null-sha.txt`, `fix/null-sha.txt`, render JSON alongside. No sound change.

## No audio perf regression

`perf-driver.js` (serve protocol): load project (8× Core Test Synth), null backend, block 128,
`engine.resetStats` after start, 15 s play, `engine.stats`. Two runs per side, same machine, no
other load during the runs:

| Metric | base r1 | base r2 | fix r1 | fix r2 |
| --- | --- | --- | --- | --- |
| callbacks | 5624 | 5626 | 5627 | 5629 |
| overruns | 0 | 0 | 0 | 0 |
| maxRenderMs | 0.5204 | 0.4650 | 0.3922 | 0.3265 |
| avgRenderMs | 0.0698 | 0.0695 | 0.0715 | 0.0714 |
| avgLoadPct | 2.618 | 2.605 | 2.683 | 2.679 |
| loadP95Pct / loadP99Pct | 4 / 5 | — | 4 / 5 | 4 / 5 |
| maxLoadPct | 19.515 | 17.438 | 14.708 | 12.244 |

Verdict: no regression. The uncontended acquire is identical by construction (one successful
`tryEnter`, no `hostCrashed` read — the atomic is only consulted when the lock is held, which for a
healthy plugin is the same brief contention the plain lock already had). Measured: avg render
+0.002 ms (~2–3%, within run-to-run variation at 2 samples/side), max render and max load lower with
the fix, p95/p99 identical, 0 overruns on both sides. Starvations (~1380) are the null backend's
free-running consumer, present equally on both sides. JSON: `base/perf-base*.json`, `fix/perf-fix*.json`.

## Vendored patch regenerated + verified

`ci/patches/juce-failed-plugin-restart.patch` regenerated with the prepared scripts
(`patchwork/reconstruct.py` + `generate_patch.py`, same method as the D apply session): 13 015 bytes,
14 hunks (was 10), covering the D patch plus the 4 audio acquires, `enterProcessMutexForAudio()` and
`ScopedProcessLockUnlessCrashed`. Verification (all in `patchwork/`):

1. `reconstruct.py` reverses every Core addition from the live vendored files → `pristine/` contains
   zero `[Core vendored patch` markers (grep count 0 in both files).
2. `git apply --check` of the new patch on a copy of reconstructed pristine: rc 0.
3. `git apply` then `cmp` against the live vendored files: byte-identical (both files).
4. The worktree copy of the patch: same check + cmp, byte-identical.
5. autocrlf round-trip: the staged blob (`git show :ci/patches/...`) with checkout's LF→CRLF
   conversion simulated (`checkout-sim.patch`) still applies and cmps byte-identical — the committed
   patch survives `core.autocrlf=true`.

### Re-application on a future JUCE update (updated note)

`D:/Core-deps/JUCE-9.0.5` is a plain directory (NOT a git repo); `git apply` works outside a repo.
From the JUCE root, only when `D:/Core-build/GATE-LOCK` is free and no cmake/ninja/cl is compiling
the shared JUCE:

```
cd D:/Core-deps/JUCE-9.0.5
git apply --check <core>/ci/patches/juce-failed-plugin-restart.patch
git apply         <core>/ci/patches/juce-failed-plugin-restart.patch
```

If a JUCE bump drifts the anchors, re-locate the EIGHT sites: the D four (the `VST3Client` struct in
`juce_AudioPluginExtensions.h`, `getVST3Client`/`markCrashedByHost`, the `processMutex` member +
control-path helper, `restartComponentOnMessageThread`) and the crash-reset four
(`processBlock(float)`, `processBlock(double)`, `processBlockBypassed(float)`,
`processBlockBypassed(double)` — each identified by its `jassert (…isUsingDoublePrecision())` line;
`prepareToPlay` and `isBusesLayoutSupported` also take a plain scoped `processLock` and are NOT part
of this patch), plus the two helpers and the RAII guard beside `processMutex`. Regenerate the diff
with the scripts in `D:/Core-build/flawless-crashreset-evidence/patchwork/` (reconstruct → generate →
apply --check → cmp). The patch is the durable record; the vendored tree carries it as a working-tree
modification (never git-committed in `D:/Core-deps`).

## Gate (Windows MSVC Release, under GATE-LOCK)

- Base merge gate @ `f6b86dc`: VERDE, 20/20 merge, 359 s (`gate-merge-base.log`).
- Fix merge gate @ `40bad88`: VERDE, 20/20 merge, 24 s (`gate-merge-fix.log`).
- Fix desktop gate @ `40bad88`: VERDE, 21/21 (merge 20 + `desktop_self_test` 1, passed 21.21 s),
  44 s total (`gate-desktop-fix.log`).
- Version: `core-engine 0.7.0+40bad882b1` == HEAD `40bad88`, no `.modified`.
- Coverage: **203 cases / 3069 verifications / 0 failures** across the 14 suites
  (`suite-counts-fix.txt`; base `f6b86dc`: 202/3059). engine_process 11/131 (was 10/121): the
  wide-window test adds 1 case / 10 verifications; nothing removed or relaxed. Per suite:
  domain 30/525, legacy_guard 9/169, role 21/216, midi 14/208, bug 39/356, bitwig 2/25, audit 7/65,
  audit2 6/80, hunt 13/647, round3 7/82, calls 12/69, menu 7/366, engine 25/130, engine_process 11/131.
- GATE-LOCK held by this stream (`flawless-crashreset`) for both fix gates; deleted after.

## Proposed ESTADO.md lines (only the scribe writes ESTADO.md — D72/D80)

Bugs use the `BUG-` namespace (D75). Proposed for the ledger scribe (owner = Diogo):

- `BUG-CRASH-RESET-AUDIO` (FIXED): "A crash inside a plugin's reset() no longer freezes the audio
  thread: the vendored VST3 processBlock/processBlockBypassed acquire is crash-aware (option a),
  giving up on the processMutex the SEH catch left held once the host marks the crash. Healthy
  hosting bit-identical (null renders sha256 equal) and uncontended cost unchanged (one tryEnter).
  Red: wide-window injection 5/5 hangs exit 1 ~11 s on the base; green: 30/30 + 50/50 loops, full
  gate merge+desktop VERDE @ 40bad88. Patch regenerated (14 hunks) and verified by apply+cmp."
  Source: this report; branch `flawless-crashreset` @ `40bad88`; evidence
  `D:/Core-build/flawless-crashreset-evidence/`.
- Estado medido (proposed): "flawless-crashreset @ 40bad88: gate merge 20/20 + desktop 21/21,
  203 cases / 3069 verifications / 0 failures, version 0.7.0+40bad882b1 == sha; crash-in-reset
  wide-window regression 30/30, natural-race loop 50/50; null renders bit-identical; perf under
  8-synth load unchanged (avg 2.6→2.7%, max render 0.52→0.39 ms, 0 overruns)."
- Pendência (proposed): ledger scribe to write the lines above; PM to verify and merge/push the
  branch (nothing pushed by this session). The natural-race red (~1/30) was reported by the prep
  agent but not reproduced in 90 base runs on this machine today — the deterministic wide-window
  injection is the registered regression.

## Pending and handoff

- For the Test Engineer: acceptance = run `engine_process_tests.exe "wide window"` (must pass in
  <1 s; on an unfixed vendored tree it hangs ~11 s and exits 1) and the "crashes in reset" loop;
  re-run the two null renders and compare sha256 with this report; the perf driver is
  `D:/Core-build/flawless-crashreset-evidence/perf-driver.js`. The vendored tree must carry the
  14-hunk patch (verify with `patchwork/reconstruct.py` + cmp) before any acceptance run.
- Residual risk: `prepareToPlay`/`isBusesLayoutSupported` still take the plain scoped lock (message
  thread, host-guarded; the restart gate skips crashed instances). A crash there would be caught by
  the host's `guarded()` and marked crashed; the lock stays held but no audio-path call waits on it
  forever now. Not changed here (outside the decided scope).
- M5 (live threads) OFF default unchanged; no engine/DSP code changed — only the vendored host
  wrapper, the fault test plugin and one new test.
- Confirmations: main checkout `D:/CORE/CORE` untouched; nothing pushed or merged; no `git stash`
  used; the vendored JUCE change is a working-tree modification, never git-committed in
  `D:/Core-deps`; healthy renders bit-identical; ESTADO.md not edited (lines proposed above).
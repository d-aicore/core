# Report — 2026-11-18 — STATE-PER-EDIT root: plugin state no longer travels with every edit

Work plan item 2 (`docs/plan/README.md`), the 157 ms/edit root. Dispatch scope: the projection sends `stateId`
only, `project.load` answers which keys need state, `plugins.states` takes a list of keys, the app fetches/supplies
state only for the keys that need it. The undo blob-sharing (`history.cpp`), live-export-state and the 08 dirty-flag
reconcile are FOLLOW-UP slices and were NOT done here.

## What was measured

- Repository / branch / SHA: `d-aicore/core`, worktree `D:/CORE/CORE/.agents/worktrees/flawless-state`, branch
  `flawless-state`, base **`f6b86dc`** (integrated main). Red-tests commit **`39194d0`**; fix commit **`5df584f`**;
  this report is the docs commit on top. Main checkout `D:/CORE/CORE` untouched; nothing pushed; no `git stash`.
- Scope and authorization: the dispatch (item 2 of the work plan, owner's mandate «Quero que melhores a app, motor,
  perceber conceito corrigir todos os erros bugs melhorar performance…»). State plumbing only — no audio change,
  proven below. Core has no built-in DSP: all sound comes from external plugins and audio tracks; the fix preserves
  plugin state integrity (quality bar 1), the undo/redo contracts (`keepLivePluginStates`) and legacy load.
- Environment: Windows host (10.0.26300), MSVC VS 18 BuildTools Release / Ninja, CMake + ctest, JUCE 9.0.5 from
  `D:/Core-deps/JUCE-9.0.5` (vendored tree NOT modified here). Build `D:/Core-build/flawless-state`; evidence
  `D:/Core-build/flawless-state-evidence/`. Machine-wide `GATE-LOCK` honored (a stale `flawless-samplehost` lock was
  cleared after verifying 0 builder processes and its VERDE `gate-resumo.txt`; the final gate waited for
  `flawless-ux-windows` and ran under its own lock entry).
- Tests executed: baseline full gate (`ci/gate.ps1 -Build D:/Core-build/flawless-state -Jobs 6`, merge) on pristine
  `f6b86dc`; all 14 merge suites run directly for the case/verification counters (baseline and fix); red and green
  runs of the three new STATE-PER-EDIT tests; the final merge + desktop gate on `5df584f`; `core-engine --version`;
  cross-build null renders of a real-plugin fixture (sha256).
- Limitations: the ms/edit fixture uses plugin places with no installed file (silent tracks) — it measures the
  state plumbing (serialization + pipe + parse + rebuild), which is the root; real-plugin load time is not part of
  it. The sound-neutral fixture uses the real test synth/gain VST3s with real captured states, not third-party
  plugins. No GUI click-through: the desktop self-test (headless, null device, real test plugins) is the app-level
  proof; Save/Export dialogs with real plugins above 8 MB remain not verified in the running app (as the plan notes).
  No sanitizer round in this task.

## The root, confirmed in code at f6b86dc

- `app/domain/src/document.cpp:753` — `toProjection` copied `t.instrument.plugin` whole (the JSON includes the full
  base64 `state`); `pluginChain` (`:695-699`) passed effect JSON verbatim (used at `:736` and `:765` for the master).
  Every edit projection therefore carried every plugin's full state blob.
- `engine/src/core/server.cpp:461-482` — `project.load` answered `missing`/`missingAssets`, not which keys need
  state; `:591-594` — `plugins.states` took no key argument and captured ALL loaded plugins (`captureStates`).
- `engine/src/plugins/juce_host.cpp:367-407` — the live instance is kept by identity `file|id|stateId` (`:370`);
  state is only needed when an instance is (re)created; `:449` `captureStates` skips failed plugins.
- `app/desktop/src/EngineSession.cpp:567-597` — `sync` sent the projection on every edit;
  `MainComponent.cpp:3875-3890` — `fetchPluginStates`/`adoptPluginStates` refresh the document copy before
  Save/Export and re-send it (the engine's stored copy is what an offline export renders from).
- Measured baseline (this machine, `f6b86dc`, 32 tracks x 318 128 B of base64 state — the Vital default patch size
  measured for M2): projection **10 189 817 bytes**; **138.8 ms/edit avg (worst 171.9)** in run 1 and
  **116.2 ms/edit avg (worst 126.5)** in run 2 (projection ~4.2-4.8 ms + round-trip ~112-134 ms). The plan's
  ~157 ms is the same root under its load conditions.

## The fix (design)

State is named, not carried. The blob lives in exactly two places: the document (Save writes it, Undo restores it)
and the engine's accepted project (what an export makes its instances from).

1. **Projection** (`document.cpp:699-716`, `:770`): `pluginRefWithoutState` strips `state`; a place whose state is
   non-empty gets `hasState: true`; `stateId` passes through unchanged. Applied to track instruments, track effects
   and the master chain. `toJson` (Save) is untouched — documents keep every blob, byte-identical.
2. **`project.load`** (`server.cpp:494-548`): accepts an optional `states: {key: base64}` argument, merged into the
   incoming project before acceptance. Then, for every place marked `hasState` whose state the engine does not hold
   under that identity (`pluginIdentity` = `file|id|stateId`, the host's own identity), it answers
   `{statesNeeded: [keys]}` and **accepts nothing** — no revision spent, the old graph keeps playing (the same
   "old sound until the new one is ready" behavior as a plugin load today). A place whose identity the engine already
   holds keeps the stored state, so an ordinary edit needs no state at all. An accepted load answers `statesNeeded: []`.
   A legacy song with its state in the projection is accepted in one pass, exactly as before; a default plugin
   (no state, no `hasState`) is accepted in one pass. `hasState` is validated (`project.cpp:385-387`).
3. **`plugins.states`** (`server.cpp:657-670`, `juce_host.cpp:449-456`, `server.hpp:47`): takes `{keys: [...]}` and
   captures only those (batched); absent/null = every loaded plugin, as before. Failed plugins are still skipped.
4. **App sync** (`EngineSession.cpp:581-620`): on `statesNeeded`, supplies exactly those keys from the document
   (`doc::pluginStatesFor`, `edit.cpp`) in a second `project.load` with the same revision. Refreshed live states
   adopted before Save/Export (`MainComponent::adoptPluginStates`, `:3885-3897`) ride the next `project.load` as its
   `states` argument via `EngineSession::pushPluginStates` (`:127-137`) — which requests the sync itself, because a
   refresh changes nothing in the now-stateless projection (`setDocument`'s unchanged-projection early return would
   otherwise swallow it). Pending states are kept until accepted (a failed/timed-out sync retries with them); a newer
   push for the same key during a send survives. `newSong` clears them.
5. **Export correctness**: the engine's accepted project always holds the state of every plugin place (supplied at
   load or refreshed before export), so `render.start`/`render` keep making offline instances from the same state as
   before — proven bit-identical below. The CLI `render` refuses a stateless projection with a clear message instead
   of rendering silence (`engine/src/app/main.cpp:690-694`).
6. **Batched fetch**: `fetchPluginStates`/`takePluginStates` ask `plugins.states` for `doc::pluginKeys(document)` —
   the keys of the song, not every instance the engine ever loaded (`MainComponent.cpp:2121`, `:3877`).

Undo/redo: `keepLivePluginStates` is untouched (it works on documents, which keep their blobs). A plugin brought back
by Undo is a place whose state the engine no longer holds → `statesNeeded` → the app supplies the preserved blob →
the instance is recreated with the sound Undo restored. The engine keeps live instances by identity across edits, so
an ordinary edit never restarts a plugin (unchanged).

## Red / green (measured)

Red on `f6b86dc` + the new tests (commit `39194d0`), commands
`D:/Core-build/flawless-state/tests/<suite>.exe "STATE-PER-EDIT"`:

| Suite | Red (f6b86dc) | Green (5df584f) |
| --- | --- | --- |
| domain_tests — projection carries no blobs | 16 checks, **9 failures** | 28 checks, 0 failures |
| engine_tests — statesNeeded handshake, batched keys, refresh, legacy | 23 checks, **10 failures** | 25 checks, 0 failures |
| engine_process_tests — 32-track bench + sound-neutral (real synth) | 104 checks, **7 failures** | 105 checks, 0 failures |

Logs: `red-domain.txt`, `red-engine.txt`, `red-process.txt`, `green-*.txt` in the evidence dir. The red bench printed
the slow baseline (below); the red sound-neutral renders differed (the stateless load played the DEFAULT state —
the data-loss the handshake prevents).

### ms/edit, 32-track fixture (the root measurement)

Fixture: `doc::Document` of 32 plugin tracks, each with 318 128 B of base64 state; 20 ordinary edits (a fader move),
each `doc::toProjection` + `project.load` through the app's own client against `core-engine serve` (null backend).
Exact command: `D:/Core-build/flawless-state/tests/engine_process_tests.exe "STATE-PER-EDIT"` (the bench test prints
the numbers; the first load answers `statesNeeded` and is supplied once, then the 20 edits are measured).

| | projection bytes | projection avg | round-trip avg | **total avg ms/edit** | worst |
| --- | --- | --- | --- | --- | --- |
| BEFORE (f6b86dc), run 1 | 10 189 817 | 4.83 ms | 134.0 ms | **138.8** | 171.9 |
| BEFORE (f6b86dc), run 2 | 10 189 817 | 4.18 ms | 112.0 ms | **116.2** | 126.5 |
| AFTER (5df584f), run 1 | 9 881 | 0.85 ms | 12.3 ms | **13.2** | 15.5 |
| AFTER (5df584f), run 2 | 9 881 | 0.89 ms | 12.7 ms | **13.6** | 17.5 |

~9x faster end-to-end; the per-edit message shrank 1031x. The remaining ~12 ms round-trip is the engine's
transactional rollback snapshot in `Server::handle` (`server.cpp:289-290` copies `project_` AND `sources_`, which
still hold the stored blobs) plus the 32-track graph rebuild — a follow-up optimization, not the projection root.
(Baseline runs varied with machine load — another stream was gating; both before/after pairs are reported.)

### Sound-neutral (no audio change)

Fixture (`fixture.json` in the evidence dir): 2 CoreTestSynth tracks + a CoreTestGain effect on the first, real
captured states (synth gain 0.25, effect gain 0.7), one bar, 48 kHz, 24-bit, rendered by `core-engine render`:

| render | engine | path | sha256 |
| --- | --- | --- | --- |
| baseline.wav | f6b86dc | state in the song (legacy) | `699b529e98ef2e859d37a5c0037946cc91bde6b17ccbfc700f22ee9b0b570c58` |
| after.wav | 5df584f | state in the song (legacy) | `699b529e98ef2e859d37a5c0037946cc91bde6b17ccbfc700f22ee9b0b570c58` |
| stateless.wav | 5df584f | stateless projection → statesNeeded → supplied | `699b529e98ef2e859d37a5c0037946cc91bde6b17ccbfc700f22ee9b0b570c58` |

All three byte-identical, peak 0.5730107426643372, `missing: []`. Commands: `node scratch/protocol.mjs make …`
(captures the states over the real protocol), `core-engine.exe render fixture.json <out>.wav`,
`node scratch/protocol.mjs render-stateless …`, `sha256sum`. In-gate, the sound-neutral test also proves the
REFRESH path: live gain moved to 0.75, adopted state rides `project.load`, the render is bit-identical to the legacy
load of that state (1 152 044 bytes each; peaks 0.201 vs 0.602 — two different sounds, each matching its legacy
counterpart, so the comparison is not silence==silence).

## Verification (gate and counts)

Final gate on the fix commit `5df584f` (clean tree), under the machine-wide `GATE-LOCK`
(`flawless-state 2026-11-18T15:01:11Z`), exact commands from the worktree:

```
powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/flawless-state -Jobs 6
powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/flawless-state -Jobs 6 -Desktop
```

- **Merge: VERDE, 100% tests passed out of 20** @ `5df584f`, 47 s (`gate-resumo-merge.txt`).
- **Full (merge + desktop): VERDE, 100% tests passed out of 21** @ `5df584f`, 45 s (`gate-resumo-desktop.txt`);
  `desktop_self_test` Passed 21.36 s.
- **Version == HEAD sha, no `.modified`**: `core-engine --version` = `0.7.0+5df584fd68`;
  HEAD = `5df584fd6835b63728fd0411928a9c8684545594`. The self-test report confirms
  `build=0.7.0+5df584fd68 engine=0.7.0+5df584fd68`.
- **Desktop self-test: ok** (`desktop-selftest-report.txt`), all plugin-state steps green: "plugin gain moved;
  autosave asked; engine killed again; … plugin state kept across the crash; … removed plugin back as it sounded;
  … copy sounds as the original; undo keeps the living plugin's state; timeouts simulated; restart offered;
  restarted after timeouts"; `document=71 accepted=71 applied=71`, `loudest=0.735` (the consolidation baseline's
  0.735 — the app-level sound is unchanged too).
- Baseline gate (pristine `f6b86dc`, fresh build dir): VERDE 20/20 merge, 630 s (`baseline-gate-resumo.txt`).
- Logs: `gate-merge.log`, `gate-desktop.log`, `baseline-gate.log` in the evidence dir.

Per-suite counters (suites run directly, full, no filter; `baseline-suite-counts.txt` / `green-suite-counts.txt`):

| Suite | f6b86dc cases/verifs | 5df584f cases/verifs |
| --- | --- | --- |
| engine_tests | 25 / 130 | 26 / 155 |
| domain_tests | 30 / 525 | 31 / 553 |
| legacy_guard_tests | 9 / 169 | 9 / 169 |
| role_tests | 21 / 216 | 21 / 216 |
| midi_tests | 14 / 208 | 14 / 208 |
| bug_tests | 39 / 356 | 39 / 356 |
| bitwig_tests | 2 / 25 | 2 / 25 |
| audit_tests | 7 / 65 | 7 / 65 |
| audit2_tests | 6 / 80 | 6 / 80 |
| hunt_tests | 13 / 647 | 13 / 647 |
| round3_tests | 7 / 82 | 7 / 82 |
| calls_tests | 12 / 69 | 12 / 69 |
| menu_tests | 7 / 366 | 7 / 366 |
| engine_process_tests | 10 / 121 | 12 / 226 |
| **Total** | **202 / 3059** | **206 / 3217** |

+4 cases / +158 verifications, all in the three touched suites; nothing shrunk, no criterion relaxed. The cli_*
tests print no case counts (unchanged). Baseline gate: VERDE 20/20 (merge) @ f6b86dc, 630 s.

## Changes

Implementation (fix commit `5df584f`): `app/domain/src/document.cpp` (projection names state),
`app/domain/src/edit.hpp/.cpp` (`pluginKeys`, `pluginStatesFor`, `applyPluginStates` changed-keys out-param),
`engine/src/core/server.hpp/.cpp` (statesNeeded handshake, `states` argument, batched `plugins.states`, command
docs), `engine/src/core/project.cpp` (`hasState` validation), `engine/src/plugins/juce_host.hpp/.cpp` (batched
capture), `engine/src/app/main.cpp` (wrapper + CLI render guard), `app/desktop/src/EngineSession.h/.cpp`
(two-pass sync, `pushPluginStates`, batched fetch), `app/desktop/src/MainComponent.cpp` (batched fetch, refresh
push). Tests (red commit `39194d0` + extensions in `5df584f`): `tests/domain_tests.cpp`, `tests/engine_tests.cpp`,
`tests/engine_process_tests.cpp`, `tests/CMakeLists.txt` (engine_process_tests links core_domain for the bench).
Documentation: this report + the index row (docs commit). No vendored dependency touched; no ESTADO.md edit.

## Pendências e passagem (follow-ups and handoff)

NOT done here (dispatch follow-up slices):
1. **Undo blob-sharing** (`history.cpp`): undo snapshots still copy the whole document per step (the 1 GB / 50-step
   ceiling guards it). Goes with item 2's remaining cost.
2. **Live-export-state**: `render_job` could capture the live instances' state at export time instead of relying on
   the app's pre-export refresh round-trip (engine → app → engine). Today's refresh flow is preserved and proven.
3. **08 dirty-flag reconcile** (a plugin that calls `setDirty` on load opening the song already marked changed).
4. Observed here, for the perf follow-up: `Server::handle`'s rollback snapshot copies `project_` + `sources_`
   (with the stored blobs) on every edit — the ~12 ms that remains of the 13.2 ms/edit. Also the engine-side
   stored-state copy into the incoming project on acceptance (~1-2 ms of memcpy for 10 MB).
5. Compatibility note: a NEW app talking to an OLD engine (pre-fix binary) would send stateless projections the old
   engine accepts with default state. App and engine ship together (the gate's version==sha, Help > About shows a
   mismatched engine); no cross-version protocol negotiation was added. Legacy DOCUMENTS are fully supported.

For the Test Engineer: the acceptance surface is Save/Export/Undo with REAL third-party plugins above 8 MB of state
(the plan's "not verified" note stands); the desktop self-test covers the app flow headlessly with the test plugins.
Suggested acceptance: open a 32-track Vital song, edit rapidly (the edit latency should drop ~10x), move a knob in a
plugin window, Save (the file keeps the live state), Export (the render matches the live sound), Undo a plugin
replacement (the old sound returns), restart the engine (the song reloads with its states).

### PROPOSED ESTADO.md lines (the scribe writes them; D72/D80 — not written here)

- **Estado medido:** «STATE-PER-EDIT root fixed (work plan item 2): the edit projection names plugin state
  (`stateId`/`hasState`) instead of carrying the blobs; `project.load` answers `statesNeeded` and accepts nothing
  until those keys arrive; `plugins.states` is batched by key. Measured on Windows/MSVC Release, 32 tracks x
  318 128 B: 116-139 ms/edit → 13.2-13.6 ms/edit (~9x), projection 10 189 817 B → 9 881 B; null renders
  sha256-identical before/after and through the stateless path (`699b529e…`). Gate VERDE merge+desktop @ `5df584f`,
  206 cases / 3217 verifications / 0 failures (baseline f6b86dc 202/3059, +4/+158, none lost). Branch
  `flawless-state` (red `39194d0`, fix `5df584f`); report `docs/relatorios/2026-11-18-state-per-edit-root.md`.
  Source: this delivery's measurements, evidence `D:/Core-build/flawless-state-evidence/`.»
- **Pendência (owner = Diogo):** «Item 2 follow-ups: undo blob-sharing (history.cpp), live-export-state
  (render_job), 08 dirty-flag reconcile; the engine's per-edit transactional copy of the stored state
  (~12 ms of the remaining 13.2 ms/edit). Save/Export with real plugins above 8 MB not verified in the running app.
  Branch `flawless-state` ready for consolidation; nothing pushed.»
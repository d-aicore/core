# Built-in-sound REMOVAL reconciled onto slice-A-integrated main

Integration-worker report (semantic reconciliation, not a mechanical merge). Worktree
`D:/CORE/CORE/.agents/worktrees/removal-reconcile`, branch `main-removal-reconcile`.
Nothing pushed; main and the shared checkout untouched; `ESTADO.md` not edited.

- **Baseline (coordinator-pinned):** `2d953c26e9ef99e2eb266faaa3268bd4facf5f09` (slice-A-integrated main; merge 22/22 + desktop 23/23, version==sha).
- **Removal branch:** `fdb72f3` (remove-builtin-sound tip; base `8c6837e`, pre-round-2; VERDE 20/20 on its own base).
- **Merge base:** `8c6837e`.
- **Integrated merge commit:** `7dbb5b0` (parents `2d953c2` + `fdb72f3`).
- **Auto-merge fixups on top:** `cedf4d0` (randomizeAxes/allAxes "sound"), `92fb173` (recipe[sound]) — final tip.
- **Final tip sha reported:** `92fb173` (version==sha evidence + gate counts below).

## The semantic rule applied

Built-in SOUND removal WINS; everything that makes no sound is PRESERVED (round-2 non-sound fixes,
slice-A UX surfaces, pattern catalogue + kick/bass/cymbal alignment, D:\Samples browser, audio
clips/import/export, mixer gain/pan, hosting/MIDI/timeline/routing/undo/file-format). Tracks stay
GENERIC plugin hosts. Removal made **0 engine changes** (engine was already plugins-only at base); it
deleted the *domain-layer* catalogue, the sampler/sample-host, the built-in data model, UI/control
offerings, 21 templates, app/sampling, and removed-feature tests.

## Conflict resolutions (20 conflicts: 19 content + 1 modify/delete)

**Removal-wins (built-in sound deleted), taken per conflict region (auto-merged parts of main kept):**
- `edit.cpp` — `builtinInstruments()` catalogue deleted (the 8-entry kick/wave/hats/snare/drums/sampler list).
- `document.hpp` — Instrument stripped to `kind = plugin|none|audio|bus`; `.version/.params/.samples/.synth`/`SampleRef` gone.
- `document.cpp` — parse of `kind=builtin|sample` (+params/samples/synth), the kind error string, and trackJson emission all removed. **Post-merge:** the `toProjection` `kind=="sample"` branch removed too (it referenced the deleted fields).
- `samples.hpp`/`samples.cpp` — sampler/sample-host API deleted: `samplerInstrument`, `setTrackSample`, `applySuggestedSamples`, `suggestSamples`, `anchorPack`, `multiSampleSet`, `levelFor`, `roleLoudnessTarget`, `sampleRef`, `shownName(SampleIndex,SampleRef)`. **D110(10) user-sample host superseded** by owner «O Core não tem sampler… apaga». The no-sound browser (`LibrarySample`, `SampleIndex`, `measureLoudness`, `shownName(LibrarySample)`) kept.
- `pcore.cpp` — `fitToSampler` (sampler-key note snapping) deleted; kick/bass/cymbal note alignment kept.
- `pattern_rack.cpp` — `rackStepRow` sampler branch → `return false`; role-part detection kept.
- `InspectorPanel.cpp` — sampler zone facts removed; volume/pan/effects kept.
- `PatternsWindow.cpp` — `instrumentLabel` built-in/sample cases → audio/bus/none.
- `calls.cpp` — `chainJson` (built-in effect reporting) + the sampler `sound` reporting; `songJson`→1-arg (the `SampleIndex` sampler-name lookup dropped). `effectName`/`effectParam` (generic JSON helpers, not the deleted `findEffect`) kept.
- `calls.hpp` — FunctionInfo `instrument`/`native` (built-in flags) dropped; `rises`/`presets`/`options` kept.

**Main-wins (slice-A UX + D110(11) plugin randomize preserved):**
- `randomize.cpp` — the sound axis (K): **built-in half deleted** (`SoundRange`/`soundRanges`, the `kind=="builtin"` `findInstrumentParam/setInstrumentParam` loop); the **D110(11) role-plugin parameter randomize kept** (`soundAxisPluginTracks`, `o.pluginParams`, `setAutomationPoint "vst:<id>"`, graceful no-op when no catalog).
- `MainComponent.cpp` R1/R6/R7 — randomizeSong plugin-param wiring + "Randomize sound" menu + slice-A `togglePanelShown`/`bringWindowsBackToScreen` callbacks kept.
- `randomize.hpp` (auto-merged) — PluginTrackParams/pluginParams/soundAxisAvailable/soundAxisPluginTracks kept.

**Hand-combined (delete built-in sound, keep UX/fixes in the same region):**
- `MainComponent.cpp` R2 (Add Track offer model: kept `addTrackFixedItems()`/BUG-SWEEP-09 loop, dropped `builtinInstruments`/`kSamplerTrack`/`kBuiltinAddFirst`); R5 (MenuId enum: dropped `kSampleApply/Auto/Shuffle`, kept browser ids + `kRandomize+5` sound + slice-A `kViewPlaylist/ChannelRack/PianoRoll/BringWindows`); R8 (dropped the `kSampleAuto` case, kept `togglePanelShown("browser")`/`toggleSidePanel(false)`).
- `LibraryPanel.h` — kept slice-A `onDetach`/`isDetached`; dropped `onUseSample`/`onPreviewSample` (sampler/preview).
- `tests/CMakeLists.txt` — dropped `legacy_guard_tests` registration (file deleted), kept `modules_tests`+`window_tests` labels.
- `README.md` — index UNION: all round-2/slice-A rows (main) + the built-in-sound-removal row (removal).

**Delete (modify/delete, D106):** `tests/legacy_guard_tests.cpp`.

## The auto-merge drop class (the hard part — a removed-side deletion silently dropped a LIVE main line)

git's auto-merge applied removal's deletions for lines main left untouched, which stripped declarations
main still uses. Found and fixed (each as a follow-up commit; the first two only surfaced via compile/red gate):
1. `soundOn` declaration (randomize.cpp:446) — **compile error** at `7dbb5b0`. Re-declared.
2. `randomizeAxes()` `"sound"` entry — **runtime**: `randomizeStyle(axes={"sound"})` threw "no axis 'sound'". Re-added.
3. default `allAxes` `"sound"` exclusion (`x != "tempo" && x != "sound"`) — restored the K-axis opt-in semantics.
4. `if (soundOn) recipe["sound"] = sound;` — behaviour guard silently dropped by auto-merge; restored.

Audited the other combined regions for the same class: `git diff 2d953c2 HEAD` on `document.cpp`/`calls.cpp`/
`LibraryPanel.h`/`MainComponent.cpp` shows **no other dropped live line** (remaining deletions are all
legitimate built-in parse/effect-tag/modulator-cleanup; no slice-A detach/panel/view lines lost).

## Kept-by-design / disclosed scope boundaries (NOT acceptance-grep targets)

- **Engine `sample_host`** (`dsp/sample_host.hpp`, registered as a `"sample"` instrument in `instruments.cpp`
  + built in `graph.cpp`): removal made 0 engine changes and the engine was already plugins-only; round-2
  shipped it. The domain no longer produces `kind="sample"` (parser rejects it; `samplerInstrument` deleted),
  so no Core track can reach it — dormant, unreachable. Left byte-identical to main (single-owner, engine
  union preserved). Physically removing it edits main's engine + `engine_tests` and is out of this merge's
  scope; flagged for the coordinator.
- **`builtinTemplatesFolder()` acceptance grep = 0**: satisfied by dropping the method and reading only the
  user templates folder. The now-inert `restoreBuiltinTemplates()`/`onRestoreTemplates` UI path (restores
  nothing, no built-in templates ship) was KEPT — removal itself kept it and it is not an acceptance target;
  noted as residual built-in-template plumbing.

## Removed-feature tests (D106 — asserted a deleted built-in sound/sampler feature)

- `tests/legacy_guard_tests.cpp` — deleted entirely (built-in/legacy/template guards).
- `hunt_tests.cpp` — P04 (built-in effects+modulators in templates) removed.
- `domain_tests.cpp` — removed: BUG-SWEEP-09 catalogue characterisation (`builtinInstruments().size()==8`) + 3×
  BUG-SWEEP-10 sampler/user-host tests (`setTrackSample`/`applySuggestedSamples`/`kind="sample"`/`.samples`/`.synth`).
  ADJUSTED (not deleted, live behaviour kept): BUG-SWEEP-11 sound-axis test — dropped its `kind="builtin"`
  sub-checks, kept the role-plugin path + graceful no-op + positive plugin-param change.
- `bug_tests.cpp` — BUG-SWEEP-09 menu test: dropped the `kBuiltinAddFirst`/`kSamplerTrack`/`builtinInstruments`
  assertions, kept audio/import offered + Replace plugins-only.
- `modules_tests.cpp` — deleted "legacy project … keeps its data" (asserted `kind="builtin"` + `.synth` loadable;
  parser now refuses it); bus-reuse coverage duplicated by the surviving R-Core/D-Core test. Adjusted the
  wave-role test: name is now `"wave"` (role) not `"Synth"` (deleted catalogue label); the "not W-Core" intent kept.

No live-behaviour test was weakened to force green; the only count shrink is removed-feature deletions.

## GATE (measured, this worktree, fresh build `D:/Core-build/removal-reconcile`, unique-bat + GATE-LOCK stream=removal-reconcile)

Final integrated tip **`92fb1734af…`** (merge `7dbb5b0` of `2d953c2`+`fdb72f3`, then fixups `cedf4d0`, `92fb173`).

- **merge:** VERDE, `ctest -L merge` **21/21 passed**, **253 test-case lines / 0 failures**. 21 = the slice-A
  baseline's 22 merge suites minus `legacy_guard_tests` (deleted, D106).
- **desktop:** VERDE, **22/22** (21 merge + `desktop_self_test` **Passed, 24.51 s**). Self-test
  `loudest=0.735 > 0` (sound out through the generic test-synth plugin — not silence-by-construction),
  `build=0.7.0+92fb1734af == engine=0.7.0+92fb1734af`, document=75/accepted=75/applied=75.
- **version == sha, no `.modified`:** `core-engine --version` → `0.7.0+92fb1734af`; `git status --porcelain` = 0.
- **self-test content (generic plugin-host, both UX groups, single final exit):** play / bpm / undo / seek /
  loop / save+reopen / export / device / crash+restart / **new from template (generic)** / tracks / notes /
  arrangement / live notes / piano key / MIDI / audio import+split+trim / **"effects: plugins only; no built-in
  limiter"** / automation / plugin-state-across-crash / removed-plugin-back-as-it-sounded / timeout→Restart
  offered / failing-plugin→Try again→rebuilt→played again / waveform+autosave recovered / picture / mixer /
  diagnostics / **piano group** (piano gestures, piano surfaces) / **window group** (mixer detach/dock,
  piano roll+browser detach/dock, channel rack dock/detach/close, playlist detach/dock, layout save/restore,
  off-screen rescue, bring-back, legacy settings load). One `ok:` final line = single exit.
- **Denominator reconciliation:** slice-A main (`2d953c2`) carried 269 cases; this integrated tip measures
  **253 case-lines** — the −16 is the D106 removed-feature test deletions (the whole `legacy_guard_tests`
  suite; `hunt` P04+P08; the 3 `domain` BUG-SWEEP-10 sampler/user-host cases + 1 BUG-SWEEP-09 catalogue case;
  the `modules` "keeps built-in data" case; `bug`/`menu` built-in assertions ADJUSTED not deleted). No
  live-behaviour coverage was lost; every shrink is a removed-feature deletion.
  Exact per-suite case breakdown was NOT cleanly re-parsed from the non-verbose gate (limitation) — total
  253 cases / 0 failures is measured directly (`ctest -V`, 253 `ok` / 0 `FAIL` lines).
- **Catalogue + kick/bass/cymbal alignment PRESERVED (proven):** `patterns/` (bass/break/cymbals/…) and
  `pcore/styles.json` present; the alignment-bearing suites (engine/domain/role/midi/bug/menu/hunt/round3/
  calls/modules/window) all pass; `fitToSampler` (sampler-key snapping) deleted but note alignment kept.
- **Grep proof (live code):** `builtinInstruments`=0, `builtinTemplatesFolder`=0, `builtinEffects`=0,
  `samplerInstrument`/`setTrackSample`/`applySuggestedSamples`/`SoundRange`/`findInstrumentParam`/
  `setInstrumentParam`=0, `kind=="builtin"|"sample"`=0 (only an explanatory comment in modules_tests).

## Preservation PROVEN (not asserted)

- `patterns/` catalogue (bass/break/cymbals/…) + `pcore/styles.json` present; kick/bass/cymbal alignment suites pass.
- round-2 markers present: crash-aware `markCrashedByHost` (juce_host.cpp), `writeFileSafely`, `statesNeeded`/
  `keepLivePluginStates`, `hasRoll`/`setRackSwing`.
- slice-A surfaces byte-identical to main: `PanelWindows.cpp`, `WindowLayout.cpp`, `window_tests.cpp`.
- self-test: generic plugin-host (test synth `CORE_TEST_SYNTH_VST3`), both UX groups, single final exit — see gate.

## Proposed ESTADO lines (for the ledger scribe only — D72/D80; NOT written here)

- Decisões: reference the owner «O Core não tem sampler… apaga» (D104/D105) prevailing over D110(10); the
  built-in sound removal reconciled onto the slice-A main.
- Estado medido: branch `main-removal-reconcile` @ `92fb173` (merge `7dbb5b0` of `2d953c2`+`fdb72f3` + 2 fixups);
  counts here.
- Bug namespace: record the auto-merge-dropped-live-symbol pattern (`soundOn`, `randomizeAxes` "sound",
  `recipe[sound]`) as a merge-hazard note; it is not a product defect.
- Pendência (owner/coordinator): whether to physically remove the dormant engine `sample_host` + residual
  `restoreBuiltinTemplates` plumbing in a follow-up.

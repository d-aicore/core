# Report — 2026-11-18 — Sample-host and plugin-parameter randomize (BUG-SWEEP-10/11 positives)

## What was measured

- Repository `d-aicore/core`, worktree `D:/CORE/CORE/.agents/worktrees/flawless-samplehost`, branch `flawless-samplehost`.
  Base **`e035eb7`** (`flawless-builtins` tip; its code tip is `5730842`, the 09/10/11 interim guards, 207 cases / 3134
  verifications). **BUG-SWEEP-10 positive `23b7df1`** (`23b7df15e98fcd6552080abe3292fdf5d68cf247`) was inherited from the
  prior agent and is **not** redone here — only re-verified. **BUG-SWEEP-11 positive `254a55c`**
  (`254a55ce0783de79a71a70e56573e58ef06db100`) is this session's code commit; the delivery report is committed on top as
  the branch HEAD (code tip vs delivery commit, as `e035eb7`). The main checkout `D:/CORE/CORE` was never edited.
- Scope and authorization: the owner's ratifications of 2026-11-18 (verbatim, PT kept):
  - 11: «randomize sobre parâmetros dos plugins dos papéis (undoável); escondido quando não há plugins.»
  - 10: «sampler = host de samples do utilizador, sem fontes de som próprias.»
  These resolve the two positive-target decisions the interim report
  (`2026-11-18-silent-builtins-09-10-11.md`) left open. implements PROD :83 (no apparently-active option that does
  nothing) and PROD :113 (a working plugin is never silently replaced). No new numbered decision is created; no DSP,
  instrument, oscillator, filter or sampler-synth is added — the sample-host plays the user's files and randomize sound
  moves the user's plugins' own parameters.
- Environment: Windows 10.0.26300 x64, MSVC (VS 18 BuildTools), Ninja, Release, JUCE `D:/Core-deps/JUCE-9.0.5`,
  build `D:/Core-build/flawless-samplehost`, evidence `D:/Core-build/flawless-samplehost-evidence/`. Self-test on the
  null audio device. Gates run under the machine-wide `D:/Core-build/GATE-LOCK`.
- Tests executed: per-finding red and green (targeted MSVC builds + suite executables), the full merge gate, and the
  full gate including the desktop self-test — both on a **freshly configured build folder** (see Verification for why).
  Commands and counts in **Verification**.
- Limitations: the plugin-parameter change is proven at the domain layer (the document edit, its projection to the
  engine's `vst:` envelopes, bounded/stepped values, one-Undo restore) and the engine's `vst:` envelope path is confirmed
  in code (`engine/src/core/graph.cpp:472`); a live render through a real VST3 plugin whose parameter audibly moves was
  **not** measured here (no plugin fixture in the repo). The menu hide and the UI catalog wiring are confirmed in code
  and compile; the desktop self-test runs end-to-end but has **no dedicated step** asserting the hidden sound item or a
  plugin-parameter randomize (no `selfTestStep` added or renumbered). A live menu click against a real plugin catalog
  was not verified.

## Results

| ID | Problem / result | Evidence (SHA, file:line or run) | Classification | State |
| --- | --- | --- | --- | --- |
| BUG-SWEEP-10 positive | A sample on an unassigned (`none`) track becomes a **user-sample host** (`kind="sample"`) that plays the user's file — no synth. One-shot zones play to the file's end; looped zones loop while held and release; pitched zones transpose around `rootKey`; level/start/retrigger/decay are playback controls. Save/reopen keeps the host and zones. | `23b7df1`; `samples.cpp` samplerInstrument/setTrackSample; `engine/src/dsp/sample_host.hpp`; `document.cpp` projection; domain `BUG-SWEEP-10 positive` render peak ~0.5 (not silence), engine `sample host` 6/29 | code + test | fixed (inherited, re-verified) |
| 10 plugin-overwrite guard | A track played by a plugin is still refused a sample (the working plugin is never overwritten); the bulk flow skips plugin role tracks. | `23b7df1`; `samples.cpp` setTrackSample guard; domain `BUG-SWEEP-10` 3/29 (`CHECK_THROWS`, kind stays `plugin`) | code + test | preserved |
| BUG-SWEEP-11 positive | The sound axis (K) randomizes the **role plugins' parameters**: a new value within 0..1 on each parameter's own discrete steps, written as a constant `vst:<id>` curve at tick 0 the engine plays over the plugin. **One undoable edit** — one Undo restores the song exactly. A user-drawn curve is never touched; a curve the axis wrote before (marked `extra["randomizer"]="sound"`) is redrawn so "try again" keeps working. | `254a55c`; `randomize.cpp` K plugin loop + `soundAxisPluginTracks`; `randomize.hpp` PluginParamInfo/PluginTrackParams/RandomizeOptions::pluginParams; red 2/26/8 → green 2/36/0 | code + test | fixed |
| 11 hidden with no plugins | `soundAxisAvailable` = role plugins present OR a legacy built-in target; the menu **hides** the sound item when neither (styled is ANDed by the caller). No catalog (engine stopped / plugin not loaded): graceful no-op, never an error. | `254a55c`; `randomize.cpp` soundAxisAvailable; `MainComponent.cpp` menu `if (styled && soundAxisAvailable)`; domain test (none-track song → unavailable, `soundAxisPluginTracks` empty) | code + test | fixed |
| 11 UI wiring | `randomizeSong` fills `RandomizeOptions::pluginParams` from the engine (`plugin.params` per `pluginKey(trackId)`) for the role plugin tracks; the domain never talks to the engine. Menu label "Randomize sound" (lang key updated). | `254a55c`; `MainComponent.cpp` randomizeSong + menu; `lang/pt-PT.json`; CoreApp builds | code | fixed |
| Legacy built-in K | A legacy built-in song still has the sound axis and the old built-in K ranges (preserved for legacy load). | `254a55c`; `randomize.cpp` soundRanges loop kept; domain test legacy builtin → available; `legacy_guard_tests` 9/169 unchanged | code + test | preserved |
| No synth added | The sample-host has no oscillator/filter/synth (`params` carry no cutoff/filterType/resonance, `synth` null); randomize sound moves plugin parameters, not removed built-ins. Self-test: "effects: plugins only; no built-in limiter". | `23b7df1` domain test no-synth checks; `sample_host.hpp`; self-test.txt | code + test | preserved |
| Catalogue / alignment | Pattern catalogue and kick/bass/cymbal alignment untouched (no `pattern_rack`, alignment or `pcore` edit in 11; 10 touched projection/sample path only). | `legacy_guard_tests` 9/169, `hunt_tests` 13/647, `round3_tests` 7/82 unchanged | test | preserved |

## Changes

BUG-SWEEP-11 implementation (5 files, +228/−28 vs `23b7df1`; no `engine/src` or DSP file touched, no instrument added):

- `app/domain/src/randomize.hpp` — `PluginParamInfo` (id/name/value/steps), `PluginTrackParams` (trackId + params),
  `RandomizeOptions::pluginParams` (the engine's catalog, filled by the caller); `soundAxisPluginTracks(doc)` declaration;
  `soundAxisAvailable` contract rewritten to the ratification (role plugins OR legacy built-in; hidden when neither).
- `app/domain/src/randomize.cpp` — the role-part list hoisted to file scope (`kParts`, shared by the groove loops and the
  sound-axis target predicate); `soundAxisAvailable` returns true when a role track is played by a plugin;
  `soundAxisPluginTracks` returns those tracks in part order; the K block adds the plugin-parameter loop (bounded 0..1,
  discrete steps `k/(N-1)`, constant `vst:<id>` curve at tick 0 via `setAutomationPoint`, user curves skipped, the axis's
  own marked curves redrawn, recipe records the values). The legacy built-in K loop and the sole-sound-axis graceful
  no-op are kept.
- `app/desktop/src/MainComponent.cpp` — `randomizeSong` fills `o.pluginParams` from `session_->pluginParams(pluginKey(id))`
  for each `soundAxisPluginTracks` track when the sound axis is on; the menu **hides** "Randomize sound" unless
  `styled && soundAxisAvailable` (was: enabled/disabled "built-in instruments" label).
- `lang/pt-PT.json` — menu key "Randomize sound (built-in instruments)" → "Randomize sound" ("Baralhar o som").
- `tests/domain_tests.cpp` — the interim BUG-SWEEP-11 test updated to the ratified contract (axis available on plugins,
  hidden with no plugins, legacy kept, no-catalog no-op) and the **positive** test added (bounded/stepped parameter
  curves, user curve untouched, projection carries `vst:mode`, one Undo restores exactly, same-seed determinism,
  different-seed variety, re-run redraws its own curves and keeps the user's). No existing check removed or weakened.

BUG-SWEEP-10 (`23b7df1`, inherited — not redo): `samples.cpp/.hpp`, `document.cpp/.hpp`, `engine/src/dsp/sample_host.hpp`,
`engine/src/core/graph.cpp`/`project.cpp/.hpp`/`instruments.cpp`, the sample-host projection and save/reopen, plus the
domain and engine tests. Design and render proof in that commit and re-verified below.

Documentation: this report + the index row. `ESTADO.md` is **not** edited (D72/D80); proposed lines below.

## Verification

Red-first (BUG-SWEEP-11), measured on the **interim built-in-only/disabled path** (HEAD `randomize.cpp` + a
`soundAxisPluginTracks` returning `{}` stub for the new symbol; the new header and tests in place):

```
cmd /c D:\Core-build\flawless-samplehost-evidence\build-target.bat domain_tests   (rc 0)
D:\Core-build\flawless-samplehost\tests\domain_tests.exe "BUG-SWEEP-11"           (rc 1)
  -> 2 testes, 26 verificacoes, 8 falhas
```
Failures: `soundAxisAvailable(d)` false on a plugin song; `soundAxisPluginTracks` 0 vs 3; `!changes.empty()`;
`changed > 0`; `hasVst` (projection); `h.canUndo()`; different-seed variety; re-run `!changes2.empty()`.
Evidence: `11-red-build.log`, `11-red-domain_tests.txt`.

Green (the committed `254a55c` implementation):
```
build-target.bat domain_tests                                                   (rc 0)
domain_tests.exe "BUG-SWEEP-11"   -> 2 testes, 36 verificacoes, 0 falhas         (rc 0)
domain_tests.exe (full)           -> 36 testes, 631 verificacoes, 0 falhas       (rc 0)
```
Evidence: `11-green-build.log`, `11-green-domain_tests.txt`, `11-green-domain-full.txt`.

BUG-SWEEP-10 re-verified (no change to 10):
```
domain_tests.exe "BUG-SWEEP-10"   -> 3 testes, 29 verificacoes, 0 falhas   (render peak ~0.5 from the generated
                                     user file, silence before the note; plugin track refused, kind stays plugin)
engine_tests.exe "sample host"    -> 6 testes, 29 verificacoes, 0 falhas   (file plays at its level; level/velocity
                                     scale; pitched zone follows the key; loop holds/releases; block-size invariant;
                                     missing file silent and reported, song plays on)
```
Evidence: `11-engine-samplehost.txt` (and the inherited `10-*` logs).

Full gate — **fresh build folder**. The inherited build folder had 42/505 objects with `#deps 0` (ninja recorded no
header dependencies — objects compiled before the 2026-11-18 `/showIncludes` code-page fix). The stale
`core-engine.exe` still reported `0.7.0+e035eb7d1a.modified` even though `generated/core_version.h` was current, and
`cli_version` only matches the `0.7.0` prefix so it passed regardless. Per the gate's own remedy ("a build folder
configured before this fix … delete it and run the gate again"), the folder was deleted and re-gated fresh:

```
powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/flawless-samplehost -Jobs 6
  -> VERDE, 100% tests passed out of 20 (merge), 634 s, @ 254a55c clean tree        (rc 0)
powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/flawless-samplehost -Jobs 6 -Desktop
  -> VERDE, 100% tests passed out of 21 (merge + desktop), 44 s, @ 254a55c          (rc 0)
core-engine --version -> core-engine 0.7.0+254a55ce07   (== HEAD 254a55ce07, no .modified)
```
After the fresh build `main.cpp.obj` records `#deps 28`; the only remaining `#deps 0` objects are JUCE C units and
Windows `.rc.res` resources (rc.exe emits no include deps) — every Core C++ object tracks headers.
Evidence: `11-gate-merge-fresh.log`, `11-gate-desktop-fresh.log`, `11-version.txt` (the earlier incremental
`11-gate-merge.log`/`11-gate-desktop.log` are kept but superseded — they ran on the stale folder).

Coverage (14 suite executables, fresh build, all 0 failures): **215 cases / 3215 verifications** vs the builtins tip
207/3134 — **+8 cases / +81 verifications, all additions** (10 positive: domain +1, engine `sample host` +6; 11 positive:
domain +1; the interim 11 test expanded). Per suite: domain 36/631, engine 31/159, legacy_guard 9/169, role 21/216,
midi 14/208, bug 40/377, bitwig 2/25, audit 7/65, audit2 6/80, hunt 13/647, round3 7/82, calls 12/69, menu 7/366,
engine_process 10/121. No suite shrunk.

Desktop self-test (`tests/self-test.txt`): status=Ready, document=68 accepted=68 applied=68, loudest=0.735, null device
48000 Hz block 128; build=engine=`0.7.0+254a55ce07`; steps include play/undo/save+reopen/export, plugin gain moved,
plugin state kept across a crash, "effects: plugins only; no built-in limiter", automation plays.

GATE-LOCK coordination: this stream held the lock from 14:23:07Z through its gates (finished ~14:35Z) and post-gate
verification. At 14:40:58Z `flawless-ux-windows` acquired the lock (this stream's timestamp then looked stale) and began
a gate; this stream's cleanup deleted that lock at ~14:41Z and **restored it immediately** (`flawless-ux-windows
2026-11-18T14:40:58Z`) on seeing ux-windows's live cl/ninja processes — ux-windows's gate was not interrupted. Lesson
recorded: release the lock right after the gate, not after verification.

## Pending items and handoff

- Integration: `flawless-ratified` (30-follow-up) touched `follow.cpp`/`groove.cpp`/`pattern_rack.cpp`/`pattern_tools.cpp`
  and **not** `randomize.cpp` (verified `effe585` stat) — no genuine overlap with this stream's sound-axis region. The
  `kParts` hoist is internal to `randomize.cpp`. Rebase at integration; re-run the 11 tests there. `flawless-l9` (engine)
  and the UX streams are disjoint from these files.
- Test Engineer acceptance: (1) load a styled song with real role plugins, run Randomize sound, confirm plugin parameters
  move within range and one Undo restores; (2) confirm the menu item is absent on a song with no plugins and no legacy
  built-in; (3) confirm a user-automated parameter is never overwritten and re-running redraws only the axis's own curves;
  (4) a sample on a `none` track audibly plays the user's file (render peak > 0) and a plugin track is still refused.
  A live VST3 render proving an audible parameter move is the remaining unmeasured step (no plugin fixture in the repo).
- Residual risk: the engine plays a `vst:<id>` curve only if the loaded plugin exposes that parameter id
  (`graph.cpp:472` `parameterIndex`); a catalog id the plugin does not expose is silently skipped by the engine (the
  document still holds the curve). The UI catalog comes from the same engine query, so ids match in practice.
- No numbered decision needed beyond the two ratifications above. Nothing pushed, no merge, no `git stash`; main untouched.

### Proposed ESTADO.md lines (the scribe writes them; D72/D80 — not edited here)

Decisions / Estado medido (BUG namespace, D75; no D numbers occupied):
- `BUG-SWEEP-10` positive fixed @ `23b7df1` (base `e035eb7`): a sample on an unassigned track becomes a user-sample host
  (`kind="sample"`) that plays the user's file through the engine's audio path — no oscillator/filter/synth; the
  plugin-overwrite guard stays; legacy built-in load preserved. Owner ratified 2026-11-18: «sampler = host de samples do
  utilizador, sem fontes de som próprias.» Source: this report; domain render peak ~0.5, engine `sample host` 6/29.
- `BUG-SWEEP-11` positive fixed @ `254a55c`: the sound axis randomizes the role plugins' parameters within 0..1 on their
  own steps as one undoable edit (one Undo restores); hidden when no plugins and no legacy built-in; user automation never
  taken; legacy built-in K preserved. Owner ratified 2026-11-18: «randomize sobre parâmetros dos plugins dos papéis
  (undoável); escondido quando não há plugins.» Source: this report; red 2/26/8 → green 2/36/0.
- Measured state: full gate **VERDE** merge 20/20 + desktop 21/21 @ `254a55c` (fresh folder), **215 cases / 3215
  verifications / 0 failures** (builtins tip 207/3134, +8/+81 all additions), version `0.7.0+254a55ce07` == HEAD no
  `.modified`, self-test ok (document=68, loudest=0.735). No DSP/instrument added; catalogue, kick/bass/cymbal alignment
  and legacy load preserved; main untouched, nothing pushed, no stash.
- Note (build hygiene): the inherited `flawless-samplehost` build folder had pre-fix `#deps 0` objects and a stale
  `core-engine.exe` (`cli_version` matches only the version prefix); deleted and re-gated fresh per the gate's remedy.

Pending (owner = Diogo): none new — the two positive-target decisions are resolved by the ratifications above. A live
VST3 parameter-move render remains a Test Engineer acceptance step (no plugin fixture in the repo).
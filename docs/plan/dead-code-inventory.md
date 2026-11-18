> Translated from `codigo-morto.pt.md`, a study of commit e7968c8. Line numbers refer to e7968c8 and have drifted since; they are kept as written and not re-anchored to current HEAD.
>
> Verdict legend (1:1, none upgraded or downgraded): **CL** = confirmado por leitura = confirmed by reading · **NV** = não verificado = not verified. The owner's quotes stay verbatim in Portuguese.

> Delivery of the subagent `mapa-morto` (DSH session 2ff54f03, 2026-11-18), extracted from the log without editing. Read-only study of commit e7968c8. In Portuguese; to be translated into English before entering the repo.

# Dead code inventory — Core @ e7968c8 (read-only)

I did not compile or run tests: everything that follows is code reading and grep. Paths relative to `D:\CORE\CORE`. **CL** = confirmed by reading; **NV** = not verified. I wrote no report in `docs/relatorios` (your instruction), although the repo's `AGENTS.md` asks for it. The tree has other people's uncommitted changes (`ESTADO.md`, `docs/relatorios/*`); I did not touch them.

## Findings that change the plan

1. **There is no live native effect.** `engine/src/dsp/effects.cpp:8-10` registers zero effects and `makeEffect` always returns `nullptr`; the «builtin» branch of `graph.cpp:310-329` dies at `if (!e) continue`. (CL)
2. **The sampler path ruins tracks with a plugin.** `takesSamples` (`MainComponent.cpp:3207-3209`) accepts any track named kick/bass/hat…, and `setTrackSample` does `t.instrument = next` (`samples.cpp:1251`). The plugin is swapped for a mute sampler (undone with Undo). (CL)
3. **16 of the 21 templates do not even appear in the app.** They are «base» and `templates.cpp:172` filters them out; only two tests load them (`domain_tests.cpp:114`, `hunt_tests.cpp:176`). They are 7,0 MB in the package. (CL)
4. **The app never sends modulators to the engine.** `toProjection` (`document.cpp:721-777`) does not have the key, and `hunt_tests.cpp:187` asserts it. The domain's 11 modulator/ducking functions have zero callers. (CL)
5. **`core_previews` always renders silence.** It only accepts `builtin` tracks (`pattern_sound.cpp:38`), which the projection converts to `none`; it returns empty at `:133`. (CL)

Leads from the previous auditor that were wrong: `edit.cpp:309-319` (`addTrack`), `EngineSession.cpp:50-53`, `document.cpp:696-758` and `Waveforms.cpp:7-66` are alive — see list (c).

## Lots, from safest to riskiest

### L1 — Symbols without caller or definition · (a) · ≈135 lines, 1 file · gate: no test affected

| Item | Proof (CL) | Dependencies |
|---|---|---|
| `app/domain/src/document.hpp:259-264` — `migrateLegacyBCore`, `migrateLegacyWave` | Declared, with no definition in any `.cpp` | None |
| `document.hpp:265-267` + `edit.cpp:1295-1310` — `upgradeOldVersions` | 0 callers; the comment «parseDocument does it» is false | None |
| `engine/src/dsp/song_clock.hpp` (whole file, ≈60 lines) | `SongClock`/`SongPhase` unused | Remove the include in `effects.hpp:15` |
| `engine/src/dsp/instruments.hpp:12-13` — forward decls `AudioData`, `wmod::Patch` | Unused | None |
| `engine/src/core/project.hpp:81-89,150,165-167` — `SampleZone`, `kMinLoopSeconds`, `Track::samples`, `Track::synth` | Never filled; only read at `project.cpp:613`, always false | Does not touch `project.cpp:524-529` or the note at `:610-611`, which reads the JSON directly |
| `registry.hpp:56,58-59` / `registry.cpp:50,55-56`; `project.hpp:197,201` / `project.cpp:111,113` — `nativeIsInstrument`, `instrumentNames`, `effectNames` | 0 callers (app, tests, CLI) | None |
| `graph.hpp:139-140` `heardParam`; `:188-192` `failInstrumentForTest` | 0 callers, tests included | None |
| `graph.cpp:16-20,358` — `steppedInstrumentParam` | Always returns `false` | None |
| `document.cpp:291,293` — `fromVst` | Reads `extra` before it is assigned: always false | Keep `dropVst` (`:538-541,655`), which cleans up old files |
| False comments: `MainComponent.cpp:4474-4475`, `main.cpp:2`, `measure.hpp:6`, `pattern_sound.hpp:2-6`, `project.hpp:133`, `PatternEditor.h:143-144` | They describe instruments/Electron that no longer exist | — |
| 17 references to `docs/contracts/*.md` in comments | The folder does not exist | — |

### L2 — Domain API without callers · (a) · ≈445 lines · gate: no test affected (0 references in `tests/*.cpp`)

| Item | Proof (CL) |
|---|---|
| `edit.cpp:277-299` (`edit.hpp:47-51`) — `resetInstrumentParams`, `instrumentAtDefaults` | Definition and declaration only |
| `edit.cpp:1472-1477`, `1541-1564`, `1656-1662`, `1924-1935` (`edit.hpp:173-175, 187-190, 201-204, 275-277`) — `addEffectWith`, `addPhraseEndComb`, `setEffectSidechain`, `setSampleLoop` | Same; if called, they write builtin effects or zones that the projection discards |
| `edit.cpp:2210-2336`, except `setSend`/`removeSend` (`edit.hpp:76-107`) — modulators and ducking | Same; the projection does not send modulators |
| `edit.cpp:2344-2414` (`edit.hpp:339-353`) — A/B `TrackSound` | Same |
| `samples.cpp:1365-1431` + `samples.hpp:232-246` — `sampleFitInRegion`, `noteStarts`, `RegionFit`; `browseSampleRoles` (`samples.cpp` ~1090, `samples.hpp:219-221`) | Same |

File format: intact. `Track::modulators` is still read and written (`document.cpp:384-394, 459`).

### L3 — Pattern waveform in the clips · (b) · ≈345 lines, 3 files · gate: `hunt_tests` loses 1 case

| Item | Dependencies to adjust |
|---|---|
| `app/previews/` (`pattern_sound.cpp` 140, `.hpp` 50, `CMakeLists.txt` 7) | `CMakeLists.txt:75,95`; `app/desktop/CMakeLists.txt:26`; `tests/CMakeLists.txt:62` |
| `Waveforms.cpp:68-148`, `Waveforms.h:17,76-107` — `PatternWaves` | `MainComponent.h:489` |
| `MainComponent.cpp:337-356` (`paintClipWave`) and `:456-461` | Only `paintClipNotes` remains, which is already what is seen today |
| `tests/hunt_tests.cpp:24,266-273` — the «U-5» case | Goes out with the structure; coverage reduction to be declared |

Risk: low. Gain: there is no longer an offline render of silence per pattern in old songs, with a 512 MB `AssetStore`. NV: whether `desktop_self_test` looks at waveforms.

### L4 — `core-engine knobcheck` · (b) · 136 lines · gate: no test

`engine/src/app/main.cpp:178-312` and `:673`. The knob loop never runs: `instrumentParams` only returns the `"_"` marker with min = max = 0 (`instruments.cpp:27`), so `to == was` and it skips (`main.cpp:291`); `effectParams` is always empty. It runs without plugins (`Server(nullptr, sr, nullptr)`, `:187`), so it compares silence with silence. It is not in the usage text (`:719`). Only reference: `docs/PLANO-CORE-PLUGINS.md:31`.

### L5 — The 16 «base» templates · (b), depends on question 1 · 16 files, ≈172 000 JSON lines, 7,0 MB

The ones with `source.base = true` (table below). Invisible in the app; the CMake globs (`app/desktop/CMakeLists.txt:55`, `app/installer/CMakeLists.txt:8,13`) follow on their own. By reading, `domain_tests.cpp:113-132` and `hunt_tests.cpp:175-201` stay green with the remaining 5 (`psycore-blacktide` has the 24 modulators that `:191` requires) — NV by execution. `templates.hpp:73-75` attributes to the owner (07/11) the order to keep them «for the generators and the tests»; there is no generator at all in the repo.

### L6 — Menus that lie · (d) · ≈110 lines

| Item | What happens today (CL) |
|---|---|
| `MainComponent.cpp:4491-4496, 4508-4521` — «Core's own instruments» + «Sampler track (empty)» | Creates a `none` track with the role's name (`edit.cpp:319`); plays nothing |
| `MainComponent.cpp:4537, 4581-4586, 4622-4626` — 8 entries in «Replace instrument» | All do the same: `replaceInstrument` ignores the name and puts `none` (`edit.cpp:1600-1611`) |
| `edit.cpp:44-48` (`builtinInstruments`) and the label loop `:316-317` | They only serve these menus |
| `MainComponent.cpp:5304, 5350` + `randomize.cpp:302-323, 684-710` — «Randomize sound (built-in instruments)» | Only rewrites stored parameters of instruments that do not play |

It is already listed as pending in `ESTADO.md:217` (items 16/17: touching menus is UI). Low technical risk.

### L7 — Sampler on note tracks · (d) · ≈650-750 lines

| Item | What happens today (CL) |
|---|---|
| `samples.cpp:1149-1255` — `samplerInstrument`, `setTrackSample` and helpers | Creates `kind = "builtin"`, `name = "sampler"`; the projection sends `none` (`document.cpp:758`) |
| `samples.cpp:1264-1328` — `anchorPack`, `applySuggestedSamples` | Swaps the instrument of every track with a role, plugins included |
| `MainComponent.cpp:3988` | Runs by itself on every «novo a partir de template» (new from template), with `suggestSamples` on by default |
| `samples.cpp:1433-1454` + `MainComponent.cpp:5085-5107` — Library preview | Two `setDocument` to the engine and a note on a mute track |
| `MainComponent.cpp:4264-4319, 3220-3231, 3267-3272, 5043-5083, 5271-5274, 5346-5347` | «Use», dropping a WAV on a note track, «Build-up on a new track», Samples > Use / Shuffle / Suggest |
| `Settings.cpp:269-405, 925-935` — the «Samplers» tab | Configures folders for a sampler that does not play |
| `pcore.cpp:1283-1306` (`fitToSampler`); labels in `LibraryPanel.cpp:486-497` | Depend on the same |

Stays alive: sample → audio track (`MainComponent.cpp:3215`), catalogue, scan, analysis. Format: `instrument.samples` is still read and written (`document.cpp:253-276, 421-430`). `relinkMovedSamples` stays for old songs.

### L8 — The app's own instrument/effect catalogues · (b) + (d) · ≈900 lines + test adjustments

| Item | Dependents (CL) |
|---|---|
| `edit.cpp:15-16, 83-275` — `instrumentCatalogue` (kick, W-Core, hats, snare, drums, sampler, Core Glitch) and accessors | `InspectorPanel.cpp:237-254`; `template_sheet.cpp:46,60-71`; `edit.cpp:1703-1708`; `randomize.cpp:691-699` |
| `edit.cpp:951-1101` — `coreEqInfo`, `coreFxInfo`, `builtinEffects` | `findEffect` in `Mixer.cpp:123`, `InspectorPanel.cpp:172,264`, `edit.cpp:1338,1711`, `template_sheet.cpp:85` |
| `edit.cpp:1103-1288, 1461-1470, 1512-1539, 1566-1575, 1639-1652` — presets, `addEffect`, `setEffectParam` | Only `calls.cpp:775-795` and the tests |
| `InspectorPanel.cpp:210-234` | Describes «Core's own kick: a sine…» and «Filter: cuts the highs» as if they played |
| `PatternEditor.cpp:47-48, 61-63, 364, 370, 376-379` | «Kick Long / Punchy…» lines tied to the keys of the removed Kick |

Gate tests affected: `bug_tests.cpp:140-171` (uses `addEffect("utility"/"eq")`, the labels «Utility» / «Core EQ» and `fx:fx1:gain`) and `role_tests.cpp:427-474` (fills chains with `addEffect("eq")`). It is (d) because it changes what an old song shows: raw names («eq», «kick») and no knob values. It does not touch `project.cpp:524-529` or the format.

Out of scope (AI/bridge), clearly orphaned: `calls.cpp:85-96, 364-382, 511-560, 709-722, 775-795, 812-819`. No function of those families is built: `buildFunctions` (`:237-258`) only gives stutter, voicestutter, mix and open.

### L9 — Native scaffolding in the audio path · (b), high risk · ≈400-500 lines (estimate, NV)

| Item | Proof (CL) |
|---|---|
| `graph.cpp:296, 310-329, 519, 534-544, 580-598, 613-634, 705-729, 803-807, 836-849, 1253-1279, 1291` + `KeyDelayRt` / `keyKept*` in `graph.hpp` | Builtin branch, knob glide and sidechain: `sidechain` never goes ≥ 0 |
| `graph.cpp:663-668`; `graph.hpp:86-98, 215-218`; `engine.hpp:94-95, 207`; `server.cpp:43, 546-548, 767`; `EngineSession.cpp:246-252, 353-357, 504-508, 627-628` | `analysis.watch`: `watchKey` is only written in the dead branch, and nobody calls `EngineSession::watchEffect` |
| `effects.hpp:58`; `graph.cpp:1101-1104`; `server.cpp:743` — `gainReduction` | Always 0; the app does not read the field |
| `graph.hpp:242-253`; `graph.cpp:352-359, 1147-1148, 1203-1217` — instrument parameters | `paramCount` is always 0 (`noParams = true` in the 4 types) |
| `project.cpp:473-507, 649-661`; `effects.hpp:19-39, 69-82`; `registry.*` on the effects side | Empty catalogue |

Affects `engine_tests` (adopt, render) and every render. Requires a before/after comparison, sample by sample, of a project with plugins and audio. I recommend leaving it for last, in its own commit.

### L10 — `demo`, `measure`, startup document · (d)

- `measure.cpp:15-34` (`demoProject`): the kick/wave/hats names become `none`, so the peak is always 0. Kick onset, block independence, `clickRatio` and «live = offline» compare silence with silence. The scheduling error is still real: the `EventLog` records the note-ons (`graph.cpp:1018`).
- Not removable without a replacement: `tests/CMakeLists.txt:9` compiles `measure.cpp` into `engine_tests`, which uses the demo as an «canção antiga» (old song) (`engine_tests.cpp:213-228, 232-239, 305-325`). `cli_demo_render` and `cli_measure` are 2 of the 15 `merge` tests.
- `app/desktop/demo.core-document.json` is the document the app opens with (`Main.cpp:97`): 3 builtin tracks (kick, bass, hats), mute. It goes in the package (`app/desktop/CMakeLists.txt:46-47,80`; `app/installer/CMakeLists.txt:12,18-19`).
- `docs/PLANO-CORE-PLUGINS.md:50` («o motor toca-os», the engine plays them) misleads; the file is marked as historical (`:3`).
- `lang/pt-PT.json`: ≈700 of the 1711 keys without `{}` have no literal in the app code (script; NV key by key). `round3_tests` and `hunt_tests.cpp:275-280` require some keys.
- `MainComponent.cpp:6954-6959`: self-test steps 29/30 are empty. Touching them renumbers the cases; NV.

## Templates, one by one

All: 0 plugin tracks, 0 audio tracks, 0 audio clips, 0 plugin effects. Total: 266 tracks, 1108 own effects, 24 modulators, 41 sampler zones, 21 478 notes. **None produces sound in the current engine** — the instruments become `none` and `pluginChain` (`document.cpp:699-703`) throws the effects away. What they still carry: notes, sections, buses, tempo and 62 volume/pan curves.

| Template | In the app? | Tracks | Instruments `{kind, name}` | Own effects (tracks + master) | Other |
|---|---|---|---|---|---|
| darkpsy | no (base) | 14 | hats×4, kick×3, wave×4, bus×3 | 53 + 8 | — |
| darkpsycore | no (base) | 14 | hats×4, kick×3, wave×4, bus×3 | 52 + 8 | — |
| darkpsycore-dense | no (base) | 14 | hats×4, kick×3, wave×4, bus×3 | 52 + 8 | — |
| darkpsycore-minimal | no (base) | 12 | hats×3, kick×2, wave×4, bus×3 | 44 + 8 | — |
| darkpsycore-syncopated | no (base) | 14 | hats×4, kick×3, wave×4, bus×3 | 52 + 8 | — |
| fullon | no (base) | 13 | hats×4, kick×3, wave×3, bus×3 | 46 + 8 | — |
| hitech | no (base) | 13 | hats×4, kick×3, wave×3, bus×3 | 51 + 9 | — |
| hitech-dense | no (base) | 13 | hats×4, kick×3, wave×3, bus×3 | 51 + 9 | — |
| hitech-minimal | no (base) | 11 | hats×3, kick×2, wave×3, bus×3 | 42 + 9 | — |
| hitech-triplets | no (base) | 13 | hats×4, kick×3, wave×3, bus×3 | 51 + 9 | — |
| progressive | no (base) | 13 | hats×4, kick×3, wave×3, bus×3 | 45 + 8 | — |
| slambient | no (base) | 11 | hats×4, kick×3, wave×1, bus×3 | 31 + 6 | — |
| slambient-dense | no (base) | 11 | hats×4, kick×3, wave×1, bus×3 | 31 + 6 | — |
| slambient-extreme | no (base) | 11 | hats×4, kick×3, wave×1, bus×3 | 31 + 6 | — |
| slambient-minimal | no (base) | 9 | hats×3, kick×2, wave×1, bus×3 | 26 + 6 | — |
| twilight | no (base) | 14 | hats×4, kick×3, wave×4, bus×3 | 51 + 8 | — |
| hitech-ref | **yes** | 14 | hats×4, kick×3, wave×4, bus×3 | 60 + 9 | 3 instrument curves |
| psycore-blacktide | **yes** | 11 | sampler×6, wave×2, bus×3 | 31 + 9 | 24 modulators, 10 zones `D:\Samples\…` |
| psycore-tremolo | **yes** | 15 | hats×2, sampler×6, wave×4, bus×3 | 54 + 9 | 31 zones `D:\Samples\…` |
| slambient-deep | **yes** | 13 | hats×3, kick×4, wave×3, bus×3 | 46 + 6 | 2 instrument curves |
| slambient-ref | **yes** | 13 | hats×3, kick×4, wave×3, bus×3 | 45 + 6 | 2 instrument curves |

Each file also carries a `ficha` of 80–220 kB describing these sounds.

## (c) Looks dead, is alive — do not remove

- **`project.cpp:524-529` and `:700-711`**: conversion of a removed instrument and its automation. Guarded by `engine_tests.cpp:213-228` and `:562-595`. No lot above touches it.
- **`document.cpp:240-277, 419-430`** (read/write `kind = "builtin"`, zones, `synth`) and **`:696-777`** (`toProjection`): it is the filter that keeps old songs opening.
- **`EngineSession.cpp:50-53`** (`setDocument`): main path of any edit.
- **`edit.cpp:311-329`** (`addTrack`): creates the `none` track; used by the app and by dozens of tests. Only the loop `:316-317` is legacy.
- **`edit.cpp:63-81`** (`newTemplateDocument`): fixture of 39 calls in tests and it is the Library's «Basic».
- **`Waveforms.cpp:7-66`** (`WaveformCache`): waveforms of the audio clips and of the Library.
- **Tools W-Core / F-Core / R-Core / D-Core / SUCK / PUM / ROLL** (`MainComponent.cpp:2300-2389`, `role_tools.cpp`, `core_plugins.cpp`): they already work on the roles' plugins; only the names are old.
- **`roles.cpp:258-277`**, **`PatternsWindow.cpp:23-37`** («old, silent: choose a plugin»), `follow.cpp:484-490`, `score.cpp:97-102`, `pattern_rack.cpp:191-213`, `stutter.cpp:117-118`: reads of `builtin` that guess the role of old tracks.
- **`effectMenuEntries` and `removeEffect`** (`edit.cpp:1329-1345, 1613-1629`): the only place where an own effect is removed from an old song (bug of 17/11, with a test).
- **Randomize A–F and J** (`randomize.cpp`): they touch notes and tempo; only the «sound» axis is dead.
- **Native audio still alive in the engine**: fader/pan/master with ramp, buses and sends, latency compensation, audio clips with pitch/freeze (`graph.cpp:1032-1060`, `clip_grains.hpp`), NaN/runaway guard (`graph.cpp:132-138, 1247`), ear guard, loudness, analyzer (`analysis.enable`). Modulators on `gainDb`/`pan` work in the engine (`engine_tests.cpp:401-430`), but without a client in the app.
- **CLI `scan`, `qualify`, `params`** (used by `Plugins.cpp:135,152,180`), `render`, `serve`, `devices`, `--version`, `soak`, `loopcheck`, `matrix`.
- **Engine commands without a client in the app, but hosting ones**: `plugins.missing`, `plugins.failed`, `plugin.retry`, `plugins.versions`. They are not instrument residue.
- **`patterns/`**, `pcore/styles.json`, `lang/` as a resource, `app/sampling` (audio editing), `app/selftest-plugins`.

## Questions for the owner

1. Can the 16 invisible «base» templates go out now, and do the 5 visible ones stay as mute skeletons of notes and arrangement or go out too (Randomize only works on songs born from them)?
2. Does the sampler on note tracks (Use, preview, Samples > Use/Shuffle/Suggest, Settings > Samplers, «Build-up on a new track») go out entirely, with samples staying only as audio clips?
3. Do «Core's own instruments», «Sampler track», the list of 8 in «Replace instrument» (a «No instrument» remains) and «Randomize sound» go out of the menu?
4. Do you accept that old songs start showing the own instruments and effects only by raw name, without knob values or editable curves?
5. `core-engine demo`/`measure` and the «Psy demo» the app opens with: do I rebuild them on generated audio (they keep 2 of the 15 gate tests) or do they go out, and the app opens empty?

The engine's modulators, `track.add` in the JS editor's v1 format and `note.audition` have no client in the app; I left them out of the lots because they are not instrument residue.
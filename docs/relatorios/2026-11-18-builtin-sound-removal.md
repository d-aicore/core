# Built-in sound removal — Core is plugins-only

- **Date:** 2026-11-18 · **Branch:** `remove-builtin-sound` (worktree `.agents/worktrees/remove-builtin-sound`, base `main @ 8c6837e`). Not merged, not pushed. `main` and `ESTADO.md` untouched.
- **Authorization:** owner's direct instruction (this session, 2026-11-18), verbatim: «Remoção de funcionalidades, não limpeza de código morto. O Core não tem sampler, efeitos, filtros, osciladores, instrumentos. Só plugins. Apaga a funcionalidade, quem a chama, e os testes que a testam. Não os adaptes: apaga. O número de testes vai descer. Está autorizado. Não é regressão.» Clarifying criterion: «if it generates or processes sound, it belongs to a plugin, not the DAW. Delete it. That covers sampler, synths, effects, filters, oscillators, modulators, ducking, A/B sound, preview — and every template that uses them.» And: «tracks arent going to be specific any track will be able to lodge a sampler a synth a effect, so testing for bass is wrong.» Templates: «Delete built-in templates». Sampling: «remover tudo depois faz se independenete».
- **Scope kept (does not make sound):** plugin hosting, MIDI, timeline/tempo/sections, routing, undo, file format, pattern catalogue, kick/bass/cymbal alignment (decides notes, not sound), the D:\Samples browser, audio clips + import/export, mixer gain/pan. Tracks are generic plugin hosts (`Instrument.kind` = plugin/none/audio/bus).

## What was deleted (numbers)

Totals: **70 files changed, +227 / −277,533 lines; 25 files deleted.** Code only (app/engine/tests/ci/CMake/AGENTS/demo): **+227 / −3,017**. The rest is the 21 built-in template JSON files (**−274,516 lines**). Engine: **0 changes** (it was already plugins-only since 2026-11-16; it loads any old built-in instrument as a silent "none" track).

| # | Removed | Where | Lines |
| --- | --- | --- | --- |
| 1 | Built-in instrument catalogue + parameter API: `builtinInstruments`, `instrumentCatalogue` (kick / wave W-Core / hats / snare / drums / sampler / samplepath Core Glitch, incl. all W-Core layer/section knobs), `findInstrumentInfo`, `findInstrumentParam`, `instrumentParamValue`, `setInstrumentParam`, `replaceInstrument`, `dropInstrumentMoods` | `edit.cpp` / `edit.hpp` | edit.cpp −671 total (instruments + effects) |
| 2 | Built-in effect catalogue + presets: `builtinEffects` (filter, compressor, limiter, utility, GROWL, METAL, PUNCH, ZAP, GLITCH, PULSE, BODY, Core EQ 82 params, Core FX 95 params), `coreEqInfo`/`coreFxInfo`, ~90 style presets (`effectPresets`/`presetRecipes`), `findEffect`, `findEffectPreset`, `addEffect` (both overloads), `applyEffectPreset`, `setEffectParam` | `edit.cpp` / `edit.hpp` | (in the −671 above) |
| 3 | Sampler: `samplerInstrument`, `setTrackSample`, `withPreviewTrack` (preview), `applySuggestedSamples`, `suggestSamples`, `relinkMovedSamples`, `levelFor`, `roleLoudnessTarget`, `anchorPack`, `sampleRef`, `shownName(index,SampleRef)`, `noteStem`/`multiSampleSet`, sampler level/pluck/late-start constants | `samples.cpp` / `samples.hpp` | samples.cpp −348 |
| 4 | Built-in data model: `Instrument.version/params/samples/synth`, `SampleRef`, `Track.modulators`, `kSamplerVersion`/`kMaxModulators`/`kMaxModSteps`/`kMinSampleLoopSeconds`; built-in parse/write/import branches; modulator cleanups in `removeEffect`/`replaceInstrumentWithPlugin`/`removeTrack` | `document.hpp` −34, `document.cpp` −84, `edit.cpp` | as listed |
| 5 | Sampler-specific alignment branches (`fitToSampler`, sampler root-key pitch) — note alignment kept | `pcore.cpp`, `pattern_rack.cpp`, `pattern_tools.cpp` | small |
| 6 | Desktop UI offering built-in sound: instrument/effect menus + cards, InspectorPanel built-in param editors, sample preview / put-sample-on-track / use-suggested-samples, built-in menu-id bands | `MainComponent.cpp` −261, `InspectorPanel.cpp` −91, `LibraryPanel`, `MenuIds.h`, `Mixer`, others | desktop −473 total |
| 7 | AI-control built-in actions: instrument/effect/sample "setting"/"effect"/"sample" proposal branches, built-in tags/chain reporting | `calls.cpp` −194, `calls.hpp` | control −233 total |
| 8 | Helper built-in sound: randomize "sound" axis, template_sheet built-in param/preset tables + LFO block, roles built-in role-guess branch, stutter/follow/score built-in branches | `randomize`, `template_sheet`, `roles`, `stutter`, `follow`, `score` | (in domain −1472) |
| 9 | **21 built-in style templates** (darkpsy, darkpsycore×4, fullon, hitech×5, progressive, psycore×2, slambient×6, twilight) | `templates/*.core-template.json` | **−274,516** |
| 10 | **app/sampling module** ("Save as sample / Render to sample"): `sampling.cpp/hpp` + CMake | `app/sampling/` (3 files) | −207 |
| 11 | Tests of removed features: **`legacy_guard_tests.cpp` deleted** (−457; built-in/legacy/template guards); hunt **P04** (built-in effects+modulators in templates) and **P08** (sampling) removed (−42); domain built-in-template render test + sampler kind case; role built-in role-guess/conversion cases; bug built-in-effect chain-menu test; menu built-in bands | `tests/` | tests −599 total |

**AGENTS.md amended** (owner's order): dropped the "five visible templates stay" pin, dropped "coverage never decreases" (test count may drop when a feature is removed), and narrowed "do not delete" so it no longer protects these built-in sound features or the built-in templates.

**Preserved-as-independent (not a built-in sound feature):** the no-sound audio-analysis helpers `hasBrokenSamples`/`audibleRange` used by audio-clip *Cut silence* were moved out of the deleted sampling module into `app/desktop/src/AudioClipAnalysis.h` (`core::audio`), per «faz se independenete». The self-test demo `demo.core-document.json` was converted from built-in kick/bass/hats to generic note-tracks (its notes/tempo/mix unchanged; the self-test's synth plugin plays them).

## Gates (measured, Windows MSVC Release, `D:/Core-build/remove-builtin-sound`)

- Full gate `ci/gate.ps1 -Desktop`: **VERDE, 100% — 20/20** (merge 19 + desktop 1).
- **`cli_measure`: PASSED** (allFinite, sample-exact scheduling, block-size independence).
- **`hunt_tests`: 11 cases / 106 checks / 0 failures** (was 13 cases at HEAD; P04/P08 removed — both exercised deleted features; P04's template loop was the bulk of the old check count).
- **`desktop_self_test`: PASSED** — play/BPM/undo/seek/loop, save+reopen, export, engine crash+restart, new-from-template, notes/arrangement/MIDI, audio import/split/trim, "effects: plugins only; no built-in limiter", automation, plugin state across crash, mixer; loudest=0.735.
- Per-suite (cases/checks/0 fails): engine 25/130, domain 29/503, role 21/215, midi 14/208, bug 38/342, bitwig 2/25, audit 7/65, audit2 6/80, hunt 11/106, round3 7/82, calls 12/69, menu 7/301.

## Notes / limits

- No legacy projects exist (owner), so no migration and no byte-identical-reopen proof was required; built-in document data was removed outright.
- Real-device audio not verified (self-test uses the null device, as designed).
- One delegated UI engineer hit its 150-turn limit before final verification; the coordinator completed the last `MainComponent.cpp` cleanup (Cut-silence dependency) and fixed two build/test fallout items (a samples.cpp namespace boundary, role_tests built-in cases). No auto-retry of failed operations: each failure was diagnosed and corrected.

## Proposed ESTADO.md lines (for the ledger scribe only — D72/D80)

Under Decisões (owner verbatim, this session 2026-11-18): the feature-removal instruction and criterion quoted above; tracks are generic plugin hosts; built-in templates deleted; test-count drop authorized.

Under Estado medido:
> Built-in sound removed on branch `remove-builtin-sound` (base `8c6837e`, not merged): 70 files, +227/−277,533 (code +227/−3,017; 21 templates −274,516; app/sampling −207; legacy_guard_tests deleted). Gate VERDE 20/20 (merge 19 + desktop 1); cli_measure green; hunt 11/106/0; self-test ok. Engine untouched (already plugins-only).

Pendência (owner = Diogo): ratify a D-number for this removal and merge `remove-builtin-sound` when satisfied; the independent audio-analysis header and the plugins-only demo are the only surviving pieces of the old sampling/demo surface.
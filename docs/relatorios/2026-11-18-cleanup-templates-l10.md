# Report — 2026-11-18 — Cleanup: templates with effects + L10 silence gap

## What was measured

- Repository / branch / SHA: `d-aicore/core`, worktree `flawless-clean`, base **`f6b86dc878`** (integrated main),
  final product tip **`4409418645`**. Main checkout `D:\CORE\CORE` (`ed4642e`) **not touched**.
- Scope and authorization: owner «limpa tudo» (full cleanup) and, specifically, «remove templates com efeitos»
  (remove templates that contain effects). This dispatch covers (1) templates with builtin effects, (2) the hidden
  base templates, (3) dead-code inventory **L10** (amplitude test + conversionNotes telemetry + empty app).
  **L9 (graph.cpp / effects.cpp) is a separate later dispatch and was NOT touched.** No native DSP resurrected.
- Environment: Windows 11 x64 (10.0.26300), MSVC VS 18 BuildTools, Ninja, Release, JUCE 9.0.5 local
  (`D:/Core-deps/JUCE-9.0.5`), build `D:/Core-build/flawless-clean`, evidence `D:/Core-build/flawless-clean-evidence/`.
  Gate under the machine-wide `GATE-LOCK` (acquired `flawless-clean 2026-11-18T13:32:42Z`, released after).
- Tests executed: full `ci/gate.ps1` **merge** and **-Desktop**; verbose `ctest -L merge` for per-suite counts;
  baseline counts measured in the idle base build `D:/Core-build/integration` (`core-engine 0.7.0+f6b86dc878`);
  L10 amplitude **red-first** then green in a lightweight `engine_tests` build (`CORE_BUILD_APP=OFF`);
  conversionNotes telemetry proven with `core-engine render` / `measure` on a legacy builtin song and the demo.
- Limitations: the desktop self-test is the app's own end-to-end test on the **null** device (not a real WASAPI
  listening test). The demo/measure note-scheduling sweep is now vacuous (the demo is audio, no note-ons); that
  live coverage is preserved by a new dedicated engine test (see Coverage). Real-plugin load was not exercised
  beyond the self-test's test synth and `cli_plugin_crash_render`.

## Results

| ID | Problem / result | Evidence (SHA, file:line or run) | Classification | State |
| --- | --- | --- | --- | --- |
| TPL | All 21 shipped templates carry builtin effects (`"type":"builtin"`), 0 plugin effects; Core has no builtins since 2026-11-16, so each projects to silent `none` tracks — dead by construction. Owner override removes even the 5 visible ones. | `evidence/templates-analysis.json` (per-file counts); `project.cpp:522-527` conversion; commits `9d3c6b0`, `2906309` | confirmed in code + test | removed |
| L10a | No gate test checked that a render sounds; the builtin demo rendered peak 0 and the gate compared silence with silence. | RED `evidence/l10-amplitude-RED.txt` (3 failed checks, peak 0); GREEN `l10-engine-tests-GREEN.txt`; `engine_tests.cpp` amplitude tests | proven by test | fixed |
| L10b | conversionNotes carried (`project.cpp:365`, `project.hpp:170`) but measure/render did not report it. | `evidence/telemetry-legacy-stderr.txt` + `telemetry-legacy-stdout.json` (3 notes, peak 0); `main.cpp` render, `measure.cpp` runMeasure | proven by test | fixed |
| L10c | App autoloaded the dead builtin demo; engine demo/measure built on removed builtins. | `Main.cpp` opens `newEmptyDocument()`; `measure.cpp` demo on generated audio; demo peak 0.7434 | confirmed in code + test | fixed |
| GATE | Merge + desktop green, version == HEAD, no `.modified`. | `gate-merge-resumo.txt` 20/20; `gate-desktop-resumo.txt` 21/21; `core-engine --version` = `0.7.0+4409418645` | proven by test | validated |

## Templates removed / kept (effects check)

Every file was parsed and its effects counted (`document.tracks[].effects` + `document.master.effects`).
**All 21 have builtinEffects > 0 and pluginEffects = 0 → all removed. None was effect-free, so none kept.**
Total builtin effects removed: **1108** (matches the dead-code inventory). The `patterns/` catalogue is separate
and preserved.

| Template | In app (base flag) | Tracks | Builtin effects | Plugin effects | Modulators | Decision |
| --- | --- | --- | --- | --- | --- | --- |
| darkpsy | hidden (base) | 14 | 61 | 0 | 0 | removed |
| darkpsycore | hidden (base) | 14 | 60 | 0 | 0 | removed |
| darkpsycore-dense | hidden (base) | 14 | 60 | 0 | 0 | removed |
| darkpsycore-minimal | hidden (base) | 12 | 52 | 0 | 0 | removed |
| darkpsycore-syncopated | hidden (base) | 14 | 60 | 0 | 0 | removed |
| fullon | hidden (base) | 13 | 54 | 0 | 0 | removed |
| hitech | hidden (base) | 13 | 60 | 0 | 0 | removed |
| hitech-dense | hidden (base) | 13 | 60 | 0 | 0 | removed |
| hitech-minimal | hidden (base) | 11 | 51 | 0 | 0 | removed |
| hitech-triplets | hidden (base) | 13 | 60 | 0 | 0 | removed |
| progressive | hidden (base) | 13 | 53 | 0 | 0 | removed |
| slambient | hidden (base) | 11 | 37 | 0 | 0 | removed |
| slambient-dense | hidden (base) | 11 | 37 | 0 | 0 | removed |
| slambient-extreme | hidden (base) | 11 | 37 | 0 | 0 | removed |
| slambient-minimal | hidden (base) | 9 | 32 | 0 | 0 | removed |
| twilight | hidden (base) | 14 | 59 | 0 | 0 | removed |
| hitech-ref | **visible** | 14 | 69 | 0 | 0 | removed (owner override) |
| psycore-blacktide | **visible** | 11 | 40 | 0 | 24 | removed (owner override) |
| psycore-tremolo | **visible** | 15 | 63 | 0 | 0 | removed (owner override) |
| slambient-deep | **visible** | 13 | 52 | 0 | 0 | removed (owner override) |
| slambient-ref | **visible** | 13 | 51 | 0 | 0 | removed (owner override) |

- **Hidden base templates removed (scope 2 / inventory L5): 16** (the `source.base=true` rows), commit `9d3c6b0`.
- **Visible templates removed (scope 1, owner override): 5**, commit `2906309`. The keep-the-visible-5 rule is
  overridden by «remove templates com efeitos» because all five contain builtin effects.
- Kept: the `patterns/` catalogue, `pcore/styles.json` (7 styles / 7 aligns), the kick/bass/cymbal alignment,
  user projects/audio, legacy project load (projection-none migration), and the Library's generated **"Basic"**
  template (`newTemplateDocument`, `MainComponent.cpp:4996`) — none of these is a shipped template file.

## Consumer maps (every removal)

Shipped template **files** (`templates/*.core-template.json`) — the only consumers of the files themselves:

| Consumer | Reference | Action |
| --- | --- | --- |
| `app/domain/src/templates.cpp` `loadTemplates`/`readFolder` | reads the folder dynamically | unchanged (generic; missing folder = empty). Live user-template save/load/delete/restore kept |
| `app/desktop/CMakeLists.txt` | glob `../../templates/*`, copy + package templates | removed glob/copy/package; stale `templates` dir still removed from app/package; lang/patterns/pcore kept |
| `app/installer/CMakeLists.txt` | glob, `copy_directory`, tar list | removed templates from all three; lang/patterns/pcore kept |
| `app/installer/Setup.cpp:6` | layout comment | comment updated |
| `tests/CMakeLists.txt` | `CORE_TEMPLATES_DIR` (domain/legacy_guard/hunt) | definition removed (no longer used) |
| `tests/domain_tests.cpp` "templates: todos abrem" | `loadTemplates(CORE_TEMPLATES_DIR,…)` over 21 files | rewritten: live `saveTemplate`/`loadTemplates` user round-trip → project → render finite (1 generated template) |
| `tests/hunt_tests.cpp` P04 | `loadTemplates(CORE_TEMPLATES_DIR,…)` over 21 files | rewritten: same live projection assertions on a constructed legacy song (builtin effects + modulators) |
| `tests/legacy_guard_tests.cpp` "five visible templates" | `loadTemplates(…,false)`, `CHECK_EQ(size,5)` | removed (deleted feature); header comment updated |
| `tests/cli_package_stale.cmake` | templates as a packaged resource + stale seed | templates dropped from lists; live BUG-SWEEP-14 coverage for lang/patterns/pcore unchanged |
| `app/desktop/src/MainComponent.cpp` menus / `builtinTemplatesFolder()` | generic loader, no hardcoded ids | unchanged; lists only user templates now |

**Not consumers of the files (free-text style names, preserved):** `calls.cpp:278-281`, `BuildupPanel.cpp:21-22`,
`edit.cpp` F-Core style racks, `score.cpp` shapes, `samples.cpp`/`randomize.cpp`/`pcore.cpp` style words,
`bug_tests.cpp:1004` / `legacy_guard_tests.cpp:316` pcore style/align "fullon", `domain_tests.cpp:752` /
`hunt_tests.cpp:299` `extra["template"]["style"]` sample-suggestion words. `template_sheet.cpp` is generic.
`PatternsWindow` rack templates (`PatternTemplates`, user folder) are a different feature, untouched.

L10 consumers: `measure.cpp` `demoProject` (engine_tests, measure, `core-engine demo`/`render`);
`Main.cpp` startup document (normal launch vs `--self-test`); `main.cpp` render result; `server.cpp:480`
already returns `converted` (A-009 guards it). The `--self-test` path keeps `demo.core-document.json` as its
fixture (its note tracks play via the test synth, `selfTestSynth`), so the desktop self-test is unchanged.

## L10 — amplitude red → green, telemetry, empty app

**(a) Amplitude (red-first, measured):**
- RED on the base silent builtin demo (`engine_tests` lightweight build, old `demoProject`):
  `FAIL render: the demo sounds (amplitude > 0), not silence-by-conversion` — 3 failed checks
  (`!allSilencedByConversion`, `r.peak > 0.0f`, "the demo rendered silence (peak 0)"); exit 1.
  The general sound-out test passed even at red: `ok render: generated audio and the test synth sound out`.
  Evidence: `evidence/l10-amplitude-RED.txt`.
- GREEN after the demo rebuild: `engine_tests` **28 tests / 139 verifications / 0 failures**; the demo test passes.
  Demo render peak **0 → 0.7434** (`evidence/telemetry-demo-stdout.json`). The test fails when conversion notes
  silence **every** track (instrument `none` + loadNotes) and peak is 0.
- General sound-out: generated sine clip (`signals.hpp`) peak > 0 and plugin/test-synth stand-in peak > 0;
  the real VST3 test synth's sound-out is asserted by `cli_plugin_crash_render` (peak ≥ 0.05) and now
  `cli_demo_render` (peak > 0). `cli_measure` asserts `sweep.summary.maxPeak > 0` (measured 0.7434).
- Scheduling coverage preserved: new `render: note-ons land on the exact grid (scheduling accuracy)` (max error
  < 1 sample on a plugin note track) replaces the grid-timing the audio demo sweep no longer measures.

**(b) conversionNotes telemetry (proven):** `core-engine render` on a legacy builtin song
(`evidence/legacy-builtin-song.json`) → peak **0.0**, stderr prints
`conversion: "kick: Core's own 'kick' is not played any more: the track waits for a plugin"` (and bass/hats),
and `result.conversionNotes` carries all three. `runMeasure` reports `conversionNotes` in JSON + stderr
(the audio demo's is `[]`). The loadNotes "waits for a plugin" message and legacy load are unchanged.
Evidence: `telemetry-legacy-stderr.txt`, `telemetry-legacy-stdout.json`, `telemetry-measure.json`.

**(c) Empty app + rebuilt demo:** normal launch opens `newEmptyDocument()` (no dead builtin demo autoload);
`demo.core-document.json` remains only as the `--self-test` fixture. The engine demo/measure is rebuilt on a
deterministic **generated-audio** backing track (an audio fixture written once to a temp folder, loaded through
an `AssetStore` in every render path) — not the removed builtins, not engine DSP. Null renders bit-exact:
`blockIndependence` maxDifference **0.0** at blocks 1/32/64/128/333/1024 (`telemetry-measure.json`).

## Changes

Implementation:
- `9d3c6b0` — remove the 16 hidden base templates (data only; dynamic loader, tests still find the 5 visible).
- `2906309` — remove the 5 visible templates + consumer updates (desktop/installer CMake, Setup.cpp comment,
  tests/CMakeLists `CORE_TEMPLATES_DIR`, domain/hunt/legacy_guard tests, cli_package_stale).
- `4409418` — L10: `engine/src/app/measure.cpp` (+`.hpp`) generated-audio demo + AssetStore + conversionNotes;
  `engine/src/app/main.cpp` render telemetry; `app/desktop/src/Main.cpp` empty launch;
  `tests/engine_tests.cpp` amplitude + scheduling tests and retargeted legacy/silent fixtures;
  `tests/cli_demo_render.cmake` / `cli_measure.cmake` peak assertions (messages converted to English).
Documentation: this report + index row. Proposal: ESTADO.md lines below (not written — scribe only, D72/D80).

## Verification

Commands and exit codes:
- `ci/gate.ps1 -Build D:/Core-build/flawless-clean -Jobs 6` → **VERDE**, rc 0, 380 s, **20/20** merge @ `4409418`.
- `ci/gate.ps1 … -Desktop` → **VERDE**, rc 0, 38 s, **21/21** (merge + desktop); `desktop_self_test` Passed 21.14 s,
  report `ok … loudest=0.735 … build=0.7.0+4409418645` (51 logged steps, null device).
- `core-engine --version` = `0.7.0+4409418645` == HEAD `4409418645`, **no `.modified`** (clean tree).
- `ctest -V -L merge` → rc 0; per-suite counts below.
- L10 red: lightweight `engine_tests` (old demo) exit 1, 3 failures; green: exit 0, 28/139/0.
- Telemetry: `core-engine render legacy-builtin-song.json` exit 0 (peak 0, 3 conversionNotes);
  `core-engine demo 145` + render exit 0 (peak 0.7434, conversionNotes []); `core-engine measure` exit 0
  (maxPeak 0.7434, allFinite true, blockIndependence all 0.0).

Counts per scope (merge), baseline measured @ `f6b86dc878` in `D:/Core-build/integration` vs final @ `4409418`:

| Suite | Baseline cases/verifs | Final cases/verifs | Delta cause |
| --- | --- | --- | --- |
| engine_tests | 25 / 130 | 28 / 139 | +3 tests (demo amplitude, generated/synth amplitude, scheduling); retargeted legacy/silent fixtures |
| domain_tests | 30 / 525 | 30 / 505 | −20 verifs: "todos abrem" loop over 21 deleted templates → 1 user round-trip |
| legacy_guard_tests | 9 / 169 | 8 / 158 | −1 case / −11 verifs: five-visible-template test removed (deleted feature) |
| hunt_tests | 13 / 647 | 13 / 121 | −526 verifs: P04 loop over 21 templates / 266 tracks → 1 legacy song / 3 tracks |
| role_tests | 21 / 216 | 21 / 216 | unchanged |
| midi_tests | 14 / 208 | 14 / 208 | unchanged |
| bug_tests | 39 / 356 | 39 / 356 | unchanged |
| bitwig_tests | 2 / 25 | 2 / 25 | unchanged |
| audit_tests | 7 / 65 | 7 / 65 | unchanged |
| audit2_tests | 6 / 80 | 6 / 80 | unchanged |
| round3_tests | 7 / 82 | 7 / 82 | unchanged |
| calls_tests | 12 / 69 | 12 / 69 | unchanged |
| menu_tests | 7 / 366 | 7 / 366 | unchanged |
| engine_process_tests | 10 / 121 | 10 / 121 | unchanged |
| **merge total** | **202 / 3059** | **204 / 2511** | **+2 cases, −548 verifs** |

**Coverage statement (no live-behavior shrink):** the −548 verifications are entirely the deleted shipped-template
fixtures (21 files / 266 tracks) that hunt P04 (−526), domain "todos abrem" (−20) and legacy_guard five-visible
(−11) looped over; engine added +9. The **case count rose** (+3 engine, −1 legacy_guard). The live behaviors those
loops exercised remain covered: builtin effects/modulators stored-but-silenced in projection (hunt P04 on a legacy
song + legacy_guard `legacySongJson`/L2), template→engine render (domain round-trip), legacy load + null render +
pattern catalogue + kick/bass/cymbal alignment (legacy_guard, unchanged), note-on grid scheduling (new engine test).
Removing tests of a deleted feature is authorized (owner: «se tivermos de alterar os testes alteramos e removemos
pq nao estao a provar nada, se nao existe»); no live-behavior test was weakened, re-targeted to hide a defect, or
skipped. The 20 ctest merge tests and 21 full tests are all present and green.

## Proposed ESTADO.md lines (proposal only — the scribe writes ESTADO.md, D72/D80)

To enter under **Estado medido** (with citation/source):
- «Cleanup `flawless-clean` @ `4409418645` (base `f6b86dc878`): all 21 shipped templates removed — 16 hidden bases
  (`9d3c6b0`) + 5 visible (`2906309`), every one with builtin effects (1108 total, 0 plugin effects), dead by
  construction since the builtins left on 2026-11-16. Owner: "remove templates com efeitos" (overrides keeping the
  visible 5). patterns/ catalogue, pcore styles/aligns, kick/bass/cymbal alignment, user projects/audio and legacy
  load preserved. Gate VERDE merge 20/20 + desktop 21/21, version==sha no .modified; 204 cases / 2511 verifs / 0
  failures (baseline 202/3059; −548 verifs = deleted-template loops, +2 cases; no live coverage shrink).»
  Source: this report; owner instruction via the cleanup dispatch (DSH coordinator session
  `session-2ff54f03-8344-40bb-8cb4-54df0601eb69`).
- «L10 silence gap closed (`4409418`): amplitude gate test red on the silent builtin demo (peak 0) → green on the
  generated-audio demo (peak 0.7434); conversionNotes reported by render/measure in JSON + stderr (legacy song:
  3 notes, peak 0); app opens empty, demo/measure rebuilt on generated audio (not removed builtins, no DSP).
  Null renders bit-exact (blockIndependence 0.0). graph.cpp/effects.cpp (L9) untouched.» Source: this report.

To enter under **Decisões** (ratification needed — proposal, not a new D number):
- The owner's «remove templates com efeitos» is the acting instruction for this cleanup; it overrides the
  2026-11-07/18 keep-the-visible-5 / keep-as-bases notes for templates that contain builtin effects. Ratify with
  verbatim quote + date + source (D48/D50). The 21 removed files are recoverable from git history (`f6b86dc`).

Open pendência:
- L9 (native scaffolding in graph.cpp/effects.cpp) remains for its own dispatch — not touched here.
- The demo/measure note-scheduling sweep is vacuous on the audio demo; grid scheduling is guarded by the new
  engine test. If a note-based sounding demo is wanted later (test synth), that is a separate decision.
- Ledger scribe to write the proposed lines; this delivery does not edit ESTADO.md.

## Confirmations

- Main checkout `D:\CORE\CORE` untouched (worktree-only work at `f6b86dc` → `4409418`).
- Nothing pushed, no merge, **no `git stash`**. Commits per concern: `9d3c6b0` (hidden), `2906309` (visible +
  consumers), `4409418` (L10); docs commit follows.
- Preserved: patterns/ catalogue, kick/bass/cymbal alignment, user projects/audio, legacy project load
  (projection-none migration) and the loadNotes "waits for a plugin" message.
- No native DSP resurrected; `graph.cpp` / `effects.cpp` / `instruments.cpp` untouched (**L9 NOT touched**).
- Evidence: `D:/Core-build/flawless-clean-evidence/` (templates-analysis.json, l10-amplitude-RED.txt,
  l10-engine-tests-GREEN.txt, gate-merge/desktop logs + resumos, merge-verbose.txt, telemetry-*.json/txt,
  legacy-builtin-song.json, self-test report). Scratch red/green build: `D:/Core-build/flawless-clean-redgreen`.
# S2+S3 — missing-plugin notice + relink wiring; invalid plugin state reported and preserved

- Date: 2026-11-20
- Snapshot: `main` @ `a9a038430d781959829f8510067ce59afffaad62` + uncommitted round-1 fixes (S1/S4/S5/S11), all preserved. **Nothing committed** (no verbatim owner authorization).
- Owner contract (S-round item 5, data priority, verbatim):
  - S2: «O motor já fornece o motivo por plugin e a app guarda-o no snapshot… falta de aviso útil e uma função relinkPlugins sem chamadas… Prova primeiro a experiência de abrir um projeto afetado. O relink tem de identificar inequivocamente a instância, preservar o estado e a automação e manter o projeto recuperável quando não há correspondência.»
  - S3: «um estado inválido pode ser ignorado e deixar o plugin com os valores de fábrica. Reproduz com um estado deliberadamente inválido, confirma onde é perdido e prova que a app identifica a instância e conserva o estado original para recuperação.»
- Build: `D:\Core-build\s2s3\build` — Debug, Ninja, MSVC (VS 18 BuildTools, `VSLANG=1033`, `chcp 65001`), `CORE_BUILD_TESTS=ON`, `CORE_BUILD_APP=ON`, `FETCHCONTENT_SOURCE_DIR_JUCE=D:/Core-deps/JUCE-9.0.5` (D113 patch verified present by CMake). Full build ~15 min, rc 0.
- Working notes: `D:\Core-build\coordinator-round2\s2s3-notes.md`. Evidence: [evidencia/2026-11-20-s2s3-state-recovery/](evidencia/2026-11-20-s2s3-state-recovery/) (7 files).

## 1. Red measured (UNFIXED tree, own build)

New permanent suite `tests/state_recovery_tests.cpp` (serve harness as `engine_process_tests`: real `core-engine serve` through the app's own `EngineClient`, null backend; plus the app's own `EngineSession` — the snapshot the window reads — and the app's document save/reopen path). Run on the unfixed tree: **7 tests, 68 checks, 16 failures, rc 1** (`red-run.txt`).

| # | Case | Unfixed result | Evidence |
|---|------|----------------|----------|
| 1 | S2 missing-plugin open: engine answer + session notice | **FAIL** (:155/:156) | Engine answer HAS `missing:[{key:"track:vst", plugin:"No Such Plugin", why:"not found in Z:/…"}]` (checks pass). `EngineSession` stores it in `snap_.missingPlugins` but `snapshot().message` is the device line — contains neither the name nor the why. The user sees nothing (only the disabled Open… menu, MainComponent.cpp:4687/4698/5300). |
| 2a | relink semantics (state+automation, duplicates, no-match) | ok — characterization | `relinkPlugins` is correct but **dead code**: `git grep relinkPlugins` → only `plugin_refs.cpp:63` (definition) + `plugin_refs.hpp:23` (declaration), **zero callers** in app/engine/tests. |
| 2b | stale reference stays missing without relink | ok — measurement | Plugin available at its real path; a reference with another machine's path/hash loads `missing` and stays `missing` on reload (the engine has no relink path). `relinkPlugins(d,{scan})` → 1; reload → `missing:[]`; `plugins.states[key]` == the original blob bit-identical. Recovery exists; nothing called it. |
| 3 | S3 invalid Base64 state (`"%%% not base64 %%%"`) | **FAIL** (:292–310) | `missing:[]` — plugin loads at factory defaults, no why. `plugins.states` CONTAINS the key (factory blob). `doc::applyPluginStates` changed=[key]: the original is REPLACED; `saveFile`+`openFile` persists the factory blob. |
| 4 | S3 valid-Base64 garbage blob (64 bytes, not a VST3 state) | **FAIL** (:292–310) | Same path; original `AwoRGB8mLTQ7QklQV15lbHN6gYiPlp2kq7K5wMfO1dzj6vH4/wYNFBsiKTA3PkVMU1phaG92fYSLkpmgp661vA==` replaced by the same factory blob. |
| 5 | healthy-state control | ok | Healthy state round-trips bit-identical (engine capture and save/reopen) — byte stability confirmed before the fix. |

**Where the original state is lost (S3 core, confirmed by execution):**
`JuceHost::create` ignores the invalid blob (`juce_host.cpp:347-352`: Base64 failure has no else; `setStateInformation` is void; the VST3 wrapper itself skips a non-XML blob in silence — `juce_VST3PluginFormatImpl.h:2896`). The instance loads "healthy" at factory defaults → `plugins.states` → `JuceHost::captureStates` (`juce_host.cpp:472-493`) hands the factory blob to the app → `MainComponent::fetchPluginStates` → `adoptPluginStates` (`MainComponent.cpp:3967-3988`, called by `writeTo` on **Save**, and before export / plugin replacement) → **`doc::applyPluginStates` (`edit.cpp:1053-1074`) overwrites `plugin["state"]` — the original is lost here** → `doc::saveFile` persists it. The engine's stored copy is also overwritten via `pushPluginStates` → `server.cpp:509-517`.

Factory blob decoded (both cases): base64 `VkMyIbwAAAA8P3htbCB2ZXJzaW9uPSIxLjAi…` → `VC2!` magic + `<?xml version="1.0" encoding="UTF-8"?> <VST3PluginState><IComponent>64…JU0PEAkboYWXzUFQgQWX.DP.Bk…</IComponent></VST3PluginState>` — JUCE binary XML at factory gain.

## 2. Fix (minimal, data first)

**S3 — `engine/src/plugins/juce_host.cpp` `create()` (:347-366, +16/−2):** an invalid state refuses the instance with a why, instead of factory defaults in silence:
- Base64 decode failure → `why="invalid state (not base64)"`, return nullptr.
- Decoded blob that is not JUCE binary XML (`juce::AudioProcessor::getXmlFromBinary` — the VST3 wrapper's own gate; this host loads VST3 only) → `why="invalid state (not a VST3 state blob)"`, return nullptr.
- The refused instance rides the EXISTING missing channel: `graph.cpp:322` `missingPlugins_ {key,plugin,why}` (unchanged) → `project.load` answer `missing` → the app announces it (S2) and **S1's export refusal inherits it** (`missingNeededPlugins`). It is never live, so `captureStates` never hands factory state to the document: **the original blob survives the live-state refresh, save and reopen**. The carried-state path (live instance handoff at another sample rate) is untouched. `server.cpp` and `graph.cpp` NOT changed (S1 regions preserved).

**S2 notice — `app/desktop/src/EngineSession.h/.cpp` (+22/−2):** shared helper `missingPluginsNotice(map)` (count + "name: why" per plugin, key order); `sync()` sets `snap_.message` from it after the missingAssets block (mirrors the :650-652 pattern; the plugin notice wins when both happen). `MainComponent.cpp` timerCallback (:6764-6769): the status line shows the notice built from `snap_.missingPlugins` — it survives the device line that overwrites `snap_.message` at engine start (`connect()` runs `openDevice` after the first `sync`), outranks transient notices, and a refused document still speaks first. No UI redesign; the existing `message_` label is the surface.

**S2 relink — `app/desktop/src/MainComponent.cpp` (+44/−1 total):** two callers of the existing, already-safe `relinkPlugins` (match by format+name+uid; two scanned copies → no guess; writes ONLY id+file — state, preset and automation stay; `plugin_refs.cpp/.hpp` NOT changed):
- `open()` (:4106-4121): before the first load, relink against the catalog (the last scan, persisted in `%APPDATA%/Core/plugins.json`). No live-state fetch here — the document just came from disk and the engine may still hold the previous song's plugins (`releasePlugins` is queued). One undo step ("Relink plugins") brings the old references back; the first sync carries the relinked document.
- `scanPlugins()` completion (:3960-3980): probe relink → `fetchPluginStates()` (live edits into the document FIRST, so the relink never drops them) → relink again → `history_.commit` → `documentChanged(false)` → notice + log.
- No match / two copies: references stay as they are — missing, announced, recoverable (the user picks; a later rescan relinks what it can).

## 3. Green measured (fixed tree, same suite)

**7 tests, 68 checks, 0 failures, rc 0** (`green-run.txt`):

| # | Case | Fixed result |
|---|------|--------------|
| 1 | missing-plugin notice | ok — engine answer name+why AND `snapshot().message` contains "No Such Plugin" + "not found" |
| 2a | relink semantics | ok — 3 references relinked (2 track instances + master effect), each keeps its OWN state, automation byte-identical; already-local → 0 |
| 2b | no-match / duplicates recoverable; stale-ref recovery | ok — empty scan / wrong uid / two copies → 0 changes, document byte-identical, original state kept; stale ref stays missing without relink, relink recovers it with its state bit-identical |
| 3 | invalid Base64 | ok — `missing` names the instance with `why="invalid state (not base64)"`; `plugins.states` has no key; adoption changes nothing; save/reopen keeps `"%%% not base64 %%%"` byte-for-byte |
| 4 | garbage blob | ok — `why="invalid state (not a VST3 state blob)"`; original preserved across refresh/save/reopen |
| 5 | healthy control | ok — still bit-identical (no regression) |

**Full `ctest -L merge` (Debug, own build): 24/25 passed, 84 s** (`merge-run.txt`). Per-suite: engine_tests, wav_float_tests, domain_tests, modules_tests, role_tests, midi_tests, bug_tests, bitwig_tests, audit_tests, audit2_tests, export_plugin_failure_tests, hunt_tests, round3_tests, calls_tests, menu_tests, window_tests, cli_version, cli_demo_render, cli_measure, cli_unicode, cli_package_stale, installer_samples, cli_plugin_crash_render, **state_recovery_tests (0.91 s)** — all Passed.

**ENV-03 (separate, pre-existing, NOT conflated):** `engine_process_tests` fails ONLY its first crash case at `engine_process_tests.cpp:159` — "The engine exited (code 259)" instead of 0 (Debug live-playback exit; the other 14 cases / 256 checks pass). **Proven pre-existing**: the identical failure (same line, same 259) reproduces on the round-1 `s1-red` build (`D:\Core-build\s1-red\build`, a9a0384 + S1, WITHOUT the S2/S3 changes) — `env03-preexisting-s1-red.txt`. The crash case loads plugins with no state: the S3 code path is not exercised. 24/24 excluding ENV-03. The round-1 integrated Release gate passed 25/25 (ENV-03 is Debug-only; 3A continues).

## 4. Files changed

| File | Change | Lines (post-fix) |
|---|---|---|
| `engine/src/plugins/juce_host.cpp` | S3 invalid-state refusal (Base64 + VST3-XML gate) | :347-366 |
| `app/desktop/src/EngineSession.h` | `missingPluginsNotice` declaration; statusText comment → English (touched line) | :232-236 |
| `app/desktop/src/EngineSession.cpp` | helper definition; sync() notice | :45-56, :665-670 |
| `app/desktop/src/MainComponent.cpp` | include; relink at open + scan completion; status-line precedence | :34, :3960-3980, :4106-4121, :6764-6769 |
| `tests/state_recovery_tests.cpp` | NEW permanent regression (7 cases) | whole file |
| `tests/CMakeLists.txt` | additive registration block (the three existing registration blocks preserved) | :210-230 |

NOT changed: `server.cpp` (the missing channel already carries why; `plugins.states` already skips non-live instances — S1 render region untouched), `graph.cpp/.hpp` (S1 accessors untouched), `plugin_refs.cpp/.hpp` (semantics already safe), all other round-1 files.

## 5. Limitations (declared)

- **UI click-through NOT performed**: headless serve + `EngineSession` snapshot path + build only. The window wiring (status-line precedence, relink at open/scan, undo step) is verified by code and compilation, not by clicks. The desktop self-test was not run in this build (label `desktop`, opens a window).
- **Residual undetectable case (S3)**: a blob that IS valid JUCE binary XML but carries no `IComponent`/`IEditController` state parses, and the VST3 wrapper applies it as a no-op (`setStateInformation` is void) — factory defaults would still be captured on save. Detecting it would require format-internal validation beyond the minimal fix; declared, not fixed.
- Invalid-state instances are silent until recovered (relink/replace/undo/preset) — the data-first choice: never publish or save factory defaults over the user's state. Export refusal inherits S1's mechanism (covered by `export_plugin_failure_tests` + `cli_plugin_crash_render`, both green).
- ENV-03 remains open (3A), Debug-only, pre-existing.
- Relink at open uses the persisted catalog; a plugin installed after the last scan is relinked at the next scan completion (both callers wired).

## 6. Proposed BUG- lines for the scribe (ESTADO.md is the scribe's only, D72/D80)

- **BUG-S2-NOTICE** (fixed, uncommitted): missing plugins were stored in the snapshot (`EngineSession.cpp:644-648`) but never announced — only the disabled Open… menu; opening an affected project showed nothing. Fix: session message + status-line notice with the count and "name: why" per plugin (shared helper; survives the device-line overwrite at engine start). Red: session message lacked name/why (2 checks) → green.
- **BUG-S2-RELINK** (fixed, uncommitted): `relinkPlugins` had ZERO callers (`git grep` @ a9a0384: definition + declaration only); a song from another computer stayed missing even with the plugin installed. Fix: callers at song open (before first load) and scan completion (live states fetched first); one undo step; state+automation preserved; no-match/duplicate recoverable. Red: stale reference missing twice with the plugin available → green recovery via relink (state bit-identical).
- **BUG-S3-STATE** (fixed, uncommitted, DATA): an invalid plugin state (corrupt Base64 OR valid-Base64 garbage) was ignored → factory defaults in silence; `plugins.states` + `applyPluginStates` then OVERWROTE the document's original on Save (loss persisted by `saveFile`; engine stored copy too). Fix: the host refuses the instance with `why="invalid state (…)"` (Base64 gate + the VST3 wrapper's own XML gate); the missing channel identifies it, S1 export refusal inherits it, the original blob survives refresh/save/reopen. Red: 14 checks incl. byte evidence of the overwrite → green 0; healthy control bit-identical before and after.
- *Estado medido — S2+S3 (2026-11-20, a9a0384 + round-1 + local fix por commitar)*: state_recovery_tests 7/68 red 16 falhas → verde 0; merge Debug 24/25 (ENV-03 pré-existente provado no build s1-red não-corrigido; 24/24 excluindo). UI click-through não realizado; caso residual XML-válido-sem-estado declarado.
- *Pendências*: commit S2+S3 (dono); ENV-03 (3A) continua; click-through da janela (roteiro: abrir canção afetada → aviso com nome+why; rescan → relink com estado/automação; undo → referências antigas; guardar/reabrir estado inválido → blob original intacto).
- *Referência*: este relatório + [evidencia/2026-11-20-s2s3-state-recovery/](evidencia/2026-11-20-s2s3-state-recovery/) (7 ficheiros) + `D:\Core-build\coordinator-round2\s2s3-notes.md`.

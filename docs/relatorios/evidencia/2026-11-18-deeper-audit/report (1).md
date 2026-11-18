<!-- coderabbit-ai-generated:v1 -->

# Deeper audit: behavior, UI wiring, obsolete paths and missing capabilities

## Scope and evidence

Date: 2026-11-18. Repository: d-aicore/core. Branch: `coderabbit/audit-fix-web-ui/029b227d`. Product sources remain at `ed4642e61b78e30fc4f7d950a28a214d0ac7c801`, Core 0.7.0. This is a follow-up to the first sweep, not a replacement of its evidence. No product implementation or ledger edits.

Owner request in this conversation: “preciso que vas mais fundo, de uma pontaa outra seja codigo morto, features por implemementar ui mal feita, coisas quenao funcionam. se mexer num paramrtro e depois voltar a meter no sitio anterio revertE?”

Inspected history/gestures, UI edits, plugin state capture, MIDI input, menus, Tools/catalogues, sample assignment, pattern waveforms, bridge proposals, installer/package rules, device input/output, and engine latency updates. Product requirements remain docs/DIRECCAO-CORE-PATTERN-MAKER-TOOLS.md and docs/FRONTEIRA-CORE-AICORE.md. No knowledge-base content was available in the earlier repository search. Windows UI and real plugins cannot be exercised by this Linux build; UI findings below concern code paths, not observed visual rendering. No web application exists to inspect with Chromium. Layout quality, focus, DPI and actual click behavior still need native Windows observation.

## Does moving a parameter back undo the change?

**Only in some cases.** The history compares the entire serialized document, not just the displayed knob value. It also only cancels the immediately preceding undo step with the same label.

| Case | Measured result | Meaning |
| --- | --- | --- |
| Scalar 145 → 150 → 145, two commits labelled Tempo | equal=1, modified=0, undo=0 | Returning exactly to the original document cancels the previous change. |
| Scalar 145 → 150 → 145 within one drag | equal=1, modified=0, undo=0 | No net document change and no new undo step. |
| Scalar 145 → 150 → 160 → 145, three commits | equal=1, modified=0, undo=1; Undo returns to 160 | The saved-state indicator clears, but history does not collapse an arbitrary multi-step round trip. This is the implemented history policy, not automatically a defect. |
| Real `setTempo`: 145 → 300 → 145 in separate edits, bass note initially 3500 ticks | equal=0, modified=1; pattern count 1 → 3; active note length 1692 | The BPM returns, but the song does not. Bass retiming is lossy and creates fresh patterns. |
| Same real tempo movement inside one drag, using the UI's captured baseline | equal=1, modified=0, undo=0 | The UI resets each drag update from tempoBase_, avoiding cumulative retiming within that gesture. |
| Plugin knob moved and returned | Dirty indicator remains set by current code | `pluginsDirty_` is an event flag, not equality against the saved plugin state. Static confirmation; not a real-plugin runtime test. |
| Ctrl+Z after changing a living plugin's sound | Existing contract preserves the plugin's newest state | `keepLivePluginStates` is intentional and tested. Core Undo is not an undo stack for every knob movement inside third-party editors. |

Evidence: `app/domain/src/history.cpp:32`, `:72`, `:118`; `app/domain/src/document.cpp:827`; `app/domain/src/randomize.cpp:325`, `:725`; `app/desktop/src/MainComponent.cpp:1891`, `:3542`, `:3904`, `:6329`; `app/desktop/src/MainComponent.h:444`. The reproduction calls actual domain functions and mirrors the inspected gesture boundary. No GUI test is claimed.

The tempo behavior must be discussed against the existing gate-preservation/clamping contract in randomize.hpp: simply removing retiming would change a documented musical behavior. A repair should preserve the gesture's original musical data or make the destructive retiming explicit, with regression coverage for separate edits as well as a single drag.

## Additional findings

IDs here are report-local, continuing the first sweep; they do not allocate ledger decision numbers.

| ID | Priority | Finding and user impact | Evidence/status |
| --- | --- | --- | --- |
| BUG-SWEEP-07 | High | Tempo round trips can shorten bass notes and retain extra patterns even after restoring the displayed BPM. | Proven with baseline library: 3500 → 1692 ticks; one → three patterns. `randomize.cpp:325`, `:725`. Open. |
| BUG-SWEEP-08 | Medium | Plugin edits keep the song marked unsaved after returning to the original sound. | `MainComponent.cpp:6329` sets pluginsDirty_; only save/new-song paths clear it (`:3933`, `:6594`). `adoptPluginStates` does not reconcile the flag with saved-state equality. Even byte-identical returned state leaves this flag true. Static confirmation; real plugin state equality depends on its serialization. Open. |
| BUG-SWEEP-09 | High functional | Add-track menu promises internal instruments and an empty sampler but creates silent unassigned tracks. | `MainComponent.cpp:4622` exposes the catalogue; `edit.cpp:313` always creates kind=none. Probe confirms all eight catalogue entries create none. Do not restore removed native DSP as a shortcut: the product mandates external plugins. Open. |
| BUG-SWEEP-10 | High functional | Applying a library sample writes a removed sampler and cannot play that sample through this engine projection. It can replace a working plugin instrument with a silent legacy instrument. | `MainComponent.cpp:4434` → `samples.cpp:1278` → `samplerInstrument` kind=builtin/name=sampler → `document.cpp:758` projects none. Synthetic domain probe confirms builtin → none. Sample assignment itself does not read the synthetic file; this proves routing, not decoding/audio output. Open. |
| BUG-SWEEP-11 | Medium | Randomize sound remains enabled for styled songs, but works only on removed built-in instruments. | `MainComponent.cpp:5433` tests only styled; `randomize.cpp:686` skips plugins/none and throws if nothing matched. With legacy builtins it changes stored knobs that the projection does not play. Static confirmation. Open. |
| BUG-SWEEP-12 | Medium | Accepted requests to open effects do not open the requested effect/chain UI. | `calls.cpp:564` defaults to effects and accepts named effects; `MainComponent.cpp:3726` forwards; `openCoreWindow` at `:2228` handles score/mixer/patterns/instrument but falls through to selecting the track for effects/named effects. No corresponding editor opening follows. Static confirmation; bridge action runtime not exercised. Open. |
| BUG-SWEEP-13 | Medium | Tool success notices can be shown after the edit was refused. | `MainComponent.cpp:2296`: done=true is set inside the mutation callback. `edit` subsequently validates projection and may fail plugin-state capture (`:2104–2111`), returning void. runTool still reports the result and “Ctrl+Z puts it back”. Similar unconditional follow-up exists after proposal application (`:3720–3728`). Static confirmation of failure ordering; injected GUI failure not run. Open. |
| BUG-SWEEP-14 | Medium | Development zip packaging can retain removed language/pattern/pcore files. | `app/desktop/CMakeLists.txt` core-package reuses package/Core and removes only templates before copying. copy_directory does not remove destination-only files. App resources and installer staging have explicit cleanup; development zip staging does not. Static confirmation; Windows packaging not run. Open. |
| BUG-SWEEP-15 | Medium | MIDI note ownership ignores channel. | `MainComponent.cpp:3065`: held notes are (track,pitch), and Note Off consumes the oldest matching pitch regardless of channel. Example: channel 1 starts C on track A, selection changes, channel 2 starts C on B; channel 2's release ends A first. Static trace, no MIDI-device test. Existing UI merges input onto the selected track; retaining the channel for release pairing is a separate issue. Open. |

The original six findings remain open. This report does not claim all nine additional items were reproduced in a running desktop app.

## Dead code, obsolete behavior and compatibility

| Area | What is actually present | Disposition |
| --- | --- | --- |
| Built-in effect execution | `engine/src/dsp/effects.cpp`: registration is empty and makeEffect always returns nullptr. Graph still contains the legacy builtin construction branch. | The branch has no producing factory in this build. Candidate for a bounded cleanup; preserve unsupported effect data and migration. |
| Internal sound catalogue/knobs | `edit.cpp` retains instrumentCatalogue, builtinInstruments, reset/default helpers; Inspector builds controls for builtin kinds. | Not all unreachable: menus, old projects and tests use it. It is obsolete sound-facing behavior, not permission to delete serialized legacy content. |
| Pattern sound preview | `app/previews/src/pattern_sound.cpp:35` admits only builtin tracks; plugin/none return no request. Legacy builtins are projected to none. `Waveforms.cpp:86` still requests/renders/cache-manages previews. | Ineffective for current plugin patterns; live code with obsolete eligibility. Plugin waveform rendering would require an explicit design/performance scope. Do not confuse this with auditioning patterns, which has a separate path. |
| Analysis client commands | EngineSession::enableAnalysis and watchEffect have definitions/dispatch/restart state but no application callers found. | Dormant client API. Engine protocol consumers may still use analysis commands; do not remove protocol support solely from desktop call counts. |
| SCorePanel / BuildupPanel | Both compiled and have actual entry paths: score via openCoreWindow; buildup via PatternEditor. | Not dead. Their product naming/surfaces still need reconciliation, but deleting them blindly risks arrangement/pattern behavior. |
| Source contract references | document.hpp and CMake comments refer to docs/contracts/core-document.md and core-language.md; those paths are absent from this checkout. | Stale guidance; repair links to actual contracts or explicitly document missing specifications. |

No source deletions are proposed without consumer and persistence checks. Tests of legacy data are not proof that obsolete controls still produce sound.

## Features and UI coverage

| Flow/capability | Current evidence | Remaining work |
| --- | --- | --- |
| Pattern Maker | Patterns window, pattern tools and arrangement entry points exist; catalogue/alignment code retained. | Consolidate visible P-Core naming into Pattern Maker. Preserve catalogue and kick/bass/cymbal relations. |
| Tools | Role-based external plugin orchestration exists. Cards still expose W-Core/F-Core/R-Core/D-Core and menus rather than the requested simplified per-chain panels. | Product direction is already specified; final panel design/macros and parameter ownership are still open. Do not count the existence of cards as completion. |
| Exact plugin editor access | Instrument/effect editor opening exists in track/vendor menus. | Requested contextual editors for every instance in a Tool chain are not implemented by the generic Tool card/menu path. Verify identity and persistence on Windows. |
| Save/reopen/autosave/export | Domain and engine implementation exists; first sweep found concrete data-safety issues. | Fix those before visual polish; Windows save/reopen/export still needs execution. |
| Audio import and bounce | Audio assets/clips and bounce/resample paths exist. | Importing audio into an audio track is distinct from the broken sample-to-sampler assignment. |
| Input recording | Device config is playback-only. Incoming MIDI forwards live notes; no recorded notes are committed there and no record/arm workflow was found in the inspected menus/session. | Audio/MIDI capture into the arrangement is missing in these paths. Offline bounce and engine diagnostic capture are not input recording. Product document retains recording as a DAW requirement. |
| Reference audio → templates | templates.hpp explicitly marks analysis provenance generation as not implemented. | Not a delivered feature; scope against the external AI boundary before planning. |
| More plugin formats | JUCE host configuration enables VST3 and disables AU; no extra format backend found. | Product objective permits more formats; not universal compatibility today. |
| UI usability/accessibility | ToolDock disables keyboard focus for buttons, combos and sliders to protect Space transport; footer allocates fixed widths for restart/status/export/revert controls. | Keyboard-only operation and narrow-window notices require runtime usability tests. These are review concerns, not proven clipping/accessibility failures. |
| Installer vs development zip | Installer clears staging and has a manifest safeguard for additional installed files. | Development zip stale resources remain a separate defect; installer runtime/rollback not verified here. |

## Checks and rejected suspicions

Reused: prior baseline Release build passed; merge test gate 14/15, with the realtime-priority/null-device failure. Source, build options and libraries are unchanged; documentation-only edits do not invalidate those results. No claim of a newly green full suite.

New commands, repository cwd:

- `c++ -std=c++20 -pthread -Iapp/domain/src -Iengine/third_party /tmp/core-roundtrip.cpp /tmp/core-audit-build/app/domain/libcore_domain.a -o /tmp/core-roundtrip && /tmp/core-roundtrip` — exit 0; seven behavior scenarios plus all eight menu choices. The output is observations, not a passing regression gate. Saved as roundtrip.cpp and roundtrip.log with this report.
- `/tmp/core-audit-build/tests/domain_tests keepLivePluginStates` — exit 0; 1 case, 15 assertions. Confirms the existing live-plugin preservation contract, not real plugin editor behavior.

Corrected during inspection: the preliminary Stage Core/master rejection suspicion was false: the caller converts master to an empty track before dispatch (`MainComponent.cpp:3724`). Dynamic latency was also checked: server.checkPluginLatency polls/rebuilds, and main.cpp calls it, so the absent JUCE latency-change callback is not by itself proof of missing compensation.

Not executed: Windows GUI, real plugin lifecycle/sound/null tests, live MIDI devices, input recording, Windows installer/package, sanitizers and audio-thread allocation instrumentation. Linux probes and static tracing cannot establish those outcomes.

## Next repair order and handoff

1. Original recovery/export ownership issues; no user content loss.
2. Tempo round-trip regression and misleading sampler/internal instrument actions.
3. Dirty-state feedback, accepted-action completion reporting, plugin retry and effect-opening wiring.
4. Obsolete menu/catalogue cleanup with legacy-project preservation; packaging consistency.
5. Approved Tools/Pattern Maker UI scope and a Windows user-journey test matrix; recording and further plugin formats as explicit missing capabilities.

Pending for ledger scribe, owner Diogo: record this audit's observations and additional open BUG identifiers, the measured round-trip exceptions, the passed 1-case/15-assertion contract check, and Windows verification outstanding. Source: owner's quoted request and this report. No decision number assigned and no ESTADO.md changes. This report and its index entry are the only new repository changes in this follow-up.

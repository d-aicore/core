<!-- coderabbit-ai-generated:v1 -->

# Consolidated deep audit and additional recovery/menu findings

## Scope and measured state

2026-11-18. d-aicore/core, branch `coderabbit/audit-fix-web-ui/029b227d`, product sources remain at `ed4642e61b78e30fc4f7d950a28a214d0ac7c801` (0.7.0). Owner asked “continua sempre pq existem muitos bugs entregas a listra no final” in this task. This consolidates the audit to date, preserving every earlier report. It is not a claim that the codebase has no further bugs. No product fixes or ledger edits.

Environment: Ubuntu 26.04.1, GCC 15.2.0, CMake 4.2.3, unchanged Release domain/engine build. Prior build pass and merge gate 14/15 are reused; realtime null-device failure remains outstanding. Synthetic fixtures only. No new full suite or sanitizers. Native Windows UI, real plugins, WASAPI, DPI/focus/layout and long audio stress sessions remain unverified. This native application has no web application for Chromium; no desktop browser was used. No knowledge-base directory was available in the repository checks.

## Consolidated list

These are **32 report-local findings**, not 32 independently executed bugs: the list includes static findings, a Linux environment failure, a recovery gap and compatibility questions. IDs allocate no ledger decisions. All are open; none is reported fixed. Evidence and limitations from previous rounds remain applicable.

| ID (BUG-SWEEP-) | Priority/type | Finding | Evidence | Detail |
| --- | --- | --- | --- | --- |
| 01 | High | Restarted saves overwrite retained recovery files | Probe | R1 |
| 02 | High | WAV export reuses and consumes existing .part files | Probe | R1 |
| 03 | High | Failed rollback can strand the previous export despite the UI assurance | Probe + UI code | R1 |
| 04 | Medium | Malformed add-on name/version throws from typed JSON reads | Probe | R1 |
| 05 | High | Failed plugin cache prevents promised automatic retry (existing FL-2) | Code | R1 |
| 06 | Environment | Linux null-device initialization fails at forced realtime priority | Executed failure | R1 |
| 07 | High | Separate BPM edits returning to the original BPM can shorten bass notes | Probe | R2 |
| 08 | Medium | Plugin dirty indicator remains set after returning a knob/state | Code; live plugin unverified | R2 |
| 09 | High functional | Internal instrument menu creates unassigned silent tracks | Probe + UI code | R2 |
| 10 | High functional | Sample assignment installs a legacy sampler projected to no instrument | Probe | R2 |
| 11 | Medium | Randomize sound targets removed internal instruments | Code | R2 |
| 12 | Medium | Accepted effects-open bridge action has no UI handler | Code | R2 |
| 13 | Medium | Tool success is set before final edit validation/capture | Code | R2 |
| 14 | Medium | Development ZIP staging retains obsolete resources | Code; installer is separate | R2 |
| 15 | Medium | Live MIDI held-note ownership ignores input channel | Code | R2 |
| 16 | High | MIDI/preset late write failures return success with empty files | Fault injection; Linux replacement | R3 |
| 17 | High | Cleanup does not protect recovery-only .saving-N audio references | Probe; no actual trash move | R3 |
| 18 | High | Invalid MIDI name Unicode produces an unserializable document | Probe | R3 |
| 19 | Medium | Density leaves stale roll data: MIDI exports 8 notes before save, 5 after reopening | Probe | R3 |
| 20 | Medium | Saving a reusable pattern discards active rolls | Probe | R3 |
| 21 | Medium | Splitting a frozen clip changes its held source/audio | Offline render sample comparison | R3 |
| 22 | Medium | Duplicate section loses inherited starting tempo | Probe | R4 |
| 23 | Medium | Swap sections leaves tempo attached to the old positions | Probe; intended policy needs confirmation | R4 |
| 24 | Medium | Insert bars stretches ramps and changes music before insertion | Probe + engine timing | R4 |
| 25 | Compatibility | Shortening removes a future ramp endpoint and changes the retained passage | Probe; removal itself is documented | R4 |
| 26 | Medium | Template load desynchronizes Swing control and notes | Probe | R5 |
| 27 | Medium | Accepted document metadata throws when the pattern-slot consumer reads it | Probe; UI crash not measured | R5 |
| 28 | Compatibility | Template matching uses name-first, contrary to the part-first API contract | Probe; resolve priority before changing | R5 |
| 29 | High | Tempo-only Randomize deletes rack-only ideas not placed on the timeline | Probe; Undo restored within session | R6 |
| 30 | Medium | Louder changes rolled-note base velocity without changing played hit velocities | MIDI probe + engine code | R6 |
| 31 | Recovery gap | Valid autosave .bak is not discovered when the primary is corrupt | Probe; unreadable-primary exclusion documented | This report |
| 32 | High conditional | Plugin menu IDs overlap action IDs and can replace the instrument | Code + range calculation; Windows unverified | This report |

Detailed reports:

- [R1: persistence, export, add-ons and engine baseline](../audit/report.md)
- [R2: parameter round trips, obsolete paths, missing capabilities and UI wiring](../audit-deeper/report.md)
- [R3: MIDI, roll preservation, cleanup and Freeze](../audit-round3/report.md)
- [R4: timeline and tempo continuity](../audit-round4/report.md)
- [R5: template Swing, matching and metadata](../audit-round5/report.md)
- [R6: idea preservation and roll velocity](../audit-round6/report.md)

## BUG-SWEEP-31 — valid autosave backup is undiscoverable

`app/domain/src/autosave.cpp:34` scans only files with extension `.core-autosave` and skips unreadable primaries. `.core-autosave.bak` is excluded even though the safe writer creates it. `MainComponent.cpp:6606` uses this list as the entry point to recovery; there is no backup fallback in that flow.

The probe calls `writeAutosave` twice in a synthetic folder, producing a primary named Second and a valid backup named First recoverable. Healthy discovery yields one entry. After deliberately corrupting the primary with invalid JSON, discovery yields zero entries, while `readAutosave(backupPath(primary))` still recovers First recoverable. The backup was neither deleted nor irrecoverable; manual recovery remains possible.

Contract qualification: `autosave.hpp` explicitly excludes unreadable files from discovery. This is a missing fallback/recovery capability rather than proof that the existing exclusion violates its documented API. A repair should expose a usable older backup without silently representing it as the newest save, and preserve corrupt evidence. The behavior of Windows startup dialogs was not executed.

## BUG-SWEEP-32 — plugin catalog size changes what menu actions do

`app/desktop/src/MainComponent.cpp:4684` assigns instrument replacement IDs starting at 300. The same menu assigns native instruments from 800, effect Open from 900, Remove from 920, and On from 940. The callback first checks `chosen >= 300 && chosen - 300 < instruments.size()` (`:4726`) before checking those action ranges.

Consequences from the exact dispatch condition:

- 501 instrument entries: native instrument action ID 800 selects instrument index 500.
- 601 instrument entries: Open on first effect (900) replaces the track instrument with index 600.
- 621 entries: Remove first effect (920) replaces it with index 620.
- 641 entries: toggle first effect (940) replaces it with index 640.

The catalog method returns the instrument list without a cap (`app/desktop/src/Plugins.cpp:93`). This is conditional on catalog size; no claim is made that this sandbox has that many installed plugins. A small arithmetic script evaluates the source's dispatch ranges, with controls at 500/600 entries. It does not run JUCE or simulate actual installed plugins. Actual menu selection remains Windows-unverified.

The Add Track menu has the same class of overlap: plugin IDs start at 2000 and native IDs at 3000, with the plugin branch tested first. Treat these as one identifier-allocation defect, not separate inflated counts. Repair should allocate disjoint identifiers or map each ID to an explicit action, with regression coverage at the boundaries. Do not cap the user's plugin catalog as a substitute.

## Additional negative result: invalid float WAV output

A synthetic 48 kHz mono float WAV with one NaN followed by 479 finite samples imported successfully. Offline serial rendering at 48 kHz/512 frames returned zero nonfinite samples in the left output; its first sample was zero. The nonfinite-output suspicion was **not reproduced** and is not counted as a finding. This control does not prove all malformed input or live/plugin cases safe and does not establish how much valid audio was muted by the guard.

## Coverage and remaining work

| Area | Evidence accumulated | Remaining limitation |
| --- | --- | --- |
| Save/recovery/assets | Safe-write/export fault injection, recovery discovery, cleanup reference chains | Windows filesystem and native recovery flow |
| Notes/patterns | Domain editing, MIDI byte comparisons, template/slot consumers | Native editor clicks and visual feedback |
| Timeline/tempo | Inverse edits, parser checks, engine timing calculations | Windows interaction and broader processed-audio null tests |
| Plugin lifecycle | Cache/state/Undo/caller inspection, existing contract test | Real VST3 failures, dense load, editors, sustained runs |
| Audio/export | Synthetic renderer probes, Freeze sample comparison, invalid-float negative result | Real devices, all rates/blocks, full plugin chains |
| UI/missing features/dead paths | Menu/callback/bridge/resource wiring and capability matrix in R2 | Layout, focus, DPI and native runtime |

Prioritize preservation findings 01–03, 16–18 and 29, then conditional action misrouting 32. Contract/policy items 23, 25, 28 and 31 need their qualifications retained during triage. Future authorized fixes must preserve the original contracts and demonstrate red-before/green-after behavior. No report here authorizes reintroducing removed native instruments or surfaces excluded by D83.

## Verification record for this additional pass

Working directory: `/home/vercel-sandbox/workspace/repos/github.com/d-aicore/core`.

```sh
c++ -std=c++20 -pthread -Iapp/domain/src -Iengine/src -Iengine/third_party /tmp/core-round7.cpp /tmp/core-audit-build/app/domain/libcore_domain.a /tmp/core-audit-build/engine/libcore_engine_lib.a -o /tmp/core-round7 && /tmp/core-round7 > /tmp/core-round7.log
```

Exit 0. Fixtures: `/tmp/core-round7-fixtures`; hand-built float WAV and synthetic autosaves. Probe source/log and menu-range log delivered separately. Product/config/dependency diff remains empty. The probe completion proves the measured outcomes, not a passing product recovery guarantee. No bugs were fixed in this pass.

## Ledger handoff

Open handoff, owner Diogo: triage the consolidated list and retain the evidence classifications. Proposed measured-state entry for the ledger scribe: “2026-11-18, owner requested continuous deeper audit and a final consolidated list in task 029b227d-329c-40fe-90ee-220e16fc877c. Audit at ed4642e contains 32 report-local findings including code findings, Linux limitation and policy/recovery gaps; no fixes. Additional probe demonstrates undiscovered valid autosave backup; nonfinite-WAV output suspicion not reproduced. Plugin-menu action collision confirmed statically with catalog-size thresholds. Windows runtime remains unverified.” ESTADO.md was not edited and no decision number allocated.

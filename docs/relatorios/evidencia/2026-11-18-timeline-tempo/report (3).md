<!-- coderabbit-ai-generated:v1 -->

# Additional audit: timeline edits and tempo continuity

## What was measured

- Date: 2026-11-18. Repository: d-aicore/core. Branch: `coderabbit/audit-fix-web-ui/029b227d`. Initial/final product revision: `ed4642e61b78e30fc4f7d950a28a214d0ac7c801` (Core 0.7.0).
- Authorization: owner message “mais” in the current task, continuing the deep audit. No product implementation authorized by this audit request; no product files or ledger changed.
- Environment: Ubuntu 26.04.1, GCC 15.2.0, C++20; unchanged baseline Release domain/engine libraries. No dependencies, plugins, services, or external data needed.
- Inspected timeline edits, section transforms, asset import/copy validation, MIDI note scheduling, and routing cleanup. Executable findings in this round concern tempo transforms. Missing knowledge-base directory was checked; no KB evidence was available.
- One standalone synthetic probe compiled and completed with exit 0. This means the reproductions ran; the reported behavior is defective or needs a compatibility decision, as qualified below. Original and transformed documents pass JSON validation. Passing validation does not preserve musical equivalence.
- Windows native UI was not run. No web app is provided by this repository, so Chromium cannot establish native UI correctness. No screenshots or visual quality claims.

## Results

IDs are local to the sweep reports, continuing BUG-SWEEP-01 through BUG-SWEEP-21. They do not allocate ledger decision numbers. All findings remain open.

| ID | Priority | Result | Classification |
| --- | --- | --- | --- |
| BUG-SWEEP-22 | Medium | Duplicated section starts at the preceding section's final BPM, rather than its own original starting BPM. | Proven through domain operations; UI caller confirmed in code. |
| BUG-SWEEP-23 | Medium | Swapping sections moves clips and automation but leaves the entire tempo map in its original positions. | Proven through domain operations; UI caller confirmed in code. |
| BUG-SWEEP-24 | Medium | Inserting empty bars during a tempo ramp changes tempo and event timing before the insertion, and changes the moved ramp. | Proven with domain functions and the engine TempoMap on the projected document. |
| BUG-SWEEP-25 | Medium; compatibility qualification | Shortening a song removes a future ramp endpoint and changes tempo throughout the retained passage. Extending it again does not restore the endpoint. | Proven with domain functions and engine timing; endpoint deletion is already documented, so the preservation policy needs explicit treatment. |

### BUG-SWEEP-22 — duplicate copies points but loses the initial tempo

Evidence: `app/domain/src/edit.cpp:777` (`duplicateSection`), particularly the tempo-point copy starting at `:784` and insertion at `:823`. UI caller: `app/desktop/src/MainComponent.cpp:3510`.

Synthetic song: 120 BPM initially, section A spans ticks 0–15360 (four bars), with steps to 200 BPM at tick 3840 and 160 BPM at tick 11520. Duplicate A immediately after itself. The original starts at 120 BPM, but its copy starts at 160 BPM. At one bar into each section, both reach 200 BPM. The following section remains at 160 BPM as before; the control rules out a global base-tempo change.

Root cause: the code copies explicit tempo points inside A, but never establishes the effective tempo at A's beginning. This is a gap in the existing preservation behavior. `tests/bug_tests.cpp:507` already checks the copied interior steps and ramps, but not the first interval of the copy. That test was inspected, not rerun separately in this round; its suite passed in the earlier gate.

Repair target: preserve the effective initial tempo and the whole curve of the copied passage, including inherited tempo and ramps spanning its boundaries. Preserve the later section's effective starting tempo too. Copying one extra step indiscriminately is insufficient for boundary-crossing ramps.

### BUG-SWEEP-23 — swapped music retains the other section's tempo

Evidence: `app/domain/src/edit.cpp:732` (`swapSection`). It splits/moves clips and automation, then swaps section labels/identity, without processing `tempoMap`. UI caller: `app/desktop/src/MainComponent.cpp:3508`.

Synthetic song: A starts at tick 0 at 120 BPM, B starts at tick 15360 at 240 BPM; each is four bars long. After swapping, the first section is named B but plays at 120 BPM, and the second is named A but plays at 240 BPM. The serialized tempo map is byte-equivalent as JSON before and after the operation.

The existing source contract says this operation exchanges the sections' music. The neighboring duplicate operation already explicitly treats tempo as belonging to the music (including the earlier regression test). No separate policy to pin tempo during section swaps was found. If keeping tempo fixed is intentional, that would need an explicit product clarification; it is currently an inconsistency between the two section operations.

Repair target: move tempo segments with section content, preserving values at every boundary and the tempo outside the swapped region. Test unequal section lengths and ramps, as well as the simple step reproduction.

### BUG-SWEEP-24 — insertion stretches a ramp over earlier music

Evidence: `app/domain/src/edit.cpp:835` (`insertTime`), tempo-point shift at `:853`; existing contract `app/domain/src/edit.hpp:326` says everything from the insertion moves forward. UI route: `app/desktop/src/MainComponent.cpp:3458`.

Synthetic song: 120 BPM at tick 0, ramp to 240 BPM at tick 15360. Insert two bars at tick 7680. The endpoint moves to tick 23040, but there is no tempo boundary inserted at the cut. Consequently the whole ramp stretches:

| Position | Before | After |
| --- | --- | --- |
| Tick 3840, before the insertion | 150 BPM | 140 BPM |
| Engine time at tick 3840 | 1.78515 s | 1.84981 s |
| Original tick 11520, moved to tick 19200 | 210 BPM | 220 BPM |

An otherwise identical step-tempo control retains 120 BPM before the insertion. The defect requires interpolation; merely testing moved tempo-point positions misses it. The engine timing values come from `core::fromJson(toProjection(...))` and `core::TempoMap::sampleAt` at 48 kHz. They are timing calculations, not an audible render or plugin test.

Inserting and then deleting those same two bars leaves the earlier position at 140 BPM and the document unequal to its original JSON. This is a new inverse edit, not an Undo test. The deletion operation cannot infer the original ramp once insertion has changed it.

Repair target: preserve the curve before the cut and translate the curve after it. The tempo inside the newly inserted empty interval needs a defined policy; the existing contract does not specify whether it holds or continues. Either choice can preserve the earlier music. Do not infer a new policy from this audit.

### BUG-SWEEP-25 — a removed future endpoint also changes retained music

Evidence: `app/domain/src/edit.cpp:2178` (`setSongLength`) drops all tempo points at/after the new end. UI route: `app/desktop/src/MainComponent.cpp:3517`.

Use the same 120-to-240 BPM ramp ending at tick 15360. Shorten the song to tick 7680. The retained tick 3840 changes from 150 to 120 BPM; engine time there changes from 1.78515 to 2 seconds. The map becomes empty. Returning the length to its original value leaves that position at 120 BPM and the document unequal to its original JSON.

Contract qualification: `edit.hpp:333` explicitly says tempo changes past the new end are removed. The UI also tells the user that tempo changes were removed and that Undo restores them (`MainComponent.cpp:3518`). Therefore failure to restore discarded points by extending the song is documented destructive behavior, not a separate Undo defect. The additional musical problem is that a future ramp endpoint controls the already-retained passage; the UI's “past the new end” explanation does not convey that earlier timing changes too.

A repair preserving the retained ramp must respect the rule excluding out-of-song points, for example by deriving an in-range endpoint. If the intended policy is to discard the ramp completely, the behavior requires a product decision and accurate explanation. This audit does not silently replace the documented deletion rule. Compare `templateFromRange` (`templates.cpp:122`) and its existing ramp-preservation regression in `tests/audit_tests.cpp:120`: that caller uses a different cut path which preserves retained ramp behavior.

## Changes and verification

Only this report and an index entry were added to the repository in this round. Prior reports are unchanged. Probe source and log are delivered separately as `tempo-probe.cpp` and `tempo.log`.

Command executed from `/home/vercel-sandbox/workspace/repos/github.com/d-aicore/core`:

```sh
c++ -std=c++20 -pthread -Iapp/domain/src -Iengine/src -Iengine/third_party /tmp/core-round4.cpp /tmp/core-audit-build/app/domain/libcore_domain.a /tmp/core-audit-build/engine/libcore_engine_lib.a -o /tmp/core-round4 && /tmp/core-round4 > /tmp/core-round4.log
```

Exit 0. Cases: duplicate, swap, insert ramp, shorten ramp, insert/delete inverse, shorten/extend inverse, and step insertion control. JSON parsing checks completed without exceptions for transformed documents. These are standalone probe scenarios, not newly added registered test cases. No source implementation changed and no after-fix pass is claimed.

Reused earlier results: Release build passed; merge gate 14/15 suites, with realtime null-device initialization failing. No broad gate rerun because product files, dependencies and configuration remain identical. Native Windows, real plugins, full UI, visual checks and sanitizers were not run in this round.

Other inspected paths did not establish an additional finding in this round: note-off scheduling intentionally allows notes to outlast clip boundaries; master sidechains are prohibited, so absence of master-sidechain cleanup in track deletion is not a supported-path bug; missing copied audio is surfaced by `projectFolderChanged`/`checkMissingAssets`, so the inspected Save As flow is not counted as silent success without warning.

## Pending work and ledger handoff

Open handoff, owner Diogo: triage BUG-SWEEP-22 through BUG-SWEEP-25. For implementation, retain the current reproductions and add domain regressions comparing effective tempo over time, rather than only copied point counts. Cover timing consumers after projection. Resolve the inserted-interval tempo policy and the shortening compatibility qualification before changing those guarantees. Windows UI verification is still required for end-to-end claims.

Proposed measured-state line for the ledger scribe: “2026-11-18, owner requested ‘mais’ in task 029b227d-329c-40fe-90ee-220e16fc877c: four additional open timeline/tempo findings measured at ed4642e. Synthetic domain probe plus engine timing calculations reproduced incorrect duplicate starts, unmoved swap tempo, insertion ramp distortion, and retained-passage changes after shortening. Inverse edits do not restore the original maps; Undo was not tested in this round. Endpoint removal is an existing documented policy and needs compatibility treatment. No product fixes; Windows unverified.” This is a proposed entry only. ESTADO.md was not edited and no decision numbers were allocated.

<!-- coderabbit-ai-generated:v1 -->

# Additional audit: randomizer preservation and roll velocity edits

## What was measured

2026-11-18, d-aicore/core, branch `coderabbit/audit-fix-web-ui/029b227d`, initial/final product revision `ed4642e61b78e30fc4f7d950a28a214d0ac7c801`. Owner request: “mais” in this task. Audit only. No product implementation, ledger change, or edits to prior reports.

Ubuntu 26.04.1, GCC 15.2.0, C++20, unchanged Release domain library. In-memory synthetic documents; no plugins, services, credentials or customer data. Reviewed note operations, pattern references, randomizer cleanup, and MIDI/engine roll velocity consumers. Knowledge-base search found no directory. Native Windows UI remains unverified; this repository has no web application for Chromium to exercise.

## Results

IDs are local to the sweep reports, continuing the preceding findings. They allocate no ledger decisions.

| ID | Priority | Finding | Classification | Status |
| --- | --- | --- | --- | --- |
| BUG-SWEEP-29 | High: work preservation | Tempo-only Randomize deletes patterns used by the Patterns window but not by timeline clips. | Proven domain reproduction, Undo control included; UI path inspected. | Open |
| BUG-SWEEP-30 | Medium: musical correctness | Louder changes a rolled note's displayed/base velocity but leaves its actual hit velocities unchanged. | Proven by byte-identical MIDI export; engine consumer confirmed in code. | Open |

### BUG-SWEEP-29 — arrangement-only cleanup deletes a saved idea

`app/domain/src/randomize.cpp:682` calls `removeUnusedPatterns` for known tracks whenever groove or tempo randomization is active. `edit.cpp:560` considers a pattern used only when timeline clips reference it. It has no access to `Document.extra.patternSlots`, which holds the Patterns window's references. `rackSlots` then silently drops references to deleted patterns (`pattern_rack.cpp:131`).

Reproduction uses `newTemplateDocument`, a valid style marker (`extra.template.style = Dark psycore`), and a section spanning the song. Create an additional slot, Unarranged idea; obtain its bass pattern through `rackPatternFor` and put a roll note in it without sending it to Arrange. Invoke `randomizeStyle` with axes containing only tempo.

Measured result: the idea pattern exists and the slot has one track reference before randomization; afterwards the pattern is absent and the slot has zero track references. The function returns one change message, so the inspected UI path commits it (`app/desktop/src/MainComponent.cpp:4366`). Asking `rackPatternFor` for that slot again creates an empty pattern: zero notes. The resulting document still parses successfully, so schema validation does not detect the lost content.

Control: commit the result to `History`, then Undo. The original pattern is restored. This is not a claim of irreversible deletion: the risk is silent content removal during an unrelated operation, which can be saved and later outlive the session's Undo history.

Existing contract: `pattern_rack.hpp` says a slot's patterns are the tracks' own patterns and need not be timeline clips; Randomize tempo is the tempo axis. There is no requirement that an idea must first be sent to Arrange to survive Randomize. Repair should include every supported reference when pruning, or keep cleanup restricted to obsolete patterns actually created by that transform. Do not delete unarranged ideas to satisfy an arrangement-only reachability test.

### BUG-SWEEP-30 — Louder does not change the hits of a roll

`app/domain/src/groove.cpp:159` scales only `Note.velocity`. The piano roll's Louder/Softer menu calls it (`PatternEditor.cpp:1807`), while the velocity lane and wheel also change only the base field (`:2055`, `:2116`). The MIDI exporter (`app/domain/src/midi.cpp:244`) and engine event expansion (`engine/src/core/graph.cpp:372`) use `roll.velocityFrom`/`roll.velocityTo` whenever the roll has more than one hit.

Synthetic fixture: one active four-hit roll, base velocity 0.5, hit velocities ranging from 0.3 to 0.7. Louder at factor 1.15 reports one changed note and sets its base velocity to 0.57. The full exported MIDI bytes remain identical. Control: clear the roll flag and roll payload, then apply the same tool; exported MIDI changes. Clearing the payload is necessary because the separate stale-roll export bug was already recorded as BUG-SWEEP-19.

Thus the UI can show a velocity edit and mark the song changed while this note's played hit velocities remain the same. MIDI equality was executed; the same behavior in the live engine is established by inspecting its expansion code, not a real plugin listening test. Normalization/accent paths also write the base field, but were not separately runtime-tested or counted as additional findings.

Repair should define how a base-velocity edit scales an existing roll envelope, including clipping and zero endpoints, then update all editing paths consistently. Preserve the envelope's intentional shape rather than flattening it without a decision. The existing single-note velocity contract and ordinary-note control must remain valid.

## Changes and verification

Only this report and its index entry were added to the repository. Probe and output are delivered as `edit-probe.cpp` and `edit.log`. No product/configuration/dependency changes; `git diff --name-only HEAD -- app engine tests CMakeLists.txt` returned no paths.

Final command, repository working directory `/home/vercel-sandbox/workspace/repos/github.com/d-aicore/core`:

```sh
c++ -std=c++20 -pthread -Iapp/domain/src -Iengine/third_party /tmp/core-round6.cpp /tmp/core-audit-build/app/domain/libcore_domain.a -o /tmp/core-round6 && /tmp/core-round6 > /tmp/core-round6.log
```

Final exit 0. Standalone scenarios: rolled-note velocity export, ordinary-note control, rack-only idea deletion, parser acceptance, Undo restoration and empty-slot recreation. These are reproductions, not registered suite counts or passing product assertions. The initial scratch fixture used the wrong style field and aborted with DocumentError (exit 134); it was corrected to the actual supported style location. An intermediate ordinary-note control retained inactive roll metadata and exercised the known BUG-SWEEP-19; the final control clears it. No product code was changed to obtain these results.

Earlier Release build pass and merge 14/15 result are reused; the prior realtime null-device failure remains outstanding. Broad suites were not rerun because product inputs are identical. No fixes, after-fix passes, sanitizer runs or Windows runtime results are claimed.

## Pending work and ledger handoff

Open handoff, owner Diogo: prioritize BUG-SWEEP-29 as work preservation, then BUG-SWEEP-30. Retain the executable reproductions for an authorized implementation; verify references across every slot and history behavior, and compare actual roll hit velocities after each edit surface. Windows Patterns/piano-roll checks remain required for end-to-end completion.

Proposed measured-state line for the ledger scribe: “2026-11-18, owner requested ‘mais’ in task 029b227d-329c-40fe-90ee-220e16fc877c: two additional open findings at ed4642e. Tempo-only Randomize deletes rack-only patterns (Undo restores within the session); Louder leaves rolled-hit MIDI unchanged while base velocity changes. Synthetic domain probe completed; ordinary-note control passed. No product fixes; Windows unverified.” Proposal only: ESTADO.md was not edited and no decision number was allocated.

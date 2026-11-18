<!-- coderabbit-ai-generated:v1 -->

# Additional audit: pattern templates and metadata boundaries

## What was measured

2026-11-18, d-aicore/core, branch `coderabbit/audit-fix-web-ui/029b227d`, initial/final product SHA `ed4642e61b78e30fc4f7d950a28a214d0ac7c801` (0.7.0). Owner request: “continua” in this task. Audit only; no product fixes or ledger edits. Ubuntu 26.04.1, GCC 15.2.0, C++20, unchanged baseline Release domain library. All inputs synthetic and in memory. Previous reports remain unchanged.

Inspected pattern slots, template application/matching, swing metadata, pattern growth, plugin-state preservation and projection. No new plugin-state defect was established. Knowledge-base content was absent in the preceding repository checks. Windows UI and real plugins remain unverified; this native application has no web page through which Chromium could establish its UI behavior.

## Results

IDs continue the report-local BUG-SWEEP namespace, not decision numbers. All remain open.

| ID | Finding | Classification | Priority |
| --- | --- | --- | --- |
| BUG-SWEEP-26 | Loading a user pattern template into a swung slot leaves the displayed swing active while replacing the notes with unswung notes; increasing swing can then do nothing. | Proven domain reproduction; UI wiring inspected. | Medium |
| BUG-SWEEP-27 | A document accepted by the project parser can contain malformed pattern-slot metadata that throws when the pattern list is read. | Proven parser/consumer exception; Windows crash or recovery outcome not tested. | Medium |
| BUG-SWEEP-28 | Template row matching gives exact names priority over musical parts, contradicting the declared API contract and potentially swapping parts. | Proven with conflicting valid row metadata; contract conflict explicitly identified. | Medium |

### BUG-SWEEP-26 — Swing loses its effect after loading a template

`app/domain/src/pattern_rack.cpp:528` calls `setRackBars`, replaces notes using `putNotes`, and returns without reconciling the slot's swing. `setRackSwing` at `:240` assumes existing notes already contain the delay implied by the previous value. In contrast, `rackWriteShape` at `:589` applies the slot's swing after writing fresh notes. The UI calls the template function directly at `app/desktop/src/PatternsWindow.cpp:964`; Swing is wired at `:550`.

Reproduction: one note on the odd sixteenth at tick 240; Swing 0.5 moves it to tick 300. Apply a one-bar user template containing a note at tick 240. The slot remains at Swing 0.5 but the note is now at 240. Increase Swing to 0.75: the note still stays at 240. Set Swing to zero, then 0.75: the note moves to the expected 330. This control shows that the setting works after the stale baseline is reset.

Repair target: reconcile swing when replacing the pattern's notes. Define how already-swung user templates should be handled; blindly swinging every loaded note could double an intentional delay. Preserve the normal zero-to-value behavior and verify a template load while nonzero swing is active. No policy or source change was applied in this audit.

### BUG-SWEEP-27 — extension metadata bypasses the reader's validation boundary

`app/domain/src/document.cpp:654` retains unknown document properties in `extra`. This preservation is intentional for compatibility. `rackSlots` (`pattern_rack.cpp:121`) consumes `extra.patternSlots` using `slotFromJson` (`:36`), whose typed `json.value` calls can throw on damaged field types.

The probe starts with `toJson(newEmptyDocument())` and sets `patternSlots` to an array containing `{id: "s1", bars: "four"}`. `parseDocument` succeeds. Reading `rackSlots` then throws `json.exception.type_error.302`, because bars is a string. `PatternsPanel::documentChanged` calls `refreshSlots` (`PatternsWindow.cpp:654`), and `refreshSlots` calls `rackSlots` at `:667` without a local catch. This establishes the consumer failure, not a measured process crash. Native window construction/open error handling was not executed.

Repair target: validate or conservatively recover known slot metadata at its consumption boundary, retaining unrelated unknown extension properties. Do not remove unknown properties wholesale or claim a particular Windows crash outcome without running it. A regression should cross both parser and consumer, since a parser-only pass misses this case.

### BUG-SWEEP-28 — names override musical parts

`app/domain/src/pattern_rack.hpp` documents `applyRackTemplate` as matching the same part first, falling back to the same name. The internal `matchRows` implementation at `pattern_rack.cpp:507` does the opposite: exact case-insensitive name first, then part. Each row can only be used once, so the first choice changes the remaining mapping too.

Synthetic fixture: destination tracks identified as bass and lead, named Bass and Lead. Template bass row is named Lead and contains pitch 36; template lead row is named Bass and contains pitch 84. Both rows satisfy the current accepted template structure. Applying the template writes pitch 84 into the bass pattern and pitch 36 into the lead pattern. This is a controlled conflict between the two matching keys, not an assertion that normal templates always swap parts.

Contract qualification: changing the implementation to part-first would change existing results for templates with conflicting names/parts. The current header is the baseline contract; this audit does not treat the implementation as authority to silently rewrite it. Confirm the intended priority before a compatibility-affecting fix, then test conflicting metadata and multiple tracks of the same part. Existing unambiguous matches must continue to work.

## Changes and verification

Only this report and its index entry were added in this round. External deliverables include `pattern-probe.cpp` and `pattern.log`. No product source, tests, configuration or dependencies changed; `git diff --name-only HEAD -- app engine tests CMakeLists.txt` returned no paths.

Executed from `/home/vercel-sandbox/workspace/repos/github.com/d-aicore/core`:

```sh
c++ -std=c++20 -pthread -Iapp/domain/src -Iengine/third_party /tmp/core-round5.cpp /tmp/core-audit-build/app/domain/libcore_domain.a -o /tmp/core-round5 && /tmp/core-round5 > /tmp/core-round5.log
```

Final exit 0. Initial probe compilation failed because the scratch code omitted the required songTick argument to `ensureRackSlots`; the probe was corrected and recompiled. This was a probe mistake, not a product defect. Successful execution reproduces the reported problems and the zero-reset swing control. Final transformed normal documents also pass `parseDocument(toJson(...))`. No fixes or green-after-fix result are claimed.

Earlier Release build pass and merge 14/15 suites are reused because product inputs are unchanged. The prior realtime null-device failure remains outstanding. No new full gate, sanitizers, real plugins, native UI or Windows tests in this round. The template cases use the non-builtin API path; built-in P-Core template behavior is not claimed runtime-tested here.

## Pending work and ledger handoff

Open handoff, owner Diogo: triage BUG-SWEEP-26 through BUG-SWEEP-28. Prioritize robust metadata consumption and consistent Swing behavior; settle template matching priority before changing conflicting results. Add focused pre-fix reproductions to any authorized implementation and exercise the Windows Patterns window before claiming end-to-end completion.

Proposed measured-state entry for the ledger scribe: “2026-11-18, owner requested ‘continua’ in task 029b227d-329c-40fe-90ee-220e16fc877c. Three additional open findings at ed4642e: template Swing desynchronization, accepted-document/slot-consumer type error, and template name/part priority conflict. One standalone domain probe completed with synthetic fixtures; no product fixes, Windows unverified.” This is a proposed entry only; ESTADO.md and decision numbering were not changed.

# BUG-SWEEP-22..25 — timeline/tempo continuity (2026-11-18)

- Stream: `flawless-timeline` (external audit drop `report (3).md`, 2026-11-18).
- Worktree: `D:\CORE\CORE\.agents\worktrees\flawless-timeline`, branch `flawless-timeline`, base `ed4642e` (main).
- Build: `D:\Core-build\flawless-timeline` (Ninja, Release, MSVC VS 18 BuildTools, `CORE_BUILD_TESTS=ON`, JUCE `D:/Core-deps/JUCE-9.0.5`).
- Code changed by this stream: **only** `app/domain/src/edit.cpp` and `tests/bug_tests.cpp`. `MainComponent.cpp` was **not** edited (item-1 stream owns it); the finding-25 UI text follow-up is proposed below, not implemented.
- Nothing was pushed or merged. The main checkout `D:\CORE\CORE` stays at `ed4642e`, untouched.
- Disclosure: while this stream worked, the owner committed `c064e46` ("Gate: a unique temp bat per invocation so concurrent gates cannot clobber each other", `ci/gate.ps1` only) directly on this branch. It is not this stream's commit; it was left untouched and the gates below ran with it. Branch HEAD at delivery: `c064e46` above the four fix commits.

## Method and policy

Red first, then the minimal fix, then green — one commit per finding. Every regression compares the
**effective tempo over time** (`bpmAt`) and the **engine seconds** of the projection the app actually
sends (`core::fromJson(doc::toProjection(doc))` + `core::TempoMap(bpm, map, 48000).sampleAt(tick)/48000`),
not only copied point counts, as the audit handoff asked. Saved-form validity is checked with
`parseDocument(toJson(doc))` round-trips.

PM policy defaults applied (the audit explicitly refused to infer policy):

- **22**: pin the copy's effective start tempo; split boundary-crossing ramps at the section edges so both
  curves keep their shape; pin the follower's effective start.
- **23**: tempo moves with the music (the duplicate operation's existing contract, extended to the swap).
- **24**: the inserted empty bars **hold** the tempo at the cut; a moved ramp target re-anchors at
  `at + ticks` and the ramp keeps its slope on both sides of the inserted interval.
- **25**: the documented past-end removal stays (`edit.hpp:333`); a crossing ramp gets a **derived
  in-range endpoint** at `newEnd - 1` with the value the curve had there, so the retained passage keeps
  its glide. Extending the song again does not restore the original target (documented destructive
  behavior; Undo is the restore path).

All plans are built **before** any mutation, so a `kMaxTempoPoints` (64) overflow throws with the
document untouched (all-or-nothing, matching the existing duplicate cap check).

## Red baseline (measured before any fix)

Command (own build dir, no gate lock needed for targeted builds):

```
cmd /c D:\Core-build\flawless-timeline-evidence\build-bug-tests.bat
D:\Core-build\flawless-timeline\tests\bug_tests.exe
```

Output (`evidencia` log `red-bug-tests.log`, all four new tests present, `edit.cpp` still at `ed4642e`):

```
FAIL duplicating a section copies the tempo changes inside it
FAIL BUG-SWEEP-22: a duplicated section starts at its own tempo, boundary ramps keep their shape
FAIL BUG-SWEEP-23: swapping sections moves the tempo with the music
FAIL BUG-SWEEP-24: inserted empty bars hold the tempo, the ramp keeps its shape on both sides
FAIL BUG-SWEEP-25: shortening the song keeps the retained ramp with a derived endpoint
26 testes, 246 verificacoes, 58 falhas
```

The owner's probe (`tempo-probe.cpp` / `tempo.log`, archived verbatim below) measured the same defects
independently; its numbers are quoted per finding.

## BUG-SWEEP-22 — duplicate copies points but loses the initial tempo

Audit (verbatim, `report (3).md`): "The original starts at 120 BPM, but its copy starts at 160 BPM. ...
the code copies explicit tempo points inside A, but never establishes the effective tempo at A's
beginning. ... Copying one extra step indiscriminately is insufficient for boundary-crossing ramps."

Red key numbers: probe `copyStart=160` (expected 120); the existing ratified test measured 160 where it
expected 145 at the copy's first tick; the copy's first bar took 1.5 s of engine time against the
original's 2.0 s (`engineSeconds` delta 0.5 s).

Fix (`edit.cpp`, `duplicateSection` + new helper `splitTempoRampAt`): the copy pins the section's
effective start (`bpmAt(doc, start)`) as a step; a ramp crossing the start edge is split at `start`
(collinear point, curve unchanged at integer ticks); a ramp crossing the end edge is split at `end - 1`
in the original and the copy gets its own tail ramp arriving at `end + len - 1` with the value the
curve had at `end - 1`; the follower pins its effective start (`bpmAt(doc, end)`) when a glide crossed
the end. The copied start point is a **step** (a ramp would re-interpolate the original tail). Empty
tempo map: no pins at all (constant-tempo songs are byte-unchanged). Cap checked before assignment.

Green: `23 testes, 164 verificacoes, 0 falhas` (`green-22.log`). Commit **de72561**.

Adjustment to the existing ratified test ("duplicating a section copies the tempo changes inside it",
translated to English per the stream's English-only rule): the required start pin legitimately grows
the expected count from `before + 2` to `before + 3`, and a new check pins the copy's first-tick tempo
(145 in that song). Coverage grew; nothing was removed.

## BUG-SWEEP-23 — swapped music retains the other section's tempo

Audit (verbatim): "After swapping, the first section is named B but plays at 120 BPM, and the second is
named A but plays at 240 BPM. The serialized tempo map is byte-equivalent as JSON before and after the
operation. ... it is currently an inconsistency between the two section operations."

Red key numbers: probe `firstBpm=120` (expected 240), `secondBpm=240` (expected 120), `mapUnchanged=1`;
engine time of the first four bars 8 s against the expected 4 s.

Fix (`edit.cpp`, `swapSection`): the whole tempo plan is computed before the splits/mutations. B's
interior points translate by `-aLen`, A's by `+bLen`; B's effective start becomes the song's start
(`doc.bpm` when A starts at 0, otherwise a step pin); A's effective start is pinned where A lands;
boundary-crossing glides get `at - 1` / tail ramp pins exactly as in 22; the tempo outside the swapped
region is preserved, with a step pin at `end` when a glide crossed it or the exchanged tail value
differs from the outside value. Unequal lengths and ramps inside both sections are covered by tests.

Green: `24 testes, 189 verificacoes, 0 falhas` (`green-23.log`). Commit **bea172a**.

## BUG-SWEEP-24 — insertion stretches a ramp over earlier music

Audit (verbatim): "The endpoint moves to tick 23040, but there is no tempo boundary inserted at the cut.
Consequently the whole ramp stretches ... Tick 3840, before the insertion: 150 BPM → 140 BPM; Engine
time at tick 3840: 1.78515 s → 1.84981 s ... The tempo inside the newly inserted empty interval needs a
defined policy ... Do not infer a new policy from this audit." (Policy applied: HOLD, per PM default.)

Red key numbers: probe `afterEdit=140` at bar 1 (expected 150), `afterSeconds=1.84981` (expected
1.78515; exact engine values 1.8498081579270993 vs 1.7851484105136781), `movedAt5=220` (expected 210);
insert-delete roundtrip `same=0 bpmAtBar1=140`; the 64-point cap did **not** throw (pins were not
counted because they did not exist).

Fix (`edit.cpp`, `insertTime`): points before the cut are kept untouched; a glide crossing the cut is
split at `at - 1` (curve before the cut preserved); the inserted interval holds the cut's effective
tempo (step pin at `at` when the value jumps or a glide crosses); points at/after the cut translate by
`+ticks` with the point exactly at the cut forced to a step; a crossing ramp re-anchors at `at + ticks`
with the cut's value, so the moved music keeps its slope. Insert at tick 0 holds the song's start tempo
(no tick-0 pin — the map only admits ticks ≥ 1). Cap checked before assignment, document untouched on
throw (tested with a full 64-point map).

Companion fix in `deleteTime` (same commit): after the tick-range filter, duplicate ticks are deduped
(keep the later value, mirroring the existing automation dedup). Without it, inserting and deleting a
**step exactly at the cut** produced two changes on one tick — an invalid saved document. With it, that
inverse edit is byte-identical to the original (`toJson` equality tested).

Green: `25 testes, 228 verificacoes, 0 falhas` (`green-24.log`). Commit **9d8083c**.

Inverse-edit semantics (tested, documented limitation): insert+delete restores `bpmAt` exactly at every
tested tick (the split points are collinear), but the boundary pins the insert added remain, so
`toJson` is **not** byte-identical in the ramp case — Undo (snapshot restore) is the byte-exact path.
The step-at-cut case **is** byte-identical after the dedup. This matches the audit's note that "the
deletion operation cannot infer the original ramp once insertion has changed it".

## BUG-SWEEP-25 — a removed future endpoint also changes retained music

Audit (verbatim): "The retained tick 3840 changes from 150 to 120 BPM; engine time there changes from
1.78515 to 2 seconds. The map becomes empty. ... failure to restore discarded points by extending the
song is documented destructive behavior, not a separate Undo defect. The additional musical problem is
that a future ramp endpoint controls the already-retained passage ... A repair preserving the retained
ramp must respect the rule excluding out-of-song points, for example by deriving an in-range endpoint."

Red key numbers: probe `afterEdit=120` at bar 1 (expected 150), `afterSeconds=2` (expected 1.78515),
`remainingPoints=0` (expected 1); length roundtrip `same=0 bpmAtBar1=120`.

Fix (`edit.cpp`, `setSongLength`): the documented erase of points at/after the new end stays. Before it,
when shortening and the first point at/after the new end is a **ramp**, a derived endpoint is appended
at `newEnd - 1` with `bpmAt(doc, newEnd - 1)` (the value the glide had there), keeping the map sorted
and in-range. The retained passage keeps its curve; the future target is gone as documented. Step
points and extend-only edits are byte-unchanged (tested).

Green: `26 testes, 248 verificacoes, 0 falhas` (`green-25.log`). Commit **7ab1859**.

### Finding-25 UI follow-up (report only — MainComponent.cpp NOT edited)

`app/desktop/src/MainComponent.cpp:3516-3520` counts removed points by map-size difference:

```cpp
const size_t changesBefore = history_.current().tempoMap.size();
edit([&](doc::Document& d) { doc::setSongLength(d, (int64_t)bars * kBar); }, u8("Song length"));
if (const size_t gone = changesBefore - history_.current().tempoMap.size(); gone > 0)
  notice_ = juce::String((int)gone) + u8(gone == 1 ? " tempo change was past the new end and went (Undo brings it back)."
                                                   : " tempo changes were past the new end and went (Undo brings them back).");
```

With the fix, a crossing ramp is replaced by the derived endpoint: one point removed, one added, net
`gone == 0` — the user is told nothing although the future target was replaced. Proposed follow-up for
the item-1 stream (owner of MainComponent): also capture the map before the edit, and when
`gone == 0 && changesBefore > 0 && mapAfter != mapBefore` (the re-anchor case), show:

> "The tempo glide kept its shape to the new end; its target past the end became an endpoint at the new end (Undo brings the original map back)."

The existing `gone > 0` message stays correct for step removals. The audit's complaint that the old
text "does not convey that earlier timing changes too" is resolved by the fix itself: earlier timing
no longer changes.

## Rendered sound change (measured)

All four fixes **change rendered sound** when a tempo map is present (they change tempo continuity,
which moves every tick-grid event). Core's built-in instruments left on 2026-11-16 (`demoProject`
renders silence by design — ratified in `engine_tests`), so the measurement bed is audio clips: a
440 Hz burst on every bar and an 880 Hz burst on every off-bar, placed on the tick grid (tempo moves
clips, never stretches them). Probe: `D:\Core-build\flawless-timeline-evidence\audio-probe.cpp`
(`build-audio-probe.bat`), offline serial render at 48 kHz, 0.1 s tail, float32 WAVs in
`D:\Core-build\flawless-timeline-evidence\audio\`. Pre-fix maps replicated by hand and verified against
`git show ed4642e:app/domain/src/edit.cpp`; post-fix maps produced by the fixed domain operations via
`toProjection`. RMS dB over the whole render, both channels identical (mono bed):

| Finding | Pre-fix WAV sha256 | Post-fix WAV sha256 | RMS dB delta (L=R) | Frames pre → post |
| --- | --- | --- | --- | --- |
| 22 | `aaeed30e6f6f77db66b22f177b5625eb4d0f86b5933e697e6c795574b659515e` | `631f6479fc24ac5825ff5e85ff8477d643294ec1c94edd2219d03f95218f9b4d` | −0.1193 dB | 861861 → 885861 |
| 23 | `9f3512498d1b0bffe2c8db573d62da25018dbb6b5336b13e49e945d07eb07d05` | `45bb72ae5e56e430a13c3bf7649628db5f1d56d44ce80c40553081f523b2b15e` | 0.0000 dB (bursts exchange places; same total energy, different timing — sha differs) | 580800 → 580800 |
| 24 | `5d10c35a8677a577d3ebc7d6ecb726e646008be19e29895938e7d5e930e71067` | `6dd6e7570c97bfebaccbb34d0f783a20cb5d18a7846824edc97ad27127815927` | +0.0372 dB | 596053 → 590969 |
| 25 | `c8200780d82a9674b680cd525eb69778647271170b2e5768d081d72adb054775` | `0d26f42bb80c3f7e034c2bbd9795ad25c193f619b0478c31dd8a06f7acb05a40` | +0.8855 dB | 196800 → 160499 |

Songs with an empty tempo map are sound-neutral (no pins are emitted; tested in 22 and 23).

## Gate (official, under GATE-LOCK)

Lock protocol honored: `D:\Core-build\GATE-LOCK` checked free, created with stream + timestamp before
each run, deleted after each run (both passed). Command, from the worktree:

```
powershell -ExecutionPolicy Bypass -File ci\gate.ps1 -Build D:\Core-build\flawless-timeline -Jobs 6
```

1. First run (fix commits in, evidence archive still untracked): **VERDE**, 17/17 merge, 41 s,
   `@ c064e46 (com alteracoes por commitar)` — the flag was the untracked `evidencia/` directory only.
2. Clean-tree run (evidence archive temporarily staged outside the worktree, `git status` empty):
   **VERDE**, 17/17 merge, **16 s**, `@ c064e46`, no pending-changes flag
   (`gate-resumo-clean.txt`, `gate-clean.log` in the evidence dir).

Version: `core-engine --version` → `0.7.0+c064e46d8a` — equals the branch HEAD sha, **no `.modified`**.
The docs commit that follows changes no code, so the gate result carries.

Merge suite counts, baseline `ed4642e` → now (per-suite exe output; baseline from
`D:/Core-build/estado-ctests-evidence/ed4642e-merge-verbose.txt`):

| Suite | Baseline | Now |
| --- | --- | --- |
| engine_tests | 25 / 130 | 25 / 130 |
| domain_tests | 19 / 148 | 19 / 148 |
| role_tests | 21 / 216 | 21 / 216 |
| midi_tests | 8 / 115 | 8 / 115 |
| bug_tests | 22 / 138 | **26 / 248** |
| bitwig_tests | 2 / 25 | 2 / 25 |
| audit_tests | 7 / 65 | 7 / 65 |
| audit2_tests | 6 / 80 | 6 / 80 |
| hunt_tests | 14 / 649 | 14 / 649 |
| round3_tests | 7 / 82 | 7 / 82 |
| calls_tests | 10 / 44 | 10 / 44 |
| engine_process_tests | 4 / 49 | 4 / 49 |
| **Total (cases / verifs)** | **145 / 1741** | **149 / 1851** |

Plus the 5 CLI tests (cli_version, cli_demo_render, cli_measure, cli_unicode, cli_plugin_crash_render):
17/17 suites pass. No existing suite lost a single verification; the pattern catalogue and the
kick/bass/cymbal alignment contracts (hunt_tests, cli_measure) are untouched and green.

## Proposed ESTADO.md lines (NOT written — ledger scribe only, D72/D80)

Proposals in English per the owner's 2026-11-18 language rule; the scribe adapts them to the ledger's
Portuguese when writing. No D numbers assigned (bugs use the `BUG-` namespace, D75); owner Diogo to
ratify:

- "BUG-SWEEP-22 fixed (`de72561`, flawless-timeline worktree): duplicating a section pins the copy's
  effective start tempo, splits boundary-crossing ramps and pins the following section's start; the
  copy no longer inherits the original's final BPM (160→120 on the probe song)."
- "BUG-SWEEP-23 fixed (`bea172a`): swapping sections moves the tempo with the music (the duplicate
  contract extended to the swap), with unequal lengths and ramps; the map is no longer byte-identical."
- "BUG-SWEEP-24 fixed (`9d8083c`): inserted empty bars hold the cut's tempo (HOLD policy, PM default)
  and a moved ramp re-anchors at `at+ticks` keeping its slope; `deleteTime` dedupes duplicate ticks
  (the step-at-cut inverse edit is byte-identical again). 64-point cap all-or-nothing."
- "BUG-SWEEP-25 fixed (`7ab1859`): shortening the song keeps the documented removal of points past the
  new end, but a ramp crossing the new end gains a derived endpoint at `newEnd-1` — the retained
  passage keeps its curve (150 BPM at bar 1, not 120). Compatibility qualification: extending again
  does not restore the original target (Undo does). UI notification text proposed in the report;
  MainComponent untouched."
- "Gate merge VERDE 17/17 @ `c064e46` (clean tree, 16 s), 149 cases / 1851 verifications / 0 failures
  (baseline ed4642e: 145/1741; +4 tests/+110 verifications in bug_tests only). The four fixes change
  rendered sound when a tempo map is present: sha256 + dB delta measured in the report (audio-clip
  bed; the built-in instruments left on 16/11). Nothing pushed/merged; main intact at ed4642e."

## Evidence inventory

Archived verbatim in `docs/relatorios/evidencia/2026-11-18-timeline-tempo/` (sha256 verified identical
to the `D:\Downloads` originals):

- `report (3).md` — `e48c123a0535e013db74067e8a967c2a6e1dd331b0e779412db580a83e97e58a`
- `tempo-probe.cpp` — `85e8110d11de92a458eebb88f2f2d2f417d9008dc7bb00b3db6f0f04a201794d`
- `tempo.log` — `ec7324b5e5645cae2f0e126d2fa278f9ef7047c7e2b055baf3341e8b8a3e6223`

Measurement logs (outside the repo, `D:\Core-build\flawless-timeline-evidence\`): `red-bug-tests.log`,
`green-22.log`, `green-23.log`, `green-24.log`, `green-25.log`, `full-merge.log`, `suite-counts.txt`,
`gate-console.log`, `gate-clean-console.log`, `gate-resumo-clean.txt`, `gate-clean.log`,
`audio-probe.cpp`, `build-audio-probe.bat`, `audio-probe.log`, `audio/*.wav`.

## Pending work and acceptance suggestions (Test Engineer)

- Independent acceptance on Windows UI (the audit asked for it): duplicate/swap/insert/shorten through
  the app's menus on a song with a ramp crossing the edited boundary; confirm the tempo lane and the
  audible glide. The domain+engine layer is covered here; the UI wiring was read, not driven.
- Finding-25 notification follow-up (above) belongs to the item-1 stream; until it lands, a re-anchored
  ramp shows no notice (net point count unchanged).
- Inverse edit (insert+delete) restores `bpmAt` but not byte-identical JSON in the ramp case — by
  design; Undo is the byte-exact restore. If the owner wants byte-exact inverse edits, that is a new
  policy decision (the pins would have to be marked and stripped).
- The 64-point cap now counts boundary pins: songs near the cap can refuse an insert/duplicate that
  previously fit. All-or-nothing, document untouched; surfaced as `DocumentError("tempoMap", ...)`.
- Musical contracts preserved: pattern catalogue, kick/bass/cymbal alignment (hunt_tests 14/649,
  cli_measure green). No engine code touched.
# Report — 2026-11-18 — Tempo round-trip restores the song (BUG-SWEEP-07)

## What was measured

- Repository / branch / SHA: `d-aicore/core`. Base `ed4642e61b78e30fc4f7d950a28a214d0ac7c801` (main);
  fix commit `43a0d204c1d53f50ac844af152044c86006b8853` (`43a0d20`) on branch `flawless-tempo`, worktree
  `D:/CORE/CORE/.agents/worktrees/flawless-tempo`. The main checkout was never edited; nothing pushed or merged.
- Scope and authorization: the owner's current task instruction (PM "flawless" coordinator dispatch, BUG-SWEEP-07,
  High / data loss) authorizes this exact fix path: store the pre-clamp / original gate in pattern metadata so an
  inverse tempo movement restores the original note lengths exactly, and drop the orphaned pattern copies. The
  "explicit-destructive retiming" alternative is an owner decision and was NOT pursued. Source finding: the deeper
  audit of 2026-11-18 (CodeRabbit drop, archived verbatim below), BUG-SWEEP-07, measured on a Linux probe with the
  real domain functions (`roundtrip.cpp` lines 13-15).
- Environment: Windows host 10.0.26300 (owner's PC, Ryzen 7 7800X3D, 16 logical), MSVC VS 18 BuildTools,
  CMake 4.4.4 + Ninja, Release, JUCE 9.0.5 from `D:/Core-deps/JUCE-9.0.5`. Build `D:\Core-build\flawless-tempo`
  (fresh folder, configured with `VSLANG=1033` + `chcp 65001`; Ninja header deps recorded: 44 outputs with
  `#deps [1-9]`, `domain_tests.cpp.obj` 30 deps VALID). Evidence `D:\Core-build\flawless-tempo-evidence\`.
  Machine-wide gate lock honored: `flawless-batch` held `D:\Core-build\GATE-LOCK` at 05:19:35Z, then
  `flawless-packaging` at 05:32:26Z; this stream waited (~2 min), acquired the lock at 05:37:51Z, ran the gate,
  and released it after (pass). No other actor's lock was touched.
- Tests executed: 7 new domain regression tests (red on the base code, green after the fix), the full
  `domain_tests` suite, and the full merge gate `ctest -L merge` (17 tests) plus a verbose re-run for per-suite
  counters. Commands and exit codes below.
- Limitations: domain tests are not a GUI test — the BPM drag wiring was re-confirmed statically
  (`MainComponent.cpp:1891-1902`, `:2591-2599`, `MainComponent.h:381`, `PatternsWindow.cpp:463-465`) and is
  unchanged; no Windows UI runtime session and no real-device audio in this task. The Linux probe's numbers were
  reproduced analytically and then measured on Windows in the same platform-independent domain code. The gate's
  `cli_demo_render` / `cli_measure` / `cli_plugin_crash_render` exercise rendering on the null device.

## Results

| ID | Problem / result | Evidence (SHA, file:line or run) | Classification | State |
| --- | --- | --- | --- | --- |
| BUG-SWEEP-07 | Separate edits `setTempo` 145 -> 300 -> 145 returned the BPM but not the song: bass gate 3500 -> 1692 ticks, patterns 1 -> 3, `sameContent` false, song left modified with an undo step | Red measured @ `ed4642e` + new tests: `length=1692 patterns=3 equal=0`, 18 failures / 42 checks ([red-run.txt](evidencia/2026-11-18-tempo-roundtrip/red-run.txt)); cause confirmed in code: speed-up clamp `randomize.cpp:342` (base) keeps 3500, slow-down scales unconditionally, copies never removed | proven by test | fixed @ `43a0d20` |
| BUG-SWEEP-07 fix | Round-trip now restores the original pattern exactly: `length=3500 patterns=1 equal=1`, `modified=false`, `canUndo=false` (history cancels the step); drag path unchanged | Green: 7 tests / 42 checks / 0 failures ([green-run.txt](evidencia/2026-11-18-tempo-roundtrip/green-run.txt)); full domain 26 / 190 / 0 ([green-domain-full.txt](evidencia/2026-11-18-tempo-roundtrip/green-domain-full.txt)) | proven by test | validated |
| Contract | ms-gate preservation with the 80%-gap clamp (audit 48c9c38 E8/D4, Diogo 2026-11-04) preserved: retiming NOT removed; the clamp formula is byte-identical (`randomize.cpp:391` after fix); one-way 145 -> 300 still keeps the 3500-tick gate and scales a short 400-tick gate to 828 (ms kept); drums/hats keep their ticks; kick/bass/cymbal alignment untouched (only bass note lengths change, never ticks) | One-way assertions pass on BOTH the old code (red run) and the fixed code (green run); `randomize.hpp:36-48` contract comment extended, owner citation intact | confirmed in code + proven by test | preserved |
| Catalogue | Unmarked patterns are never deleted, even when no clip plays them (the original stays after a one-way change: 2 patterns); only `tempoGate`-marked copies with zero clips are dropped; a track keeps at least one pattern | `randomize.cpp:818-828`; tests "edited source", "walk", one-way count checks | proven by test | preserved |
| No work lost | A copy edited since creation (2000-tick gate) is retimed normally (967 after 145/300), never reverted; a source edited since the copy (3600) is never restored over and survives in the catalogue | Tests at `domain_tests.cpp:610`, `:630`; guards in `revertTempoCopy` (`randomize.cpp:319-340`) compare the canonical note JSON of both sides | proven by test | validated |
| Persistence | The `tempoGate` metadata lives in `Pattern.extra`, which the reader takes back unchanged (`document.cpp:338`, `:435`); a saved tempo copy reopens byte-identical and still restores exactly | Test at `domain_tests.cpp:673` (save/reopen + inverse edit) | proven by test | validated |
| Sound | Sound-neutral: `toProjection` (`document.cpp:721-737`) reads only the referenced patterns' tick/length/pitch/velocity/roll — pattern ids, names and `extra` never reach the engine. One-way note data is byte-identical before/after the fix; the round-trip document is byte-identical to the base (`sameContent` true, hence `sameSound` true); dropped copies were unreferenced and never projected. No rendered input changes, so no audio sha256/dB delta applies | Code confirmation + one-way assertions identical in red and green runs + gate renders pass | confirmed in code + proven by test | sound-neutral |

## Changes

Implementation (fix commit `43a0d20`, three named files):

- `app/domain/src/randomize.cpp` — `notesJson` (`:302`, canonical note serialization through `toJson`, so the
  metadata never drifts from the saved format); `revertTempoCopy` (`:319`, exact-inverse restoration with
  untouched-source and untouched-copy guards, malformed-metadata safe); `retimeBass` (`:365`, takes the stretch
  BPMs instead of ms, tries the revert first, and writes `extra["tempoGate"]` = {source, fromBpm, toBpm,
  sourceNotes, createdNotes} on every new copy, chained to the ultimate source for copy-of-copy);
  `setTempo` (`:795`, drops only marked copies with zero clips after retiming, `:818-828`).
- `app/domain/src/randomize.hpp` — contract comment (`:36-48`) documents the round-trip restoration and the
  no-work-lost guards; the ratified clamp text and the owner citation (Diogo 2026-11-04) are unchanged.
- `tests/domain_tests.cpp` — self-contained BUG-SWEEP-07 block (`:520-690`, own `#include "randomize.hpp"` and
  helpers, appended before `CORE_TEST_MAIN` so the parallel batch-01 appends rebase cleanly): the probe's two
  scenarios (separate edits; single drag with baseline reset) plus five guards (walk 145->300->400->145, edited
  copy, edited source, short-gate ms scaling + multi-note exact restore, save/reopen). 7 tests / 42 checks.

Documentation (this commit): this report, one appended index row in `docs/relatorios/README.md`, the evidence
archives below, and a scoped `.gitattributes` line that keeps the verbatim archive byte-identical.
`ESTADO.md` was NOT edited (D72/D80); proposed lines are inside this report.

Evidence archived verbatim (deeper-audit drop, owner quote inside kept verbatim PT):
`docs/relatorios/evidencia/2026-11-18-deeper-audit/` — `report (1).md`
(sha256 `de7aea126dcdfaaad2b9413ad4532cddda43e442e38db64449b21bed7d54f1e2`), `roundtrip.cpp`
(`e034a173e4638287ea64f752e01f08b2d7c814e03e40aaa58aaa989fd4b67e35`), `roundtrip.log`
(`c52c5789b99d3897aa48670843eb952d30d8a7ce522488e79f8c632fcac35ea5`), `plugin-undo.log`
(`ee8a2c9e0b674bf01ae4695b80dea6a08953fb3f173986bb4768a30cced66424`) — all four hashes identical to the
`D:\Downloads` sources. The two `.log` files are force-added past the repo's `*.log` ignore because this task
names them as deliverables, and a scoped `.gitattributes` line
(`docs/relatorios/evidencia/2026-11-18-deeper-audit/** -text`, the same pattern as `knowledge/aicore/**`) keeps
the archive byte-identical across checkouts (`core.autocrlf=true` would otherwise rewrite LF to CRLF).
This stream's runs: `docs/relatorios/evidencia/2026-11-18-tempo-roundtrip/`
(red-run, green-run, green-domain-full, gate-resumo, gate-merge-verbose). Full build/gate logs:
`D:\Core-build\flawless-tempo-evidence\` and `D:\Core-build\flawless-tempo\gate.log`.

## Verification

Red (measured on the base code `ed4642e` with only the new tests added; build id `0.7.0+ed4642e61b.modified`,
tests not yet committed — declared):

```
cmd /c D:\Core-build\flawless-tempo-evidence\build-domain-tests.bat   (vcvars64 + cmake configure + --target domain_tests -j 4)  -> exit 0
D:\Core-build\flawless-tempo\tests\domain_tests.exe "BUG-SWEEP-07"    -> exit 1
  FALHOU ...:561: tempoActiveNote(h.current()).length == (int64_t)3500: medido 1692, esperado 3500
  FALHOU ...:562: h.current().tracks[0].patterns.size() == (size_t)1: medido 3, esperado 1
  FALHOU ...:563: sameContent(base, h.current())
  FALHOU ...:565: !h.modified()
  FALHOU ...:566: !h.canUndo()
     (round-trip: length=1692 patterns=3 equal=0)
FAIL tempo round-trip BUG-SWEEP-07: separate edits 145 -> 300 -> 145 restore the song exactly
ok   tempo round-trip BUG-SWEEP-07: a single drag with the baseline reset restores the song
7 testes, 42 verificacoes, 18 falhas
```

The red numbers match the Linux probe exactly (`roundtrip.log`: `activeLength=1692`, `after=3`, `equal=0`);
analytically: kPpq 960, gap 3840 -> clamp floor max(3500, 3072) = 3500 on speed-up, then
llround(3500 * 145/300) = 1692 on slow-down. The walk test also red: 1269 ticks / 4 patterns. The drag scenario
was already green at baseline (the UI resets from `tempoBase_`), as the probe measured.

Green (after the fix, same commands):

```
build-domain-tests.bat                                                -> exit 0
domain_tests.exe "BUG-SWEEP-07"                                       -> exit 0
     (round-trip: length=3500 patterns=1 equal=1)
7 testes, 42 verificacoes, 0 falhas
domain_tests.exe (full)                                               -> exit 0
26 testes, 190 verificacoes, 0 falhas        (baseline 19 / 148: +7 tests / +42 checks, none removed)
```

Gate (machine-wide lock held by this stream 05:37:51Z -> released after; `ci/gate.ps1` unchanged):

```
powershell -ExecutionPolicy Bypass -File ci\gate.ps1 -Build D:\Core-build\flawless-tempo -Jobs 6   -> exit 0
Gate do Core: VERDE | Codigo: ...\flawless-tempo @ 43a0d20 | Testes: merge | Tempo: 270 s
100% tests passed out of 17
ctest --test-dir D:\Core-build\flawless-tempo -L merge -V --output-on-failure --timeout 600        -> exit 0 (13.81 s)
```

Per-suite counts (measured, verbose run) vs the ed4642e baseline:

| Suite | ed4642e baseline | this gate @ 43a0d20 | delta |
| --- | --- | --- | --- |
| engine_tests | 25 / 130 | 25 / 130 | 0 |
| domain_tests | 19 / 148 | 26 / 190 | +7 / +42 (this fix) |
| role_tests | 21 / 216 | 21 / 216 | 0 |
| midi_tests | 8 / 115 | 8 / 115 | 0 |
| bug_tests | 22 / 138 | 22 / 138 | 0 |
| audit_tests | 7 / 65 | 7 / 65 | 0 |
| audit2_tests | 6 / 80 | 6 / 80 | 0 |
| hunt_tests | 14 / 649 | 14 / 649 | 0 |
| round3_tests | 7 / 82 | 7 / 82 | 0 |
| calls_tests | 10 / 44 | 10 / 44 | 0 |
| bitwig_tests | 2 / 25 | 2 / 25 | 0 |
| engine_process_tests | 4 / 49 | 4 / 49 | 0 |
| **Counting total** | **145 / 1741 / 0** | **152 / 1783 / 0** | **+7 / +42** |
| cli_version / cli_demo_render / cli_measure / cli_unicode / cli_plugin_crash_render | pass | pass | 0 |
| **ctest -L merge** | **17/17** | **17/17** | coverage not shrunk |

Baseline denominator note: the task pointed at `D:\Core-build\flawless-crash-evidence\baseline-suite-counts.txt`.
That file matches ed4642e on 11 of 12 suites, but its `engine_process_tests: 8 / 93` is the flawless-crash
stream's MIXED state (4 extra crash cases that do not exist at ed4642e). The authoritative measured ed4642e state
is `docs/relatorios/2026-11-18-estado-ctests.md` (branch `flawless-estado`, commit `3a25a71`; raw logs
`D:\Core-build\estado-ctests-evidence\`): 17/17, 145 cases / 1741 verifications, engine_process 4 / 49. This gate
meets or exceeds every ed4642e suite; the 8/93 row is not a denominator this branch can or should match.

Version: `cli_version` prints `core-engine 0.7.0+43a0d204c1`; `generated/core_version.h` has
`CORE_GIT_MODIFIED 0`, `CORE_BUILD_ID "0.7.0+43a0d204c1"` — version == sha, no `.modified`. The gate resumo's
"(com alteracoes por commitar)" flag counts untracked files (this stream's evidence folders, not yet committed at
gate time); the build id uses `--untracked-files=no` and is clean. Tracked code at gate time was exactly `43a0d20`.

Sound: **sound-neutral** (see Results table). No rendered input changes: one-way note data identical before/after,
round-trip byte-identical to the base, dropped copies unreferenced, pattern `extra` outside the projection. No
audio sha256/dB delta is applicable because no sound change exists to report; the gate's three render tests pass.

## Pending and handoff

- Integration: branch `flawless-tempo` @ `43a0d20` (+ the docs commit) is ready; landing on `main` is the owner's
  step. The shared main checkout carries other actors' uncommitted `ESTADO.md` / `docs/relatorios/README.md` rows —
  do not force integration over them. batch-01 also appends to `tests/domain_tests.cpp`; this block is
  self-contained before `CORE_TEST_MAIN` (own include/helpers), a merge rebase at integration is expected.
- For the Test Engineer: accept on Windows with the two probe scenarios (separate edits and drag) in the real app
  (Bass track with a long gate, type 300 then 145, watch the gate length, the pattern list and the undo/dirty
  indicators); the domain regression is `domain_tests.exe "BUG-SWEEP-07"`. Runtime conditions: null audio device is
  enough for the domain proof; a real-device session and the GUI drag remain unexercised here.
- Remaining audit items BUG-SWEEP-08..15 are other streams' scope; nothing in this fix touches them. The
  "explicit-destructive retiming" alternative stays an owner decision, not pursued.
- Residual risk: non-inverse multi-step walks (e.g. 145->300->400->300) keep the legacy compounding scaling for
  clamped gates (path-dependent); an exact return to the original BPM always restores exactly. Making every walk
  path-independent would be a behavior change beyond this fix's authorization.

**PROPOSED lines for `ESTADO.md` (NOT written — D72/D80, only the ledger scribe writes it; owner = Diogo):**

- *Estado medido:* "BUG-SWEEP-07 (18/11, deeper audit; fix path authorized by the owner's task instruction):
  `setTempo` round-trip 145 -> 300 -> 145 in separate edits restored the BPM but not the song (bass gate
  3500 -> 1692 ticks, patterns 1 -> 3). Fixed @ `43a0d20` (branch `flawless-tempo`): each retimed copy keeps its
  source and both note sides in `extra['tempoGate']`; an exact inverse restores the original pattern (gates, name,
  ids) and orphaned marked copies are dropped; edited copies/sources are never restored over. Ratified ms-gate
  contract preserved (audit 48c9c38 E8/D4, Diogo 2026-11-04, as recorded in randomize.hpp:36-43; the 80%-gap
  clamp is untouched); catalogue and kick/bass/cymbal alignment preserved; sound-neutral. Regression: red measured
  1692 / 3 patterns / sameContent false @ `ed4642e` (18 failures / 42 checks), green 3500 / 1 pattern /
  sameContent true; domain 26 / 190 / 0. Gate merge VERDE 17/17, 152 cases / 1783 verifications / 0 failures
  @ `43a0d204c1` (version == sha, no `.modified`), 270 s; baseline ed4642e 145 / 1741, +7 / +42 all in domain.
  Report: docs/relatorios/2026-11-18-tempo-roundtrip.md."
- *Pendência (owner = Diogo):* "Land `flawless-tempo` on main (owner's remote step; shared checkout has other
  actors' uncommitted ledger/index rows). Windows GUI drag + real-device acceptance of the round-trip outstanding
  (domain proven; UI wiring unchanged). BUG-SWEEP-08..15 remain open in their streams. The explicit-destructive
  retiming alternative stays a proposal pending owner decision."
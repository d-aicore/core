# BUG-SWEEP-29 — tempo-only Randomize keeps unarranged Patterns-window ideas

- Date: 2026-11-18
- Branch: `flawless-randomize` (worktree `D:/CORE/CORE/.agents/worktrees/flawless-randomize`), baseline `ed4642e`
- Fix commit: `577a0d8` — "Data: tempo-only Randomize keeps unarranged Patterns-window ideas (BUG-SWEEP-29)"
- CI gate fix cherry-picked after the fix commit: `fffbb41` -> `5143390` (unique temp bat per gate invocation)
- Build: `D:/Core-build/flawless-randomize`; evidence: `D:/Core-build/flawless-randomize-evidence/`
- Scope: `app/domain/src/randomize.cpp` (pruning loop), `app/domain/src/edit.cpp` + `edit.hpp`
  (`removeUnusedPatterns`), `tests/domain_tests.cpp` (one new regression test). `MainComponent.cpp` NOT edited.
  No DSP, no engine code, no built-in instruments reintroduced.

## The bug (confirmed in code + proven by test)

`randomizeStyle` prunes the 8 drum/bass part tracks after any groove or tempo randomization
(`randomize.cpp:682-683` at `ed4642e`; `tempoOn` at :372, `grooveOn` excludes tempo/sound at :429).
`removeUnusedPatterns` (`edit.cpp:560-566`) kept a pattern only when `clipsUsing(p.id) > 0` — timeline clips.
It takes `Track&` and has no access to `Document.extra.patternSlots`, the Patterns window's rack references.
A slot's patterns are the tracks' own and need NOT be timeline clips (`pattern_rack.hpp:1-8` contract); there is
no requirement to send an idea to Arrange for it to survive Randomize. So a tempo-only Randomize deleted every
rack-only idea on those tracks; `rackSlots` (`pattern_rack.cpp:128-132`) then silently erased the slot's ref and
`rackPatternFor` (:176-189) recreated an EMPTY pattern on reopen. The UI always commits a tempo Randomize
(tempo always yields at least one line; `MainComponent.cpp:4366/:4383`), so the loss landed in the undo history
as a committed step — work preservation, quality bar #1.

## Red (measured on the unfixed code, test added first)

Test: `TEST("randomize: tempo-only keeps an unarranged Patterns-window idea (BUG-SWEEP-29)")` in
`tests/domain_tests.cpp` (self-contained; ports the owner-side probe archived below). Song:
`newTemplateDocument` + `extra.template.style="Dark psycore"` + one section spanning the song +
`ensureRackSlots` + `addRackSlot("Unarranged idea")` + `rackPatternFor` bass idea holding one roll note
(count 4, velocity ramp 0.3 -> 0.7), never sent to Arrange.

Command (build then run, filtered):

```
cmd /c D:\Core-build\flawless-randomize-evidence\red-build.bat   (cmake configure + --target domain_tests)
D:\Core-build\flawless-randomize\tests\domain_tests.exe "BUG-SWEEP-29"
```

Red output (rc=1, verbatim; log `red-test.log`):

```
  FALHOU D:\CORE\CORE\.agents\worktrees\flawless-randomize\tests\domain_tests.cpp:554: kept != nullptr
  FALHOU D:\CORE\CORE\.agents\worktrees\flawless-randomize\tests\domain_tests.cpp:566: slot->tracks.size() == (size_t)1: medido 0, esperado 1
  FALHOU D:\CORE\CORE\.agents\worktrees\flawless-randomize\tests\domain_tests.cpp:567: slot->tracks.count("bass") == (size_t)1: medido 0, esperado 1
  FALHOU D:\CORE\CORE\.agents\worktrees\flawless-randomize\tests\domain_tests.cpp:572: reopened.notes.size() == (size_t)1: medido 0, esperado 1
  FALHOU D:\CORE\CORE\.agents\worktrees\flawless-randomize\tests\domain_tests.cpp:577: reloadedIdea->notes.size() == (size_t)1: medido 0, esperado 1
FAIL randomize: tempo-only keeps an unarranged Patterns-window idea (BUG-SWEEP-29)
1 testes, 17 verificacoes, 5 falhas
```

Red numbers: idea pattern DELETED (`kept == nullptr`), slot track refs 0 (expected 1), reopening the slot
0 notes (expected 1). The Undo control passed even in red (undo restores the deleted idea — the loss is a
committed step, matching the probe's `undo ideaRestored=1`). The archived probe measured the same shape:
`afterExists=0 rackRefs=0`, `reopening slot notes=0`.

## The fix (minimal, rack-aware used-check)

Preferred option from the brief: protect EVERY supported reference when pruning.

- `edit.hpp:128` / `edit.cpp:560`: `removeUnusedPatterns(Track&, const std::set<std::string>& rackRefs = {})`
  keeps a pattern when `clipsUsing(p.id) > 0 || rackRefs.count(p.id) > 0`. Default `{}` preserves the old
  behaviour for any future caller; the fallback `patterns.front()` and the at-least-one rule are unchanged.
- `randomize.cpp` (pruning loop, now :683-691): before pruning, collect per-track pattern ids from
  `rackSlots(doc)` (all slots, all tracks) and pass each track's set. Runs on every axis that prunes
  (groove or tempo), so groove Randomize is protected too. `#include "pattern_rack.hpp"` added.

`removeUnusedPatterns` has exactly ONE caller (`randomize.cpp`), so the signature change is cheap.
No arrangement-only reachability test is satisfied by deleting unarranged ideas any more.

## Green (measured after the fix)

```
D:\Core-build\flawless-randomize\tests\domain_tests.exe "BUG-SWEEP-29"   -> rc=0
ok   randomize: tempo-only keeps an unarranged Patterns-window idea (BUG-SWEEP-29)
1 testes, 20 verificacoes, 0 falhas

D:\Core-build\flawless-randomize\tests\domain_tests.exe                  -> rc=0
20 testes, 168 verificacoes, 0 falhas
```

Green numbers: idea pattern + its roll note SURVIVE (`kept != nullptr`, 1 note, roll count 4), slot keeps its
1 track ref, reopening the slot returns the SAME pattern id with its notes (not empty),
`parseDocument(toJson(...))` round-trips the idea + slot ref, and the Undo control restores the EXACT
pre-transform document (`canon(history.current()) == before`, full JSON equality).

Coverage: domain_tests 19 cases / 148 checks at `ed4642e` -> 20 / 168 (+1 case, +20 checks). No test removed,
no criterion relaxed.

## Interaction with BUG-SWEEP-07 (flawless-tempo @43a0d20)

Verified against `git diff ed4642e..43a0d20`: 07 touches only `retimeBass`/`setTempo` (+`randomize.hpp`,
`domain_tests.cpp`); the `randomizeStyle` pruning loop is byte-identical and `edit.cpp` is unchanged by 07.
Randomize's tempo axis calls `setTempoMap` + in-section `retimeBass`, never `setTempo`, so 07's marked-copy
cleanup does not run on the Randomize path. This fix does not touch `retimeBass`/`setTempo` and only ADDS
protection to the used-check (default `{}` = old behaviour), so 07's marked-copy semantics are preserved.
07's 7 regression tests build rack-less documents via `addTrack` and drive `setTempo` through `History`;
they never call `removeUnusedPatterns` or `rackSlots`, so they are unaffected (confirmed by reading their
diff; they are not on this branch and were not run here — they must stay green at integration).
Integration note: both branches append tests before `CORE_TEST_MAIN` in `domain_tests.cpp`; expect a trivial
adjacent-append rebase conflict there. Other delivered branches touch different regions
(flawless-timeline `edit.cpp` section ops; flawless-pattern `pattern_rack.cpp`; flawless-deadcode dead-API
removal) — none overlaps `removeUnusedPatterns` or the pruning loop.

## Full gate

GATE-LOCK protocol honoured. First run (previous turn): acquired `flawless-randomize 2026-11-18T06:47:48Z`,
compiled the full tree and reached VERDE, but the turn ended before verification; the PM cleared the stale
lock after confirming no gate process was running. Final clean run (this turn): lock free at first check,
held by `flawless-menu 2026-11-18T06:55:03Z` at acquisition, re-checked until free, acquired
`flawless-randomize 2026-11-18T06:57:55Z`, released after the gate and the verbose count pass (06:58:25Z).
Forward slashes used; gate not piped to tail.

```
powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/flawless-randomize -Jobs 6
```

**VERDE, exit code 0.** `gate-resumo.txt`: "100% tests passed out of 17", Tempo 16 s. The run was incremental:
the first run had already compiled every object at this same HEAD, so this run re-ran configure + build
(`ninja: no work to do`) + the Ninja header-dependency guard + the full `ctest -L merge` end-to-end.
A second pass, `ctest --test-dir D:/Core-build/flawless-randomize -L merge -V --output-on-failure --timeout 600
--no-tests=error`, also exited 0: 17/17, total test time 13.63 s (log `merge-verbose.log`).

Per-suite counts (verbose pass) vs the authoritative ed4642e baseline
(`docs/relatorios/2026-11-18-estado-ctests.md`, delivered on `flawless-estado`; 145 cases / 1741 verifications):

| Suite | ed4642e (cases/verifs) | this run (cases/verifs) | delta |
| --- | --- | --- | --- |
| engine_tests | 25/130 | 25/130 | 0 |
| domain_tests | 19/148 | 20/168 | +1/+20 |
| role_tests | 21/216 | 21/216 | 0 |
| midi_tests | 8/115 | 8/115 | 0 |
| bug_tests | 22/138 | 22/138 | 0 |
| audit_tests | 7/65 | 7/65 | 0 |
| audit2_tests | 6/80 | 6/80 | 0 |
| hunt_tests | 14/649 | 14/649 | 0 |
| round3_tests | 7/82 | 7/82 | 0 |
| calls_tests | 10/44 | 10/44 | 0 |
| bitwig_tests | 2/25 | 2/25 | 0 |
| engine_process_tests | 4/49 | 4/49 | 0 |
| **counting suites total** | **145/1741** | **146/1761** | **+1/+20** |
| cli_version, cli_demo_render, cli_measure, cli_unicode, cli_plugin_crash_render | pass (no counts printed) | pass (no counts printed) | 0 |

0 failures everywhere. The only delta is the new BUG-SWEEP-29 regression in domain_tests (+1 case, +20 checks);
engine_process stays 4/49 and every other suite is exactly at baseline — no coverage shrunk, no criterion relaxed.

Version check: `D:/Core-build/flawless-randomize/engine/core-engine.exe --version` prints
`core-engine 0.7.0+5143390a74` — HEAD `5143390a748f5d132f9bee6ea6a627da441b52de` (short=10), **no `.modified`**:
the tracked tree is clean (`git status --porcelain --untracked-files=no` empty; the version header uses that same
flag). Note: `gate-resumo.txt` says "(com alteracoes por commitar)" because gate.ps1's dirty marker uses plain
`git status --porcelain`, which counts the untracked report/evidence files; those do not affect the build ID.

## Evidence

- `D:/Core-build/flawless-randomize-evidence/`: `red-build.bat`, `red-build.log`, `red-test.log`,
  `green-build.log`, `green-test.log`, `green-domain-full.log`, `gate-run.log` (verbatim console of the final
  clean gate), `gate-run-first-partial.log` (console of the first run, kept), `gate.log` / `gate-resumo.txt`
  (copies from the build dir), `merge-verbose.log` (`ctest -L merge -V`, per-suite count lines),
  `gate-rc.txt` (exit codes, lock acquisition, timestamps).
- Archived verbatim (byte-compared with `cmp`) into `docs/relatorios/evidencia/2026-11-18-randomize/`:
  `report (5).md`, `edit-probe.cpp`, `edit.log` (owner-side probe: `afterExists=0 rackRefs=0`,
  `undo ideaRestored=1`, `reopening slot notes=0`).

## Proposed ESTADO.md lines (NOT written — only the ledger scribe writes ESTADO.md, D72/D80)

Estado medido (proposal):

> BUG-SWEEP-29 corrigido em `flawless-randomize` @ `577a0d8` (base `ed4642e`): Randomize (qualquer eixo que
> poda, incl. só tempo) já não apaga ideias do Patterns window sem clip na timeline — `removeUnusedPatterns`
> conta as referências dos slots (`rackSlots`) como usadas. Vermelho medido primeiro (ideia apagada, 0 refs,
> 0 notas ao reabrir; 5 falhas), verde depois (20 verif.); undo restaura o documento exacto e o round-trip
> guardar/reabrir mantém a ideia. domain_tests 19/148 -> 20/168; gate merge VERDE 17/17 (146 casos / 1761 verif. / 0 falhas)
> @ `5143390` (inclui cherry-pick de `fffbb41`, bat único por gate). Testes do BUG-SWEEP-07 intactos por
> construção (regiões disjuntas); rebase na integração esperado. Fonte: relatório
> docs/relatorios/2026-11-18-randomize-idea-preservation-29.md.

Pendência (proposal): integrar `flawless-randomize` com `flawless-tempo` (07), `flawless-timeline`,
`flawless-pattern`, `flawless-deadcode` — conflito adjacente esperado apenas no apêndice de
`tests/domain_tests.cpp`; correr os 7 testes do 07 após o rebase. Owner: Diogo.

## Limits / not verified

- Runtime UI behaviour (Patterns window open/reopen after Randomize in the running app) not exercised here;
  proven at the domain layer the window reads (`rackSlots`/`rackPatternFor`), which is the window's own API.
- 07's regression tests were not run on this branch (they live on `flawless-tempo`); non-interference is
  confirmed in code, to be re-proven at integration.
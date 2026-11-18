# Report — 2026-11-18 — BUG-SWEEP-16: write safety of MIDI export, pattern preset, rack template and roles

## What was measured

- Repository / branch / SHA: `d-aicore/core`, worktree `D:/CORE/CORE/.agents/worktrees/flawless-writesafety`,
  branch `flawless-writesafety`, baseline `ed4642e` (main). Fix commit `7ef828d`; CI unique-bat gate fix
  cherry-picked as `3d5b2a2` (= `fffbb41`); this report is the docs commit on top. Main checkout untouched.
- Scope and authorization: BUG-SWEEP-16 (High, work preservation), finding 16 of the consolidated master list,
  verbatim: `| 16 | High | MIDI/preset late write failures return success with empty files | Fault injection; Linux replacement | R3 |`.
  Narrower than project save/autosave (already safe via `writeFileSafely`): only the four writers
  `saveMidiFile` (midi.cpp), `savePatternPreset` (pattern_tools.cpp), `saveRackTemplate` (pattern_rack.cpp),
  `saveRoleMap` (roles.cpp). Product fact honored: Core has no built-in instruments/effects (removed 2026-11-16);
  nothing here touches sound generation. User files are never truncated.
- Environment: Windows host (10.0.26300), MSVC VS 18 BuildTools Release / Ninja, CMake + ctest 4.4.4,
  JUCE 9.0.5 from `D:/Core-deps/JUCE-9.0.5`. Build `D:/Core-build/flawless-writesafety`;
  evidence `D:/Core-build/flawless-writesafety-evidence/`.
- Tests executed: red and green runs of `midi_tests`, `bug_tests`, `role_tests`, `domain_tests` (full suites,
  no filter); full `ci/gate.ps1 -Build D:/Core-build/flawless-writesafety -Jobs 6` (merge label);
  `ctest -L merge -V --output-on-failure` for the per-suite counters; `core-engine --version`.
- Limitations: the late-write fault is injected (test-only seam, below), not a real full disk; it reproduces
  the exact observable state of a failed final flush (bytes lost, stream reports no error). Real-device audio
  and UI click-through are out of scope; the four UI callers were read and all catch `std::exception`
  (MainComponent.cpp:4816/4843 export, :5162 preset; PatternsWindow.cpp:992 template; Settings.cpp:542 roles),
  so the wrapped `DocumentError` reaches the existing error notices (catch sites: MainComponent.cpp:4820 and
  :4865, MainComponent.cpp:5165, PatternsWindow.cpp:996, Settings.cpp:543). No sanitizer round in this task.

## The bug, confirmed in code at ed4642e

- `midi.cpp:265-276` (`saveMidiFile`): `if (!out)` at :271 runs BEFORE the ofstream destructor flush/close;
  `fs::rename` at :274 then replaces the user's existing `.mid` with no backup. A late flush/close failure
  leaves an empty/truncated file reported as success.
- Same class: `pattern_tools.cpp:396-400` (preset written directly to its final file), `pattern_rack.cpp:495-498`
  (template written directly), `roles.cpp:121-127` (temp + rename, check before the destructor flush, no backup).
- Safe contrast reused: `storage.cpp:173-198` `writeFileSafely` (explicit close + check :183-184,
  `FlushFileBuffers` :63-74, read-back :188-189, backup replace :117-140).

## The fix (commit 7ef828d)

All four writers now serialize their content to a `std::string` and go through `writeFileSafely`:

- `saveMidiFile`: the bytes as a string; the check is byte equality (enforced inside `writeFileSafely` by the
  read-back comparison); the check lambda returns true.
- `savePatternPreset` / `saveRackTemplate` / `saveRoleMap`: JSON text; the check is a parse round-trip
  (`presetFromJson` / `rackTemplateFromJson` / `roleMapFromJson` on what is on disk).
- `SaveError` is caught in each writer and rethrown as the `DocumentError` that writer's callers and tests
  already expect (`midi` / `file` / `file` / `roles` field, previous message prefix kept, system detail appended).
- On any write/flush/read-back failure: the writer throws, the temporary file is discarded, the user's existing
  target stays byte-identical, no empty file is left. On success the previous content is kept as `<file>.bak`
  (ReplaceFileW on Windows). The id-dedup logic of preset/template (never over one: -2, -3...) is unchanged;
  the safe replace additionally covers a race where another window created the file meanwhile.
- `storage.cpp` / `storage.hpp`: ONLY the additive test seam (below) and one no-op-when-unarmed call inside
  `writeFileSafely`. `saveFile` / autosave logic untouched (domain_tests 19/148/0 unchanged, red and green).
- Header comments updated to the new contract (midi.hpp, pattern_tools.hpp, pattern_rack.hpp, roles.hpp).

## The test-only fault hook (no production behavior change)

Precedent: `setReplaceStepForTests` (storage.hpp:48) and the `_wfsopen(_SH_DENYRW)` lock trick
(domain_tests.cpp:246). Added in storage.hpp/cpp:

- `using WriteFault = void (*)(std::ostream&, const std::filesystem::path&);`
- `setWriteFaultForTests(fault)` (nullptr clears) and `writeFaultForTests(out, file)` — a no-op unless a fault
  is set; nothing in production ever sets one.
- `writeFileSafely` calls it after the bytes are in the stream and before `out.close()`.
- The tests' fault `lateFlushFault` flushes the open stream (bytes reach the system) and then truncates the
  file behind it with a second `ofstream(..., trunc)` — the state a disk that fails at the final flush leaves:
  an empty file, while the stream itself reports no error. This is exactly what the pre-fix `if (!out)` check
  could not see. For the RED measurement the same hook was called at the same point of the four old direct-write
  paths; after the fix those paths are gone and the single call site is inside `writeFileSafely`.

## Results

| ID | Problem / result | Evidence (SHA, file:line or run) | Classification | State |
| --- | --- | --- | --- | --- |
| BUG-SWEEP-16 | Late write failure answered success with an empty file; target overwritten without backup (4 writers) | red logs @ ed4642e+seam: `threw` false, `bytesIn(file) == before: medido , esperado MThd` | test (red measured) | corrected |
| BUG-SWEEP-16 fix | Throws DocumentError; target byte-identical; no empty/temp file; .bak on success | green logs @ 7ef828d: 5 new tests ok, 0 failures | test (green measured) | validated |
| Gate | VERDE 17/17 merge @ 3d5b2a2, 218 s, 150 cases / 1785 verifs / 0 fails | gate-resumo.txt, green-ctest-verbose.log | test | validated |
| Version | `core-engine 0.7.0+3d5b2a2a24` == HEAD 3d5b2a2, no `.modified`, tree clean at gate | core_version.h, gate-resumo.txt | test | validated |
| Save/autosave | Unchanged (seam additive; domain_tests 19/148/0 red and green) | red/green-domain_tests.log | code + test | validated |

## Verification (commands and exit codes)

Red (pre-fix code + seam + tests; build rc=0), each suite run in full from `D:/Core-build/flawless-writesafety/tests`:

```
midi_tests.exe    rc=1  10 testes, 128 verificacoes, 6 falhas   (2 new tests FAIL)
bug_tests.exe     rc=1  25 testes, 168 verificacoes, 16 falhas  (3 new tests FAIL)
role_tests.exe    rc=0  21 testes, 216 verificacoes, 0 falhas
domain_tests.exe  rc=0  19 testes, 148 verificacoes, 0 falhas
```

Key red lines (the finding itself): `FALHOU ...: threw` (success reported) and
`bytesIn(file) == before: medido , esperado MThd` (the user's MIDI emptied); roles file emptied
(`loadRoleMap -> 0 roles + problem`); empty `second.json` / `mine.core-patterns.json` left in the library;
no `.bak` on overwrite. Full output: `red-*.log`, `red-summary.txt` in the evidence dir.

Green (after the fix; build rc=0), same commands:

```
midi_tests.exe    rc=0  10 testes, 129 verificacoes, 0 falhas
bug_tests.exe     rc=0  25 testes, 168 verificacoes, 0 falhas
role_tests.exe    rc=0  21 testes, 216 verificacoes, 0 falhas
domain_tests.exe  rc=0  19 testes, 148 verificacoes, 0 falhas
```

Full gate (GATE-LOCK held; forward slashes; not piped to tail):

```
powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/flawless-writesafety -Jobs 6
rc=0; gate-resumo.txt: Gate do Core: VERDE; Codigo: ...flawless-writesafety @ 3d5b2a2; Tempo: 218 s;
100% tests passed out of 17
ctest --test-dir D:/Core-build/flawless-writesafety -L merge -V --output-on-failure --timeout 600  -> rc=0
core-engine.exe --version -> core-engine 0.7.0+3d5b2a2a24   (== HEAD 3d5b2a2, no .modified)
```

Coverage vs `docs/relatorios/2026-11-18-estado-ctests.md` (baseline ed4642e: 145 cases / 1741 verifs,
engine_process 4/49; that report lives on branch flawless-estado @ 3a25a71, not yet in main):

| Suite | ed4642e | this branch | delta |
| --- | --- | --- | --- |
| engine_tests | 25/130 | 25/130 | 0 |
| domain_tests | 19/148 | 19/148 | 0 |
| role_tests | 21/216 | 21/216 | 0 |
| midi_tests | 8/115 | 10/129 | +2/+14 |
| bug_tests | 22/138 | 25/168 | +3/+30 |
| audit_tests | 7/65 | 7/65 | 0 |
| audit2_tests | 6/80 | 6/80 | 0 |
| hunt_tests | 14/649 | 14/649 | 0 |
| round3_tests | 7/82 | 7/82 | 0 |
| calls_tests | 10/44 | 10/44 | 0 |
| bitwig_tests | 2/25 | 2/25 | 0 |
| engine_process_tests | 4/49 | 4/49 | 0 |
| **Total (counting suites)** | **145/1741/0** | **150/1785/0** | **+5/+44/0** |
| ctest merge label | 17/17 | 17/17 | 0 |

No test removed, no criterion relaxed. The five `cli_*` tests print no counters (by design); they pass.

Gate-lock notes (transparency): the lock was held by `flawless-crash` (acquired 07:11:32Z) with its gate logs
idle since 06:14Z and zero builder processes (Get-CimInstance scan) — cleared as stale per the documented
recovery, evidence in `gate-lock-recovery.txt`. My gate ran 07:31-07:36Z VERDE. At release time the file held
a newer crash-stream claim (07:35:46Z); it was restored verbatim immediately. My gate carries the unique-bat
fix (3d5b2a2), so it is collision-independent.

## Changes (files and reason)

Implementation (commit 7ef828d): `app/domain/src/midi.cpp` (+midi.hpp comment), `pattern_tools.cpp` (+hpp),
`pattern_rack.cpp` (+hpp), `roles.cpp` (+hpp) — the four writers routed through `writeFileSafely` with the
DocumentError wrapping; `storage.cpp`/`storage.hpp` — additive test-only seam only.
Tests (same commit): `tests/midi_tests.cpp` (+2 cases), `tests/bug_tests.cpp` (+3 cases, includes for
pattern_rack/roles). CI (cherry-pick 3d5b2a2 = fffbb41): `ci/gate.ps1` unique temp bat.
Documentation (this commit): this report, one index row in `docs/relatorios/README.md`, verbatim archive in
`docs/relatorios/evidencia/2026-11-18-writesafety/` (`consolidated-list.md` SHA-256
`002e6f105b2cac908997d0953e8b653e8070991813cb85e5ab6d4339e0b9d25c`, `recovery.log` SHA-256
`2f7c482051ef09293a7d1864d837e27413a99443e0a69540fdceb9426135917b` — identical to `D:/Downloads` originals).
Build evidence (outside the tree): `D:/Core-build/flawless-writesafety-evidence/` — configure/build logs,
red-*.log + red-summary.txt, green-*.log + green-summary.txt, gate-console.log, gate.log, gate-resumo.txt,
green-ctest-verbose.log, gate-lock-recovery.txt.

## PROPOSED ESTADO.md lines (D72/D80 — the scribe writes them, not this delivery)

Under Estado medido / bugs (BUG namespace, D75):

- `BUG-SWEEP-16 (High, work preservation) corrigido @7ef828d (branch flawless-writesafety, base ed4642e):
  saveMidiFile/savePatternPreset/saveRackTemplate/saveRoleMap passam por writeFileSafely (read-back + .bak);
  falha tardia de escrita lança DocumentError e o ficheiro do utilizador fica byte-idêntico. Vermelho medido:
  22 verificações falhadas em 5 testes novos (sucesso com ficheiro vazio, alvo sobrescrito); verde 150/1785/0,
  gate VERDE 17/17 @3d5b2a2 (218 s), versão 0.7.0+3d5b2a2a24 == HEAD. Seam de teste setWriteFaultForTests
  (inócuo fora dos testes); save/autosave do projecto por tocar. Relatório:
  docs/relatorios/2026-11-18-write-safety-16.md.`
- Pendência (owner = Diogo): integração do branch `flawless-writesafety` com rebase esperado sobre
  pattern26-28 (pattern_rack.cpp), roll30 (pattern_tools.cpp/groove.cpp) e preservation17/18 (midi.cpp
  cleanName/edit.cpp/assets.cpp) — regiões distintas, preservar as alterações deles; o cluster roll 19/20
  toca estes ficheiros depois. O escrivão regista a entrada e a pendência de integração.

## Pending and handoff

- Test Engineer acceptance: run the five new tests by name filter (`midi_tests.exe "late write failure"`,
  `bug_tests.exe "BUG-SWEEP-16"`); verify on a real export over an existing `.mid` that a failed write shows
  the error notice and leaves the old file readable, and that a successful overwrite leaves `<file>.mid.bak`.
  The fault seam is test-only; no runtime flag exists to arm it in the app.
- Integration: regions are distinct from the parallel streams (export write path midi.cpp:265,
  pattern_tools.cpp:396, pattern_rack.cpp:495, roles.cpp:121); a rebase at integration is expected — preserve
  their changes. The storage seam is additive (a function pointer + one no-op call), trivial to rebase.
- Residual risk: a failure DURING the final ReplaceFileW step follows the existing D4 semantics
  (previousFileIntact=false message says where each version is); that path is shared with project save and
  already covered by domain_tests. The preset/template id-dedup still never overwrites by design.
- Nothing pushed, no merge, no git stash; main checkout untouched; only the listed files changed.
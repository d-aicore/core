# Report — 2026-11-18 — BUG-SWEEP-02: export working-file (.part) ownership

## What was measured

- Repository: `d-aicore/core`, worktree `D:/CORE/CORE/.agents/worktrees/flawless-exportpart`, branch
  `flawless-exportpart`, base **`f6b86dc`** (integrated main). Fix commit **`2b00d5d`**
  (`2b00d5db93c7a1e795b32b9c143b60a156263a06`); docs commit follows it. The main checkout
  `D:/CORE/CORE` was never edited; nothing pushed, no `git stash`.
- Scope and authorization: the coordinator dispatch for BUG-SWEEP-02 ("export does not own/reserve its
  working filenames"), IMPLEMENTATION: red first, minimal fix, full gate green, one fix commit. Anchors
  re-confirmed at `f6b86dc` whole-block: `engine/src/core/render_job.cpp:276` (`partOf`), `:280`
  (discard removes EVERY listed `.part`), `:302` (fixed `<target>.part` opened with truncation via
  `engine/src/io/wav.cpp:31-37` `openForWriting` `"wb"`). Owner's standing criteria apply: «eu quero
  melhor motor que possamos fazer» and data > real time > performance > compatibility (2026-11-18, DSH
  coordinator session `session-2ff54f03-8344-40bb-8cb4-54df0601eb69`); English artifacts per «sempre tudo
  em ingles, meter como rule nos fecheiros necessarios» (same session).
- Environment: Windows 10.0.26300 (owner's PC), MSVC VS 18 BuildTools, Ninja, Release, 6 jobs, vendored
  `D:/Core-deps/JUCE-9.0.5` (shared dependency, already carrying the ratified item-1-D patch; not modified
  here). Build `D:/Core-build/flawless-exportpart`; evidence `D:/Core-build/flawless-exportpart-evidence/`.
  Gate ran under the machine-wide `D:/Core-build/GATE-LOCK` (created with stream+timestamp, released after).
- Tests executed: `audit2_tests` (red @ base, green after), full `hunt_tests` and `audit_tests` (green),
  full `ctest -L merge` twice (gate + verbose counts), `core-engine --version`.
- Limitations: the cross-process guarantee is by construction (pid in the name + exclusive create); the
  measured collision cases are in-process (two concurrent `RenderJobs` with a barrier hook, and 8 threads
  × 25 reservations). Two separate OS processes were not spawned by the test. Desktop self-test not run
  (merge gate per dispatch). The synchronous CLI `render` path is outside the anchored region — see
  "Remaining work".

## Results

| ID | Problem / result | Evidence (SHA, file:line or run) | Classification | State |
| --- | --- | --- | --- | --- |
| BUG-SWEEP-02 | A successful export truncated a preexisting `<target>.part` (retained partial export) and renamed it away; failure/cancel cleanup removed EVERY listed `.part`; two processes exporting one target shared the working path | red @ `f6b86dc`+tests: `fs::exists(target + ".part")` false, bytes `medido , esperado valuable previous partial export`, `partNamesIn(dir).size(): medido 0, esperado 1`, `working(seenA/B).size() >= 2` false — 3 tests / 30 checks / **17 failures**, rc 1 ([red-audit2.txt](evidencia/2026-11-18-exportpart/red-audit2.txt)) | test (red measured) | corrected |
| BUG-SWEEP-02 fix | Working files reserved uniquely and exclusively; preexisting `.part` byte-identical after success, failure (late level check and early missing-audio) and cancel; cleanup removes only this job's files; two concurrent exports hold two distinct working paths simultaneously | green @ `2b00d5d`: 4 tests / 243 checks / 0 failures; full `audit2_tests` 10/323/0, `hunt_tests` 13/647/0, `audit_tests` 7/65/0 ([green-audit2.txt](evidencia/2026-11-18-exportpart/green-audit2.txt)) | test (green measured) | validated |
| BUG-SWEEP-02 gate | Full merge gate green, no coverage shrink | gate **VERDE 20/20** @ `2b00d5d`, 315 s, rc 0; **206 cases / 3302 verifications / 0 failures** (baseline `f6b86dc` 202/3059; +4/+243 all in `audit2_tests`); `core-engine --version` = `0.7.0+2b00d5db93` == HEAD, no `.modified` ([gate-resumo.txt](evidencia/2026-11-18-exportpart/gate-resumo.txt), [ctest-merge-verbose.txt](evidencia/2026-11-18-exportpart/ctest-merge-verbose.txt)) | test (gate measured) | validated |
| BUG-SWEEP-02 sibling | The synchronous CLI `render` command still writes a fixed `<path>.part` and removes every listed `.part` on failure | `engine/src/core/server.cpp:720-731` (`writeWav(path + ".part", ...)`, catch-all removal) at `f6b86dc` and at `2b00d5d` | code (confirmed, not fixed here) | open — follow-up |

## Changes

Implementation (fix commit `2b00d5d`, 3 files, +281/−14):

- `engine/src/core/render_job.hpp` — header comment now describes the reserved working file;
  `publishRenderOutputs` gains an optional `workingPaths` parameter (empty = the legacy `<path>.part`
  contract, so the synchronous CLI and direct callers — including the archived probe's rollback section —
  are unchanged); new `reserveRenderWorkingFile(target)` declaration.
- `engine/src/core/render_job.cpp` —
  - `reserveRenderWorkingFile`: candidate `<target>.<pid>-<n>.part` with a process-wide atomic sequence;
    the exclusive create IS the reservation (Windows `CreateFileW(..., CREATE_NEW, ...)`, POSIX
    `open(O_WRONLY|O_CREAT|O_EXCL)`); an existing candidate is skipped, never opened; other OS errors
    throw with the code; 4096-attempt bound.
  - `RenderJobs::run`: per-output `working[f]` + `owned[f]`; `WavWriter` writes the reserved path;
    `discard()` removes only `owned` paths (was: every listed `.part`); publish passes `working`.
  - `publishRenderOutputs`: renames the actual working files (or the legacy names when none are given);
    the aside/`taken` reservation uses the same paths. **The undo lambda (base lines 92–100, batch-03's
    PublishRollback region) is byte-identical** — a rebase at integration is expected and must preserve
    their rollback reporting.
- `tests/audit2_tests.cpp` — 4 new English-named tests (the file's existing PT cases untouched, no
  translation sweep): success survival + folder census; failure (loud clip → late `checkExportLevel`,
  missing audio → early fail) and hook-timed cancel survival + own-file cleanup; two concurrent exports
  of one target meeting inside `beforePublish` (barrier) proving two distinct working paths held
  simultaneously; reservation API uniqueness, retained-candidate skip, 8×25 thread collision census.

Documentation (docs commit): this report, the index row, and the verbatim evidence archive.

No DSP added or changed (Core has no built-in DSP; the tests render silence or an audio-track clip).
No user file was truncated or consumed: every fixture is synthetic, under
`%TEMP%/core-audit2-exportpart/`, created by the tests themselves.

## Verification

Red (base `f6b86dc`, tests added uncommitted; `run-audit2.bat` = vcvars64 + cmake configure + build
`audit2_tests` + run):

```
D:\Core-build\flawless-exportpart\tests\audit2_tests.exe "BUG-SWEEP-02"
  FALHOU ...audit2_tests.cpp:269: fs::exists(target + ".part")
  FALHOU ...audit2_tests.cpp:270: readFileBytes(target + ".part") == ...: medido , esperado valuable previous partial export
  FALHOU ...audit2_tests.cpp:273: partNamesIn(dir).size() == (size_t)1: medido 0, esperado 1
  (same three at :296-298 loud-failure, :318-320 missing-audio, :333-335 cancel)
  FALHOU ...audit2_tests.cpp:367: working(seenA).size() >= (size_t)2
  FALHOU ...audit2_tests.cpp:368: working(seenB).size() >= (size_t)2
  FALHOU ...audit2_tests.cpp:371/372/375: preexisting .part consumed
FAIL BUG-SWEEP-02: a successful export never consumes a preexisting .part ...
FAIL BUG-SWEEP-02: a failed or cancelled export keeps the preexisting .part ...
FAIL BUG-SWEEP-02: two concurrent exports of the same target never share a working path
3 testes, 30 verificacoes, 17 falhas      (rc 1)
```

Green (fix applied, `run-green.bat`, rc 0 for all four runs):

```
=== FILTERED BUG-SWEEP-02 ===   4 testes, 243 verificacoes, 0 falhas   (all four ok)
=== FULL audit2_tests ===      10 testes, 323 verificacoes, 0 falhas
=== FULL hunt_tests ===        13 testes, 647 verificacoes, 0 falhas
=== FULL audit_tests ===        7 testes,  65 verificacoes, 0 falhas
RCS=0,0,0,0
```

Gate (under GATE-LOCK, from the worktree):

```
powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/flawless-exportpart -Jobs 6
Gate do Core: VERDE / Codigo: ... @ 2b00d5d / Testes: merge / Tempo: 315 s
100% tests passed out of 20        (rc 0)
core-engine --version → core-engine 0.7.0+2b00d5db93   (== HEAD short=10, no .modified)
```

The resumo's "(com alteracoes por commitar)" flag is the untracked evidence folder only; the version
stamp uses `--untracked-files=no` and the tracked tree was exactly `2b00d5d`. The gate ran at the fix
commit; the docs commit changes no code.

Per-suite counts at `2b00d5d` (`ctest -L merge -V`, 20/20, 0 failures; baseline `f6b86dc` 202/3059):

| Suite | Cases | Verifications | vs f6b86dc |
| --- | --- | --- | --- |
| engine_tests | 25 | 130 | unchanged |
| domain_tests | 30 | 525 | unchanged |
| legacy_guard_tests | 9 | 169 | unchanged |
| role_tests | 21 | 216 | unchanged |
| midi_tests | 14 | 208 | unchanged |
| bug_tests | 39 | 356 | unchanged |
| bitwig_tests | 2 | 25 | unchanged |
| audit_tests | 7 | 65 | unchanged |
| **audit2_tests** | **10** | **323** | **+4 / +243** (was 6/80) |
| hunt_tests | 13 | 647 | unchanged |
| round3_tests | 7 | 82 | unchanged |
| calls_tests | 12 | 69 | unchanged |
| menu_tests | 7 | 366 | unchanged |
| engine_process_tests | 10 | 121 | unchanged |
| cli_version / cli_demo_render / cli_measure / cli_unicode / cli_package_stale / cli_plugin_crash_render | pass | — | unchanged |
| **Total** | **206** | **3302** | **+4 / +243, nothing shrunk** |

## Concurrency reservation design

- Name: `<target>.<pid>-<n>.part`, `n` from a process-wide `std::atomic<uint64_t>` sequence. Different
  processes differ by pid; within one process the atomic sequence differs. Neither is trusted alone:
  the **exclusive create is the reservation** — `CREATE_NEW` / `O_CREAT|O_EXCL` fails atomically if the
  candidate exists (another operation's live reservation, a retained recovery file, a pid reused after
  a crash), and the loop skips to the next name. No candidate that exists is ever opened, so nothing
  preexisting can be truncated, and two reservations can never hold one path.
- Ownership: `run()` records `owned[f]` at reservation; `discard()` (cancel and failure paths) removes
  only those paths. A retained `<target>.part` is invisible to the job.
- Write: `WavWriter` reopens the reserved path with `"wb"` — it truncates only the empty file this
  operation created microseconds earlier; no other Core operation opens another's reserved name
  (exclusive create skips it; cleanup is own-only).
- Publish: unchanged safe-replace semantics (aside to `.previous[-k]`, rename working → target, undo on
  failure, asides removed on success) — now over the actual reserved paths. Two exports of one target
  still race at publish by design (last one in place wins; a Windows rename over an existing target
  fails safe and the undo restores it) — that is target-level semantics, not working-file sharing.
- `validateRenderOutputs` keeps its exact-`.part` guard. Residual edge (documented, not measured): a
  target literally named like another output's *future* reservation name is unpredictable (pid +
  sequence); if it ever coincided, Windows publish fails safe (rename refuses replacement, undo
  restores) — no preexisting user data can be consumed, since reservation skips existing files.

## Remaining work, risks and handoff (Test Engineer)

- **Sibling path not fixed here (out of the anchored region):** the synchronous CLI `render`
  (`engine/src/core/server.cpp:720-731`) still writes fixed `<path>.part` via `writeWav` and its catch
  removes every listed `.part`. Same defect class; recommend a follow-up dispatch routing it through
  `reserveRenderWorkingFile` + `publishRenderOutputs(..., workingPaths)` (the API is ready).
- **Batch-03 rebase:** `flawless-batch2` adds PublishRollback at base lines 92–100 of `render_job.cpp`;
  this fix does not touch that lambda. Integration must keep both (their rollback reporting + these
  working-file paths) and re-gate the integrated tip.
- Acceptance suggestions: run `audit2_tests.exe "BUG-SWEEP-02"` on Windows (4 tests / 243 checks);
  real-app check — export over a folder holding a retained `mix.wav.part`, confirm it survives
  byte-identical and the export publishes; two Core processes exporting one target do not corrupt each
  other's working file (by construction; not spawned in the suite).
- Runtime conditions: merge gate only; desktop self-test, real-plugin export load and a true
  two-process export were not run here.
- Flakiness: none observed; the concurrency test is barrier-synchronized (no timing window), the cancel
  case is hook-timed (deterministic).

## PROPOSED ESTADO.md lines (D72/D80 — the scribe writes them, not this delivery)

- Estado medido: `BUG-SWEEP-02 (export working-file ownership) corrigido @2b00d5d (branch
  flawless-exportpart, base f6b86dc): o export reserva "<target>.<pid>-<n>.part" por criação exclusiva
  (nunca trunca um candidato existente, entre processos também), a limpeza remove só os ficheiros que a
  operação criou, e a publicação mantém o safe-replace (região undo do batch-03 intacta). Vermelho
  medido @f6b86dc: 3 testes / 30 verificações / 17 falhas (o .part preexistente "valuable previous
  partial export" consumido no sucesso, na falha e no cancelamento; dois exports concorrentes partilham
  um caminho). Verde: 4 testes / 243 verificações / 0 falhas; audit2 10/323/0, hunt 13/647/0, audit
  7/65/0. Gate merge VERDE 20/20 @2b00d5d (315 s), 206 casos / 3302 verificações / 0 falhas (baseline
  f6b86dc 202/3059; +4/+243, nada reduzido), versão 0.7.0+2b00d5db93 == HEAD sem .modified. Fonte:
  docs/relatorios/2026-11-18-export-part-ownership-02.md + evidencia/2026-11-18-exportpart/.`
- Pendência (owner = Diogo): `BUG-SWEEP-02 irmão: o render síncrono da CLI (server.cpp:720-731) ainda
  usa <path>.part fixo e remove todos os .part listados na falha — fora da região ancorada desta
  entrega; despacho de seguimento recomendado (a API reserveRenderWorkingFile/workingPaths já existe).
  Integração: rebase com o PublishRollback do batch-03 (flawless-batch2) preservando ambos.`
- Bugs use the `BUG-` namespace (D75); no D number is claimed. No owner ratification is implied by this
  delivery; the dispatch is the implementation authorization.

## Evidence

Archived verbatim in [evidencia/2026-11-18-exportpart/](evidencia/2026-11-18-exportpart/) (originals in
`D:/Core-build/flawless-exportpart-evidence/`):

| File | sha256 |
| --- | --- |
| core-audit-probes.cpp (owner's probe, verbatim from D:/Downloads) | `702989b58159cbec66395e4194b97e0940f9d97bb74b3f9738d466e938d5c78b` |
| red-audit2.txt | `f20650a389eb73a8119074d7accaf54bc2118616c1d04279d09ddbb9006a908c` |
| green-audit2.txt | `b21f2439b52edcb936ec4f158de91f981b9c7482b7154bd82d4d99d228a23bc6` |
| gate-resumo.txt | `5ba0e2466a3d6526eb7ce5b04fabfc92e5428707b3fbbe0c29ad4dfb28fc4e16` |
| gate.log.txt | `8f17800ad109ab44ef819e599b38f83bfe20c669b8bf95acef5a5323dfe5da0b` |
| ctest-merge-verbose.txt | `0c74f78eae7e477a1ac0a879e0ba873a1fa0c2e0e7878a4f253d72ee59d58955` |
| run-audit2.bat.txt / run-green.bat.txt | `b73fa494573f265950ca4f405d3a97e2e1a8769034127f50c8015b10228da535` / `c37e8578b9481cd6592f1da38cb0af58778fe6b3cd72cc69dc10da24fe6e746a` |

The probe's part-ownership section (lines 14–21) is the success case reproduced here on Windows via the
`RenderJobs` harness; its addon and rollback sections belong to other findings (batch-01/03) and were
not re-run here.
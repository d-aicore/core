# Report — 2026-11-18 — BUG-SWEEP-14: the dev zip kept lang/patterns/pcore files removed from the repo

## What was measured

- **Repository, branch and SHA:** worktree `D:\CORE\CORE\.agents\worktrees\flawless-packaging`, branch
  `flawless-packaging`, base `ed4642e61b` (main's HEAD at dispatch). Fix commit `6267728eb6`; docs commit
  follows this report. The main checkout `D:\CORE\CORE` was **not edited** (its pre-existing uncommitted
  `ESTADO.md` / `docs/relatorios/README.md` changes belong to other streams and were left as found).
- **Scope and authorization:** BUG-SWEEP-14 dispatch (PM flawless team): red test first, minimal fix, full
  gate green, one fix commit + one docs commit. Closed scope: packaging CMake + one new test. This is build
  staging, not user data — AGENTS.md, verbatim: «Limpeza de samples distribuídos não é limpeza dos ficheiros
  do utilizador.» No user file, project or audio was touched.
- **Environment:** Windows 10.0.26300, owner's PC (Ryzen 7 7800X3D, 16 logical), MSVC VS 18 BuildTools,
  CMake 4.4.4 + Ninja, Release, JUCE 9.0.5 from `D:/Core-deps/JUCE-9.0.5`. Build
  `D:\Core-build\flawless-packaging` (outside the tree); evidence `D:\Core-build\flawless-packaging-evidence\`.
- **Tests executed:** new `cli_package_stale` measured **red at `ed4642e`** and **green after the fix**;
  full `merge` suite in the dev build (18/18); fresh-folder gate `ci/gate.ps1 -Jobs 6` under the
  machine-wide `GATE-LOCK` (VERDE 18/18, 279 s); per-suite case/check counts measured on the gate build;
  `core-engine --version` checked against the commit SHA.
- **Limitations:** the test proves staging-folder and zip *contents*; it does not install or run the zip on
  a clean machine (the installer's own staging was already clean and is unchanged). No UI or audio-runtime
  claim is made. The red build was `ed4642e` plus the new test only (the test is the sole difference from
  the base when red was measured).

## Results

| ID | Problem / result | Evidence (SHA, file:line or run) | Classification | State |
| --- | --- | --- | --- | --- |
| BUG-SWEEP-14 | **Confirmed:** `core-package` reused `package/Core` and removed only `templates` before copying; `cmake -E copy_directory` does not delete destination-only files, so a `lang`/`patterns`/`pcore` file removed from the repo lingered in the staging folder and in `Core-windows-x64.zip` | `app/desktop/CMakeLists.txt:77-90` @ `ed4642e` (remove only at `:82`; copies at `:84-86`); red run below | confirmed in code + proven by test | fixed |
| BUG-SWEEP-14 | The app-resources staging (`:60-67`) and the installer (`app/installer/CMakeLists.txt:10`, `rm -rf stage`) already clean explicitly; the dev zip did not | same files @ `ed4642e` | confirmed in code | — |
| BUG-SWEEP-14 | **Red:** 3 destination-only seeds survived `core-package` in staging **and** in the zip; the `templates` seed was removed by the pre-existing cleanup (control) | `red-cli_package_stale.txt` (ctest rc=8); `red-zip-entries.txt` lines 7/56/58 | proven by test | — |
| BUG-SWEEP-14 | **Minimal fix:** `remove_directory` for `lang`, `patterns`, `pcore` staging dirs before each copy, mirroring the existing patterns; `copy_if_different` for the exe/demo/README kept; packaged content unchanged | `app/desktop/CMakeLists.txt` @ `6267728eb6` (+3 commands, +3 comment lines) | proven by test | fixed |
| BUG-SWEEP-14 | **Green:** same test passes with the red run's seeds still present (the exact lingering-file scenario); staging and zip carry every current repo resource and the exe/demo/README, no stale entries | `green-cli_package_stale.txt` (rc=0); `green-zip-entries.txt` (0 stale); `gate-zip-entries.txt` (0 stale) | proven by test | validated |
| Gate | `merge` **VERDE 18/18** (baseline 17 + `cli_package_stale`); **145 cases / 1741 checks / 0 failures** across the 12 binary suites; version `0.7.0+6267728eb6`, **no `.modified`**; 279 s, fresh folder, Jobs 6, under `GATE-LOCK` | `gate-resumo.txt`, `gate-utf8.log`, `gate-suite-counts.txt`, `gate-version.txt` | proven by test | green |
| Coverage | No shrink: all 12 suites identical to `ed4642e`. The baseline file's `engine_process_tests: 8/93` includes the **unmerged** flawless-crash branch's +4 cases/+44 checks (`d434720..8fcce35`, `tests/engine_process_tests.cpp` +211 lines); at the common base `ed4642e` it is 4/49, exactly what this build reports | `baseline-suite-counts.txt` vs `gate-suite-counts.txt`; `git diff --stat ed4642e 8fcce35 -- tests/engine_process_tests.cpp` | confirmed in code + measured | — |

## Changes

Implementation (fix commit `6267728eb6`, named files only):

- `app/desktop/CMakeLists.txt` — three `remove_directory` commands added to `core-package`
  (`package/Core/lang`, `package/Core/patterns`, `package/Core/pcore`) before their `copy_directory`,
  plus a comment naming BUG-SWEEP-14 and the reason. `copy_if_different` for `Core.exe`,
  `core-engine.exe`, `demo.core-document.json`, `README.md` untouched; nothing added to or removed from
  what gets packaged.
- `tests/cli_package_stale.cmake` — new regression script: seeds one destination-only stale file per
  resource folder (`templates` is the control), runs the real `core-package` target, requires the seeds
  gone from staging **and** from `Core-windows-x64.zip` (entry listing), and requires every current repo
  resource plus the exe/demo/README present in both.
- `tests/CMakeLists.txt` — registers `cli_package_stale` with label `merge`, `TIMEOUT 300`,
  `RUN_SERIAL TRUE` (it repacks the build's shared package folder and zip), only when `TARGET core-package`
  exists (the Windows gate build), following the existing cli-test pattern.

Documentation (docs commit): this report + one appended index row in `docs/relatorios/README.md`.
**`ESTADO.md` was not edited** (D72/D80); the lines this delivery asks for are proposed below.

## Verification

All commands run on the owner's PC; runner scripts and full logs in
`D:\Core-build\flawless-packaging-evidence\` (`configure.bat`, `build.bat`, `ctest-package.bat`,
`ctest-merge.bat`, `version.bat`; MSVC env via `vcvars64.bat`, `VSLANG=1033`, `chcp 65001`).

1. **Red** @ `ed4642e` + new test (dev build configured and built fresh, parallel 4, rc=0):
   `ctest --test-dir D:\Core-build\flawless-packaging -R cli_package_stale --output-on-failure --timeout 600`
   → **rc=8**, `1/1 Test #16: cli_package_stale ................***Failed`:
   ```
   BUG-SWEEP-14: stale files survived core-package in staging:
   package/Core/lang/stale-removed-lang.json;package/Core/patterns/stale-removed-pattern.json;package/Core/pcore/stale-removed-style.json
   ```
   The `templates` seed did not survive (its cleanup already existed). Zip listing of the same run
   (`cmake -E tar tf Core-windows-x64.zip`) shows the same three stale entries
   (`red-zip-entries.txt:7,56,58`) — the bug reached the shipped artifact, not only the folder.
2. **Fix** applied and committed: `6267728eb6` (3 files, +107 lines).
3. **Green** (same command, red run's seeds still in staging): **rc=0**,
   `1/1 Test #16: cli_package_stale ................   Passed    6.52 sec`. Post-fix zip listing: 0 stale
   entries, all current resources present.
4. **Dev merge suite:** `ctest -L merge --output-on-failure --timeout 600 --no-tests=error` → rc=0,
   **100% tests passed out of 18** (`dev-merge-ctest.txt`).
5. **Version:** worktree clean; `core-engine --version` → `core-engine 0.7.0+6267728eb6`
   (`version-check.txt`) — equals the fix commit, no `.modified`.
6. **Gate lock protocol:** `D:\Core-build\GATE-LOCK` absent at 05:31; the two most recent gates had
   finished (`gate-d915710` VERDE 17/17, 388 s, SUR clone; `flawless-batch` finished with its own
   failures — other streams, not touched); no `cmake`/`ninja`/`ctest` process running. Created
   `GATE-LOCK` (stream + timestamp) at **05:32:26**; deleted my dev build folder so the gate configured
   **fresh**; ran `powershell -ExecutionPolicy Bypass -File ci\gate.ps1 -Build D:\Core-build\flawless-packaging -Jobs 6`
   → **rc=0**; **deleted the lock immediately after** (pass). No gate ran without the lock; no lock of
   another actor was touched.
7. **Gate result** (`gate-resumo.txt`, verbatim tool output):
   ```
   Gate do Core: VERDE
   Codigo: D:\CORE\CORE\.agents\worktrees\flawless-packaging @ 6267728
   Build:  D:/Core-build/flawless-packaging
   Testes: merge
   Tempo:  279 s

   100% tests passed out of 18
   ```
   `cli_package_stale` Passed 1.74 s in the gate (`gate-utf8.log:496`); Ninja header-dependency check
   passed (no refusal); clean tree (no "com alteracoes por commitar").
8. **Per-suite counts, measured on the gate build** (`gate-suite-counts.txt`) vs
   `D:\Core-build\flawless-crash-evidence\baseline-suite-counts.txt`:

   | Suite | Baseline file | This gate | Note |
   | --- | --- | --- | --- |
   | engine_tests | 25 / 130 / 0 | 25 / 130 / 0 | same |
   | domain_tests | 19 / 148 / 0 | 19 / 148 / 0 | same |
   | role_tests | 21 / 216 / 0 | 21 / 216 / 0 | same |
   | midi_tests | 8 / 115 / 0 | 8 / 115 / 0 | same |
   | bug_tests | 22 / 138 / 0 | 22 / 138 / 0 | same |
   | audit_tests | 7 / 65 / 0 | 7 / 65 / 0 | same |
   | audit2_tests | 6 / 80 / 0 | 6 / 80 / 0 | same |
   | hunt_tests | 14 / 649 / 0 | 14 / 649 / 0 | same |
   | round3_tests | 7 / 82 / 0 | 7 / 82 / 0 | same |
   | calls_tests | 10 / 44 / 0 | 10 / 44 / 0 | same |
   | bitwig_tests | 2 / 25 / 0 | 2 / 25 / 0 | same |
   | engine_process_tests | 8 / 93 / 0 | 4 / 49 / 0 | baseline file includes the unmerged crash branch's +4/+44; at `ed4642e` it is 4/49 — identical, no shrink |
   | **Totals (cases / checks / failures)** | 149 / 1785 / 0 (with crash branch) | **145 / 1741 / 0** (@ `ed4642e` + fix) | ctest merge: 17 → **18** (added `cli_package_stale`) |

   (cases / checks / failures; the binaries' summary lines are the existing Portuguese tool output,
   quoted as measured.)
9. **Gate-build zip:** `cmake -E tar tf` → 0 stale entries (`gate-zip-entries.txt`).
10. **Containment:** main checkout `D:\CORE\CORE` status identical to session start (pre-existing
    `M ESTADO.md`, `M docs/relatorios/README.md` and untracked files of other streams; HEAD still
    `ed4642e61b`); branch `flawless-packaging` has **no upstream — nothing pushed, nothing merged**;
    only `app/desktop/CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/cli_package_stale.cmake` changed in
    the fix commit; no user file touched.

## Pending and handoff

**Proposed `ESTADO.md` lines (for the ledger scribe only — D72/D80; not written by this delivery):**

- *Estado medido:* BUG-SWEEP-14 corrigido @ `6267728eb6` (branch `flawless-packaging`, base `ed4642e`):
  `core-package` remove `lang`/`patterns`/`pcore` de `package/Core` antes de copiar (espelha o staging da
  app e o instalador); `copy_if_different` do exe/demo mantido; conteúdo do zip inalterado. Regressão
  `cli_package_stale` (label `merge`): vermelho medido @ `ed4642e` (3 seeds sobreviveram no staging e no
  zip), verde depois. Gate `merge` VERDE **18/18**, **145 casos / 1741 verificações / 0 falhas**, versão
  `0.7.0+6267728eb6` sem `.modified`, 279 s, pasta fresca sob `GATE-LOCK`. Fonte:
  `docs/relatorios/2026-11-18-devzip-stale-files.md`; evidência `D:\Core-build\flawless-packaging-evidence\`.
- *Pendência (owner = Diogo):* merge de `flawless-packaging` (`6267728eb6` + commit de docs) em `main` —
  passo remoto é do dono. Nota de integração: este stream não toca `tests/engine_process_tests.cpp`; o
  denominator 8/93 do ficheiro de baseline pertence ao branch `flawless-crash` por mergear (+4 casos /
  +44 verificações), não a `main`.

**Acceptance suggestion (Test Engineer):** on any built tree, `ctest -R cli_package_stale --output-on-failure`;
manual cross-check: drop a fake file into `build/package/Core/lang/`, run
`cmake --build build --target core-package`, confirm it is gone from the folder **and** from
`Core-windows-x64.zip` (`cmake -E tar tf`), and that `Core.exe`, `core-engine.exe`,
`demo.core-document.json`, `README.md` and every current `lang/patterns/pcore/templates` file are present.
Re-run the test twice in a row (idempotence) and once with `ctest --parallel` (`RUN_SERIAL` keeps the
repack exclusive).

**Residual risk:** none known. The fix only deletes destination-only files inside the build's own staging
folders; the test asserts the full current content survives, so over-deletion would fail it. The installer
and app-resources paths were already clean and are unchanged.

**Next steps:** owner's merge; the scribe writes the proposed ledger lines; the crash stream's baseline
file should be re-labelled with the SHA it was measured on (its `engine_process_tests` row is post-fix,
not `ed4642e`) — suggested to that stream's coordinator, not edited here.
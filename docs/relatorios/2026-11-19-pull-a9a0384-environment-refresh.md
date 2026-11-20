# Relatório — 2026-11-19 — Pull to a9a0384 and review-environment refresh (stage 0)

Delivery: DSH lead session `a69b320fbfab` (root agent `lead`), 2026-11-19 ~23:05–2026-11-20 Europe/Lisbon.
Scope: **environment and development tooling only** — no product, DSP or UI code was changed.
Continues the 19/11 handover ([2026-11-19-review-debug-sanitize-environment.md](2026-11-19-review-debug-sanitize-environment.md)),
whose measurements stay pinned to `8c6837e`; this report is the dated continuation at the pulled HEAD.

## Resume point (start here: next agent, next model)

| What | Where / how |
| --- | --- |
| Baseline | `main @ a9a038430d781959829f8510067ce59afffaad62` (= `origin/main`, fast-forward from `8c6837e8`, 86 commits). **The SHA alone does not identify the build tree**: `CMakeLists.txt` carries the uncommitted ENV-01 sanitizer wiring (extended here for `modules_tests`/`window_tests`); `docs/relatorios/README.md` carries four uncommitted index rows (two from another session, the 19/11 handover row, and this delivery's row — all preserved). Untracked and preserved: the 19/11 handover report + evidence, this report + evidence, two 2026-11-17 reports + evidence (another session), `.qoderignore` (IDE artifact), `environment.yml` (conda/ipykernel notebook env — neither is a component of the DAW environment). `stash@{0}` (unrelated) untouched. |
| One-command Windows | `powershell -NoProfile -ExecutionPolicy Bypass -File D:\Core-build\core-review\run-all.ps1` — every `.done` marker is bound to `fp1 sha=… local=… juce=… script=… cfg=…`; a step is reused only when the whole fingerprint matches, otherwise it re-runs. |
| One-command WSL | `wsl.exe -e bash /mnt/d/Core-build/core-review/run-wsl.sh` — same binding plus a **hard checkout gate**: no result is accepted unless `~/core-review-src` is detached at exactly the Windows HEAD (verified every run, even on reuse). |
| Health check (read-only) | `powershell -NoProfile -ExecutionPolicy Bypass -File D:\Core-build\core-review\verify.ps1` — presence + ASan canary + **marker freshness** (context fingerprint SHA+local diff+JUCE+script vs the current tree; per-step `cfg` is checked by `run-all.ps1` itself, the WSL checkout-revision gate lives in `run-wsl.sh`, and `release\` is pinned to 8c6837e and not checked). It is NOT a suite run; a full re-run is `run-all.ps1` / `run-wsl.sh`. Current state: `VERIFY: environment ready`, exit 0. |
| Builds (out of tree) | `D:\Core-build\core-review\{debug,asan,release}`; WSL `~/core-review-build/{debug,asanubsan,tsan,tidy}` on ext4; clone `~/core-review-src`. Compile database: `debug\compile_commands.json` (507 TUs, MSVC flags). |
| Logs | `D:\Core-build\core-review\NN-*.txt` + fp1 `*.done` (canonical SHA-bound run); WSL copies under `wsl\`. Historical: `archive-8c6837e\`, `wsl\archive-8c6837e\`, WSL `~/core-review-build/archive-8c6837e/`, `archive-a9a0384-run1-prehardening\`. |
| Proofs | `proof\proof-windows-summary.txt` (7/7), `wsl-proof\proof-wsl-log.txt` (6/6; run1 kept as `proof-wsl-log-run1.txt`), `09c-lsp-harness-proof-a9a0384.txt`, `proof\main-root-reuse.txt`. |
| Tooling delivery (prepared, NOT committed) | `D:\Core-build\core-review\tooling-delivery\` — `manifest.md`, `cmake-tooling.patch`, `readme-tooling-lines.txt`, `scripts-bundle\` (all scripts + probes). No verbatim commit authorization exists (handover left it as an owner decision; P-D98 open; D48/D50). |
| Scripts outside Git | `D:\Core-build\core-review\` (bundle copy in the delivery AND in this report's evidence directory `scripts-bundle/`). Prerequisites and reconstruction recipe: `tooling-delivery\manifest.md` §Local prerequisites. Recommendation (owner decision): commit them under `ci/review-env/` so a fresh clone suffices. |
| JUCE patch bootstrap | `D:\Core-deps\JUCE-9.0.5` (extracted, no .git) must carry the D113 patch. On a fresh extraction: download the pinned 9.0.5 tarball (URL + SHA256 at `CMakeLists.txt:64-66`), extract, then `git apply <repo>\ci\patches\juce-failed-plugin-restart.patch` inside it; verify with `git apply --check -R` (succeeds iff fully applied) or `markCrashedByHost` present in both touched files. Nothing in the repo applies it automatically yet — round-2 item S11 owns that fix (clean-build proof in preparation). |
| Next action | (1) owner decision on the tooling commit (manifest ready); (2) stage 3A — ENV-03 Debug 259 persists at this SHA (task board `task-2`); (3) the program order is on the shared task board: 0 → 3A → {1, 4, 2} → 6 → 5 → 7, superseded for round 2 by the owner's S-round dispatch (S11 → S1 → S5 → S4 → S2/S3 → S6/S7, then S8–S13 and the VST3/Chain fronts); (4) the owner's CSV of 10 open findings («depois confirma») is archived at `evidencia/2026-11-19-pull-a9a0384-environment-refresh/owner-csv-10-findings.csv` and **confirmed read-only** at this SHA: [2026-11-20-csv-10-findings-confirmation.md](2026-11-20-csv-10-findings-confirmation.md) — 9 confirmed, 1 partial; verbatim notes `csv-triage-notes-full.md` in the same evidence directory. |

## O que foi medido

- Repository `d-aicore/core`, branch `main`. Initial SHA `8c6837e80030e2185d5f6016f0365bae2c3f52bd`
  (the 19/11 handover baseline) → final `a9a038430d781959829f8510067ce59afffaad62` = `origin/main`.
  Fast-forward: 86 commits, 224 files, +14,453 / −278,059 lines (mostly the built-in-sound and
  21-template removal), 0 local commits ahead. Measured by `git rev-parse`, `git rev-list --count`,
  `git diff --stat`.
- Âmbito e autorização: owner 2026-11-19 «Iniciei a preparaçao do ambiente para tratares da minha
  app daw, esta em c++ e juce, faz o pull do ultimo commit»; stage 0 prompt («Prepara a entrega do
  ambiente… Faz o commit apenas se já estiver autorizado»); standing environment authorization
  quoted in the 19/11 handover («prepara so o ambiente», «nao tentes melhorar a app so queremos
  ter todas as ferramentas para testtar sanitizar…»). Tree edits: build configuration only
  (`CORE_SANITIZED_TARGETS`) + this report + its index row.
- Ambiente: Windows 11 10.0.26300 x64, MSVC 19.51.36260 (VS 18 Build Tools 18.10.2), CMake 4.4.4,
  Ninja 1.13.2; WSL2 Ubuntu 24.04.5 (kernel 6.18.58.1), GCC 13.3.0, clang/clang-tidy 18.1.3,
  CMake 3.28.3, Ninja 1.11.1, python3 3.12.3; **no gdb, no route to the apt mirror** (ENV-04).
  JUCE 9.0.5 extracted at `D:\Core-deps\JUCE-9.0.5` (**no .git**) with the vendored D113 crash
  patch APPLIED (14 hunks; `markCrashedByHost` present in both touched files, verified).
- Testes executados: full environment re-run on both platforms at `a9a0384` (tables below),
  reuse/invalidation proofs in fresh folders, Harness `lsp` navigation proof.
- Limitações: the 19/11 handover's coverage limits stand (no real device, no sanitized JUCE
  app/VST3, no RT-safety proof, ENV-04/05/07). **Release `engine_process_tests` at `a9a0384` was
  NOT re-measured here** — the `release\` probe is pinned to `8c6837e`; stage 3A owns it.

## Resultados

| ID | Problema / resultado | Evidência (SHA, ficheiro:linha ou execução) | Classificação | Estado |
| --- | --- | --- | --- | --- |
| PULL-01 | Fast-forward 86 commits; uncommitted handover preserved exactly (backup patch → stash → ff → pop auto-merge; diff vs `origin/main` immediately after the pop = the handover changes only, CMake +6 / README +6−1, before this delivery's own edits; no conflict markers; untracked files intact; unrelated stash untouched) | `git rev-parse HEAD` = a9a0384; `pre-pull-2026-11-19-local-changes.patch`; final diff vs origin/main: CMake +13, README +6 purely additive (four rows + separators; trailing newline normalized) | comprovado por execução | validado |
| PULL-02 | Upstream removed `legacy_guard_tests` (built-in-sound removal) and `app/sampling`; the ENV-01 name stays inert via the `if(TARGET)` guard | `tests/legacy_guard_tests.cpp` deleted at a9a0384; `CMakeLists.txt:104-109` | confirmado no código | registado |
| PULL-03 | ENV-01 gap extended upstream: new unconditional merge tests `modules_tests` (links `core_control`+`core_domain`) and `window_tests` (links `core_domain`) were absent from `CORE_SANITIZED_TARGETS` → same LNK2038/`__asan_*` link failure. Fix extended (build config only); MSVC and GCC sanitized builds now link clean | `tests/CMakeLists.txt:26-27,96-97` @ a9a0384; `CMakeLists.txt` local diff; 06-build-asan rc=0 (77/77), WSL builds rc=0 | confirmado no código + comprovado por teste | corrigido (por commitar) |
| PULL-04 | `.done` markers were existence-only → a pull skipped everything (false-green class). Both scripts now bind markers to SHA + build-affecting local diff (docs excluded) + vendored-JUCE state + script hash + per-step config; legacy markers always re-run | `run-all.ps1` / `run-wsl.sh` (fp1 layer); Windows proof 7/7, WSL proof 6/6, main-root reuse | comprovado por execução | validado |
| PULL-05 | WSL results were accepted without verifying the checkout revision. Hard gate added: clone must equal the Windows HEAD on EVERY run; mirror `CMakeLists.txt` verified by `cmp`; fail-closed demonstrated (run1 W3: unreadable intended SHA → FATAL, no results accepted) | `run-wsl.sh` hard gate; `wsl-proof/proof-wsl-log-run1.txt`, `proof-wsl-log.txt` | comprovado por execução | validado |
| PULL-06 | JUCE dir has no `.git` → a git-diff fingerprint would silently miss the vendored patch state (D113 false-green risk). Fixed: fingerprint hashes the patch file AND every file it touches | `run-all.ps1` / `verify.ps1` no-git branch; context `juce=no-git/E421E87…` | confirmado no código + comprovado por execução | corrigido (tooling) |
| PULL-07 | ENV-08 (WSL audio device) is **resolved upstream** by BUG-SWEEP-06 (`bcf9224`, normal-priority retry): `engine_tests` exits 0 in WSL plain, ASan+UBSan and TSan | `asanubsan-engine-tests-known.txt`, `tsan-engine-tests-known.txt` (exit=0) | comprovado por teste | fechado por referência |
| PULL-08 | ENV-03 persists at a9a0384: Debug `engine_process_tests` 3/3 exit 1, «The engine exited (code 259)» at `tests/engine_process_tests.cpp:159` (15 cases / 256 verifications / 1 failure per run); the same test PASSES in the ASan Debug suite; Release at this SHA not re-measured | `03b-engine-process-summary.txt`, `03b-engine-process-run1.txt`, `08-asan-ctest.txt` | comprovado por teste | aberto — stage 3A (task-2) |
| PULL-09 | ENV-07 automated: the `setarch $(uname -m) -R` wrapper is built into `run-wsl.sh` for the TSan canary AND suite; the original script's deferred `\$()` substitution (which never actually wrapped the suite) is fixed | `run-wsl.sh` §30; tsan canary 5/5 + suite 17/17, 0 warnings | comprovado por teste | validado |
| PULL-10 | Evidence preserved across the refresh: 8c6837e logs archived (37 root + 25 wsl-copy + 46 WSL files), pre-hardening a9a0384 run1 archived (34 files) before the canonical SHA-bound run | `archive-8c6837e/`, `wsl/archive-8c6837e/`, `~/core-review-build/archive-8c6837e/`, `archive-a9a0384-run1-prehardening/` | comprovado por execução | validado |
| PULL-11 | Tooling delivery prepared SEPARATED from other sessions' changes; NOT committed (no authorization found) | `tooling-delivery/manifest.md` + `cmake-tooling.patch` + `readme-tooling-lines.txt` + `scripts-bundle/` | documental | decisão pendente (owner) |
| PULL-12 | Harness `lsp` navigation re-proven at a9a0384 in THIS session: hpp→cpp definition, cross-TU, 10 references in 6 files; standalone clangd smoke rc=0 | `09c-lsp-harness-proof-a9a0384.txt`, `09-lsp-smoke.txt` | comprovado por execução | validado |

## Alterações

**In the repository (all uncommitted, awaiting the owner's commit decision):**
- `CMakeLists.txt` — extension of the ENV-01 fix: `modules_tests window_tests` added to
  `CORE_SANITIZED_TARGETS` with a dated comment; the original handover comment and line kept
  verbatim (`legacy_guard_tests` inert, documented). Build configuration only, no logic change.
- `docs/relatorios/2026-11-19-pull-a9a0384-environment-refresh.md` (this report) +
  `docs/relatorios/evidencia/2026-11-19-pull-a9a0384-environment-refresh/` (proof artifacts).
- `docs/relatorios/README.md` — one new index row (this delivery). The two 2026-11-17 rows from
  another session and the 19/11 handover row are preserved untouched.
- `ESTADO.md` **not edited** (D72/D80) — proposed lines below.

**Outside the repository (tooling, `D:\Core-build\core-review\`):**
- `run-all.ps1`, `run-wsl.sh` — SHA-bound validation (fp1 markers), WSL hard checkout gate,
  built-in TSan `setarch` wrapper, `-AsanFlags`/`-AsanTests` parameters, `ONLY`/`OUT` overrides,
  strict ASan build (the pre-fix `legacy_guard_tests` tolerance removed).
- `verify.ps1` — new read-only **marker freshness** check (same fingerprint as `run-all.ps1`).
- `proof-windows.ps1`, `proof-wsl.sh`, `prep-wsl-repull.sh` — new proof/prep scripts.
- Archives, `tooling-delivery/`, `09c-lsp-harness-proof-a9a0384.txt`, proof roots
  (`proof\`, WSL `~/core-review-proof{,-src}`, `~/core-review-old-src`).
- The Windows scratch worktree `proof-src-old` (8c6837e) was removed after the proofs
  (`git worktree remove`); the WSL proof clones are kept as evidence (disposable, shared objects).

## Verificação

All commands below ran at `a9a0384` + the local CMake wiring, 2026-11-19. Canonical logs:
`D:\Core-build\core-review\` (Windows) and `wsl\` (WSL copies). Run1 (pre-hardening, same SHA)
and the 8c6837e reference are archived, not overwritten.

**Windows (run-all.ps1, SHA-bound canonical run, exit 0):**

| Step | Result |
| --- | --- |
| 02-build-debug | rc=0 (run1: 173 incremental steps; run2: no-op) |
| 03-ctest-debug (`-L merge -E engine_process_tests`) | **20/20 passed, 37.41 s**; 238 cases / 3078 verifications summed from the 14 suite summaries (cli tests carry no case counts) |
| 03b-engine-process (3 direct runs) | **3/3 exit=1**, «The engine exited (code 259)» @ `engine_process_tests.cpp:159`; 15 cases / 256 verifications / 1 failure per run (ENV-03, stage 3A) |
| 04-symbols | rc=0; stack frames resolve to `engine/src/core/tempo_map.cpp:<line>` from the build PDBs |
| 06-build-asan | rc=0, 77/77 targets, **no link failures** (extended wiring) |
| 07-asan-canary | 3/3 exit=1 + `heap-use-after-free` @ `tests/canary.cpp:10` (inside the vcvars shell) |
| 07b-asan-flags | **67/68 TUs** carry `/fsanitize=address`; only `miniaudio_impl.c` outside |
| 08-asan-ctest (`-L merge`, full) | **19/19 passed, 81.27 s**; 241 cases / 3176 verifications; counters `ERROR: AddressSanitizer` / `runtime error:` / `LeakSanitizer` / `ThreadSanitizer` / `FATAL` **all 0** |
| 09-lsp-smoke | rc=0, PASS (standalone clangd, no vcvars, `--query-driver`) |
| 09c Harness lsp | definition `tempo_map.hpp:21` → `tempo_map.cpp:47:18`; cross-TU `server.cpp:617` → same; findReferences **10 locations / 6 files** |
| 10-desktop-selftest | **1/1, 27.01 s**; `self-test.png` 74,427 B; `ok … document=75 … loudest=0.735`, null device 48000 Hz block 128, build `0.7.0+a9a038430d.modified` (the `.modified` is the uncommitted CMake wiring — declared) |
| verify.ps1 | **VERIFY: environment ready**, exit 0; all items OK incl. marker freshness (full fingerprint matches); compile DB **507 TUs** |

**WSL (run-wsl.sh, SHA-bound canonical run, exit 0):**

| Step | Result |
| --- | --- |
| 02-clone + hard gate | clone detached at `a9a0384` == Windows HEAD, **verified every run**; CMakeLists mirror `cmp`-verified |
| builds debug / asanubsan / tsan / tidy | all rc=0 (extended wiring links clean under GCC) |
| 20b flags | ASan+UBSan **66/67 TUs** (`-fsanitize=address,undefined` + `-fno-omit-frame-pointer`); only `miniaudio_impl.c` outside |
| 30b flags | TSan **65/66 TUs**; only `miniaudio_impl.c` outside |
| canaries | ASan **3/3** exit 1 @ `canary.cpp:10`; UBSan **3/3** exit 1 (`halt_on_error=1`) @ `canary.cpp:20`; TSan **5/5** exit 66 + `data race` @ `canary.cpp:38` under `setarch -R` |
| engine_tests (separate) | **exit=0** under ASan+UBSan AND TSan — ENV-08 closed by upstream BUG-SWEEP-06 |
| ctest asanubsan (`-L merge -E engine_tests`) | **17/17, 43.03 s**, all 5 report counters **0** |
| ctest tsan (under `setarch -R`) | **17/17, 92.34 s**, `WARNING: ThreadSanitizer` = **0**, `FATAL` = **0** |
| 40 tidy scope gate | rc=0: 48 TUs in db, 49 requested → **47 analysed, 2 excluded** (`engine_client.cpp` Windows-only `#error`; `juce_host.cpp` JUCE-gated), 0 missing; **negative control rc=1** as required |
| 40b clang-tidy 18.1.3 | **339 warnings, 0 errors**. Top files: `graph.cpp` 49, vendored `json.hpp` 31, `pcore.cpp` 22, `edit.cpp` 22, `server.cpp` 17. Top checks: `bugprone-easily-swappable-parameters` 122, `bugprone-narrowing-conversions` 63, `bugprone-implicit-widening-of-multiplication-result` 53, `bugprone-exception-escape` 13. Tool working, not a review verdict — triage is a separate task. |

**Validation proofs (fresh folders; historical reference untouched):**
- Windows `proof-windows.ps1`: **7/7 PASS** — P1 first run; P2 same-context reuse; P3 old revision
  (8c6837e worktree) invalidates; P4/P5 configure run+reuse; P6 flag change (`-AsanTests OFF`)
  invalidates only that configuration; P7 tracked local change at the SAME SHA invalidates
  (`local=E3B0C44…→5EFD433…` snapshots). Plus main-root reuse: `SKIP(reuse) 00-env / 04-symbols`.
- WSL `proof-wsl.sh`: **6/6 PASS** — W1 checkout at intended SHA + gate; W2 reuse + gate re-verified;
  W3 old revision invalidates (marker snapshot `sha=8c6837e…`); W4 return invalidates back;
  W5 fresh debug configure+build rc=0 (run1); W6 build reuse. Run1's W3/W4 fail-closed incident
  (Windows worktree unreadable from WSL) is preserved in `proof-wsl-log-run1.txt` and was fixed
  by using a WSL-side old-revision clone.
- Fingerprint note: Windows and WSL compute the local-diff hash in their own git environments
  (autocrlf/filemode differ across the drvfs boundary); each platform's markers are internally
  consistent and change-sensitive, which is what the binding requires. Cross-platform hash
  equality is not claimed.

## Fresh-session validation (stage 0.3, done for real)

A new agent with no context (`resume-check`, cwd `D:\CORE\CORE`, read-only) resumed from the
repository alone. Results:

- Trail: `README.md` → `docs/relatorios/README.md` → the 2026-11-19 rows → this report's
  `## Resume point` (lines 8–21). It confirmed `ESTADO.md` has no entry for this stream
  (expected — only the scribe writes it; the proposed lines are below) and that the index row
  is the current pointer.
- Baseline confirmed by measurement: HEAD = origin/main = a9a0384, ahead 0 / behind 0;
  86 commits from 8c6837e; CMake diff +13 (ENV-01 + extension); the exact porcelain set matches
  this report; `stash@{0}` untouched.
- Health check executed: `VERIFY: environment ready`, **exit 0**, all items OK incl. marker
  freshness; it correctly distinguished presence+canary from a suite run (no suite re-executed).
- Extra diagnostic: `ctest -N -L merge` in the recorded build dir → **21 tests**, reconciling
  21 − engine_process_tests = the recorded 20/20, and 20 (8c6837e) + modules + window −
  legacy_guard = 21.
- Critique and actions taken in this revision:
  1. *Fresh clone cannot resume* — declared, parked as the owner's commit decision; mitigated:
     `scripts-bundle/` (27 files) copied into this report's evidence directory, so the untracked
     archive now carries the scripts too.
  2. *ESTADO trail stale for this stream* — by design (D72/D80); proposed lines below await the scribe.
  3. *Baseline row count slip / PULL-01 snapshot numbers / trailing newline* — corrected: the row
     says four rows and lists this report + evidence; PULL-01 distinguishes the pull-time snapshot
     from the final diff; the README trailing newline was normalized (diff now purely additive +6).
  4. *JUCE patch bootstrap not spelled out* — explicit re-application recipe added to the resume
     point and the manifest; S11 (round 2) will automate it in CMake with a clean-build proof.
  5. *"Full fingerprint" overstated in the health-check row* — wording corrected: verify.ps1 checks
     the context prefix (sha+local+juce+script); per-step `cfg` is run-all.ps1's check; the WSL
     checkout gate is run-wsl.sh-only; `release\` is pinned and unchecked.
  6. *Owner CSV had no recorded path* — the CSV is archived in the evidence directory
     (`owner-csv-10-findings.csv`) with the partial triage notes; the confirmation report follows.
  7. *Minor* — `.qoderignore`/`environment.yml` explained in the Baseline row (IDE artifact /
     conda notebook env, not DAW environment components); the task board's session is named in
     this report's header.

## Pendências e passagem

**Owner decisions (pending, not blocking the next stages):**
1. Commit the tooling delivery — `tooling-delivery/manifest.md` has the exact file list, the
   partial-staging instruction for `README.md` (only the two 2026-11-19 rows), the proposed
   commit message, and the exclusions (other sessions' rows/reports, `.qoderignore`,
   `environment.yml`). No verbatim authorization found; stage 0 says commit only if authorized.
2. Ratify the `@local/dsh-cpp-lsp` desktop-profile install (19/11 handover decision 2).
3. Confirm `D:\Core-build\core-review` as the canonical out-of-tree environment directory
   (handover decision 3), and optionally commit the scripts under `ci/review-env/` so a fresh
   clone can rebuild the environment from the repository alone.

**Next tasks (shared task board, owner's suggested order):** stage 0 (task-1, this delivery) →
3A ENV-03 (task-2) → {1 RT baseline (task-3), 4 Soundtoys host (task-4), 2 timing proof (task-5)} →
6 Chain series (task-6, parallel gated by stage 2) → 5 failure recovery (task-7) → 7 Delay Drive
(task-8). The owner's CSV of 10 open findings («depois confirma») is under read-only confirmation
at this SHA (subagent `csv-triage`); its report follows separately — several findings intersect
upstream changes already pulled (reset watchdog, state-per-edit, Try again, JUCE patch infra).

**What stays unproven (do not claim it):** real-device audio; the JUCE app under ASan/UBSan/TSan;
real VST3 plugins under sanitizers; the RT-safety criterion (no alloc/lock/IO on the audio thread)
— measurable now, not proven; Release `engine_process_tests` at `a9a0384`; Linux does not prove
Windows runtime and the reverse (AGENTS.md).

**Proposed lines for ESTADO.md (the scribe writes them, D72/D80):**
- *Estado medido — Ambiente (2026-11-19, a9a0384 + wiring local por commitar)*: pull ff de 86
  commits (8c6837e→a9a0384) com o handover preservado; ambiente re-validado nas duas plataformas
  com marcadores ligados a SHA+diff local+JUCE+script+config (provas 7/7 Windows, 6/6 WSL, gate
  de checkout WSL fail-closed); Debug merge 20/20 (37,41 s), ASan MSVC 19/19 (81,27 s, 0 reports,
  67/68 TUs), WSL ASan+UBSan 17/17 (43,03 s) e TSan 17/17 (92,34 s, setarch -R), canários 3/3+3/3+5/5,
  tidy 339 avisos/0 erros (47 ficheiros), self-test ok document=75 loudest=0.735, lsp no Harness
  (definição hpp→cpp, cross-TU, 10 refs/6 ficheiros). ENV-08 fechado por referência (BUG-SWEEP-06):
  engine_tests exit 0 na WSL com e sem sanitizers. ENV-03 persiste em Debug (259, 3/3,
  engine_process_tests.cpp:159) — tarefa 3A; Release neste SHA não re-medido.
- *Pendência — commit do tooling*: entrega preparada e separada em
  D:\Core-build\core-review\tooling-delivery\ (CMake ENV-01 estendido a modules_tests/window_tests;
  relatórios 19/11 + este; scripts fora do Git com bundle). Sem autorização verbatim — não commitado
  (D48/D50; P-D98 aberta). Decisão: commit do tooling + ratificação dsh-cpp-lsp + directório canónico
  + (opcional) scripts em ci/review-env/.
- *Referência*: docs/relatorios/2026-11-19-pull-a9a0384-environment-refresh.md (este relatório);
  o relatório 19/11 mantém-se como evidência de 8c6837e (logs arquivados, não sobrescritos).

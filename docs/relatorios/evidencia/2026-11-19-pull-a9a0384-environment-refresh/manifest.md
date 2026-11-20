# Tooling delivery — Core review/debug/sanitize environment (stage 0)

Prepared 2026-11-19 (DSH lead session a69b320fbfab). Reference: `main @ a9a038430d781959829f8510067ce59afffaad62`
(= origin/main, fast-forward from 8c6837e8) **plus the uncommitted build-configuration change below**.
The SHA alone does not identify this tree: the sanitizer wiring fix is a working-tree modification.

**Status: PREPARED, NOT COMMITTED.** No verbatim owner authorization for this commit was found
(the 19/11 handover left it as an owner decision; P-D98 delegation scope is still open; D48/D50:
silence is not approval). Stage 0 instruction: «Faz o commit apenas se já estiver autorizado.»

## What belongs to THIS delivery (tooling only)

1. `CMakeLists.txt` — ENV-01 sanitizer wiring, build configuration only, no logic change:
   - at 8c6837e: added `legacy_guard_tests menu_tests` to `CORE_SANITIZED_TARGETS` (measured
     necessary: MSVC LNK2038 annotate_string/vector/optional, GCC undefined `__asan_*`);
   - extension at a9a0384 (this session): added `modules_tests window_tests` (new unconditional
     merge tests linking instrumented `core_domain`/`core_control`; same link failure otherwise).
     `legacy_guard_tests` was deleted upstream by the built-in-sound removal; the name is kept
     verbatim from the 8c6837e measurement and is inert (the `if(TARGET)` guard skips it).
   - Patch: `cmake-tooling.patch` (git diff HEAD -- CMakeLists.txt, applies with `git apply`).
2. `docs/relatorios/2026-11-19-review-debug-sanitize-environment.md` + its evidence directory
   `docs/relatorios/evidencia/2026-11-19-review-debug-sanitize-environment/` (44 files, 773 KB) —
   the 19/11 environment handover report (prior session, same tooling stream).
3. `docs/relatorios/2026-11-19-pull-a9a0384-environment-refresh.md` + its evidence directory —
   this session's pull + validation-hardening report (written with the measured numbers).
4. `docs/relatorios/README.md` — **only the two 2026-11-19 rows** (partial staging required,
   see below). Exact row texts: `readme-tooling-lines.txt`.

## What is EXCLUDED (other sessions / pre-existing — preserved untouched in the working tree)

- The two 2026-11-17 index rows in `docs/relatorios/README.md` (Plano da interface / Teste real
  da app) and their untracked reports + `evidencia/2026-11-17-teste-real-app/` — another session's
  delivery (ESTADO.md already records them as "relatórios de outra sessão, por commitar").
- `.qoderignore`, `environment.yml` — pre-existing untracked, not this delivery's.
- `stash@{0}` (flawless-crash cross-worktree recovery) — unrelated, untouched.

At commit time: stage `CMakeLists.txt` and the new report/evidence files whole; stage
`docs/relatorios/README.md` partially (`git add -p`) keeping ONLY the two 2026-11-19 rows.

## Scripts that live OUTSIDE Git (copies in `scripts-bundle/`)

Canonical location: `D:\Core-build\core-review\`
- `run-all.ps1` — Windows one-command environment (SHA-bound markers, see report §Validation).
- `run-wsl.sh` — WSL one-command environment (SHA-bound markers + hard checkout gate).
- `verify.ps1` — read-only health check (presence + canary + marker freshness); NOT a suite run.
- `proof-windows.ps1`, `proof-wsl.sh` — stage 0.2 reuse/invalidation proofs (fresh folders).
- `prep-wsl-repull.sh` — archives WSL evidence + resets the clone mirror before a revision change.
- `probe/stack_probe.cpp`, `probe/lsp-smoke.mjs` (+ built probe artifacts) — symbol/navigation proofs.
- Helper scripts from the 19/11 handover (release-probe.ps1, *-wsl.sh, etc.) — historical.

Recommendation (owner decision, not applied): commit these under `ci/review-env/` so a fresh
clone can rebuild the environment from the repository alone. Until then, this bundle + the
report's resume point are the reconstruction kit.

## Local prerequisites (where they live)

- Windows 11 x64; VS 18 Build Tools, MSVC 19.51.36260; vcvars64 at
  `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat`.
- CMake 4.4.4, Ninja 1.13.2 (Windows); WSL2 Ubuntu 24.04.5: GCC 13.3.0, clang/clang-tidy 18.1.3,
  CMake 3.28.3, Ninja 1.11.1, python3; **no gdb, no route to the apt mirror** (ENV-04).
- JUCE 9.0.5 extracted at `D:\Core-deps\JUCE-9.0.5` (no .git) with the vendored crash patch
  APPLIED (D113): `ci/patches/juce-failed-plugin-restart.patch` (14 hunks, two files under
  `modules/juce_audio_processors_headless/`). A re-fetch loses it — the scripts' fingerprint
  binds the patch file AND the touched files, so losing it invalidates every build step.
- clangd 23.1.0 at `C:\Users\vk_do\AppData\Local\Programs\clangd_23.1.0\bin\clangd.exe`;
  Node at `C:\Users\vk_do\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\node\bin\node.exe`.
- dsh-cpp-lsp wired into the desktop DSH profile (dependency + bundle + lsp-stdio override
  pointing clangd at `D:/Core-build/core-review/debug` with `--query-driver=...\cl.exe`) —
  outside this repo; ratification is a separate pending owner decision from the 19/11 handover.
- Out-of-tree builds: `D:\Core-build\core-review\{debug,asan,release}`;
  WSL `~/core-review-build/{debug,asanubsan,tsan,tidy}` on ext4; clone `~/core-review-src`.

## Proposed commit message (English, D101)

```
Build: complete CORE_SANITIZED_TARGETS for the merge tests (ENV-01)

legacy_guard_tests/menu_tests were missing at 8c6837e (MSVC LNK2038, GCC
undefined __asan_*); modules_tests/window_tests joined them at a9a0384.
Build configuration only, no logic change. With the review-environment
handover report (19/11) and the pull/validation-refresh report (a9a0384).
```

## Evidence

- Windows canonical run (SHA-bound): `D:\Core-build\core-review\NN-*.txt` + `*.done` (fp1 markers).
- Pre-hardening run at the same SHA: `archive-a9a0384-run1-prehardening/`.
- 8c6837e historical reference: `archive-8c6837e/`, `wsl/archive-8c6837e/`,
  WSL `~/core-review-build/archive-8c6837e/`.
- Proofs: `proof/proof-windows-summary.txt` (7/7 PASS), `wsl-proof/proof-wsl-log.txt`.
- Pre-pull backup of the handover diff: `pre-pull-2026-11-19-local-changes.patch`.

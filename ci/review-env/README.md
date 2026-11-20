# ci/review-env — agent environment for code review, debug, sanitization and JUCE UI/UX

Out-of-tree, idempotent environment tooling for the Core DAW. Prepared 2026-11-19, validation
hardened 2026-11-20 (SHA-bound markers). **Environment/tooling only — no product, DSP or UI code.**
Canonical working location: `D:\Core-build\core-review` (this directory is the in-repo copy so a
fresh clone can rebuild the environment; the scripts default to `D:\Core-build\core-review`).

## One-command scripts

| Script | Platform | What it does |
| --- | --- | --- |
| `run-all.ps1` | Windows/MSVC | Debug build (app+JUCE+tests) + compile DB, merge suite, debugger-symbol stack probe, ASan build + canary + suite, clangd smoke, JUCE desktop self-test. Out-of-tree builds under `D:\Core-build\core-review\{debug,asan}`. |
| `run-wsl.sh` | WSL2/GCC | Own ext4 clone + ASan+UBSan, TSan, plain Debug, clang-tidy (scope-gated by `ci/tidy_scope.py`). Builds under `~/core-review-build/{debug,asanubsan,tsan,tidy}`. |
| `verify.ps1` | Windows | **Read-only health check**: artifact presence + live ASan canary + marker freshness. NOT a suite run. Exit 0 only when ready. |
| `proof-windows.ps1` / `proof-wsl.sh` | both | Reuse/invalidation proofs of the SHA-bound validation in fresh folders. |
| `prep-wsl-repull.sh` | WSL | Archive WSL evidence + reset the clone mirror before a revision change. |
| `probe/stack_probe.cpp` | Windows | Debugger-symbol proof: walks a worker thread's stack inside `core::TempoMap`, resolves Core frames to `file:line` from the build PDBs. |
| `probe/lsp-smoke.mjs` | Windows | Standalone clangd parity (cross-file definition + references) without vcvars, via `--query-driver`. |

## SHA-bound validation (the false-green guard)

Every `.done` marker is bound to a fingerprint:

```
fp1 sha=<HEAD> local=<hash of tracked build-affecting diff, docs excluded> juce=<dep HEAD/diff or patch+touched-file hashes> script=<this script hash> cfg=<per-step configuration>
```

A step is **reused only when the whole fingerprint matches**; changing the revision, a tracked
source/CMake change, the vendored JUCE patch state, the script, or the step's configuration
**invalidates the affected steps**. Legacy markers without `fp1` always re-run. `run-wsl.sh`
additionally **hard-verifies the clone is detached at exactly the Windows HEAD on every run**
(fail-closed) before accepting any result. `verify.ps1` checks the context fingerprint
(sha+local+juce+script); per-step `cfg` is checked by `run-all.ps1` itself.

## Local prerequisites

- Windows 11 x64; VS 18 Build Tools (MSVC 19.51), vcvars64 at
  `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat`;
  CMake ≥ 4.4, Ninja. Conventions from `ci/gate.ps1`: unique guid `.bat`, `VSLANG=1033`, `chcp 65001`.
- JUCE 9.0.5 at `D:\Core-deps\JUCE-9.0.5` (extracted, no `.git`) **with the D113 crash patch
  applied** (`ci/patches/juce-failed-plugin-restart.patch`). Since S11 the CMake build applies the
  patch to a fetched copy idempotently and verifies a supplied local copy (fails early if unpatched).
  On a fresh manual extraction: `git apply ci/patches/juce-failed-plugin-restart.patch` inside it;
  verify with `git apply --check -R` (rc=0 ⇔ fully applied).
- WSL2 Ubuntu 24.04: GCC 13, clang/clang-tidy 18, CMake, Ninja, python3. **No gdb, no apt network**
  (known limitation). TSan needs `setarch $(uname -m) -R` (ASLR off) — built into `run-wsl.sh`.
- clangd 23.1.0; Node (DSH runtime). The `dsh-cpp-lsp` desktop-profile wiring (clangd pointed at the
  Debug compile DB with `--query-driver=cl.exe`) is outside this repo.

## Usage

```bat
powershell -NoProfile -ExecutionPolicy Bypass -File ci\review-env\run-all.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File ci\review-env\verify.ps1
wsl.exe -e bash ci/review-env/run-wsl.sh
```

Logs land beside the builds (`D:\Core-build\core-review\NN-*.txt` + fp1 `*.done`; WSL copies under
`wsl\`). Historical evidence is archived, never overwritten (`archive-<sha>/`). See
`docs/relatorios/2026-11-19-review-debug-sanitize-environment.md` and
`docs/relatorios/2026-11-19-pull-a9a0384-environment-refresh.md` for the full proofs and resume point.

# Report — 2026-11-18 — Measured ctest state (merge + desktop) at ed4642e and eef8db0

## What was measured

- Repository / SHA: `d-aicore/core`. Baseline `ed4642e61b78e30fc4f7d950a28a214d0ac7c801` (main); SUR-01 state
  `eef8db0b3aff95a3bf34bc655f721c77590d2f39` (= ed4642e + `5f3947f` engine change + docs; ancestry verified with
  `git merge-base --is-ancestor`). Report written in worktree `.agents/worktrees/flawless-estado` @ ed4642e.
- Scope and authorization: owner asked to "preencher o estado ctests" — measurement + documentation only.
  No product code edited, no `ci/gate.ps1` run, no `ESTADO.md` edit (ledger lines are PROPOSED below, D72/D80).
- Environment: Windows host (10.0.26300), MSVC Release / Ninja, CMake + ctest 4.4.4, JUCE 9.0.5 from
  `D:/Core-deps/JUCE-9.0.5`; both build caches have `CORE_BUILD_APP=ON`, `CORE_BUILD_TESTS=ON`.
- Build dirs used (both IDLE, verified before running: process scan for cmake/ninja/ctest/cl/link/CoreApp/
  core-engine/MSBuild returned nothing; last gate activity 04:13 and 04:47, runs at 05:08–05:13):
  - `D:/Core-build/gate-ed4642e` — built at ed4642e (`gate-resumo.txt`: "Codigo: D:\Core-Projects\gate-core @ ed4642e").
    Binaries confirmed ed4642e three ways: the fresh self-test report embeds `build=0.7.0+ed4642e61b`; engine_tests
    counts 25/130 (eef8db0 gives 26/160); gate log dated 04:13, before the source moved to eef8db0.
  - `D:/Core-build/gate-eef8db0` — built at eef8db0 (SUR stream's gate dir, idle; self-test report embeds
    `build=0.7.0+eef8db0b3a`). No rebuild was triggered; ctest only. The SUR stream was not disturbed.
  - Fidelity note: both builds use source `D:/Core-Projects/gate-core` (now at eef8db0, clean). Between the two SHAs
    only `engine/src/app/measure.cpp`, `engine/src/io/device.cpp`, `tests/engine_tests.cpp` and docs changed
    (`git diff --name-only ed4642e eef8db0`); `templates/` and `lang/`
    (the compile-time data paths the suites read) are identical, so gate-ed4642e's data-driven tests faithfully
    represent ed4642e.
- The fallback fresh build `D:/Core-build/estado-ctests` was NOT needed: both states were measurable in idle dirs.
- Active streams' dirs were not touched: `flawless-crash`, `flawless-batch`, `flawless-deadcode`.
- Limitations: the five `cli_*` tests print no case/verification counts (by design) — they count as ctest passes
  only. `desktop_self_test` reports steps, not cases/verifications. Counts are the suites' own CHECK counters under
  MSVC Release; no sanitizer round in this task. Self-test runs on the null audio device; real-device audio is out
  of scope here.

## Results — merge label (per suite)

All rows below are **measured-by-me** on 2026-11-18, from `ctest -L merge -V --output-on-failure` (exit 0 both):

- ed4642e: `ctest --test-dir D:/Core-build/gate-ed4642e -L merge -V --output-on-failure`
- eef8db0: `ctest --test-dir D:/Core-build/gate-eef8db0 -L merge -V --output-on-failure`

| Suite | ed4642e cases | ed4642e verifs | ed4642e fails | eef8db0 cases | eef8db0 verifs | eef8db0 fails |
| --- | --- | --- | --- | --- | --- | --- |
| engine_tests | 25 | 130 | 0 | 26 | 160 | 0 |
| domain_tests | 19 | 148 | 0 | 19 | 148 | 0 |
| role_tests | 21 | 216 | 0 | 21 | 216 | 0 |
| midi_tests | 8 | 115 | 0 | 8 | 115 | 0 |
| bug_tests | 22 | 138 | 0 | 22 | 138 | 0 |
| audit_tests | 7 | 65 | 0 | 7 | 65 | 0 |
| audit2_tests | 6 | 80 | 0 | 6 | 80 | 0 |
| hunt_tests | 14 | 649 | 0 | 14 | 649 | 0 |
| round3_tests | 7 | 82 | 0 | 7 | 82 | 0 |
| calls_tests | 10 | 44 | 0 | 10 | 44 | 0 |
| bitwig_tests | 2 | 25 | 0 | 2 | 25 | 0 |
| engine_process_tests | 4 | 49 | 0 | 4 | 49 | 0 |
| **Counting suites total** | **145** | **1741** | **0** | **146** | **1771** | **0** |
| cli_version | pass | n/p | 0 | pass | n/p | 0 |
| cli_demo_render | pass | n/p | 0 | pass | n/p | 0 |
| cli_measure | pass | n/p | 0 | pass | n/p | 0 |
| cli_unicode | pass | n/p | 0 | pass | n/p | 0 |
| cli_plugin_crash_render | pass | n/p | 0 | pass | n/p | 0 |
| **ctest merge label** | **17/17 pass** | | **0** | **17/17 pass** | | **0** |

n/p = no case/verification counts printed by the cli_* tests. The only ed4642e→eef8db0 delta is engine_tests
+1 case / +30 verifications (SUR-01, commit `5f3947f`, `tests/engine_tests.cpp` +47 lines).

## Results — desktop label and full set

**Measured-by-me**, `ctest -L desktop -V --output-on-failure` (exit 0 both; opens Core's window ~20 s, null device):

| State | ctest desktop | Self-test verdict | Steps | Failures | Build ID in report | Revisions | Time |
| --- | --- | --- | --- | --- | --- | --- | --- |
| ed4642e (gate-ed4642e) | 1/1 pass | ok | 49 | 0 | `0.7.0+ed4642e61b` | document=61 accepted=61 applied=61 | 19.37 s |
| eef8db0 (gate-eef8db0) | 1/1 pass | ok | 49 | 0 | `0.7.0+eef8db0b3a` | document=61 accepted=61 applied=61 | 19.86 s |

The desktop suite is one ctest case (`desktop_self_test`, `tests/CMakeLists.txt:124-127`); its internal unit is the
step (49 steps: play, undo, save/reopen, export, engine kill/restart, plugin state kept across the engine crash,
automation, autosave recovery, mixer, picture — full list in the committed self-test reports). It prints no case/verification counters.

Full set (no label filter), **measured-by-me**, `ctest --test-dir <dir> --output-on-failure` (exit 0 both):
**18/18 pass at ed4642e and 18/18 at eef8db0** ("100% tests passed out of 18"). The full set is exactly
merge (17) + desktop (1); the labels are disjoint (`ctest -N` lists 18).

## Reconciliation with the older 15 / 113 / 1456 denominator

The 17/11 baseline (15 ctest tests, 113 cases, 1456 verifications) was measured on the 17/11 snapshot under
WSL GCC and ASan MSVC — per-suite counts in
[`2026-11-17-preflight-sanitizadores-clang-tidy.md`](2026-11-17-preflight-sanitizadores-clang-tidy.md) line 186
(reported here, not re-run). It is a different scope (sanitizers, older snapshot), not a conflicting measurement.
Suite-by-suite delta to ed4642e (MSVC Release, measured above):

| Suite | 17/11 (cases/verifs) | ed4642e (cases/verifs) | Delta |
| --- | --- | --- | --- |
| engine_tests | 20/81 | 25/130 | +5/+49 |
| domain_tests | 9/64 | 19/148 | +10/+84 |
| role_tests | 21/216 | 21/216 | 0 |
| midi_tests | 8/115 | 8/115 | 0 |
| bug_tests | 10/49 | 22/138 | +12/+89 |
| bitwig_tests | 2/25 | 2/25 | 0 |
| audit_tests | 7/65 | 7/65 | 0 |
| audit2_tests | 5/66 | 6/80 | +1/+14 |
| hunt_tests | 14/649 | 14/649 | 0 |
| round3_tests | 7/82 | 7/82 | 0 |
| calls_tests | 10/44 | 10/44 | 0 |
| engine_process_tests | — (did not exist) | 4/49 | +4/+49 (new, Windows-only) |
| cli_* | 4 tests, no counts | 5 tests, no counts | +1 (`cli_plugin_crash_render`) |
| **Total** | **113/1456, 15 ctest tests** | **145/1741, 17 ctest tests** | **+32/+285, +2 tests** |

Sums check: 113+32=145, 1456+285=1741. No suite lost cases or verifications. eef8db0 adds +1/+30 on top (146/1771).

## Corrections to existing evidence (addendum, not rewrite)

1. **`D:/Core-build/flawless-crash-evidence/baseline-suite-counts.txt` is a mixed state, not pure ed4642e.**
   Its `engine_process_tests: 8 testes, 93 verificacoes` line includes the four extra crash cases added by the
   item-1 stream on branch flawless-crash (commits `d434720` "crash tests for the plugin entry points outside
   processBlock (FL item 1, A)" and `81f835a` "a crash in a parameter getter or a plugin window … (FL item 1, B)";
   the 8-case output is visible in the same dir's `A-engine_process_tests-green.txt`). At ed4642e,
   `tests/engine_process_tests.cpp` has exactly 4 TESTs and the suite measures **4/49** (measured-by-me in
   gate-ed4642e). The file's other 11 lines match my ed4642e measurement exactly. Its implicit totals
   (149 cases / 1785 verifications) therefore overstate ed4642e; the true ed4642e merge totals are **145/1741**.
2. **`C-selftest-green.txt` in the same dir is not ed4642e.** It embeds `build=0.7.0+81f835af79.modified`
   (item-1 stream working state) and shows document=68 with the extra plugin-crash steps. The ed4642e desktop
   self-test, measured here, is ok with 49 steps and document=61. It remains valid evidence for the item-1 state.

## Verification (commands and exit codes)

All run 2026-11-18 05:08–05:13 local, in idle dirs, no rebuild, no gate.ps1:

| Command | Exit | Result |
| --- | --- | --- |
| `ctest --test-dir D:/Core-build/gate-ed4642e -L merge -V --output-on-failure` | 0 | 17/17, 145 cases / 1741 verifs / 0 fails |
| `ctest --test-dir D:/Core-build/gate-ed4642e -L desktop -V --output-on-failure` | 0 | 1/1, self-test ok, 49 steps |
| `ctest --test-dir D:/Core-build/gate-ed4642e --output-on-failure` | 0 | 18/18 |
| `ctest --test-dir D:/Core-build/gate-eef8db0 -L merge -V --output-on-failure` | 0 | 17/17, 146 cases / 1771 verifs / 0 fails |
| `ctest --test-dir D:/Core-build/gate-eef8db0 -L desktop -V --output-on-failure` | 0 | 1/1, self-test ok, 49 steps |
| `ctest --test-dir D:/Core-build/gate-eef8db0 --output-on-failure` | 0 | 18/18 |

Evidence (committed): [evidencia/2026-11-18-estado-ctests/](evidencia/2026-11-18-estado-ctests/) — verbose merge
logs (per-suite count lines), desktop verbose logs, full-set logs, and the two fresh `self-test.txt` reports
(with embedded build IDs). Working copies also in `D:/Core-build/estado-ctests-evidence/`.

## PROPOSED ESTADO.md lines (scribe only — D72/D80; not written here)

For "Estado medido" (citation + source per line):

1. `merge @ ed4642e (Windows, MSVC Release): 17/17 ctest, 145 cases / 1741 verifications / 0 failures across the
   12 counting suites; 5 cli_* pass without case counts. desktop: 1/1, self-test ok, 49 steps, null device.
   Full set 18/18. Measured 2026-11-18 in idle D:/Core-build/gate-ed4642e (build ID ed4642e61b).
   Source: docs/relatorios/2026-11-18-estado-ctests.md + evidencia/2026-11-18-estado-ctests/.`
2. `merge @ eef8db0 (SUR-01, Windows, MSVC Release): 17/17 ctest, 146 cases / 1771 verifications / 0 failures
   (engine_tests 26/160, +1/+30 vs ed4642e; all other suites unchanged). desktop: 1/1, ok, 49 steps. Full 18/18.
   Measured 2026-11-18 in idle D:/Core-build/gate-eef8db0 (build ID eef8db0b3a).
   Source: docs/relatorios/2026-11-18-estado-ctests.md.`
3. `Denominator reconciliation: 17/11's 15 tests / 113 cases / 1456 verifications (WSL GCC + ASan MSVC, older
   snapshot) → ed4642e's 17 / 145 / 1741 (MSVC Release): +2 ctest tests (engine_process_tests 4/49,
   cli_plugin_crash_render), +32 cases / +285 verifications (engine +5/+49, domain +10/+84, bug +12/+89,
   audit2 +1/+14, engine_process +4/+49); no suite lost coverage.
   Source: docs/relatorios/2026-11-18-estado-ctests.md; old per-suite counts from
   docs/relatorios/2026-11-17-preflight-sanitizadores-clang-tidy.md:186.`
4. Correction by reference (invalidação por referência): `D:/Core-build/flawless-crash-evidence/
   baseline-suite-counts.txt line engine_process_tests 8/93 is the item-1 state (d434720 + 81f835a), not ed4642e
   (measured 4/49); its totals 149/1785 are a mixed state. C-selftest-green.txt in the same dir is
   81f835a.modified, not ed4642e. Source: docs/relatorios/2026-11-18-estado-ctests.md §Corrections.`

Pendência (owner = Diogo): ledger scribe to transcribe lines 1–4; no product decision is blocked by this report.

## Changes

Documentation only: this report, its evidence directory, and one index row in `docs/relatorios/README.md`.
No product code, no test code, no `ESTADO.md`, no gate scripts touched.

## Next steps

- Scribe transcribes the proposed lines; the item-1 stream should relabel its `baseline-suite-counts.txt`
  capture as the post-A/B state (or re-capture a pure ed4642e baseline) so future streams do not cite 8/93
  as baseline.
- Uncovered scope: sanitizer rounds at these SHAs (not run here), real-device audio (SUR-01's own report covers
  WASAPI telemetry), and any case counts for cli_* (they print none by design).
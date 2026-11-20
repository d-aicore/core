# 2026-11-19 - Environment for code review, debugging and sanitization (agent handover)

Base: **main @ 8c6837e80030e2185d5f6016f0365bae2c3f52bd** (measured 2026-11-19 21:26 Europe/Lisbon with `git rev-parse HEAD`).
Platform: Windows 11 `10.0.26300.9974` x64; MSVC `19.51.36260` (VS 18 Build Tools 18.10.2, toolset 14.51.36231); CMake 4.4.4; Ninja 1.13.2;
WSL2 Ubuntu 24.04.5, GCC 13.3.0, clang/clang-tidy 18.1.3, **no gdb** (the WSL has no route to the apt mirror).
Scope: **environment and development tooling only**. No product, DSP or UI code was changed.
Authorisation: the owner on 2026-11-19 - «prepara so o ambiente» and «nao tentes melhorar a app so queremos ter todas
as ferramentas para testtar sanitizar, melhorar motor, tambem para design ui, ux do juce, aqui estamos a criar op
ambiente perfeito».

## Resume point (start here: next agent, next model)

| What | Where / how |
| --- | --- |
| Baseline | `main` @ `8c6837e8`; branch is **behind origin/main** (measured by the fresh-session check below). Working tree, measured before and after this delivery: `M CMakeLists.txt` (this task, ENV-01), `M docs/relatorios/README.md` (another session's two rows, preserved + this delivery's row), and pre-existing untracked files not touched here: `.qoderignore`, `environment.yml`, `docs/relatorios/2026-11-17-plano-interface-chain-abas-laterais.md`, `docs/relatorios/2026-11-17-teste-real-app.md`, `evidencia/2026-11-17-teste-real-app/`. `stash@{0}` (unrelated) left untouched. Everything this delivery produced is **uncommitted** - see the risk note in Next action |
| One-command Windows environment (idempotent) | `powershell -NoProfile -ExecutionPolicy Bypass -File D:\Core-build\core-review\run-all.ps1` |
| One-command WSL environment (idempotent) | `wsl.exe -e bash /mnt/d/Core-build/core-review/run-wsl.sh` |
| One-command health check (read-only) | `powershell -NoProfile -ExecutionPolicy Bypass -File D:\Core-build\core-review\verify.ps1` (add `-Lsp` for the clangd smoke; prints OK/MISSING per item and exits 0 only when ready) |
| Builds (out of tree, never in the repo) | `D:\Core-build\core-review\{debug,asan,release}`; WSL `~/core-review-build/{debug,asanubsan,tsan,tidy}` on ext4 |
| Compile database for clangd/clang-tidy | `D:\Core-build\core-review\debug\compile_commands.json` (505 TUs, MSVC flags) |
| Logs of every step | `D:\Core-build\core-review\NN-*.txt` (markers `NN-*.done`); WSL copies under `D:\Core-build\core-review\wsl\` |
| Current state | environment built and proven on both platforms; see the evidence table and ENV-01..ENV-08 |
| Next action | (1) read ENV-01/ENV-02 and decide on committing the two tooling changes; (2) run the two one-command scripts (idempotent) and `verify.ps1`; (3) note that a plain `ctest -L merge` on the Debug build exits non-zero only because of `engine_process_tests` (ENV-03): the script runs the suite with `-E engine_process_tests` and measures that test separately |

## Reproduce from scratch (no scripts needed)

```bat
REM Windows: Debug build + compile database (the scripts wrap exactly this, under vcvars64)
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set VSLANG=1033 & chcp 65001 >nul
cmake -S D:\CORE\CORE -B D:\Core-build\core-review\debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCORE_BUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "-DFETCHCONTENT_SOURCE_DIR_JUCE=D:/Core-deps/JUCE-9.0.5"
cmake --build D:\Core-build\core-review\debug --parallel 8
ctest --test-dir D:\Core-build\core-review\debug -L merge --output-on-failure --timeout 300 --no-tests=error
REM ASan (separate tree; the same vcvars shell is REQUIRED to run the canary: it needs the ASan runtime DLLs)
cmake -S D:\CORE\CORE -B D:\Core-build\core-review\asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCORE_BUILD_TESTS=ON -DCORE_BUILD_APP=OFF -DCORE_SANITIZE=address
cmake --build D:\Core-build\core-review\asan --parallel 8
D:\Core-build\core-review\asan\tests\asan_canary.exe   REM expected: exit 1 + heap-use-after-free at canary.cpp:10
ctest --test-dir D:\Core-build\core-review\asan -L merge -V --timeout 900 --no-tests=error
```

```sh
# WSL: own clone on ext4, three out-of-tree configurations
git clone /mnt/d/CORE/CORE ~/core-review-src && cd ~/core-review-src   # detached at the Windows HEAD
cmake -S . -B ~/core-review-build/asanubsan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCORE_BUILD_APP=OFF \
      -DCORE_BUILD_TESTS=ON -DCORE_SANITIZE=address,undefined
cmake --build ~/core-review-build/asanubsan -j
UBSAN_OPTIONS=halt_on_error=1 ~/core-review-build/asanubsan/tests/ubsan_canary   # expected: exit 1 + runtime error
ctest --test-dir ~/core-review-build/asanubsan -L merge -V --no-tests=error
cmake -S . -B ~/core-review-build/tsan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCORE_BUILD_APP=OFF \
      -DCORE_BUILD_TESTS=ON -DCORE_SANITIZE=thread
cmake --build ~/core-review-build/tsan -j
setarch $(uname -m) -R ~/core-review-build/tsan/tests/tsan_canary   # -R: TSan needs ASLR off in this WSL
cmake -S . -B ~/core-review-build/tidy -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCORE_BUILD_APP=OFF \
      -DCORE_BUILD_TESTS=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build ~/core-review-build/tidy -j
python3 ci/tidy_scope.py ~/core-review-build/tidy/compile_commands.json . && \
  clang-tidy -p ~/core-review-build/tidy --checks='-*,bugprone-*,concurrency-*,performance-*,clang-analyzer-*' \
  --header-filter='(engine|app)/(src|include)/' engine/src/core/*.cpp engine/src/dsp/*.cpp
```

## What was built

### 1. Code navigation (clangd through the Harness)

The `dsh-cpp-lsp` plugin was **not loaded** at the start: the `lsp` tool did not exist in the session, the desktop
profile had no LSP bundle, and the published `@deepseek-ai/dsh-lsp*` packages are not shipped inside the app.

- Installed the supported way: `dsh plugin --profile lspverify add file:<workspace>/dsh-cpp-lsp` created a scratch
  profile and installed 8 packages (`@local/dsh-cpp-lsp` + `@deepseek-ai/dsh-lsp`, `dsh-lsp-stdio`, `dsh-tool-lsp`
  at 0.2.2-rc.5, the app's own version).
- The `desktop` profile is managed by the Electron app, so the CLI refuses it; the same changes were applied to its
  files (dependency + bundle in `package.json`, id-targeted `lsp-stdio` override in `cordis.patch.yml`), then
  **proven by dumping an identical copy profile**: `dsh --profile deskcheck --dump-config` exits 0 and contains
  `lsp`, `lsp-stdio` (with the Core compile database and cl.exe as query driver) and `tool-lsp`.
- The override points clangd at `D:/Core-build/core-review/debug` and passes `--query-driver=...\cl.exe` because the
  app does not inherit the vcvars environment (the MSVC system headers would not be found otherwise). Verified
  against the live clangd process command line.
- **Proof inside the Harness** (the `lsp` tool itself, workspace `D:\CORE\CORE`): `tempo_map.hpp:21` -> definition
  `engine/src/core/tempo_map.cpp:47`; `server.cpp:552` (another translation unit) -> the same `tempo_map.cpp:47`;
  `findReferences` -> 3 locations in 2 files; `hover` -> signature. Log: `09b-lsp-harness-proof.txt`.
- Cold-index caveat: the first query after a DSH start can return the declaration because clangd is still indexing;
  retrying a few seconds later returns the definition.

### 2. Debug build and debugger symbols

- `run-all.ps1` configures a normal Core build **Debug** out of tree (JUCE from `D:/Core-deps/JUCE-9.0.5`, tests ON,
  `CMAKE_EXPORT_COMPILE_COMMANDS=ON`): 553 targets, 505 translation units, PDBs listed in `02b-pdbs.txt`.
  Build rc=0: `02-build-debug.txt`.
- **Symbols proven** by a dedicated stack probe (`probe/stack_probe.cpp`): a worker thread loops inside
  `core::TempoMap::sampleAt`/`tickAt` linked from the real `core_engine_lib.lib`; the main thread suspends it and
  walks the stack with `StackWalk64` + `SymFromAddr` + `SymGetLineFromAddr64`. Result: frames resolving to
  `engine\src\core\tempo_map.cpp:<line>` from the build's PDBs. Log: `04-symbols.txt`.
- **Where the proof ran**: in the engine library through a dedicated probe, **not** in the JUCE app and not in a
  running DAW session.
- JUCE UI/UX path: `desktop_self_test` (Core.exe --self-test, null device) is green in **24.17 s** and writes a
  **1560x734** screenshot; the window renders arrangement, inspector, step sequencer and transport, status bar
  `TEST BUILD (Debug) 0.7.0+8c6837e.modified`. Log: `10-desktop-selftest.txt`, image
  `D:\Core-build\core-review\debug\tests\self-test.png`.

### 3. Sanitizers

The existing `CORE_SANITIZE` wiring and the canaries (`tests/canary.cpp`) were reused. **A real gap was found and
fixed (ENV-01)** before any sanitized build could link.

- **ASan with MSVC** (`-DCORE_SANITIZE=address -DCMAKE_BUILD_TYPE=Debug -DCORE_BUILD_APP=OFF`, out of tree): build
  91/91 targets; flags proven from the compile database (all our TUs carry `/fsanitize=address`, only
  `miniaudio_impl.c` deliberately not instrumented); the canary trips **3/3** with exit 1 and
  `ERROR: AddressSanitizer: heap-use-after-free ... tests/canary.cpp:10`; the merge suite runs with the report
  counters grepped from the full verbose log: **18/18 tests pass in 460 s with 0 reports** (E11). Logs: `06-build-asan.txt`, `07-asan-canary.txt`,
  `07b-asan-flags.txt`, `08-asan-ctest.txt`, `08b-asan-summary.txt`.
- **ASan+UBSan in WSL** (GCC 13, separate tree): 67 TUs, **66 instrumented**, only `miniaudio_impl.c` outside; ASan
  canary 3/3 exit 1 with the report at `canary.cpp:10`; UBSan canary prints
  `runtime error: signed integer overflow ... canary.cpp:20` and, with `UBSAN_OPTIONS=halt_on_error=1`, exits **1
  3/3** (`ubsan-wsl-canary-strict.txt`). Merge suite: **16/16, 163.55 s, 0 sanitizer reports**. `engine_tests` is
  excluded from that green run and recorded on its own: it fails case FL-1 with
  `excepcao: Cannot open the audio device: Invalid operation`, and **the same case fails the same way in the plain
  Debug build of the same tree** - the WSL having no openable audio device (ENV-08), not the sanitizers. Files
  `asanubsan-ctest-green.txt`, `asanubsan-engine-tests-known.txt`.
- **TSan in WSL** (separate tree, `-DCORE_SANITIZE=thread`): flags proven (63/66 TUs carry `-fsanitize=thread`).
  Under the default ASLR of this WSL2 kernel every TSan binary dies before `main` with
  `FATAL: ThreadSanitizer: unexpected memory mapping` (exit 66, 5/5) - a known TSan/ASLR interaction, not missing
  instrumentation (ENV-07). Under `setarch $(uname -m) -R` (ASLR off) the canary reports the data race 3/3
  (`tsan-wsl-canary-setarch.txt`, `SUMMARY: ThreadSanitizer: data race ... canary.cpp:38`), and the merge suite was
  re-run under the same wrapper with `engine_tests` excluded (recorded separately): **16/16, 223.82 s,
  `WARNING: ThreadSanitizer` = 0 and `FATAL` = 0**. The excluded test fails FL-1 for the same WSL device reason in
  both sanitizer configurations (ENV-08), and that failure is kept in its own log.
- **Where the sanitized runs happen**: `engine_tests`, `domain_tests` and the other test executables (the engine and
  the app's domain library), on the **null audio backend**. Not the JUCE UI, not VST3 plugins, not a real device.
- **Coverage that stays out**: every `juce_*`/JUCE target and the VST3 wrappers (JUCE is not instrumented by design,
  D92); `miniaudio_impl.c`; the audio callback as such (the tests do not open a device); the desktop self-test under
  sanitizers; and anything that only happens with real plugins under a real device.

### 4. clang-tidy

`clang-tidy` does not exist on the Windows host and cannot be installed in the WSL (no network). The WSL has
clang-tidy 18.1.3; the run uses the WSL compile database produced by `-DCORE_BUILD_APP=OFF` (so `juce_host.cpp` is
outside the tidy scope - the same limitation already recorded in
`docs/relatorios/2026-11-17-preflight-sanitizadores-clang-tidy.md` PF-11). The scope gate `ci/tidy_scope.py` is run
first (and its negative control must fail), then the analysis with the checks used by the previous round
(`-*,bugprone-*,concurrency-*,performance-*,clang-analyzer-*`). Result: scope gate **rc=0** (49 analysed / 2 excluded, negative control **rc=1** as required) and clang-tidy
finished on the 50-file list with **351 warnings, 0 product errors**. The two `clang-diagnostic-error` lines in the
log are the Windows-only `engine_client.cpp` (`#error`, `windows.h` not found) that the gate excludes but the run's
file list still named; the script now feeds clang-tidy the gate's own list (`40-tidy-scope.txt`), so future runs have
neither. Top files: `graph.cpp` 49, vendored `json.hpp` 31, `edit.cpp` 25, `pcore.cpp` 22, `server.cpp` 17. Top
checks: `bugprone-easily-swappable-parameters` 132, `bugprone-narrowing-conversions` 63,
`bugprone-implicit-widening-of-multiplication-result` 53, `bugprone-exception-escape` 13,
`performance-inefficient-vector-operation` 12, `performance-enum-size` 11, `bugprone-empty-catch` 11,
`performance-inefficient-string-concatenation` 9, `concurrency-mt-unsafe` 5 (three of them in vendored `json.hpp`).
**This is the tool working, not a review verdict**: triaging the 351 findings (RT allocations/locks/IO, thread
communication, plugin isolation) is the separate review task. Logs: `40-tidy-round.txt`, `40-tidy-summary.txt`,
`40-tidy-scope-gate.txt`.

### 5. The test failure this environment surfaced

- **`engine_process_tests` fails in Debug, passes in Release** (ENV-03).
- The rest of `ctest -L merge` is green: **19/19 in 88.30 s** on Windows Debug (the script runs it with
  `-E engine_process_tests` and measures that test separately, 3/3 failing with 259 in Debug and 3/3 passing in
  Release) and **16/16** in each WSL sanitizer suite (the script excludes `engine_tests` there and records its
  environmental FL-1 failure separately, ENV-08). The evidence table has the numbers.

## Evidence

| # | Check | Command (repeatable) | Result | Artefact |
| --- | --- | --- | --- | --- |
| E1 | Baseline recorded | `git rev-parse HEAD; git status --porcelain` | `8c6837e8...`; `M docs/relatorios/README.md`, `M CMakeLists.txt` | `00-env.txt` |
| E2 | Debug build | `run-all.ps1` | 553 targets, rc=0 | `02-build-debug.txt` |
| E2b | Debug merge suite | `ctest -L merge -E engine_process_tests -V` | **19/19 passed, 88.30 s** | `03-ctest-debug.txt` |
| E3 | Compile database | same | 505 TUs, MSVC flags | `compile_commands-debug.json` |
| E4 | Debugger symbols | stack probe | frames at `tempo_map.cpp:<line>` | `04-symbols.txt` |
| E5 | JUCE UI/UX | `ctest -L desktop` | Passed 24.17 s + screenshot 1560x734 | `10-desktop-selftest.txt`, `self-test.png` |
| E6 | LSP definition (hpp->cpp) | `lsp goToDefinition` | `tempo_map.cpp:47` | `09b-lsp-harness-proof.txt` |
| E7 | LSP definition (cross-TU) | `lsp goToDefinition` from `server.cpp:552` | `tempo_map.cpp:47` | same |
| E8 | LSP references | `lsp findReferences` | 3 in 2 files | same |
| E8b | Standalone clangd parity | `probe/lsp-smoke.mjs` with the plugin's exact configuration (no vcvars, no INCLUDE) | PASS after index warm-up: definition `tempo_map.cpp:47`, **8 references in 5 files** (`engine.cpp`, `graph.cpp`, `server.cpp`, `tempo_map.cpp`, `bug_tests.cpp`) | `09-lsp-smoke.txt` |
| E9 | ASan MSVC flags | compile db grep | **67/68 TUs** instrumented (only `miniaudio_impl.c` outside) | `07b-asan-flags.txt` |
| E10 | ASan MSVC canary | canary 3x **inside the vcvars shell** (bare execution fails with 0xC0000135 - ASan runtime DLLs are not on PATH) | exit 1 3/3 + report | `07-asan-canary.txt` |
| E11 | ASan MSVC suite | `ctest -L merge -V` | **18/18 passed, 460.33 s, all sanitizer counters 0** | `08-asan-ctest.txt`, `08b-asan-summary.txt` |
| E12 | WSL ASan+UBSan flags | compile db | 66/67 TUs | `20-asanubsan-flags.txt` |
| E13 | WSL ASan canary | 3x | exit 1 3/3 + report | `asan-wsl-canary.txt` |
| E14 | WSL UBSan canary | 3x, halt_on_error | exit 1 3/3 + report | `ubsan-wsl-canary-strict.txt` |
| E15 | WSL TSan flags/canary | compile db, 5x | see `30-tsan-flags.txt`, `tsan-wsl-canary.txt` | same |
| E16 | WSL sanitized suites | `ctest -L merge -V -E engine_tests` (the excluded test recorded separately) | ASan+UBSan **16/16, 163.55 s, 0 reports**; TSan under `setarch -R` **16/16, 223.82 s, 0 TSan warnings/FATAL**; `engine_tests` exit=1 recorded separately in both (FL-1 device, ENV-08) | `asanubsan-ctest-green.txt`, `tsan-ctest-green.txt`, `*-engine-tests-known.txt` |
| E17 | clang-tidy scope + run | `ci/tidy_scope.py` + clang-tidy 18.1.3 | scope gate **rc=0** (49 analysed, 2 excluded with reasons, negative control **rc=1** as required); analysis finished: **351 warnings, 0 product errors** in 49 files | `40-tidy-scope-gate.txt`, `40-tidy-round.txt`, `40-tidy-summary.txt` |
| E18 | engine_process_tests Debug vs Release | direct runs | Debug **3/3 exit=1** (code 259), Release **3/3 pass** | `03b-engine-process-summary.txt`, `release/release-probe.txt` |

## Findings

**ENV-01 (confirmed, fixed as build configuration) - the D92 sanitizer wiring could not link a sanitized tree.**
`CMakeLists.txt:92-98` lists the targets to instrument but **omitted `legacy_guard_tests` and `menu_tests`**, which
are merge tests (`tests/CMakeLists.txt:114`) linking the instrumented `core_domain`/`core_engine_lib`. MSVC refused
the mix with `LNK2038 annotate_string/vector/optional`; GCC failed with `undefined reference to __asan_report_store8`
(and `__tsan_*`). Without the fix **no sanitized build of this tree could be produced at all**. Fix applied: the two
names added to the list (build configuration only, no logic change). This is the only product-tree edit of this
delivery; it should be committed by whoever owns the tree.

**ENV-02 (confirmed, fixed as environment configuration) - `dsh-cpp-lsp` was not loaded.**
Evidence before: the `lsp` tool did not exist (`tools.lsp is not a function`), the profile had only the base and web
bundles, and the app bundle contains no `dsh-lsp` code. Installed and wired as described above; the dump proves the
composition and the tool proves navigation. The `desktop` profile edits are: one dependency, one bundle name, one
id-targeted override - reversible by deleting those three and running `pnpm install`.

**ENV-03 (confirmed, measured) - `engine_process_tests` fails only in Debug.**
In Debug it fails 2/2 with `The engine exited (code 259)`; 259 is `STILL_ACTIVE`. The client emits that reason from
`app/engine-client/src/engine_client.cpp:333-338`: after the pipes close it waits **2 s** and then reads the exit
code, so a slower Debug shutdown (JUCE plugin teardown after the crash/retry scenario) is reported as 259. In
**Release the same test passes 3/3** (10 cases / 121 checks each, including the crash case). This is a Debug-timing
characteristic of the test/engine, not flakiness and not a sanitizer problem; a product/test fix belongs to a
separate task (either a longer wait in the client's disconnect path, or a faster engine close).

**ENV-04 (limitation) - no debugger in the WSL.** `gdb` is absent and the WSL has no route to the apt mirror
(`archive.ubuntu.com` unreachable, apt rc=100), so the WSL breakpoint proof could not run. The Windows stack probe
covers the "debugger has symbols / stack points at Core file:line" requirement.

**ENV-05 (limitation) - clang-tidy has no Windows host and no `juce_host.cpp` coverage.** Same as PF-11 of the
2026-11-17 preflight; recorded again here rather than silently dropped.

**ENV-07 (limitation) - TSan needs ASLR disabled in this WSL.** The TSan binaries abort with
`FATAL: ThreadSanitizer: unexpected memory mapping` unless run under `setarch $(uname -m) -R`; with it the canary
reports the expected data race. Any future TSan run in this WSL must use the wrapper, otherwise the failure looks
like a broken build.

**ENV-08 (environment, not sanitizer) - no openable audio device in the WSL.** `engine_tests` case FL-1 fails with
`excepcao: Cannot open the audio device: Invalid operation` **with and without sanitizers** in the same WSL tree;
it is the WSL lacking a device the default backend can open, not instrumentation. The Windows build of the same SHA
passes that case (only the Debug `engine_process_tests` differs, ENV-03).

**ENV-06 (behaviour to know) - clangd cold index.** The first definition query after a DSH start can return the
declaration until clangd finishes indexing the 505-TU database; a retry returns the definition. Agents should retry
once before concluding a symbol is unresolved. The same applies to the standalone client: asked immediately after `initialize` it sees the declaration and zero
references; with retries the definition appears on the **3rd attempt** (~15 s) and references return 8 in 5 files
(`09-lsp-smoke.txt`). The **authoritative** proof of navigation is the Harness `lsp` tool result above; the smoke
harness proves the same clangd + compile-database configuration works outside the app too.

## What is not covered (do not claim it)

- Sanitized **UI**: the JUCE app is not instrumented and the desktop self-test does not run under ASan/UBSan/TSan.
- **VST3**: the plugin wrappers are JUCE-generated and outside the instrumented scope; the sanitized runs exercise
  the test plugins only through the tests that load them, on the null backend.
- **Audio callback / real device**: no WASAPI device was opened by any sanitized run; the RT-safety claim
  ("no allocation/lock/IO on the audio thread") is **not** proven by this environment, only made measurable.
- **Windows ASan under the app**: ASan MSVC was built with `CORE_BUILD_APP=OFF`, so `Core.exe` is not an ASan target
  here.
- **Linux does not prove Windows runtime** (AGENTS.md), and the reverse.

## Fresh-session verification (done for real)

A new agent with no context (`subagent resume-check`, cwd `D:\CORE\CORE`) was asked to resume from the repository
alone. Result:

- It found the resume point **by convention**, not by being told the path: README.md -> docs/relatorios/README.md ->
  the only 2026-11-19 index row -> this report, whose `## Resume point` section is at lines 11-22. It confirmed
  `ESTADO.md` has **no** entry for this delivery (expected - only the scribe writes it) and that the index row is the
  only pointer.
- It extracted the commands verbatim and **ran diagnostics**: `git rev-parse` -> `8c6837e8` (matches), `git status`,
  `ctest -N -L merge` -> 20 tests, the done markers, the 505-TU compile database, and the ASan canary **inside the
  vcvars shell** (exit 1, `heap-use-after-free ... tests/canary.cpp:10`) - the environment proven live by a fresh
  agent.
- Its criticism (acted on in this revision): the baseline row was incomplete, the WSL logs were not where this report
  said, the ASan suite evidence was a stale pre-fix log, the canary needs the vcvars shell, there was no
  reproduce-from-scratch recipe in the repo, and the whole handover is uncommitted. Sections above and below were
  corrected accordingly; the uncommitted state is called out in Next action.

## Proposed lines for `ESTADO.md` (the scribe writes them, not this report)

- **Pending** - `Agent environment for review/debug/sanitize | docs/relatorios/2026-11-19-review-debug-sanitize-environment.md | three owner decisions: (1) commit the sanitizer wiring fix in CMakeLists.txt (legacy_guard_tests + menu_tests in CORE_SANITIZED_TARGETS) - build configuration only, measured necessary on MSVC and GCC; (2) ratify the @local/dsh-cpp-lsp install in the desktop DSH profile (dependency + bundle + lsp-stdio override pointing clangd at D:/Core-build/core-review/debug); (3) confirm D:\Core-build\core-review as the canonical out-of-tree environment directory for agents.`
- **Measured state** - `Environment (2026-11-19, 8c6837e8) | Debug out-of-tree 553 targets; clangd navigation proven inside the Harness (definition hpp->cpp and cross-TU, 3 references in 2 files); JUCE self-test green with a 1560x734 screenshot; ASan MSVC build 91/91 with canary 3/3 and the merge suite; WSL ASan+UBSan 66/67 TUs with canaries 3/3; WSL TSan canary 5/5; clang-tidy 18.1.3 scope-gated on the WSL database; engine_process_tests fails in Debug (code 259, 2/2) and passes in Release (3/3) - Debug-only shutdown timing, separate task.`

## Next action

0. **Commit the handover.** Everything this delivery wrote is uncommitted (`M CMakeLists.txt`, `M docs/relatorios/README.md`,
   the new report and its evidence directory), and the branch is behind `origin/main`. A fresh clone or a `git clean -fd`
   loses the resume point; the fresh-session check below found it only because it ran in this same working tree.
1. Review the two uncommitted changes (`CMakeLists.txt` sanitizer wiring; the DSH profile files are outside the repo)
   and commit the CMake one with the ledger entry the scribe writes.
2. Re-run the two one-command scripts on a clean tree; they are idempotent and rebuild only what is missing.
3. Open the product task for ENV-03 (Debug-only `engine_process_tests` exit-code race) - not fixed here.
4. If sanitized UI/VST3 coverage is wanted, that needs a separate decision (the JUCE targets are out of the
   instrumented scope by D92).


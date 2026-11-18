# Report — 2026-11-18 — Engine RT fixes: M5 re-arm, A-004 held-note swap, Stop/resetAll bounded watchdog

## What was measured

- Repository `d-aicore/core`, worktree `D:/CORE/CORE/.agents/worktrees/flawless-m5`, branch `flawless-m5`,
  base `f6b86dc` (origin/main at dispatch). Commits delivered: `da91dab` (fix 1), `0e45d58` (fix 2),
  `88aa357` (fix 3). Main checkout `D:/CORE/CORE` never edited; nothing pushed; no `git stash`.
- Scope and authorization: the engine RT fixes stream dispatched by the coordinator — fix 3
  (Stop/resetAll bounded watchdog) with red→green measured in the apply session, then the full gate
  and this report covering all three fixes. The gate was finalized in a follow-up session (the apply
  session ended mid-gate): both modes re-run cleanly, per-suite counts measured, one intermittent
  observed in the aborted run investigated and declared below. Fixes 1 and 2 were committed and green
  before the fix-3 session; their red/green numbers below come from the saved run logs of this stream
  (`D:/Core-build/flawless-m5-evidence/`); their suite counts were re-measured by the final gate.
- Environment: Windows 10.0.26300, MSVC (VS 18 BuildTools), Ninja, Release, vendored JUCE
  `D:/Core-deps/JUCE-9.0.5` (carries the D crash-aware-acquire patch; **not modified by this stream**).
  Build `D:/Core-build/flawless-m5`; evidence `D:/Core-build/flawless-m5-evidence/`.
- Tests executed: `engine_tests.exe` (fixes 1/2, prior session), `engine_process_tests.exe` (fix 3 red,
  green, full suite — prior session), and in the finalizing session: full `ci/gate.ps1` merge and
  `-Desktop` under `GATE-LOCK` (both VERDE), `ctest -L merge -V` (per-suite counts), and 6 standalone
  runs of `engine_process_tests.exe` investigating the one intermittent observed in the aborted prior
  gate (below). The aborted prior gate run (lock `11:19:43Z`) was VERMELHO — investigated, not
  reproduced, declared below; its failing excerpt is preserved verbatim in
  `D:/Core-build/flawless-m5-evidence/prior-gate-merge-failure-excerpt.txt`.
- Limitations: the watchdog bounds the **lock wait** in `reset()`, not plugin code already running inside
  `setActive` (a plugin hung in its own reset code is a different fault, out of scope here). The audio
  thread stays inside a stalled plugin until it returns — the host cannot interrupt plugin code; the
  engine's stall→silence path (M5) and the stop fade cover the sound side. M5 serial-vs-multicore full
  matrix with real plugins under load remains a follow-up on SUR-16 convergence (quality bar 4/5).

## Results

| ID | Problem / result | Evidence (SHA, file:line or run) | Classification | State |
| --- | --- | --- | --- | --- |
| BUG-M5-REARM | `liveFallback_` set on first stall recovery, never cleared: one late worker meant serial playback for the engine's whole life | `da91dab`; `engine.cpp`/`engine.hpp`; red `fix1-red.log` (1 test / 10 checks / **5 failures**), green `fix1-green.log` (1/10/0), suite `fix1-suite.log` (**26 tests / 140 checks / 0**) | proven by test | fixed |
| BUG-A004-SWAP | after a live instance swap (Retry, plugin change) the fresh instance carried phantom held bits but never got the note-ons: a held note went silent until the next note-on or seek | `0e45d58`; `graph.cpp` (`adopt` compares processor identity); red `fix2-red.log` (1 test / 5 checks / **3 failures**), green `fix2-green.log` (1/5/0), suite `fix2-suite.log` (**27/145/0**) | proven by test | fixed |
| BUG-STOP-HANG | **REAL hang**: a stalled plugin (missed deadline, no crash) holds JUCE's process lock inside `processBlock`; `resetAll` waits on it on the message thread and the vendored crash-aware acquire only gives up on `hostCrashed`, which a mere stall never sets → Stop hangs the message thread, the engine answers nothing | `88aa357`; `juce_host.cpp` `resetAll`; red `fix3-red.log` (**3 failures**: `transport.stop` no reply within 5000 ms, `plugins.failed` no reply within 2000 ms, crash report missing), green `fix3-green.log` (1 test / 10 checks / 0), suite `fix3-suite.log` (**11/131/0**) | proven by test (REAL, classified by prior agent, re-confirmed at the whole-block anchor) | fixed |

### Fix 3 anchor re-confirmation (whole block)

`JuceHost::resetAll()` (`engine/src/plugins/juce_host.cpp`): copies the live slots under `impl_->mutex`,
then `runSync` on the JUCE message thread (VST3 wants `setActive` there) loops the slots, skips
`p->failed()` (the failed-plugin skip, kept), and calls `guarded([&]{ p->instance().reset(); })`.
JUCE's vendored `reset()` (`juce_VST3PluginFormatImpl.h:2820`) enters
`enterProcessMutexUnlessCrashed()` (`:3053`): `while (!processMutex.tryEnter()) { if (hostCrashed)
return false; Thread::yield(); }`. A stall inside `processBlock` holds `processMutex` and never sets
`hostCrashed` (stall ≠ crash; the SEH catch that marks crashes never fires), so the acquire spins for
the whole stall. `Server::stopTransport` (`server.cpp:229`) calls `resetAll()` while serving
`transport.stop` — the serve thread is the one that hangs, so every later command goes unanswered.
The stop fade wait is already bounded (200 ms); the hang was solely the reset lock wait. The M5
stall→silence→`liveFallback_` path (`engine.cpp`, `process()` entry) covers the multicore audio side
but does not mark the processor failed, so `resetAll` still tried to reset the stalled instance.

### Fix 3 design (least-invasive bounded wait)

Per plugin, a watchdog thread with a 2 s budget runs beside the reset, which stays on the message
thread exactly as before:

- healthy plugin: the lock is acquired promptly, `reset()` runs on the message thread as always, the
  watchdog sees `done` and exits without touching anything — **healthy reset unchanged**;
- stalled plugin: past the budget the watchdog calls `p->markCrashed()` — atomic stores only
  (`crashed_`, `failure_` CAS, `markCrashedByHost()` → `hostCrashed`), no plugin code, no lock — the
  vendored acquire gives up, `reset()` returns without touching the plugin, and the message thread
  goes on. The track is silenced (the stop fade already ran), `plugins.failed` reports `crash`, Retry
  loads it again; the crashed instance is leaked on purpose by the existing destructor path.

`watchdog.join()` after the `done` store is bounded by one 5 ms poll. The watchdog cannot interrupt
plugin code already inside `setActive` (documented in the comment): that is the plugin's own hang,
not the lock wait this budget bounds. No audio-thread change, no allocation/lock added to the audio
path (quality bar 3); a failing plugin no longer takes Stop/the engine down (quality bar 2).

## Changes

- `engine/src/plugins/juce_host.cpp` — `resetAll` watchdog + `<chrono>` include (fix 3, implementation).
- `app/selftest-plugins/TestPlugins.cpp` — `CORE_TEST_FAULT_HANG`: the Fault effect stalls once inside
  `processBlock` for 8 s without crashing (test-only fault plugin, written by the prior agent, committed here).
- `tests/engine_process_tests.cpp` — new serve test "a plugin stuck in processBlock does not hold Stop
  hostage (bounded reset)" (prior agent's red test, committed here; 10 checks).
- `engine/src/core/engine.cpp`/`engine.hpp` — fix 1 (`da91dab`): first publish ≥2 s after a stall
  recovery re-arms the live workers while fewer than 3 stalls happened; a repeat staller stays serial;
  silence-until-idle guard unchanged. **M5 stays OFF by default (D51)** — `liveThreads` 0 = serial.
- `engine/src/core/graph.cpp` — fix 2 (`0e45d58`): `PluginInstrument::adopt` compares processor
  identity; a different instance gets the held notes re-chased as note-ons.
- `tests/engine_tests.cpp` — fixes 1/2 regression tests (+150 lines, prior session).
- No DSP file touched by any of the three commits (`git diff --stat f6b86dc..88aa357`: 7 files, all
  engine control path, host, test plugins, tests). Vendored JUCE untouched.

Sound: fix 3 silences a stalled plugin at Stop (its track was already faded out by the stop fade);
healthy plugins reset exactly as before — no sound change for healthy songs. Fix 2 makes a held note
keep sounding across a live swap (the intended correction). Fix 1 only affects M5 multicore, off by
default. No null-render re-measurement was done in this stream; the gate's render/determinism tests
ran green (below).

## Verification

Commands (from `D:/Core-build/flawless-m5/tests`, Release, this machine):

- Fix 3 RED @ `0e45d58` + uncommitted test (build `build-fix3-red.log`):
  `engine_process_tests.exe "Stop hostage"` → exit 1, `fix3-red.log`:
  `transport.stop: No reply within 5000 ms`, `plugins.failed: No reply within 2000 ms`,
  **1 test / 10 checks / 3 failures** — the hang, bounded by the client timeouts.
- Fix 3 GREEN @ `88aa357` (build `build-fix3-green.log`): same command → exit 0, `fix3-green.log`:
  **1 test / 10 checks / 0 failures**. Stop answers within the budget (watchdog 2 s + bounded fade
  wait + healthy resets < 4.5 s asserted), `plugins.failed` reports `track:fault` as `crash`,
  `engine.stats` answers, and after the 8 s stall ends the song plays on without the plugin;
  the engine exits code 0.
- Full process suite: `engine_process_tests.exe` → exit 0, `fix3-suite.log`:
  **11 tests / 131 checks / 0 failures** (baseline at the crash-follow-ups delivery: 10/121; +1/+10,
  nothing removed).
- Fixes 1/2 (prior session logs, this stream): red 5 failures → green, `engine_tests` 26/140/0;
  red 3 failures → green, `engine_tests` 27/145/0. Baseline `engine_tests` at `f6b86dc`: 25/130.
- Full gate under `GATE-LOCK`, forward slashes, no tail:
  `powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/flawless-m5 -Jobs 6`
  then with `-Desktop`. The apply session's run (lock `flawless-m5 2026-11-18T11:19:43Z`) ended
  VERMELHO and is investigated below; the finalizing session re-claimed the stale lock
  (`flawless-m5 2026-11-18T11:25:46Z`, no gate process running) and re-ran both modes cleanly.
  Logs: `gate-merge.log`, `gate-desktop.log`, `ctest-merge-verbose.log`, `repro-process-1..6.log`;
  summary `D:/Core-build/flawless-m5/gate-resumo.txt` (all in `D:/Core-build/flawless-m5-evidence/`).

### Full gate — measured (finalizing session, binary @ `88aa357`)

- **Merge: VERDE, 20/20 ctest entries, 0 failures, 37 s** (warm build; the aborted prior run took
  243 s including the build). **Desktop (`-Desktop`): VERDE, 21/21, 0 failures, 58 s**;
  `desktop_self_test` Passed 21.21 s (the app opens, runs its self-test and closes).
- `ctest -L merge -V --timeout 600`: **100% tests passed out of 20**, Total Test time 34.43 s.
- Version: `CORE_BUILD_ID "0.7.0+88aa357026"` == HEAD `88aa357` (short=10), **no `.modified`**
  (`cmake/core_version.cmake` checks `git status --porcelain --untracked-files=no`; the only
  uncommitted file was this untracked report, which cannot dirty the version). `cli_version` Passed.
- Per-suite counts (verbose log) vs the `f6b86dc` baseline (consolidation tip `fcf38e0` + docs,
  measured in `D:/Core-build/integration-evidence/merge-verbose-finaltip2.txt`):

| Suite | Baseline f6b86dc | @ 88aa357 | Delta |
| --- | --- | --- | --- |
| engine_tests | 25 / 130 | **27 / 145** | +2 / +15 (M5 re-arm 1/10, A-004 swap 1/5) |
| engine_process_tests | 10 / 121 | **11 / 131** | +1 / +10 (Stop watchdog) |
| domain_tests | 30 / 525 | 30 / 525 | unchanged |
| legacy_guard_tests | 9 / 169 | 9 / 169 | unchanged |
| role_tests | 21 / 216 | 21 / 216 | unchanged |
| midi_tests | 14 / 208 | 14 / 208 | unchanged |
| bug_tests | 39 / 356 | 39 / 356 | unchanged |
| bitwig_tests | 2 / 25 | 2 / 25 | unchanged |
| audit_tests | 7 / 65 | 7 / 65 | unchanged |
| audit2_tests | 6 / 80 | 6 / 80 | unchanged |
| hunt_tests | 13 / 647 | 13 / 647 | unchanged |
| round3_tests | 7 / 82 | 7 / 82 | unchanged |
| calls_tests | 12 / 69 | 12 / 69 | unchanged |
| menu_tests | 7 / 366 | 7 / 366 | unchanged |
| cli_version / cli_demo_render / cli_measure / cli_unicode / cli_package_stale / cli_plugin_crash_render | 6 entries Passed | 6 entries Passed | unchanged |
| **Total (counted suites)** | **202 / 3059 / 0** | **205 / 3084 / 0** | **+3 / +25, nothing shrunk** |

### Intermittent in the aborted prior gate — investigated, not reproduced, declared

The apply session's merge gate (right after a 6-job build) failed ONE pre-existing test:
"serve: a plugin that crashes in reset is silenced, the engine answers and plays on" —
2 failed checks (`engine_process_tests.cpp:385` `playingOn(engine)` after the replay, and `:387`
engine exit code 1 instead of 0); the new Stop-watchdog test and all other 19 entries passed.
Verbatim excerpt preserved in `prior-gate-merge-failure-excerpt.txt` (the re-run overwrote the
original log file; the excerpt was captured before that).

What the evidence says:

- No "No reply within" line was printed for the failing test: `engine.stats` answered both times,
  so the **audio callbacks did not advance** after the replay; and exit code 1 is the client's
  kill semantics — `engineOptions()` sets `closeTimeout = 10000ms`, "an engine that does not exit
  by itself is terminated then, with exit code 1" (`engine_process_tests.cpp:28`). The engine
  hung at close, it did not throw.
- None of the three fixes changes a code path this test exercises: the reset watchdog is inert
  when `reset()` crashes in milliseconds (`done` stored, watchdog joined within one 5 ms poll);
  fix 1's re-arm is gated by `pool_`/`stalled_` (M5 OFF, null backend, serial — untouched);
  fix 2 fires only on a live instance swap (none here); the HANG fault is env-gated and not set
  in this test. The crash-in-reset path (SEH catch → `markCrashed` → crash-aware acquire gives up)
  is the baseline M1/D behavior, unchanged by this stream.
- **Not reproduced**: 6 standalone runs of the full process suite (`repro-process-1..6.log`,
  11/131/0 each) plus 3 full-suite gate runs (merge ×1, desktop ×1, `ctest -V` ×1) — 9 green
  runs of the same binary after the single failure.

Classification: an intermittent under post-build load, root cause **not established** (a rare
race in the pre-existing crash/lock handling or load starvation of the null-device/message
threads are the candidates; the evidence does not decide). Declared per the flaky rule, not
hidden and not "fixed" by relaxation — no criterion was relaxed and no test removed. Flagged to
the Test Engineer and to integration: re-run `engine_process_tests` under load at acceptance;
if it recurs, capture the engine stderr (`plugins:` trace lines) and a dump (`serve --dumps`)
to pin the hung thread.

Coverage vs `f6b86dc` baseline (consolidation tip `fcf38e0` + docs: **202 cases / 3059 checks**):
+1/+10 (M5 re-arm), +1/+5 (A-004 swap), +1/+10 (Stop watchdog) = **measured 205 cases / 3084 checks /
0 failures** (per-suite table above), all additions, no coverage shrink, no criterion relaxed.

## Pendências and handoff

- To the ledger scribe (owner = Diogo): the proposed ESTADO.md lines below; this delivery does not
  edit `ESTADO.md` (D72/D80).
- To the Test Engineer: independent acceptance of the three fixes on real plugins; the Stop watchdog
  with a real stalled plugin (the fault plugin stalls 8 s deterministically); Retry of a
  watchdog-silenced plugin; Stop with many healthy plugins (per-plugin watchdog threads are created
  sequentially on the message thread — control path only). **Watch the declared intermittent**: the
  pre-existing crash-in-reset serve test failed once in the aborted post-build gate (callbacks stopped
  after the replay; engine hung at close, client kill code 1) and did not reproduce in 9 green runs;
  re-run `engine_process_tests` under load at acceptance and, if it recurs, capture the engine stderr
  (`plugins:` traces) and a `serve --dumps` dump to pin the hung thread. Runtime conditions: Windows
  MSVC Release, null device for the serve tests; WASAPI/device path not exercised by these tests.
- Follow-up (SUR-16 convergence): M5 serial-vs-multicore full matrix with real plugins under load,
  null tests at every sample rate/block size (quality bar 4/5) — not proven by this stream.
- Residual risk: a plugin hung inside its own `setActive` (not the lock wait) still blocks the reset;
  out of scope, declared. A healthy plugin whose reset legitimately takes >2 s is treated as stalled
  (silenced, Retry recovers) — the budget is a policy choice, ratify if the house plugins need more.
- Integration: branch `flawless-m5` not merged, nothing pushed; PM verifies and integrates.

### Proposed ESTADO.md lines (not written here — D72/D80)

Estado medido (proposal):

- «BUG-M5-REARM corrigido (`da91dab`, flawless-m5): o primeiro publish ≥2 s após uma recuperação de
  stall volta a armar os workers live enquanto houve <3 stalls; um plugin que continua a falhar o
  prazo fica serial. Vermelho medido 5 falhas → verde; engine_tests 26/140/0. M5 continua OFF por
  omissão (D51). Matriz serial-vs-multicore completa: pendência na convergência SUR-16.»
- «BUG-A004-SWAP corrigido (`0e45d58`): adopt compara a identidade do processador; uma instância nova
  num swap live recebe as notas presas como note-ons. Vermelho 3 falhas → verde; engine_tests 27/145/0.»
- «BUG-STOP-HANG (REAL) corrigido (`88aa357`): resetAll tem watchdog de 2 s por plugin; um plugin
  parado (stall, não crash) que segura o process lock é marcado crashed (só stores atómicos), o
  acquire vendido desiste e o Stop responde dentro do orçamento; plugins saudáveis resetam como antes.
  Vermelho medido: transport.stop sem resposta em 5000 ms (3 falhas) → verde 1/10/0;
  engine_process 11/131/0. JUCE vendido intocado.»
- «Gate completo flawless-m5 @ 88aa357: VERDE merge 20/20 (37 s) + desktop 21/21 (58 s),
  205 casos / 3084 verificações / 0 falhas (engine_tests 27/145, engine_process 11/131; restantes
  suites iguais à baseline), versão 0.7.0+88aa357026==SHA sem .modified; cobertura 202/3059 (f6b86dc)
  + 3 testes/25 verificações, nada removido. Um intermitente no teste pré-existente crash-in-reset
  foi observado uma vez no gate abortado (pós-build) e não reproduzido em 9 execuções verdes
  subsequentes; causa raiz não estabelecida, declarado no relatório, vigiado na aceitação.»

Pendência (proposal): «Escritão: registar as linhas acima (BUG-M5-REARM, BUG-A004-SWAP, BUG-STOP-HANG,
gate flawless-m5) e abrir o seguimento da matriz M5 serial-vs-multicore na convergência SUR-16.
Owner = Diogo.»
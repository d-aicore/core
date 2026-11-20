# Relatório — 2026-11-20 — S6: engine RT proofs — M5 bit-exact matrix (GREEN) + reset watchdog measured

| | |
|---|---|
| Scope | Owner S-round item 6 (engine RT proofs). Contract verbatim: «Exercita o caminho serial, o M5 e o watchdog de reset com casos dirigidos. Prova que foi realmente usado o caminho paralelo e compara as amostras bit a bit com entradas e plugins de teste determinísticos. Para o watchdog, mede a saída do timeout e a recuperação.» |
| Snapshot | `a9a038430d781959829f8510067ce59afffaad62` (main) + uncommitted round-1/S2/S3 fixes — ALL preserved (re-read shared files before editing; no other agent's file touched) |
| Build | Own build `D:\Core-build\s6\build`: Debug, Ninja, `CORE_BUILD_TESTS=ON`, `CORE_BUILD_APP=ON`, `FETCHCONTENT_SOURCE_DIR_JUCE=D:/Core-deps/JUCE-9.0.5` (D113 full-patch reverse check passed), vcvars64 (VS 18 BuildTools) + `VSLANG=1033` + `chcp 65001`, `--parallel 6`. 566 targets green. |
| Verdict | **GREEN — no defect found.** Serial ≡ M5(2) ≡ M5(4) BIT-EXACT at 4 block sizes with real deterministic VST3 test plugins; parallel usage proven by worker-job counts; watchdog 2 s timeout exit measured with recovery. Merge **25/26** — the one failure is the known pre-existing Debug ENV-03 (exit 259 @ `engine_process_tests.cpp:159`), recorded separately, NOT conflated. |
| Confirmed by | code reading (capture seam, live dispatch, watchdog) + test execution (matrix, timing, merge). Not a UI test; no desktop gate claimed. |

## 1. Capture-seam choice (task 1)

**Existing seam, no engine change for capture:** `Engine::startCapture(frames)` / `stopCapture()` /
`captureL()` / `captureR()` (`engine.hpp:129-133`). The audio thread copies L/R at `engine.cpp:386-398`
AFTER the whole render — `g.process(...)` at `engine.cpp:347` dispatches to `Graph::processLive`
(`graph.cpp:1512-1513`) when the live workers are armed (`live_ && !looping && !log && frames <= 4096 &&
parallel_.size() >= 2`) — and BEFORE `guard_.process` (ear guard). So the capture holds the live path's own
mixed samples, bit for bit. `measure.cpp:170-194` already uses this seam; the serve protocol does not expose
it, which is why the matrix drives the engine in-process.

**Harness:** the serve architecture without the protocol thread — `plugins::Runtime` +
`runWithMessageLoop` + in-process `Server(makeMiniaudioOutput, 48000, &host)` with a `PluginHost` wrapping
the real `JuceHost` (exactly `main.cpp:73-89,583-587`), null audio backend. Real VST3 test plugins
(CoreTestSynth ×3 tracks, CoreTestGain insert on the bass), one instance per track/effect, created on the
message thread. `server.engine()` gives the capture and `livePool()`.

**One minimal test-only seam (justified, unavoidable):** `RtWorkerPool` had NO cumulative counter of jobs
executed by worker threads (only the state machine, `testDelayNs`, `pendingWakes`); `engine.stats`
(`server.cpp:477-478`) exposes `liveThreads/liveFallback/liveStalls/liveStallBlocks/callbacks` but nothing
that observes worker job execution — a configured pool can still play every job on the audio thread.
Added `workerJobs_` (one relaxed `fetch_add` per job in `Worker::loop`, worker threads ONLY; the audio
thread's `take()` is untouched) + `workerJobs()` accessor (`rt_workers.hpp`, +7 lines). Same instrument
kind as `EngineStats`; no RT-path behaviour change, no default change (D51).

## 2. Determinism control (task 2, first)

Song: kick (pitch 36, beats 0/1) + bass (40/43) + lead (64/67) through CoreTestSynth (sine per note, fixed
params), bass through a CoreTestGain insert; 120 BPM, fixed ticks/velocities, no automation/modulators/
randomness; capture = 48000 frames (1 s = 2 beats, every note inside), song 16 s (no loop wrap).

**Serial vs serial repeat (fresh instances each run): bit-identical at every block** (memcmp, sha256 equal,
`firstDiff=-1 count=0 maxDelta=0`). The plugins are deterministic — proven by measurement, not assumed.
Peak 0.6236 (the song really sounds through the gain insert, not silence).

## 3. Bit-exact matrix (task 2) — permanent suite `tests/m5_bitexact_tests.cpp`

All 16 runs share ONE SHA-256 over the raw float bytes (L then R, 48000 frames each):
`0b5bbf9266966d0563e57c5107ff41a7e06f21aeecf4a5eda1ff178b69689f9f`.

| engine block | config | sha256 | workerJobs (workers only) | callbacks | stalls/fallback | vs serial |
|---|---|---|---|---|---|---|
| 128 | serial ×2 (control) | 0b5bbf92… | 0 (no pool) | 377 | 0 / false | identical |
| 128 | M5 liveThreads=2 | 0b5bbf92… | **749** | 380 | 0 / false | **memcmp = 0** |
| 128 | M5 liveThreads=4 | 0b5bbf92… | **752** | 377 | 0 / false | **memcmp = 0** |
| 333 | serial ×2 (control) | 0b5bbf92… | 0 | 146 | 0 / false | identical |
| 333 | M5 liveThreads=2 | 0b5bbf92… | **291** | 147 | 0 / false | **memcmp = 0** |
| 333 | M5 liveThreads=4 | 0b5bbf92… | **292** | 146 | 0 / false | **memcmp = 0** |
| 512 | serial ×2 (control) | 0b5bbf92… | 0 | 95 | 0 / false | identical |
| 512 | M5 liveThreads=2 | 0b5bbf92… | **189** | 95 | 0 / false | **memcmp = 0** |
| 512 | M5 liveThreads=4 | 0b5bbf92… | **188** | 95 | 0 / false | **memcmp = 0** |
| 1024 | serial ×2 (control) | 0b5bbf92… | 0 | 48 | 0 / false | identical |
| 1024 | M5 liveThreads=2 | 0b5bbf92… | **96** | 47 | 0 / false | **memcmp = 0** |
| 1024 | M5 liveThreads=4 | 0b5bbf92… | **96** | 47 | 0 / false | **memcmp = 0** |

- **Parallel-usage proof:** `workerJobs > 0` in every parallel run (jobs executed by the worker threads,
  not the audio thread's `take()`), corroborated by `engine.stats` (`liveThreads` = the opt-in,
  `liveFallback=false`, `liveStalls=0`, `callbacks>0`). Serial runs have no pool (`livePool()==nullptr`,
  workerJobs 0). Second, behavioural proof with REAL plugins: a worker made late on purpose
  (`testDelayNs=50 ms` ≫ the 10.7 ms deadline of 2×256/48000 s) stalls the graph — `liveStalls=1`,
  `liveStallBlocks=9`, then idle + `liveFallback=true` (the re-arm contract). A serial graph can never
  stall, so the stall itself proves the graph ran on the workers.
- **Bonus control (quality bar 5):** the serial path is block-size independent — 128 vs 333/512/1024 all
  bit-identical. The live PLUGIN block is compile-time 128 (`juce_host.cpp:23 kPluginBlock`); the ENGINE
  callback block varied (all ≤ the live cap `kMaxLiveFrames=4096`). The plugin block itself was not varied
  (CSV-09 partial remains open, out of S6 scope).
- **D51 preserved:** M5 default stays 0 (serial); every parallel run opts in by `engine.start
  {liveThreads:n}`; an RAII `LiveEnvGuard` clears `CORE_LIVE_THREADS` for the whole suite (the FL-1
  pattern). No default changed anywhere.
- Suite result: **2 tests / 129 checks / 0 failures** (Debug, ~20 s). Registered `LABELS merge TIMEOUT 600`,
  additive block guarded by `CoreTestSynth_VST3 + CoreTestGain_VST3 + core_plugin_host`; the 5 existing
  plugin registration blocks are untouched.

## 4. Reset watchdog — directed TIMING measurement (task 3)

3 additive cases in `tests/engine_process_tests.cpp` (serve harness, `CORE_TEST_FAULT_VST3` guard), next to
the existing bounded-reset case (which stays as it was). Each prints the measured milliseconds.

| Fault seam | transport.stop measured | mechanism | bounds checked | recovery |
|---|---|---|---|---|
| `CORE_TEST_FAULT_HANG` (8 s stuck in processBlock, holds JUCE process lock) | **2205 ms** | 200 ms bounded fade wait (audio thread stuck) + **2000 ms watchdog budget** + ~5 ms poll → `markCrashed` → vendored acquire gives up | ≥1900 (the timeout REALLY fired) and <4500 (the 8 s stall never held Stop) | `plugins.failed` = track:fault why=crash (0 ms); engine.stats answers; play again → healthy synth plays on after the stall ends; engine exits code 0 |
| `CORE_TEST_FAULT_RESET` + `DELAY_MS=300` (crash inside reset) | **332 ms** | SEH catches the crash below the budget — the CRASH path, not the timeout | ≥250 and <1900 | crashed mark; engine answers; healthy plays on; exit 0 |
| `CORE_TEST_FAULT_RESET` + `DELAY_MS=3000` (plugin code inside reset > budget) | **3008 ms** | watchdog fires at 2 s (`markCrashed`) but CANNOT interrupt plugin code already inside reset — the limit documented at `juce_host.cpp:463-464`, now measured | ≥2900 and <6000 (bounded by the plugin, never a hang) | crashed mark; engine answers; healthy plays on; exit 0 |

The three measurements separate the two mechanisms the old pass/fail case conflated (lock-wait timeout vs
crash catch) and make the documented limit evidence. **3 tests / 32 checks / 0 failures.** No ENV-03 in the
new cases (1 intentionally-leaked instance each ≈ 1.2 s Debug CRT dump < the client's 2 s wait).

## 5. Full merge (task 5) — `ctest -L merge`, own Debug build

**26 registered = 25 before S6 + m5_bitexact_tests. 25/26 passed (120.29 s).** The only failure is
`engine_process_tests` with the KNOWN pre-existing Debug ENV-03: `engine_process_tests.cpp:159`,
«The engine exited (code 259)» — the first crash/retry case (2 leaked instances, CRT leak dump ~2.7–3.3 s >
the client's hardcoded 2 s wait; cause proven in [2026-11-20-env03-debug-259.md](2026-11-20-env03-debug-259.md),
fix awaits owner authorization). **Excluding ENV-03: 25/25.** My 3 new timing cases pass inside the same
engine_process_tests run (18 cases / 288 checks / 1 failure = the ENV-03 one). Nothing removed or weakened.

Per suite (tests / checks / failures):

| suite | counts | suite | counts |
|---|---|---|---|
| engine_tests | 39 / 221 / 0 | hunt_tests | 11 / 106 / 0 |
| wav_float_tests | 16 / 360 / 0 | round3_tests | 7 / 82 / 0 |
| domain_tests | 33 / 568 / 0 | calls_tests | 12 / 69 / 0 |
| modules_tests | 6 / 128 / 0 | menu_tests | 7 / 301 / 0 |
| role_tests | 21 / 215 / 0 | window_tests | 10 / 70 / 0 |
| midi_tests | 20 / 402 / 0 | **m5_bitexact_tests (new)** | **2 / 129 / 0** |
| bug_tests | 48 / 462 / 0 | state_recovery_tests | 7 / 68 / 0 |
| bitwig_tests | 2 / 25 / 0 | engine_process_tests | 18 / 288 / **1 (ENV-03)** |
| audit_tests | 7 / 65 / 0 | cli_version / cli_demo_render / cli_measure / cli_unicode | 4 passed |
| audit2_tests | 15 / 364 / 0 | cli_package_stale / cli_plugin_crash_render / installer_samples | 3 passed |
| export_plugin_failure_tests | 5 / 54 / 0 | | |

Totals across the 19 exe suites: **286 cases / 3977 checks / 1 failure (ENV-03, pre-existing)** + 7 script
tests passed. Growth vs the S2/S3 merge: +1 suite (m5_bitexact 2/129), engine_process_tests 15→18 cases /
256→288 checks (the 3 timing cases).

## 6. Files changed (write scope respected)

| file | change |
|---|---|
| `engine/src/core/rt_workers.hpp` | +7: test-only worker-job counter (relaxed add per job in `Worker::loop` only) + accessor. Justified §1; no RT-path behaviour change |
| `tests/m5_bitexact_tests.cpp` | NEW permanent suite (matrix + determinism control + parallel proof + late-worker case) |
| `tests/CMakeLists.txt` | +22 additive registration block (m5_bitexact_tests, merge, TIMEOUT 600); all 5 existing plugin blocks preserved |
| `tests/engine_process_tests.cpp` | +115: 3 additive watchdog timing cases inside the existing Fault guard |
| `docs/relatorios/…` | this report + index line + evidence (delivery per AGENTS.md) |

NO engine processing change, NO default change (D51), NO other agent's file touched. Nothing committed
(no verbatim authorization).

## 7. Limitations

- Debug, this machine, null backend, deterministic TEST plugins (sine/gain) — not the owner's real plugin
  library. D51's ratification gate for M5 default ON remains the owner's: soak 8 h + bit-a-bit vs serial on
  the owner's PC with `CORE_LIVE_THREADS=4`. This matrix is the directed proof that gate asks for, not the
  soak itself; M5 stays OFF by default.
- The live plugin block is compile-time 128; only the ENGINE callback block varied (CSV-09 partial open).
- Capture is pre-ear-guard (the song as measured); the guard is identical on both paths and out of the
  comparison.
- The watchdog cannot interrupt plugin code already inside reset — measured (3008 ms case), documented,
  not a defect; a plugin that hangs INSIDE reset for >2 s still holds Stop for its own duration.
- No UI execution; not a desktop gate. ENV-03 (Debug teardown timing) is a separate open task (3A).

## 8. Proposed ESTADO.md lines (the escrivão writes them; D72/D80) and pendências

- *Estado medido — S6 (2026-11-20, a9a0384 + round-1/S2/S3 locals, own Debug build)*: M5 live path
  BIT-EXACT with serial (memcmp + SHA-256 `0b5bbf92…689f9f`) with deterministic VST3 test plugins at
  blocks 128/333/512/1024, liveThreads 2/4; parallel usage proven by worker-job counts (749/752, 291/292,
  189/188, 96/96) + a real-plugin late-worker stall; serial determinism and block-independence controls
  identical. Reset watchdog measured: Stop 2205 ms on the 2 s timeout path (8 s hang seam), 332 ms on the
  crash-in-reset path, 3008 ms when plugin code inside reset exceeds the budget (documented limit);
  crashed mark + engine answers + healthy plays on + exit 0 in all three. `m5_bitexact_tests` 2/129/0
  permanent (merge); engine_process_tests +3 timing cases (18/288, the 1 failure = ENV-03 pre-existing).
  Merge 25/26 (25/25 excluding ENV-03). Quality bar 4 proven by measurement on this machine; D51 default
  OFF unchanged.
- *Pendências*: commit S6 (owner); ENV-03 fix authorization (3A, separate — do not conflate); CSV-07/SUR-16
  bit-exact matrix gap CLOSED by reference (this suite); M5 default-ON ratification still requires the
  owner's soak 8 h + bit-a-bit on his PC (D51 verbatim); CSV-09 live plugin block fixed 128 remains open.

## Evidence

[evidencia/2026-11-20-s6-engine-rt-proofs/](evidencia/2026-11-20-s6-engine-rt-proofs/):
`m5-run1.log` (full matrix output), `watchdog-run1.log` (3 timing cases), `merge-run1.log` (ctest 26),
`per-suite-counts.txt`, `s6-engine-and-process-tests.patch` (rt_workers.hpp + engine_process_tests.cpp
diff; the CMake block is additive and quoted in §3/§6), `s6-notes.md` (step log). Build: `D:\Core-build\s6\`.

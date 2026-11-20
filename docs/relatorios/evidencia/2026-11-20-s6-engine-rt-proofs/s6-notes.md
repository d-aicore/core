# S6 — engine RT proofs (owner round-2 item 6)
Repo D:\CORE\CORE @ a9a038430d781959829f8510067ce59afffaad62 + uncommitted round-1/S2/S3 fixes (all preserved).
Build: D:\Core-build\s6\build (Debug, Ninja, TESTS=ON, APP=ON, JUCE source dir D:/Core-deps/JUCE-9.0.5).

## Step 1 — recon (2026-11-20 ~02:07 UTC)
- git HEAD verified a9a0384; uncommitted: CMakeLists.txt, app/desktop (EngineSession/MainComponent), installer Setup.cpp,
  engine graph/render_job/server/audio_file/juce_host, tests/CMakeLists.txt + new suites (S1/S2S3/S4/S5) — preserved, re-read before edit.
- engine.hpp: Engine(sampleRate, liveThreads=0) default serial (D51). RtWorkerPool in graph.hpp. stats: liveStalls/liveStallBlocks.
  EXISTING CAPTURE SEAM: Engine::startCapture(frames)/stopCapture()/captureL()/captureR() — records device output into memory.
  Candidate for bit-exact matrix; must verify it sits AFTER the live-worker mix.
- tests/CMakeLists.txt has 5 registration blocks for test plugins (Crash, Fault, Throw, CrashRestart, S2S3) — additive only.

## Step 1b — recon complete; capture seam VERIFIED (no new capture seam needed)
- engine.cpp:347 g.process(..., scratchPeaks_, deadlineNs) -> graph.cpp:1512 dispatches to processLive when
  live_ && !looping && !log && frames<=4096 (kMaxLiveFrames) && parallel_.size()>=2 (non-bus, non-keyed tracks).
- engine.cpp:386-398: capture copy of L/R happens AFTER the whole render (live-worker mix included), after the stop
  fade, BEFORE guard_.process (ear guard). So Engine::startCapture/captureL/captureR is the bit-exact live-path seam.
  measure.cpp:170-194 already uses it (liveCase). No engine change needed for capture.
- Harness choice: in-process Server(makeMiniaudioOutput, 48000, &host) + null backend + core::plugins::Runtime +
  runWithMessageLoop + JuceHost (same architecture as core-engine serve, main.cpp:583-587), wrapped in a PluginHost
  like EnginePlugins (main.cpp:73-89). Real VST3 test plugins via scanFile() in-process. server.engine() gives capture
  + livePool(). No protocol change, no subprocess needed for the matrix.
- Parallel-usage proof gap: RtWorkerPool (rt_workers.hpp) has NO cumulative worker-job counter (only state machine,
  testDelayNs, pendingWakes); engine.stats (server.cpp:477-478) exposes liveThreads/liveFallback/liveStalls/liveStallBlocks
  + callbacks but nothing counting worker job execution. PLAN: minimal test-only seam in rt_workers.hpp — one relaxed
  atomic counter incremented per job in Worker::loop (worker threads only; audio-thread take() untouched) + accessor.
  Justification: direct proof 'worker activity > 0' is otherwise unobservable; same instrument kind as EngineStats.
- Plugins deterministic by construction: TestSynth = per-note sine, double phase/level ramps, voice alloc deterministic;
  TestGain = applyGain. Determinism control (serial vs serial repeat, fresh instances) will prove it by measurement.
- Live plugin block is compile-time 128 (juce_host.cpp:23 kPluginBlock); engine callback block varies (device.cpp:149
  blockSize arg, default 128). Matrix blocks: 128, 333, 512, 1024 (all <= 4096 live cap).
- Watchdog: juce_host.cpp:445-484 resetAll, per-plugin 2 s budget / 5 ms poll -> markCrashed (atomic) -> vendored
  crash-aware acquire gives up. Cannot interrupt plugin code already inside reset (documented). stopTransport
  (server.cpp:270-286): fade wait bounded 200 ms, then resetAll, then rebuild(keepTails=false).
  Timing cases to add (additive, engine_process_tests.cpp, CORE_TEST_FAULT_VST3 guard):
  (a) HANG seam: stopMs must be >= ~1.9 s (watchdog timeout really fired) AND < 4.5 s (bounded, << 8 s stall);
      crashed mark + engine answers + healthy synth plays on (recovery).
  (b) RESET seam + DELAY_MS=300: stopMs ~ 0.3 s (crash path, below budget), crashed mark.
  (c) RESET seam + DELAY_MS=3000: watchdog fires at 2 s but reset waits plugin code -> stopMs ~ 3 s (characterizes
      the documented limit: budget bounds the LOCK WAIT, not plugin code inside reset).
- Toolchain: VS 18 BuildTools vcvars64 at C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build.
- ENV-03 (engine_process_tests Debug 259): lead says PROVEN pre-existing (CRT leak dump ~3 s > client 2 s wait);
  record separately if hit, do not conflate.

## Step 2 — build (2026-11-20 ~02:19 UTC)
- Configure + build D:\Core-build\s6\build: Debug, Ninja, CORE_BUILD_TESTS=ON, CORE_BUILD_APP=ON,
  FETCHCONTENT_SOURCE_DIR_JUCE=D:/Core-deps/JUCE-9.0.5 (D113 full-patch reverse check passed), vcvars64 (VS 18 BuildTools),
  VSLANG=1033, chcp 65001, --parallel 6. 566 targets; all test exes + 6 test VST3 plugins + CoreApp + Core Setup.exe built.
- Next: write tests/m5_bitexact_tests.cpp (in-process Server+JuceHost+null device, Engine capture seam),
  minimal worker-job counter seam in rt_workers.hpp, additive registration; watchdog timing cases in engine_process_tests.cpp.

## Step 3 — code written (2026-11-20 ~02:25 UTC)
- engine/src/core/rt_workers.hpp: MINIMAL test-only seam — workerJobs_ atomic (relaxed) incremented per job in
  Worker::loop ONLY (worker threads; audio-thread take() untouched) + workerJobs() accessor. Justification: no
  existing counter observes worker job execution (engine.stats has only stalls); direct 'worker activity > 0'
  proof otherwise unobservable. No RT-path behaviour change (one relaxed add per job, same kind as EngineStats).
- tests/m5_bitexact_tests.cpp (NEW, permanent): in-process Server+JuceHost+null device (serve architecture,
  Runtime+runWithMessageLoop), existing Engine capture seam. TEST 1: determinism control (serial x2 bit-identical)
  then serial(0) vs M5(2) vs M5(4) at blocks 128/333/512/1024, memcmp + SHA-256 of raw float bytes, workerJobs>0
  proof per parallel run, liveFallback=false/liveStalls=0 checks, serial block-independence bonus control.
  TEST 2: real-plugin late-worker stall proof (testDelayNs=50ms >> 10.7ms deadline -> liveStalls>=1 -> fallback).
  RAII LiveEnvGuard clears CORE_LIVE_THREADS (D51 default untouched; opt-in by argument only).
- tests/CMakeLists.txt: ADDITIVE registration block (m5_bitexact_tests, LABELS merge, TIMEOUT 600, guarded by
  CoreTestSynth_VST3 + CoreTestGain_VST3 + core_plugin_host). All 5 existing plugin registration blocks preserved.
- tests/engine_process_tests.cpp: 3 ADDITIVE timing cases inside the existing CORE_TEST_FAULT_VST3 guard:
  (a) HANG: stopMs in [1900,4500) — the 2 s timeout really fired and bounded Stop (vs 8 s stall); crashed mark;
      engine answers; healthy synth plays on (recovery).
  (b) RESET+300ms: stopMs in [250,1900) — crash path below budget (separates the two mechanisms).
  (c) RESET+3000ms: stopMs in [2900,6000) — watchdog cannot interrupt plugin code inside reset (documented limit,
      now measured). Each prints measured ms to stderr.
- ENV-03 note: new cases leak 1 crashed instance each (~1.2 s Debug CRT dump < 2 s client wait) — expected code 0;
  if 259 appears it is the ENV-03 pattern, recorded separately.

## Step 4 — BIT-EXACT MATRIX: GREEN (2026-11-20 ~02:30 UTC), m5-run1.log
- 2 tests / 129 checks / 0 failures. ALL 16 runs (4 blocks x {serial x2, M5-2, M5-4}) share ONE sha256:
  0b5bbf9266966d0563e57c5107ff41a7e06f21aeecf4a5eda1ff178b69689f9f (48000 frames L+R raw float bytes).
- Determinism control: serial vs serial repeat bitExact=1 at every block (plugins deterministic, proven).
- serial(0) vs M5(2) vs M5(4): memcmp identical, firstDiff=-1, count=0, maxDelta=0 at blocks 128/333/512/1024.
- Parallel-usage proof (workerJobs delta, workers only): 128: 749/752; 333: 291/292; 512: 189/188; 1024: 96/96
  (threads=2/4). stalls=0, fallback=0 everywhere; callbacks 377/146/95/48. Serial workerJobs=0 (no pool).
- Serial block-independence bonus: 128 vs 333/512/1024 all bitExact=1 (plugin block compile-time 128).
- Late-worker proof with REAL plugins: workerJobs=76 after 200 ms; testDelayNs=50 ms -> liveStalls=1,
  liveStallBlocks=9, then idle + liveFallback=true (re-arm contract). A serial graph can never stall.
- NO DEFECT FOUND: serial == M5 bit for bit (quality bar 4 proven with real VST3 plugins, Debug, this machine).

## Step 5 — WATCHDOG TIMING: GREEN (watchdog-run1.log), 3 tests / 32 checks / 0 failures
- HANG seam (stuck in processBlock, 8 s): transport.stop = 2205 ms = 200 ms bounded fade wait (audio thread
  stuck) + 2000 ms watchdog budget + ~5 ms poll. In [1900,4500): the TIMEOUT path really fired and bounded Stop.
  plugins.failed = track:fault why=crash (0 ms answer); engine.stats answers; recovery: healthy synth plays on
  after the stall ends; engine exits code 0.
- RESET seam + 300 ms delay: stop = 332 ms (< 1900): the CRASH path (SEH catch + markCrashed), not the timeout.
- RESET seam + 3000 ms delay: stop = 3008 ms (>= 2900, < 6000): watchdog fires at 2 s but cannot interrupt
  plugin code already inside reset (juce_host.cpp documented limit) — now MEASURED, bounded, crash caught,
  engine answers and plays on, exits code 0.
- No ENV-03 in the new cases (1 leaked instance each ~1.2 s Debug CRT dump < 2 s client wait), as predicted.

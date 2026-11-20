# S7 — engine RT measurement (owner S-round item 6, second part; FINAL round-1 item)

Agent: s7-rt. Repo: D:\CORE\CORE @ a9a038430d781959829f8510067ce59afffaad62 (main) + uncommitted round-1/S2/S3/S6 locals — ALL preserved.
Contract: «Verifica alocações, locks e I/O no callback com medição explícita e controlos negativos que façam a prova falhar. Reporta o que ficou fora da instrumentação. Depois mede um projeto de referência em Release, com dispositivo real, a 64 e 128 samples quando suportado. Regista dropouts, tempo máximo e percentil 99 por bloco, interface, driver, sample rate e carga.»

## Step 1 — context (2026-11-20 ~02:48)
- git: HEAD a9a0384, branch main, uncommitted round-1 files present (CMakeLists, EngineSession, graph, render_job, rt_workers, server, juce_host, audio_file, tests…). S6 report read: Debug M5 bit-exact matrix GREEN, merge 25/26 (ENV-03 pre-existing). S6 seam: rt_workers workerJobs counter. S6 harness template: tests/m5_bitexact_tests.cpp (Runtime + runWithMessageLoop + Server(makeMiniaudioOutput) + JuceHost wrapper, null backend, capture seam).
- Plan: Phase A (map → seam → probe build → negative controls → null-backend measurement serial+M5(4)) then Phase B (Release real-device baseline, quiet machine).
- Own dirs: probe D:\Core-build\s7\probe, build D:\Core-build\s7\build. Evidence: docs/relatorios/evidencia/2026-11-20-s7-rt-measurement/.

## Step 2 — callback path map + audit (2026-11-20 ~03:00)
Chain: miniaudio → MiniaudioOutput::callback (device.cpp:79-106) → Engine::process (engine.cpp:192-408) →
Graph::process (graph.cpp:1506-1529; dispatch to processLive at 1512-1513 when live_ && !looping && !log &&
frames<=4096 && parallel>=2) → runTrack (1173-1374) → renderTrack (969-1066) → PluginInstrument::renderStereo
(graph.cpp:169-189) / PluginEffect::process (242-255) → JuceProcessor::process (juce_host.cpp:142-159) → runBlock
(196-222) → JUCE VST3 processBlock (juce_VST3PluginFormatImpl.h:2429-2443, vendored crash-aware SpinLock acquire
3088-3126) → plugin DLL (OUT OF SCOPE). Post: loudness (loudness.cpp:57-69), analyzer (analyzer.cpp:45-63),
ear guard (output_guard.cpp:31-72), capture copy (engine.cpp:386-398), recordCallback (device.cpp:105 → engine.cpp:152-171).

AUDIT — allocations reachable (all preallocated/fixed by design; probe must confirm):
- Engine::process: fixed arrays offs_[32]/scratchPeaks_/swapLog_[512]/overflowTracks_ (engine.hpp:178-223), SpscQueue fixed;
  graph delete only on control thread (publish/collectGarbage engine.cpp:56-78).
- processLive: steps_ reserved (graph.cpp:1433,1449), keptL/R preassigned (1427-1432), stack bufs (1459).
- renderTrack: stack Pending[256]/start[256][8] (985-991). runTrack: stack target[kMaxParams] (1184), dry bufs (1126).
- PluginInstrument: fixed notes_[1024]/pendingOffs_[128] (graph.cpp:226-232).
- JuceProcessor: fixed in/out arrays[512] (juce_host.cpp:245), buffer_ kMaxBlock (91), midi_.ensureSize(kMaxNotes*16) (92);
  AudioBuffer view wraps pointers (197, no alloc); MidiMessage 3-byte ctors INLINE packedData (juce_MidiMessage.cpp:173-182;
  allocateSpace 349-359 mallocs only > sizeof(packedData) = sysex); MidiBuffer::addEvent reallocs only past capacity
  (juce_MidiBuffer.cpp:139-165,123) — note-ons capped kMaxNotes (juce_host.cpp:188), note-offs always added (186-187 comment
  admits JUCE may grow: probe watches). midi_.clear = clearQuick (juce_MidiBuffer.cpp:122).
- ERROR paths only: runPlugin catch (graph.cpp:96-101, the throw is the plugin's), processLive worker-failure throw
  std::runtime_error (graph.cpp:1458 — allocates, error path), device catch(...) (device.cpp:97-100). noteFailed fixed slots
  + snprintf (graph.cpp:34-43, no alloc).
- applyPluginAutomation → setParameter (graph.cpp:776 → juce_host.cpp:173-176) → VST3Parameter::setValue
  (juce_VST3PluginFormatImpl.h:2040-2044) → CachedParamValues::set atomic FlaggedFloatCache (juce_VST3Common.h:1597-1600) +
  EditControllerParameterDispatcher::push (impl.h:133-142): MessageManager::getInstance plain static ptr
  (juce_MessageManager.cpp:72-82), isThisTheMessageThread atomic (244-247), cache.set atomic; editor flush 60 Hz timer.
  NOT reached by the reference song (no automation).

AUDIT — locks reachable:
- JUCE VST3 processMutex = juce::SpinLock (impl.h:3063): PURE ATOMIC CAS (juce_SpinLock.h:71-81; enter juce_Thread.cpp:432-443:
  20 spins + Thread::yield). NOT EnterCriticalSection → not IAT-hookable (intrinsics); audited. Uncontended = 1 CAS;
  contended = spin + SwitchToThread (probe hooks SwitchToThread to catch audio-thread yields).
- JuceHost::Impl::mutex std::mutex (juce_host.cpp:285): ONLY control-thread methods (get 387/395/418, keepOnly 427,
  releaseAll 441, resetAll 448, captureStates 490, showEditor 513, setParameter 564, parameters 589, vstPreset 620).
  Never from the callback (graph holds shared_ptr from build time). MSVC std::mutex = SRWLOCK → probe hooks SRWLOCK APIs too.
- RtWorkerPool: NO locks; audio thread releases worker semaphores (rt_workers.hpp:131-133, WakeByAddressSingle),
  bounded _mm_pause spin to deadline (90-93); workers sleep on semaphores BETWEEN jobs only.

AUDIT — IO reachable: NONE on the engine callback path (no file API in engine/graph render paths). trace() fprintf
(juce_host.cpp:56-59) only load/release/crash paths; markCrashed does NOT trace (129-137, atomic only). Plugin DLL
internals (own CRT/imports), JUCE SpinLock atomics, OS/WASAPI internals = OUT OF INSTRUMENTATION (audited boundary).

SEAM DESIGN (one, test-only, device.hpp+cpp): std::atomic<RtProbe*> rtProbe, null by default (production = ONE relaxed
load/callback). RtProbe { atomic<bool>* inCallback; atomic<uint32_t>* audioThreadId; void(*inject)(); void(*block)(renderNs,
budgetNs,gapNs,frames) }. Flag covers the WHOLE callback (set at entry, clear after recordCallback/observer/inject);
thread id for attribution (control-thread noise never miscounted); inject = directed negative controls INSIDE the window
(probe-injected, allowed by task — a plugin-DLL fault would NOT be seen: own CRT, out of scope); block observer = exact
per-block timing for max/p99 (existing loadBands = 1%-of-budget bands = 13.3 us at 64 samples, too coarse; observer rides
the SAME seam, engine.cpp untouched). Worker coverage: processLive run() waits all workers before return (non-stalled) →
worker jobs execute entirely inside the flag window; late-worker tails after a stall = declared gap.

PROBE DESIGN: tests/s7_rt_probe_tests.cpp (permanent suite = detector regression) + --baseline/--devices modes for Phase B.
(a) global operator new/delete overrides (record size, forward to CRT malloc/free; TLS dedup vs heap hook);
(b) IAT hooks in the exe (kernel32/kernelbase/api-ms-win-core-*): CreateFileW/A, ReadFile, WriteFile, FlushFileBuffers,
    Enter/TryEnter/Leave/Delete/InitializeCS(+Ex), Acquire/TryAcquire/ReleaseSRWLockExclusive+Shared, WaitForSingleObject(Ex),
    Sleep(Ex), SwitchToThread, HeapAlloc/HeapReAlloc/HeapFree. Heap hooks = the malloc/free coverage: static UCRT malloc
    funnels through kernel32 HeapAlloc (IAT) — safer than overriding the CRT malloc family (heap-mismatch risk), strictly
    broader (catches JUCE/miniaudio direct heap use). Verify imports with dumpbin; negative controls prove wiring.
(c) recorder: fixed 8192-slot ring, atomic index, CaptureStackBackTrace(2,12), NO alloc/lock/IO in the hook path
    (TLS recursion guard); records on ANY thread while flag set; classify at dump: AUDIO (seam tid) / CONTROL (message+body
    tids) / OTHER (worker candidates). dbghelp symbolization at dump only.
Timing ring: 262144 BlockRec (8 MB BSS), single producer; exact max/p50/p95/p99 per scenario; cross-check vs stats.maxRenderNs.
Negative controls: injectAlloc (malloc 96 + new char[128]), injectIO (CreateFileW NUL + ReadFile + CloseHandle),
injectLock (Enter+LeaveCriticalSection on a probe-owned CS) — each MUST fire on the AUDIO thread.
Phase B: same exe --baseline <outDir> [device] [seconds]: loads 3/6/12/24 instances (3 synth tracks; 3×(synth+gain);
6×2; 12×2), 60 s each, 48 kHz, buffers 64/128 (record actual), serial (D51 default) + one M5(4) comparison at top load;
warmup 2 s, resetStats+rings, NO control calls inside the window (avoid control noise), engine.cpu before/after;
listenDb -15 (ear guard stays ON). If 24 clean → extend 48/96 to find breaking point.
Toolchain: VS 18 BuildTools vcvars64, VSLANG=1033, chcp 65001, Ninja, --parallel 6, JUCE D:/Core-deps/JUCE-9.0.5 (D113 patched).

## Step 3 — seam implemented (2026-11-20 ~03:05)
- engine/src/io/device.hpp: RtProbe struct + extern std::atomic<RtProbe*> rtProbe (justification comment: attribution,
  negative controls, exact p99 — one seam, engine.cpp untouched). device.cpp: definition (null) + callback arm
  (entry: flag+tid; exit: block observer → inject → flag clear). Production = one relaxed load/callback.
- tests/s7_rt_probe_tests.cpp (NEW, permanent): recorder ring 8192 (no alloc/lock/IO in hook path, TLS guards,
  CaptureStackBackTrace), operator new/delete full override set (dedup vs heap hooks), 25 IAT hooks
  (kernel32/kernelbase/api-ms-win-core-*), timing ring 262144, harness = m5 architecture, 6 TESTs
  (wiring + 3 negative controls + serial 64/128 + M5(4)), --devices and --baseline modes for Phase B.
- tests/CMakeLists.txt: ADDITIVE s7 block (merge, TIMEOUT 600, same guards as m5). All existing blocks preserved.
- Field names verified against server.cpp:464-479: stats uses "starvations" (=lateCallbacks), "maxRenderMs" (ms),
  NO "frames" field → delivered seconds computed from the seam ring's per-block frames (exact).

## Step 4 — build (2026-11-20 ~03:15)
- Configure D:\Core-build\s7\build: Release, Ninja, TESTS=ON, APP=ON, JUCE D:/Core-deps/JUCE-9.0.5 — D113 full-patch
  reverse check PASSED. Build run 1: 544/572 then FAILED — my probe bug: CHECK_EQ with 1 arg (line 795). Fixed
  (liveStalls == 0). Build run 2: 35/35 GREEN, s7_rt_probe_tests.exe linked. Logs: probe/build-run1.log, build-run2.log.
- Next: dumpbin IAT verification + Phase A suite run.

## Step 5 — Phase A runs + RED FINDING (2026-11-20 ~03:35)
- dumpbin: exe imports HeapAlloc/HeapFree/HeapReAlloc/CreateFileW/ReadFile/WriteFile/Enter/TryEnter/LeaveCS/
  WaitForSingleObject(Ex)/Sleep/SwitchToThread from KERNEL32 + AcquireSRWLockExclusive from api-ms-win-core-synch — all hookable.
- run1: ring was fill-once (not circular) + classification skipped when from>cap → all counts unclassified; fixed circular.
  Also Release had NO PDB → stacks raw; added /DEBUG to the probe target ONLY (test target, no code change).
- run2/run3: NEGATIVE CONTROLS ALL FIRE (alloc: HeapAlloc 96 + new 128 on AUDIO; IO: CreateFileW+ReadFile on AUDIO;
  lock: Enter+LeaveCS on AUDIO). Clean path: THOUSANDS of AUDIO-thread records → characterized with symbols:
  **EnterCriticalSection/LeaveCriticalSection from juce::MidiEventList's Array<Vst::Event, CriticalSection>
  (juce_VST3Common.h:1289)** via processAudio→associateWith/hostToPluginEventList/toMidiBuffer (impl.h:2538,2558,3431-3459)
  + plugin-DLL→host IEventList vtable callbacks. ~1.3 CS pairs per plugin processBlock; M5 workers too (2889 pairs/2s).
  ZERO alloc/IO/SRWLOCK/Sleep on audio+workers: every HeapAlloc/new in window = JUCE TimerThread (EditControllerParameterDispatcher
  60Hz timer started at setActive, impl.h:2250) or control-thread json — classified by tid+stacks.
- cpu read bug FIXED: transport.stop rebuilds the graph (fresh zeroed CpuMeter) → stats+cpu now read BEFORE stop
  (intervalMs≈2000, totalPct 1.6-2.4% — plugins really worked). Seam max == stats.maxRenderMs exact (cross-validation).
- Ring enlarged 8192→65536 (16MB BSS): whole 4s suite window classifies; 60s baseline keeps steady-state tail (totals exact).
- **RED reported to lead (message sent ~03:35): locks on audio thread+workers; NO fix without authorization.**
  Permanent suite stays honest-red on the zero-violation assertions (wiring+negctrls green) — like ENV-03, documented.
- Null-backend starvations ~187/2s = miniaudio null pacing artifact (sleep granularity vs 2-period buffer), NOT engine
  dropouts; real WASAPI in Phase B is the authoritative dropout measurement. Declare in report.
- Next: final Phase A numbers (run5), devices.list recon, Phase B after lead quiet-machine ok.

## Step 6 — lead directives (2026-11-20 ~03:40) + devices recon
- Lead: machine QUIET, GO Phase B; record lock counts per scenario; volume safe.
- Restructure per ENV-03 precedent: merge keeps wiring + 3 negctrls + CHARACTERIZATION (detector asserts + measured
  lock counts as DATA); strict zero-lock assertion → separate case, label rt_strict (NOT merge), owner-decision gate.
  Verify ctest -L merge green after restructure (run it). Cite AGENTS.md «do not relax criteria or weaken the remaining
  tests to obtain green» + ENV-03 exclusion pattern: declared gap, not relaxed criterion.
- Precision (lead): EnterCriticalSection UNCONTENDED fast path = user-mode interlocked; kernel transition only on
  contention. Criterion 3 not met AS WRITTEN (lock API calls on RT path, ~1.3 CS pairs/processBlock, JUCE MidiEventList
  Array<Vst::Event,CriticalSection> in statically-linked hosting layer); priority-inversion risk if another thread ever
  contends the list. Do NOT overstate «kernel transitions per call».
- Fix directions = PROPOSALS only (D113 precedent: vendored JUCE changes need explicit owner authorization). Not implemented.
- run5 (ring 65536): negctrls green; clean-path zero assertions red as expected (6 falhas = the 2 engine-path tests'
  audio/other zero checks + cpu… cpu fixed). Devices present: **Auscultadores (USB-C Audio) [default]** — the owner's
  17/11 interface — + Altifalantes (Realtek), Q27G3XMN/KB272 G0 (NVIDIA HD Audio), Realtek Digital Output.
- Phase B target: Auscultadores (USB-C Audio), 48 kHz, 64/128 (shared→exclusive fallback recorded), 60 s/scenario.

## Step 7 — restructure per lead + FULL-WINDOW numbers + merge GREEN (2026-11-20 ~03:50)
- Restructure done (ENV-03 precedent): merge = wiring + 3 negctrls + characterization (detector sanity asserts +
  counts as DATA); strict zero gate = s7StrictGate, registered ONLY under CORE_S7_STRICT=1 (ctest s7_rt_strict_tests,
  label rt_strict, ENVIRONMENT set) → default exe run is exactly the merge set; no silent pass, nothing relaxed.
- FULL-WINDOW classification (ring 65536, lost=0) CORRECTS the earlier tail-sample estimate:
  serial-64: 22674 audio lock calls / 1502 callbacks = 15.06/callback (= 7.53 per plugin block, 2 callbacks/block);
  serial-128: 22644 / 753 = 30.11/callback = 7.53/plugin block (~3.76 CS enter/leave PAIRS per processBlock);
  M5(4): audio 6296 (8.38/callback) + workers/other 16602 records, workerJobs=1496, stalls=0.
  **Rare AUDIO allocations FOUND (were hidden by ring wrap): 3 (serial-64), 3 (serial-128), 1 (M5) per 2 s —
  HeapAlloc 384 = juce::MidiEventList::addEvent → Array<Event>::add → realloc GROWTH (same JUCE layer).**
  File IO on audio: 0. SRWLOCK/waits/Sleep on audio: 0. All other allocs = JUCE TimerThread (MessageBase::post) / control json.
- Strict gate asserts 0/0/0 (locks/allocs/IO) → measured RED: 4 falhas (serial+M5 × locks+allocs). Owner-decision gate.
- Merge set exe: 6 tests / 77 checks / 0 falhas. **ctest -L merge (Release, own build): 27/27 PASSED, 89.21 s**
  (merge-run1.log) — incl. s7_rt_probe_tests 19.66 s, m5_bitexact 17.42 s, engine_process_tests 32.57 s (Release: no ENV-03).
- Correction sent to lead (rates + rare allocs + merge green). Phase B STARTING (lead confirmed QUIET).

## Step 8 — Phase B run1 (background died) + resume in foreground batches (2026-11-20 ~04:00)
- run1 (--baseline full ramp, job pwsh-238faf59bd2947e1) DIED when the turn ended (lead verified; background jobs
  don't survive). Completed before death: load3-b64/b128, load6-b64/b128, load12-b64 (archived baseline-run1/).
  ALL ended EXCLUSIVE: shared quantizes above 128; exclusive gives internalPeriod 128 but data callbacks of
  64 (b64 runs: minFrames=maxFrames=64) or 128 — **64 AND 128 callback sizes SUPPORTED (exclusive), verified by
  stats.min/maxCallbackFrames**. 48 kHz, Pro Audio, latency device 5.33 ms.
- run1 findings: overruns=0 everywhere; maxLoad ≤58.6% (load12-b64); p99 load ≤17%; **starvations ~3200-3374/60s
  (~54/s) at EVERY load incl. load3** → load-INDEPENDENT: device burst pacing (~18 ms bursts, drift -60..-70 ppm)
  vs noteDelivery tolerance (deviceBuffer 5.33 ms + 2 ms). NOT a load-induced engine failure; characterize with a
  shared-mode comparison (owner's 17/11 config was shared, 0 late callbacks) before concluding.
- Probe upgraded (additive): EXACT record-time audio counters (g_audioLocks/Allocs/IOs, one relaxed add per audio
  violation) → whole-window totals (ring keeps tail only); --scenario mode = ONE scenario per foreground call
  (<600 s executor cap; lead directive: never background measurements again). scenarioJson: violations.audioExact
  + violations.ringTail (labelled). Rebuild green.
- **load12-b128 (exclusive, new probe): 22506 cb, 60.02 s, overruns=0, starvations=2917, max=0.625 ms,
  p99=0.222 ms, p50=0.110 ms, mean load 4.3%, p99 8.3%, max 23.4%; AUDIO EXACT locks=1899128 (84.38/cb =
  7.03/plugin call — linear in plugin count, matches Phase A 7.53), allocs=6 (rare Array growth), io=0.**
- Remaining: load24-b64, load24-b128, re-run load3/6/12-b64 with exact counters, shared comparison, breaking-point
  extension (48/96), M5(4) comparison. One per foreground call.

## Step 9 — Phase B foreground batches (2026-11-20 ~04:05)
- load24-b64: 45000 cb, overruns=0, starv=3374, max=0.702 ms, p99=0.306, p50=0.142, mean load 7.8%, p99 23%, max 52.6%;
  locks=3797280 (84.38/cb), allocs=12, io=0.
- load24-b128: 22506 cb, overruns=0, starv=2327, max=0.806, p99=0.390, p50=0.193, mean 7.8%, max 30.2%;
  locks=3798336 (168.77/cb = 24 plugin calls × 7.03), allocs=12, io=0.
- load48-b64: overruns=0, max=1.155 ms (86.6% of 1.333 ms budget), p99=0.570 (42.8%), mean 14.7%; locks 168.76/cb, allocs=24.
- **load96-b64: BREAKING POINT — overruns=30/45012 cb (0.067%), overBudget=30 (ring agrees exactly), max=1.737 ms
  (130.3%), p99=1.007 (75.5%), p95=0.833, p50=0.544, mean 0.376 (28.2%); starv=1608 (fewer: render fills the burst
  gaps); locks=15193344 (337.54/cb, still 7.03/plugin call), allocs=48, io=0; callbackErrors=0, delivered 60.02 s.**
  FIRST METRIC TO MOVE: the TAIL — maxLoadPpm crossed 100% (130.3%) → overruns; p99 42.8%→75.5%, mean 14.7→28.2%.
  Tail-driven (worst blocks: all 96 plugins + dense events), not average saturation. Engine kept playing (no crash/stall).
- Lock rate invariant: ~7.03 CS calls per plugin processBlock at every load (3→96 instances) — the JUCE MidiEventList
  overhead scales LINEARLY with plugin count; at 96/b64 it is 337 calls/callback.
- Next: 72-b64 (narrow bracket), re-runs with exact counters, shared comparison, M5(4) comparison.

## Step 10 — Phase B COMPLETE + delivery (2026-11-20 ~04:25)
- load72-b64: overruns=0, max=1.229 ms (92.2%), p99=0.784, mean 21.5% → bracket CLEAN ≤72, FAILS 96.
- Re-runs with exact counters: load3-b64 (locks 544320, allocs 3), load6-b64 (949320, 3), load12-b64 (1898640, 6),
  load3-b128 (544428, 3), load6-b128 (949416, 3) — all overruns=0, io=0; starv 3374/3251/3164 pattern.
- **load24-b128-SHARED: block=480 engine period BUT minFrames=maxFrames=128 (miniaudio splits) — starvations=0!**
  load3-b64-shared: 64 frames shared, starvations=0. → exclusive starvations = burst pacing (~18 ms) vs tolerance
  7.33 ms (deviceBuffer 5.33+2); shared tolerance 22 ms > gap → 0. Load-independent, reproducible (3374 ×3).
  Hardware underrun in exclusive NOT provable engine-side → pendência (listening/WASAPI buffer depth).
- **load24-b64-M5(4): p99 0.131 (vs serial 0.306, 2.3×), p50 0.054 (2.6×), mean load 3.9% (vs 7.8%), max 0.677,
  workerJobs=388134, stalls=0, fallback=false; audio locks 898556 (19.96/cb) + workers (total ≈ serial 3.81M vs 3.82M);
  allocs=2, io=0.** D51 default untouched.
- ALL 14 scenarios delivered 60.00-60.02 s, callbackErrors=0, nonFiniteBlocks=0.
- DELIVERED: docs/relatorios/2026-11-20-s7-rt-measurement.md (MODELO format, proposed ESTADO lines, limitations)
  + index row (before «Acrescentar» footer, all rows preserved) + evidencia/2026-11-20-s7-rt-measurement/
  (728 KB: notes, probe source, seam patch, phaseA-final2 + rt-strict-run2 + merge logs, devices.json, build
  recipe/logs, baseline/ 14 JSONs, baseline-run1/ archive, scenarios/ 14 stderr logs).
- Files changed: device.hpp/cpp (seam), tests/s7_rt_probe_tests.cpp (new), tests/CMakeLists.txt (additive s7 block
  + /DEBUG probe target), report/index/evidence. NO RT behaviour change, NO default change, round-1 preserved,
  nothing committed. Merge Release 27/27; strict gate rt_strict RED 1/12/4 (owner decision).
- S7 COMPLETE. Final answer to lead with coverage table, negctrl results, violation counts, baseline table,
  breaking point, files, limitations, report path.

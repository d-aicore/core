# Relatório — 2026-11-20 — S7: engine RT measurement — callback alloc/lock/IO proof (RED finding) + Release real-device baseline

| | |
|---|---|
| Scope | Owner S-round item 6, second part (FINAL round-1 item). Contract verbatim: «Verifica alocações, locks e I/O no callback com medição explícita e controlos negativos que façam a prova falhar. Reporta o que ficou fora da instrumentação. Depois mede um projeto de referência em Release, com dispositivo real, a 64 e 128 samples quando suportado. Regista dropouts, tempo máximo e percentil 99 por bloco, interface, driver, sample rate e carga.» |
| Snapshot | `a9a038430d781959829f8510067ce59afffaad62` (main) + uncommitted round-1/S1/S2/S3/S4/S5/S6/S11 locals — ALL preserved (shared files re-read before editing; no other agent's file touched). Nothing committed (no verbatim authorization). |
| Build | Own build `D:\Core-build\s7\build`: **Release**, Ninja, `CORE_BUILD_TESTS=ON`, `CORE_BUILD_APP=ON`, `FETCHCONTENT_SOURCE_DIR_JUCE=D:/Core-deps/JUCE-9.0.5` (D113 full-patch reverse check passed), vcvars64 (VS 18 BuildTools 14.51) + `VSLANG=1033` + `chcp 65001`, `--parallel 6`. Probe target links `/DEBUG` (its own PDB for stack symbolization; test target only, no code change). |
| Verdict | **Proof tooling GREEN (negative controls all fire); engine path RED on criterion 3 as written** — lock API calls (~7 CS calls per plugin processBlock) + rare realloc growth on the audio thread and M5 workers, from the statically-linked JUCE VST3 hosting layer (`MidiEventList`'s `Array<Vst::Event, CriticalSection>`); **zero file I/O, zero blocking waits** on the audio thread. **Real-device baseline: 0 overruns up to 72 plugin instances at 64 samples; breaking point at 96 (30 overruns/45012 callbacks, tail-driven).** Shared mode: 0 starvations; exclusive small-buffer runs show a load-independent delivery-gap detector pattern (~54/s) characterized below, not an engine overload. Merge **27/27** (Release); the strict zero-lock gate is a separate `rt_strict` case, measured RED = owner-decision gate (ENV-03 precedent). |
| Confirmed by | code reading (audit map, file:line below) + test execution (probe suite, negative controls, merge) + real-device measurement (14 scenarios × 60 s, WASAPI, owner's interface). Not a UI test; no desktop gate claimed. |

## 1. O que foi medido (what was measured)

- **Phase A** — allocation/lock/IO instrumentation of the audio callback, explicit measurement, negative
  controls that make the proof fail, null backend, Release, reference song (kick+bass+lead through
  CoreTestSynth ×3 + CoreTestGain insert, the S6 bit-exact song), serial and M5(4), blocks 64/128.
- **Phase B** — Release real-device baseline: **Auscultadores (USB-C Audio)** (the owner's 17/11 interface,
  default playback device; also present: Altifalantes Realtek, Q27G3XMN/KB272 G0 NVIDIA HD Audio, Realtek
  Digital Output — `devices.json`), WASAPI, 48 kHz, load ramp 3→6→12→24 plugin instances plus breaking-point
  extension 48/72/96, buffers 64 and 128, 60 s per scenario, serial (D51 default) + one M5(4) comparison +
  two shared-mode comparisons. Listening level −15 dB, ear guard ON (forced by `engine.start`,
  server.cpp:395) — never disabled. Machine QUIET (lead-confirmed; no other agents/builds during Phase B).
- Environment: Windows, this machine (16 logical cores), VS 18 BuildTools, JUCE 9.0.5 + D113 vendored patch.
- Limitations: §8.

## 2. Instrumentation — the callback path and its coverage

Chain (read, file:line): miniaudio → `MiniaudioOutput::callback` (device.cpp:79-120) → `Engine::process`
(engine.cpp:192-408) → `Graph::process` (graph.cpp:1506-1529; dispatch to `processLive` at 1512-1513) →
`runTrack` (1173-1374) → `renderTrack` (969-1066) → `PluginInstrument::renderStereo` (graph.cpp:169-189) /
`PluginEffect::process` (242-255) → `JuceProcessor::process`/`runBlock` (juce_host.cpp:142-222) → JUCE VST3
`processBlock` (juce_VST3PluginFormatImpl.h:2429-2443, vendored crash-aware SpinLock acquire 3088-3126) →
`processAudio` (2509-2559) → plugin DLL. Post: loudness (loudness.cpp:57-69), analyzer (analyzer.cpp:45-63),
ear guard (output_guard.cpp:31-72), capture copy (engine.cpp:386-398), `recordCallback` (engine.cpp:152-171).

| Coverage | Primitives | How |
|---|---|---|
| **HOOKED (measured)** | C++ `new`/`delete` (all sizes/align/nothrow) | global overrides in the probe exe, record size, forward to CRT; TLS dedup vs heap hooks |
| **HOOKED** | C `malloc`/`free`/`realloc` (CRT, JUCE, miniaudio — exe + static libs) | IAT hooks on kernel32 `HeapAlloc`/`HeapFree`/`HeapReAlloc`: the static UCRT malloc family funnels through them (dumpbin-verified imports). Safer than overriding the CRT allocator (heap-mismatch risk), strictly broader |
| **HOOKED** | File I/O: `CreateFileW/A`, `ReadFile`, `WriteFile`, `FlushFileBuffers` | IAT hooks (kernel32/kernelbase/api-ms-win-core-*), file name copied into the record |
| **HOOKED** | Locks: `Enter/TryEnter/Leave/Delete/InitializeCriticalSection(+Ex)`, `Acquire/TryAcquire/ReleaseSRWLockExclusive/Shared` (MSVC `std::mutex`) | IAT hooks |
| **HOOKED** | Blocking: `WaitForSingleObject(Ex)`, `Sleep(Ex)`, `SwitchToThread` | IAT hooks |
| **AUDITED (intrinsics, not hookable)** | JUCE VST3 `processMutex` = `juce::SpinLock` (impl.h:3063): pure atomic CAS (juce_SpinLock.h:71-81; enter juce_Thread.cpp:432-443: 20 spins + `Thread::yield`); vendored crash-aware audio acquire (impl.h:3088-3126). Uncontended = one CAS; contention would spin + `SwitchToThread` (hooked — never fired on the audio thread) | code audit + the hooked yield path |
| **AUDITED (never reached from the callback)** | `JuceHost::Impl::mutex` std::mutex (juce_host.cpp:285) — only control-thread methods (get 387/395/418, keepOnly 427, releaseAll 441, resetAll 448, captureStates 490, showEditor 513, setParameter 564, parameters 589, vstPreset 620); `RtWorkerPool` has NO locks (semaphore release + bounded `_mm_pause` spin, rt_workers.hpp:64-97,131-133); engine/graph render paths use fixed arrays + atomics + SPSC queues (engine.hpp:176-223; graph.cpp:985-991,1184,1427-1433,1449,1459; juce_host.cpp:91-92,245); `MidiMessage` 3-byte ctors inline (juce_MidiMessage.cpp:173-182, allocateSpace 349-359 mallocs only > packedData); `MidiBuffer` pre-reserved kMaxNotes×16 (juce_host.cpp:92; juce_MidiBuffer.cpp:123,139-165), note-ons capped (juce_host.cpp:188); plugin-param automation path lock-free (impl.h:2040-2044 → juce_VST3Common.h:1597-1600 atomic cache; MessageManager fast path juce_MessageManager.cpp:72-82,244-247) — not reached by the reference song (no automation) | code audit; the probe confirms zero SRWLOCK/CS from these on the audio thread |
| **OUT OF INSTRUMENTATION (declared)** | **plugin DLL internals** (own CRT/imports — a DLL-side fault is invisible to exe-side hooks; the negative controls are therefore probe-injected through the seam, inside the window); **third-party plugins**; **OS/WASAPI internals** (COM vtable calls, scheduler); JUCE atomics (above). Error paths that allocate by design: the plugin's own throw (graph.cpp:96-101), `processLive`'s worker-failure `std::runtime_error` (graph.cpp:1458) — not reached in any measured run (callbackErrors=0, liveStalls=0) | declared boundary + audit |

**The seam (one, test-only, lead-reviewed and approved):** `core::rtProbe` — `std::atomic<RtProbe*>`, null by
default (device.hpp/device.cpp, `s7-seam.patch`). Production cost: **one relaxed atomic load per callback**;
no RT-path behaviour change, no default change (D51). When armed, the callback publishes (a) the in-callback
window (whole callback: engine call AND timing record), (b) the audio thread's id (attribution —
control-thread activity is classified, never miscounted), (c) an optional directed-fault `inject` (negative
controls), (d) an optional per-block observer with the exact values `recordCallback` gets (exact max/p99
without a second seam — `EngineStats::loadBands` are 1 %-of-budget bands, 13.3 µs at 64/48000, too coarse for
a p99 claim; engine.cpp untouched). Justification: quality bar 3 demands proof by tooling, not reading; the
flag needs thread attribution; the inject point lets the proof FAIL on purpose; one seam serves all three.

**Recorder:** fixed 65536-slot circular ring + cumulative count, `CaptureStackBackTrace(2,12)`, TLS recursion
guard — **no allocation, lock or I/O in the hook path**. Exact record-time audio-thread counters
(locks/allocs/IO, one relaxed add per audio violation) give whole-window totals; the ring keeps the tail of
long runs for stacks/kind detail (labelled `ringTail` in the JSONs). dbghelp symbolization at dump only
(probe PDB via `/DEBUG`). M5 worker coverage: `processLive` waits every worker before returning
(graph.cpp:1455; rt_workers.hpp:84-96), so non-stalled worker jobs run entirely inside the window; a late
worker's tail after a stall is the declared gap (no stalls occurred).

## 3. Negative controls — the proof CAN fail (all fired)

Each fault runs inside the in-callback window on the AUDIO thread via the seam's inject point (0.4 s windows,
null backend, block 128; `phaseA-final2.err.log`):

| Control | Injected | Detected (AUDIO tid, stacks captured) |
|---|---|---|
| allocation | `malloc(96)` + `new char[128]` | `HeapAlloc` size=96 **and** `new` size=128 (+ matching free/delete) — both the C-heap IAT path and the C++ override fired |
| file I/O | `CreateFileW(L"NUL")` + `ReadFile` | `CreateFileW` (file name recorded) **and** `ReadFile` |
| lock | `EnterCriticalSection` + `Leave` on a probe-owned CS | `EnterCriticalSection` **and** `LeaveCriticalSection` |

Wiring test: 20 IAT entries hooked; every critical name installed ≥1 with a non-null original
(`CreateFileW`, `ReadFile`, `Enter/LeaveCriticalSection`, `HeapAlloc/Free/ReAlloc`,
`AcquireSRWLockExclusive`, `WaitForSingleObject`, `Sleep`). If any hook were absent the suite fails —
the proof cannot pass silently broken.

## 4. Phase A results — engine path (null backend, Release, 2 s windows, reference song = 4 instances)

| run | audio lock calls | per callback | audio allocs | audio file IO | workerJobs | cross-validation |
|---|---|---|---|---|---|---|
| serial block 64 | 22674 | 15.06 | 3 | 0 | 0 (no pool) | seam max == stats.maxRenderMs (±1 µs); cpu totalPct 1.46% |
| serial block 128 | 22644 | 30.11 | 3 | 0 | 0 | same; cpu 1.52% |
| M5(4) block 128 | 6296 audio + 16602 other-thread records (workers + JUCE TimerThread, stacks) | 8.38 audio | 1 | 0 | **1496** (stalls 0, fallback false) | same; cpu 2.44% |

**RED finding (criterion 3 as written).** Symbolized stacks + source:
`juce::MidiEventList` stores **`Array<Steinberg::Vst::Event, CriticalSection>`**
(D:/Core-deps/JUCE-9.0.5 `modules/juce_audio_processors_headless/format_types/juce_VST3Common.h:1289`).
Every `getEventCount`/`getEvent`/`addEvent` locks that array. Reached on every plugin processBlock via
`processAudio` → `associateWith(data, midiBuffer)` / `hostToPluginEventList` (impl.h:2538, 3431-3459) and
`MidiEventList::toMidiBuffer(midiMessages, *midiOutputs)` (impl.h:2558; common.h:1155-1193), **plus
plugin-DLL → host `IEventList` vtable callbacks** (DLL frames visible in the stacks). Measured rate:
**~7.0–7.5 CS calls (≈3.5–3.76 enter/leave pairs) per plugin processBlock**, invariant across loads
(Phase B: 7.03/plugin call from 3 to 96 instances). The rare allocations are the same layer:
`MidiEventList::addEvent` → `Array<Event>::add` → **realloc growth** (`HeapAlloc` 384), 1–3 per 2 s window
while the per-instance list grows to the song's peak event density (Phase B: 3–48 per 60 s, scaling with
instance count). **Zero file I/O, zero SRWLOCK, zero waits/Sleep on the audio thread and workers.** All
other in-window allocations classify to JUCE's TimerThread (`EditControllerParameterDispatcher` 60 Hz timer
started at setActive, impl.h:2250; `MessageBase::post` stacks) or control-thread json — control plane, not
the engine path.

**Exact wording (lead directive).** The UNCONTENDED `EnterCriticalSection` fast path is user-mode
(interlocked); a kernel transition happens only on contention. The measured calls were uncontended during
process (the per-instance lists are touched only by the processing thread; JUCE's `processMutex` SpinLock
excludes concurrent control-path access). Criterion 3 — «no allocation, lock, I/O or blocking call on the
audio thread, proven by tooling» — is nonetheless **NOT met as written**: lock API calls and rare realloc
growth ARE on the RT path, in the statically-linked hosting layer (`core_plugin_host`), not in plugin-DLL
internals. Residual risk: priority inversion / unbounded spin if any other thread ever contends these
lists or the processMutex while the audio thread holds them (the crash-in-reset deadlock family, D/S6).

**Strict gate structure (ENV-03 precedent; AGENTS.md: «Do not relax criteria or weaken the remaining tests
to obtain green»).** Nothing is relaxed: the merge set asserts the detector works (wiring + 3 negative
controls + characterization sanity: run happened, seam==stats, plugins worked, counts recorded as DATA);
the FULL criterion (0 locks / 0 allocs / 0 IO on the audio thread) lives in a separate case
`S7 STRICT gate`, registered only under `CORE_S7_STRICT=1` → ctest `s7_rt_strict_tests`, label **rt_strict**
(NOT merge), measured **RED: 1 test / 12 checks / 4 failures** (serial+M5 × locks+allocs). It goes green
only when the calls are gone (authorized fix) or the owner explicitly accepts them — a declared gap, never
a silent pass. This mirrors the ENV-03 exclusion (known pre-existing Debug failure measured separately).

**Fix directions — PROPOSALS only, NOT implemented** (D113 precedent: vendored JUCE changes need explicit
owner authorization): (a) vendored patch giving the RT event lists a single-threaded/no-op lock type or a
lock-free array (audio-thread-only access during process is already guaranteed by processMutex), with
pre-reserved capacity to kill the realloc growth; (b) accept the uncontended calls with an owner decision
recorded in the ledger. Either way the strict gate is the acceptance test.

## 5. Phase B — Release real-device baseline (WASAPI, Auscultadores USB-C Audio, 48 kHz, 60 s/scenario)

**Buffer support (measured, `stats.min/maxCallbackFrames`):** WASAPI **shared** quantizes the engine period
to **480 frames (10 ms, latency 20 ms)** but miniaudio splits it into the requested data-callback size —
**64 and 128 both delivered in shared** (minFrames=maxFrames=64/128). **Exclusive** gives internal period
128 (latency 5.33 ms) with data callbacks of **64 or 128 as requested**. So 64 and 128 are SUPPORTED on this
interface in both modes; the ramp below ran exclusive (smallest device buffer), the shared comparisons are
included. Scheduling: Pro Audio (MMCSS) + TIME_CRITICAL. Driver: WASAPI (Windows), interface firmware clock
drift −53…−70 ppm (exclusive), −388…−458 ppm (shared, tracked by the detector baseline).

**Reference project:** load-ramp song, 120 BPM, 960 PPQ, pattern 3840 ticks repeating for 60 s
(115200 ticks); roles cycle kick/bass/lead; every track a CoreTestSynth instance (fixed params, sine per
note) + a CoreTestGain insert when withGain; instances = 3 (3 synths, no inserts), 6 (3×2), 12 (6×2),
24 (12×2), 48/72/96 (24/36/48 tracks ×2). Plugin identity: Core Test Synth / Core Test Gain, vendor Core,
version 1.0.0, files `D:\Core-build\s7\build\app\selftest-plugins\…\CoreTestSynth.vst3` /
`CoreTestGain.vst3` (exact paths + ids in every scenario JSON's `plugins`). No automation/modulators;
deterministic (S6 proved bit-exactness). listenDb −15, ear guard ON.

**Baseline table (serial unless noted; exact whole-window audio counters; load% = render/budget):**

| scenario | mode | cb frames | callbacks | overruns | starvations | max ms | p99 ms | p95 ms | p50 ms | load mean/p99/max % | audio locks (per cb) | allocs | IO |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| load3-b64 | excl | 64 | 45000 | 0 | 3374 | 0.435 | 0.075 | 0.057 | 0.029 | 2.0 / 5.6 / 32.6 | 544320 (12.10) | 3 | 0 |
| load3-b128 | excl | 128 | 22504 | 0 | 3251 | 0.444 | 0.104 | 0.078 | 0.051 | 2.0 / 3.9 / 16.7 | 544428 (24.19) | 3 | 0 |
| load6-b64 | excl | 64 | 45000 | 0 | 3374 | 0.496 | 0.103 | 0.078 | 0.042 | 2.6 / 7.7 / 37.2 | 949320 (21.10) | 3 | 0 |
| load6-b128 | excl | 128 | 22502 | 0 | 3164 | 0.492 | 0.136 | 0.104 | 0.068 | 2.6 / 5.1 / 18.4 | 949416 (42.19) | 3 | 0 |
| load12-b64 | excl | 64 | 45000 | 0 | 3374 | 0.538 | 0.174 | 0.132 | 0.075 | 4.4 / 13.1 / 40.3 | 1898640 (42.19) | 6 | 0 |
| load12-b128 | excl | 128 | 22506 | 0 | 2917 | 0.625 | 0.222 | 0.167 | 0.110 | 4.3 / 8.3 / 23.4 | 1899128 (84.38) | 6 | 0 |
| load24-b64 | excl | 64 | 45000 | 0 | 3374 | 0.702 | 0.306 | 0.244 | 0.142 | 7.8 / 23.0 / 52.6 | 3797280 (84.38) | 12 | 0 |
| load24-b128 | excl | 128 | 22506 | 0 | 2327 | 0.806 | 0.390 | 0.300 | 0.193 | 7.8 / 14.6 / 30.2 | 3798336 (168.77) | 12 | 0 |
| load48-b64 | excl | 64 | 45002 | 0 | 2476 | 1.155 | 0.570 | 0.450 | 0.274 | 14.7 / 42.8 / 86.6 | 7594688 (168.76) | 24 | 0 |
| load72-b64 | excl | 64 | 45004 | 0 | 3121 | 1.229 | 0.784 | 0.639 | 0.406 | 21.5 / 58.8 / 92.2 | 11392825 (253.15) | 36 | 0 |
| **load96-b64** | excl | 64 | 45012 | **30** | 1608 | **1.737** | 1.007 | 0.833 | 0.544 | 28.2 / 75.5 / **130.3** | 15193344 (337.54) | 48 | 0 |
| load3-b64-shared | shared | 64 | 45015 | 0 | **0** | 0.174 | 0.074 | 0.059 | 0.028 | 2.0 / 5.5 / 13.1 | 544500 (12.10) | 3 | 0 |
| load24-b128-shared | shared | 128 | 22505 | 0 | **0** | 0.852 | 0.359 | 0.296 | 0.195 | 8.1 / 13.5 / 32.0 | 3798054 (168.76) | 12 | 0 |
| load24-b64-**m5(4)** | excl | 64 | 45008 | 0 | 3375 | 0.677 | **0.131** | 0.100 | **0.054** | 3.9 / 9.8 / 50.8 | 898556 audio (19.96) + workers | 2 | 0 |

Budget: 64/48000 = 1.333 ms; 128/48000 = 2.667 ms. Every scenario delivered 60.00–60.02 s of audio;
callbackErrors=0, nonFiniteBlocks=0, liveStalls=0, liveFallback=false everywhere. M5(4):
**workerJobs=388134** (workers really played), total lock work ≈ serial (3.81M vs 3.82M) redistributed
audio→workers; **p99 0.306→0.131 ms (2.3×), p50 0.142→0.054 ms (2.6×), mean load 7.8→3.9%** at 24 instances.
D51 default stays serial; the M5 run opted in by argument.

**Breaking point.** 0 overruns through **72 instances @ 64** (max load 92.2% — the edge); at **96 instances
@ 64: 30 overruns / 45012 callbacks (0.067%)**, max render 1.737 ms = **130.3% of budget**. **The first
metric to move is the TAIL**: maxLoadPpm crossed 100% (86.6→92.2→130.3 across 48→72→96) while the mean was
still 28.2% and p99 75.5% — worst-case blocks (all 96 plugins gathered + dense note events), not average
saturation. The engine kept playing (no crash, no stall, no callback error; delivered 60.02 s). The lock
overhead scales linearly (7.03 CS calls/plugin call → 337/callback at 96) but is a small fraction of the
budget (~tens of µs); the dominant cost is the plugin processing itself. Bracket: **clean ≤72, fails at 96**
(64-sample exclusive buffer, this machine, these test plugins).

**Starvations characterization (exclusive vs shared).** `engine.stats.starvations` = the delivery-gap
detector (`noteDelivery`, engine.cpp:178-190: device consumed more time than delivered + its buffer +
2 ms tolerance). Exclusive runs show **~54/s at EVERY load including load3** (3374 events/60 s — the SAME
count across load3/6/12 at b64, i.e. load-INDEPENDENT and highly reproducible), while overruns=0 and max
load ≤52.6% at 24: the engine renders within budget. The interface delivers in ~18 ms bursts (exclusive
event pacing; drift −60 ppm tracked); the exclusive tolerance is deviceBuffer 5.33 ms + 2 ms = 7.33 ms <
burst gap → every gap flagged. **Shared mode (480-frame period, 20 ms buffer, tolerance 22 ms): 0
starvations at the same loads** — consistent with the owner's 17/11 real-app test (shared, 0 late callbacks
over 10 min 54 s). Conclusion: the exclusive counts are the detector reacting to the interface's burst
pacing with a small buffer tolerance, NOT a load-induced engine failure. Whether the hardware itself
underruns between bursts in exclusive mode cannot be proven from engine-side counters — declared pendência
(listening check / WASAPI buffer-depth investigation); the detector's tolerance calibration (buffer model vs
actual device pacing) is a follow-up proposal, not changed here.

## 6. Resultados (MODELO table)

| ID | Problema / resultado | Evidência | Classificação | Estado |
|---|---|---|---|---|
| S7-1 | Negative controls fire (alloc/IO/lock) — proof can fail | phaseA-final2.err.log; §3 | comprovado por teste | validado |
| S7-2 | **Lock calls on the audio thread + M5 workers** (~7 CS calls/processBlock; JUCE MidiEventList `Array<Event,CriticalSection>`, juce_VST3Common.h:1289) | symbolized stacks (phaseA-final2.err.log, sc-*.err.log); exact counters (baseline JSONs `violations.audioExact`) | comprovado por teste + código | **aberto — RED, owner decision (rt_strict gate)** |
| S7-3 | Rare realloc growth on the audio thread (same layer; 3–48/60 s) | ALLOC stacks HeapAlloc 384 `MidiEventList::addEvent` | comprovado por teste | aberto — same decision as S7-2 |
| S7-4 | Zero file I/O / zero blocking waits / zero SRWLOCK on the audio thread (all runs) | exact counters io=0; kinds tables | comprovado por teste | validado |
| S7-5 | Engine alloc/lock-free by design EXCEPT the JUCE hosting layer (audit map) | §2 citations | confirmado no código | validado |
| S7-6 | Real-device baseline: 0 overruns ≤72 instances @64; breaking point 96 (30/45012, tail-first) | baseline/*.json; §5 | comprovado por teste (real device) | validado / breaking point registado |
| S7-7 | Exclusive starvations ~54/s load-independent = burst pacing vs 7.33 ms tolerance; shared 0 | load3/6/12 identical 3374; shared comparisons; engine.cpp:178-190 | comprovado por teste + código | aberto — hardware underrun not provable engine-side (pendência) |
| S7-8 | M5(4) halves callback time at 24 instances (p99 2.3×), workers proven, stalls 0 | load24-b64-m5.json (workerJobs 388134) | comprovado por teste | validado (D51 default OFF unchanged) |
| S7-9 | Merge 27/27 Release; strict gate separate RED (ENV-03 pattern) | merge-run1.log; rt-strict-run2.* | comprovado por teste | validado / gate aberto |

## 7. Alterações (files changed — write scope respected)

| file | change | kind |
|---|---|---|
| `engine/src/io/device.hpp` | RtProbe struct + `extern std::atomic<RtProbe*> rtProbe` (justification comment) | implementation — ONE test-only seam, null by default |
| `engine/src/io/device.cpp` | seam definition + callback arm/disarm (entry: flag+tid; exit: observer→inject→flag clear) | implementation — production = one relaxed load/callback; NO RT-path behaviour change |
| `tests/s7_rt_probe_tests.cpp` | NEW permanent probe suite (hooks, recorder, negative controls, characterization, strict gate, --devices/--scenario/--baseline modes) | implementation (tests) |
| `tests/CMakeLists.txt` | ADDITIVE s7 registration block (merge + rt_strict entries, `/DEBUG` on the probe target only); all existing blocks preserved | implementation (tests) |
| `docs/relatorios/…` | this report + index row + evidence dir | documentation |

NO engine processing change, NO default change (D51), NO other agent's file touched, all round-1/S1–S6/S11
uncommitted work preserved (shared files re-read before editing). Nothing committed. Fix directions for
S7-2/S7-3 are PROPOSALS (§4), not implemented.

## 8. Verificação (commands, exit codes, counts)

- Configure: `cmake -S D:\CORE\CORE -B D:\Core-build\s7\build -G Ninja -DCMAKE_BUILD_TYPE=Release
  -DCORE_BUILD_TESTS=ON -DCORE_BUILD_APP=ON -DFETCHCONTENT_SOURCE_DIR_JUCE=D:/Core-deps/JUCE-9.0.5`
  → ok, D113 reverse check passed. Build run 1: 544/572 then FAILED (my probe bug: `CHECK_EQ` with one
  argument, line 795) — fixed; build run 2: **35/35 GREEN** (build-run1.log/build-run2.log; recipe build.bat).
- Probe merge set (default exe run): **6 tests / 77 checks / 0 failures** (phaseA-final2.out.log) — wiring,
  3 negative controls, serial 64/128 characterization, M5(4) characterization.
- Strict gate (`CORE_S7_STRICT=1`, filter "S7 STRICT"): **1 test / 12 checks / 4 failures — RED by design**
  (rt-strict-run2.out.log): serial+M5 × (locks≠0, allocs≠0); IO=0 passes. Owner-decision gate.
- **`ctest --test-dir D:\Core-build\s7\build -L merge --output-on-failure`: 27/27 PASSED (100%), 89.21 s**
  (merge-run1.log) — includes s7_rt_probe_tests 19.66 s, m5_bitexact_tests 17.42 s (S6 suite still green in
  Release), engine_process_tests 32.57 s (Release: ENV-03 is Debug-only, not hit). Baseline 26 merge → 27
  (+s7), nothing removed or weakened. `s7_rt_strict_tests` registered under label rt_strict (excluded from
  merge explicitly, ENV-03 precedent).
- Phase B: 14 foreground scenario runs (`--scenario … exclusive|shared`, 60 s each), all completed, JSON per
  scenario in `baseline/` (run1's first five, produced before the probe gained exact counters, archived in
  `baseline-run1/` — same method/params, tail-classified breakdowns; all ramp numbers in §5 are from the
  exact-counter runs). Per-scenario stderr logs (kind tables + alloc stacks) in `scenarios/`.
- Cross-validations: seam per-block max == engine.stats.maxRenderMs (±1 µs) in every suite run;
  overBudget(ring) == overruns(stats) exactly at the breaking point (30 == 30); delivered seconds from the
  seam's per-block frames (60.00–60.02 s everywhere); cpu totalPct > 0 (plugins really worked).

## 9. Limitations

- Deterministic TEST plugins (sine/gain), not the owner's real plugin library; plugin DLL internals are out
  of instrumentation (own CRT/imports) — real plugins may add their own RT behaviour, covered only by the
  audit boundary. The live plugin block is compile-time 128 (juce_host.cpp:23; CSV-09 partial open).
- One machine, one interface (USB-C Audio), 60 s scenarios — not the D51 8 h soak; M5 default-ON
  ratification remains the owner's (soak + bit-a-bit on his PC).
- Exclusive-mode starvations: engine-side counters cannot prove hardware underruns between bursts (§5) —
  listening/hardware verification pending; shared mode (the owner's 17/11 configuration class) is clean.
- Kind/thread breakdowns of long runs come from the ring's steady-state tail (labelled); the headline
  audio-thread lock/alloc/IO totals are EXACT (record-time counters). Worker vs JUCE-TimerThread records
  separate by stack signature, not by tid (both classify "other").
- Phase A used the null backend (its ~187 starvations/2 s are miniaudio null-pacing artifacts, declared);
  the real-device Phase B is the authoritative dropout measurement.
- Release `/OPT:ICF` folds some symbols — stack names like `associateWith+0x2b` are neighborhood-accurate;
  the call chain and the source citations were verified against the JUCE code, not by symbol alone.
- No UI execution; not a desktop gate. ENV-03 (Debug teardown) remains a separate open task (3A).

## 10. Pendências e passagem (proposed ESTADO lines — the escrivão writes them, D72/D80)

- *Estado medido — S7 (2026-11-20, a9a0384 + round-1 locals, own Release build)*: callback alloc/lock/IO
  proof by tooling — negative controls (alloc/IO/lock) all fire; engine audio thread + M5 workers: **zero
  file I/O, zero blocking waits, zero allocations except rare JUCE event-list realloc growth (3–48/60 s);
  lock API calls present (~7 CS calls per plugin processBlock, JUCE MidiEventList
  `Array<Vst::Event,CriticalSection>`, juce_VST3Common.h:1289) — criterion 3 NOT met as written**;
  uncontended fast path user-mode, kernel only on contention. Real-device baseline (Auscultadores USB-C
  Audio, WASAPI, 48 kHz, 64/128 both supported in shared+exclusive, 60 s × 14 scenarios): **0 overruns
  through 72 plugin instances @64 (max load 92.2%); breaking point 96 instances (30 overruns/45012 cb,
  max 130.3%, tail-first)**; shared mode 0 starvations, exclusive ~54/s load-independent delivery-gap
  detector pattern (burst pacing vs 7.33 ms tolerance — hardware underrun not provable engine-side).
  M5(4) at 24 instances: p99 0.306→0.131 ms, p50 2.6×, workerJobs 388134, stalls 0; D51 default OFF
  unchanged. Merge Release **27/27** (+s7_rt_probe_tests 6/77/0); strict zero-lock gate `s7_rt_strict_tests`
  (rt_strict) measured RED 1/12/4 = owner-decision gate (ENV-03 precedent, nothing relaxed).
- *Pendências*: (1) **owner decision on the strict gate** — authorize a vendored JUCE RT event-list fix
  (single-threaded/no-op lock + pre-reserved capacity; D113 precedent) OR explicitly accept the uncontended
  calls; the gate is the acceptance test. (2) Exclusive-mode small-buffer starvations: listening/hardware
  verification + detector tolerance calibration proposal (buffer model vs device burst pacing). (3) Commit
  S7 (owner). (4) ENV-03 fix authorization (3A, separate — do not conflate). (5) CSV-09 live plugin block
  fixed 128 remains open. (6) M5 default-ON ratification still requires the owner's D51 soak 8 h +
  bit-a-bit on his PC. (7) Real-plugin-library RT measurement (this baseline used deterministic test
  plugins) — follow-up when the owner's library is available.

## Evidence

[evidencia/2026-11-20-s7-rt-measurement/](evidencia/2026-11-20-s7-rt-measurement/):
`s7-notes.md` (step log), `s7_rt_probe_tests.cpp` (probe source, = tests/ copy), `s7-seam.patch`
(device.hpp/cpp diff), `phaseA-final2.err/out.log` (merge-set run: wiring, negative controls with stacks,
characterization), `rt-strict-run2.err/out.log` (strict gate RED), `merge-run1.log` (ctest 27/27),
`devices.json`, `build.bat` + `build-run1/2.log` (recipe + red→green build), `baseline/` (14 scenario JSONs
with exact counters, kinds, device/stats/cpu/timing), `baseline-run1/` + `baseline-run1.err.log` (first five
scenarios, pre-exact-counter probe, archived), `scenarios/` (per-scenario stderr: kind tables + alloc
stacks). Build: `D:\Core-build\s7\` (probe + build trees).
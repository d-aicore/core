# Relatório — 2026-11-20 — S9: mute CPU + fixed-128 plugin block (MEASUREMENT-FIRST, no implementation)

Owner contract (verbatim, round-2 «Restantes etapas», S9): «Mede a CPU das faixas em mute e confirma o
impacto do bloco de plugin fixo em 128. Antes de suspender processamento, cobre caudas, sidechain, latência
e retoma sem clique. Mantém os dois problemas separados se exigirem correções distintas.»

This wave MEASURES and CHARACTERIZES. Suspending muted-track processing is a decision AFTER the numbers and
the four covers — nothing is implemented here. The two problems stay separate: (A) mute is a fader-only
gate (a CPU defect), (B) the live plugin block is compile-time 128 (a latency/amortization policy
limitation). They need different fixes in different files with different acceptance tests.

## O que foi medido

- Repositório, branch e SHA inicial/final: `D:\CORE\CORE`, `main` @
  `a9a038430d781959829f8510067ce59afffaad62` + uncommitted round-1 fixes (ALL preserved; final SHA
  identical, nothing committed). CSV-09's `graph.cpp:1302-1304` re-located to `:1329-1331` and
  `juce_host.cpp:396` to `:410` in the round-1 working tree (symbols unchanged).
- Âmbito e autorização: owner S-round S9, measurement-first. Write scope: `docs/relatorios/` (+ index row),
  evidence dir, ONE additive permanent measurement suite in `tests/` (the six plugin registration blocks
  preserved), probes/builds outside the repo. NO `graph.cpp`/`juce_host.cpp` change; no engine source change
  at all. The VST3 front (EchoBoy/Decapitator) files/build NOT touched.
- Ambiente: Windows, VS 18 BuildTools MSVC 14.51.36231, **Release**, Ninja, `CORE_BUILD_TESTS=ON`,
  `CORE_BUILD_APP=ON`, `FETCHCONTENT_SOURCE_DIR_JUCE=D:/Core-deps/JUCE-9.0.5` (D113 full-patch reverse
  check PASSED at configure), vcvars64 + `VSLANG=1033` + `chcp 65001`, `--parallel 6`. Own build
  `D:\Core-build\s9\build` (575 targets, rc=0). Null backend, 48 kHz, ear guard ON (captures are taken
  BEFORE the guard). Plugins: deterministic CoreTestSynth + CoreTestGain (repo test plugins) and an
  OUT-OF-REPO CoreTestDelayProbe build (`D:\Core-build\s9\delay-proj`, compiles the repo's
  `TestPlugins.cpp` with `CORE_TEST_DELAY=1`; the repo has no CoreTestDelay target). M5 stays OFF by
  default (D51): every parallel run opts in by argument, RAII `CORE_LIVE_THREADS` guard.
- Testes executados: `s9_mute_block_tests` permanent suite **5 cases / 194 checks / 0 failures** (rc=0);
  `--measure` 18-scenario A/B matrix (3 s windows, rc=0); `--covers` C1–C4 capture evidence (rc=0);
  **`ctest -C Release -L merge --parallel 6`: 28/28, 100% passed** (S7's 27 + the new s9 entry, 72.27 s;
  166.15 s*proc / 80.05 s real). Delay probe build rc=0 (56 targets).
- Limitações: see §8.

Instrumentation REUSES the S7 seam `core::rtProbe` (`engine/src/io/device.hpp:22-28`,
`device.cpp:84-122`), armed with the per-block observer only (exact renderNs/budgetNs per callback; the
IAT violation machinery is S7's suite, not duplicated). CPU attribution = `engine.cpu`
(`server.cpp:923-964`; `CpuMeter` relaxed atomics, `graph.hpp:45-55`, M5-worker safe; instrument slot
`graph.cpp:1286`, per-insert slot `:1324`). Capture = the EXISTING `Engine::startCapture` seam
(`engine.hpp:129-134`, copies AFTER the live-worker mix, BEFORE the ear guard — the m5_bitexact seam).
Harness = the serve architecture (in-process Server + JuceHost + null device), S7 window discipline
(2 s warmup, `resetStats`, ring marks, `engine.cpu` interval start, sleep-only window, stats+cpu read
BEFORE `transport.stop`).

## 1. Problem A — mute CPU (CSV-09 CONFIRMED in code + MEASURED)

### 1.1 Where the gate sits (confirmado no código)

`engine/src/core/graph.cpp` (working tree a9a0384 + round-1):

- `:1329` `const bool audible = !t.mute && (solo_ >= 0 ? ... )` — the ONLY mute consultation in the
  render path, and it is AFTER: the instrument (`:1266-1274` → `PluginInstrument::renderStereo` `:183`
  `processor_->process(...)` — the plugin's `processBlock` runs via the gather, `juce_host.cpp:142-159`),
  the audio clips (`:1278-1285`), EVERY insert (`:1289-1325`, `process` at `:1319`), and the sidechain
  key capture (`:1328`).
- `:1331` mute zeroes the fader target; `:1343` zeroes the send targets; `:1337-1351` ramps them over an
  aligned 32-sample sub-block (`kSubBlock = 32`, `graph.hpp:99`). Nothing upstream is skipped.
- The model has NO instrument-only mute. The per-insert distinction is BYPASS: `:1300`
  `if (t.effects[e]->bypass || t.effects[e]->failed()) continue;` — a bypassed insert IS skipped (and
  drops its latency, `:539`). Mute does neither.
- Mute toggle path: `track.set` (`server.cpp:587-608`) → `commit` → `rebuild(keepTails = true)`
  (`server.hpp:99`): a new Graph with the SAME plugin instances (shared_ptr via the host), held pitches
  and ramp state adopted (`graph.cpp:190-211`, `:856-857`). The instance keeps processing while muted.

### 1.2 Measured waste (comprovado por teste; 3 s windows, block 128, null, Release)

Per-track CPU in % of one core (`engine.cpu`, whole window; budget per block = 2.667 ms @ 48 kHz/128):

| Scenario (tracks muted) | serial kick / bass / lead | M5(4) kick / bass / lead | totalPct serial / M5 | meanMs serial / M5 |
| --- | --- | --- | --- | --- |
| none | 0.212 / 0.261 / 0.186 | 0.263 / 0.409 / 0.278 | 0.659 / 0.950 | 0.0422 / 0.0461 |
| mute1 (kick) | 0.231 / 0.279 / 0.199 | 0.283 / 0.417 / 0.291 | 0.709 / 0.991 | 0.0450 / 0.0476 |
| mute2 (kick+bass) | 0.233 / 0.275 / 0.197 | 0.262 / 0.405 / 0.286 | 0.704 / 0.953 | 0.0452 / 0.0469 |
| mute3 (all) | 0.230 / 0.285 / 0.196 | 0.263 / 0.398 / 0.291 | 0.711 / 0.952 | 0.0459 / 0.0462 |
| bypassfx (bass insert bypassed, none muted) | 0.230 / 0.208 / 0.196 | 0.263 / 0.322 / 0.282 | 0.634 / 0.867 | 0.0444 / 0.0460 |

**Wasted per muted track (mute3), ms per 128-block = cpuPct/100 × 2.667 ms:**

| Track | serial muted cpuPct | serial wasted ms/block (% of budget) | M5(4) muted cpuPct | M5 wasted ms/block | muted/unmuted ratio (serial, M5) |
| --- | --- | --- | --- | --- | --- |
| kick | 0.230% | 0.0061 ms (0.23%) | 0.263% | 0.0070 ms | 1.08, 1.00 |
| bass (synth + gain insert) | 0.285% | 0.0076 ms (0.29%) | 0.398% | 0.0106 ms | 1.09, 0.97 |
| lead | 0.196% | 0.0052 ms (0.20%) | 0.291% | 0.0078 ms | 1.05, 1.05 |
| **all three** | **0.711%** | **0.0190 ms (0.71%)** | **0.952%** | **0.0254 ms** | ≈1.0 |

- **The defect, quantified: a muted track costs the SAME as an unmuted one (ratios ≈ 1.0, both paths).**
  Muting everything saves NOTHING: serial meanMs 0.0422 → 0.0459 (+8.8%, run jitter — no saving),
  totalPct 0.659 → 0.711; M5 meanMs 0.0461 → 0.0462 (ratio 1.00), totalPct 0.950 → 0.952.
- Insert split proves the gate position (bass): serial instrument 0.1903 → 0.2069 muted, gainfx
  0.0704 → 0.0784 muted; **bypassed gainfx = 0.0000** (M5: 0.3065 → 0.2964, 0.1023 → 0.1011, bypass
  0.0000). Bypass saves the insert's CPU; mute saves nothing — the gate is at the fader, not upstream.
- Scale caveat: these are LIGHT deterministic test plugins (a sine per note, a gain). The waste equals
  each muted track's FULL processing cost; with heavy real plugins it scales to that track's share (a
  track at 20% of a core returns ~20% when suspended). The structural defect (ratio ≈ 1.0) is
  plugin-independent; the absolute ms/% here are the test-plugin floor.
- M5 nuance: with 3 tracks on 4 workers every track is parallel, so muting frees worker WORK (0.0254
  ms/block of parallel CPU) but not wall time (critical path unchanged). With more tracks than workers,
  suspended tracks free worker slots → wall-time gain as well.

Evidence: `summary.json` + `a-*.json` (10 rows), `gate-run.log`, `measure-run.log`.

## 2. Problem B — the fixed-128 live plugin block (SEPARATE verdict)

### 2.1 Confirmed in code

- `engine/src/plugins/juce_host.cpp:23` `constexpr int kPluginBlock = 128;` — compile-time.
- `:410` live creation ALWAYS uses `kPluginBlock`; only the offline path takes the knob:
  `offline ? exportPluginBlock() : kPluginBlock`. `:26-40` `CORE_EXPORT_PLUGIN_BLOCK=256|512`
  (export-only, default 128 «until it is measured with the plugins of the house»). `:18` `kMaxBlock=512`.
- The graph calls in sub-blocks ≤ 32 (`graph.hpp:99`); `JuceProcessor::process` (`:142-159`) gathers them
  into `block_ = 128`: per-sample copy in (effects) / copy out (instruments) EVERY sub-block, and when
  full, `runBlock` (`:196-220`): `view.clear()` + `copyFrom(inL/inR)` + `processBlock(128)` + copy out.
- `:162` `latencySamples() = instance_->getLatencySamples() + block_` — the gather adds a FIXED 128
  samples to every plugin path, reported to the PDC map (`graph.cpp:535-541`, `:610-672`).

### 2.2 Measured device-block sweep (same reference song, 3 s windows, null, Release)

| Device block | path | meanMs | p50 | p99 | max | load avg | totalPct | latency (samples) | plugin calls/callback |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 64 | serial | 0.0235 | 0.0213 | 0.0461 | 0.0892 | 1.76% | 0.719 | 256 | 0.5 |
| 128 | serial | 0.0465 | 0.0484 | 0.0735 | 0.1852 | 1.74% | 0.709 | 256 | 1.0 |
| 256 | serial | 0.0899 | 0.0934 | 0.1302 | 0.2819 | 1.69% | 0.702 | 256 | 2.0 |
| 512 | serial | 0.1837 | 0.1916 | 0.2583 | 0.5791 | 1.72% | 0.694 | 256 | 4.0 |
| 64 | M5(4) | 0.0275 | 0.0263 | 0.0442 | 0.1585 | 2.06% | 0.962 | 256 | 0.5 |
| 128 | M5(4) | 0.0468 | 0.0461 | 0.0665 | 0.2497 | 1.75% | 0.946 | 256 | 1.0 |
| 256 | M5(4) | 0.0853 | 0.0862 | 0.1148 | 0.3296 | 1.60% | 0.922 | 256 | 2.0 |
| 512 | M5(4) | 0.1546 | 0.1565 | 0.2075 | 0.2331 | 1.45% | 0.852 | 256 | 4.0 |

The null backend honoured every requested block (`device.blockSize == request`, asserted). Latency
`tracksSamples = 256` (bass: synth gather 128 + gain gather 128; the others wait for it) is CONSTANT
across all 8 runs — the gather does not depend on the device block. Plugin-call arithmetic: 4 instances
(3 synth + 1 gain) × deliveredFrames/128 ≈ 4500 calls per 3 s window in every scenario (≈3010 in the 2 s
gate runs) — the call count is set by the SAMPLES, not by the device block.

### 2.3 Verdict (separate from A)

**The fixed 128 block adds constant gather LATENCY, not extra plugin calls at small device blocks.**

- At device 64 the host does NOT split into extra calls: it processes 64-sample callbacks into a
  128-capacity gather and calls the plugin ONCE PER TWO callbacks (0.5/callback) — HALF the call rate a
  matched 64 block would have. The price is the fixed +128-sample gather latency (2.67 ms @ 48 kHz) on
  EVERY plugin path, even zero-latency plugins, at EVERY device block.
- The per-sample gather/scatter copies (`:149-153`) happen at every device block regardless (the graph's
  32-sample sub-blocks never match the plugin block); they are sample-proportional, not call-proportional.
- The per-CALL fixed overhead (`view.clear`, `copyFrom`/copy-out of 128 floats ×2, MidiBuffer clear, the
  JUCE VST3 wrapper + `processMutex`) is amortized over 128 samples. At device 256/512 a matched block
  would halve/quarter the call count (2×/4× fewer per-callback fixed costs); today those calls still
  happen 2/4 times per callback. With light plugins the effect is small (serial load flat ≈1.7% across
  blocks; M5@512 already 0.1546 vs 0.1837 ms serial); with per-call-heavy plugins (FFTs, oversampling)
  it is the dominant term — exactly what the export knob's comment defers «until measured with the
  plugins of the house».
- So (B) is a latency + amortization POLICY limitation, not a call multiplier; (A) is a gate-position
  CPU defect. Different files (`juce_host.cpp` vs `graph.cpp`), different acceptance tests. Kept separate
  as the owner required.

Evidence: `summary.json` + `b-*.json` (8 rows), `gate-run.log`, `measure-run.log`.

## 3. The four covers (preconditions BEFORE suspending muted tracks)

### C1 — TAILS

- Current semantics (code): mute = immediate ramped silence at the fader (`graph.cpp:1331-1351`); the
  tail is still COMPUTED (instrument release, delay/reverb lines keep running — §1 pins) but never
  heard. There is no tail-out on mute. Stop, by contrast, fades 10 ms then cuts tails
  (`server.cpp:267-286`, `resetAll` + `keepTails=false`).
- Measured (capture seam, `covers.json` `c1c4Serial`/`c1c4M5`): mute mid-note → silence in **31 samples**
  (one aligned 32-sample ramp), `rmsMutedWindow = 0.0`. With the out-of-repo CoreTestDelayProbe
  (480-sample line): a 0.5 s note's tail is audible in the reference (`tailRmsReference = 0.2278` over
  samples 24128–25568); muted over the tail window → 0.0; **after unmuting at 0.75 s the mute run is
  SILENT (`postUnmuteRmsMuteRun = 0.0`, `tailDrainedWhileMuted = true`)** — the line DRAINED WHILE MUTED
  because processing continued.
- What a suspend design must handle: a frozen line releases a STALE tail on resume, time-shifted by the
  suspend duration (the exact opposite of the measured drain). Options: (a) drain-then-suspend (keep
  processing until the tail ends — needs `getTailLengthSeconds()`, which every repo test plugin declares
  as 0; real reverbs/delays declare seconds), (b) immediate freeze + flush on resume (click risk, tail
  lost), (c) owner-defined tail-out semantics. **Owner decision required: does a muted track's tail still
  sound?** Today the answer is NO (immediate silence); suspend must not change that answer silently.
- Needs a real plugin: infinite/feedback tails (EchoBoy/Decapitator — the VST3 front tests them in
  parallel; NOT touched here). CoreTestDelayProbe is a pure finite line (no feedback).

### C2 — SIDECHAIN

- Contract (code): the key is captured AFTER the source's inserts, BEFORE its fader and mute
  (`graph.cpp:1328`), with the documented semantics «a muted "ghost" kick still ducks»
  (`project.cpp:509-510`). Ordering (`graph.cpp:571-608`) and key/program delay compensation
  (`:617-671`) exist. **Suspending a muted key source kills the sidechain signal — the contract forbids
  it unless the owner changes the semantics.**
- Current state (code, grep-proven): the M7c path is DORMANT for plugins — `Effect::sidechain` is never
  assigned anywhere (only the default −1, `dsp/effects.hpp:54`); `PluginEffect::process` passes no key to
  the processor (`graph.cpp:247`); `JuceProcessor` has no aux-input wiring. The JSON field passes through
  for plugin effects (`project.cpp:474-480`) and `validateRouting` checks it (`:128-140`), but no effect
  consumes it (the built-in compressor that used it left on 2026-11-16).
- Measured (`covers.json` `c2`, gate case): with the kick muted in both captures, the sidechain-field
  song is **BIT-IDENTICAL** to the no-field song (memcmp, 1 s); latency equal (256/256). Three routing
  negative controls THROW (self-key, bus-key, missing-track-key) — validation is alive.
- Design consequence: today suspending a muted track cannot break any audible sidechain (none exists);
  the suspend design must STILL exempt key sources (keep them processing, or process them at reduced
  rate) so the pre-mute key contract holds when plugin sidechain hosting lands. The exemption must be
  computed from the routing map at graph build, not from the mute flag alone.

### C3 — LATENCY

- Code: the PDC map is static per graph build (`graph.cpp:610-672`); each plugin path reports
  `instance latency + 128 gather` (`juce_host.cpp:162`); `checkPluginLatency` rebuilds when a reported
  latency changes (`server.cpp:288-292`). Mute does not touch `latencySamples`; bypass DOES
  (`graph.cpp:539` — a bypassed insert drops out of the map).
- Measured (`covers.json` `c3`, `c3Delay`, gate case): reference song `tracksSamples = 256` INVARIANT
  across 7 states (unmuted, mute-kick, mute-bass, mute-lead, unmute-kick, unmute-bass, unmute-lead);
  delay song `tracksSamples = 736` (= 128 synth gather + 480 declared + 128 delay gather) INVARIANT
  across mute/unmute. Also invariant across device blocks 64/128/256/512 (§2.2).
- Design consequence: suspend must keep `latencySamples()` CONSTANT (keep the instance and its gather
  block; do not report 0 while suspended). If reported latency changed with mute, every rebuild would
  resize/shift ALL delay lines (`graph.cpp:641-649`) — alignment clicks and a moving `engine.latency`
  the app displays. The measured invariance is the acceptance criterion.

### C4 — CLICK-FREE RESUME

- Code: the instance stays warm across the mute rebuild (shared_ptr; `PluginInstrument::adopt` copies
  held pitches + pending note-offs, `graph.cpp:190-211`; fader ramp state adopted `:856-857`); the fader
  ramps over an aligned 32-sample sub-block (`:1337-1351`). No reset, no discontinuity by construction.
- Measured (`covers.json` `c1c4Serial`/`c1c4M5`, gate case, 3 s captures, mute@1 s / unmute@2 s):
  - ramp to silence **31 samples**, ramp to signal **0 samples** (swap lands on the ramp start);
  - max inter-sample step in the transition windows ≤ **1.21×** the signal's own steady-state max slew
    (serial mute window 0.01321 vs 0.01087; M5 1.00×; a hard cut of this sine would be ≈23×);
  - **steady-state resume BIT-EXACT with the uninterrupted reference** (last 0.5 s, memcmp, serial AND
    M5(4)) — the plugin kept processing while muted, so phase/state continuity is exact;
  - peak after resume = reference peak (0.25197).
- Design consequence: suspend must preserve warm state (never `reset()`/`releaseResources()` on
  suspend), resume through the SAME adopted fader ramp, and keep note accounting: note-offs sequenced
  while suspended must reach the plugin (or held pitches be re-triggered as `adopt` does for a fresh
  instance, `:205-210`) or notes hang on resume. The bit-exact warm-resume test is the acceptance
  criterion for the resume path; a suspend that freezes state must prove an equivalent continuity
  (phase-coherent resume or a ramped crossfade) — measured, not assumed.

## 4. Proposals (NOT implemented — two separate designs, owner decision gates)

### P-F-S9-A — mute-suspend (defect A; `graph.cpp` gate position)

Sketch: consult a suspend flag BEFORE the instrument/inserts (move the gate upstream of `:1266`/`:1289`
for the suspended case), with the four covers as hard constraints:
1. **Tails**: drain-then-suspend — keep processing until the declared tail
   (`getTailLengthSeconds()`, host-side query) has drained, then skip; or the owner's tail-out policy.
   Immediate freeze is rejected by C1 (stale tail on resume).
2. **Sidechain**: a track that keys another track's effect (`keySource`, routing map at build) is NEVER
   suspended while the keyed effect is active — the pre-mute «ghost kick» contract (C2).
3. **Latency**: `latencySamples()` unchanged while suspended (instance + gather kept); PDC map identical
   (C3 acceptance: the existing invariance case).
4. **Resume**: warm instance, adopted fader ramp, note-off accounting during suspend (no hanging notes);
   acceptance = the existing C1+C4 bit-exact warm-resume + slew cases, extended with a suspend interval.
M5: suspended tracks submit no worker job (frees slots when tracks > workers); serial: the sub-block
skips instrument+inserts, keeps fader/ramp/key bookkeeping. `engine.cpu` then reports ≈0 for suspended
tracks (the meter already measures only what runs).
Regression sketch: flip the §1 defect pins to savings assertions (suspended track cpuPct ≈ 0 except
key sources / tail drain); serial meanMs(muteN) drops by ≈ the muted share; tail-drain probe case
(postUnmuteRms ≈ 0 after drain); ghost-kick capture case when plugin sidechains land; C3/C4 cases
unchanged as acceptance; note-hang case (note-off during suspend); m5 bit-exact matrix with muted tracks.
**Expected gain (from §1 numbers): the FULL processing cost of each muted track** — with the test
plugins ≈0.0061/0.0076/0.0052 ms/block (serial kick/bass/lead, 0.71% of one core for all three); M5
0.0254 ms/block of parallel work (wall gain when tracks > workers). With real plugins the gain scales to
each track's measured share (the ratio ≈1.0 defect is plugin-independent). Zero gain today: mute saves
nothing.

### P-F-S9-B — block-size matching (limitation B; `juce_host.cpp` policy)

Sketch: make the live gather block a policy instead of a compile-time constant. Two variants for the
owner to choose AFTER real-plugin measurement (the export knob's own comment defers exactly this):
- **Upward-only match** `block = clamp(deviceBlock, 128, 512)`: latency never worse than today (128 floor
  at device ≤128); at device 256/512 the plugin call count drops 2×/4× (per-call fixed overhead amortized
  over more samples). Gather latency grows to 256/512 at those device blocks (+2.67/+5.33 ms) — the
  trade-off the owner must accept for live monitoring at large buffers.
- **Full match** `block = clamp(deviceBlock, 32, 512)`: at device 64 latency halves (128→64, −1.33 ms)
  but the call rate doubles (per-call overhead up); at 256/512 same amortization gain as above.
Either way: `prepareToPlay(sampleRate, block)` honesty, `latencySamples() = instance + actual gather`,
PDC rebuild on device-block change (already happens via `engine.start`), export knob untouched, M5
bit-exact re-proven per block.
Regression sketch: latency report per device block (instance + actual gather); m5 bit-exact matrix at
64/128/256/512 with the matched block; render-time A/B vs fixed-128 with the house's real plugins
(per-call-heavy plugins are where the gain lives); `kMaxBlock`/prepare bounds; export path unchanged.
**Expected gain (from §2 numbers): modest with light plugins** (serial load flat ≈1.7% across blocks;
M5@512 0.1546 vs serial 0.1837 ms already), **up to ≈4× fewer plugin calls at device 512** for
per-call-heavy real plugins; the latency variant is a monitoring-latency decision, not a CPU fix.
**This is NOT the mute defect and does not fix it** — the two stay separate (owner contract).

## 5. Alterações

| File | Change | Kind |
| --- | --- | --- |
| `tests/s9_mute_block_tests.cpp` | NEW permanent characterization suite (5 gate cases + `--measure`/`--covers` evidence modes); reuses `core::rtProbe` observer + capture seam; defect pins declared | implementation (tests only) |
| `tests/CMakeLists.txt` | ONE additive S9 registration block after the S7 block (merge, TIMEOUT 600); the six plugin registration blocks and every existing entry preserved | implementation (tests only) |
| `docs/relatorios/2026-11-20-s9-mute-block.md` | this report | documentation |
| `docs/relatorios/README.md` | one index row appended before «Acrescentar» (all rows preserved) | documentation |
| `docs/relatorios/evidencia/2026-11-20-s9-mute-block/` | 33 files: 18 scenario JSONs + summary + covers + 5 raw captures + 4 run logs + recipe bats + out-of-repo delay-proj CMakeLists | evidence |
| `D:\Core-build\s9\` (outside repo) | own build tree, delay-probe project/build, notes | probe |

NO engine source change (`graph.cpp`, `juce_host.cpp`, `device.*` untouched). NO VST3-front file touched.

## 6. Verificação

| Command | Result |
| --- | --- |
| `configure.bat` (vcvars64 + VSLANG=1033 + chcp 65001; Release/Ninja/TESTS=ON/APP=ON/local JUCE) | rc=0; D113 full-patch reverse check PASSED |
| `cmake --build D:\Core-build\s9\build --parallel 6` | rc=0, 575 targets (`build.log`) |
| `s9_mute_block_tests.exe` (gate) | rc=0, **5 cases / 194 checks / 0 failures** (`gate-run.log`) |
| `s9_mute_block_tests.exe --measure <ev> 3` | rc=0, 18 scenarios (`measure-run.log`, `summary.json`) |
| `s9_mute_block_tests.exe --covers <ev> <delay.vst3>` | rc=0 (`covers-run.log`, `covers.json`, 5 `.f32`) |
| `delay-build.bat` (out-of-repo CoreTestDelayProbe) | rc=0, 56 targets |
| `ctest -C Release -L merge --output-on-failure --parallel 6` | **28/28, 100% passed** (S7's 27 + s9 72.27 s; 166.15 s*proc / 80.05 s real; `ctest-merge.log`) |

Counts per scope: merge 27 → **28** (+1 additive entry, nothing removed/weakened); the s9 suite itself
5/194/0. No red→green claim: this wave implements no fix; the defect pins assert the CURRENT (defective)
behavior on purpose and must be updated consciously when an authorized suspend change lands
(characterization, not a quality gate — no existing criterion is relaxed).

## 7. Pendências e passagem (proposed ESTADO lines — the escrivão writes them, D72/D80)

- *Estado medido — S9 (2026-11-20, a9a0384 + round-1 locals, own Release build, null backend 48 kHz)*:
  **CSV-09 confirmed + quantified, two separate problems.** (A) Mute is a fader-only gate
  (`graph.cpp:1329-1331`, after instrument/clips/inserts/key): muted tracks pay FULL CPU — per-track
  muted/unmuted ratios ≈1.0 serial AND M5(4); muting all 3 reference tracks saves nothing (serial
  totalPct 0.659→0.711, meanMs 0.0422→0.0459; M5 0.950→0.952, 0.0461→0.0462); wasted ≈0.0061/0.0076/
  0.0052 ms per 128-block (kick/bass/lead, 0.71% of one core total) with the light test plugins — the
  waste equals each track's full cost and scales with real plugin weight; bypass contrast: gainfx
  0.0704→0.0000 (insert skipped, `:1300`). (B) Live plugin block compile-time 128 (`juce_host.cpp:23`,
  export-only 256/512 knob `:26-40`): device sweep 64/128/256/512 serial+M5 — **constant +128-sample
  gather latency on every plugin path (tracksSamples 256 invariant across all 8 runs), NOT extra calls
  at small blocks** (0.5/1/2/4 calls per callback; ≈4500 calls/3 s set by samples); per-call overhead
  amortization missed at device ≥256. Four covers characterized + measured: tails drain WHILE muted
  (delay probe tailRms 0.2278→0, postUnmute 0, `tailDrainedWhileMuted=true` — suspend would release a
  stale tail); sidechain M7c pre-mute key contract («ghost kick», `project.cpp:509-510`) DORMANT for
  plugins (field bit-identical, 3 routing negative controls throw); PDC invariant under 7 mute states
  (256) and delay song (736); resume click-free (31-sample ramp, transition slew ≤1.21× steady) and
  steady-state BIT-EXACT warm vs uninterrupted reference, serial AND M5. Permanent suite
  `s9_mute_block_tests` 5/194/0 (defect pins declared); merge Release **28/28** (+1 additive, six plugin
  registration blocks preserved). NO engine change; suspend/block-match are PROPOSALS (P-F-S9-A/B)
  awaiting owner decision. Evidence: `evidencia/2026-11-20-s9-mute-block/` (33 files) + `D:\Core-build\s9\`.
- *Pendências*: (1) **BUG-S9-MUTE-CPU** (CSV-09 A, D75 namespace) — owner decision: authorize
  mute-suspend (P-F-S9-A) with the four covers as acceptance + the TAIL POLICY question (does a muted
  track's tail still sound? today: immediate silence, tail computed not heard); numbers ready.
  (2) **P-F-S9-BLOCK** (CSV-09 B) — owner decision on the block policy (upward-only match vs full match
  vs keep 128): latency-vs-amortization trade-off, to be measured with the house's real plugins first
  (the export knob's own comment defers this); NOT the mute defect, kept separate. (3) Sidechain
  hosting (M7c dormant): product decision when plugin sidechains are wanted; the suspend exemption for
  key sources must land with it. (4) Real-plugin re-measurement of the mute waste + block policy
  (EchoBoy/Decapitator — the VST3 front tests them in parallel; this wave did NOT touch its files/build).
  (5) Commit S9 (owner). (6) The defect pins in `s9_mute_block_tests` must be updated consciously when
  an authorized suspend change lands (declared in the suite header). No new D numbers proposed — nothing
  here is a ratified decision (D48/D50: silence ≠ approval).

## 8. Limitações

- Null backend: no real-device pacing/hardware underruns (S7's real-device baseline owns that); absolute
  CPU values depend on machine load (the ≈1.0 ratios and invariances are the robust claims).
- Light deterministic test plugins: the waste is structural (ratio ≈1.0) but the absolute ms/% are the
  test-plugin floor; real-plugin numbers pending (VST3 front, not touched).
- CoreTestDelayProbe is a pure finite 480-sample line (no feedback); infinite/reverb tails
  (`getTailLengthSeconds() > 0`) NOT measured — every repo test plugin declares tail 0.
- Sidechain is dormant for plugins: C2 measured at the graph/JSON level (bit-identical + validation);
  there is no audible key path to break today.
- Suspend is NOT implemented: the covers characterize the CURRENT path + what a suspend design must
  handle; the resume transient measured is today's warm path, not a future suspend path.
- M5(4) only (not 2/8); 48 kHz only; Windows Release only; capture actions have ±1-block control jitter
  (the exact swaps are recorded in `covers.json`); 3 s steady-state windows (no long-session drift).
- The gate's defect pins assert the defect (>0 muted-track CPU) on purpose — characterization, not a
  quality gate; they go red when a fix lands and must be updated with the new numbers (nothing relaxed).

## Evidence

[evidencia/2026-11-20-s9-mute-block/](evidencia/2026-11-20-s9-mute-block/) (33 files): `summary.json` +
18 per-scenario JSONs (`a-*` mute matrix incl. bypass contrast, `b-*` device-block sweep; device/stats/
cpu/timing/latency per row), `covers.json` (C1–C4: ramps, slews, bit-exactness, tail RMS, latency
invariance, sidechain dormancy), 5 raw captures (`c1-reference.f32`, `c1-mute-serial.f32`,
`c1-mute-m5.f32`, `c1-delay-reference.f32`, `c1-delay-mute-over-tail.f32`; L then R, float32, 48 kHz),
`gate-run.log` (5/194/0), `measure-run.log`, `covers-run.log`, `ctest-merge.log` (28/28),
`configure.bat`/`build.bat`/`delay-build.bat` (recipe), `delay-proj-CMakeLists.txt` (out-of-repo probe).
Build/probe trees: `D:\Core-build\s9\` (build, delay-build, logs); step log
`D:\Core-build\coordinator-round2\s9-notes.md`.
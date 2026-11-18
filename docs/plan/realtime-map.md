> Translated from `tempo-real.pt.md`, a study of commit e7968c8. Line numbers refer to e7968c8 and have drifted since; they are kept as written and not re-anchored to current HEAD.
>
> Verdict legend (1:1, none upgraded or downgraded): **confirmado por leitura** = confirmed by reading · **confirmado por execução** = confirmed by execution · **medido** = measured · **não medido** = not measured · **não decidido** = not decided · **latente** = latent · **refutado** = refuted · **corrigido** = fixed · **confirmado** = confirmed · **suspeita** = suspected · **por leitura** = by reading · **não verificado** = not verified. The owner's quotes stay verbatim in Portuguese.

> Delivery of the subagent `mapa-rt` (DSH session 2ff54f03, 2026-11-18), extracted from the log without editing. Read-only study of commit e7968c8, with offline measurements. In Portuguese; to be translated into English before entering the repo.

# Real-time audio path map — delivery with what was established (HEAD e7968c8)

I deliver incomplete: M5 was assessed by reading only, and the live measurements (device pace, Stop, edits) never ran. I did not touch the repo; everything I created is in `%TEMP%\core-mapa-rt\`. I wrote no report in `docs/relatorios` (your order was read-only).

Two premises of the request are outdated:
- **Native effects no longer exist.** `engine/src/dsp/effects.cpp:10` «std::unique_ptr<Effect> makeEffect(const std::string&, double, double, int) { return nullptr; }». There is no «com/sem efeitos nativos» (with/without native effects) to measure.
- **The sidechain path is dead code.** Only builtin effects receive `sidechain` (`graph.cpp:324`), and there are none left.

## 1. Defects and risks (by severity)

**1. A crashing plugin leaves a lock held forever; later the engine blocks — bug (data)**
- **Where:** `juce_host.cpp:64-66` catches the crash with `__except (1)`. JUCE processes with `juce_VST3PluginFormatImpl.h:2422` «const SpinLock::ScopedLockType processLock (processMutex);» and destruction goes through `:2393` (`releaseResources`, same lock), called at `:2181`.
- **Mechanism:** the build is `/EHsc`, and with `/EHsc` an access violation does not run destructors. The lock stays taken, and `juce_Thread.cpp:440-441` «while (! tryEnter()) Thread::yield();» spins forever on the message thread.
- **Scenario:** a plugin crashes and is silenced (M3b). Then the user removes the track, loads another song, retries or changes the rate. The instance is destroyed (`juce_host.cpp:101-106`) and the message thread gets stuck. The next `runSync` never returns: `plugins.states` (save), the Stop (`server.cpp:236`), or loading a plugin. After a Stop the output goes mute.
- **Status:** mechanism measured in isolation with the same compiler and flags (AV: lock stays held; C++ `throw`: it is released) and confirmed by reading. Not reproduced end to end with a real VST3.
- **Minimal fix:** in `~JuceProcessor`, if `failure()==Crash`, do not call `releaseResources()` or `delete` (let the dead instance leak).
- **Proof:** a test VST3 that dereferences null in `process`; remove the track; `plugins.states` must answer.
- **Risk:** low.

**2. FL-3 confirmed by execution, worse than described — bug**
- **Where:** `server.cpp:336-339` only keeps the engine when `otherEngine`; `:357-360` replaces the engine when the device picks another rate; `:373-375` restores `sampleRate_` with no engine to restore. `device.cpp:180-185` returns only `{sampleRate}` without starting when the rate does not match.
- **Measured** (real Server, fake output reproducing `device.cpp:180-185`): the start fails with «device busy»; the engine stays at 44100 and the server at 48000. The next two `engine.start` answer `ok:true` with only `{sampleRate:48000}` and no device running.
- **Scenario:** switch to a device of another rate whose second open fails. It stays without sound, the start says OK, and it only gets out with another rate or another `liveThreads`.
- **Minimal fix:** keep the engine in the `:357` branch too, and treat a `start` answer without `blockSize` on the second attempt as an error.
- **Proof:** test in `tests/engine_tests.cpp` with a fake `OutputFactory` (my `driver.cpp`, `fl3` mode, serves as a template).
- **Risk:** low.

**3. Every Stop makes the audio thread wait for the plugins' reset — glitch risk (serial), functional bug (M5)**
- **Where:** `juce_host.cpp:361-377`; the comment at `:367-368` assumes «the audio thread waits for it». JUCE does `reset()` at `:2811-2820` with `setActive(false/true)` under the same `processMutex` that `processBlock` takes blocking (`:2422`, it is not a try-lock).
- **Serial:** the output is already fading (`server.cpp:229-233`), so it is inaudible, but it counts as an overrun.
- **M5:** a worker stuck for more than 2 buffers is a `liveStall`, and the engine goes serial forever (`engine.cpp:193-197`). A Stop with a slow-reset plugin turns M5 off until restart.
- **Status:** confirmed by reading; reset duration not measured.
- **Minimal fix:** before `resetAll`, set the graph to not call plugins (a flag acknowledged by the audio thread) and only then do the reset.
- **Proof:** soak with periodic `transport.stop`, reading `overruns`, `liveStalls` and `liveFallback`.

**4. One late worker turns M5 off for the session, without warning — glitch risk**
- **Where:** `engine.cpp:193-197`, `engine.hpp:149`.
- **Scenario:** with load that only fits in parallel, the fallback means continuous overruns until restart. Measured offline: 8 Twin 3 tracks spend 62 % of one core in serial, 16 spend 149 %.
- **Associated race (ART-010b):** `engine.cpp:349` «timedOffs(frames - done);» calls `g.liveNote` while the late worker may still be on that track. Suspected; I did not construct the interleaving.
- **Minimal fix:** re-arm M5 after N seconds without a stall, report the fallback in the state event, and defer `timedOffs` while `stalled_`.

**5. M5 does not have a single test in the tree — risk**
- In `tests/` there is no occurrence of `RtWorkerPool`, `useLiveWorkers`, `setLiveDeadline` or `testDelayNs`.
- Only `engine_tests.cpp:483-524` touches the subject, and it merely counts threads.
- The comments at `rt_workers.hpp:77-81` and `:105-108` speak of tests that no longer exist.

**6. Mute, solo and idle tracks do not save CPU — waste**
- **Where:** `graph.cpp:1292-1294` «const bool audible = !t.mute && …» only zeroes the fader; instrument and inserts have already run.
- **Measured:** see table C. Mute saves nothing; an instance without notes still costs 22 % (Twin 3) to 37 % (One) of one playing.
- **Fix:** skip instrument and inserts of a muted track (without pre-sends) and suspend plugins in silence after the tail.
- **Risk:** medium (free-running LFOs, tails, export/live equality).
- **How to measure:** `engine.cpu` per track, before and after.

**7. The engine's own cost per track and per 32-sample sub-block — waste**
- **Measured** (no plugins, block 480): about 6 to 7,6 µs per track per callback, that is ≈0,4 µs per track per sub-block (table D).
- **Relative weight:** for a light synthesizer like One (≈0,24 % of real time per track) this is about a quarter of the track's cost.
- **Where it comes from (by reading, not profiled):**
  - `graph.cpp:1294-1298`: `pow`, `cos` and `sin` per sub-block even without a gain or pan change;
  - `:1219`, `:1249`, `:1287`: clock read 2 times plus once per effect;
  - `:947-955`: 8 KB tables on the stack in every `renderTrack`;
  - `:117-126`, `:1232`, `:1247`, `:1283`: several verification passes over the same buffer;
  - `:1312-1336`: ramp in double and sample-by-sample sum.

**8. Plugin block fixed at 128 — waste and latency**
- **Where:** `juce_host.cpp:22`. That is 375 `processBlock` calls per second per plugin (not 1500: the 32-sample sub-block is aggregated), and +128 samples of latency per plugin in series (`:133`).
- **Measured** (export, 512 against 128): One×32 takes 26 % less; Twin×8 takes 13 % less, within the noise.
- **Suspected by reading:** with a device buffer below 128, all plugins fire in the same callback and the peak lands at about twice the average.

## 2. M5

**Is it correct? By reading, the core looks solid; it was not executed.**
- **Pool (`rt_workers.hpp:64-97`, `126-155`):** I walked the interleavings of stale wake, resume `1→0` against start `1→2`, and the `pending` flag. I found none that fails. `job_` and `count_` are published by `state.store(1)`; the semaphore is never released twice.
- **On a missed deadline** (`:94`), the following workers are not resumed, but they find `next_ >= count_` and the Engine waits for `idle()`.
- **Order of the sums:** `graph.cpp:1423-1457` sums in the same order as serial (sends and then output, by `order_`). The state is per track; modulator sources are pre-computed (`graph.cpp:471-492`).
- **Hypothesis refuted by measurement:** the workers' MMCSS init takes 4 to 15 µs, it does not cause a stall on the first block.
- **Minor:**
  - the `FxWatch` analyzer can be written by two threads if the key changes mid-callback (`graph.hpp:216-218`, suspected, only affects the UI spectrum);
  - false sharing in `CpuMeter::ns[]` (`graph.hpp:52-53`).

**What prevents the bit-for-bit comparison today:**
1. **Real plugins are not repeatable.** Twin 3 with 1 track gives different peaks between two runs (0,0158 and 0,0166), and with 1 track the parallel path does not even engage. A fake, deterministic `PluginProvider` is needed (the tests compile without JUCE).
2. **There is no harness.** `loopcheck` compares in dB through the device. My `driver.cpp` proves that Server + fake output + `engine.start{liveThreads}` can be wired and `Engine::process` called without a device; the comparison mode compiles, but it did not run.
3. **`KeepSink` and `DirectSink` are two instantiations of `runTrack` under `/fp:fast`** (`engine/CMakeLists.txt:9`). Equality depends on the compiler generating the same arithmetic in both.
4. **The deadline:** the comparison requires a large `setLiveDeadline`, so no stall spoils it.

**Is the 8 h soak worth it? Not yet.** In this order:
1. deterministic serial-against-M5 test with a fake provider (buses, sends, automation, edits while playing);
2. fix finding 3 (Stop);
3. fix finding 4 (fallback policy and ART-010b);
4. only then the soak, with Stops and edits included (the current one does not Stop).

## 3. The three optimizations with the best gain/risk

1. **M5, after the prerequisites above.** Export with 8 workers, which uses the same division of work, measured 4,8× to 8× (table A). The old measurement in the code gives p99 72 % → 23 % with 4 threads. It is the only path to Twin 3 or equivalent in quantity. Measured with a soak on the null backend, with and without `--live-threads`.
2. **Do not process what is not heard** (finding 6). The gain is proportional to the muted or idle tracks; today mute saves 0 %. Medium risk.
3. **Cut the per-track and per-sub-block cost** (finding 7): gain/pan cache, a single verification pass, skip the clip search when no event is due, clock per callback. I estimate −60 % of that cost, that is about 1,5 % of the budget at 32 tracks and 6 % at 128. Low risk if it stays bit-exact. Measured with the driver's `overhead` mode or with `measure`.

In fourth place comes the larger plugin block (finding 8).

## 4. Verdicts

| Lead | Verdict |
|---|---|
| A-001 | Refuted: `server.cpp:193` stops the output first; `output_` is declared after `engine_` (`server.hpp:124-125`), so it dies first. |
| A-002 | Latent: the only `throw` is `graph.cpp:1421`, unreachable because `graph.cpp:101-106` catches everything; the callback has a catch-all (`device.cpp:92-95`). |
| FL-1 | Fixed (`server.cpp:101`), with a test. |
| FL-3 | Confirmed by execution (finding 2). |
| ART-005 | Not measured (the driver's `edits` mode did not run). |
| ART-007 | Not decided: `graph.cpp:120` uses `std::isfinite` under `/fp:fast`; the `finiteBits` net at `:1232` and `:1283` holds. |
| ART-008 | Confirmed by reading (`engine.cpp:108-111` against `:209-216`); low impact. |
| ART-010 | (a) latent; (b) only with an M5 stall; (c) `engine.cpp:50` waits without limit. |
| ART-011 | Mechanism right, unreachable today: among the 110 plugins in `plugins.json` the maximum is 12 channels (Multipass). |
| ART-012 | Confirmed by reading: mode set per callback (`device.cpp:76`) and per run in the workers (`rt_workers.hpp:144`); export runs without FTZ/DAZ by design (`float_mode.hpp:7`). |
| ART-015 | Latent (`workers.hpp:88` without `try`). |
| ART-017 | Confirmed (`device.cpp:170`, `strstr`). |
| Null-check `graph.cpp:867/870` | `makeInstrument` gives `Silent` for plugin/audio/bus/none (`instruments.cpp:29`); for another name it depends on `fromJson`'s validation, which I did not verify. |

## 5. Raw measurements

**Conditions:** Ryzen 7 7800X3D (8 cores, 16 logical), 48 kHz, binary `D:\Core-build\teste-real`. The machine had another agent compiling since ≈02:05 and another doing renders.

**A. Offline render, 111,7 s song (min. of 2 runs, 01:56–02:27).** I did not record the time of each row; the Twin 16 and 32 ones are certainly after 02:05. The time includes loading the plugins.

| Plugin × tracks | Serial (ms / ×real time) | 8 workers (ms / ×real time) |
|---|---|---|
| One ×1 | 271 / 412,8 | 271 / 412,0 |
| One ×8 | 2 193 / 50,9 | 842 / 132,8 |
| One ×16 | 5 213 / 21,4 | 1 368 / 81,6 |
| One ×32 | 11 348 / 9,8 | 2 351 / 47,5 |
| Twin 3 ×1 | 7 578 / 14,7 | 7 520 / 14,9 |
| Twin 3 ×8 | 71 944 / 1,6 | 12 253 / 9,1 |
| Twin 3 ×16 | 166 566 / 0,7 | 24 027 / 4,6 |
| Twin 3 ×32 | 388 993 / 0,3 | 47 633 / 2,3 |

**B. Fixed load cost** (0,43 s song, 3 runs, ≈02:35): One×32 takes 575 / 578 / 591 ms; Twin×8 takes 1 911 / 1 959 / 2 108 ms. The Twin×32 render reached 4,2 GB of working set.

**C. Serial, 3 runs, ≈02:28, with noise** (best / median / worst, in ms):

| Project | Plugin block | Best | Median | Worst |
|---|---|---|---|---|
| One×32, 56,9 s | 128 | 7 904 | 8 702 | 11 370 |
| One×32 without notes | 128 | 3 282 | 3 433 | 3 699 |
| One×32 | 512 | 5 861 | 5 963 | 6 001 |
| Twin×8, 29,4 s | 128 | 18 743 | 19 540 | 24 036 |
| Twin×8 without notes | 128 | 5 526 | 5 616 | 6 062 |
| Twin×8 muted | 128 | 16 687 | 18 405 | 19 359 |
| Twin×8 | 512 | 16 286 | 19 331 | 19 374 |

The Twin×8 series at 512 is inconsistent (the median matches the one at 128); I draw no conclusion from it.

**D. Engine without plugins, block 480, 20 000 callbacks, best of 3 (≈02:27):**

| Tracks | µs per callback | % of the budget |
|---|---|---|
| 0 | 49,9 | 0,5 |
| 1 | 58,0 | 0,6 |
| 8 | 93,3 | 0,9 |
| 32 | 245,1 | 2,5 |
| 64 | 513,3 | 5,1 |
| 128 | 1 020,8 | 10,2 |

**E. Others:** MMCSS init per worker, 4 to 15 µs (2 executions × 3 rounds × 4 threads). `/EHsc` test: AV leaves `lockStillHeld=1`, `throw` leaves 0.

## To verify

- The driver's `m5cmp`, `stop`, `edits` and `load` modes are compiled and did not run. Without them, missing are: live load at device pace, the real duration of `resetAll`, the cost of each graph swap (ART-005), and the serial/M5 comparison.
- End-to-end reproduction of finding 1 with a real VST3.
- Untested suspicion: a leak of up to 128 samples of old tail when unmuting after Stop. `JuceProcessor`'s `outL_`/`inL_` buffers are not cleared on reset and unmuting has no fade-in (`engine.cpp:370-372`).

## Perfetto, one line each

1. `melatonin_perfetto.h` includes `juce_core`, and `core_engine_lib` has no JUCE: the macros must stay behind a Core-only header.
2. The engine does not always end cleanly: the app closes stdin and kills after 2 s (`engine_client.cpp:150-152`), kills on a protocol error (`:281`, `:287`) and has a kill-on-close job (`:108`); the trace should be written at the end of `serve()`, before unloading plugins, or by command.
3. Do not trace per 32-sample sub-block: that is 1500 events per second per track, which fills the 80 MB fast and can disturb the ≈0,4 µs cost one wants to measure; trace per callback and per track.

Files in `%TEMP%\core-mapa-rt\`:
- `driver\driver.cpp`
- `driver\driver.exe`
- `ehtest\eh.cpp`
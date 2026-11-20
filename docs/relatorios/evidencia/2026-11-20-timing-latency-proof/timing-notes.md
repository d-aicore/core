# Timing / latency-compensation / sound-integrity PROOF — working notes (agent: timing-proof)

Repo: D:\CORE\CORE @ a9a038430d781959829f8510067ce59afffaad62 + uncommitted round-1/S2/S3/S6/S7/S9 fixes (preserve all).
Front: PROOF only — no product edits; defects go to report + proposals. Build: D:\Core-build\timing\build (Release, Ninja, TESTS=ON, APP=ON, JUCE source D:/Core-deps/JUCE-9.0.5).

## Step 0 — kickoff (2026-11-20 ~05:29 Lisbon)
- Plan: (1) read PDC/MIDI/automation code paths in graph.cpp + capture seams in engine.hpp/device.hpp + test harness patterns (m5_bitexact, s7_rt_probe) + S9 CoreTestDelay plugin; (2) configure own build; (3) write out-of-repo probe harness + songs exercising known-delay plugin, MIDI block-edge events, automation block-edge points, kick/bass alignment, render subtraction, two buffer sizes and 44.1/48 kHz if supported; (4) measure sample deviations; (5) report docs/relatorios/2026-11-20-timing-latency-proof.md + evidence dir + index row + proposed ESTADO lines.
- Owner contract focus: kick/bass alignment priority, parallel-chain behaviour gates Chain parallel acceptance.
- Next: git state check + code reading.

## Step 1 — code reading done (05:35 Lisbon)
Git: main @ a9a03843, uncommitted round-1/S2/S3/S6/S7/S9 fixes present (graph.cpp/hpp, device.cpp/hpp, juce_host.cpp, render_job, rt_workers, server.cpp, tests/CMakeLists + new suites m5/s7/s9/s2s3/s4/s1/state_recovery). Nothing touched by me yet.
PDC map (confirmed in code):
- graph.cpp:531-551 collects plugin latency: instrument + non-bypassed inserts per track; master chain separate (masterLatency_). latencies_ snapshot for change detection.
- graph.cpp:610-672 path latency: bus waits for latest input (in[bus]=max(out[feeds])), each output/send gets a delay line (wait = in[dest]-out[i]); sidechain key/program delays (M7c).
- graph.cpp:707-720 preroll(): renders+discards latency_+masterLatency_, shift_=total. EXPORT ONLY (server.cpp:156 renderProject, render_job.cpp:395). Real time does NOT preroll: capture[c] = song[c-latency_] (whole mix plays latency_ late; tracks mutually aligned).
- graph.cpp:1367-1371 per-track delay line applied at the fader output; sends at :1360-1364.
- juce_host.cpp:23,162: EVERY plugin path gathers into fixed 128-sample blocks and reports instance latency + 128. CoreTestSynth track = 128; CoreTestDelay insert = 480+128 = 608; synth+delay track = 736.
- Latency CHANGE supported: Server::checkPluginLatency (server.cpp:288-292) compares live latencySamples() vs snapshot, rebuild() on change; serve ticker calls it every 40 ms (main.cpp:155). adopt() (graph.cpp:868-896) carries delay-line content only when sizes match.
- MIDI: renderTrack (graph.cpp:969-1066) schedules events at sampleOfTick (llround), splits instrument render at exact offsets; stereo plugins get queueNote(at) -> JuceProcessor::addNote at exact gathered-block offset (juce_host.cpp:148,185-194). EventLog records scheduled sample (measurement seam).
- Automation: read once per ALIGNED 32-sample sub-block (step(), graph.cpp:1146-1167, kSubBlock=32); gainDb/pan -> linear fader ramp across the sub-block (:1347-1352); plugin params (vst:) glide smoothCoef_=1-exp(-32/(0.015*sr)) per sub-block then setParameter (:769-777), effective at plugin-block granularity. valueAt = linear interpolation between points (graph.cpp:17-25).
- Parallel chains: M7a bus routing IS implemented (output/sends -> bus, PDC recombination above). M7c sidechain DORMANT for plugins (S9 report: Effect::sidechain never set on PluginEffect; bit-identical measured).
Harness plan (S9 precedent): additive tests/timing_proof_tests.cpp (fake-provider offline cases always; real-VST3 cases when test plugins built; --evidence mode writes JSON/f32/WAV + probe songs). Out-of-repo configurable delay probe plugin at D:\Core-build\timing\probe-plugin (env: actual delay, reported latency, runtime change) for before/after/change measurements; reuse S9 CoreTestDelayProbe (480 fixed) as the canonical known-delay plugin. Null backend: Server(makeMiniaudioOutput, SR, &host), engine.start {backend:null, blockSize}; SR from Server ctor (44100/48000 both possible).
Next: configure own build D:\Core-build\timing\build, then probe plugin + harness.

## Step 2 — DISK BLOCKER + approved adaptation (05:42 Lisbon)
- D: drive 100% full (1.6 MB free). First build (job pwsh-ee36ae0a706c45d4) failed ~308/575: cl C1085 "No space left on device". Environment blocker, NOT a code defect.
- Lead APPROVED: build on C:\Core-build\timing\build (443 GB free), same flags; remove only my own partial D: build; note deviation in report. Lead is freeing superseded D: trees (do NOT touch other agents' dirs).
- Removed my own D:\Core-build\timing\build (1.32 GB) -> D: free 1.32 GB (enough for report+evidence). Reconfigured on C: OK. Full build job pwsh-2926ec806a064983 (log C:\Core-build\timing\build.log). Probe source at C:\Core-build\timing\probe-proj\TimingProbe.cpp.
- Probe plugin: CoreTimingDelay (effect, delay from CORE_TIMING_DELAY_SAMPLES env default 480, runtime "delay" param re-reports latency, gain default 1.0 unity) + CoreTimingClick (instrument, 1-sample impulse at exact note-on samplePosition, 0 latency). Deterministic.
- Measurement design: BEFORE = single-track Graph::process WITHOUT preroll (raw path offset); AFTER = two-track PDC + preroll (renderProject) and real-time null-device capture. Impulse = audio clip (deterministic). MIDI = EventLog scheduling + CoreTimingClick audio onset. Automation = gainDb step near 32-sample sub-block edge. Parallel = M7a bus recombination. Latency change = fake proc + real probe "delay" param -> Server::checkPluginLatency rebuild.
- Main build DONE on C: (575/575, BUILD_EXIT=0, job pwsh-2926ec806a064983 collected). m5/s7/s9/core-engine + CoreTestSynth/Gain VST3 present. D: free 57 GB (lead cleaned).
- Probe plugins build job pwsh-2e6c4eb8d0aa49fe (log C:\Core-build\timing\probe-build.log) -> CoreTimingDelay + CoreTimingClick VST3.
- BPM clamped 40-600 (project.cpp:335). Use 600 BPM -> samples/tick=5 @48k (integer grid) for boundary tests; ideal sample always = graph.sampleOfTick(tick). plugin.param {key,id,value} moves a live param (server.cpp:684) -> real latency-change via probe "delay".
- PluginInstrument (graph.cpp:140) stereo: queueNote(at) exact sub-block offset -> process(notes offset). PluginEffect (:236) process per sub-block. Fake click onset = exact sample (no gather); real click onset = sample+128 gather, PDC-compensated.

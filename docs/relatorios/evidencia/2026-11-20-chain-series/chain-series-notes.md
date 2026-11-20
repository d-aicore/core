# CHAIN SERIES — working notes (owner S-round «Frentes próprias» / stage 6)

Front: PROOF only. No product edits. Repo D:\CORE\CORE @ a9a038430d781959829f8510067ce59afffaad62 + uncommitted round-1/S2/S3/S6/S7/S9 fixes (preserve all).
Build under test: EXISTING Release D:\Core-build\gate-round1d\core-engine.exe (identical tree).
Driver reuse: D:\Core-build\vst3-host\engine_drive.py (serve --backend null, JSON lines).
Evidence dir: D:\Core-build\chain-series\. Report target: docs/relatorios/2026-11-20-chain-series.md.
Parallel/sidechain: characterization ONLY — architecture decision belongs to owner; do NOT implement.

## Step log

### 2026-11-20 ~05:30 WET — START
- Session opened as chain-series subagent (lead). AGENTS.md rules loaded (English in repo; ledger scribe-only; report+index+proposed ESTADO lines; no product edits).
- Plan: (1) read prior VST3 front notes/report + driver; (2) inspect graph.cpp/project.cpp chain architecture; (3) series proof EchoBoy↔Decapitator both orders, same signal/presets, editor-init states; (4) save/reopen incl. duplicate EchoBoy instances; (5) live reorder/bypass transient capture; (6) parallel/sidechain characterization; (7) report + index + evidence copy.
- Next: read vst3-host-notes.md, vst3-host report, engine_drive.py; confirm git SHA/tree state.

### ~05:35 WET — Step 1 done: prior front read, SHA/tree confirmed
- HEAD a9a038430d781959829f8510067ce59afffaad62; uncommitted round-1/S2/S3/S6/S7/S9 set present (git status matches the other fronts' lists). Nothing touched.
- vst3-host-notes.md + report read: editor-init capture condition (param move WHILE editor open → restorable; pure headless → factory defaults), param keys fx:<trackId>:<effectId>:vst:<paramId>, EchoBoy Mix=vst:3, Decapitator Drive=vst:2, driver engine_drive.py understood (serve --backend null, transcripts, numpy WAV analysis).
- Binary: D:\Core-build\gate-round1d\app\desktop\CoreApp_artefacts\Release\core-engine.exe (also engine\core-engine.exe, identical size).

### ~05:45 WET — Step 2 done: ARCHITECTURE MAP (confirmado no código)
- SERIES EXISTS as track inserts + master chain. Document effects-array order = signal order:
  buildChain graph.cpp:325-347 (key "fx:"+owner+":"+id, :329); runTrack graph.cpp:1287-1325 processes
  t.effects sequentially IN PLACE (process at :1319), before fader/pan/mute (:1329+). Max 16/chain
  (project.cpp:459, effects.hpp:18).
- INSTANCE IDENTITY: effect id unique per chain (project.cpp:473 refuses duplicates; ≤40 chars, no ':',
  :471-472). UI assigns fx1,fx2… never reused while present (edit.cpp:937-943 nextEffectId;
  addPluginEffect :978-984). Key fx:<trackId>:<effectId> (graph.cpp:329, edit.cpp:1002-1005 pluginKey).
  JuceHost::get caches live instances PER KEY (juce_host.cpp:382-422, impl_->live[key], identity
  file|id|stateId). Two EchoBoys = two ids = two keys = two independent instances. Offline render makes
  FRESH instances from document state (server.cpp:150, offline=true skips the cache).
- VISUAL ORDER: Mixer strip inserts + track/master menus list doc::effectChain in document order
  (Mixer.cpp:311-321, MainComponent.cpp:4731-4744, edit.cpp:914-925 effectMenuEntries) → visual = signal
  order (both document order). BUT doc::moveEffect (edit.cpp:1211-1217) has NO UI caller (repo-wide grep:
  only definition/declaration) → no reorder gesture in the app today; reorder possible via document edit
  (protocol track.set patch.effects, v2; server.cpp:587-608 ignores effects only for v1).
- BYPASS: Effect.bypass kept in chain, skipped at graph.cpp:1300 (place + tail stay; effects.hpp:28);
  bypassed insert drops from PDC map (:539); rebuild keepTails=true adopts by effect ID (adoptChain
  graph.cpp:830-847); PluginEffect::adopt no-op — the host-cached instance itself goes on (:256).
  Latency reported = instance latency + 128 gather (juce_host.cpp:162).
- REBUILD on edit: commit → new Graph, same host instances (shared_ptr per key), ramp/glide/tail state
  adopted (graph.cpp:848-889). Mute = fader-only ramp AFTER all processing (S9 CSV-09, :1329-1351).
- PARALLEL: graph.cpp parallel_ = TRACK-level multicore (M5 workers, :1422-1580), NOT effect branches.
  No split/merge inside a chain. Track-level parallel routing EXISTS: sends (pre/post, ramped :1342-1359)
  + buses (M7a, output/sends, topological order :552-608) + per-path PDC (:610-672: each path delayed to
  the latest at its destination). Effect-level parallel plugin routing: NOT implemented.
- SIDECHAIN: M7c machinery complete in graph (key capture post-inserts pre-fader/mute :1328, keySource/
  keyed ordering :571-608, program/key delay compensation :617-671, keyL/keyR effects.hpp:52-54) but
  DORMANT for plugins: buildChain never assigns Effect::sidechain for plugin effects (:328-341);
  PluginEffect::process passes nullptr key (:247); JuceProcessor has no aux-input wiring (create sets only
  main buses stereo, juce_host.cpp:343-346). JSON sidechain field passes through for plugin effects
  (project.cpp:474-480) and validateRouting checks it (:128-140: not self, not bus, must exist); only the
  removed built-in compressor could carry it semantically (:511-514). S9 C2 measured bit-identical dormancy.
- LIVE CAPTURE: Engine::startCapture seam (engine.hpp:126-134, copies after live-worker mix, before ear
  guard) is NOT exposed by serve (no capture command). S9/m5 use in-process harnesses. Plan: out-of-repo
  C++ harness compiled against gate-round1d static libs (exact m5_bitexact_tests recipe from build.ninja:
  /EHsc /O2 /Ob2 /DNDEBUG -std:c++20 -MT /utf-8, -DJUCE_MODAL_LOOPS_PERMITTED=1, includes engine/src +
  engine/third_party + tests; link core_plugin_host.lib core_engine_lib.lib miniaudio_impl.lib dbghelp.lib
  + default win libs; + engine/src/io/device.cpp). No repo edits, no rebuild of the product.
- project.load states arg writes blobs INLINE into the document (server.cpp:509-518) → offline render
  uses them; statesNeeded gate for hasState projections (:519-534).
- Next: series proof script (editor-init states, both orders, contrasts), then save/reopen + duplicates.

### ~05:42 WET — Step 3 done: SERIES PROOF + SAVE/REOPEN + DUPLICATES (chain_series.py, all green)
- Probe: D:\Core-build\chain-series\chain_series.py (reuses engine_drive.py, WORK redirected). One CoreTestSynth
  track, one 125 ms sine note (tick 0, len 240, pitch 60, vel 0.9), bpm 120, 48 kHz/24-bit, tailMs 8000 (10 s files).
  Presets (editor-init, param move WHILE editor open): EchoBoy Mix=1.0 (text 100.0) + Feedback=0.6 (text 0.75),
  delay default 1/8; Decapitator Drive=0.7 (text 7.0). SAME signal + SAME presets both orders.
- ORDER A (eb→dec) sha 2c62e77c… peak 0.28104 rms 0.04872; ORDER B (dec→eb) sha 097c4258… peak 0.272843
  rms 0.026411. A≠B: max sample diff 0.269154. Order signatures: A echo peaks 0.27/0.53/0.84/1.10… tail to
  6.3834 s (distortion sustains the echo train); B peaks 0.28/0.53/0.78/1.03… clean 0.25 s spacings, tail 5.5299 s.
- BYPASS-ONE CONTRASTS (bit-identical, sha256): A_bypassEB == dec_only (80aa28c3…), A_bypassDEC == eb_only
  (21467242…), same for B. Each plugin's effect present: A vs A_bypassEB diff 0.288087, A vs A_bypassDEC 0.201294.
  Dry sha f19ef6d4… ends exactly 0.145 s.
- SAVE/REOPEN (FRESH engine processes, inline editor-init states in saved docs): reopen-A params Mix 100.0 /
  FB 0.75 / Drive 7.0 recalled, render BIT-IDENTICAL to pre-save A; reopen-B same; both missing=[].
- DUPLICATE EchoBoy (eb1, eb2): distinct keys fx:src:eb1 / fx:src:eb2, distinct blobs (0075fa5f… / e8e5cf09…).
  Independence: eb1 Mix moved to 0.2 (editor open) while eb2 stayed 0.5/0.38. Reopen: eb1 Mix 20.0 FB 0.75,
  eb2 Mix 50.0 FB 0.38 — independent states survive; dup render BIT-IDENTICAL pre/post save (3986c8d9…).
- SIDECHAIN DORMANCY (real plugins, my confirmation of S9 C2): dec effect with sidechain:"key" render
  BIT-IDENTICAL to no-field (463dba49…); negative controls self/bus/missing all THROW at project.load.
- Engines all exit 0. Results: chain-series-results.json; WAVs + saved docs + states in probe dir.
- Next: live reorder/bypass capture harness (C++ against gate-round1d libs) + parallel partial-path impulse measurement.

### ~06:02 WET — Step 4 done: LIVE REORDER + BYPASS (out-of-repo C++ harness, capture seam)
- Harness: D:\Core-build\chain-series\chain_live_probe.cpp + build_probe.bat — compiled with the EXACT
  m5_bitexact_tests recipe (build.ninja) against gate-round1d static libs (core_plugin_host/core_engine_lib/
  miniaudio_impl + dbghelp + win libs), + engine/src/io/device.cpp. NO repo edits, NO product rebuild.
  Loads the saved editor-init docs; null backend 48 kHz block 128 listenDb -15; capture 4 s (192000 frames)
  via Engine::startCapture (before ear guard); edits via track.set patch.effects with STATE-PER-EDIT refresh
  (plugins.states before each edit, as the app does). 9 scenarios: refs A/B continuous+impulse, reorder-cont
  (serial + M5(4)), bypassEB/bypassDEC (bypass@1.5 s, unbypass@2.5 s), reorder-imp@0.5 s (mid echo train).
- Two PROBE bugs found+fixed (declared, not engine defects): (1) dangling-else made the unbypass branch dead
  code (first run never unbypassed — latency "stuck" at 318/288 was the probe, engine correct); (2) TestHost
  missed the parameters/setLiveParameter overrides (PluginHost defaults return []) → params read null.
  Fixed runs 2/3: unbypass restores latency 318→478 / 288→478; paramsAfter Mix 100.0 / FB 0.75 / Drive 7.0
  after EVERY edit sequence. Run 1 exit code 1 was a shutdown race; runs 2/3 exit 0 (reproducible).
- STATE: all 5 final offline renders BIT-IDENTICAL to the fresh reference of the same final configuration
  (reorder-cont/reorder-cont-m5/reorder-imp == refBA; bypassEB/bypassDEC after unbypass == refAB) →
  reorder + bypass/unbypass preserve instance identity and state exactly. overruns=0, callbackErrors=0,
  failedPlugins=[], maxEditToAudioMs ≈ 9.0-9.7 ms, graphSwaps 2-3 per scenario.
- CONTINUITY: continuous-note scenarios have ZERO silent windows (no break/dropout); reorder mid-echo-train
  keeps the tail ringing (echo peaks 0.27/0.53/0.78/1.03… continue across the 0.5 s swap, FB decay intact);
  periodic near-silent gaps after reorder = Mix-100% echo-gap structure with silent input (same structure in
  the offline B render), not dropouts.
- TRANSIENTS (the finding): chain edits swap the path at a sub-block boundary with NO ramp/crossfade.
  Max inter-sample step at the swap vs the signal's own steady max slew (0.0273/0.0290):
  run2: reorder-cont 1.89×, m5 2.57×, reorder-imp 3.02×, bypassEB 9.40×/unbypass 13.58×, bypassDEC 2.00×/1.99×;
  run3: reorder 2.57×/0.068×/3.22×, bypass 11.41×/2.01×, unbypass 12.05×/19.43×. Phase-dependent, UNBOUNDED
  by the signal slew (a hard cut of this sine would be ≈23× per S9). Bypass additionally changes reported
  latency (478→318 eb / 478→288 dec; EchoBoy reports 32, Decapitator 62, +128 gather each) → PDC delay lines
  resize (graph.cpp:641-649, adoption only when sizes match :868) — S9 C3's alignment-click risk CONFIRMED
  with real plugins. Mute by contrast ramps (S9 C4: ≤1.21×). Candidate defect/decision: ramp or crossfade
  chain edits (reorder/bypass) — owner decision; NOT implemented here.
- Latency detail: tracksSamples 478 = synth 128 + eb(128+32) + dec(128+62). Reorder keeps 478 (sum unchanged).
- Evidence: live-results.json, live-analysis.json, live-*.raw (float32 L/R), live-*-final.json/.wav, logs.
- Next: parallel partial-path impulse measurement (send→bus→master PDC), then report.

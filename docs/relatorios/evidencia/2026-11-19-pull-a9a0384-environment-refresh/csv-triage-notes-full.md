# CSV triage notes — csv-triage agent (read-only repo triage)

Repo: D:\CORE\CORE @ a9a038430d781959829f8510067ce59afffaad62 (main = origin/main, verified via git rev-parse).
Uncommitted: CMakeLists.txt sanitizer target list (ignored per dispatch) + docs/relatorios/README.md and untracked docs (not code).
Method: reading + git read commands only. Nothing built/run. Statuses: CONFIRMED / PARTIAL / REFUTED / NOT-VERIFIABLE-BY-READING.

Upstream context read: AGENTS.md, README.md, ESTADO.md (full, 665 lines), reports 2026-11-18-engine-rt-fixes.md,
2026-11-18-state-per-edit-root.md, 2026-11-18-plugin-crash-followups.md, 2026-11-18-sweep-batch-05-03.md.

Key upstream facts:
- BUG-SWEEP-05 (ae6c06a, merged): "Try again" for plugins that FAILED WHILE PLAYING (snap_.failedPlugins, plugin.retry).
- BUG-SWEEP-03 (9c457ab, merged): failed-export publish rollback reporting (rollbackAttempted/restored/backups). NOT about success-with-failed-plugins.
- engine-rt-fixes (flawless-m5: da91dab M5 re-arm, 0e45d58 held-note swap, 88aa357 reset watchdog): watchdog test lives in engine_process_tests.cpp, re-arm/swap tests in engine_tests.cpp. M5 serial-vs-multicore full matrix with real plugins = declared follow-up (SUR-16), not done.
- state-per-edit-root (5df584f, merged): projection stateless + statesNeeded handshake. Undo blob-sharing (history.cpp) EXPLICITLY NOT DONE (follow-up slice 1).
- plugin-crash-followups D (cc5b874, merged): vendored JUCE patch applied to pinned D:/Core-deps/JUCE-9.0.5 as working-tree mod; patch tracked at ci/patches/juce-failed-plugin-restart.patch (10 hunks); never git-committed in JUCE; re-fetch loses it.
- ESTADO.md:355 (FL-2 pendência, ronda 18/11 @ f6b86dc) still lists: export success with silent/missing plugin; plugins.missing not announced; relinkPlugins never called; invalid state ignored silently.
- ESTADO.md:357: JUCE patch infra pendência (how bootstrap applies it undecided; false-green risk registered).
- ESTADO.md:356: M5 OFF (D51); soak 8h + bit-a-bit not done.
- ESTADO.md:332 (ART-018): float WAV NaN/Inf silenced in 32-sample sub-blocks; audio_file.cpp lets float in raw; AssetStore does not check.

---

## CSV-01 — CONFIRMED
PT verbatim: «Exportação diz "Exported" com plugin falhado ou não carregado — só verifica áudio em falta.» (render_job.cpp:363-367)
Current evidence:
- engine/src/core/render_job.cpp:363-367 — only pre-write check is `if (!graph.missingAssets().empty()) { ... throw std::runtime_error("Missing audio, nothing exported: " + list); }`. No failed/missing-plugin check anywhere in run() (:336-415).
- engine/src/core/render_job.cpp:417-443 describe(): success emits peak/renderMs/threads only; no plugin-failure field (failure fields are SWEEP-03 rollback only).
- app/desktop/src/MainComponent.cpp:6765 — `if (job.state == "succeeded") notice_ = ... u8("Exported: ") + path;` unconditional on plugin health.
- Failed plugins are silently skipped by the graph: engine/src/core/graph.cpp:1239 `if (!t.instrument->failed())`, :1273 failed effects skipped, :1101 master effects; graph.cpp:300 records missingPlugins_ but the export never reads it.
Reasoning: an export whose plugin failed to load (or crashed) renders that track silent and still reports "succeeded" → app says "Exported". Only missing AUDIO aborts.
Upstream: SWEEP-03 (9c457ab) fixed failure-path file reporting, not this. ESTADO.md:355 FL-2 still tracks it («export reporta sucesso com plugin calado/em falta»). Old line numbers 363-367 still match almost exactly.
One-line direction: surface graph.missingPlugins()/failed processors in the job result and condition the "Exported" notice.

## CSV-02 — CONFIRMED
PT verbatim: «O motor diz porquê que cada plugin não carregou; a app só usa isso para desactivar "Open…" — nunca avisa.» (EngineSession.cpp:644-648)
Current evidence:
- app/desktop/src/EngineSession.cpp:644-648 — `snap_.missingPlugins.clear(); for (const auto& m : result.value("missing", ...)) snap_.missingPlugins[key] = plugin + ": " + why;` (comment: "their tracks are silent").
- Only consumers (git grep missingPlugins in app/): MainComponent.cpp:4687, :4698, :5300 — all `.setEnabled(!snap_.missingPlugins.count(key))` on "Open…" menu items. The stored "name: why" value is never displayed.
- Contrast: missingAssets DOES set snap_.message (EngineSession.cpp:650-652); missingPlugins does not.
- Engine side provides the reasons: graph.cpp:300 missingPlugins_ {key, plugin, why}; server.cpp:476 plugins.missing, :541 project.load answer "missing".
Reasoning: no notice/dialog/track mark announces a plugin that never loaded. SWEEP-05's "Try again" (MainComponent.cpp:5283-5284, :5451-5453, notice :6802) covers snap_.failedPlugins (failed WHILE PLAYING, event-driven EngineSession.cpp:726-731) — a different set; it does not announce missing-at-load plugins.
Upstream: ESTADO.md:355 «plugins.missing não é anunciado» still open; docs/plan/plugin-hosting-map.md:38 same finding at e7968c8.
One-line direction: a load-time notice listing name+why from snap_.missingPlugins (and a track mark).

## CSV-03 — CONFIRMED
PT verbatim: «relinkPlugins existe e tem zero chamadas.» (plugin_refs.cpp:63)
Current evidence:
- app/domain/src/plugin_refs.cpp:63 `int relinkPlugins(Document& d, const std::vector<json>& scanned) {` — definition.
- app/domain/src/plugin_refs.hpp:23 — declaration.
- `git grep -n relinkPlugins -- app engine tests ci` returns ONLY those two lines: zero call sites, including tests.
Reasoning: dead API; nothing relinks missing plugin references against a rescan.
Upstream: ESTADO.md:355 «relinkPlugins nunca chamado»; docs/plan/plugin-hosting-map.md:38-40 proposes calling it on open.
One-line direction: call relinkPlugins on song open / library rescan (needs owner decision per FL-2 scope).

## CSV-04 — CONFIRMED
PT verbatim: «Estado de plugin inválido é engolido em silêncio (plugin volta aos valores de fábrica).» (juce_host.cpp:348,351)
Current evidence (same line numbers as the old snapshot):
- engine/src/plugins/juce_host.cpp:347-352 (Impl::create):
  `const std::string state = plugin.value("state", "");`
  `if (carried && carried->getSize() > 0) instance->setStateInformation(carried->getData(), (int)carried->getSize());`
  `else if (!state.empty()) { juce::MemoryOutputStream data;`
  `  if (juce::Base64::convertFromBase64(data, juce::String(state))) instance->setStateInformation(data.getData(), (int)data.getDataSize()); }`
- Base64 decode failure: the `if` has no else — no `why`, no trace, no failure mark; instance keeps factory defaults.
- Decode success but invalid blob: JUCE setStateInformation is void; a plugin that rejects the data stays at defaults; nothing is reported. `why` is only set for load failures (:332 no file, :338 not found, :341 could not load, :398 crashed while loading).
- The STATE-PER-EDIT handshake (server.cpp statesNeeded) checks state PRESENCE, not validity; the state-per-edit report's red sound-neutral render showed a stateless load playing DEFAULT state (the data-loss the handshake prevents) — corrupt-but-present state still passes.
Reasoning: invalid state is swallowed silently; the plugin plays factory defaults and the app is never told.
Upstream: ESTADO.md:355 FL-2 «estado inválido ignorado em silêncio» still open; docs/plan/README.md:75 same.
One-line direction: validate/report state application (e.g. why + notice when decode fails or the plugin rejects the blob).

## CSV-05 — CONFIRMED
PT verbatim: «Sem verificação de NaN/Inf na importação de WAV float.» (audio_file.cpp:115-140)
Current evidence:
- engine/src/io/audio_file.cpp:115-156 readWav(): :139 `if (h.format == 3 && h.bits == 32) std::memcpy(&v, p, 4);` :140 `else if (h.format == 3) { double d; std::memcpy(&d, p, 8); v = (float)d; }` — raw copy, no isfinite check; header validation (:84-88) checks format/channels/rate/bits only.
- grep isfinite|isnan|NaN|finite in engine/src/io: zero checks (only the identifier wavInfo matched).
- engine/src/core/assets.cpp:49 `AudioData source = readWav(path, room);` — AssetStore adds no finite check either (ART-018's proposed fix location, still unimplemented).
- Downstream containment only: graph nativeOutputBad silences 32-sample sub-blocks (ART-018, ESTADO.md:332), no import-time counter/warning.
- App-side partial mitigations (NOT import gates): sample_analysis.cpp:56-57 maps non-finite to 0 for analysis; samples.cpp:206 skips non-finite in peak/RMS stats; AudioClipAnalysis.h:20 hasBrokenSamples is used ONLY by the "Cut silence" editor action (MainComponent.cpp:925-928 shows "the file is damaged" and refuses the cut). None sanitizes or rejects the audio the engine loads/plays/exports.
Reasoning: a float WAV with NaN/Inf still enters the engine raw; the finding's core claim holds at the cited function (now :115-156, raw float copy :139-140).
Upstream: ART-018 CONFIRMED in ESTADO.md:332/479 (triagem 17-11); no fix commit since (assets.cpp unchanged in this respect).
One-line direction: finite-check at AssetStore load and report as missing/broken audio (ART-018's proposed direction).

---

# csv-triage-2 continuation @ a9a038430d781959829f8510067ce59afffaad62 (re-verified: HEAD = main = origin/main)

Same method: reading + git read commands only; nothing built/run. Uncommitted tree state unchanged from above
(CMakeLists.txt sanitizer list + docs only). Findings CSV-06…CSV-10 below.

## CSV-06 — CONFIRMED
PT verbatim: «Instalador ainda sobrescreve um sample que o utilizador editou.» (Setup.cpp:88-93)
Current evidence:
- app/installer/Setup.cpp:89-92 (copySamples): `const bool same = to.existsAsFile() && to.getSize() == from.getSize() && to.getLastModificationTime() == from.getLastModificationTime();` then `if (!same) { if (to.getParentDirectory().createDirectory() && from.copyFileTo(to)) ours.add(rel); ... }` — unconditional overwrite whenever size OR mtime differs from the source.
- The skip heuristic is size+date only (:65-66 comment admits it). A user edit of an installed sample changes size and/or mtime → `same` is false → the source file is copied over the edit. No content hash, no backup, no prompt, no "modified since we copied it" check; samples.txt ownership (:80-99, Audit 27) decides UNINSTALL deletion only, never guards the install-time copy.
- The comment's protection «Files Core added there (Save as sample) are never touched» (:66) covers only NEW files (not in the source listing); it does not cover edits to distributed samples.
- Scope: the installed copy <folder>\Samples is what Core's Library uses (:9-10); the source (D:\Samples or --samples) is only read (:13), so edits to the SOURCE are safe — the loss is the edit made in the installed library. Both entry points hit copySamples: window Install/Update (:276) and silent --install (:382-383).
- Old lines 88-93 map to the same block now at :89-93. The heuristic dates to b80017e (git log -S getLastModificationTime); no fix since. No installer test exists (grep copySamples|Setup|samples.txt in tests/: zero).
Reasoning: any edit that changes size or mtime (i.e. virtually every edit) is silently overwritten on the next install/update; the finding holds at the current code.
Upstream: no ESTADO.md entry for install-time overwrite (grep Setup|instalador|sobrescrev: only build evidence and Save-dialog items). Audit 27 (:76-79) fixed the sibling bug — uninstall DELETING user files — not this one.
One-line direction: compare content (hash) or track copied hashes in samples.txt; skip/backup/prompt when a listed file was modified by the user.

## CSV-07 — PARTIAL
PT verbatim: «Não existe teste bit-a-bit serial≡M5, nem teste do watchdog de reset.» (engine_tests.cpp)
Current evidence — the two claims split:
- Claim A (no bit-a-bit serial≡M5 test): HOLDS. Every M5 test in the tree: tests/engine_tests.cpp:720 (FL-1/D51 default OFF — asserts liveThreads counters 0/2/4 only), :957-1007 (M5 re-arm, da91dab — asserts liveStalls/liveFallback with a fake constant-level LevelProc, no audio comparison), :1039-1066 (A-004 held-note swap, 0e45d58 — peak > 0.2f checks, not bit-exact). No test anywhere renders one song serial vs multicore and compares samples (grep liveThreads in tests/: only the :720 default test; grep bit-a-bit|bit-exact|serial.*multicore: nothing of the kind). The existing "bit-identical" tests are other axes: engine_process_tests.cpp:755 (STATE-PER-EDIT: state-on-demand vs state-in-song), engine_tests.cpp:1150 (freeze/split null-identical), MIDI byte-identical export tests.
- Claim B (no reset-watchdog test): REFUTED at current code. tests/engine_process_tests.cpp:489-527 — TEST("serve: a plugin stuck in processBlock does not hold Stop hostage (bounded reset)"): CORE_TEST_FAULT_HANG plugin stalls 8 s in processBlock; comment :510-511 «the reset watchdog silences it (markCrashed unblocks the vendored acquire)»; CHECK(stopMs >= 0) and CHECK(stopMs < 4500) with «the watchdog's per-plugin budget (2 s)» (:512-514); failedAsCrash (:517); plays on afterwards (:521-524). Added by 88aa357 (docs/relatorios/README.md:45: red transport.stop no-reply 5000 ms → green, engine_process 10/121→11/131). It lives in engine_process_tests.cpp, NOT engine_tests.cpp — the finding's file pointer is why the old snapshot missed it.
Reasoning: the finding was true at the pre-merge snapshot (ESTADO.md:356 still carries the stale «M5 não tem teste algum»); since flawless-m5 merged, the watchdog half is covered by a real regression test, while the bit-exact serial≡M5 matrix remains absent — a declared follow-up, not an oversight.
Upstream: docs/relatorios/2026-11-18-engine-rt-fixes.md:202 «Follow-up (SUR-16 convergence): M5 serial-vs-multicore full matrix with real plugins under load»; ESTADO.md:41/285/356 (D51: M5 ON requires soak 8 h + bit-a-bit vs serial on the owner's PC; not done). ESTADO.md:356 also records the plan's demand for a deterministic serial-vs-M5 test with a fake provider BEFORE soak — still not done.
One-line direction: add the deterministic serial≡M5 bit-exact test (fake provider, per the plan), then the real-plugin matrix on SUR-16 convergence; watchdog half already covered — mark it done in the triage.

## CSV-08 — CONFIRMED
PT verbatim: «O patch do JUCE (D113) não é aplicado por nada no repo → risco de gate falso-verde num fetch novo.» (ci/patches/…patch)
Current evidence (repo-side, read in full):
- ci/patches/juce-failed-plugin-restart.patch exists and is tracked: 14 hunks, 2 files (modules/juce_audio_processors_headless/utilities/juce_AudioPluginExtensions.h and format_types/juce_VST3PluginFormatImpl.h) — matches the regenerated D113 + crash-reset form. It is an INERT record: nothing consumes it.
- CMakeLists.txt:64-67 — FetchContent_Declare(JUCE URL …9.0.5.tar.gz URL_HASH SHA256=d92207…) + FetchContent_MakeAvailable(JUCE): NO PATCH_COMMAND. A fresh fetch downloads PRISTINE 9.0.5 and builds it unpatched; a local FETCHCONTENT_SOURCE_DIR_JUCE is used as-is with no content check.
- ci/gate.ps1 (81 lines, read in full): :33 only `if (Test-Path $Juce) { $juceArg = -DFETCHCONTENT_SOURCE_DIR_JUCE=… }`; :51-56 configure/build/deps-check/ctest. No git apply, no patch fingerprint, no verification that the vendored tree carries markCrashedByHost/enterProcessMutexUnlessCrashed. Its false-green guards (VSLANG/chcp :48-49, ninja -t deps :54, --no-tests=error :56) cover OTHER failure modes, not this one.
- ci/compilar.bat (401 lines, read in full): CfgLine :198-207 passes the same -D; :240 only checks the JUCE folder EXISTS; Gate button :341-350 delegates to gate.ps1. No patch step.
- ci/gate.sh:7 — Linux gate runs CORE_BUILD_APP=OFF (no JUCE at all). No tools/ dir, no root-level scripts, no bootstrap/setup script anywhere: grep patch|apply over all *.ps1/*.bat/*.sh/*.cmake/*.py in the repo matches only unrelated JS inside a docs evidence file.
- Re-application is a MANUAL procedure documented in reports (docs/relatorios/2026-11-18-plugin-crash-followups.md:180-185; 2026-11-18-crash-in-reset-deadlock.md:162-169: cd into the JUCE root, git apply --check, git apply) — report text, not repo automation.
Reasoning: on any fresh fetch/clone of JUCE the patch is lost and no repo mechanism restores or even detects it; the gate then builds/tests against pristine JUCE while believing it gates the ratified D113 behavior. The false-green RISK is real and registered; whether a given unpatched run goes green or red is timing-dependent (the crash-in-reset report measured the natural race 0/90 that day) and is NOT measurable by reading — the confirmed part is the absence of application/verification in the repo.
Upstream: ESTADO.md:357 infra pendência («falta decidir como o bootstrap aplica o patch … Risco de falsa-verde registado»); D113 at ESTADO.md:206 (patch lives as a never-committed working-tree mod of D:/Core-deps/JUCE-9.0.5). External review scripts at D:\Core-build\core-review fingerprint the patch state, but they are OUTSIDE the repo — the finding is about the repo itself and holds.
One-line direction: decide and implement bootstrap patch application (e.g. FetchContent PATCH_COMMAND or a gate pre-step git apply --check + fingerprint), so a fresh fetch cannot gate unpatched JUCE silently.

## CSV-09 — CONFIRMED
PT verbatim: «Faixas em mute pagam CPU total; bloco de plugin fixo em 128.» (graph.cpp:1239/1302)
Current evidence:
- Mute: engine/src/core/graph.cpp:1146 `Graph::runTrack` (one track over one sub-block; used by BOTH the serial loop and the M5 workers — liveJob :1412-1417 runs it for every parallel track; parallel_ :1397 excludes only buses/keyed tracks, never muted ones). Inside it, in order: automation/modulators/glide :1156-1227, instrument render :1239-1247 (old line 1239 is now the `if (!t.instrument->failed())` guard; renderTrack at :1241), audio clips :1251-1258, ALL inserts :1260-1298 (`t.effects[e]->process` :1292; skipped only for bypass/failed), key source :1301. Track mute is FIRST consulted at :1302 `const bool audible = !t.mute && (solo_ >= 0 ? …)` and only zeroes the fader (:1304 `fader = audible ? … : 0.0`) and send targets (:1316) — the fully processed signal is ramped to zero. No early-out for mute anywhere.
- Track mute has exactly one consumer in the engine: graph.cpp:1302 (grep .mute over engine/src: project.cpp:564 load / :197 save, graph.cpp:371 publish, :1302). The other mute skip, graph.cpp:402 `if (a.mute) continue;`, is per-AUDIO-CLIP mute at publish, not track mute.
- Fixed 128: engine/src/plugins/juce_host.cpp:23 `constexpr int kPluginBlock = 128;` — live instances are ALWAYS created with it (:330 default arg; :396 `offline ? exportPluginBlock() : kPluginBlock`). The graph feeds 32-sample sub-blocks (graph.hpp:99 kSubBlock = 32) and the host gathers them into 128-sample plugin calls (:19-22 comment: audit 2.2 chose 128 because 1500 calls/s hurt FFT/oversampling plugins; the 128-sample gathering latency is reported via latencySamples and compensated). Nuance: EXPORT ONLY may opt into 256/512 via CORE_EXPORT_PLUGIN_BLOCK (:24-40), default still 128 «until it is measured with the plugins of the house»; the live block never follows the device block size.
Reasoning: both halves hold at the current code. A muted track pays full instrument + insert DSP (and modulation/automation work) every sub-block, serial and M5 alike; only the final gain is zero. The live plugin block is a compile-time 128, not adaptive. Old line 1302 still matches exactly; old 1239 shifted to the failed() guard as warned.
Upstream: no ESTADO.md pendência for either (grep mute|CPU|kPluginBlock: only D54 priority order «perda de dados/sessão > caminho de áudio > CPU > cosmético» and unrelated items). The 128 choice is a documented design decision (audit 2.2), not an oversight; the mute CPU cost is untracked. Note for any fix: a muted track's key source still feeds sidechains pre-fader (:1301 comment «before its fader and mute»), so skipping muted DSP must preserve sidechain/key behavior or be a decision.
One-line direction: skip instrument+insert DSP for muted tracks (preserving key/sidechain feeds and un-mute state), and treat the live plugin block size as a measured tuning item (export already has the env knob).

## CSV-10 — CONFIRMED
PT verbatim: «Undo copia o documento inteiro e serializa-o em JSON a cada commit só para lhe calcular o preço.» (history.cpp:9,14-18)
Current evidence (app/domain/src/history.cpp — same file, lines shifted by one):
- :8 `size_t sizeOf(const Document& d) { return toJson(d).dump().size(); }` — serializes the WHOLE document (plugin base64 blobs included) and dumps it to a string, then keeps only .size(); the string is discarded.
- :14-18 `History::push(std::deque<Step>& stack, Document document, …) { const size_t bytes = sizeOf(document); bytes_ += bytes; stack.push_back({std::move(document), label, bytes}); }` — the by-value parameter plus the lvalue call in commit (:51 `push(undo_, current_, label)`) means every commit makes a FULL Document copy and then a FULL JSON serialization of it, solely to compute the byte price used by the budget accounting (bytes_ / trim() :22-29).
- The stored Step holds the Document itself, not the JSON (history.hpp:69 `struct Step { Document document; std::string label; size_t bytes; };`) — the serialization is pure pricing overhead, exactly as the finding says.
- Same price serialization on every undo (:92 push(redo_, …)), redo (:104) and endGesture (:77) — those move the document (no extra copy) but still dump it to JSON for sizeOf.
- Whole-document snapshots are the design: history.hpp:2-3 «Steps are whole-document snapshots … Assets (audio) will be shared by reference when they exist, never copied per step» — intent declared, NOT implemented; the 64 MB budget / kMinSteps 50 / 1 GB hard ceiling (history.hpp:14-23) guard memory but do not remove the per-commit copy+serialize cost.
- Old lines 9,14-18 map to :8,14-18 today (identical shape).
Reasoning: per commit = one full document copy + one full toJson+dump whose only product is a byte count; with large plugin states this is the dominant undo cost. The claim holds verbatim at the current code.
Upstream: docs/relatorios/2026-11-18-state-per-edit-root.md:200-202 — «NOT done here (dispatch follow-up slices): 1. Undo blob-sharing (history.cpp): undo snapshots still copy the whole document per step (the 1 GB / 50-step ceiling guards it)»; also :5-6. The state-per-edit root fix (5df584f) removed blobs from edit PROJECTIONS only; documents (and thus undo snapshots) keep every blob byte-identical (:52,:57). Related measured cost context: :206-208 (Server::handle rollback snapshot ~12 ms/edit) is a separate follow-up, not this one.
One-line direction: the declared slice-1 follow-up — share state blobs by reference between snapshots (structural sharing) and price steps from shared-blob accounting instead of a full JSON dump per push.

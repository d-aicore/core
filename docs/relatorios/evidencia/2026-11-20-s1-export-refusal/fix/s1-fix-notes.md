# S1 fix notes (s1-fix) â€” export refusal on needed-plugin failure (DATA priority)

Repo: D:\CORE\CORE @ a9a038430d781959829f8510067ce59afffaad62 (main). Pre-existing uncommitted changes
(CMakeLists.txt D113 JUCE patch gate + sanitize list, tests/CMakeLists.txt S5 installer test,
app/installer/Setup.cpp, docs) preserved untouched. Build dir: D:\Core-build\s1-fix\build (own; red
evidence dir D:\Core-build\s1-red\build untouched).

## Step 0 â€” read + design (code-confirmed)

- Read s1-repro-notes.md in full (cases aâ€“e, fix contract 1â€“6).
- CLI chain: main.cpp:677-712 â†’ Server::render (server.cpp:739-812) â†’ renderProject (server.cpp:144-170,
  block 512). main.cpp:712 `return r["ok"] ? 0 : 1` â€” a throw from renderProject â†’ Server::handle catch
  (server.cpp:344-347) â†’ {ok:false, error} â†’ exit 1 already works; no main.cpp change needed.
- App chain: render.start (server.cpp:700-733) â†’ RenderJobs::run (render_job.cpp:336-415). Only
  missingAssets aborts (:363-367). Throw in run() â†’ discard() working files â†’ State::Failed + error
  (:409-414) â†’ describe()["error"] (:433) â†’ MainComponent.cpp:6767-6781 showError("Could not export",
  job.error + ...). "Exported" only on succeeded (:6765). **No MainComponent.cpp edit needed** â€” the
  existing failure branch surfaces the refusal reason verbatim.
- Failure source of truth: PluginProcessor::failed()/failure()/failureName (plugins.hpp:42-57), marked by
  runPlugin (graph.cpp:93-107: exception OR host-caught crash) and checkOutput (:112-122: NonFinite/Runaway).
  Server::failedPlugins (server.cpp:874-880) already reports {key, plugin, why} from loadedPlugins_ â€” the
  export graph gets the same accessor over its own plugins_ list.
- Bypass: Effect::bypass set ONLY at graph build (graph.cpp:311; project.cpp:465; no runtime mutation in
  engine/src â€” grep confirmed). Bypassed plugin effects are loaded but never processed (graph.cpp:1273,
  :1101). Design: record bypassed plugin keys at build; exclude them from BOTH refusal sets (a bypassed
  unit is intentional silence even if its plugin file is missing or its instance failed). Instruments have
  no bypass (tracks have mute; contract is conservative: only bypass excluded).
- missingPlugins_ (graph.cpp:300) includes bypassed effects whose plugin failed to load (loadPlugin runs
  before the bypass check) â€” the pre-render refusal must filter those out too.
- Global failedUnits() (graph.cpp:270-280) records NATIVE unit failures only (noteFailed call sites
  :1108/:1243/:1294); plugin failures live in processor->failure_. Reuse the processor flag (same source
  engine.stats' failedPlugins uses), not the native list.

### Design (minimal)

1. graph.hpp/.cpp: `bypassedPluginKeys_` recorded in buildChain; new accessors
   `failedPlugins()` (loaded plugins with processor->failed(), minus bypassed; {key, plugin, why}) and
   `missingNeededPlugins()` (missingPlugins_ minus bypassed).
2. render_job.hpp/.cpp: shared `checkExportPlugins(const Graph&)` â€” throws naming each plugin/track/why
   ("Export refused: ... Nothing was published; the previous file is untouched."). Called in
   RenderJobs::run (a) after graph build + missingAssets check, BEFORE preroll/render; (b) after each
   file's render loop, BEFORE wav.finish/publish. Throw â†’ existing discard()/Failed path: nothing
   published, working files cleaned, previous targets untouched. describe() unchanged: the Failed error
   field already carries the refusal (other states' JSON contracts untouched).
3. server.cpp renderProject: same check inside renderOnce after graph build and after the render loop â†’
   throw â†’ Server::render â†’ handle ok:false â†’ CLI exit 1. No writeWav/publish reached on refusal.
4. Success-path .previous deletion (render_job.cpp:181-184) NOT changed (separate product decision â€”
   recommendation recorded). CLI legacy .part naming NOT changed (SWEEP-02 follow-up â€” recorded).

### Existing-test impact analysis (code-confirmed before editing)

- audit2_tests RenderJobs cases: silent projects / audio clips only â€” unaffected.
- engine_tests renderProject cases: FakeProvider/LevelProvider/SwapProvider never fail and always load;
  audio-clip songs have no plugins â€” unaffected. audit_tests B04/B06: audio projects â€” unaffected.
  hunt_tests P02: demo (generated audio) â€” unaffected. engine_process_tests crash cases: LIVE playback
  (serve+play), not export; STATE-PER-EDIT renders use the healthy synth â€” unaffected.
- **cli_plugin_crash_render (merge) encodes the OLD contract**: it asserts a crash song exports with
  rc 0 / ok:true / WAV written. The owner-authorized S1 contract INVERTS this: the export must be
  refused. The test is updated to the new contract (refusal + previous WAV intact), KEEPING its
  no-hang (TIMEOUT) and crash-really-happened (stderr) assertions and adding a healthy green control.
  This is a sanctioned contract change, not a weakening; recorded here and in the final answer.

## Step 1 â€” regression test written (red-first, before any engine change)

- tests/export_plugin_failure_tests.cpp (NEW executable, support/check.hpp harness, links
  core_engine_lib; in-process fake PluginProvider â€” no JUCE needed, cross-platform): cases 1 load-fail,
  2 crash-during-render (self-marked Failure::Crash, the exact state the SEH catch leaves; plus a
  throwing variant), 3 healthy control, 4 stems whole-set refusal, 5 bypassed effect (+ bypassed
  load-fail variant + non-bypassed control). Each case asserts BOTH paths: RenderJobs (app) and
  renderProject (CLI engine function). Byte-identical previous WAV checked by full byte comparison.
  Compiles against UNFIXED code (uses only existing APIs).
- tests/cli_plugin_crash_render.cmake updated to the new contract + extended to all 5 real-plugin CLI
  cases (Core Test Synth / Core Test Crash, scan identities, exit codes, ok:false, SHA256-identical
  previous targets, stems whole set, bypass). Real SEH crash proof end-to-end.
- Registration: tests/CMakeLists.txt â€” export_plugin_failure_tests added to the merge label list.

## Step 2 — RED measured (UNFIXED engine @ a9a0384 + new tests only; D:\Core-build\s1-fix\build)

- Configure rc=0 (configure.txt; D113 local JUCE patch verified), full build rc=0, 559 targets (build-red.txt).
- In-process: tests\export_plugin_failure_tests.exe ? rc=1, "5 testes, 54 verificacoes, 33 falhas"
  (red-inprocess.txt). Case 1 FAIL, case 2 FAIL, case 3 ok (green control), case 4 FAIL, case 5 FAIL.
  Case 5 nuance: the bypass sub-blocks (bypassed crash / bypassed missing) PASS unfixed — bypass was
  already silent; what fails red is case 5's non-bypassed controls (active track effect + master effect
  crash render "succeeded"). Typical red line: `state == "failed": medido succeeded, esperado failed`
  and the previous-WAV byte comparison fails (target replaced by the degraded render).
- Real plugins CLI: ctest -R cli_plugin_crash_render ? FAILED (red-cli.txt). Case 3 healthy control ok
  (peak 0.40157479047775269, sha256 09b25081b51ce282357c4843c396746abe6e40792c2973ce4c0266abdeaaaabc);
  case 1 load failure: "the export was NOT refused (exit 0)" — answer ok:true, missing attached load-time
  only, WAV written (peak 0.4015747904777527 = healthy synth alone). Matches the repro's case a exactly.
- One test-script bug found and fixed during red (not engine): cmake string(JSON SET) needs quoted JSON
  string values; corrected before the red run above.

## Step 3 — fix implemented (minimal, within write scope)

- engine/src/core/graph.hpp:113-119 — accessors missingNeededPlugins() / failedPlugins(); :398 bypassedPluginKeys_.
- engine/src/core/graph.cpp:282-302 — accessor implementations ({key, plugin, why}; failedPlugins reads the
  same processor->failed() flag the SEH/exception catch marks — the source engine.stats' failedPlugins uses);
  :326-341 buildChain records bypassed plugin keys (bypass is build-time only; grep-verified no runtime mutation).
- engine/src/core/render_job.hpp:66-72 — shared checkExportPlugins(const Graph&) declaration.
- engine/src/core/render_job.cpp:294-316 — implementation (throws "Export refused: plugin '<name>' (<key>)
  did not load / failed during the render: <why>; ... Nothing was published; the previous export is untouched.");
  :391-393 pre-render call (after missingAssets, before preroll/working-file reservation); :418-423 post-render
  call (before wav.finish/checkExportLevel/publish). Throw -> existing discard()/State::Failed path: working
  files cleaned, nothing published, previous targets untouched. describe() UNCHANGED — the Failed state's
  error field already carries the refusal; other states' JSON contracts untouched.
- engine/src/core/server.cpp:152-154 + :163-165 — same two checks inside renderProject's renderOnce (mix
  first, so any needed-plugin failure refuses the whole stem set). Throw -> Server::handle catch
  (server.cpp:344-347) -> ok:false + error -> main.cpp:712 exit 1 (no main.cpp change needed).
- NOT changed: MainComponent.cpp (failure branch :6767-6781 already shows job.error verbatim via
  showError("Could not export", ...); "Exported" only on succeeded :6765 — code-verified, no UI click-through);
  success-path .previous deletion (render_job.cpp:181-184 region, now shifted); CLI legacy .part naming.

## Step 4 — GREEN (fixed, incremental rebuild rc=0, 44 targets, build-green.txt)

- export_plugin_failure_tests: 5 testes, 54 verificacoes, 0 falhas (green-inprocess.txt). All 5 cases ok.
- cli_plugin_crash_render (real Core Test Synth / Core Test Crash, SEH): Passed 4.18 s (green-cli.txt,
  green-cli-cases.txt):
  - case 3 healthy control: ok, peak 0.40157479047775269, sha256 09b25081b51ce282357c4843c396746abe6e40792c2973ce4c0266abdeaaaabc
  - case 1 load failure: refused, previous WAV SHA256-identical — "Export refused: plugin 'Ghost Missing
    Plugin' (track:ghost) did not load: not found in ...no-such-plugin.vst3..."
  - case 2 SEH crash: refused before publish, previous WAV SHA256-identical — "Export refused: plugin
    'Core Test Crash' (track:crash) failed during the render: crash." + stderr "crashed plugin" (crash really happened, no hang)
  - case 4 stems: WHOLE set refused, all 3 pre-existing targets SHA256-identical, no .part debris
  - case 5 bypass: exported, peak 0.40157479047775269 (bypassed crash effect = intentional silence)

## Step 5 — full merge (D:\Core-build\s1-fix\build)

- Registered: 24 merge tests = baseline 21 (s1-red list) + export_plugin_failure_tests (S1, this delivery)
  + wav_float_tests (S4 teammate, added concurrently to tests/CMakeLists.txt during this session)
  + installer_samples (S5 teammate; file created 00:46, after the red configure at 00:17 — hence absent
  from the red baseline list). Teammate edits preserved.
- Full run (merge-full.txt): 23/24 Passed; ONLY engine_process_tests Failed — the known Debug ENV-03
  exit 259 at engine_process_tests.cpp:159 ("The engine exited (code 259)", live-playback crash/retry/exit
  case; 15 testes, 256 verificacoes, 1 falhas — the other 14 cases pass). PRE-EXISTING proven: the UNFIXED
  s1-red build fails identically (same line, same 259, same counts). Not conflated with the fix.
- Excluding engine_process_tests (merge-excl.txt): **23/23 Passed, 100%**, 45.5 s.
- Per-suite counts (all rc=0): engine_tests 39/221, wav_float_tests 16/360, domain_tests 33/568,
  modules_tests 6/128, role_tests 21/215, midi_tests 20/402, bug_tests 48/462, bitwig_tests 2/25,
  audit_tests 7/65, audit2_tests 15/364, export_plugin_failure_tests 5/54, hunt_tests 11/106,
  round3_tests 7/82, calls_tests 12/69, menu_tests 7/301, window_tests 10/70 (testes/verificacoes, 0 falhas each).
  CLI: cli_version, cli_demo_render, cli_measure, cli_unicode, cli_package_stale, installer_samples,
  cli_plugin_crash_render all Passed.
- Contract change recorded: cli_plugin_crash_render formerly asserted crash-song export SUCCESS (rc 0,
  ok:true, WAV written). The S1 owner contract inverts this; the test now asserts refusal + intact previous
  export, keeping the anti-hang TIMEOUT and the crash-really-happened stderr proof, plus a healthy control.
  Sanctioned contract change, not a weakening (AGENTS.md: no criteria relaxed).

## Limitations (declared)

- Headless proof only: no UI click-through. The app notice path is code-verified (MainComponent.cpp:6765
  succeeded-only "Exported"; :6767-6781 failed -> showError with job.error). desktop self-test (label
  desktop) NOT run; UI gates (open/play/save/reopen) not declared.
- Debug build, 48 kHz / 24-bit / stereo, CLI block 512 and RenderJobs kBlock 4096 (in-process suite runs
  the real RenderJobs worker incl. WorkerPool::forExport). Other rates/bits/mono not exercised by the new
  tests (existing suites cover them on healthy songs).
- In-process crash fake reproduces the post-SEH state (processor marked Failure::Crash, buffer cleared);
  the actual SEH access violation is exercised by cli_plugin_crash_render with the real Core Test Crash.
- engine_process_tests exit-259 (ENV-03) pre-existing in Debug, reproduced on the unfixed red build;
  separate follow-up, not addressed here.
- Mid-publish process-death partial set (repro case e code verdict) NOT addressed: out of S1 scope
  (refusal happens before publish; crash-atomic placement is a separate decision).
- Report/ledger: write scope limited to engine/src/core + tests by the owner-authorized task; the
  docs/relatorios report and ESTADO.md lines are PROPOSED below for the scribe/coordinator (D72/D80).

## Recommendations recorded (NOT implemented — separate decisions)

1. Success-path backup retention: publishRenderOutputs still DELETES the .previous aside on success
   (render_job.cpp, fs::remove(asides[f]) in the success tail). With S1 refusal a degraded export can no
   longer overwrite, but any future success-with-warnings design (or a plain re-export) still destroys the
   old file's only copy. Recommend a product decision on retaining .previous on success (DATA priority).
2. CLI legacy .part naming: Server::render still writes the FIXED "<target>.part" (server.cpp writeWav
   calls) opened with truncation; SWEEP-02's exclusive reservation covers the app path only. A retained
   .part from an interrupted CLI run is silently consumed. Recommend extending reserveRenderWorkingFile
   to the CLI (SWEEP-02 follow-up).
3. Crash-atomic stem placement (repro case e): per-file renames leave a sub-ms partial-set window on
   process death; recommend a separate decision (e.g. staging dir + atomic directory swap or a manifest).

## Proposed ledger lines (for the scribe — BUG namespace per D75; this delivery does not edit ESTADO.md)

- BUG-S1 (candidate, comprovado por teste): export succeeded despite needed-plugin load failure or
  render-time crash (SEH), publishing a degraded render byte-identical to the healthy control and
  replacing the previous export; fixed by pre-render + pre-publish refusal in both export paths
  (checkExportPlugins), bypass excluded. Red: s1-repro-notes.md cases a-e + s1-fix-notes.md Step 2
  (unfixed @ a9a0384, in-process 33 falhas + CLI exit 0). Green: Step 4 (5/5 in-process, 5/5 real-plugin
  CLI) + merge 23/23 excl. engine_process_tests. Source: coordinator-round2 notes, 2026-11-20.
- Estado medido proposal: "S1 export refusal implemented and proven @ a9a0384 + fix (Debug, headless):
  needed-plugin load failure / render-time crash refuses CLI (ok:false, exit 1) and app job (Failed,
  error names plugin/track/why) before publish; previous WAV byte-identical; stems whole set; bypass is
  not failure. merge 23/23 excl. engine_process_tests (ENV-03 259 pre-existing, reproduced unfixed)."
- Note for the ledger: cli_plugin_crash_render's contract inverted by owner-authorized S1 (success ->
  refusal); anti-hang + crash-proof assertions kept.

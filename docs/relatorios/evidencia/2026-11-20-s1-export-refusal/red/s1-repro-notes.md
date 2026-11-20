# S1 red-reproduction notes (DATA priority) — export succeeds despite plugin load failure / crash

- Repo: D:\CORE\CORE @ a9a038430d781959829f8510067ce59afffaad62 (READ-ONLY; pre-existing uncommitted changes in CMakeLists.txt / docs left untouched).
- Engine: D:\Core-build\s1-red\build\engine\core-engine.exe (32,890,368 bytes, built 2026-11-20 00:18). Debug build, 555 targets.
- Probe dir: D:\Core-build\s1-red\probe\ (all songs, scans, WAVs, raw JSON answers here).
- Method: headless engine CLI (`core-engine render <song.json> <out.wav>`), no UI click-through performed.

## CLI / render-path facts (code-confirmed before running)

- CLI render entry: engine/src/app/main.cpp:677-712. Sample rate default **48000** (`--sr`, main.cpp:684); bits default 24; tail default 2000 ms; `--stems`, `--mono`, region flags.
- CLI render → `Server::render` (engine/src/core/server.cpp:739-812) → `renderProject` (server.cpp:144-170) with **blockSize 512** (server.cpp:775). Graph built per render with `offline=true`; **no check of missingPlugins() or failed units anywhere in this path**; result JSON (server.cpp:809-811) carries no missing/failed fields.
- main.cpp:694-698 comment: "A render with a plugin or a sample that did not load still renders (they are silent), but says so." — load-time `missing` from `project.load` is printed to stderr and **attached to the CLI result JSON at main.cpp:710** (`result.missing`). This covers LOAD failures only; a crash DURING render is not in `missing`. Exit code 0 when ok (main.cpp:712).
- App export path: `render.start` (server.cpp:700-733) → RenderJobs (engine/src/core/render_job.cpp). Only **missing AUDIO** aborts: render_job.cpp:363-367 (`if (!graph.missingAssets().empty()) throw ... "Missing audio, nothing exported"`). No missingPlugins/failed-unit check.
- `RenderJobs::describe` (render_job.cpp:417-443): state/progress/path/bits/sampleRate/files/peak/error — **no missingPlugins or plugin-failure field ever**. `State::Succeeded` set at render_job.cpp:408 after publish.
- graph.cpp:291-301 `loadPlugin`: on failure records `missingPlugins_.push_back({key, plugin, why})` (:300) and returns nullptr → track silent.
- graph.cpp silent-skip sites (current checkout line numbers): **:1239** `if (!t.instrument->failed())` (failed instrument track muted), **:1273** `if (t.effects[e]->bypass || t.effects[e]->failed()) continue;`, **:1101** same for master effects. Crash/exception marking: graph.cpp:90-108 (`runPluginCall`, SEH catch marks failed), :185/:253 track silenced from then on.
- App notice: app/desktop/src/MainComponent.cpp **:6765** `if (job.state == "succeeded") notice_ = ... "Exported: " + path` (multi-file: "Exported N files to ..."). Failure branch :6767-6781. **UI click-through NOT performed** — headless engine-level proof + this citation.
- Stems: PRESENT. CLI `--stems` (main.cpp:703) → Server::render stems branch server.cpp:766-773 (one WAV per track, `<base>-<trackId>.wav`); renderProject renders mix then each solo stem (server.cpp:162-166). App path: `render.start` `files:[{path,track}]` (server.cpp:711-724), RenderJobs loops outputs (render_job.cpp:356-393), publishes after ALL files done (:394-408). Case e applicable.

## Plugin identities (core-engine scan, rc=0 both)

- Core Test Synth: id `VST3-Core Test Synth-3c79fbd2-bc3b5f3e`, vendor Core, v1.0.0, instrument, 0 in / 2 out.
  File: D:\Core-build\s1-red\build\app\selftest-plugins\CoreTestSynth_artefacts\Debug\VST3\Core Test Synth.vst3\Contents\x86_64-win\Core Test Synth.vst3
- Core Test Crash: id `VST3-Core Test Crash-fa11cd27-bc3b4f37`, vendor Core, v1.0.0, instrument, 0 in / 2 out.
  File: D:\Core-build\s1-red\build\app\selftest-plugins\CoreTestCrash_artefacts\Debug\VST3\Core Test Crash.vst3\Contents\x86_64-win\Core Test Crash.vst3
- Crash seam: `CORE_TEST_CRASH` is a **compile-time variant flag**, not a runtime env var (app/selftest-plugins/TestPlugins.cpp:202-222). The built Crash plugin crashes **unconditionally in processBlock after 0.1 s of playback** (null-pointer write, :215; limit set in prepareToPlay :212). No env var needed to trigger it. (Runtime env seams exist only on the Fault variant, e.g. CORE_TEST_FAULT_HANG — not used here.)
- Song JSON format: tests/engine_process_tests.cpp:79-87 — track `{id, name, instrument:"plugin", plugin:<scan object>, patternTicks, notes:[{tick,length,pitch,velocity}]}`; song `{format:"core-project", version:2, name, bpm, lengthTicks, masterGainDb, tracks}`. PPQ 960.

## Cases

### Case a — LOAD FAILURE (nonexistent plugin path) — DEFECT CONFIRMED (render succeeds, failed track silent)

- Song: D:\Core-build\s1-red\probe\song-a-loadfail.json — 2 tracks: `synth` (healthy Core Test Synth) + `ghost` (same scan identity but file → `D:\Core-build\s1-red\probe\no-such-plugin.vst3\Contents\x86_64-win\no-such-plugin.vst3`, id `VST3-Ghost-00000000`, does not exist). Both tracks have notes (pitch 60 then 64, 2 s song @120 bpm, lengthTicks 3840).
- Command: `"D:\Core-build\s1-red\build\engine\core-engine.exe" render "D:\Core-build\s1-red\probe\song-a-loadfail.json" "D:\Core-build\s1-red\probe\a-loadfail.wav" --tail-ms 500`
- **Exit code: 0.** Render path: 48000 Hz, block 512 (server.cpp:775), 24-bit stereo, 2.5 s (2 s song + 0.5 s tail).
- Engine JSON answer (verbatim, D:\Core-build\s1-red\probe\a-answer.json):

```json
{
  "ok": true,
  "result": {
    "channels": 2,
    "conversionNotes": [],
    "denseEventBlocks": 0,
    "missing": [
      {
        "key": "track:ghost",
        "plugin": "Ghost Missing Plugin",
        "why": "not found in D:\\Core-build\\s1-red\\probe\\no-such-plugin.vst3\\Contents\\x86_64-win\\no-such-plugin.vst3"
      }
    ],
    "missingAssets": [],
    "path": "D:\\Core-build\\s1-red\\probe\\a-loadfail.wav",
    "peak": 0.6250405311584473,
    "realtimeFactor": 33.60111179358703,
    "renderMs": 74.4023,
    "sampleRate": 48000.0,
    "seconds": 2.5,
    "stems": []
  }
}
```

- WAV: D:\Core-build\s1-red\probe\a-loadfail.wav — 720,044 bytes, 48 kHz / 2 ch / 24-bit.
  sha256 `ff4b1f30d4fe35911fc2c249adc6f18fa81947885ec7a76384b6029f5793548b`, measured sample peak **0.62504053115844727** (independent byte-level parse; matches engine `peak` exactly). Audio = the healthy synth only; the ghost track contributed silence.
- stderr (D:\Core-build\s1-red\probe\a-stderr.txt): `plugins: could not load 'Ghost Missing Plugin' for track:ghost: not found in ...` twice (live graph + export graph), plus `missing plugin: {...}` line.
- **Where the 'missing' info lives:** present in the CLI result ONLY because main.cpp:710 attaches the load-time `project.load` missing list (`result.missing`), and on stderr. The render/export result itself (Server::render server.cpp:809-811; RenderJobs::describe render_job.cpp:417-443 used by the app) carries **no** missing-plugin field, and nothing refuses the export: `ok:true`, exit 0, file written. The app's `render.start` job would report `succeeded` → "Exported" (MainComponent.cpp:6765) with no hint of the silent track.
- Measurement note: first WAV-peak parse was wrong (PowerShell `-shl` truncates `[byte]` operands); helper fixed with `[int]` casts (probe\wav-info.ps1), re-measured peak matches engine JSON to the last digit. sha256 unchanged by the fix.

### Case c — NORMAL (healthy Core Test Synth) — GREEN CONTROL

- Song: D:\Core-build\s1-red\probe\song-c-normal.json — single `synth` track (Core Test Synth), same notes as case a.
- Command: `"D:\Core-build\s1-red\build\engine\core-engine.exe" render "D:\Core-build\s1-red\probe\song-c-normal.json" "D:\Core-build\s1-red\probe\c-normal.wav" --tail-ms 500`
- **Exit code: 0.** 48000 Hz, block 512, 24-bit stereo, 2.5 s.
- Engine JSON answer (verbatim, D:\Core-build\s1-red\probe\c-answer.json):

```json
{
  "ok": true,
  "result": {
    "channels": 2,
    "conversionNotes": [],
    "denseEventBlocks": 0,
    "missing": [],
    "missingAssets": [],
    "path": "D:\\Core-build\\s1-red\\probe\\c-normal.wav",
    "peak": 0.6250405311584473,
    "realtimeFactor": 42.07103947441492,
    "renderMs": 59.4233,
    "sampleRate": 48000.0,
    "seconds": 2.5,
    "stems": []
  }
}
```

- WAV: D:\Core-build\s1-red\probe\c-normal.wav — 720,044 bytes; sha256 `ff4b1f30d4fe35911fc2c249adc6f18fa81947885ec7a76384b6029f5793548b`; measured peak 0.62504053115844727 (non-zero audio ✓).
- **Cross-check: case c WAV is BYTE-IDENTICAL to case a WAV (same sha256).** The load-failed `ghost` track contributed exactly zero samples, and the engine could not tell the two exports apart — a "succeeded" export with a needed plugin missing is indistinguishable from the healthy render of what remains.

### Case b — CRASH DURING RENDER (Core Test Crash, access violation in processBlock at 0.1 s) — DEFECT CONFIRMED (worse: no trace in result JSON)

- Song: D:\Core-build\s1-red\probe\song-b-crash.json — tracks `crash` (Core Test Crash, id VST3-Core Test Crash-fa11cd27-bc3b4f37) + `synth` (healthy), same notes as a/c. The Crash variant crashes unconditionally 0.1 s into playback (TestPlugins.cpp:212-215); no env seam needed (CORE_TEST_CRASH is the compile-time variant flag).
- Command: `"D:\Core-build\s1-red\build\engine\core-engine.exe" render "D:\Core-build\s1-red\probe\song-b-crash.json" "D:\Core-build\s1-red\probe\b-crash.wav" --tail-ms 500`
- Measured with a 60 s watchdog first (Start-Process): **no hang — process exited by itself in 1.49 s.** Exit code captured via direct invocation (Start-Process with redirected handles returned an empty ExitCode property — measurement artifact, re-run): **EXIT CODE: 0.** 48000 Hz, block 512, 24-bit stereo, 2.5 s.
- The crash really happened — stderr (D:\Core-build\s1-red\probe\b-stderr.txt): both plugins loaded for the export graph, then
  `plugins: a crashed plugin is left as it is: its instance is leaked on purpose, never released or deleted`
  (the host's SEH catch fired; graph.cpp:90-108 marks the unit failed, :185 silences the track).
- Engine JSON answer (verbatim, D:\Core-build\s1-red\probe\b-answer.json):

```json
{
  "ok": true,
  "result": {
    "channels": 2,
    "conversionNotes": [],
    "denseEventBlocks": 0,
    "missing": [],
    "missingAssets": [],
    "path": "D:\\Core-build\\s1-red\\probe\\b-crash.wav",
    "peak": 0.6250405311584473,
    "realtimeFactor": 22.914316039746257,
    "renderMs": 109.1021,
    "sampleRate": 48000.0,
    "seconds": 2.5,
    "stems": []
  }
}
```

- WAV: D:\Core-build\s1-red\probe\b-crash.wav — 720,044 bytes; sha256 `ff4b1f30d4fe35911fc2c249adc6f18fa81947885ec7a76384b6029f5793548b`; measured peak 0.62504053115844727. **BYTE-IDENTICAL to the healthy control (case c)** — the crashed track is silent (it clears its buffer before crashing, then is muted), the synth plays on, and the export "succeeds".
- **`missing: []`** — the load-time list cannot see a render-time crash, and nothing else in the result (or in RenderJobs::describe for the app path) reports it. `failedUnits` exists only in `engine.stats`/state (server.cpp:470, :870), never in the export answer. The only trace is one stderr line. App path: job `succeeded` → "Exported" (MainComponent.cpp:6765) after a plugin crashed mid-render.
- Reproducibility: 3 runs (watchdog run + 2 direct), identical sha256 every time, exit 0 every time.

### Case d — EXISTING WAV (degraded export over a previous healthy export) — DEFECT CONFIRMED (healthy export replaced, NO backup on the success path)

Continuation note: the previous agent ran this case (artefacts `d-a-*`, `d-b-*`, `d-prev-*`, `d-a2-*` in probe, UTF-16LE `FF FE` BOM from Windows PowerShell redirection; content recovered and kept) and confirmed verbally that the previous healthy export was overwritten with no backup, but died before writing this section. The sequence below re-runs the case cleanly and adds a content-distinct proof (sha256 alone cannot show replacement here because the degraded WAV is byte-identical to the healthy control).

- Setup: `Copy-Item c-normal.wav d-target.wav -Force`. BEFORE: 720,044 bytes, sha256 `ff4b1f30d4fe35911fc2c249adc6f18fa81947885ec7a76384b6029f5793548b`, LastWriteTime 00:27:46 (copy preserves the source mtime). The previous agent's final `d-target.wav` state was recorded before the reset: same sha256 `ff4b1f30…` (their last run was the degraded case-a render over their distinct previous export — consistent with their verbal finding).
- Run 1 — command: `"D:\Core-build\s1-red\build\engine\core-engine.exe" render "D:\Core-build\s1-red\probe\song-a-loadfail.json" "D:\Core-build\s1-red\probe\d-target.wav" --tail-ms 500`
  - **Exit code: 0.** JSON: probe\d-answer.json — `ok:true`, `missing:[{key:"track:ghost", plugin:"Ghost Missing Plugin", why:"not found in …no-such-plugin.vst3…"}]` (load-time attachment only, main.cpp:710), `peak:0.6250405311584473`, 48 kHz / 2 ch / 24-bit / 2.5 s. stderr: probe\d-stderr.txt (same engine lines as a-stderr.txt; measurement artifact: pwsh `2>` wraps the first native stderr line in a NativeCommandError record — engine output itself intact, JSON on stdout unaffected).
  - AFTER: 720,044 bytes, sha256 `ff4b1f30…` (unchanged — degraded render == healthy control byte-for-byte), **LastWriteTime 00:27:46 → 00:32:55: the file WAS replaced.** Because the bytes are identical, replacement is proven by mtime here and by content in runs 2–3.
- Run 2 (content-distinct previous export) — `render song-d-previous.json d-target.wav --tail-ms 500` (the previous agent's distinct song: single healthy synth track, pitches 67/71 instead of 60/64). **Exit 0**, probe\d-prev2-answer.json: `ok:true`, `missing:[]`, **peak 0.6091101169586182** (different audio). d-target.wav sha256 now **`d917fe282675c7016151af5a2e8b560eb5a5b346e515a33acd8f4c395ac872fe`** — a healthy, content-distinct export occupies the target.
- Run 3 (the overwrite) — `render song-a-loadfail.json d-target.wav --tail-ms 500` again. **Exit 0**, probe\d-overwrite-answer.json: `ok:true`, ghost in `missing` (load-time only), peak 0.6250405311584473. d-target.wav sha256 **back to `ff4b1f30…`**: the distinct healthy export (`d917fe28…`) was REPLACED by the degraded render. No refusal, no warning beyond the load-time `missing` attachment, exit 0.
- **Backup artefact check (measured):** `Get-ChildItem probe -Force -Recurse` filtered on `\.(bak|previous|part|tmp)$|^~|\.previous-` after runs 1 and 3 → **NONE**. Full directory listing shows only the expected probe files. The old healthy content is gone with no recoverable copy beside the target.
- **Overwrite mechanism (code-confirmed; the CLI does NOT truncate the target directly):**
  - CLI: main.cpp:677-712 → Server::render. server.cpp:795 `writeWav(path + ".part", …)` (stems: :796-797), then server.cpp:799 `publishRenderOutputs(outputs, published)` — the 2-arg call takes the defaults of render_job.hpp:74-76 (`workingPaths={}` → legacy `<path>.part`, `rollback=nullptr`). The target file itself is never opened for writing: `writeWav` → `WavWriter` → `openForWriting` = `_wfopen(…, L"wb")` (wav.cpp:31-37, called :68) creates/truncates only the `.part` working file. Replacement happens by rename inside the shared publisher.
  - Shared publisher `publishRenderOutputs` (render_job.cpp:110-186, used by BOTH CLI and app): reserves `<target>.previous` aside names (:124-133, `-2`/`-3`… on clash); renames each existing target to its `.previous` aside (**:158**); renames each `.part` over the target (**:165** — this is the overwrite); then **on success DELETES the `.previous` asides: render_job.cpp:181-184, `fs::remove(asides[f], ignored)` at :183**. That is the absence-of-backup citation: a successful publish always destroys the old file's only copy.
  - App path: render_job.cpp:356-393 renders ALL files first into exclusively reserved working files `<target>.<pid>-<seq>.part` (:370; `reserveRenderWorkingFile` :58-89, `CREATE_NEW` :75 — BUG-SWEEP-02), only `missingAssets` aborts (:363-367), no missingPlugins/failed-unit check; publishes after ALL files (:394-408, publish call :405 with `&job->rollback`), `State::Succeeded` :408 → same publisher, same success-path deletion of `.previous` at :183.
  - **SWEEP-03 contrast:** `.previous` backups survive ONLY on the FAILED-publication rollback path: `undo()` render_job.cpp:137-151 removes placed new files and renames asides back; a restore that fails keeps the old copy as `.previous`, named in the error (:145-147, :172-178; BUG-SWEEP-03). `PublishRollback` (render_job.hpp:66-73: "a failure never deletes an old file") is reported by `describe` only in `State::Failed` (render_job.cpp:432-439, `backups` :438). Success-with-degraded-audio is `Succeeded`, so the rollback machinery never runs and the aside is deleted — the data-priority guarantee ("never loses work") is inverted on exactly this path.
  - Side observation (not the defect above): the CLI still uses the legacy FIXED `<target>.part` name opened with `"wb"` truncation (server.cpp:795; render_job.hpp:36-38 "empty = the legacy `<path>.part` names"); SWEEP-02's exclusive reservation covers the app path only. A retained `.part` from an earlier interrupted CLI run would be silently consumed.
- Verdict: **a "succeeded" export with a needed plugin missing silently replaces the previous healthy export and deletes its only backup.** CLI result says `ok:true` + exit 0 (the `missing` attachment is load-time information, not a refusal); the app path would show "Exported" (MainComponent.cpp:6765) with no missing-plugin field anywhere in `RenderJobs::describe`. DATA-priority escalation: this is work loss, not merely a silent track.

### Case e — STEMS (crash track + healthy track, CLI --stems) — DEFECT CONFIRMED (silent crash stem published as success); partial-set question answered by measurement + code verdict

- Command: `"D:\Core-build\s1-red\build\engine\core-engine.exe" render "D:\Core-build\s1-red\probe\song-b-crash.json" "D:\Core-build\s1-red\probe\e-stems.wav" --stems --tail-ms 500`
- **Exit code: 0.** Engine JSON (verbatim, probe\e-answer.json): `ok:true`, `missing:[]`, `missingAssets:[]`, `peak:0.6250405311584473`, `renderMs:728.9483`, 48 kHz / 2 ch / 24-bit / 2.5 s, and
  `"stems": ["D:\\Core-build\\s1-red\\probe\\e-stems-crash.wav", "D:\\Core-build\\s1-red\\probe\\e-stems-synth.wav"]` — naming per server.cpp:767-772 (`<base>-<trackId>.wav`).
- Files produced (all 720,044 bytes, 48 kHz / 2 ch / 24-bit; independent byte-level parse via probe\wav-info.ps1):

| file | sha256 | measured peak |
|---|---|---|
| e-stems.wav (mix) | `ff4b1f30d4fe35911fc2c249adc6f18fa81947885ec7a76384b6029f5793548b` | 0.62504053115844727 |
| e-stems-crash.wav | `9363ef6d68b9da36aa1cd6100db07f0dea5afaa77376614baf36105e41da79e6` | **0 (pure silence)** |
| e-stems-synth.wav | `ff4b1f30d4fe35911fc2c249adc6f18fa81947885ec7a76384b6029f5793548b` | 0.62504053115844727 |

  Mix and synth stem are BYTE-IDENTICAL to the healthy control (case c): the crashed track contributed exactly zero. **The crash stem is an all-zero WAV published as a successful stem** — indistinguishable from a genuinely silent track; nothing in the result marks it.
- stderr (probe\e-stderr.txt): **three** `plugins: a crashed plugin is left as it is: its instance is leaked on purpose, never released or deleted` lines — one per render pass (mix, crash-solo, synth-solo): the crash plugin is instantiated and crashes even when the OTHER track is soloed, so a stems export leaks 3 crashed instances. SEH catch fires every time (graph.cpp:90-108); the render continues; `missing:[]` — a render-time crash has no field anywhere in the answer.
- Why silence is publishable (code-confirmed): `checkExportLevel` render_job.cpp:238-249 refuses only NaN/Inf (`broken`) or peak > 0 dBFS; **peak 0 passes**. CLI checks mix + every stem before writing (server.cpp:783-793); app checks each file at render_job.cpp:392.
- Render/write ordering (code-confirmed): `renderProject` server.cpp:144-170 renders the **mix first** (:162 `renderOnce(-1,…)`) then **each solo stem** (:163-166), all into memory; the CLI then writes ALL `.part` working files one-by-one (server.cpp:795-797) and publishes ALL outputs in one `publishRenderOutputs` call (:799). App path: all files rendered into reserved working files first (render_job.cpp:356-393), publish after ALL (:394-408).

**Partial-set question — can a crash/failure mid-stems leave a PARTIAL set published?**

- MEASURED (process death by `Stop-Process -Force` = TerminateProcess, file-triggered kills; no in-process seam needed):
  - Death during startup/render (e-k0, kill at ~300 ms): **no files at all** — no finals, no `.part`; a previous export would be untouched.
  - Death during the sequential `.part` writes (e-l1/l2/l3, 10 s song probe\song-e-long.json to widen the write window; kill at first/second/third `.part` appearance, ~0.9 s): **orphan `.part` debris only, ZERO final files written**. Pre-created healthy targets (copies of c-normal.wav) survived with original mtime 00:27:46 and size 720,044 — untouched. Orphan header state proves the phase: e-l1.wav.part + e-l1-crash.wav.part complete with PATCHED headers (dataChunkSize 3,024,000 = 10.5 s finished writes), e-l1/l2/l3-synth.wav.part partial with UNPATCHED construction headers (riffSize 36, dataChunkSize 0; wav.cpp:72-75 vs finish() :102-109). 9 orphan `.part` files kept in probe as evidence. **A death mid-write cannot produce a partial FINAL set: every write precedes any publish.**
  - Death just after publish (e-kA/kB, 2.5 s song, kill at first `.part` / first placement trigger): landed after the last rename — FULL degraded set in place, answer JSON 0 bytes: files committed, caller gets no success answer. (Warm runs write+publish in a few tens of ms; the triggers could not land earlier.)
  - `.previous` aside phase (e-kC, pre-created targets, trigger on `.previous` appearance): the aside phase (3 renames, ~1 ms) fell inside the 2 ms polling blind spot; the process completed normally and **no `.previous` survived** — consistent with the success-path deletion at render_job.cpp:183.
- NOT MEASURABLE headless without new seams (DECLARED): death BETWEEN the individual placement renames. The window is sub-millisecond (3 renames); 6 kill attempts at 1–2 ms polling + Stop-Process latency never landed inside it. The only pause seams (`testHooks.beforePublish` / `afterPublishClaimed` / `beforePlace`, render_job.hpp:114-119, invoked render_job.cpp:394/:404/:405) live on the app-path RenderJobs and are wired by tests — the CLI `render` mode exposes none. **Code verdict (reading, not measurement):** `publishRenderOutputs` places files one rename at a time (render_job.cpp:163-168); a TerminateProcess between renames leaves earlier stems at their final paths and later ones absent — a PARTIAL set with no undo, because `undo()` runs only on in-process exceptions (:169-179), never on process death. With pre-existing targets there is an additional aside window (:153-161): death there leaves the old content at `.previous` with the target path absent (recoverable, but the export has vanished until manual restore). The app path shares this hole: its atomicity is "all files rendered before publish", NOT crash-atomic placement.
- In-process failure mid-stems (code-confirmed, both paths): app — any exception while rendering a later file (missingAssets :363-367, NaN/clip :392, disk full, write error) → `State::Failed` + `discard()` removes the job's owned working files (:409-414, :345-349) → NOTHING published, no partial set. A mid-publish exception → `undo()` restores old targets; new files that could not be removed are REPORTED as `published` > 0 (describe :434) — a partial set survives only if the undo's own remove fails, and it is named. CLI — same publisher, same undo; kept `.previous` copies are named in the error text (:172-178). **But an SEH-catched plugin crash is NOT an exception: it never fails the job — measured above, the full set including the silent crash stem is published with exit 0.**
- Debris side-note (measured + code): the CLI's legacy FIXED `<target>.part` names (server.cpp:795; render_job.hpp:36-38) are left behind after process death and silently truncated (`"wb"`, wav.cpp:31-37) by the next CLI export to the same target; SWEEP-02's exclusive reservation covers the app path only (render_job.cpp:58-89).
- Verdict: **stems export "succeeds" with a fully silent crash stem (peak 0), exit 0, no failure field.** The set is all-or-nothing for in-process render failures and for process death during render/write (measured), but NOT crash-atomic for process death inside the sub-ms publish rename sequence (code verdict; injection infeasible headless without new seams). The fix must make a crashed/failed track refuse the WHOLE stem set, not publish silence.

## FINAL SUMMARY — S1 red reproduction complete (cases a–e)

All measurements: headless engine CLI, Debug build @ a9a0384, 48 kHz / block 512 (server.cpp:775) / 24-bit stereo, `--tail-ms 500`. Engine: D:\Core-build\s1-red\build\engine\core-engine.exe. Evidence: D:\Core-build\s1-red\probe\ (songs, JSON answers, stderr, WAVs, orphan `.part` debris, wav-info.ps1 parser).

| case | result | key evidence | sha256 (where relevant) |
|---|---|---|---|
| a — load failure | **DEFECT (measured):** render "succeeds", failed track silent | exit 0, `ok:true`; ghost track in `missing` ONLY as load-time attachment (main.cpp:710) + stderr; no refusal anywhere in Server::render | WAV `ff4b1f30…548b` — byte-identical to healthy control |
| b — crash during render | **DEFECT (measured, worse):** render "succeeds", crash invisible in result | exit 0, `ok:true`, `missing:[]`; SEH catch (graph.cpp:90-108) → track muted; only trace = 1 stderr line; `failedUnits` never in export answer; 3 runs identical | WAV `ff4b1f30…548b` — byte-identical to control |
| c — healthy control | GREEN (measured) | exit 0, non-zero audio, peak 0.62504053115844727 (independent parse == engine JSON) | WAV `ff4b1f30…548b` |
| d — existing WAV | **DEFECT (measured, DATA loss):** degraded export replaces previous healthy export, NO backup | content-distinct proof: healthy `d917fe28…72fe` → degraded `ff4b1f30…548b`, exit 0 both; zero `.bak/.previous/.part/~` artefacts (recursive scan); success path DELETES the `.previous` aside (render_job.cpp:181-184, remove at :183); overwrite = rename, not truncation (:158 aside, :165 place; CLI `.part` via server.cpp:795-799) | before `d917fe282675c7016151af5a2e8b560eb5a5b346e515a33acd8f4c395ac872fe` → after `ff4b1f30…548b` |
| e — stems | **DEFECT (measured):** silent crash stem published as success; partial-set analysis | exit 0, `stems:[e-stems-crash.wav, e-stems-synth.wav]`; crash stem peak **0** (`9363ef6d…79e6`), mix/synth stem == control; 3 SEH crashes + 3 leaked instances per export; silence passes checkExportLevel (render_job.cpp:238-249). Death mid-render/mid-write: NO partial final set, orphan `.part` only (measured, 6 kills); death between publish renames: partial set possible (CODE VERDICT — sub-ms window, no headless seam) | mix + synth stem `ff4b1f30…548b`; crash stem `9363ef6d68b9da36aa1cd6100db07f0dea5afaa77376614baf36105e41da79e6` |

**Escalated finding (DATA priority):** a "succeeded" export with a needed plugin missing (a) or crashed (b) silently REPLACES the previous healthy export (d) and deletes its only backup — the `.previous` aside created during publish is removed on the success path (render_job.cpp:183). SWEEP-03's `.previous` backups exist ONLY on the FAILED-publication rollback path (undo :137-151; PublishRollback render_job.hpp:66-73 "a failure never deletes an old file"; reported by describe only in State::Failed, :432-439). Success-with-degraded-audio bypasses all of it: job `succeeded` → "Exported" (MainComponent.cpp:6765). This violates quality criterion 1 ("Never loses work") and criterion 2 ("A failing plugin never takes the song down") — here the failing plugin takes the PREVIOUS EXPORT down. Stems (e) additionally publish an all-zero crash stem as a deliverable.

**What the fix must satisfy:**
1. REFUSE the export when a needed plugin fails to load OR fails during render (missingPlugins / failed units, incl. SEH-marked crashes): non-zero exit / `ok:false` (CLI) and `State::Failed` with error (app), BEFORE any publish. The result must name plugin / track / why (the `{key, plugin, why}` shape already exists at graph.cpp:300 and in `missing`).
2. Distinguish intentional bypass from failure: a user-bypassed effect/instrument is not a refusal reason; `failed()` / missingPlugins are. (Silent-by-bypass renders legitimately; silent-by-failure must refuse.)
3. On refusal the previous WAV stays BYTE-IDENTICAL (sha256 unchanged), no degraded/partial file at the target, and NO "Exported" notice (MainComponent.cpp:6765 must not fire).
4. Stems atomic: a failed/crashed track refuses the WHOLE set (no silent stem published); placement must be all-or-nothing even against process death mid-publish (the sub-ms rename window, render_job.cpp:163-168 — today only "all rendered before publish", not crash-atomic).
5. The CLI's load-time `missing` attachment (main.cpp:710) is NOT a substitute for refusing: information ≠ rejection; it also cannot see render-time crashes (`missing:[]` in case b). `RenderJobs::describe` (render_job.cpp:417-443) needs a plugin-failure field for the app path, which today has none.
6. Protect the previous export on the SUCCESS path too (refusal per 1–3 makes the overwrite impossible; any future success-with-warnings design must keep a recoverable backup instead of deleting the aside at :183).

**Limitations (declared):**
- Headless engine CLI only; NO UI click-through — the app "Exported" notice and `render.start` job flow are code-cited (MainComponent.cpp:6765; server.cpp:700-733; render_job.cpp), not clicked.
- Debug build (555 targets); runtime measurements on the CLI render path, block 512 (server.cpp:775). The app RenderJobs path uses kBlock 4096 (render_job.cpp:55) and WorkerPool — its refusal gap is code-confirmed, not runtime-measured here.
- 48 kHz / 24-bit / stereo only; other rates/bits/mono not exercised.
- Mid-publish process-death partial set: CODE VERDICT only — injection infeasible headless without new seams (testHooks are app-path/test-only, render_job.hpp:114-119); 6 timed kills landed before/after the sub-ms window, never inside it.
- Crash seam is the compile-time Core Test Crash variant (unconditional AV at 0.1 s, TestPlugins.cpp:202-222); other real-world failure modes (hang, slow corruption, NaN output) not exercised here (NaN/clip refusal exists: checkExportLevel).
- Repo READ-ONLY @ a9a0384: no fix implemented; this is red reproduction only. Pre-existing uncommitted changes untouched.

**Proposed ledger lines (for the scribe — BUG namespace per D75; this agent cannot write ESTADO.md or docs/relatorios/, repo read-only):**
- BUG candidate 1 — comprovado por teste (cases a/b/c/e): export succeeds despite plugin load failure or render-time crash; failed track silent; no refusal, no plugin-failure field in Server::render result or RenderJobs::describe; CLI `missing` is load-time only. Evidence: probe\a/b/c/e-answer.json + WAV sha256 equality with control.
- BUG candidate 2 — comprovado por teste (case d), DATA priority: successful degraded export replaces the previous healthy export and deletes its `.previous` backup (render_job.cpp:183); SWEEP-03 rollback covers failed publishes only. Evidence: d-prev2/d-overwrite answers, sha256 transition d917fe28→ff4b1f30, zero backup artefacts.
- BUG candidate 3 — confirmado no código + parcialmente comprovado por teste (case e): stem publication is not crash-atomic (per-file renames, render_job.cpp:163-168); process death mid-publish can leave a partial set (code verdict; mid-write death measured safe). Silent crash stem published (measured).
- Estado medido proposal: "S1 red reproduced a–e headless @ a9a0384 (Debug, 48 kHz/block 512/24-bit): degraded exports byte-identical to healthy control (sha256 ff4b1f30…), previous export overwritten without backup, silent crash stem published; fix not implemented (read-only repro)." Source: this notes file + probe artefacts, 2026-11-20.

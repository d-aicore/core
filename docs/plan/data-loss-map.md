> Translated from `perda-de-dados.pt.md`, a study of commit e7968c8. Line numbers refer to e7968c8 and have drifted since; they are kept as written and not re-anchored to current HEAD.
>
> Verdict legend (1:1, none upgraded or downgraded): **confirmado por leitura** = confirmed by reading · **reproduzido** = reproduced · **confirmado e reproduzido** = confirmed and reproduced · **parcial** = partial · **refutado** = refuted (**refutado na prática** = refuted in practice) · **por leitura** = by reading · **não verificado** = not verified. The owner's quotes stay verbatim in Portuguese.

> Delivery of the subagent `mapa-dados` (DSH session 2ff54f03, 2026-11-18), extracted from the log without editing. Read-only study of commit e7968c8. In Portuguese; to be translated into English before entering the repo.

# Data loss/corruption map — Core @ e7968c8

Project saving is solid; the real risks are around it: audio cleanup, plugin state, recovery and undo. Six points were reproduced with an executable outside the repo; the rest are by reading.

- **Reproductions:** `%TEMP%\core-mapa-dados\` (`repro.cpp`, compiled with MSVC/Ninja against the HEAD sources). Result: 9 tests, 6 expected reds, 3 greens «bem» (good).
- **Repo:** not touched; `git status` the same as at the start.
- **Report:** I wrote nothing in `docs/relatorios/` and proposed no `ESTADO.md` lines (the order was read-only). AGENTS.md asks for both; they are left to you.

## Verdicts on the leads

| Lead | Verdict | Where |
|---|---|---|
| AC-01 predictable `.part` | **Partial**, no real loss scenario | item 12 |
| AC-02 rollback ignores failures | **Confirmed and reproduced** (rare double failure; the original stays in `.previous`, it is not lost) | item 12 |
| AC-03 predictable `.saving-<n>` | **Refuted in practice** | the «Bem» (fine) section |
| AC-04 installer and samples | **Partial** | item 11 |
| AC-05 `app-<versão>.part` | **Refuted** for user work | the «Bem» (fine) section |
| AC-06 undo without live plugin state | **Confirmed by reading** | item 3 |
| ART-018 silenced NaN | **Confirmed and reproduced** | item 9 |

## List ordered by probability × damage

### 1. «Remove unused» sends other songs' audio to the Recycle Bin — reproduced
- **Code:**
  - [assets.cpp:352](<CORE/app/domain/src/assets.cpp#L352>) «`const fs::path folder = projectDir / kAssetFolder;`»
  - [MainComponent.cpp:4106](<CORE/app/desktop/src/MainComponent.cpp#L4106>) «`for (const auto& p : strayNow) candidates.push_back(p);`»
  - [MainComponent.cpp:4100](<CORE/app/desktop/src/MainComponent.cpp#L4100>) «`if (f.moveToTrash()) ++trashed; else ++failed;`»
  - The project folder is the file's folder ([MainComponent.cpp:3130](<CORE/app/desktop/src/MainComponent.cpp#L3130>)), so all songs in the same folder share `<pasta>\Audio`.
- **Scenario:** real exposure on this machine. `D:\Core-Projects` has `diogo.core`, `Psy demo.core` and `Song.core` at the root; the only WAV in `Audio\` (111 MB) belongs to `diogo.core`. Open `Song.core` → Library > Project → «Remove 1 unused» → Remove. The dialog says «audio file(s) of this song».
- **Another case:** `Documents\Audio` has 8 WAVs imported by Core with no `.core` beside them; any song saved in Documents (the default folder, [MainComponent.cpp:4224](<CORE/app/desktop/src/MainComponent.cpp#L4224>)) lists them as «Files the song does not know».
- **What is lost:** the other song's audio, which opens with missing clips. Recoverable from the Recycle Bin if the user notices.
- **Minimal fix:** `strayAudioFiles` reads the folder's `*.core` and `*.core.bak` and excludes what they reference; if any cannot be read, it returns nothing. Also fix the dialog text.
- **Red test:** `tests/bug_tests.cpp`. Two songs with audio in the same folder; `filesNoStateNeeds(History(A), strayAudioFiles(A, dir), dir)` must come back empty. Today it measures 1 and B is left with 1 missing file.
- **Effort:** small.

### 2. The engine dies and the plugins' sound since the last Save is lost — confirmed by reading
- **Code:**
  - [MainComponent.cpp:6384-6387](<CORE/app/desktop/src/MainComponent.cpp#L6384-L6387>): the autosave asks for the states and applies them only to the local copy («`else doc::applyPluginStates(document, states);`»), never to `history_`.
  - [EngineSession.cpp:593](<CORE/app/desktop/src/EngineSession.cpp#L593>) «`… The document is intact.`»
  - [juce_host.cpp:278-283](<CORE/engine/src/plugins/juce_host.cpp#L278-L283>): the new instance is born from the document's `state`.
- **Scenario:** 40 minutes tuning a synthesizer without saving; a plugin takes down `core-engine.exe`; Restart. The plugins come back as at the last Save.
- **Aggravating factor:** the autosave on disk had the sound from less than 60 s ago, but on the next tick it is rewritten with the old states. The good one stays one cycle in `.core-autosave.bak` and disappears.
- **What is lost:** all sound design since the last Save/Export, without warning.
- **Minimal fix:** in the autosave callback, when the states arrive, also do `history_.amend(document)` and `session_->setDocument(engineDocument())`, as in `fetchPluginStates`. Replace the message with one that tells the truth.
- **Red test:** new step in `--self-test` ([MainComponent.cpp](<CORE/app/desktop/src/MainComponent.cpp#L6660>), next to `killEngineForTest`). Change a CoreTestSynth parameter, force an autosave tick, kill the engine, restart, and the state must be the new one. In `tests/*.cpp` it only works with the logic extracted into the domain.
- **Effort:** small for the fix, medium for the test.

### 3. AC-06: removing a track or putting a sample over a plugin, and Undo brings the old state — confirmed by reading
- **Code:**
  - [MainComponent.cpp:4629](<CORE/app/desktop/src/MainComponent.cpp#L4629>) «`if (chosen == 3) edit([&](doc::Document& d) { doc::removeTrack(d, trackId); }, "Remove " + name);  // undo brings it back`» — without `fetchPluginStates()`.
  - Same at [4624](<CORE/app/desktop/src/MainComponent.cpp#L4624>) (replace instrument) and [4304](<CORE/app/desktop/src/MainComponent.cpp#L4304>) (`setTrackSample`, which does «`t.instrument = next;`» in [samples.cpp:1251](<CORE/app/domain/src/samples.cpp#L1251>)).
  - The engine releases the instance at [server.cpp:213](<CORE/engine/src/core/server.cpp#L213>); `stateId` is never written by the app.
  - The paths that already ask for the state (4597, 4617, 5408, [Mixer.cpp:342](<CORE/app/desktop/src/Mixer.cpp#L342>)) ignore the `false`.
- **Scenario:** Remove on the wrong track, or drop a sample on a plugin track, and Ctrl+Z. The plugin comes back with the sound of the last Save, or factory default if it was never saved.
- **What is lost:** that track's sound since the last capture.
- **Minimal fix:** centralize in `edit()`. If any plugin key of the current document disappears or changes file/id in `next`, call `fetchPluginStates()` and redo the `change`; if it fails, refuse with a message.
- **Red test:** `tests/domain_tests.cpp` for a new helper `droppedPluginKeys(antes, depois)` (non-empty for `removeTrack`, `replaceInstrument`, `setTrackSample`), plus a self-test step (remove the synth track, undo, compare state). Today the helper does not exist.
- **Effort:** small to medium.

### 4. After a crash, recovery may not be offered — reproduced
- **Code:** [Autosave.cpp:25-28](<CORE/app/desktop/src/Autosave.cpp#L25-L28>) only asks if a live process with that PID exists; [MainComponent.cpp:6463](<CORE/app/desktop/src/MainComponent.cpp#L6463>) «`if (!processRunning(a.process)) left.push_back(std::move(a));`».
- **Scenario:** Core crashes or the power fails; on the next start the old PID belongs to another program. «Recover unsaved work» does not appear.
- **What is lost:** in practice the unsaved work. The file remains in `%LOCALAPPDATA%\Core\Autosave` and may appear at a later start, already out of context.
- **Probability:** I estimate a few percent per crash; not measured.
- **Minimal fix:** consider it «a correr» (running) only if the image is `Core.exe` and the process creation (`GetProcessTimes`) is earlier than the autosave's `savedAt`.
- **Red test:** `tests/audit2_tests.cpp`. An autosave written with the test's own PID must be offered; today it measures 0, expected 1. I reproduced it with a literal copy of `processRunning`; it asks for the decision in the domain with an injectable probe.
- **Effort:** small.

### 5. Two windows on the same project: the last one to save erases the other — confirmed by reading
- **Code:** [Main.cpp:80](<CORE/app/desktop/src/Main.cpp#L80>) «`bool moreThanOneInstanceAllowed() override { return true; }`»; [MainComponent.cpp:3847](<CORE/app/desktop/src/MainComponent.cpp#L3847>) saves without checking whether the file changed on disk. I reproduced that there is only one generation of `.bak` (3 saves: v3 and v2 remain).
- **Scenario:** the same `.core` in two windows. A saves, B saves (A goes into `.bak`), B saves again and A's work is gone. With a single window: a mistake saved twice no longer has a good version.
- **What is lost:** a whole session, without warning.
- **Minimal fix:** store the file's size and date on open and on save; if it changed, ask (Overwrite / Save as / Cancel).
- **Red test:** `tests/domain_tests.cpp`. `saveFile(doc, path, carimboEsperado)` throws when the file changed; new API.
- **Effort:** small.

**Decision for you:** N dated copies (for example, the last 10) solves the second scenario, but it is a product decision.

### 6. Partial `ReplaceFileW` failure: the file «desaparece» (disappears) and the app says nothing changed — confirmed by reading, not reproduced
- **Code:** [storage.cpp:64-65](<CORE/app/domain/src/storage.cpp#L64-L65>) treats every error the same; [:124-126](<CORE/app/domain/src/storage.cpp#L124-L126>) deletes the temporary; [MainComponent.cpp:3849](<CORE/app/desktop/src/MainComponent.cpp#L3849>) «`The previous file was not changed.`».
- **Scenario:** on error 1177 the original has already been renamed to `.bak` and the new one did not land. The code then deletes the only new copy. Plausible with OneDrive or antivirus holding files. I quote the semantics of 1177 from memory of the documentation; when provoking it I only got 1175, which is benign.
- **What is lost:** nothing while the app stays open (saving again fixes it). If it closes, only `X.core.bak` remains with the previous version, and the older generation is already gone.
- **Minimal fix:** after the failure, if the target does not exist and the temporary does, `MoveFileExW(temp → alvo)`; never delete the temporary without the target in place; a message matching the real state.
- **Red test:** `tests/domain_tests.cpp`, with a seam to simulate 1177: after the error, `openFile(path)` must open one of the two versions.
- **Effort:** small.

**Cosmetic:** the message comes out «error 1175 (1175)» on non-English Windows, because `FormatMessageA` asks for English ([storage.cpp:36](<CORE/app/domain/src/storage.cpp#L36>)).

### 7. Plugin changes that are not parameters do not mark the song as changed — confirmed by reading
- **Code:** [juce_host.cpp:85](<CORE/engine/src/plugins/juce_host.cpp#L85>) and [:195](<CORE/engine/src/plugins/juce_host.cpp#L195>) only listen to `parameterValueChanged`. JUCE delivers the VST3 `setDirty` through `audioProcessorChanged` (`juce_VST3PluginFormatImpl.h:3520-3523`), which nobody listens to. [MainComponent.h:431](<CORE/app/desktop/src/MainComponent.h#L431>) depends on `pluginsDirty_`.
- **Scenario:** load a sample or wavetable into a plugin, or edit its internal sequencer, and close. No asterisk, no prompt, and the autosave does not fetch them either.
- **What is lost:** that change, if the user does not Ctrl+S out of habit.
- **Minimal fix:** `JuceProcessor` also becomes an `AudioProcessorListener` and, on `audioProcessorChanged` with `nonParameterStateChanged`, marks `changed_`.
- **Red test:** `tests/engine_tests.cpp` does not reach JUCE. It takes a test plugin in `app/selftest-plugins` that calls `updateHostDisplay` without touching parameters, with a self-test step requiring `unsaved()`.
- **Effort:** small, plus the test plugin.

### 8. The undo history shrinks to a few steps with large plugin states — reproduced
- **Code:** [history.hpp:14](<CORE/app/domain/src/history.hpp#L14>) (64 MB); [history.cpp:7](<CORE/app/domain/src/history.cpp#L7>) «`size_t sizeOf(const Document& d) { return toJson(d).dump().size(); }`»; [:20-23](<CORE/app/domain/src/history.cpp#L20-L23>) throws away the oldest. Each step is the whole document, base64 states included.
- **Measured:** 12 MB of state, 12 edits, 5 undo steps.
- **Scenario:** a song with several heavy plugins after the first Save. The user counts on 30 undos and has 5.
- **What is lost:** the ability to go back; not the file.
- **Minimal fix:** guarantee a minimum number of steps (for example 50) regardless of bytes. The good fix is sharing the state blobs between snapshots.
- **Red test:** `tests/domain_tests.cpp`, the case above with `CHECK_EQ(steps, 12)`.
- **Effort:** small for the minimum, medium for the sharing.

### 9. ART-018: a float WAV with NaN is silenced and the export says «Exported» — reproduced
- **Code:**
  - [graph.cpp:1247](<CORE/engine/src/core/graph.cpp#L1247>) zeroes the sub-block.
  - [render_job.cpp:290](<CORE/engine/src/core/render_job.cpp#L290>) uses the same graph, so the check at [:179](<CORE/engine/src/core/render_job.cpp#L179>) never sees the NaN.
  - [audio_file.cpp:139](<CORE/engine/src/io/audio_file.cpp#L139>) lets the float in raw.
  - The app's import only reads the header ([assets.cpp:36](<CORE/app/domain/src/assets.cpp#L36>)); this closes the ledger's «por verificar» (to verify).
- **Measured:** 100 NaN samples; `missingAssets: []`; `render.start` ends in `succeeded`; the file has 128 samples at zero.
- **Scenario:** only with a corrupted float WAV. Rare.
- **What is lost:** nothing of the project; an export comes out with holes, without warning.
- **Minimal fix:** check on load in `AssetStore::get` (not finite or |x| > 256) and report as missing audio; the export already aborts in that case.
- **Red test:** `tests/engine_tests.cpp`, the case above with `CHECK(state != "succeeded")`.
- **Effort:** small.

### 10. The user's sample catalogue rewritten when the index cannot be read — confirmed by reading
- **Code:** [samples.cpp:420](<CORE/app/domain/src/samples.cpp#L420>) refuses different versions; [MainComponent.cpp:4021](<CORE/app/desktop/src/MainComponent.cpp#L4021>) «`Sample index not read (it is made again)`»; [:4060](<CORE/app/desktop/src/MainComponent.cpp#L4060>) carries empty `overrides`; [:4174](<CORE/app/desktop/src/MainComponent.cpp#L4174>) writes over it without a copy.
- **Scenario:** a future version raises the index version and the user uses the installer's «Back»; or the file (5,4 MB on this machine) becomes damaged.
- **What is lost:** all manual corrections of role, key, BPM, tags and names.
- **Minimal fix:** if the index exists and cannot be read, rename it to `samples-index.unreadable-<data>.json` before writing the new one, and try to salvage only the `overrides`.
- **Red test:** `tests/bug_tests.cpp`. An index with `version` 99 and `overrides` must return the overrides through a new function `salvageOverrides(json)`.
- **Effort:** small.

### 11. AC-04: the installer overwrites and deletes samples with the same path — partial, by reading
- **Code:**
  - [Setup.cpp:89-91](<CORE/app/installer/Setup.cpp#L89-L91>): if size or date differ, it copies over.
  - [:108-111](<CORE/app/installer/Setup.cpp#L108-L111>): uninstall deletes the listed ones without comparing.
  - `copyFileTo` deletes before copying (`juce_File.cpp:317-320`).
- **Scenario:** the user edits in place a sample inside `<install>\Samples`, which is the library the app uses by default. The next update restores the original from `D:\Samples`; uninstall deletes it. Whoever edits in `D:\Samples` is not affected.
- **Minimal fix:** store size and date (or hash) in `samples.txt`; if the destination does not match the recorded one, neither overwrite nor delete. Copy to `.part` and rename.
- **Red test:** the installer has no suite; extract the decision into a pure function and test it in `tests/bug_tests.cpp`.
- **Effort:** medium.

### 12. Export: false messages and orphans — AC-02 reproduced, AC-01 partial; nobody loses files
- **AC-02:** [render_job.cpp:97](<CORE/engine/src/core/render_job.cpp#L97>) «`if (aside[f]) { fs::rename(asides[f], utf8(outputs[f].path), ignored); (void)ignored; }`» and [MainComponent.cpp:6325](<CORE/app/desktop/src/MainComponent.cpp#L6325>) «`No existing file was changed.`».
  - Single failure (with the `.part` locked): the original goes back in place.
  - Double failure (forced, with a folder in the destination's name): the original stays in `mix.wav.previous` and the error does not say so.
- **AC-01:** `.part` with "wb" only collides with two instances exporting to the same file at the same time. No real scenario.
- **By reading:**
  - Engine dead mid-way: orphan `.part`/`.previous` remain and the UI never reaches «failed»; «Exporting…» stays in the footer covering «The engine stopped».
  - [MainComponent.cpp:4655](<CORE/app/desktop/src/MainComponent.cpp#L4655>) ignores the `false` from `fetchPluginStates()`: it exports with old sound without warning.
  - The OS messages come out in the wrong code page.
- **Minimal fix:** include the `.previous` path in the error when the restore fails; the UI only claims «nada mudou» (nothing changed) in the absence of that warning; `disconnected` with an active job becomes `failed`; the export refuses (or warns) without states.
- **Red test:** `tests/audit_tests.cpp`, with `beforePlace` occupying the destination; the error must contain `.previous`.
- **Effort:** small.

## What I verified and is fine

- **Project saving** ([storage.cpp:106-131](<CORE/app/domain/src/storage.cpp#L106-L131>)): temporary in the same folder → `FlushFileBuffers` → re-read and byte-by-byte comparison plus parse → `ReplaceFileW` with `.bak` (or `MoveFileExW` with write-through the first time).
  - A crash or power failure before the replace leaves the original intact.
  - Reproduced: with the `.bak` locked the save is refused with an error, the previous one stays and no temporary is left over.
  - Caveat: that is item 6.
- **AC-03:** two processes with 400 saves each to the same file and the same `.saving-N` names gave 0 corrupted files and 69 refusals with a visible error; the final one opens. The predictable counter does not corrupt. Adding the PID to the name is hygiene.
- **Opening:** truncated, non-JSON and a future `schemaVersion` are refused without touching the file (reproduced byte by byte). Schema 1 and `core-project` open as imported and force a new name ([storage.cpp:90](<CORE/app/domain/src/storage.cpp#L90>), [MainComponent.cpp:3906](<CORE/app/desktop/src/MainComponent.cpp#L3906>)).
- **Unknown fields:** survive open and save at root, transport, loop, master, track, instrument, plugin, mix, pattern, note and clip (reproduced, empty diff). There is no destructive migration in schema 2/3.
- **Close, open another, new, recover:** all go through `confirmUnsaved` ([MainComponent.cpp:4776](<CORE/app/desktop/src/MainComponent.cpp#L4776>)). A failed save or one without plugin states does not close the song.
- **Plugin states on save:** with no answer in 5 s, it saves with the old ones, warns and keeps the song marked as changed ([MainComponent.cpp:3857-3865](<CORE/app/desktop/src/MainComponent.cpp#L3857-L3865>)).
- **Windows session end:** the dialog blocks shutdown (`juce_Windowing_windows.cpp:4359-4363`).
- **Autosave:**
  - Own folder, never over the song, safe write, every 60 s, failures warned.
  - Removed on a clean close, kept on a crash.
  - Up to 60 s of edits is lost, plus item 2.
- **Audio edits** (reverse, stretch, pitch, normalize, cut silence, slice): never rewrite the original; they create a new file with a non-existing name and import by content.
- **`importAudio`, `copyAssets`, `relocateAsset`:** copy to `.part`, verify the SHA-256 and rename; conflicts in Save As are refused without overwriting. Assets only accept relative paths without `..`.
- **AC-05:** reinstall refuses if there are extra files in `app-<versão>` ([Setup.cpp:176](<CORE/app/installer/Setup.cpp#L176>)). Uninstall only removes what the manifest lists. The `.part` belongs to the installer.
- **Export over an existing file:** disk space checked beforehand, peak above 0 dBFS and NaN in the output refuse it, and cancelling does not touch the destination. Stems only exist in the CLI.
- **Others:** deleting a template moves it to `.bak`; MIDI export via temporary and rename; pattern presets always pick a new name.

## What I could not verify

- **Nothing ran in the real app.** Items 2, 3 and 7 are by reading the full path, without engine, plugin and window running.
- **`ReplaceFileW` 1176/1177:** not reproduced, nor is power loss mid-replace.
- **`moveToTrash` on volumes without a Recycle Bin** or with files larger than it: I suspect it deletes for good, not tested.
- **Frequency of PID reuse:** the mechanism is reproduced, the probability is not measured.
- **Paths longer than 260 characters or outside code page 1252:** reading only. Saving and export use wide paths; `path::string()` appears only in error branches.
- **Step 3 of the request:** the test ideas of items 3, 4, 5, 10 and 11 ask for new API. Today they fail to compile, not at a `CHECK`. Items 1, 8, 9 and 12 give red directly with the current API.
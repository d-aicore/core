> Translated from `hosting-de-plugins.pt.md`, a study of commit e7968c8. Line numbers refer to e7968c8 and have drifted since; they are kept as written and not re-anchored to current HEAD.
>
> Verdict legend (1:1, none upgraded or downgraded): **[lido] → [read]** = confirmed by reading · **[medido] → [measured]** = measured · **[n/v] → [n/v]** = not verified. The owner's quotes stay verbatim in Portuguese.

> Delivery of the subagent `mapa-plugins` (DSH session 2ff54f03, 2026-11-18), extracted from the log without editing. Read-only study of commit e7968c8, with offline measurements. In Portuguese; to be translated into English before entering the repo.

# Core plugin hosting (HEAD e7968c8): robustness, state, capacity and VST2

The engine survives a crashing plugin, but cleaning up that instance hangs it forever — it is the most serious problem and it is measured end to end. The hosting is all in-process in `core-engine.exe` (separate from the GUI); only the scan and the qualification run in their own process. Marking: **[read]** confirmed by reading, **[measured]**, **[n/v]** not verified.

To measure I used, besides the offline render: `serve` with miniaudio's `null` backend (no real device), a probe VST3 compiled in `%TEMP%` and two excerpts compiled with the engine's flags. Nothing was written in the repo.

## 1. Real problems (by priority)

**P1 — Real crash in `processBlock`: survives, then hangs (ruins the use).**
- **Cause [read]:** `engine/src/plugins/juce_host.cpp:64-67` catches the crash with `__except (1)`. With `/EHsc` destructors do not run on an access violation, so JUCE's lock (`juce_VST3PluginFormatImpl.h:2422`, `processLock (processMutex)`) stays held. `juce_host.cpp:103` (`raw->releaseResources(); delete raw;`) asks for that lock again (`:2393`) and spins forever on the message thread.
- **Measured:**
  - In an excerpt with the engine's flags, the lock stays held after an AV and after a division by zero; it does not after a C++ `throw`.
  - CLI render with the probe (AV at 0,1 s): the process never ends (killed at 426 s, one thread at 100 %, no WAV).
  - In `serve`: `failedPlugins` says «crash», `plugin.retry` returns `loaded:true`, and then `plugins.states` and `engine.stats` time out at 10 s; the engine spends 0,95 s of CPU per second and does not exit when stdin closes.
- **Scenario:** a plugin crashes; the user removes it, replaces it, opens another song or exports. The engine goes mute to commands and the Restart button does not appear (`MainComponent.cpp:6298` only shows it in `Stopped`/`NoDevice`).
- **Minimal fix:** in `~JuceProcessor`, if `failed()`, do not call `releaseResources` or `delete` (deliberate leak); the same for the window. Add `failed()` to `showEditor`, `parameters` and `setParameter` (`:402`, `:454`, `:436`). Show Restart after repeated timeouts.
- **Red test:** `CORE_TEST_CRASH` variant in `TestPlugins.cpp` (AV after 0,1 s). `core-engine render` with a 30 s timeout must exit 0 and write the WAV; in `serve`, after a failure and `plugin.retry`, `plugins.states` answers in less than 2 s.

**P2 — FL-2: a failed plugin never comes back and the notice lies (ruins the use).**
- **Cause [read]:** `juce_host.cpp:306` returns the same failed instance on every rebuild. `plugin.retry` does not exist in `app/`. `MainComponent.cpp:6345` promises «silent until your next change».
- **Measured** (`CORE_TEST_THROW`, `null` backend): the track stays at 0,0 after `project.load`, `track.set` and Stop+Play; only `plugin.retry` restores it.
- **Minimal fix:** a «Tentar de novo» (Try again) action that sends `plugin.retry`, and fix the text. Depends on P1.
- **Red test:** selftest with Throw on an insert; after the failure, the app action must return peak > 0 on the track.

**P3 — Export «succeeded» with a missing or failed plugin (ruins the use).**
- **Cause [read]:** `render_job.cpp:295-299` only checks `missingAssets`; `:322-338` and `server.cpp:712-714` do not report failures.
- **Measured:** with Throw on an insert the render gives `ok:true` and the track comes out mute from 0,1 s; with a crash on load it also gives `ok:true`.
- **Minimal fix:** the job fails (or returns `failedPlugins`/`missing`) if `graph.missingPlugins()` is not empty or any `processor->failed()` at the end.
- **Red test:** `render.start` with Throw; the job state must not be `succeeded` without a warning.

**P4 — A plugin that does not load: the user is not warned (ruins the use).**
- **Cause [read]:** `snap_.missingPlugins` is only used to disable «Open…» (`MainComponent.cpp:4555`, `:4566`, `:5229`). `relinkPlugins` (`plugin_refs.cpp:63`) is never called.
- **Measured:** a crash in `prepareToPlay` and in `setStateInformation` gives `missing: "it crashed while it was loaded"`; the engine continues and the song plays without it.
- **Minimal fix:** a notice with name and reason, a mark on the track, and call `relinkPlugins` on open.
- **Red test:** open a song with a non-existing `file`; `notice_` must contain the plugin's name.

**P5 — The 8 MB per-message limit against plugin states (ruins the use with many synthesizers).**
- **Cause [read]:** `server.hpp:60` fixes 8 MB. `document.cpp:757` sends every plugin's `state` on every edit. `engine_client.cpp:185-188` refuses the send and `:285-289` kills the engine if the answer exceeds the limit.
- **Measured:** 27 Vital tracks with the default patch (318 128 base64 bytes each) give a `plugins.states` answer of 8 589 883 bytes, above the limit — every Save or Export would kill the engine. The `project.load` of 8 678 854 bytes got no answer.
- **Cost per edit [measured, under load]:** 157 ms with 32 tracks (1,09 MB message) and 59 ms with 8 Vital (2,7 MB), when the graph rebuild takes 1,2 ms.
- **Minimal fix:** do not send `state` when the live instance already exists; ask for states by key.
- **Red test:** test plugin with a 9 MB state; Save must not take down the engine.

**P6 — AC-06 and live state only in the engine (ruins the use after an engine crash).**
- **Without `fetchPluginStates()` beforehand [read]:** remove track (`MainComponent.cpp:4629`), duplicate (`:4628`), replace instrument (`:4624`), sample (`:4304`) and undo/redo (`:3481-3482`). The paths that do call it ignore the `false`.
- **Facts from the other map, confirmed [read]:** the autosave applies the states only to the copy (`:6384-6387`); the host only listens to parameters (`juce_host.cpp:85`, `:195`).
- **I add [read]:** `parameterValueChanged` also fires on the audio thread (`VST3PluginFormatImpl.h:2512-2519`), which gives false «modificado» (modified) flags while playing.
- **Scenario:** hours of tuning on a synthesizer, the engine falls, Restart restores the last Save.
- **Minimal fix:** in `pluginsChanged` (with debounce), fetch the states and do `history_.amend`; fetch before the five paths; register `audioProcessorChanged`.
- **Red test:** change `gain` live, `killEngineForTest`, restart; `plugins.states` must bring the new value. Duplicate the track: the copy's state must equal the live one.

**P7 — Editor and locks without guard or watchdog (ruins the use; not measured, no GUI).**
- **Cause [read]:** only `createEditorIfNeeded` is guarded (`juce_host.cpp:418`). `setVisible(true)` at `:428` fires `view->attached` (JUCE `juce_VST3PluginFormat.cpp:402-404`, `:486`) outside the guard, as do all the window's events. A crash there kills the engine.
- **Blocking [read]:** if the message thread blocks, `main.cpp:170` holds the lock and the protocol freezes. A block in `processBlock` stops the audio thread. There is no watchdog in the engine.
- **Minimal fix:** guard the window's creation and visibility; the app counts repeated timeouts and offers Restart.
- **Red test:** `CORE_TEST_HANG` (sleeps 30 s in `createEditor`); the app must offer Restart in less than 15 s.

**P8 — Invalid state ignored in silence (annoyance, loses sound).**
- **Cause [read]:** `juce_host.cpp:279-283` and `VST3PluginFormatImpl.h:2856` return no error. The app never reads `versionChanged`.
- **Measured:** non-base64, random or empty state gives `ok:true`, default sound and no warning. The next Save writes the default over it.
- **Minimal fix:** `project.load` returns the keys whose state was not accepted, and the app does not overwrite that state.
- **Red test:** `CORE_TEST_SYNTH` with `state:"!!!!"`; the load result must list the key.

## 2. Verdicts

- **FL-2:** confirmed [read + measured on the real-time path].
- **AC-06:** confirmed, partial — 5 paths without capture; the others capture but ignore the failure [read].
- **ART-011:** confirmed [read] — `juce_host.cpp:167` creates the view per block and JUCE allocates with ≥32 channels (`juce_AudioSampleBuffer.h:1214-1220`, `:1268`). It is not reachable with the cached plugins (maximum 12 channels, Multipass); the ones the scan gives as 0/0 stay [n/v].
- **ART-002:** confirmed [read `graph.cpp:246` + measured] — with Throw on a bus the master stays at 0,0, offline and live, and the track meters keep moving.

## 3. Measured capacity

**Conditions and corrections to the brief:**
- Machine: Ryzen 7 7800X3D (8 cores, 16 threads), 31 GB.
- Song: 30 s at 120 bpm, sixteenth notes with chords (3-4 voices), default patch.
- Cost = pure render time ÷ 30 s; 100 % means one core is no longer enough.
- The render is **not serial** by default: it uses up to 8 workers (`workers.hpp:44-59`, `server.cpp:112`). «Serial» below is `CORE_EXPORT_THREADS=0`.
- Real time in the app **is** serial: `liveThreads` is 0 and the app never sends it (`server.cpp:89-105`, `EngineSession.cpp:513-522`).
- The CLI instantiates each plugin twice (live + export), so the memory is that of 2N instances. In the app, exporting doubles the memory.
- One and Twin 3 are FabFilter's, not TAL's.

**Before the load warning (01:50-02:01, one run each):**

| Instrument | N | Serial %RT | Parallel %RT | Load live / export (s) | Commit MB |
|---|---|---|---|---|---|
| One | 1 | 0,24 | 0,22 | 0,11 / 0,02 | 36 |
| One | 8 | 1,78 | 0,59 | 0,22 / 0,12 | 133 |
| One | 16 | 3,85 | 0,93 | 0,37 / 0,25 | 243 |
| One | 32 | 7,87 | 1,55 | 0,59 / 0,49 | 462 |
| One | 64 | 16,64 | 2,92 | 1,04 / 1,04 | 900 |
| Twin 3 | 1 | 5,22 | 4,88 | 0,26 / 0,17 | 162 |
| Twin 3 | 8 | 45,8 | 8,1 | 1,43 / 1,39 | 1125 |
| Twin 3 | 16 | 105,5 | 16,1 | 2,95 / 3,11 | 2222 |
| Twin 3 | 32 | 307,7 | 33,8 | 6,31 / 6,61 | 4413 |
| Twin 3 | 64 | 1043,9 | 90,6 | 21,6 / 21,0 | 8809 |

**Under load from another agent (3 runs, best / median):**

| Configuration | Serial %RT | Parallel %RT | Commit MB |
|---|---|---|---|
| Vital ×1 | 1,50 / 1,54 | 1,48 / 1,49 | 311 |
| Vital ×8 | 15,5 / 15,6 | 3,7 / 3,8 | 2335 |
| Vital ×16 | 38,9 / 47,8 | 8,6 / 10,4 | 4652 |
| Vital ×32 | 87,3 / 94,0 | 13,6 / 13,9 | 9276 |
| Serum 2 ×1 | 1,40 / 1,43 | 1,39 / 1,46 | 550 |
| Serum 2 ×8 | 16,6 / 16,6 | 3,5 / 3,6 | 3832 |
| Serum 2 ×16 | 25,5 / 26,1 | 6,5 / 7,5 | 7579 |
| Serum 2 ×32 | 62,2 / 74,0 | 12,2 / 12,9 | 15063 |
| One + 1 effect | 0,29 / 0,30 | — | 54 |
| One + 4 effects | 2,23 / 2,27 | — | 201 |
| One + 8 effects | 7,43 / 7,67 | — | 505 |
| One + 16 effects | 20,9 / 22,0 | — | 854 |
| 8 × (One + 8 effects), 72 instances | 63,7 / 65,1 | 10,6 / 10,6 | 3128 |

The 72-instance project took 12,5 s to load the live ones and 11,1 s the export ones.

**Reading:**
- One is linear (0,22-0,26 % per instance).
- Twin 3 is not: 5,2 → 5,7 → 6,6 → 9,6 → 16,3 % per instance in serial. Vital and Serum 2 also grow.
- Practical ceiling of one core: about 15 Twin 3, about 32 Vital.
- Memory per instance (commit ÷ 2N): One ≈ 7 MB, Twin 3 ≈ 69 MB, Vital ≈ 145 MB, Serum 2 ≈ 235 MB. 32 Serum 2 in export take 15 GB.
- The cause of the non-linearity stays a hypothesis: memory and cache per instance. I tried to isolate it with blocks of 512 instead of 128 on Twin 3 ×16; the series came out inconsistent (128: 117 / 211 / 184 %; 512: 124 / 127 / 131 %) and I draw no conclusion.
- Each plugin adds 128 samples of latency (`juce_host.cpp:22`, `:133`): a chain of N adds up N×128 [read].

**What this does not prove:** nothing about deadlines, jitter, a real device, open editors, heavy presets or the real-time path with meters.

**Scan [measured]:** 109 files in 60,6 s, median 169 ms, maximum 2,8 s, zero failures. It runs in its own process with a 20 s limit (60 s in qualification) and per-file quarantine (`Plugins.cpp:8-28`, `:217-244`). Weaknesses [read]: the qualification does not open the editor nor test the state round trip; the quarantine does not stop the engine from loading that plugin from a project. Serum 2 appears twice because the bundle was added as a folder.

## 4. VST2

**(a) Installed:** 30 unique DLLs — 29 in `Common Files\VST2` and `Vital.dll`. All are x64 and export `VSTPluginMain` (I read the export table without loading them). **All have a VST3 equivalent in `plugins.json`**: none is left out. Today VST2 adds no plugin on this PC.

**(b) What the local JUCE 9.0.5 requires [read, not compiled]:**
- The `JUCE_PLUGINHOST_VST=1` flag (`juce_audio_processors_headless.h:70-76`).
- `pluginterfaces/vst2.x/aeffect.h` and `aeffectx.h` on the include path (`juce_VSTPluginFormatImpl.h:61-62`). They do not exist in `D:\Core-deps` or in `D:\CORE`.
- `juce_set_vst2_sdk_path(<pasta>)` (`JUCEUtils.cmake:2507-2525`) creates the `juce_vst2_sdk` target; in a host it must be linked by hand or the include added.
- With the flag, `addDefaultFormatsToManager` registers VST2 by itself (`juce_AudioPluginFormatManagerHelpers.cpp:44-45`).
- I saw no extra dependencies on Windows x64.

**(c) Headers:**
- JUCE uses 206 distinct `Vst2::` names: 45 `audioMaster*` opcodes, 45 `eff*`, 6 `effFlags*`, 13 `kPlugCateg*`, 31 `kSpeakerArr*`, 19 `kSpeaker*`, 32 `kVst*` constants and 15 types (`AEffect`, `ERect`, `VstEvents`, `VstMidiEvent`, `VstTimeInfo`, `VstSpeakerArrangement`, `VstPinProperties`…), plus `VSTCALLBACK`.
- Route (i): official headers from an old SDK. JUCE itself points at `vstsdk3610_11_06_2018_build_37` or JUCE 5.3.2 (`:56-60`).
- Route (ii): a compatible header written from scratch, covering those names with the exact binary layout of the structures. I estimate 300-400 lines [n/v].
- Licensing: [n/v]. Question for the owner, kept verbatim in Portuguese: «tem uma cópia legítima do SDK VST2 antigo, ou aceita um cabeçalho de interface escrito de raiz para uso privado?» — whether he has a legitimate copy of the old VST2 SDK, or accepts an interface header written from scratch for private use.

**(d) Core points to change [read]:**
- `engine/CMakeLists.txt:51-52`: flag and include.
- `juce_host.cpp:266` and `:529`: `getName() == "VST3"` filter; `:532`: `"format","vst3"` hardcoded.
- `juce_host.cpp:149-152`: outside VST3 the parameter id is the index.
- `juce_host.cpp:487`: preset VST3 only.
- `project.cpp:381`: refuses `vst`. The domain already accepts it (`document.cpp:283`, `edit.cpp:1369`); I measured that today the engine refuses the whole song with that format.
- `Plugins.cpp:47` and `:55`: only `.vst3` and only the VST3 folder. Pre-filter the DLLs by the export, without loading them.
- `plugin_refs.cpp:47-61`: the format prefix comes from the id, and the id depends on the path (`juce_PluginDescription.cpp:48-67`).
- «VST3» texts in `MainComponent.cpp:5207-5208` and `:5384`.
- State: VST2 stores an fxb chunk (`juce_VSTPluginFormatImpl.h:1581-1584`), incompatible with VST3's XML. Migrating the same plugin between formats loses the state [n/v].
- With VST2 on, the 30 would appear duplicated: a «VST3 primeiro» (VST3 first) rule is needed.

**(e) Risks:** P1, P3, P4 and P7 apply equally, without per-process isolation. Loading a DLL runs its `DllMain` (`juce_VSTPluginFormatImpl.h:640-645`), so only in the scan process. Whether JUCE's VST2 host holds any lock around process: [n/v].

**(f) Plan in slices (estimates [n/v]):**
1. Headers + flag, compile only. Test: green build. ~20 lines.
2. Test plugins also in VST2 (`FORMATS VST3 VST` in `app/selftest-plugins/CMakeLists.txt`; requires the same headers, `JUCEUtils.cmake:1580-1581`). Test: the artifacts exist.
3. Engine accepts `vst`: `project.cpp:381` and `juce_host.cpp:266`, `:529`, `:532` by format. Test: render of a VST2 `CORE_TEST_SYNTH` with peak > 0. ~60 lines.
4. DLL scan and quarantine with PE pre-filter and VST3-first deduplication. Test: a folder with a DLL that is not a plugin neither enters nor crashes. ~120 lines.
5. State round trip and automation by index. Test: save and reopen keeps `gain`. ~40 lines.
6. Interface and texts. ~40 lines.

**Recommendation:** do it, but only after P1 to P4, and behind a build flag until the header question is decided.

## 5. Investigated and ruled out

- **`/fp:fast` cancelling `std::isfinite` at `graph.cpp:120`:** in an excerpt with the engine's flags, NaN, ±Inf and excessive level are detected. With the probe, all three outputs silence the track. The inconsistency with `finite.hpp` remains.
- **Crash in `getStateInformation`:** the probe crashed right on load (JUCE asks for the state at initialization), so the path `juce_host.cpp:391-395` stays [read] only.
- **Kick 3, Phase Plant and Tactic without sound:** the qualification gives the warning «made no sound with notes». It matches «sem preset» (no preset), but it is [n/v].
- **Latency change:** handled in `serve` (`main.cpp:154`, `server.cpp:246-250`), not in export [read].
- **Bus change mid-playback:** Core's buffer has a fixed size (`juce_host.cpp:89-90`). Theoretical risk [n/v].

I did not write the report in `docs/relatorios` that `AGENTS.md` asks for, by express order. Scripts and logs are in `%TEMP%\core-mapa-plugins\`.
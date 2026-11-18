# Report — 2026-11-18 — Retired pattern preview (CLEAN-07) and inactive Rise action (BUG-LEGACY-RISE) removed

## What was measured

- Repository `d-aicore/core`, worktree `.agents/worktrees/flawless-deadcode2`, branch `flawless-deadcode2`.
  Base `ed4642e61b78e30fc4f7d950a28a214d0ac7c801` (main). Commits: `5a5eced` (CLEAN-07), `3345024`
  (cherry-pick of `fffbb41`, the CI unique-bat gate fix), `08ddc65` (BUG-LEGACY-RISE), plus this delivery's
  docs commit (report + index row + evidence archive).
- Scope and authorization: the owner's CodeRabbit report (8) of 2026-11-18 (archived verbatim), dispatched as two
  bounded removals. Product contract: Core has no built-in instruments/effects/oscillators/filters (removed
  2026-11-16); all sound comes from external plugins and audio tracks. The pattern waveform preview only worked
  for retired native instruments. Authorization chain quoted in report (8): owner's «mais» of 2026-11-18
  continuing the instruction to remove old native instrument/effect code; MIDI import must remain.
- Environment: Windows host (10.0.26300), MSVC 14.51.36231 Release / Ninja, CMake + ctest, JUCE 9.0.5 from
  `D:/Core-deps/JUCE-9.0.5`, build `D:/Core-build/flawless-deadcode2`, `-Jobs 6`. Gate environment per AGENTS.md
  (`VSLANG=1033`, `chcp 65001`, vcvars64).
- Tests executed: full `ci/gate.ps1` (configure + build + `ctest -L merge`), then `ctest -L merge -V`,
  `ctest -L desktop -V` (the app's self-test, null audio device), the full ctest set; targeted `domain_tests` and
  `hunt_tests` runs before committing; the owner's before-probe compiled and run on Windows; two null-render
  fixtures (audio and test-synth plugin) rendered before and after and compared by sha256 and byte-for-byte.
- Limitations: self-test runs on the null audio device (real-device audio out of scope); no sanitizer round in
  this task; native visual acceptance beyond the self-test's new assertions belongs to the Test Engineer.
  Baseline binaries for the probe and the "before" renders came from the idle, identity-verified
  `D:/Core-build/gate-ed4642e` build (`core-engine --version` = `0.7.0+ed4642e61b`; fidelity established by
  `docs/relatorios/2026-11-18-estado-ctests.md`), used read-only. Baseline suite counts are that report's
  measurement at ed4642e (17/17, 145 cases / 1741 verifications; engine_process 4/49; desktop ok, 49 steps).

## CLEAN-07 — the offline pattern waveform preview (retired native instruments only)

### Consumer map (grep at ed4642e across app/, engine/, tests/, ci/, CMake)

| Symbol | Consumers found | Disposition |
| --- | --- | --- |
| `core_previews` target | `app/previews/CMakeLists.txt` (definition); root `CMakeLists.txt:75` (add_subdirectory), `:95` (sanitizer list); `app/desktop/CMakeLists.txt:26` (CoreApp link); `tests/CMakeLists.txt:63` (hunt_tests link) | all removed |
| `pattern_sound.cpp/.hpp` | `Waveforms.h:17` include; `hunt_tests.cpp:24` include; self | deleted (`app/previews/` gone) |
| `core::preview::makeRequest/render/Sound/Request` | `Waveforms.cpp:86,108,115` (PatternWaves only); `hunt_tests.cpp:267` (U-5 case) | removed with the module |
| `PatternWaves` / `patternWaves_` | `Waveforms.h:81-107` (class), `Waveforms.cpp:68-148` (impl incl. its 512 MB `AssetStore`), `MainComponent.h:503` (member), `MainComponent.cpp:458` (the only call) | removed |
| `paintClipWave` | `MainComponent.cpp:340-356` (definition), `:459` (the only call) | removed; the existing `paintClipNotes` path is now unconditional |
| `app/previews/src/*.cpp` glob | `ci/tidy_scope.py:20` | removed from the tidy scope |
| historical mentions | `docs/relatorios/tidy-round1.txt`, `docs/relatorios/evidencia/2026-11-17-compilar-gui/log-configbuild.txt` | history — never edited |

Windows-only wiring found by the pre-commit syntax check (the Linux audit could not see it: the app target is
Windows-only): CoreApp reached `core_engine_lib` (engine headers and symbols, e.g. `core/project.hpp`,
`dsp::stretch`) transitively through `core_previews`' PUBLIC link. With the target gone, CoreApp now links
`core_engine_lib` explicitly (`app/desktop/CMakeLists.txt`). Without this the desktop build fails on
`core/project.hpp` — measured (`cl /Zs` fatal C1083), then fixed.

### Removed vs kept

Removed: `app/previews/` (3 files, the whole `core_previews` target), `PatternWaves` (class + implementation +
member + call site), `paintClipWave`, the four CMake references, the tidy-scope glob, the U-5 test case.
Kept (confirmed in code and measured green): `WaveformCache` (`Waveforms.cpp:7-66`) serving audio clips
(`MainComponent.cpp:309`), the audio editor (`:1275`) and the Library (`:4991`, `library_.waveform`);
`drawWaveColumn`/`drawWaveform` (`LibraryPanel.cpp:993`); audio clip rendering; pattern note playback; MIDI
import/export; the pattern catalogue and kick/bass/cymbal alignment. No engine or DSP code touched
(`git diff --name-only ed4642e..08ddc65 -- engine/` is empty).

### Proof

- Before-probe on Windows (the owner's `before-probe.cpp`, compiled against the verified ed4642e libraries,
  `tools/build-tools.bat` in the working evidence): exit 0, `request created; waveform empty=1; notes=4` —
  identical to the Linux measurement in report (8). A four-note retired Wave pattern creates a render request
  and renders silence; `makeRequest` rejects non-builtin tracks (`pattern_sound.cpp:38`), so plugin and audio
  tracks never used the pipeline. Its removal is sound-neutral for them.
- Null render, bit-exact (before @ ed4642e engine `0.7.0+ed4642e61b`, after @ this branch's engine
  `0.7.0+08ddc6541e`; `core-engine render … --bits 32`, default 2 s tail; both renders 6.0 s, peaks identical):
  - F1 audio fixture (two audio tracks; gain/pan/fades/offsets; master −1.5 dB; deterministic 2 s stereo tone):
    sha256 `49f83b81f07e7087b0076a7eb82c9b05337287405bd530462f26de876130be2c` — BEFORE == AFTER, `cmp` clean.
  - F2 plugin fixture (Core Test Synth — a deterministic sine per note — three notes; the SAME VST3 copy used
    by both renders): sha256 `7f2fe3a13d95ee7e8e34a31d7206928b79585d554f812b4ef6fad90bd0b8ac3c` —
    BEFORE == AFTER, `cmp` clean.
  - Determinism control: a second same-engine render of F1 is byte-identical (the comparison can fail).
  - Fixture tone sha256 `184e5c989d975e446b07b755f363ff6740a34f2d6b9c6e7782e25ea6834bb96e`.
- Characterization added to the app self-test (desktop label, measured green): step 19 asserts the bass pattern
  clip's MIDI notes are actually drawn (kClipNote pixel count inside the clip box, away from the selection edge);
  step 35 asserts the imported audio clip has a real file waveform from `WaveformCache` (non-empty peaks,
  peak ≥ 0.05) — the same cache the Library reads. Step count unchanged (49).
- Pattern note playback and MIDI remain guarded by the existing green suites: `midi_tests` 8/115,
  `engine_process_tests` 4/49, `cli_plugin_crash_render` (the synth's note is heard in the render), and the
  self-test's play/edit/heard steps (loudest 0.735 on the null device).

## BUG-LEGACY-RISE — the inactive Stage Core "Rise on build-ups" action

### Consumer map (grep at ed4642e)

| Symbol | Consumers found | Disposition |
| --- | --- | --- |
| `doc::scoreRises` | `score.hpp:57` (decl), `score.cpp:255` (definition, always `return 0`), `SCorePanel.cpp:208` (the only caller) | removed |
| `SCorePanel::rises()` | `SCorePanel.h:30`, `SCorePanel.cpp:33` (onClick), `:205-211` (impl) | removed |
| `rise_` / `riseDot_` (panel) | `SCorePanel.h:45-46`; `.cpp:32-35` (text + tooltip), `:39` (visibility list), `:74` (setEnabled), `:145` (layout) | removed |
| namesakes — NOT consumers | `BuildupPanel.cpp:126-128,170,230` `rise_` ComboBox (the roll's pitch rise, live); `calls.cpp:169,196,348,695,705` `f.rises` (the AICORE bridge's section-limit flag, live); `lang/pt-PT.json:1401` "Rise" (section name), `:2103` "Risers" (role) | kept |

No test referenced `scoreRises`; the self-test never called `rises()`. `scoreStageOf` still maps a section named
"Rise" to a build-up (characterized in the new test). SCorePanel's live functions stay: Song shape, Arrange, the
grid's cell on/off, the track-name click, hover/paint/layout. BuildupPanel is untouched.

### Proof

- The implementation always returned zero and its message instructed users to add retired FX Core
  (`score.cpp:254-255`, `SCorePanel.cpp:209-210` at ed4642e) — confirmed in pre-change code; the button's only
  effects were an empty undo step and that message.
- New domain characterization (these live functions had no test before): `domain_tests.cpp` —
  "score: Stage Core's live operations (song shape, arrange, cell on/off) on the template song":
  `setScoreShape` layout, `scoreStageOf`, `scorePartOf`/`scorePartName`, `scoreArrange` (15 clips by the rules;
  kick rests in the break; no kick/bass in the bar before the drop; bass enters the intro at bar 4),
  `scoreSetPlays`/`scorePlays` cell toggling, pattern notes intact, and the domain guards. 1 case / 53 checks,
  measured green (`domain_tests` 20 testes / 201 verificacoes / 0 falhas).

## Gate and coverage (Windows, this machine)

- Full gate: `powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/flawless-deadcode2 -Jobs 6`
  (unique-bat version, cherry-picked `fffbb41` → `3345024`). GATE-LOCK honored; one stale-lock recovery documented
  in the evidence (the previous holder's gate had passed at 07:37:47Z and its agent never released the lock;
  zero builder processes belonged to it). Result: **VERDE**, **203 s**, `Codigo: …flawless-deadcode2 @ 08ddc65`
  (clean tree, no `.modified`); `core-engine --version` = `0.7.0+08ddc6541e` == HEAD.
- `ctest -L merge`: **17/17 pass**, exit 0. Per-suite counts (`-V`), vs the ed4642e baseline measured in
  `docs/relatorios/2026-11-18-estado-ctests.md`:

| Suite | ed4642e cases/verifs | this branch cases/verifs | delta |
| --- | --- | --- | --- |
| engine_tests | 25/130 | 25/130 | 0 |
| domain_tests | 19/148 | 20/201 | +1/+53 (new Stage Core characterization) |
| role_tests | 21/216 | 21/216 | 0 |
| midi_tests | 8/115 | 8/115 | 0 |
| bug_tests | 22/138 | 22/138 | 0 |
| audit_tests | 7/65 | 7/65 | 0 |
| audit2_tests | 6/80 | 6/80 | 0 |
| hunt_tests | 14/649 | 13/647 | −1/−2 (U-5, removed feature — below) |
| round3_tests | 7/82 | 7/82 | 0 |
| calls_tests | 10/44 | 10/44 | 0 |
| bitwig_tests | 2/25 | 2/25 | 0 |
| engine_process_tests | 4/49 | 4/49 | 0 |
| **total** | **145/1741** | **145/1792** | **0/+51** |
| cli_* (5) | pass, no counts | pass, no counts | 0 |

- Removed-feature coverage (documented, legitimate): hunt case "caca 2, U-5" tested `Sound.seconds()` on the
  deleted `core::preview::Sound` datatype (1 case / 2 checks). It goes out with the datatype; no live-behavior
  test was removed or weakened. No replacement empty implementation was introduced.
- Desktop: `ctest -L desktop -V` — **1/1 pass**, self-test **ok, 49 steps** (including the new note-drawing and
  file-waveform assertions), `build=0.7.0+08ddc6541e engine=0.7.0+08ddc6541e`, document=61 accepted=61 applied=61,
  null device. Full set: **18/18 pass**, exit 0.

## Changes

- `5a5eced` CLEAN-07: deleted `app/previews/CMakeLists.txt`, `app/previews/src/pattern_sound.cpp`,
  `app/previews/src/pattern_sound.hpp`; edited root `CMakeLists.txt`, `app/desktop/CMakeLists.txt` (also the
  explicit `core_engine_lib` link), `app/desktop/src/Waveforms.h`, `Waveforms.cpp`, `MainComponent.h`,
  `MainComponent.cpp` (paintClipWave removal + self-test steps 19/35 characterization), `tests/CMakeLists.txt`,
  `tests/hunt_tests.cpp` (U-5), `ci/tidy_scope.py`. 12 files, +45/−361.
- `3345024` cherry-pick of `fffbb41` (ci/gate.ps1 unique temp bat).
- `08ddc65` BUG-LEGACY-RISE: `app/desktop/src/SCorePanel.h`, `SCorePanel.cpp`, `app/domain/src/score.hpp`,
  `score.cpp`, `tests/domain_tests.cpp` (new characterization). 5 files, +87/−29.
- Docs commit (this report): one index row in `docs/relatorios/README.md`, and the verbatim archive in
  `docs/relatorios/evidencia/2026-11-18-preview-rise/` — report (8).md, before-probe.cpp, provenance (1).json,
  source-hashes.json, merge.log, build (1).log (sha256 match the provenance file) — plus this delivery's
  measurement logs (gate resumo/log, verbose merge/desktop/full logs, self-test report, probe output, sha256
  lists, fixture generator and fixture JSONs, stale-lock recovery note).
- `ESTADO.md` NOT edited (D72/D80). Main checkout untouched; nothing pushed; no `git stash` used. L1/L2 dead
  code (edit.cpp/samples.cpp) delivered separately — confirmed still present, not re-removed.

## PROPOSED ESTADO.md lines (scribe only — D72/D80; not written here)

1. Estado medido: `merge @ 08ddc65 (Windows, MSVC Release): 17/17 ctest, 145 cases / 1792 verifications /
   0 failures (hunt 13/647 — U-5 removed with its deleted Sound datatype; domain 20/201 — new Stage Core
   characterization; engine_process 4/49 unchanged). desktop 1/1, self-test ok, 49 steps incl. new note-drawing
   and file-waveform assertions, null device. Full 18/18. Gate VERDE 203 s, version==08ddc65 (0.7.0+08ddc6541e),
   no .modified. Source: docs/relatorios/2026-11-18-dead-code-preview-rise.md + evidencia/2026-11-18-preview-rise/.`
2. Estado medido (null): `CLEAN-07 removal sound-neutral: before-probe @ ed4642e on Windows exit 0
   "request created; waveform empty=1; notes=4"; audio and test-synth fixture renders bit-exact before/after
   (sha256 49f83b81… / 7f2fe3a1…, cmp clean); engine/DSP untouched. Source: same report; fixtures and hashes in
   evidencia/2026-11-18-preview-rise/ and D:/Core-build/flawless-deadcode2-evidence/fixtures/.`
3. `BUG-LEGACY-RISE closed: the inactive scoreRises action (always returned 0; told users to add retired FX
   Core) removed from SCorePanel and the domain; SCorePanel's live functions kept and now characterized.
   Source: owner's CodeRabbit report (8) dispatch 2026-11-18; commit 08ddc65.`
4. `CLEAN-07 closed: core_previews (retired native-instrument pattern preview) removed; the arrangement uses its
   existing MIDI note drawing; file waveforms, Library, playback, MIDI, catalogue and alignment preserved.
   Source: report (8); commit 5a5eced.`
5. Pendência (owner = Diogo / Test Engineer): Windows native visual acceptance — MIDI clips show notes, audio
   clips/Library show file waveforms, Stage Core controls work. Integration: rebase expected with
   flawless-crash (MainComponent Restart ~:6427) and flawless-menu (:4606-4761); this branch touches the
   paintClipWave region (~:337-461) and self-test steps 19/35 (~:6901/:7261 post-change) — different regions,
   preserve theirs. L1/L2 dead code (edit.cpp/samples.cpp) delivered separately; not re-removed here.

## Next steps

- Test Engineer: independent acceptance on Windows (the self-test assertions are a floor, not the acceptance);
  real-device audio if in scope for the milestone.
- Integration: sequence the MainComponent.cpp rebase with the crash/menu branches; re-gate the integrated tip
  (version==sha, no .modified). Remote steps are the owner's.
- Ledger scribe: transcribe the proposed lines; no product decision is blocked by this delivery.
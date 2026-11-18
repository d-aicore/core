# Report — 2026-11-18 — Piano roll UX slice: Pulse concepts, Core's look

## What was measured

- Repository / branch / SHAs: worktree `D:/CORE/CORE/.agents/worktrees/flawless-ux-piano`, branch
  `flawless-ux-piano`, base `f6b86dc` (integration tip). Commits, in order:
  `9f008de` (red-first self-test steps), `4a3c042` (expandable key range + scale dropdowns),
  `6c5144a` (Pulse gestures), `2cf253a` (wheel velocity), `0191dce` (this session: rendering fixes +
  Note tools button/window + surfaces self-test step). Code tip gated: **`0191dce`**
  (`0191dcee414bf99c4f9d9c0393490a3c62ceb335`). The report commit (final HEAD) is re-gated in both
  modes with version == its own sha (evidence `-finaltip`).
- Scope and authorization: the owner's piano-roll UX directive, verbatim (2026-11-18, DSH coordinator
  session `session-2ff54f03-8344-40bb-8cb4-54df0601eb69`): «metam as janelas assim [Pulse], implementem
  os conceitos que tem, deixem o ui como esta no core. usem dropdowns em vez de muitos botoes»,
  «apenas muda no piano», «piano deve conseguir expandir», «no piano ctrl+scroll keys+ e keys», and the
  Note-tools window button: «quero uma pequena janela com isto , ou sej um botao no piano que me da
  estas opçoes, incluindo as do baixo kbbb que ja existem». Reference `D:/CORE/plan/pulse-src`
  (AGPL): **behaviours/concepts only, no code transcribed**. Core has no built-in DSP/sampler-synth;
  none was added.
- Environment: Windows 10.0.26300, MSVC VS 18 BuildTools 14.51.36231 (English resources not installed:
  `VSLANG=1033` ignored, compiler notes in Portuguese), CMake 4.4.4 + Ninja, JUCE 9.0.5 pinned at
  `D:/Core-deps/JUCE-9.0.5`, Release. Build `D:/Core-build/flawless-ux-piano`; evidence
  `D:/Core-build/flawless-ux-piano-evidence/`. `GATE-LOCK` acquired by this stream 14:11:31Z after
  `flawless-modules` released it; the final re-gate waits for the lock again (another stream took it
  at 14:28:17Z while this report was being written).
- Tests executed: full `ci/gate.ps1` merge and `-Desktop` at the code tip (both VERDE, below); the
  desktop self-test red at `9f008de` and green after; per-suite counts (`ctest -L merge -V`);
  `engine_process_tests` re-run alone after one load-induced flake.
- Limitations: **visual acceptance is not done here** — the checklist below is for the owner / Test
  Engineer with the app open. The full Note-tools window (preview/Apply-Cancel, Euclidean,
  Arpeggiator, position window, ends/filters) is the next slice (E); this slice delivers the button
  and a minimal window that applies immediately, one undo step per action, like Edit ▾ does today.

## Results

| ID | Problem / result | Evidence (SHA, file:line or run) | Classification | State |
| --- | --- | --- | --- | --- |
| UX-PIANO gestures | Drag-to-size create, last used length, Shift+drag clone, Alt free move (snap shows Off), Paint seeds per cell, Ctrl+drag lasso in any tool, per-tool cursors — Pulse concepts, Core look | `6c5144a`; self-test step 39 green (`self-test-final2.txt`, "piano gestures" transition); red at `9f008de` ("failed: create drag length", `self-test-red.txt`) | proven by test | validated |
| UX-PIANO expand | Ctrl+wheel = Keys± (owner verbatim above), Keys−/Keys+/Fit keys buttons, Fit keys = 128; range grows up from the lowest key shown so expanding never hides the view | `4a3c042` + `0191dce` (`PatternEditor.cpp` `keysMore/keysLess/keysWheel`); self-test step 39 keys+ check and step 40 Fit/Keys± checks green | proven by test | validated |
| UX-PIANO velocity | Alt+wheel ±5 % on the note under the mouse / the selection / the velocity lane; Ctrl+Alt fine ±1 %; plain wheel scrolls, never edits loudness | `2cf253a` + `0191dce` (Ctrl+Alt dispatched before the Ctrl zoom branch that used to swallow it); self-test step 39 wheel checks green | proven by test | validated |
| UX-PIANO guide | Scale guide root mark was overwritten by the next row's fill when 128 keys make rows sub-pixel; the guide is now a pass AFTER the rows and the keyboard | `0191dce` (`PatternEditor.cpp` paint); self-test step 40 counts exact-accent pixels before/after the guide (≥ +12) green | proven by test | validated |
| UX-PIANO note tools | Toolbar button opens the small window: Quantize (grid dropdown, position strength, grid swing), Humanize, Groove variation dropdown + Apply, the kick/bass FOLLOW tools, −12/+12 transpose; each action one Ctrl+Z; close hides, settings stay | `0191dce` (`NoteToolsPanel`/`NoteToolsWindow`, `quantize(gridTicks)` overload); self-test step 40 opens the window, clicks Quantize, checks tick on grid + undo label "Quantize", closes — green | proven by test | validated |
| UX-PIANO dropdowns | Selectors are dropdowns, not button rows: Scale, Root, Quantize grid, Groove variation | `4a3c042` (`scaleBox_`, `rootBox_`), `0191dce` (`grid_`, `groove_`) | confirmed in code | validated |
| UX-PIANO look | Core UI look unchanged: Theme colours/fonts/menus untouched; the only Theme change is an additive "paint" icon for the Paint tool | `git diff f6b86dc..0191dce -- Theme.*` (one new `drawIcon` branch + its comment); no colour constant changed | confirmed in code | validated |
| CI-DEPS incident | Dev compiles launched from the Git Bash console produced objects with **zero recorded header deps**: `vcvars64.bat` restores the console code page to 850 there, cl's `/showIncludes` notes came out in CP850 bytes while `rules.ninja` stores the UTF-8 prefix → no match. The version-header change at the commit then did not rebuild `MainComponent.cpp.obj`, and Core.exe kept `build=2cf253a.modified` while the engine said `0191dce` | measured: `cp-probe.log` ("Active code page: 850" after vcvars; note bytes `207 306` = CP850 çã), `ninja -t deps` `#deps 0` on 26 outputs; the gate's own PowerShell chain emits UTF-8 notes (0 "incluindo" lines in its logs = all consumed) and records deps | proven by measurement | corrected (26 outputs deleted, re-gated; `MainComponent.cpp.obj` now lists `core_version.h`) |
| FLAKE-1 | `engine_process_tests` "crashes opening its editor" timed out once (`plugin.editor` > 5000 ms) in the `-V` coverage run while another stream's gate ran concurrently; passes alone and in all three gate runs | `coverage-merge-final.txt` (1 falha @ line 418) vs `engine-process-rerun.txt` (rc 0, 28.4 s) and gates final/final2 merge+desktop 20/20, 21/21 | proven by test (load-induced) | reported to the crash stream; not this slice's code |

## Changes

Implementation (all in this stream's regions; `MainComponent.cpp` only inside `selfTestStep`,
single hunk `@@ -7398`):

- `app/desktop/src/PatternEditor.cpp` / `.h` — piano-roll editor: gestures and expand (inherited
  commits), wheel dispatch order, Keys± semantics, the scale-guide pass after the rows, the Note
  tools button + minimal window, `quantize(int64_t gridTicks = 0)` overload (0 = the editor's snap
  grid; the window passes its own; the "nothing moves" message names the actual grid).
- `app/desktop/src/MainComponent.cpp` — self-test only: step 39 (gestures, inherited), step 40
  (surfaces: Paint per cell + one undo, Fit keys 128 / Keys±, the guide's root-mark pixel count,
  the Note tools window open/Quantize/close), step 41 finish; the wheel details fixed to
  `{deltaX=0, deltaY=1}`; Quantize clicked via `q->onClick()` — synchronous, the same pattern as
  `arrangeButton_.onClick()` (MainComponent.cpp:1961) — because `triggerClick()` posts a command
  message (`juce_Button.cpp:359`) the step would not wait for (measured red: tick stayed 100,
  label stayed "Offset").
- `app/desktop/src/Theme.cpp` / `.h` — additive "paint" icon only (inherited, `6c5144a`).

Documentation: this report + the index row. No `ESTADO.md` edit (D72/D80): lines proposed below.

## Verification

Commands and exit codes (all on Windows, this worktree; logs in
`D:/Core-build/flawless-ux-piano-evidence/`):

1. **Red** (prior session, at the red-test commit): gate `-Desktop` VERMELHO @ `9f008de`,
   `desktop_self_test` failed "create drag length" — `gate-resumo-red.txt`, `self-test-red.txt`
   (52 logged transitions; the gestures did not exist yet).
2. Compile check: `cmake --build D:/Core-build/flawless-ux-piano --parallel 4` via a vcvars bat —
   rc 0 (`compile-check-final.log`). Note: this dev-bat path is what caused CI-DEPS above; the
   authoritative builds are the gate's.
3. **Green dev run 1**: `ctest -R desktop_self_test` rc 8 — "note tools quantize n=1 tick=100
   label=Offset" (the async `triggerClick` harness bug). Fixed (`q->onClick()`), rebuilt, re-ran:
   rc 0, ok, 54 transitions (`self-test-green-dev.log`, `self-test-green.txt`).
4. Commit `0191dce` (explicit paths: MainComponent.cpp, PatternEditor.cpp/.h). Tree clean.
5. **Gate merge** @ `0191dce`: VERDE 20/20, 39 s (`gate-resumo-merge-final.txt`) — but the self-test
   build ID was stale (CI-DEPS). After deleting the 26 zero-deps outputs: **gate merge VERDE 20/20,
   42 s** (`gate-resumo-merge-final2.txt`), `MainComponent.cpp.obj` deps now include `core_version.h`.
6. **Gate desktop** @ `0191dce`: VERDE 21/21, 47 s (`gate-resumo-desktop-final2.txt`); self-test
   `ok: … piano roll Pulse gestures | build=0.7.0+0191dcee41 engine=0.7.0+0191dcee41` —
   **version == HEAD sha, no `.modified`**; `document=68 accepted=68 applied=68`, `loudest=0.735`,
   null device 48000 Hz block 128; **54 logged transitions** = baseline 51 + "diagnostics" (split
   introduced by `9f008de`) + "piano gestures" + "piano surfaces" (`self-test-final2.txt/.png`).
7. Coverage `ctest -L merge -V` (`coverage-merge-final.txt`): **202 cases / 3059 verifications**,
   identical to `f6b86dc` (consolidation table), 0 failures in 13 suites; `engine_process_tests`
   10/121 with one load-induced flake (FLAKE-1) — re-run alone rc 0. No unit test added or removed
   by this slice; the new coverage is the self-test steps. The remaining `#deps 0` outputs after the
   re-gate are JUCE unity/LV2/BinaryData/`.res` files (pinned JUCE headers only, no `core_version.h`;
   pre-existing, they compile with `/MP` or no `/showIncludes` path) — noted, not this slice's risk.
8. Final re-gate at the report commit (final HEAD): merge + desktop VERDE, version == its own sha
   (evidence `gate-*-finaltip*`).

Characterization (existing editing still works): the self-test's editing steps pass unchanged
(notes edited/heard, arrangement, MIDI, audio clips, save/reopen, export, crash/restart, plugin
state); the gestures step exercises create/move/clone/undo with snap and Alt-free; the ghost
(the draw tool's dashed preview) renders in the overlay pass after the guide pass — order unchanged
(`PatternEditor.cpp` paint, confirmed in code); the merge suite (domain quantize/humanize, menu,
midi, bug regressions) is green at baseline counts.

## Visual acceptance checklist (not verified — for the owner / Test Engineer)

Open Core, a pitched track's pattern in the piano roll:

1. Toolbar row 2 shows Keys− / Keys+ / Fit keys, the **Scale** and **Root** dropdowns (Root appears
   when a scale is chosen), and the **Note tools** button; the drums row is unchanged.
2. Ctrl+wheel over the grid grows/shrinks the key range by 4 (Keys± buttons do the same); the lowest
   key shown stays put; Fit keys shows all 128 and the top key is reachable.
3. With a scale chosen, scale keys carry a faint tint and every root key a solid 3 px accent mark on
   the keyboard — **still visible with Fit keys (128) in a short grid** (the sub-pixel fix).
4. Draw: click-drag sizes the new note; a plain click reuses the last length; Shift+drag clones;
   Alt moves off the grid (snap reads Off); Ctrl+drag lassos in any tool; Paint seeds one note per
   cell dragged (one Ctrl+Z for the whole stroke); cursors match the tool.
5. Alt+wheel over a note / the velocity lane: ±5 % velocity (the selection's when selected);
   Ctrl+Alt: ±1 %; plain wheel scrolls and never changes loudness. The velocity lane under the grid
   shows one bar per note, height = velocity.
6. Note tools opens the small window: QUANTIZE (Grid dropdown, Position strength, Grid swing,
   Quantize + Humanize), GROOVE (Variation dropdown + Apply groove), BASS / KICK (CORE) (the three
   FOLLOW buttons, −12/+12). Quantize pulls an off-grid note onto the chosen grid in one Ctrl+Z;
   close hides the window and the settings stay for the next open.
7. Core's look is unchanged everywhere else: same colours, fonts, menus; no new button rows where a
   dropdown fits; no instrument/sampler/DSP surface anywhere.

## Confirmations

- Main checkout `D:/CORE/CORE` untouched by this session (HEAD `ed4642e`, branch `main`; its dirty
  files are other sessions' work, present before this session).
- Nothing pushed, no merge, no `git stash` (the one stash entry is the pre-existing flawless-crash
  recovery, documented in the dead-code report).
- No AGPL code transcribed: Pulse concepts only (drag-to-size, last length, Shift clone, Alt free,
  paint, lasso, wheel velocity, expandable keys, a note-tools window), written fresh in Core's idiom.
- Core UI look preserved (Theme additive icon only); dropdowns for selectors; no DSP added.
- Other streams' regions preserved: `MainComponent.cpp` changes are confined to `selfTestStep`;
  the ux-windows framework, modules cards and l9 engine regions are untouched — rebase at integration.

## Pending and handoff

- **Slice E**: the full Note-tools window (preview/Apply-Cancel, Euclidean, Arpeggiator, position
  window, ends/filters) — the button and the minimal applying window are this slice.
- **Visual acceptance** by the owner / Test Engineer (checklist above); the self-test proves the
  wiring and the pixel count, not the look.
- **CI proposal (not implemented)**: the gate's deps check (`ninja -t deps | findstr "#deps [1-9]"`)
  is global — it passes while individual objects have zero deps. A dev compile from a foreign console
  (Git Bash → vcvars restores CP850) silently produces such objects (CI-DEPS, measured). Proposal:
  check `#deps 0` per object for the targets that include `core_version.h` (or refuse greens when any
  CXX object of a Core target has zero deps), and/or document that dev compiles must go through the
  gate's PowerShell chain. Owner/coordinator decides; `ci/gate.ps1` not touched here.
- **FLAKE-1** to the crash stream: `plugin.editor` 5000 ms answer window is load-sensitive (failed
  once under a concurrent gate, passes alone and in three gate runs).
- Integration: rebase with `flawless-ux-windows` / `flawless-modules` (MainComponent/PatternEditor
  framework/card regions); this slice's hunks are disjoint today.

## PROPOSED ESTADO lines (not written — D72/D80, only the scribe writes ESTADO.md)

- **Decisions (proposal, ratification by the owner's verbatim quotes above, 2026-11-18, DSH
  coordinator session `session-2ff54f03-8344-40bb-8cb4-54df0601eb69`)**: the piano roll takes Pulse's
  concepts with Core's look unchanged — drag-to-size create with the last length, Shift clone, Alt
  free + snap Off, Paint, Ctrl lasso, Alt+wheel velocity (Ctrl+Alt fine), Ctrl+wheel Keys± / Fit keys
  128, scale guide dropdowns, and a Note tools window opened from a piano-roll button (the full
  preview/Apply-Cancel window is the next slice). Dropdowns, not button rows. No DSP.
- **Measured state (proposal)**: branch `flawless-ux-piano` @ the report commit — gate merge VERDE
  20/20 + desktop VERDE 21/21 (Windows MSVC Release, under GATE-LOCK), **202 cases / 3059
  verifications / 0 failures** (= `f6b86dc`, nothing shrunk), self-test ok **54 logged transitions**
  (51 + diagnostics + piano gestures + piano surfaces), `document=68 loudest=0.735`, version == HEAD
  sha no `.modified`. Red measured at `9f008de` ("create drag length"), green after the fixes.
- **Pending (proposal)**: slice E (full Note-tools window); visual acceptance; the CI per-object
  deps check proposal (CI-DEPS incident measured and corrected here); FLAKE-1 to the crash stream;
  integration rebase with ux-windows/modules.
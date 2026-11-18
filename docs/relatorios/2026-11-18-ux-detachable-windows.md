# Report — 2026-11-18 — Detachable windows (UX slice): Pulse's window concepts with Core's look

## What was measured

- Repository `d-aicore/core`, worktree `D:/CORE/CORE/.agents/worktrees/flawless-ux-windows`, branch
  `flawless-ux-windows`, base `f6b86dc`. Commits of this delivery:
  - `51dfc95` — layout model tests, red on the base behavior (no monitor validation).
  - `19f10ab` — monitor validation + bring-back cascade in `winlayout`, green (10 tests, 70 checks).
  - `38e695d` — framework + integration (PanelWindows, MainComponent, panels, dropdowns, keys).
  - (docs commit) — this report + index row.
- Authorization: owner directive, verbatim (2026-11-18, coordinator dispatch): «metam as janelas assim
  [github.com/nalexand/pulse], implementem os conceitos que tem, deixem o ui como esta no core. usem
  dropdowns em vez de muitos botoes para escolhar». Reference source `D:/CORE/plan/pulse-src` used for
  behaviors/concepts only; **no AGPL code transcribed** (Pulse is AGPL; Core's code is written from the
  concept descriptions, not from its source).
- Environment: Windows 10.0.26300, MSVC BuildTools 18 (English requested via `VSLANG=1033`, UTF-8 code
  page), JUCE 9.0.5 (`D:/Core-deps/JUCE-9.0.5`), Ninja, build `D:/Core-build/flawless-ux-windows`,
  evidence `D:/Core-build/flawless-ux-windows-evidence/`.
- Tests executed: `window_tests` (new, merge label), `desktop_self_test` (CoreApp `--self-test`, desktop
  label, null audio backend), full gate merge + `-Desktop` via `ci/gate.ps1`. Numbers in Verification.
- Limitations: the self-test runs with the null audio device and simulated input; it proves window
  state transitions, layout persistence and rendering, not a human's visual experience on real monitors.
  The visual-acceptance checklist below is for the Test Engineer on a real desktop (two monitors if
  available). No real-plugin load was part of this slice.

## The framework (concepts taken from Pulse, implementation written for Core)

Pulse organizes its workspace as detachable sub-windows. The concepts implemented here, with Core's own
code and Core's unchanged look:

1. **Five detachable panels**: Playlist (ruler + tracks), Channel rack, Piano roll, Mixer, Browser.
   Each has a detach control (⧉) and a close control (×) at its top-right; docked panels show them in
   their header/ruler, detached windows use the native OS title bar plus the same controls.
2. **Pop out to an OS window on any monitor**: `PanelWindow` (`app/desktop/src/PanelWindows.h/.cpp`,
   new) is a `juce::DocumentWindow` shell with native title bar, resizable (min 420×240), non-owned
   content. The panel component itself moves between the main window and the shell — **nothing is
   rebuilt on detach/dock**, so edit state (selection, scroll, undo context) survives the move. The
   channel rack reuses its existing `PatternsWindow`, which now derives `PanelWindow`.
3. **Dock back / hide / show**: ⧉ toggles detach↔dock; × hides the panel (its last place is kept);
   View menu entries and F5–F9 show/toggle each panel again where it lives. Ctrl+M still toggles the
   Mixer. Keys are forwarded from detached windows to the app's key handling unless a text editor has
   focus (`GlobalKeys::focusTakesKeys` guard).
4. **Layout persistence, settings-compatible**: each panel's state (Docked/Detached/Hidden) and last
   detached rectangle are stored as JSON under `window.<id>` in the existing user settings
   (`juce::PropertySet`), written at most once a second after a move/resize. Legacy settings load:
   with no `window.*` keys, defaults apply and the Browser honors the old `showLibrary` key.
5. **Monitor validation + bring-back cascade**: `winlayout` (`WindowLayout.h/.cpp`, commits 51dfc95 /
   19f10ab) validates every restored rectangle against the current monitors' work areas; an off-screen
   or unreachable window is cascaded back onto the primary monitor. View > "Bring windows back to
   screen" (Ctrl+Shift+H) rescues windows at any time, e.g. after a monitor is unplugged.
6. **Dropdowns instead of button rows** (owner directive): the piano roll's Draw/Select/Erase tool
   buttons became one tool dropdown; the channel rack's All/Built-in/Mine filter buttons became one
   filter dropdown. Menus and colors are Core's existing ones.

**Core UI look unchanged**: `Theme.cpp/.h` diff is purely additive — three new line icons
(`detach`, `dock`, `close`) in `drawIcon`; no color, font, metric or menu-style change. All panels keep
their existing paint code. No DSP, instruments or designers added; plugin menus still populate from
plugins only.

## Results

| ID | Problem / result | Evidence (SHA, file:line or run) | Classification | State |
| --- | --- | --- | --- | --- |
| UX-W1 | Layout model had no monitor validation; off-screen places restored as-is | red: `red-window-tests.txt` (17 failed checks @ base behavior); green: `window-tests-green2.txt` (10 tests, 70 checks, 0 failures) @ `19f10ab` | proven by test | validated |
| UX-W2 | Compile breaks in inherited uncommitted integration: duplicate menu ID 1820, private callbacks, `juce::PropertiesSet` (does not exist in JUCE 9.0.5 → `juce::PropertySet`), `PropertiesFile` default ctor | `framework-build3.log` (red) → `framework-build8.log` (green, CoreApp EXIT=0) @ `38e695d` | proven by test (build) | fixed |
| UX-W3 | Self-test step 19 red after the ArrangementView refactor: note-pixel probe used main-window coordinates for view-local lane geometry | red: `self-test2.txt` ("0 note pixels", reproduced twice); fix: probe origin via `arrangementView_->getLocalPoint(row, …)`; green: `self-test6.txt` | proven by test | fixed |
| UX-W4 | First rack detach no-opped: JUCE `setContent` ignores the same content pointer; the window still named the panel from construction | red: `self-test4.txt` ("rack detach: content still in the main window"); fix: `clearContentComponent()` before `setContentNonOwned` in `detachPanel`; green: `self-test6.txt` | proven by test | fixed |
| UX-W5 | Legacy restore read the live `showLibrary_` member instead of the loaded settings value | red: `self-test5.txt` ("legacy: showLibrary kept"); fix: `props.getBoolValue("showLibrary", showLibrary_)`; green: `self-test6.txt` | proven by test | fixed |
| UX-W6 | Detach/dock/close, layout round-trip, off-screen bring-back, legacy load, detached-browser picture | self-test steps 39–45, `self-test6.txt` + `self-test6.png`, exit 0, 58 logged transitions (base `f6b86dc`: 51) | proven by test | validated |
| UX-W7 | Visual experience on real monitors (drag to second monitor, native title bar, resize limits, focus/keys) | checklist below; not exercised by the null-backend self-test | not verified (needs human) | open — Test Engineer |
| UX-W8 | Build-integrity catch: 55 CoreApp objects had **zero Ninja header deps** (compiled by an auxiliary bat that skipped the configure step which refreshes the `/showIncludes` prefix), so the regenerated `core_version.h` did not rebuild them and `Core.exe` reported the stale build id `0.7.0+19f10ab0ef.modified` while `core-engine` reported `38e695da43`. The gate's global deps check passes if *any* target has deps, so it did not catch this. Fix: deleted the 55 zero-dep objects, rebuilt through the gate (which reconfigures); auxiliary bat now reconfigures first | `ninja -t deps` before: `MainComponent.cpp.obj: #deps 0`; after: `MainComponent.cpp.obj: #deps 544`; the only zero-dep object left is `juce_core_CompilationTime.cpp` (no `#include`, `__DATE__`/`__TIME__` only — legitimate); self-test build id after: `build=0.7.0+38e695da43 engine=0.7.0+38e695da43`, no `.modified` (`self-test-final.txt`, exit 0, 58 transitions, `self-test-final.png`) | proven by test (build) | fixed |

## Changes

Implementation (commit `38e695d`, 16 files, +946/−156):

- `app/desktop/src/PanelWindows.h/.cpp` (new): the `PanelWindow` shell — native title bar, non-owned
  content, resize limits, close callback, bounds-change callback, key forwarding.
- `app/desktop/src/MainComponent.h/.cpp`: panel registry (`detached_`), detach/dock/hide/show/toggle,
  `ArrangementView` (the ruler + tracks as a movable view; lane geometry now view-local),
  `saveWindowLayout`/`restoreWindowLayout`/`collectWindowLayout`/`bringWindowsBackToScreen`,
  View menu entries (IDs 1830–1833; 1820 stays `kViewSectionShades`), self-test steps 39–45.
- `app/desktop/src/PatternsWindow.h/.cpp`: `PatternsWindow : PanelWindow`; rack docked in the bottom
  slot or in its window; filter dropdown; window/close buttons.
- `app/desktop/src/PatternEditor.h/.cpp`: tool dropdown; detach/close buttons with icon swap;
  gesture code untouched (flawless-ux-piano region preserved).
- `app/desktop/src/Mixer.h/.cpp`, `LibraryPanel.h/.cpp`: detach button + callbacks, icon swap.
- `app/desktop/src/Theme.h/.cpp`: additive only — `detach`/`dock`/`close` line icons.
- `app/desktop/src/Keymap.cpp`: F5 playlist, F6 channel rack, F7 piano roll, F8 browser, F9 mixer
  (Ctrl+M kept), Ctrl+Shift+H bring windows back.
- `app/desktop/CMakeLists.txt`: `WindowLayout.cpp` + `PanelWindows.cpp` in CoreApp.

Earlier commits: `51dfc95` (tests), `19f10ab` (`WindowLayout` validation/cascade + tests green).
Documentation: this report + index row (docs commit). `ESTADO.md` not edited (D72/D80); lines proposed
below.

Parallel-stream preservation: the diff does not touch the flawless-modules region
(MainComponent base lines ~2379–2512, section/pcore tool menus; nearest hunks end at base 2238 and
start at base 2556), nor PatternsWindow base line 31 (hunks start at 17/40/109/132), nor the
flawless-ux-piano gesture code, nor flawless-clean templates/autoload logic (the filter dropdown
changes only how the existing filter is chosen). Rebase at integration may still produce textual
conflicts in these files; no semantic overlap found.

## Verification

Commands (from the worktree; gate under `D:/Core-build/GATE-LOCK` held by this stream during the runs):

- Build: `cmake --build D:/Core-build/flawless-ux-windows --parallel 4 --target CoreApp` via
  `build-target2.bat` (VSLANG=1033 + chcp 65001 + vcvars64) — EXIT=0 (`framework-build8.log`).
- `window_tests`: red @ base behavior `10 testes, 65 verificacoes, 17 falhas`
  (`red-window-tests.txt`); green @ `19f10ab` `10 testes, 70 verificacoes, 0 falhas`
  (`window-tests-green2.txt`). (Suite summary wording comes from the shared test main; not translated
  here to avoid an unrelated sweep.)
- Self-test red-first per fix: `self-test2.txt` (note pixels), `self-test4.txt` (rack detach),
  `self-test5.txt` (legacy) — each red measured before its fix, green after (`self-test6.txt`,
  exit 0, 58 transitions; picture `self-test6.png` written with the Browser detached).
- Full gate merge: `powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/flawless-ux-windows -Jobs 6`
  — VERDE, 21/21 ctest cases, 77 s, `gate-merge3.log` (final clean run; earlier `gate-merge.log`/`gate-merge2.log` kept as history)
- Full gate desktop: same command with `-Desktop` — VERDE, 22/22 ctest cases (merge + desktop), 44 s, `desktop_self_test` Passed 22.03 s, `gate-desktop2.log` (final clean run; `gate-desktop.log` kept as history)
- Baseline coverage @ `f6b86dc`: 202 tests / 3059 checks (merge). This slice adds `window_tests`
  (10 tests / 70 checks); no existing test removed or relaxed. Measured merge total @ `38e695d`:
  **212 tests / 3129 checks / 0 failures** (sum of the suite summaries in ctest's `LastTest.log`).
- Version == HEAD: `core-engine --version` → `core-engine 0.7.0+38e695da43`; self-test report →
  `build=0.7.0+38e695da43 engine=0.7.0+38e695da43`, no `.modified` (`self-test-final.txt`, exit 0,
  58 transitions, picture `self-test-final.png`). Gate resumo: `@ 38e695d`, no dirty marker.
  The first gate runs exposed UX-W8 (stale build id from zero-dep objects); after the rebuild the
  final runs above are the measured state.

## Visual-acceptance checklist (Test Engineer, real desktop)

1. Start Core with fresh settings: all five panels docked, look identical to before (colors, menus,
   fonts); piano roll tools and rack filter are dropdowns, not button rows.
2. ⧉ on each panel (Playlist ruler, rack, piano roll, Mixer, Browser): pops out to an OS window with
   native title bar; content identical, edit state kept (make an edit, detach, verify it is still
   there; dock back, verify again).
3. Drag a detached window to a second monitor; move/resize it; dock it back with ⧉ — it returns to
   its slot, shown.
4. × hides a panel; View menu entry (or F5–F9) brings it back where it lived; ticks in View reflect
   shown state.
5. Detach windows, move them partly/fully off screen (or unplug the second monitor), restart Core:
   windows come back cascaded on the primary monitor; View > "Bring windows back to screen"
   (Ctrl+Shift+H) rescues them without restart.
6. Quit and reopen: every panel returns to its saved state/place; an old settings file (no `window.*`
   keys, `showLibrary=false`) loads with the Browser hidden and everything else default.
7. Keys work while a detached window has focus (space play/stop, F5–F9, Ctrl+Z), except when typing
   in a text field.
8. No built-in DSP/synth appears anywhere; plugin menus unchanged.

## Pending items and handoff

- **Proposed ESTADO.md lines** (for the ledger scribe only; owner = Diogo):
  - Estado medido: «UX windows: detachable panels (Playlist, channel rack, piano roll, Mixer, Browser)
    with monitor-validated layout persistence and bring-back cascade; Core look unchanged (Theme diff
    additive-only), dropdowns replace tool/filter button rows. Gate VERDE merge+desktop @ `38e695d`,
    212 tests / 3129 checks / 0 failures (baseline 202/3059 + window_tests 10/70), version==sha; window_tests 10/70 (red 17 checks on base), self-test 58 transitions
    (base 51), steps 39–45. Source: owner directive 2026-11-18 «metam as janelas assim … deixem o ui
    como esta no core. usem dropdowns em vez de muitos botoes para escolhar» (coordinator dispatch);
    report docs/relatorios/2026-11-18-ux-detachable-windows.md.»
  - Pendência: «UX-W7 visual acceptance on real monitors (two-monitor detach/dock, off-screen rescue,
    legacy settings) — checklist in the report; self-test is null-backend, not a UI proof. Owner:
    Diogo / Test Engineer.»
  - Pendência: «Integration rebase with flawless-modules / flawless-clean / flawless-ux-piano:
    MainComponent.cpp, PatternsWindow, PatternEditor carry parallel edits; regions preserved, textual
    conflicts possible. Owner: PM.»
  - Pendência (gate hardening proposal, not implemented here — `ci/gate.ps1` is shared): «the gate's
    Ninja deps check accepts any target with deps; a per-target check (every C++ target's objects have
    deps > 0) would catch the UX-W8 stale-object case. Also compare the self-test's `build=` id against
    HEAD in the desktop gate. Owner: Diogo / gate maintainer.»
- Risks: `MainComponent.cpp` is a hot file across streams — rebase carefully, keep each stream's
  regions; the rack's bottom-slot docking shares space with the piano roll (only one occupies the
  slot; the other detaches or hides — by design, as in the Mixer/patterns behavior before).
- Nothing pushed, no merge; main checkout untouched; no `git stash` used; no AGPL code transcribed;
  no DSP added; `ESTADO.md` not edited.
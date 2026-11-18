# Pulse UX slice A — the two UX branches reconciled into one build, on the revised plan

- Date: 2026-11-18
- Stream: `ux-pulse-A` (Pulse UX adoption plan §4.A)
- Branch: `ux-pulse-slice-A` · Worktree: `D:/CORE/CORE/.agents/worktrees/ux-pulse-A`
- Base: `integration/flawless-round2` @ `a209002` (already carries `flawless-ux-piano` @ `8ce28fc`)
- Merged: `flawless-ux-windows` @ `d23704a` (merge-base `f6b86dc`, verified before merging)
- Delivered tip: **`69c2850b8a88c3f61f791d994a5b895ed77fb30c`** (`69c2850`)
- Plan followed: `pulse-ux-adoption-plan.md`, **the revised version the owner attached mid-slice** — its §3 conflict
  contract, §4.A acceptance and §5 gates. Two of its deltas supersede the version this slice started from and are
  implemented here: the key-range modifier is **Shift+wheel** (not D103's Ctrl+wheel), and the piano toolbar is **one
  compact row** with a View menu (no second permanent button row). The ledger and the old branch reports were not
  rewritten; the superseded wording is kept as a quote and marked.
- Authorization: D102 / D103 / D111 plus the owner's latest instruction in the revised plan. No new D-number invented.
- Build: `D:/Core-build/ux-pulse-A` · Evidence: [evidencia/2026-11-18-ux-pulse-A/](evidencia/2026-11-18-ux-pulse-A/)
- Scope: reconcile + qualify. Nothing pushed, no merge to main/origin, no `git stash`. The main checkout working tree
  and the `core-pulse-ux-adoption` worktree were never written to (the plan was read by path only).

## Verdict

**One build carries ux-piano's gestures and ux-windows' detachable panels. Gate VERDE in both modes at `69c2850`,
version == sha with no `.modified`, and the app self-test `ok` with both UX groups in its step list — the piano group
(`piano gestures`, `piano surfaces`) and the window group (`mixer detach/dock`, `piano roll + browser detach/dock`,
`channel rack dock/detach/close`, `playlist detach/dock`, `layout save/restore, off-screen, bring back`,
`legacy settings load`), 64 logged transitions, one final exit.** 269 cases / 3684 verifications / 0 failures
(round-2 tip 259/3614, **+10/+70 = `window_tests`**, nothing else moved). Real-monitor and real-plugin runtime are
**declared not verified** — see Classification.

## Commits

| Commit | What |
| --- | --- |
| `f5f9b23` | the merge of `d23704a` into `a209002` with the five §3 collisions resolved |
| `0d2135d` | the revised plan's deltas: Shift+wheel key range, one compact toolbar row + View menu, step-38 probe |
| `69c2850` | the self-test's Shift+wheel proof corrected (40 notches to the 128 cap, both directions) |

Diffstat vs `a209002`: **22 files changed, 1738 insertions(+), 243 deletions(-)**.

## The five collisions, and what was kept from each side

| # | Collision | Resolution in this build | Kept from ux-piano | Kept from ux-windows |
| --- | --- | --- | --- | --- |
| 1 | Four tool buttons vs a three-item dropdown | ONE `toolBox_` dropdown: **Draw, Paint, Select, Erase** | `enum class Tool { Draw, Paint, Select, Erase }` untouched; the `tool_ == Tool::Paint` seeding path (`PatternEditor.cpp:1285`), every gesture handler (drag-to-size create, last used length, Shift clone, Alt free move, Ctrl lasso in any tool, Alt / Ctrl+Alt wheel velocity), Core's right-button delete (`:1264`, `e.mods.isPopupMenu()`) and the per-tool cursors (Draw normal, Erase hand, Paint/Select crosshair) | the dropdown representation itself («usem dropdowns em vez de muitos botoes para escolhar») and `toolBox_.setEnabled(editable)`; its three-item list was **not** taken unchanged |
| 2 | Piano toolbar vs window controls | **One compact row**: context (pattern box, New/Clear/Remove, length, title + the keys hint), Tool, Snap, Note tools, **View ▾**, Edit, then a **dedicated header area** at the far right for detach/close. Key range, all-128, octave, scale/root and zoom moved into the View menu — no second permanent button row | every ux-piano function survives and is driven by the same code: `keysMore()/keysLess()/fitKeys()`, the octave and zoom actions, `setScaleGuide()`, `openNoteTools()`; the View menu calls those functions directly (not `Button::triggerClick()`, which only *posts* a message), so the menu, the wheel gestures and the self-test share one path | `detach_` / `closePanel_`, the `detach`↔`dock` icon and tooltip driven by `isDetached()`, wired to `onToggleWindow` / `onClosePanel` (`MainComponent.cpp:2067-2069`) |
| 2b | Narrow widths | `placeToolbarRow()` lays the row out in priority order; a slot that no longer fits is hidden and collected into `overflowMore_` ("More ▾") instead of being clipped. `buildOverflowMenu()` re-exposes a hidden button as its own action and a hidden dropdown as a submenu of its choices, honouring each control's enabled state. The detach/close header area and the title's 60 px floor never overflow, so a narrow window can always dock or hide the panel again | — (required by plan §3) | — |
| 3 | Rack filter buttons vs dropdown | ONE `filterBox_` catalogue-source dropdown: **All / Built-in recipes / Mine**; `filter_` semantics unchanged (`0/1/2`, `refreshTemplates()`) | the round-2 wording that retired P-Core's name — the tooltip says **Pattern Maker's** built-in pattern recipes, "notes only, never an instrument", and the item is labelled "Built-in **recipes**" so it cannot read as a built-in instrument (D110: Core ships none) | the dropdown plus `windowBtn_` / `closeBtn_` and their `host_.toggleWindow` / `host_.closePanel` wiring |
| 4 | Both branches use self-test cases 39/40/41 | Piano group kept at **39-40**, window group renumbered **41-47**, ONE final exit at case 47. Case 38 keeps ux-piano's `next("diagnostics")` transition (ux-windows' `next("logs")` label dropped — same step). No test was deleted to resolve the conflict | case 39 "piano gestures" (create drag length, one undo, last used length, Shift clone, Alt free move, Ctrl lasso in any tool, plain/Alt/Ctrl+Alt wheel velocity, and now the Shift+wheel key range both ways) and case 40 "piano surfaces" (Paint seeds per cell + one undo, all-128 keys, Keys−/Keys+, the scale root mark, Note tools open/quantize/close) | cases 41-47: mixer detach/save/dock, piano roll + browser detach/dock, channel rack dock/detach/close, playlist detach (clip edit + undo + geometry probe), layout save/restore + off-screen rescue + bring-back cascade, legacy settings load, and the detached-Browser picture |
| 5 | Context menu vs imported erase convention | No change needed and none made: `git diff f6b86dc d23704a` touches no right-button, context-menu or `mouseDown` handler, so Core's right-button action stands on every surface — the piano's right-click still deletes a note (`PatternEditor.cpp:1264`) and no arrangement context menu erases clips | Core's right-button behaviour on every surface | its window plumbing only |

Framework reused in place, as §3 requires: `PanelWindow`, `winlayout::Panel`, docked/detached/hidden state,
`window.<id>` settings, bring-back-to-screen. No second docking framework, no new persistence store, no global
keyboard hook. License: Pulse was read for behaviour only (`D:/CORE/plan/pulse-src`); no Pulse source or asset is in
this diff; Core's Theme tokens, colours, typography and menus are unchanged apart from the additive icon-list comment.

## The revised plan's two deltas

**Shift+wheel is the key range; Ctrl+wheel is the time zoom** (`mouseWheelMove`). Dispatch order, so nothing is
swallowed: Ctrl+Alt = fine velocity ±1 % → Ctrl = zoom, with Ctrl+Shift = move along the pattern while zoomed →
Alt = velocity ±5 % over a note, or bigger/smaller keys over empty space → **Shift = Keys+/Keys−** on the piano roll
(drums have no key range, so they keep the horizontal move) → a sideways swipe moves along the pattern → the plain
wheel scrolls the keys by one, or a drum step's velocity. The old Shift+wheel horizontal navigation is therefore
reassigned, and one gesture never has two jobs. `keysWheel()` now uses the y it was always given: the key under the
mouse stays put, so the range grows or shrinks around what is being looked at instead of jumping an octave.
Tooltips, the keys hint, the editor tooltip and the self-test all follow; the historical owner quote
«no piano ctrl+scroll keys+ e keys» is kept in the header as a quote, marked superseded.

**One compact toolbar row.** `headerHeight()` is now `kHeader` for every track and `kRow2` is gone. The nine
key-range/scale/zoom controls are still constructed and still own their handlers, but they are `addChildComponent`
children rather than row residents, so the View menu and the self-test drive exactly the same functions.

## Two reds measured in the combined build, then green

The plan requires both groups to *execute*, so the desktop gate was the real test of the merge — and it went red twice
before it went green. Merge mode was green at every iteration; only the self-test chain failed.

1. **`failed: patterns window`** (step 38) — a semantic clash the auto-merge hid. `openPatterns()` came from the
   window branch: `ensurePatternsPanel(); showPanel("channelRack")`. The rack is now a **docked panel**, and a native
   `PatternsWindow` exists only while it is detached (`MainComponent.cpp:2676`). Step 38 — the modules branch's
   kept-surface check — still asserted on `patternsWindow_->isVisible()`, so the combined build failed with every unit
   suite green. The probe now reads the merged reality (`panelShown("channelRack")`,
   `patternsPanel_->patternOf("kick")`, `hidePanel`), and the Pattern Maker card check the same way. The checks were
   kept, not dropped: the rack must still open and still show the kick pattern. Fixed in `0d2135d`.
2. **`failed: shift wheel keys+ keys=93`** (step 39) — my own new probe was wrong, not the product. Twenty notches
   were tuned to the old *unanchored* Ctrl+wheel; from the fitted 13-key view of the self-test window, 20 × 4 keys
   only reaches 93, so pitch 100 stayed out of view. Now 40 notches (enough from any starting view) reach the 128 cap,
   the check asserts the count *and* the visibility of a high key, a Ctrl+wheel notch is proven **not** to change the
   key count (and is zoomed straight back out so the following steps keep their view), and Shift+wheel down is proven
   to shrink the range and return to the same 128-key view. Fixed in `69c2850`.

## Gate (Windows MSVC Release, Ninja, vendored JUCE `D:/Core-deps/JUCE-9.0.5`, under GATE-LOCK)

Commands, from the worktree: `powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/ux-pulse-A -Jobs 6`
then the same with `-Desktop`. `GATE-LOCK` (`stream=ux-pulse-A`) was created by this stream before the first gate and
deleted right after the last one exited, before verification; no lock held by another stream was touched (the
`coord-round2-verify` re-gate was not running when this stream took the lock).

| Mode | Result | ctest | Test time | Exit |
| --- | --- | --- | --- | --- |
| merge | **VERDE** | 100 % tests passed out of **22** | 32.15 s | 0 |
| desktop | **VERDE** | 100 % tests passed out of **23** (22 + `desktop_self_test`) | 55.71 s | 0 |

- **version == sha**: `core-engine 0.7.0+69c2850b8a` == HEAD `69c2850b8a88c3f61f791d994a5b895ed77fb30c`, **no `.modified`**.
- **Per-suite counts** (`ctest -L merge -V`, 31.99 s, `ctest3-merge-verbose.txt`): **269 cases / 3684 verifications /
  0 failures** across the 16 suites that print summaries; the 6 CLI tests (`cli_version`, `cli_demo_render`,
  `cli_measure`, `cli_unicode`, `cli_package_stale`, `cli_plugin_crash_render`) pass separately. Round-2 tip
  `a209002` = 259 / 3614, so the net is **+10 cases / +70 verifications, all of them `window_tests`** — every other
  suite is byte-for-byte at its round-2 count, nothing shrank.

| Suite | cases | verifs | fails | | Suite | cases | verifs | fails |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| engine_tests | 39 | 221 | 0 | | audit2_tests | 15 | 364 | 0 |
| domain_tests | 37 | 639 | 0 | | hunt_tests | 13 | 121 | 0 |
| legacy_guard_tests | 8 | 158 | 0 | | round3_tests | 7 | 82 | 0 |
| modules_tests | 7 | 137 | 0 | | calls_tests | 12 | 69 | 0 |
| role_tests | 21 | 216 | 0 | | menu_tests | 7 | 366 | 0 |
| midi_tests | 20 | 402 | 0 | | **window_tests (new)** | **10** | **70** | **0** |
| bug_tests | 49 | 493 | 0 | | engine_process_tests | 15 | 256 | 0 |
| bitwig_tests | 2 | 25 | 0 | | audit_tests | 7 | 65 | 0 |
| | | | | | **total** | **269** | **3684** | **0** |

## Self-test: both UX groups execute in the combined build

`desktop_self_test` → **ok**, `build=0.7.0+69c2850b8a engine=0.7.0+69c2850b8a`, document=73 accepted=73 applied=73,
tick=1942.6, loudest=0.735, status=Ready, `Output: Null NULL Playback Device · 48000 Hz · block 128`,
**64 logged transitions** (56 through `diagnostics` = the round-2 chain, +2 piano, +6 window), picture
`self-test.png` plus the detached-Browser picture. Final line:

`ok: M1c + M2 path, live notes, audio clips, mixer, save with audio, piano roll Pulse gestures, detachable windows`

- **Piano group (cases 39-40)** — logged transitions: `piano gestures`; `piano surfaces`.
  Checks executed: create-drag length + one undo, last used length, Shift clone + one undo, Alt free move, Ctrl lasso
  in any tool, plain/Alt/Ctrl+Alt wheel velocity, Ctrl+wheel leaves the key count alone, Shift+wheel up to all 128
  keys and back down; Paint seeds per cell + one undo, all-128 keys, Keys−/Keys+, the scale root mark, Note tools
  open → Quantize in one undo → close.
- **Window group (cases 41-47)** — logged transitions: `mixer detach/dock`; `piano roll + browser detach/dock`;
  `channel rack dock/detach/close`; `playlist detach/dock`; `layout save/restore, off-screen, bring back`;
  `legacy settings load`; case 47 takes the detached-Browser picture and is the single final exit.
  Checks executed: the mixer's detached place saved as `Detached` with a non-empty rect and docked back into the main
  window; the piano roll and the Browser out and back with the side panel's place freed while detached; the rack
  docked → detached (content no longer parented to the main window) → re-docked → closed; the Playlist detached with a
  track select, a clip edit, an undo and the geometry probe `xOfTick(kBar)==laneSpan().getX()` /
  `xOfTick(5*kBar)==laneSpan().getRight()`; a saved place restored, an off-screen place rescued through
  `winlayout::reachable`, the bring-back cascade; a legacy settings file with no window keys loading the defaults with
  `showLibrary` honoured.

Compile success alone was not accepted as proof: the step list above is the measured log of the same run that produced
the gate numbers, and the two reds in the previous section show the chain can fail and did.

## Classification

- **Confirmed in code:** the five resolutions, the compact row + View menu, the overflow mechanism, the Shift/Ctrl
  wheel remap and its dispatch order, the renumbered chain (cases 39-47, one `finish(true, …)`), the step-38 probe
  fix, the tooltip corrections, and that no test was deleted to resolve a conflict (`tests/window_tests.cpp` added
  whole; `tests/CMakeLists.txt` keeps `modules_tests` **and** `window_tests`).
- **Proven by test:** gate merge 22/22 VERDE + desktop 23/23 VERDE at `69c2850`; 269 cases / 3684 verifications /
  0 failures; version == sha, no `.modified`; self-test `ok` with both UX groups' transitions in its step list; the
  window group's no-recreation probes (`getParentComponent()` identity across detach/dock, edit + undo + geometry
  while the Playlist is detached) and the layout save/restore, off-screen rescue, bring-back and legacy-settings cases.
- **Reported only (not re-measured here):** the two branches' own red-first measurements, cited from
  `8ce28fc:docs/relatorios/2026-11-18-ux-piano-roll.md` and `d23704a:docs/relatorios/2026-11-18-ux-detachable-windows.md`.
- **Not verified (declared, not claimed):** real monitors (mixed DPI, an unplugged monitor, negative coordinates,
  maximisation on a second screen), a real external plugin in a detached editor, save/undo/transport exercised by hand
  in a detached editor (the self-test covers them on the null backend), the visual acceptance of the compact row, the
  View menu and the overflow menu at 1366x768 / 1920x1080 and at 100/150/200 % scaling, the revised plan's
  performance comparison and its "Core visual identity" before/after comparison, and external-plugin-editor focus as a
  separate case from in-process focus. Slice B's "Fit used notes" as a distinct action is not in this slice: the View
  menu offers "All 128 keys", which is what `fitKeys()` does.

## Safety confirmations

- The main checkout `D:/CORE/CORE` working tree was **never edited**; the `core-pulse-ux-adoption` worktree was read by
  path only. All work is isolated in `D:/CORE/CORE/.agents/worktrees/ux-pulse-A`.
- **Nothing pushed**, no merge to main/origin, **no `git stash`**. The coordinator gates and sequences the merge.
- Vendored JUCE (`D:/Core-deps/JUCE-9.0.5`) used read-only, not modified. No Pulse source or asset copied (AGPL).
- No instrumentation left in the tree: the working tree is clean at `69c2850` apart from this report, its index row and
  the evidence folder.

## Proposed ESTADO.md lines (for the ledger scribe — NOT written here, D72/D80)

Source: this run, worktree `ux-pulse-A`, tip `69c2850`, evidence `docs/relatorios/evidencia/2026-11-18-ux-pulse-A/`.
No new owner verbatim quote is claimed beyond those already in the ledger and the revised adoption plan.

- **Estado medido (proposed):** "Pulse UX slice A: `flawless-ux-windows` (`d23704a`) merged into
  `integration/flawless-round2` (`a209002`) on branch `ux-pulse-slice-A` @ `69c2850`, the round-2 blocker cleared
  with the plan's §3 contract — one Tool dropdown (Draw/Paint/Select/Erase, `Tool::Paint` and every gesture handler
  kept), one compact piano toolbar row with a View menu and a dedicated detach/close header area (no second permanent
  button row; overflow menu before clipping), one rack catalogue-source dropdown (All / Built-in recipes / Mine),
  the self-test chain renumbered so the piano group (39-40) and the window group (41-47) both run with a single final
  exit, and Core's right-button behaviour untouched. The owner's revised plan deltas are in: Shift+wheel is Keys+/Keys−
  (anchored on the key under the mouse, proven both ways to all 128 keys) and Ctrl+wheel is the time zoom, the old
  Shift+wheel horizontal move reassigned to Ctrl+Shift+wheel and the sideways swipe; the stale Ctrl+wheel tooltips were
  corrected. Gate **VERDE merge 22/22 (32.15 s) + desktop 23/23 (55.71 s)** under GATE-LOCK, **269 cases /
  3684 verifications / 0 failures** (round-2 tip 259/3614; +10/+70 all in `window_tests`, nothing shrank), version
  `0.7.0+69c2850b8a` == tip, no `.modified`; self-test **ok**, 64 logged transitions, loudest 0.735, both UX groups in
  the step list. Two reds measured in the combined build and fixed: step 38 `patterns window` (the rack has been a
  docked panel since the window slice, so the probe read the panel state) and step 39 `shift wheel keys+ keys=93`
  (the probe's notch count, not the product). Real-monitor and real-plugin runtime **not verified**; nothing pushed,
  main checkout untouched, vendored JUCE unmodified."
- **Pendência (proposed):** close the round-2 pendência for the ux-windows design decision — the decision arrived as
  the revised adoption plan and slice A implements it; keep `ux-pulse-slice-A` @ `69c2850` awaiting the coordinator's
  gated merge after the round-2 and built-in-sound-removal streams land. Open/keep the follow-ups this slice did not
  cover: real-monitor qualification (mixed DPI, unplugged monitor, negative coordinates, maximisation on a second
  screen), real-plugin runtime in a detached editor, the performance and Core-visual-identity comparisons, and slice B
  (`Fit used notes` as an action distinct from `All 128 keys`, cross-track ghost notes, the Note tools
  Preview/Apply/Cancel contract). Do not renumber existing decisions.

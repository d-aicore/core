# Consolidation round 2 — 13 of 14 branches integrated and gated; ux-windows blocked on a UX design overlap

- Date: 2026-11-18
- Base: `f6b86dc` (origin/main)
- Integration branch: `integration/flawless-round2`
- Integrated tip (gated): **`bfcd2119b94f89b486b505ce05afe1458ee0386f`** (`bfcd211`)
- Worktree: `D:/CORE/CORE/.agents/worktrees/integration2` (the main checkout working tree was never edited)
- Build: `D:/Core-build/integration2` · Evidence: `D:/Core-build/integration2-evidence/`
- Scope: consolidate + gate only. Nothing pushed, no merge to main/origin, no `git stash`. The PM verifies and pushes.

## Verdict

**13 of the 14 ready branches are merged `--no-ff`, union-proven and gated VERDE (merge + desktop), version == tip sha, clean tree.**
The 14th, `flawless-ux-windows` (`d23704a`), is **BLOCKED**: it collides semantically with `flawless-ux-piano` (and the
merged modules/builtins surfaces) on the piano-roll toolbar, the Patterns-window filter, and the self-test step
sequence. This is a genuine UI design overlap, not a textual conflict — it cannot be unioned mechanically (duplicate
`case 39/40/41` labels are a hard compile error) and resolving it requires an owner/PM design decision. The merge was
aborted; the branch ref is intact and nothing of it was lost. See "Blocker" below.

## The 14 branches

| # | Branch | HEAD | Merge commit | Status |
| --- | --- | --- | --- | --- |
| 1 | flawless-l9 | `e7037da` | `9876481` | merged |
| 2 | flawless-crashreset | `7ad1b72` | `05d55d3` | merged |
| 3 | flawless-m5 | `569d995` | `caeb980` | merged |
| 4 | flawless-state | `81da46e` | `6ac6fbd` | merged |
| 5 | flawless-exportpart | `dff5ba9` | `6139be9` | merged |
| 6 | flawless-batch2 | `b64e95e` | `6c4efef` | merged |
| 7 | flawless-roll2 | `2718c64` | `9a5429a` | merged |
| 8 | flawless-ratified | `c7612c3` | `3a9a854` | merged |
| 9 | flawless-builtins | `e035eb7` | `49db936` | merged |
| 10 | flawless-samplehost | `4a508b3` | `581ae30` | merged |
| 11 | flawless-clean | `e5f2fca` | `ac116cb` | merged |
| 12 | flawless-modules | `e6ead02` | `9975453` | merged |
| 13 | flawless-ux-piano | `8ce28fc` | `bfcd211` (tip) | merged |
| 14 | flawless-ux-windows | `d23704a` | — | **BLOCKED (not merged)** |

All 14 branch HEADs were verified equal to the assignment's expected SHAs before merging; every branch forks directly
from `f6b86dc` (merge-base checked).

## Conflict resolutions per hotspot

Every conflict was resolved preserving all sides' behaviour. Resolutions were verified by a two-way deletion proof
where applicable (removing one side's added block restores the other side's blob byte-for-byte) and by the whole-tree
union proof below.

- **docs/relatorios/README.md** (all 13 owners): pure table-row appends at one anchor. Kept every row in merge order;
  de-duplicated the identical "Silent built-ins" row that `flawless-samplehost` re-carries from `flawless-builtins`.
  Union-verified: all 13 branches' rows present.
- **tests/engine_process_tests.cpp** (crashreset × m5): two whole new TESTs interleaved by shared body lines.
  Reconstructed as crashreset's wide-window crash-in-reset TEST + m5's bounded-Stop TEST, each whole. Two-way proof passed.
- **tests/engine_tests.cpp** (m5/state × ratified × clean): EOF appends kept whole; clean's PT→EN test renames and
  demo→explicit-legacy-song rewrites auto-merged; clean's L10 amplitude TESTs and samplehost's sample-host TESTs both
  kept (shared closing brace restored). All helpers (`pluginSong`, `FakeProvider`, `EventLog` from graph.hpp, `writeSine`) present.
- **engine/src/core/render_job.cpp/.hpp** (exportpart × batch2): genuine signature overlap. Unified
  `publishRenderOutputs(outputs, published, beforePlace={}, workingPaths={}, rollback=nullptr)` — exportpart's reserved
  working files AND batch2's `PublishRollback` reporting both kept. The auto-merged body already used `partOf(f)`
  (exportpart) and the rollback undo/report (batch2). The `run()` call passes `working, &job->rollback`. batch2's two
  audit2 call sites updated to insert `{}` for `workingPaths` before `&rb` (they test the legacy `.part` path). The
  synchronous CLI caller (`server.cpp:799`, 2 args) is unchanged via defaults.
- **tests/audit2_tests.cpp** (exportpart × batch2): include union; both test blocks kept whole (shared brace restored);
  batch2 call sites coordinated as above.
- **app/domain/src/pattern_rack.cpp/.hpp** (roll2 × ratified × samplehost × modules): both template fields preserved.
  `rackTemplateFromJson`/`ToJson` keep roll (BUG-SWEEP-20) AND swingBase (BUG-SWEEP-26). In `rackTemplateFromSlot` the
  ratified swingBase extra-filter is kept but its `n.hasRoll = false;` line was dropped so roll survives (roll2's
  feature); the `unswing` export parameter (ratified) is kept. Verified orthogonal: `applyRackTemplate` copies whole
  Notes (roll included) and `swingNote` only moves ticks.
- **app/domain/src/document.cpp** (state × samplehost): kept state's `pluginRefWithoutState` projection (state-per-edit)
  AND added samplehost's `sample` instrument projection branch.
- **engine/src/dsp/instruments.cpp** (l9 × samplehost): l9 removed the `makeEffect` field from `NativePluginDesc`
  (6 fields now). Kept l9's 6-field registration loop and added samplehost's sample-host registration with the trailing
  `nullptr` removed to match the merged struct. `kSampleHostVersion`/`SampleHost` from sample_host.hpp; graph.cpp builds
  the real host (auto-merged, l9's plugin-only effects intact).
- **app/domain/src/randomize.cpp/.hpp, samples.cpp/.hpp** (builtins × samplehost): HEAD is byte-identical to samplehost.
  samplehost's history shows it built ON builtins (`5730842` BUG-SWEEP-11 guard → `254a55c` BUG-SWEEP-11 positive); the
  interim guards were intentionally superseded by the positive plugin-param / sample-host features. Not lost work.
- **tests/domain_tests.cpp** (state × builtins × samplehost × clean): builtins' BUG-SWEEP-09 characterization and both
  BUG-SWEEP-10 tests are present verbatim; builtins' interim BUG-SWEEP-11 "graceful no-op" test is superseded by
  samplehost's stronger "targets role plugins / hidden with no plugins / keeps legacy" + positive tests. state's and
  clean's tests kept.
- **app/desktop/src/MainComponent.cpp** (state/batch2/ratified/builtins/samplehost/modules/ux-piano): all regions kept.
  builtins' "Randomize sound (built-in instruments)" menu item was refined by samplehost to "Randomize sound" with the
  same `soundAxisAvailable` guard and `kRandomize+5` handler (feature preserved). modules' tools/score self-test checks
  kept in case 38; the case-38 transition takes ux-piano's `next("diagnostics")` so ux-piano's cases 39–41 run (modules'
  `finish(true,…)` would have terminated the chain early and skipped ux-piano's piano steps). The final finish message is
  ux-piano's; modules' checks still execute.
- **tests/CMakeLists.txt** (state/batch2/builtins/samplehost/clean/modules): all test registrations combined; the
  `set_tests_properties` merge-label list keeps every target. `window_tests` is absent (ux-windows not merged).
- **lang/pt-PT.json** (batch2/ratified/samplehost/modules): union-verified, all entries present.
- **ci/patches/juce-failed-plugin-restart.patch**: byte-identical to crashreset's regenerated 14-hunk version.

## Union proof (whole tree, 13 merged branches)

Method (`tools/union_proof.py`, output `union-proof.txt`): for every file changed by any merged branch vs `f6b86dc`,
single-owner files must be byte-identical to the owner branch blob (`git diff --quiet`); multi-owner files must contain
every line each owner added (line-multiset subset); deletions must be absent; zero real conflict markers
(`^<<<<<<< `, `^>>>>>>> `, `^=======$` — the precise form, so miniaudio.h's decorative `====` banners are not false hits).

- **Single-owner: 103 files byte-identical, 0 failures.**
- **Deletions: 22 files correctly absent** (l9's `effects.cpp` + clean's 21 shipped templates).
- **Multi-owner: 46 files; 35 fully clean; 11 with gaps — every gap accounted for** as a documented coordination
  (render_job signature/calls, instruments struct field, pattern_rack roll+swing, audit2 call sites, MainComponent
  self-test transition / menu label) or an intended supersession (randomize/samples HEAD==samplehost; domain_tests
  interim BUG-SWEEP-11 test → positive). No accidental loss.
- **Conflict markers: NONE.**
- **No undocumented test removal.** TEST() counts base→HEAD grew or held in every suite except `legacy_guard_tests`
  9→8: the removed TEST is "the five visible templates open, project and render finite", deleted by clean because the
  shipped templates were removed (owner «remove templates com efeitos»). clean's hunt P04 replaced the 21-template loop
  with a single legacy-song projection guard (TEST count unchanged; the live behaviour is still guarded). modules renamed
  role/round3 TESTs PT→EN and added 7 `modules_tests` (no coverage lost).

Per-suite TEST() counts (base → HEAD): engine 25→39, domain 30→37, legacy_guard 9→8 (documented), modules 0→7,
role 21→21, midi 14→20, bug 39→49, bitwig 2→2, audit 7→7, audit2 6→15, hunt 13→13, round3 7→7, calls 12→12,
menu 7→7, engine_process 10→15.

## Gate (Windows MSVC Release, Ninja, vendored JUCE `D:/Core-deps/JUCE-9.0.5`, under GATE-LOCK)

Commands (forward slashes, from the worktree):
`powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/integration2 -Jobs 6` then `-Desktop`.
GATE-LOCK created (stream=integration2, timestamp) before the gate and deleted right after both modes, before verification.

- **Merge: VERDE**, 100% tests passed out of **21** ctest entries, **368 s**, code @ `bfcd211`, clean tree.
- **Desktop: VERDE**, 100% tests passed out of **22** (21 merge + desktop_self_test), **58 s**, @ `bfcd211`.
- **version == sha**: `core-engine 0.7.0+bfcd2119b9` == HEAD `bfcd2119b9…`, **no `.modified`** (clean committed tree).
- **Per-suite counts** (`ctest -L merge -V`, 32.02 s, `ctest-merge-verbose.txt`): **259 cases / 3614 verifications / 0 failures**
  across the 15 suites that print summaries; the 6 CLI tests (cli_version, cli_demo_render, cli_measure, cli_unicode,
  cli_package_stale, cli_plugin_crash_render) pass separately. Base `f6b86dc` = 202 cases / 3059 verifications
  (baseline-counts.txt); net **+57 cases / +555 verifications**, no undocumented shrinkage.

| Suite | cases | verifs | fails | | Suite | cases | verifs | fails |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| engine_tests | 39 | 221 | 0 | | audit2_tests | 15 | 364 | 0 |
| domain_tests | 37 | 639 | 0 | | hunt_tests | 13 | 121 | 0 |
| legacy_guard_tests | 8 | 158 | 0 | | round3_tests | 7 | 82 | 0 |
| modules_tests | 7 | 137 | 0 | | calls_tests | 12 | 69 | 0 |
| role_tests | 21 | 216 | 0 | | menu_tests | 7 | 366 | 0 |
| midi_tests | 20 | 402 | 0 | | engine_process_tests | 15 | 256 | 0 |
| bug_tests | 49 | 493 | 0 | | bitwig_tests | 2 | 25 | 0 |
| audit_tests | 7 | 65 | 0 | | **total** | **259** | **3614** | **0** |

- **App self-test** (`ctest -R desktop_self_test -V`, 23.22 s, `self-test.txt`/`.png`): ok,
  `build=0.7.0+bfcd2119b9 engine=0.7.0+bfcd2119b9`, document=73 accepted=73 applied=73, loudest=0.735, status=Ready,
  Null device 48000 Hz block 128. Steps include "effects: plugins only; no built-in limiter" (l9), "plugin state kept
  across the crash" (state/crashreset), "Try again offered and used; retried plugin rebuilt healthy" (batch2-05),
  "mixer; diagnostics" (modules/tools), "piano gestures; piano surfaces" (ux-piano cases 39–40 — confirms the case-38
  `next("diagnostics")` resolution runs the piano steps). The detachable-window steps are absent (ux-windows not merged).

## Diffstat vs f6b86dc

`171 files changed, 9563 insertions(+), 274999 deletions(-)`. The large deletion count is clean's 21 removed shipped
templates (e.g. `psycore-blacktide.core-template.json` alone = 55323 lines) plus l9's `effects.cpp`; these are the
documented removed-feature deletions, not lost product code.

## Blocker — flawless-ux-windows (required input before it can land)

`flawless-ux-windows` (`d23704a`, detachable panels framework) was attempted last and aborted. It redesigns the same
surfaces as the already-merged `flawless-ux-piano` (and modules/builtins) in incompatible ways:

1. **Piano-roll tool widget** (`PatternEditor.h/.cpp`): ux-piano uses four tool buttons `drawTool_/selectTool_/eraseTool_/paintTool_`
   with a `Tool::Paint` enum value, a second toolbar row (keys/scale dropdowns, Note-tools button) and a two-row header;
   ux-windows replaces the tools with ONE `toolBox_` dropdown (Draw/Select/Erase, no Paint) plus `detach_/closePanel_`
   window controls. `setTool()`, the visibility list and `resized()` are contradictory between the two.
2. **Self-test step sequence** (`MainComponent.cpp selfTestStep`): both define `case 39/40/41` — ux-piano = piano Pulse
   gestures / surfaces / finish; ux-windows = mixer/piano/browser/channel-rack detach-dock / finish. Case 38 transitions
   differ (`next("diagnostics")` vs `next("logs")`). **Duplicate case labels are a hard compile error**; no mechanical union exists.
3. **Patterns-window filter** (`PatternsWindow.cpp`): filter buttons (All/Built-in/Mine) vs a `filterBox_` dropdown + window controls.

Conflicted files at abort: MainComponent.cpp (1 block), PatternEditor.cpp (6), PatternEditor.h (1), PatternsWindow.cpp (1),
Theme.h (1, icon-list comment), README.md (1), tests/CMakeLists.txt (1). Evidence: `ux-windows-blocker.txt`.

**Why this is a stop, not a guess:** the project rules require an explicit numbered decision for the final design of new
surfaces and design agreement for UI changes; ux-windows itself quotes the owner favouring dropdowns
(«usem dropdowns em vez de muitos botoes para escolhar»), while ux-piano ships tool buttons incl. Paint. Choosing one
representation, deciding how Paint/Note-tools compose with detach/dock/close, and renumbering the self-test chain are
owner/PM design decisions, not mechanical merge work. Forcing a guess would overwrite one branch's ratified design.

**Required input:** an owner/PM decision on (a) the unified piano-roll tool representation (buttons vs dropdown) and how
ux-piano's Paint tool + Note-tools/keys row coexist with ux-windows' detach/dock/close controls; (b) the Patterns-window
filter representation; (c) the merged self-test step sequence (renumber ux-windows' detach cases after ux-piano's piano
cases, or vice-versa). With that decision, ux-windows can be re-merged onto this tip; its window framework
(PanelWindows/WindowLayout/Keymap/LibraryPanel/Mixer detach plumbing, `window_tests`) is otherwise disjoint and ready.

**Impact:** the integrated tip carries 13 branches (all engine/data/state/export/render/menu/modules/piano-roll fixes and
tests). The detachable-panels UX slice is the only missing piece; nothing else is blocked.

## Safety confirmations

- Main checkout working tree **never edited** by this stream; the scribe's uncommitted `ESTADO.md` / `docs/relatorios/README.md`
  are still present. (Observed: main HEAD read `f6b86dc` during the run vs `ed4642e` at session start — a concurrent change
  by another actor, not this stream; all this stream's work is isolated in the integration2 worktree.)
- **Nothing pushed**, no merge to main/origin, **no `git stash`**.
- Every merged branch's fixes and tests are present (union proof above); ux-windows ref intact, nothing lost.
- **Vendored JUCE not modified by this stream** (`D:/Core-deps/JUCE-9.0.5` is a non-git source drop; it already carries the
  D + crashreset patches — `hostCrashed` crash-aware acquire present in `juce_VST3PluginFormatImpl.h`; the gate used it
  read-only via `FETCHCONTENT_SOURCE_DIR_JUCE`). The repo patch file is crashreset's regenerated 14-hunk version.
- GATE-LOCK created before the gate and deleted right after both modes (before verification).

## Proposed ESTADO.md lines (for the ledger scribe — NOT written here, D72/D80)

Source: this consolidation run, worktree `integration2`, tip `bfcd211`, evidence `D:/Core-build/integration2-evidence/`.
The owner's consolidation instruction is the assignment of 2026-11-18 (this session); no new owner verbatim quote is
claimed beyond those already in the ledger.

- **Estado medido (proposed):** "Consolidation round 2: 13 of 14 ready branches merged `--no-ff` into
  `integration/flawless-round2` @ `bfcd211` (base `f6b86dc`), whole-tree union proven (103 single-owner files byte-identical,
  22 deletions correct, 46 multi-owner hotspots union-verified, 0 conflict markers, no undocumented test removal). Gate
  VERDE merge 21/21 (368 s) + desktop 22/22 (58 s), Windows MSVC Release under GATE-LOCK, **259 cases / 3614 verifications /
  0 failures** (base 202/3059, +57/+555), version `0.7.0+bfcd2119b9` == tip no `.modified`; self-test ok document=73
  loudest=0.735 with piano-gesture steps. `flawless-ux-windows` (`d23704a`) BLOCKED on a genuine UX design overlap with
  ux-piano (piano-roll tool buttons-vs-dropdown + Tool::Paint, self-test case 39/40/41 collision, Patterns filter); merge
  aborted, ref intact, owner/PM design decision required. Nothing pushed; main checkout untouched; vendored JUCE not modified."
- **Pendência (proposed):** open/keep a pending item for the ux-windows design decision (unified piano-roll tool
  representation + Paint/Note-tools vs detach/dock/close composition + merged self-test step sequence), then re-merge
  ux-windows onto the round-2 tip and re-gate. Reference this report; do not renumber existing decisions.

## Classification

- **Confirmed in code:** the 13 merge commits, every conflict resolution, the unified render_job signature, the
  roll+swing template preservation, the l9 struct/sample-host registration, the self-test case-38 transition.
- **Proven by test:** gate merge 21/21 + desktop 22/22 VERDE; 259/3614/0; version==sha; self-test ok; union proof
  (byte-identical single-owner, line-presence multi-owner, deletion checks, marker scan, TEST-count comparison).
- **Reported only (not re-measured here):** the individual branches' red-first measurements and null-render bit-exactness
  (cited from their own reports/evidence; this run re-gates the integrated tip green but does not re-run each branch's red).
- **Not verified:** ux-windows runtime (not merged); the owner/PM UX design decision (pending); live WASAPI device behaviour
  (self-test uses the null backend, as in prior rounds).

## Evidence

`D:/Core-build/integration2-evidence/`: `union-proof.txt`, `ux-windows-blocker.txt`, `gate-merge-stdout.txt`,
`gate-desktop-stdout.txt`, `gate-resumo-merge.txt`, `gate-resumo-desktop.txt`, `ctest-merge-verbose.txt`,
`ctest-desktop-verbose.txt`, `self-test.txt`, `self-test.png`, `tools/` (resolution + union-proof scripts).
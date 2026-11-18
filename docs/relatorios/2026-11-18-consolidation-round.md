# Report — 2026-11-18 — Consolidation round: 14 fix branches integrated and gated

## What was done

Consolidated the PM "flawless" team's 14 ready fix branches into one integration branch and gated the
integrated tip on Windows, per the owner's «consolida e envia». This session consolidates and gates;
the PM verifies and pushes (nothing was pushed here).

- Repository: `d-aicore/core`. Base `ed4642e61b78e30fc4f7d950a28a214d0ac7c801` (main, == origin/main).
- Integration branch: **`integration/flawless-round`**, worktree
  `D:/CORE/CORE/.agents/worktrees/integration` (created fresh from `ed4642e`; the main checkout
  `D:/CORE/CORE` was never edited — it carries another session's uncommitted `ESTADO.md` /
  `docs/relatorios/README.md`, left untouched).
- Integrated HEAD: **`29ec5e01e05aa1220085f4cfbea1c9181822dd94`** (`29ec5e0`).
- Build: `D:/Core-build/integration` (MSVC Release / Ninja, JUCE 9.0.5 from `D:/Core-deps/JUCE-9.0.5`).
  Evidence: `D:/Core-build/integration-evidence/`.
- Method: 14 sequential `git merge --no-ff` commits in the order below; shared-file conflicts resolved
  preserving every change from both sides; no `git stash`; no push/merge to main or origin.

## The 14 branches merged (order, branch HEAD, merge commit)

| # | Branch | HEAD merged | Merge commit | Scope |
| --- | --- | --- | --- | --- |
| 1 | `ci/gate-unique-bat` | `cbfab61` | `848ab17` | unique-bat gate.ps1 fix + report |
| 2 | `docs/maps-en` | `7bed196` | `f15ba9b` | English maps (docs/plan) |
| 3 | `flawless-estado` | `3a25a71` | `a620c92` | measured ctest-state report |
| 4 | `flawless-batch` | `15306b0` | `1070983` | BUG-SWEEP-01/04 (storage.cpp, bridge.cpp, domain/calls tests) |
| 5 | `flawless-packaging` | `20e3cb8` | `1b77f9d` | BUG-SWEEP-14 (desktop/tests CMake, cli_package_stale) |
| 6 | `flawless-preservation` | `6888473` | `6ee2f58` | BUG-SWEEP-17/18 (assets.cpp, midi.cpp cleanName, edit.cpp renameTrack) |
| 7 | `flawless-deadcode` | `3830db6` | `8f19f82` | dead-code L1/L2 deletions + legacy_guard_tests |
| 8 | `flawless-tempo` | `5019873` | `ebf0070` | BUG-SWEEP-07 (randomize.cpp retimeBass/setTempo) |
| 9 | `flawless-timeline` | `753fcfd` | `5cb5e60` | BUG-SWEEP-22..25 (edit.cpp section ops) |
| 10 | `flawless-randomize` | `0f83221` | `230fc30` | BUG-SWEEP-29 (edit.cpp removeUnusedPatterns, randomize.cpp pruning) |
| 11 | `flawless-pattern` | `41c38da` | `7465598` | BUG-SWEEP-26..28 (pattern_rack.cpp) |
| 12 | `flawless-roll` | `7b7c82c` | `80a91a3` | BUG-SWEEP-30 (groove.cpp, pattern_tools.cpp, UI, document.hpp) |
| 13 | `flawless-menu` | `365a154` | `cb6d828` | BUG-SWEEP-32 (MainComponent.cpp menu region, MenuIds.h, menu_tests) |
| 14 | `flawless-crash` | `c61bcd1` | `29ec5e0` | item 1 A/B/C + vendored patch file + CI (juce_host.cpp, Restart, selftest-plugins, engine_process_tests) |

All 14 HEADs are ancestors of the integrated HEAD (`git merge-base --is-ancestor` verified for each).

**Moving-branch note (crash):** the task pinned `flawless-crash` at `c61bcd1`. During this session the
D-restart follow-up stream advanced that branch to `cc5b874` ("Engine: a crashed plugin that asks to
restart no longer deadlocks the message thread (FL item 1, D)"). The merge here used exactly `c61bcd1`
(verified: second parent of `29ec5e0` == `c61bcd1`); `cc5b874` (the D Core wiring) is **not** an ancestor
of the integrated HEAD and is **not** included — it is the separately-landing follow-up. Confirmed at
code level: `markCrashedByHost` is called nowhere in the integrated `juce_host.cpp` / `MainComponent.cpp`.

## Conflict resolutions (per hotspot)

Every conflict was an adjacent append or a disjoint region, not a competing semantic edit. No semantic
overlap required stopping. Resolutions:

- **`docs/relatorios/README.md`** (all 14 branches append one index row): union of all rows, append-only.
  Final index = 16 baseline rows + 14 new rows = 30 rows; every branch's row present (verified by link).
- **`ci/gate.ps1`** (unique-bat + 11 identical cherry-picks): all sides carry the same content; the
  integrated file is byte-identical to the `ci/gate-unique-bat` version (`git diff --quiet` confirmed).
- **`app/domain/src/edit.cpp`** (preservation + deadcode + timeline + randomize): auto-merged disjoint
  regions. Verified every line each branch added vs `ed4642e` is present (0 missing for all four), and
  every deadcode-removed function signature is absent (`resetInstrumentParams`, `instrumentAtDefaults`,
  `upgradeOldVersions`, `addEffectWith`, `addPhraseEndComb`, `setEffectSidechain`, `setSampleLoop`,
  `setModulator*`, `duckPresets`, `setDucking*`, `removeDucking`, `removeModulator`, `soundKey`,
  `soundExtra`, `instrumentJson`, `trackSoundKind`, `setTrackSound`, `sameTrackSound` — all 0 occurrences).
  Preservation's `wellFormedUtf8` + `renameTrack` UTF-8 guard kept; randomize's rack-aware
  `removeUnusedPatterns(*t, rackRefs[t->id])` kept.
- **`app/domain/src/edit.hpp`** (deadcode + randomize): union; `removeUnusedPatterns(Track&, const
  std::set<std::string>& rackRefs = {})` signature present (randomize), deadcode declarations removed.
- **`app/domain/src/randomize.cpp`** (tempo + randomize29): union of tempo's `retimeBass`/`setTempo`
  round-trip fix (new `fromBpm`/`toBpm` signature, `notesJson`, `revertTempoCopy`) and randomize29's
  rack-reference pruning loop (`rackRefs` map + `removeUnusedPatterns(*t, rackRefs[t->id])`). Verified
  the merged file is tempo's file + randomize29's pruning, and randomize29's file + tempo's retiming.
- **`app/domain/src/randomize.hpp`** (deadcode + tempo): union; all added lines of both present.
- **`tests/domain_tests.cpp`** (batch + tempo + randomize): the three blocks append before the shared
  trailing `}` + `CORE_TEST_MAIN`. Resolved by closing each block's last TEST before the next block.
  Final 29 TESTs = 19 baseline + 2 batch + 7 tempo + 1 randomize. Verified strict superset of both the
  tempo and randomize branch files (0 removed lines vs each).
- **`tests/bug_tests.cpp`** (preservation + timeline + pattern): same trailing-`}` append pattern.
  Final 36 TESTs = 22 baseline + 2 preservation + 4 timeline + 8 pattern. Verified superset of the
  pattern branch; the only delta vs preservation/pattern is the timeline branch's intentional English
  translation + BUG-SWEEP-22 assertion added to the shared `duplicateSection` test (taken from timeline,
  the newer fix of that same region — no test lost).
- **`tests/midi_tests.cpp`** (preservation + roll): same pattern. Final 12 TESTs = 8 baseline + 2
  preservation + 2 roll. Verified strict superset of both branch files (0 removed lines vs each).
- **`tests/CMakeLists.txt`** (packaging + deadcode + menu + crash): kept packaging's `cli_package_stale`
  block (`if(TARGET core-package)`, RUN_SERIAL) AND the combined label list including both
  `legacy_guard_tests` and `menu_tests`; `legacy_guard_tests` and `menu_tests` executable registrations
  and crash's selftest-plugin wiring all present. (One intermediate edit briefly dropped the packaging
  block; caught and restored before committing — final file verified to contain all registrations.)
- **`app/desktop/src/MainComponent.cpp`** (deadcode + menu + crash): auto-merged disjoint regions —
  deadcode's plugin-era comment update at the add-track menu, menu's rewritten menu region (MenuIds.h
  routing), crash's Restart region. Verified every line each of the three branches added is present
  (0 missing for all three).
- **`app/domain/src/document.hpp`** (deadcode + roll): deadcode's removals + roll's `setNoteVelocity`
  inline helper (BUG-SWEEP-30 envelope scaling). `<algorithm>` include present.

**Union proof (whole tree):** of the 138 changed files, 126 are single-owner (touched by exactly one
branch); all 126 are byte-identical to that branch's version (`git diff --quiet <branch> HEAD -- <file>`,
0 mismatches). The 12 multi-owner files are the hotspots above, each verified to contain every branch's
additions. No conflict marker remains anywhere in the tree.

## Diffstat vs ed4642e

**138 files changed, 10504 insertions(+), 769 deletions(-).**

| Area | Files | + | − |
| --- | --- | --- | --- |
| app/desktop | 10 | 285 | 89 |
| app/domain | 20 | 417 | 516 |
| app/other (control, engine-client, previews, selftest-plugins) | 7 | 137 | 13 |
| engine | 13 | 59 | 133 |
| tests | 9 | 2131 | 10 |
| ci | 2 | 64 | 2 |
| docs (plan + reports) | 21 | 3720 | 6 |
| docs/relatorios/evidencia | 55 | 3688 | 0 |
| other (.gitattributes) | 1 | 3 | 0 |

The net domain/engine deletion count is the dead-code L1/L2 removal (443 deletions in that branch);
coverage is guarded by the new `legacy_guard_tests` (9 cases / 169 checks).

## Gate (official, Windows, under GATE-LOCK)

GATE-LOCK coordination: on arrival the lock was held first by `flawless-crash` D-restart (07:11:32Z),
then `flawless-writesafety` (07:31:25Z), then `flawless-crash` D-restart full gate (07:35:46Z). Waited
and re-checked (~30 s poll); acquired the lock atomically at **07:37:37Z** once free; ran both modes;
released it afterwards (confirmed absent). The unique-bat fix (merged first) makes concurrent gates
collision-independent regardless of lock state.

Both modes **VERDE** on the clean committed tree (`git status --porcelain` = 0 lines):

| Mode | Command | Verdict | Tests | Time |
| --- | --- | --- | --- | --- |
| merge | `powershell -ExecutionPolicy Bypass -File ci/gate.ps1 -Build D:/Core-build/integration -Jobs 6` | VERDE | **20/20** | 361 s |
| desktop (full) | same + `-Desktop` | VERDE | **21/21** | 42 s |

- merge label = 17 baseline + `legacy_guard_tests` + `menu_tests` + `cli_package_stale` = **20** (all pass,
  including `cli_package_stale` which runs the real `core-package` target, and `cli_plugin_crash_render`).
- full set = 20 merge + 1 desktop = **21/21** ("100% tests passed out of 21").
- **Version == HEAD sha, no `.modified`:** `core-engine --version` = `0.7.0+29ec5e01e0`; self-test report
  `build=0.7.0+29ec5e01e0 engine=0.7.0+29ec5e01e0`; resumo `Codigo: ...integration @ 29ec5e0` with no
  dirty flag. HEAD = `29ec5e01e05aa1220085f4cfbea1c9181822dd94`.
- Desktop self-test: **ok, 52 steps**, `document=68 accepted=68 applied=68`, `loudest=0.735`, null device
  48000 Hz block 128 — identical to the crash branch's own clean gate run (52 steps / document=68 /
  loudest=0.735). The 3 extra steps over the 49-step baseline are item C's "restart offered / restarted
  after timeouts".

### Per-suite counts (ctest -L merge -V, measured-by-me, exit 0)

Baseline `ed4642e` = 145 cases / 1741 verifications (docs/relatorios/2026-11-18-estado-ctests.md).

| Suite | ed4642e | integrated | delta |
| --- | --- | --- | --- |
| engine_tests | 25 / 130 | 25 / 130 | 0 / 0 |
| domain_tests | 19 / 148 | 29 / 472 | +10 / +324 |
| legacy_guard_tests | — | 9 / 169 | new |
| role_tests | 21 / 216 | 21 / 216 | 0 / 0 |
| midi_tests | 8 / 115 | 12 / 194 | +4 / +79 |
| bug_tests | 22 / 138 | 36 / 326 | +14 / +188 |
| bitwig_tests | 2 / 25 | 2 / 25 | 0 / 0 |
| audit_tests | 7 / 65 | 7 / 65 | 0 / 0 |
| audit2_tests | 6 / 80 | 6 / 80 | 0 / 0 |
| hunt_tests | 14 / 649 | 14 / 649 | 0 / 0 |
| round3_tests | 7 / 82 | 7 / 82 | 0 / 0 |
| calls_tests | 10 / 44 | 12 / 69 | +2 / +25 |
| menu_tests | — | 7 / 366 | new |
| engine_process_tests | 4 / 49 | 9 / 104 | +5 / +55 |
| **Counting total** | **145 / 1741** | **196 / 2947** | **+51 / +1206** |

0 failures everywhere. The 6 `cli_*` tests print no case counts (by design). **Nothing shrunk:** every
counting suite is at or above its baseline; every branch's added tests are present:

- menu 7/366; roll 2/44 (midi); preservation 4/50 (bug 2/15 + midi 2/35); pattern 8/63 (bug);
  timeline 4/110 (bug); tempo 7/42 (domain); randomize 1/20 (domain); batch 4/287 (domain 2/262 +
  calls 2/25); deadcode legacy_guard 9/169; packaging cli_package_stale (+1 ctest test);
  crash engine_process +5/+55 (A +4/+44, B +1/+11).

## Sound check

The timeline fixes (BUG-SWEEP-22..25) intentionally change rendered tempo-timing audio when a tempo map
is present; their report documents the measured RMS dB deltas on an audio-clip probe bed (22: −0.1193 dB;
23: 0.0000 dB, bursts exchange places; 24: +0.0372 dB; 25: +0.8855 dB), and songs with an empty tempo map
are sound-neutral. The integrated render/null tests all pass: `cli_demo_render`, `cli_plugin_crash_render`,
`cli_measure`, the engine null-render cases in `engine_tests`/`hunt_tests`, and the desktop self-test's
audio steps ("kick-2 heard", "arrangement heard", "audio heard, split, trimmed", "midi heard", etc.).
Self-test `loudest=0.735` matches the crash branch's clean baseline. **No unexpected sound change** beyond
the documented, intentional timeline tempo-continuity deltas. (Core's built-in instruments left the engine
on 2026-11-16, so `demoProject` renders silence by design — ratified in `engine_tests`.)

## What is NOT included (running streams / follow-ups)

- **`flawless-writesafety` (BUG-SWEEP-16)** — still running; gated its own branch during this session
  (VERDE @ `3d5b2a2`), not part of this round.
- **`flawless-deadcode2` (preview / Rise)** — still running, not part of this round.
- **D Core wiring (`cc5b874`, FL item 1 D)** — landed on `flawless-crash` after the pinned `c61bcd1`;
  the restart-deadlock Core-side call to `markCrashedByHost` is a separately-landing follow-up. The
  vendored JUCE patch FILE (`ci/patches/juce-failed-plugin-restart.patch`) IS included (from `c61bcd1`).
- **SUR engine-improvements (SUR-01 etc.)** — the SUR stream's shas live in its own clones; not integrated
  here. `engine_tests` stays at the 25/130 baseline (SUR-01's +1/+30 is not in this round).

## Vendored JUCE (runtime condition)

`D:/Core-deps/JUCE-9.0.5` is **already patched** by the D stream: `markCrashedByHost` is present in
`modules/juce_audio_processors_headless/utilities/juce_AudioPluginExtensions.h` (default empty virtual)
and `.../format_types/juce_VST3PluginFormatImpl.h` (override). The integrated build compiles against this
patched JUCE; the default no-op keeps healthy hosting bit-identical. **This session did not modify
`D:/Core-deps`** (read-only verification only). The patch is tracked in-repo as
`ci/patches/juce-failed-plugin-restart.patch` with a re-application note for future JUCE updates.

## Verification (commands and exit codes)

| Command | Exit | Result |
| --- | --- | --- |
| `git merge --no-ff <branch>` × 14 (sequential, conflicts resolved) | 0 | 14 merge commits, all HEADs ancestors |
| single-owner union check (126 files, `git diff --quiet <branch> HEAD -- <file>`) | 0 | 0 mismatches |
| `powershell ... ci/gate.ps1 -Build D:/Core-build/integration -Jobs 6` | 0 | VERDE merge 20/20, 361 s |
| `powershell ... ci/gate.ps1 -Build D:/Core-build/integration -Jobs 6 -Desktop` | 0 | VERDE full 21/21, 42 s |
| `ctest --test-dir D:/Core-build/integration -L merge -V --output-on-failure` | 0 | 196 cases / 2947 verifs / 0 fails |
| `ctest --test-dir D:/Core-build/integration -L desktop -V --output-on-failure` | 0 | 1/1, self-test ok 52 steps |
| `ctest --test-dir D:/Core-build/integration --output-on-failure` | 0 | 21/21 |
| `core-engine --version` | 0 | 0.7.0+29ec5e01e0 (== HEAD, no .modified) |
| `git status --porcelain` (integration worktree) | 0 | 0 lines (clean) |

Evidence (working copies): `D:/Core-build/integration-evidence/` — `gate-resumo-merge.txt`,
`gate-resumo-desktop.txt`, `gate-merge-stdout.txt`, `gate-desktop-stdout.txt`, `merge-verbose.txt`,
`desktop-verbose.txt`, `full-set.txt`, `per-suite-counts.txt`, `self-test-report.txt`, `version.txt`,
`head-sha.txt`, `worktree-status.txt`.

## PROPOSED ESTADO.md lines (scribe only — D72/D80; not written here)

For "Estado medido" (citation + source per line):

1. `Consolidation round @ 29ec5e0 (integration/flawless-round, Windows MSVC Release): 14 fix branches
   merged (ci/gate-unique-bat cbfab61, docs/maps-en 7bed196, flawless-estado 3a25a71, batch 15306b0,
   packaging 20e3cb8, preservation 6888473, deadcode 3830db6, tempo 5019873, timeline 753fcfd,
   randomize 0f83221, pattern 41c38da, roll 7b7c82c, menu 365a154, crash c61bcd1). Gate VERDE merge
   20/20 + full 21/21; 196 cases / 2947 verifications / 0 failures (baseline ed4642e 145/1741, +51/+1206,
   nothing shrunk); version 0.7.0+29ec5e01e0 == HEAD, no .modified; self-test ok 52 steps document=68
   loudest=0.735. Diffstat 138 files +10504/−769. Source: docs/relatorios/2026-11-18-consolidation-round.md
   + D:/Core-build/integration-evidence/.`
2. `Not in this round (follow-ups): flawless-writesafety (BUG-SWEEP-16), flawless-deadcode2 (preview/Rise),
   D Core wiring cc5b874 (FL item 1 D, landed on flawless-crash after the pinned c61bcd1), SUR-01.
   Vendored JUCE already patched by the D stream (markCrashedByHost present); patch file tracked at
   ci/patches/juce-failed-plugin-restart.patch; D:/Core-deps not modified by the consolidation.
   Source: docs/relatorios/2026-11-18-consolidation-round.md.`

Pendência (owner = Diogo): ledger scribe to transcribe lines 1–2; PM to verify and push
`integration/flawless-round` (this session did not push). The D Core wiring follow-up (`cc5b874`) and the
running writesafety16 / deadcode2 streams integrate in a later round.

## Changes

Documentation only: this report and one index row in `docs/relatorios/README.md` (committed in the
integration worktree). The 14 merge commits carry the product/test changes. No `ESTADO.md` edit, no push,
no `git stash`, no edit to the main checkout or to `D:/Core-deps`.
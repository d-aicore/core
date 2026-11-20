# S8 — undo cost measurement notes (agent s8-undo)

Wave: measurement-first, NO history.cpp change. Repo D:\CORE\CORE @ a9a038430d781959829f8510067ce59afffaad62 + uncommitted round-1 fixes (git status verified: engine/app/tests/docs locals present; none touched).

## Step 1 (in progress) — code map of the undo cost

Confirmed by reading (all paths app/domain/src):
- history.cpp:8 `sizeOf(d) = toJson(d).dump().size()` — FULL serialization (nlohmann json tree build + canonical dump to string) used ONLY for byte pricing.
- history.cpp:14-18 `push(stack, Document document, label)` — Document passed BY VALUE (whole-document copy: tracks, patterns, notes, plugin-state base64 strings, json extras), then sizeOf serializes that copy again, then push_back moves it into the deque Step.
- history.cpp:51 `commit`: `push(undo_, current_, label)` copies current_; before that `sameContent(next, current_)` (document.cpp — checking impl) and `cancelsLastStep` also call sameContent → likely more toJson comparisons.
- history.cpp:47/54/68/112 `current_ = next` — assignment copies the incoming Document again.
- history.cpp:62 `beginGesture`: `gestureStart_ = current_` — another whole-document copy.
- history.cpp:77 `endGesture`: pushes gestureStart_ (move, but sizeOf still serializes it).
- history.cpp:89-95 undo / 101-107 redo: pop step (move), `push(otherStack, std::move(current_), label)` — move avoids the copy BUT sizeOf STILL does toJson+dump on every undo/redo. current_ = std::move(step.document) is a move.
- history.cpp:117 markSaved: `saved_ = toJson(current_)` — full json tree kept in memory as the saved baseline.
- history.cpp:124 modified(): `toJson(current_) != saved_` — full serialization on first ask after each revision (window asks every frame; cached per revision).
- history.hpp:21-25 constants: kMinSteps=50, kFloorSteps=1, kHardMaxBytes=1<<30 (1 GB), default maxBytes=64 MB, hardMaxBytes ctor param for tests.
- history.cpp:22-29 trim(): while bytes_>maxBytes && undo>kMinSteps drop oldest; while bytes_>hardMaxBytes && undo>kFloorSteps drop oldest. NOTE: trim() is called in commit and redo but NOT in undo() — checking whether that is a defect (undo moves bytes undo→redo; total bytes_ unchanged, so probably fine; redo pushes current_ into undo_ then trims — ok).
- Pricing bytes_ counts ONLY undo_+redo_ step bytes (each step's serialized dump size); current_, gestureStart_, saved_ json NOT counted against the ceiling.

Per-operation cost map (preliminary, to be confirmed against document.cpp sameContent):
- commit: sameContent(next,current) [serialize? check] + cancelsLastStep sameContent [check] + copy current_ into push + toJson+dump for pricing + copy next into current_ + trim.
- undo/redo: 1× toJson+dump pricing (no copy, moves) + possible trim on redo.
- beginGesture: 1 whole-doc copy. endGesture: sameContent + toJson+dump pricing (move push).
- modified() first call per revision: 1 toJson (tree compare, no dump).

## Step 1 DONE — full cost map (confirmed in code)

- document.cpp:778 `sameContent(a,b) = toJson(a) == toJson(b)` — TWO full json-tree builds + tree compare per call.
- document.cpp:362-364 trackJson: `instrument["plugin"] = t.instrument.plugin` — the WHOLE plugin json incl. the base64 "state" string is copied into the json tree on every toJson. dump() then copies it again into a string. So one sizeOf = blob copied twice (tree + string) + all notes/clips/automation rebuilt.
- toJson builds: format/schema/name/transport/loop/master/tracks(each: instrument+plugin state, mix+sends, patterns+notes, clips, automation, effects)/sections/assets/ui/extras. Sorted keys → canonical dump.

Per-operation serialization/copy count (N = tracks, S = state bytes):
- commit (no cancel): sameContent(next,current) 2×toJson + [cancelsLastStep if same label: +2×toJson] + push: 1 Document COPY + sizeOf 1×toJson+1×dump + `current_=next` 1 Document COPY. → 3–5 toJson + 1 dump + 2 whole-doc copies.
- undo/redo: push(move(current_)) → sizeOf 1×toJson+1×dump; documents MOVE (no copy). redo also trim().
- beginGesture: gestureStart_=current_ (1 copy, no serialization). endGesture: sameContent 2×toJson (+2 if cancel-check) + sizeOf 1×toJson+dump (push by move).
- updateGesture/amend: current_=next copy, no serialization.
- modified(): toJson(current_) once per revision (tree != saved_ tree). markSaved/reset: toJson kept as saved_ json tree (persistent, NOT priced).

Constants (history.hpp:21-25): kMinSteps=50, kFloorSteps=1, kHardMaxBytes=1 GB, default maxBytes=64 MB; ctor takes maxBytes/hardMaxBytes (tests use small ceilings). trim() (history.cpp:22-29) drops oldest undo_ only.

DEFECT CANDIDATES to test with probe (record red, no fix):
- D-a: trim() only pops undo_. undo() pushes into redo_ WITHOUT any ceiling check; after k undos, redo_ holds k full documents and bytes_ can exceed hardMaxBytes with undo_ too small to trim → 1 GB ceiling not enforced on the redo stack. Test: small hardMaxBytes ctor + many undos, observe bytes_/stack sizes.
- D-b: pricing counts dump() bytes only; current_, gestureStart_ and the saved_ json tree are unpriced memory (saved_ nlohmann tree is several× the dump size). Actual RSS > budget. Measure heap delta vs bytes_.
- D-c (accounting check): undo() subtracts step.bytes then push() re-serializes current_ — bytes_ stays ~constant across undo; verify no drift.

Build env: VS 18 BuildTools at C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools (vswhere). tests/CMakeLists guards app-only tests with if(TARGET) → APP=OFF safe. core_domain = static lib, C++20, includes app/domain/src + engine/third_party (nlohmann json.hpp lives there).

## Step 2 — own build D:\Core-build\s8\build (Release, Ninja, TESTS=ON, APP=OFF)

DONE: cmake configure MSVC 19.51.36260 (VS 18 BuildTools), Ninja, Release, CORE_BUILD_APP=OFF CORE_BUILD_TESTS=ON; `cmake --build --parallel 6 --target core_domain` → 28/28 ok, core_domain.lib linked. vcvars64 + VSLANG=1033 + chcp 65001 used. (Full test targets not built — probe needs only core_domain; TESTS=ON configured as mandated.)

## Step 3 — probe D:\Core-build\s8\probe\probe.cpp

Written + compiled clean (cl /std:c++20 /O2 /MT, links core_domain.lib + psapi). Fixture matches tests/engine_process_tests.cpp:721 stateSong: state string EXACTLY 318128 chars, first char 'B'+i%20; deterministic LCG base64-alphabet fill. Docs: N tracks × (1 pattern 64 notes, 2 clips, gainDb 32-pt + vst 16-pt automation). Configs: 4/16/32/64 tracks × {no state, 318128} + 32×65536 + 32×1048576 (blob-size sweep). 3 reps each.
Measures: QPC per commit (60, constant label → cancel-check active), undo/redo traversals with sameContent integrity checkpoints vs deterministic replay (u/r = 15/30/45/60) + blob-intact check; components toJson / toJson+dump / Document copy / sameContent (5 iters); gesture begin/update×10/end; modified() first-vs-cached; PrivateUsage deltas + PeakWorkingSet; forEachState counts; small-ceiling ctor (cap=10×price, 12 commits + full undo → redo-stack conservation check) + floor exception (cap=price/2). Counterfactual commit = 2×copy (no serialization).
RAM 31.1 GB → 1 GB-ceiling configs safe.

## Step 4 — run 1 results (complete) + probe bug found & fixed

Run 1 (results-run1-probebug.csv): all timing/memory metrics valid. Headlines (mean of 3):
- commit: t32_s0 62.6 ms; t64_s0 130.0 ms; t16_s318128 73.3 ms; t32_s318128 155.2 ms; t64_s318128 331.4 ms; t32_s1048576 451.0 ms (max 955.7).
- undo/redo ≈ toJson+dump: t32_s318128 80.4/79.4 ms; t64_s318128 164.9/163.4; t32_s1048576 273.4/249.8. Moves are free; ALL of it is sizeOf pricing.
- components t32_s318128: toJson 15.75 ms, toJson+dump 76.62 ms, Document copy 1.70 ms, sameContent 32.08 ms. Commit ≈ 5×toJson + 1×dump + 2 copies = 78.8+76.6+3.4 ≈ 155 ms ✓ (sameContent×2 = 4 toJson incl. cancel-check; sizeOf = 1 toJson + 1 dump).
- serial_share_of_commit ≈ 0.97–0.99 everywhere. pred_commit_no_serial (2 copies) = 3.39 ms @32×318KB vs 155 ms → ~46× headroom before structural sharing.
- Scaling: node-count driven for note-heavy docs (t64_s0 toJson 24.6 ms for 376 KB), byte driven for blobs (dump scans byte-by-byte ~160 MB/s: +64 ms for +10.4 MB @32 tracks). toJson tree copy of blobs ~3 GB/s (cheap); dump is the expensive blob pass.
- Memory: states_after_commits 61 (no-blob, all 60 kept) / 51–52 (blob, soft 64 MiB + kMinSteps 50) / 32 (t32_s1048576, HARD 1 GB trim: 31 steps × 33.74 MB = 1046 MB ≤ 1073.7 MB; 32 would exceed). Budget math checks out exactly (64 MiB = 67.11 MB decimal; 51 × 1.296 MB = 66.1 MB fits @t4).
- private_delta_commits: t64_s318128 1150 MB vs priced stack 1037 MB → unpriced gap 161 MB (current_ + saved_ json tree + transients). Peak working set 1731 MB process-wide vs 1 GB nominal ceiling → ceiling prices ONLY step dumps; actual memory ~1.7× ceiling.
- modified() first ask per revision = full toJson: 12–42 ms; cached ask ~0. During gestures revision bumps per update → per-frame serialization if the window asks (dirty-flag follow-up "08" of state-per-edit).
- ceiling_respected = 1 in ALL configs (small-ceiling ctor, 12 commits + full undo): undo→redo conserves bytes_; trim on commit/redo suffices. undo() not trimming is NOT a defect. Floor exception (cap < 1 step keeps 1 undo) confirmed by design.
- PROBE BUG (not repo): redo integrity checkpoints assumed 60 undos available; trim keeps U=50/31 → after full undo current=expected(60-U), redo r → expected(60-U+r). Run 1 "3 failures" = exactly the 3 redo checkpoints; undo checkpoints + no-blob configs all green. FIXED (trim-aware + full-undo/full-redo landing checks). Run 2 = job pwsh-c47a2f0e524343b3 (executor backgrounded at 600 s; job id sent to lead).

## Step 5 — characterization + design proposal (drafting while run 2 completes)

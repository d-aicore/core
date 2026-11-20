# S11 fix notes — idempotent D113 patch application in CMake (coordinator round 2)

Agent: s11-fix. Repo D:\CORE\CORE @ a9a038430d781959829f8510067ce59afffaad62.
Write scope: CMakeLists.txt ONLY (repo); everything else under D:\Core-build\s11-* and this file.
The uncommitted ENV-01 sanitizer-wiring hunk (CORE_SANITIZED_TARGETS: legacy_guard_tests/menu_tests/
modules_tests/window_tests) is preserved untouched — the fix is a separate logical hunk.

## Step 1 — Survey (2026-11-20 ~00:26 UTC)

Repo state:
- HEAD = a9a038430d781959829f8510067ce59afffaad62 (matches mandate).
- Uncommitted: CMakeLists.txt (ENV-01 hunk only, +13 lines at the CORE_SANITIZED_TARGETS list),
  docs/relatorios/README.md (+8, other agent's work — untouched), untracked reports/environment.yml.
- Patch ci/patches/juce-failed-plugin-restart.patch: 2 files, 14 hunks
  (juce_AudioPluginExtensions.h ×1; juce_VST3PluginFormatImpl.h ×13), all under
  modules/juce_audio_processors_headless/. Adds VST3Client::markCrashedByHost + crash-aware locks.

Measured RED baseline (cited, not redone):
- coordinator-round2/s11-clean-build-notes.md: clean fetch configure rc=0, JUCE 9.0.5 downloaded,
  URL_HASH SHA256=d92207f5...2315a verified MATCH, extracted to D:\Core-build\s11-clean\deps\juce-src.
- coordinator-round2/s11-negctrl-notes.md: BOTH unpatched copies fail identically —
  juce_host.cpp(136) error C2039 'markCrashedByHost' not a member of
  juce::AudioPluginExtensions::VST3Client, build rc=2 at [19/555] (core_plugin_host).
  Clean-fetch build log: D:\Core-build\s11-clean\ (configure.txt); local-unpatched build log:
  D:\Core-build\s11-local-unpatched-build.txt.

Environment facts (measured this step):
- git 2.55.0.windows.5 on PATH; cmake + ninja on PATH (WinGet). Toolchain per baseline caches:
  MSVC cl 14.51.36231 (VS18 BuildTools), Ninja generator.
- vcvars64.bat present: C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat
- JUCE copies (none is a git repo — tarball extracts; `git apply` works outside a repo, proven by
  the negctrl step 2 which ran git apply -R in the copy):
  - D:\Core-deps\JUCE-9.0.5 — PATCHED (markCrashedByHost = 1 match in juce_AudioPluginExtensions.h),
    project(JUCE VERSION 9.0.5). User deps dir — must NOT be modified.
  - D:\Core-build\s11-juce-unpatched — UNPATCHED (0 matches), 9.0.5.
  - D:\Core-build\s11-clean\deps\juce-src — UNPATCHED pristine download (0 matches), 9.0.5.
- Baseline configure flags recovered from CMakeCache.txt:
  - Case 1 dir D:\Core-build\s11-clean\build: Debug, Ninja, CORE_BUILD_TESTS=ON, CORE_BUILD_APP=ON,
    FETCHCONTENT_BASE_DIR=D:/Core-build/s11-clean/deps, FETCHCONTENT_SOURCE_DIR_JUCE empty.
  - Case 2 dir D:\Core-build\s11-local-unpatched: Debug, Ninja, CORE_BUILD_TESTS=ON,
    FETCHCONTENT_BASE_DIR=D:/Core-build/s11-local-unpatched/_deps,
    FETCHCONTENT_SOURCE_DIR_JUCE=D:/Core-build/s11-juce-unpatched.
- D:\Core-build\s11-release-proof exists (empty) — target dir for case-4 behaviour logs.

Plan: implement hunk after FetchContent_MakeAvailable(JUCE) (CMakeLists.txt:67) inside the
CORE_BUILD_APP AND WIN32 block; then proof cases 1–5 per mandate.

## Step 2 — Implementation (2026-11-20 ~00:29 UTC)

Edited D:\CORE\CORE\CMakeLists.txt ONLY. New logical hunk inserted immediately after
FetchContent_MakeAvailable(JUCE), inside the existing if(CORE_BUILD_APP AND WIN32) block
(old lines 60–68; new hunk spans CMakeLists.txt:69–124). Logic:
1. find_program(GIT_EXECUTABLE git); absent → FATAL_ERROR (clear, names the patch).
2. CORE_JUCE_PATCH = ${CMAKE_CURRENT_SOURCE_DIR}/ci/patches/juce-failed-plugin-restart.patch;
   target tree = ${juce_SOURCE_DIR} (FetchContent variable; equals the local copy when
   FETCHCONTENT_SOURCE_DIR_JUCE is set).
3. Full-patch proof: execute_process(git apply --check -R <patch>, WORKING_DIRECTORY juce_SOURCE_DIR,
   RESULT_VARIABLE, OUTPUT_QUIET ERROR_QUIET). rc==0 ⇔ every one of the 14 hunks present.
4. rc==0 → STATUS "already carries the full crash/restart patch" (idempotent; fetched or local).
5. rc!=0 AND FETCHCONTENT_SOURCE_DIR_JUCE set → FATAL_ERROR naming patch path, JUCE dir, rc, and
   that a local copy must already carry the full D113 patch; explicitly states the local copy was
   NOT modified (user deps dir never touched).
6. rc!=0 AND fetched copy → git apply <patch>; failure → FATAL_ERROR (incompatible JUCE version,
   expected pinned hash-verified 9.0.5); success → re-run reverse --check to PROVE full application
   (failure → FATAL_ERROR, false-green refusal), then STATUS "Applied D113 crash patch to fetched
   JUCE at ... (full application verified)".
Comments in English reference D113 and the false-green risk (configure-green/late-C2039, or green
build with the crash fix absent if the host target is skipped).

Diff verification (git diff -- CMakeLists.txt):
- 2 separate hunks: @@ -65,6 +65,62 @@ (D113, +56 lines) and @@ -94,6 +150,19 @@ (ENV-01, +13).
- ENV-01 sanitizer hunk preserved verbatim: '+    legacy_guard_tests menu_tests' and
  '+    modules_tests window_tests' still present; total +69 = 56 + 13, no deletions.
- No other repo file touched (docs/relatorios/README.md and untracked files left as found).

## Step 3 — Case 1: clean-fresh-deps re-configure + previously failing target (GREEN)

Core SHA a9a0384 + uncommitted D113 hunk (ENV-01 hunk intact). JUCE source: downloaded pristine
9.0.5 at D:/Core-build/s11-clean/deps/juce-src (hash-verified per baseline notes; no .git).
- Bat: D:\Core-build\s11-clean\configure-fixed.bat (VSLANG=1033, chcp 65001, vcvars64 VS18 BuildTools;
  cmake -S D:\CORE\CORE -B D:\Core-build\s11-clean\build -G Ninja -DCMAKE_BUILD_TYPE=Debug
  -DCORE_BUILD_TESTS=ON -DFETCHCONTENT_BASE_DIR=D:/Core-build/s11-clean/deps; NO local source dir).
- Configure rc=0. Log: D:\Core-build\s11-clean\configure-fixed.txt. Key line:
  "-- Applied D113 crash patch to fetched JUCE at D:/Core-build/s11-clean/deps/juce-src
  (full application verified: git apply --check -R)". Configuring done 1.2s (deps already present).
- Independent verification AFTER configure: git apply --check -R in juce-src → rc=0 (all 14 hunks
  present); tree-wide grep markCrashedByHost (*.h,*.cpp) = **2 matches**, exactly:
  juce_AudioPluginExtensions.h:99 (virtual no-op) + juce_VST3PluginFormatImpl.h:2260 (override).
- Build: cmake --build ... --target core_plugin_host --parallel 6 → **rc=0** (background job
  pwsh-75b9d62295914619). Log: D:\Core-build\s11-clean\build-core_plugin_host-fixed.txt.
  core_plugin_host is a STATIC library (engine/CMakeLists.txt:46, sole source juce_host.cpp).
  [17/35] juce_host.cpp.obj COMPILED (the exact object that failed C2039 in the red baseline at
  [19/555]); only a pre-existing unrelated C4996 deprecation warning at juce_host.cpp(523).
  [34/35] Linking CXX static library engine\core_plugin_host.lib → present, 265,017,766 bytes,
  2026-11-20 00:31:14. 35 ninja steps, no errors. RED→GREEN on the same dir/deps/flags.

## Step 4 — Case 2: NEGATIVE local unpatched copy (FAIL EARLY, as required)

JUCE source: D:/Core-build/s11-juce-unpatched (9.0.5, reverse-applied copy, 0 patch markers).
- Bat: D:\Core-build\s11-local-unpatched\configure-fixed.bat (same flags as its red-baseline cache:
  Debug, Ninja, TESTS=ON, FETCHCONTENT_BASE_DIR=D:/Core-build/s11-local-unpatched/_deps,
  FETCHCONTENT_SOURCE_DIR_JUCE=D:/Core-build/s11-juce-unpatched).
- Configure **rc=1** — failed at CONFIGURE time (CMakeLists.txt:92), before any build.
  Log: D:\Core-build\s11-local-unpatched\configure-fixed.txt. Verbatim message:
    CMake Error at CMakeLists.txt:92 (message):
      D113: the local JUCE copy 'D:/Core-build/s11-juce-unpatched'
      (FETCHCONTENT_SOURCE_DIR_JUCE) does NOT carry the full crash/restart patch
      'D:/CORE/CORE/ci/patches/juce-failed-plugin-restart.patch' (git apply
      --check -R rc=1).  A local copy must already have the complete D113 patch
      applied (14 hunks in 2 files under
      modules/juce_audio_processors_headless/); an unpatched or incompatible JUCE
      version otherwise fails the build later with C2039 'markCrashedByHost'.
      Apply the full patch to the local copy yourself, or unset
      FETCHCONTENT_SOURCE_DIR_JUCE so Core downloads and patches JUCE 9.0.5.  The
      local copy was NOT modified.
    -- Configuring incomplete, errors occurred!
- Copy untouched proof (after the failed configure): reverse-check rc=1 (git: "patch does not
  apply" for both files), grep markCrashedByHost in its juce_audio_processors_headless = 0.

## Step 5 — Case 3: LOCAL PATCHED copy (configure pass; build running)

JUCE source: D:/Core-deps/JUCE-9.0.5 (user deps, patched, 9.0.5, no .git). Fresh dir
D:\Core-build\s11-local-patched. Bat: configure.bat (Debug, Ninja, TESTS=ON,
FETCHCONTENT_BASE_DIR=D:/Core-build/s11-local-patched/_deps,
FETCHCONTENT_SOURCE_DIR_JUCE=D:/Core-deps/JUCE-9.0.5).
- Configure rc=0 (22.7s). Log: D:\Core-build\s11-local-patched\configure.txt. Key line:
  "-- D113: JUCE at D:/Core-deps/JUCE-9.0.5 already carries the full crash/restart patch
  (verified: git apply --check -R)" — verification-only branch, no git apply executed.
- User deps untouched proof: reverse-check in D:/Core-deps/JUCE-9.0.5 after configure → rc=0
  (still fully patched); code path for a local copy never runs git apply (only --check -R).
- Build --target core_plugin_host --parallel 6: **rc=0** (job pwsh-528b9151ae6a4865). Log:
  D:\Core-build\s11-local-patched\build-core_plugin_host.txt. [19/53] juce_host.cpp.obj compiled
  against D:\Core-deps\JUCE-9.0.5 modules; [52/53] Linking core_plugin_host.lib → present,
  264,887,728 bytes, 2026-11-20 00:33:25. Only pre-existing unrelated C4996 warnings (wav.cpp
  strerror, juce_host.cpp createEditorIfNeeded). GREEN.

## Step 6 — Case 5: idempotence (re-configure of case-1 dir)

Re-ran D:\Core-build\s11-clean\configure-fixed.bat (identical flags) → **rc=0**.
Log: D:\Core-build\s11-clean\configure-idempotent.txt. Only D113 line:
  "-- D113: JUCE at D:/Core-build/s11-clean/deps/juce-src already carries the full crash/restart
  patch (verified: git apply --check -R)"
— the reverse --check hit FIRST; NO "Applied D113" line, i.e. git apply was not re-run.
Patch state after: reverse-check rc=0, grep markCrashedByHost = 2 (no duplicate application).

## Case 4 configure (verified) — full build running

- Configure rc=0 (job pwsh-f46d0c508f90471f; Configuring done 38.7s incl. download+juceaide).
  Log: D:\Core-build\s11-release-fixed\configure.txt. Key line:
  "-- Applied D113 crash patch to fetched JUCE at D:/Core-build/s11-release-fixed/deps/juce-src
  (full application verified: git apply --check -R)".
- Fresh pristine download proven: deps\juce-subbuild\...\juce-populate-urlinfo.txt →
  url=https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.5.tar.gz,
  hash=SHA256=d92207f51ba728d3d195b6d5e93ecd049717a915fc250ff6844c32ea7272315a (== pinned;
  FetchContent verify step passed, else configure would have failed). NO local source dir.
- Independent verification: reverse-check rc=0 in deps/juce-src; grep markCrashedByHost = 2;
  project(JUCE VERSION 9.0.5).
- FULL build: cmake --build D:\Core-build\s11-release-fixed\build --parallel 6, background job
  pwsh-e559fa0e1b2a4886, log D:\Core-build\s11-release-fixed\build-full.txt (pending).
  Next: behaviour proofs (engine_process_tests "restart the component" ×3, "crashes in
  processBlock" ×3, ctest -R cli_plugin_crash_render -V) under D:\Core-build\s11-release-proof\.

## Case 4 COMPLETE (coordinator lead ran the behaviour proofs after the agent ended; Release build resumed and finished by lead, job pwsh-feacc4c7454c4020, rc=0, 495 targets incremental)
- Full Release build: D:\Core-build\s11-release-fixed\build, rc=0 (fresh downloaded JUCE 9.0.5, hash d92207f5... verified, D113 patch applied+verified at configure).
- engine_process_tests.exe "restart the component" x3: EXIT 0/0/0; run1: ok, 1 testes / 17 verificacoes / 0 falhas (D113 restart-deadlock regression).
- engine_process_tests.exe "crashes in processBlock" x3: EXIT 0/0/0; run1: ok, 1 testes / 25 verificacoes / 0 falhas.
- ctest -R cli_plugin_crash_render -V: 1/1 Passed (0.14 s), exit 0.
- Logs: D:\Core-build\s11-release-proof\{restart-run1..3,crash-run1..3,cli-crash-render,proof-summary}.txt.
- Core SHA a9a038430d781959829f8510067ce59afffaad62 + local CMake hunks (D113 +56, ENV-01 +13); JUCE 9.0.5 fetched (commit d60b7a78 per pin); patch state: applied+verified per case.
S11 ACCEPTANCE: cases 1,2,3,4,5 ALL GREEN as mandated.

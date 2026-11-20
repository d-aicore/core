# S5 fix — implementation log (agent s5-fix, DATA priority)

Repo: D:\CORE\CORE @ a9a038430d781959829f8510067ce59afffaad62 (main). Pre-existing uncommitted changes
(CMakeLists.txt sanitizer wiring, docs/relatorios) preserved untouched. Write scope: app/installer/Setup.cpp,
tests/installer_samples.cmake (new), tests/CMakeLists.txt (registration only). Build: D:\Core-build\s5-fix\build.
Red method/evidence read first: D:\Core-build\coordinator-round2\s5-notes.md (s5-red, logs 00–17).

## Step 1 — context + design (2026-11-20 ~00:30 Europe/Lisbon)

Read: s5-notes.md (full), app/installer/Setup.cpp (398 lines), app/installer/CMakeLists.txt,
tests/cli_package_stale.cmake, tests/CMakeLists.txt, ci/gate.ps1, root CMakeLists.txt, app/domain/src/sha256.{hpp,cpp}.

SHA-256 availability: CoreSetup links only CoreSetupData + juce::juce_gui_basics (app/installer/CMakeLists.txt:32).
juce_cryptography (juce::SHA256) is NOT in that link set (no dependency from juce_gui_basics). The repo's proven
self-contained SHA-256 lives in core_domain (app/domain/src/sha256.cpp, tested by domain_tests) but linking
core_domain would need an installer CMake change — outside write scope. DECISION: mirror that same FIPS 180-4
implementation inside Setup.cpp's anonymous namespace (self-contained, no new deps, no CMake change), hashing
files via juce::FileInputStream in 1 MiB chunks.

Design (hash manifest, backward-compatible):
- samples.txt lines: `<relpath>\t<sha256-of-content-Core-installed>`; plain-path lines stay readable as LEGACY.
- Copy decision per distributed file:
  * dest absent -> copy, record rel+source hash.
  * dest present, hashed entry H: hash(dest)==H -> Core's own copy (H==srcHash: skip copy, keep entry;
    H!=srcHash: legitimate update, copy, record srcHash). hash(dest)!=H -> user-modified: SKIP copy, DROP
    entry from manifest, stdout message naming the file.
  * dest present, LEGACY entry: hash(dest)==distributed source hash -> Core's copy, upgrade to hashed entry,
    no copy. Differs/unreadable -> cannot prove ownership -> SKIP, DROP, message (conservative; documented:
    a legacy entry whose content differs from the current source is never overwritten — after one install
    with the new installer every owned entry carries a hash and updates work fully).
  * dest present, NEVER listed -> never copy, never list (no hash needed). Preserves Audit-27 (case v:
    exact size+date match stays unlisted and survives) AND fixes fresh-target user-owned files (case ii).
    Rationale: without manifest evidence an existing file is user-owned; data > compatibility.
- Uninstall (removeSamples gains the source-library parameter; uninstall CLI accepts optional --samples,
  GUI passes samplesSource({})): hashed entry -> delete ONLY if hash(current)==recorded; legacy entry ->
  delete ONLY if source lib available AND hash(current)==hash(source file); source unavailable/unreadable
  -> do NOT delete (conservative, documented). Preserved files are dropped from the manifest; manifest
  deleted when empty and all deletions succeeded (failure keeps remaining entries for retry).
- Intact distributed sample (iii): hash matches recorded==source -> stays listed, removed at uninstall. No regression.
- Messages: English stdout lines "Kept your file ..." naming the preserved path; install/uninstall still exit 0.
- Cost note: reinstall now hashes listed files (one sequential read each) instead of stat-only size+date;
  required for correctness (case iv hides user content behind identical size+date). Never-listed fast path kept.

Test plan (tests/installer_samples.cmake, ctest -P script modelled on cli_package_stale.cmake): real
Core Setup.exe silent flow, temp targets under the build tree, synthetic fixture WAVs (same sizes as s5-red:
kick 4096, snare 5120, bass 6144, keys 4608; fixed source mtime 2026-11-01T10:00:00 via PowerShell).
Targets: main (cases i,ii,iii,iv: install->mutate->reinstall->assert->uninstall->assert), fresh (v,vi),
evidence (vii + LEGACY uninstall revalidation: manifest rewritten to plain lines before uninstall),
revalidate (hashed-entry revalidation at removal: intact reinstall, then edit bass, uninstall must keep it),
legacy-nosrc (legacy manifest + unavailable source at uninstall -> all preserved, conservative rule).
Expected hashes computed in-script with string(SHA256) of the exact fixture bytes; file(SHA256) for assertions.
Red on unfixed: i, ii (overwrite), iv, vii (manifest deletion), revalidate, legacy-nosrc must FAIL;
iii, v, vi must PASS. Registration: if(WIN32 AND CORE_BUILD_APP AND TARGET CoreSetup), LABELS merge,
RUN_SERIAL TRUE, TIMEOUT 300, exe via $<TARGET_FILE:CoreSetup>.

Next: write the test + registration, then configure/build the UNFIXED installer (background) for the red run.

## Step 2 — regression test written (red-first, permanent)

- NEW tests/installer_samples.cmake (~330 lines): ctest -P script modelled on cli_package_stale.cmake.
  Drives the real Core Setup.exe silent flow; 5 temp targets under build/tests/installer-samples:
  target-main (i,ii,iii,iv), target-fresh (v,vi), target-evidence (vii + legacy plain-line manifest
  re-validated against the source at uninstall), target-revalidate (hashed entry re-hashed at removal:
  intact reinstall then user edits bass -> uninstall must keep it), target-legacy-nosrc (legacy manifest
  + unavailable source at uninstall -> nothing deleted, conservative rule). Fixtures: same sizes as
  s5-red (4096/5120/6144/4608), deterministic ASCII content, expected hashes via string(SHA256) of the
  exact written bytes, file(SHA256) assertions; source mtime fixed 2026-11-01T10:00:00 via PowerShell;
  _core-plan skip check; manifest rel-set + hashed-entry format checks; "Kept your file" stdout message
  checks naming each preserved path. Failures accumulate (one run reports every case); FATAL_ERROR at
  the end lists them all. Expected red on unfixed: i, ii (overwrite), iv, vii, revalidate, legacy-nosrc
  (manifest deletion) + hashed-format checks; green on unfixed: iii, v, vi.
- tests/CMakeLists.txt: registration ONLY, appended right after the cli_package_stale block:
  if(WIN32 AND CORE_BUILD_APP AND TARGET CoreSetup) -> add_test(installer_samples ... -DSETUP=$<TARGET_FILE:CoreSetup>
  -DWORK=${CMAKE_CURRENT_BINARY_DIR}/installer-samples -P installer_samples.cmake), LABELS merge,
  TIMEOUT 300, RUN_SERIAL TRUE. No other entry touched.
- Setup.cpp still UNFIXED at this point (red-first order).

## Step 3 — configure + build UNFIXED installer (background)

Build dir D:\Core-build\s5-fix\build, Debug, APP=ON, TESTS=ON, FETCHCONTENT_SOURCE_DIR_JUCE=D:/Core-deps/JUCE-9.0.5,
gate conventions (vcvars64 VS18 BuildTools, VSLANG=1033, chcp 65001, Ninja), targets CoreSetup CoreSetupData,
--parallel 6. Log: D:\Core-build\s5-fix\build-unfixed.log. CoreSetup pulls CoreApp+core-engine (package zip),
so this is a full app Debug build — background job, collect on notification.
Job: pwsh-620b828f14ae4d97 (started ~00:37). While it runs (no repo edits until red is measured):
- Fixed a test bug before it could bite: expect_kept_message now normalizes backslashes in the installer
  stdout (juce prints Windows paths; the needles use CMake forward slashes).
- Drafted the full fix OUTSIDE the repo: D:\Core-build\s5-fix\Setup-fix-draft.md + Setup-fix-section.cpp.txt
  (SHA-256 mirror of core_domain's proven implementation; SampleEntry parsing; copy/uninstall decision
  tables as in step 1; removeSamples gains a source param with a self-comparison guard — Uninstall.exe
  running from the target has the target's own Samples beside it, which would "prove" a file against
  itself, so that source counts as unavailable). Apply only AFTER the red run.
- Prepared the second-confirmation manual matrix tool: D:\Core-build\s5-fix\manual-matrix.ps1 (same
  s5-red method: 3 targets, hash snapshots 00–16, identical fixture byte patterns to the cmake test).

## Step 4 — RED measured on the UNFIXED binary (2026-11-20 ~00:50)

Build completed by lead (357/357, exit 0; log D:\Core-build\s5-fix\build-unfixed.log).
Command: ctest --test-dir D:\Core-build\s5-fix\build -R installer_samples -V --output-on-failure --no-tests=error
Full output: D:\Core-build\s5-fix\red-run.txt. Result: ***Failed, 35 of 70 checks FAILED, ctest exit 8.
NOT vacuous — the data-loss cases fail exactly as s5-red measured:
- (i) edited kick OVERWRITTEN: after reinstall sha256=a91a2031… (distributed content) instead of the
  user edit 9cd4d1e9…; then DELETED at uninstall ("file is gone").
- (ii) user-owned snare OVERWRITTEN: 2b3b64a2… (distributed) instead of user 7d592e6d…; DELETED at uninstall.
- (iv) user-owned keys (same size+date, listed): copy skipped (hash kept) but STAYS LISTED -> DELETED at uninstall.
- (vii) listed user-content keys DELETED at uninstall while the non-listed user-take survives byte-identical.
- revalidate: bass edited after an intact reinstall -> DELETED at uninstall (no re-validation).
- legacy-nosrc: all 4 listed samples DELETED with no source available (no conservative rule).
- manifest format: plain lines, no sha256 entries (4+1+1 format failures).
Controls PASS on unfixed (must not regress): (iii) intact bass unchanged/listed/removed; (v) never-listed
exact size+date snare unlisted and survives; (vi) non-distributed user-take untouched and survives.
Fixture self-checks PASS (string(SHA256) == file(SHA256) of the written bytes).
Note: fixture hashes differ from s5-red's (different deterministic content); internally consistent here.
Cosmetic: CMP0219 warning from a '\t' in a fail() message — reworded to "tab+sha256" (no assertion change).
Also fixed a counting artifact: ';' inside fail() details split list elements in macro expansion
(35 reported vs 29 real) — reworded to '-- got:' and comma-joined listed_rels. Assertions unchanged.
FINAL RED (final script text, unfixed binary): D:\Core-build\s5-fix\red-run-final.txt (UTF-8),
ctest exit 8, "installer_samples: 29 of 70 checks FAILED", 41 PASS / 29 FAIL lines.
Per case: (i) FAIL overwrite+delete, (ii) FAIL overwrite+delete, (iii) PASS (control), (iv) FAIL
stays-listed+delete, (v) PASS (control), (vi) PASS (control), (vii) FAIL delete (non-listed take
survives), revalidate FAIL delete, legacy-nosrc FAIL all-4-delete, 6x manifest-format FAIL (plain lines).

## Step 5 — fix applied to app/installer/Setup.cpp (only repo code file touched)

As drafted (Setup-fix-section.cpp.txt): self-contained Sha256 (mirror of core_domain's proven FIPS 180-4
implementation; juce_cryptography/core_domain not in the installer link set, no CMake change needed),
sha256OfFile (1 MiB chunks, empty on read failure), SampleEntry manifest parsing ('<rel>\t<hash>',
plain line = legacy). copySamples: dest absent -> copy+record hash; listed+hashed -> hash(dest)==recorded
? (==srcHash: keep, no copy; !=srcHash: legitimate update, copy+record) : user content -> skip+drop+message;
listed+legacy -> hash(dest)==hash(source) ? keep+upgrade to hashed : skip+drop+message; never-listed ->
never copy/claim (Audit-27 fast path, no hashing). removeSamples(folder, source): delete only when content
proves ownership (hashed: current==recorded; legacy: current==source-file hash AND source available;
source==target\Samples guarded as unavailable — no circular self-proof); unproven -> keep+drop+message;
failed deletion -> entry kept for retry; manifest deleted when empty+ok. uninstall gains samplesLib param
(GUI: samplesSource({}); CLI: optional --samples on uninstall, backward compatible). English stdout
messages name every preserved file; exit codes unchanged (0 success).
Line refs (fixed file): Sha256 :74, sha256OfFile :132, SampleEntry :148, readSampleEntries :152,
copySamples :169 (decision :202, kept-message :213), removeSamples :233 (haveSource guard :237,
kept-message :257), uninstall :376, GUI call :441, CLI call :530. Header docs :4, :11-13.

## Step 6 — rebuild + GREEN (fixed binary, same build dir)

cmake --build D:\Core-build\s5-fix\build --target CoreSetup CoreSetupData --parallel 6 -> exit 0
(Setup.cpp.obj + link only; log D:\Core-build\s5-fix\build-fixed.log, BUILD-FIXED-OK).
ctest --test-dir D:\Core-build\s5-fix\build -R installer_samples -V -> PASSED, exit 0,
"installer_samples: all 70 checks passed (7-case matrix + legacy/revalidation sub-cases)".
Evidence: D:\Core-build\s5-fix\green-run.txt. Re-ran once more for stability: 100% passed, 4.53 s, exit 0.
Per case (all PASS): (i) edit kept on reinstall AND uninstall, dropped from manifest, message names it;
(ii) user snare kept both, dropped, message; (iii) intact bass unchanged, stays listed with installed hash,
removed at uninstall; (iv) user keys kept both, dropped, message; (v) never-listed exact-match snare
unlisted + survives; (vi) non-distributed take untouched + survives; (vii) listed user keys survive
uninstall beside surviving non-listed take, legacy intact bass/kick/snare removed; revalidate: bass edited
after intact reinstall survives uninstall (re-hashed), others removed; legacy-nosrc: all 4 preserved with
no source available, manifest dropped. 70/70 checks, 0 failures.

## Step 7 — second confirmation: original s5-red manual matrix method, fixed binary

D:\Core-build\s5-fix\manual-matrix.ps1 (same 3-target method, PowerShell snapshots; $Args automatic-variable
clash fixed -> $SetupArgs). Log: D:\Core-build\s5-fix\manual-matrix\matrix-log.txt (375 lines, all 8
invocations exit 0). Hashes cross-match the cmake test exactly (same fixture bytes):
- 05 after reinstall: kick 4352 B 9cd4d1e9… (user edit), snare 3000 B 7d592e6d… (user), keys 4608 B
  b47b6017… (user, mtime 2026-11-01 restored), bass 6144 B 9eab42db… (distributed, intact).
  samples.txt = single hashed line "Bass/bass-a.wav\t9eab42db…". stdout names kick/snare/keys as kept.
- 07 after uninstall: kick/snare/keys survive byte-identical; bass gone; samples.txt gone; folder stays.
- 10/12 fresh: user snare 710877b7… never listed, survives; user-take 331a995a… untouched, survives;
  the 3 listed distributed samples removed.
- 16 evidence: listed user keys b47b6017… SURVIVES uninstall (message names it); non-listed take
  b996cb27… survives; bass/kick/snare removed. Per-file isolation of the manifest fix confirmed.

## Step 8 — registration + scope

ctest -N -L merge: Total Tests 22 = the 21 pre-existing merge tests (engine/domain/modules/role/midi/bug/
bitwig/audit/audit2/hunt/round3/calls/menu/window + cli_version/demo_render/measure/unicode/package_stale
+ engine_process_tests + cli_plugin_crash_render) + installer_samples (#20). Nothing removed or weakened;
tests/CMakeLists.txt diff is +12 lines, purely additive (registration block after cli_package_stale).
("Could not find executable" notices in -N are expected: this build dir built only CoreSetup/CoreSetupData,
not the test exes; listing/registration is intact. desktop_self_test keeps its separate 'desktop' label.)
Repo scope check: only app/installer/Setup.cpp (M), tests/CMakeLists.txt (M), tests/installer_samples.cmake
(new). Pre-existing uncommitted root CMakeLists.txt sanitizer wiring + docs untouched.
Evidence index: red-run.txt / red-run-final.txt (UTF-8) / green-run.txt / merge-list.txt / build-unfixed.log
/ build-fixed.log / manual-matrix\matrix-log.txt, all under D:\Core-build\s5-fix\.

## Proposed ledger lines (for the scribe; not written to ESTADO.md — outside this task's repo write scope)

- BUG- entry proposal: "Core Setup copySamples overwrote user-edited/user-owned samples at distributed
  paths (size/date-only decision) and removeSamples deleted previously-listed paths holding user content
  at uninstall. Fixed at a9a0384+ by content-hash ownership: samples.txt records '<rel>\t<sha256>' per
  installed file (legacy plain lines re-validated against the distributed source, conservatively kept when
  the source is unavailable); copy skips+unlists user content, uninstall re-hashes before deleting.
  Red measured pre-fix (29/70 checks failed incl. cases i/ii/iv/vii), green post-fix (70/70) with the real
  silent installer; permanent regression tests/installer_samples.cmake (ctest -L merge, RUN_SERIAL).
  Evidence: D:\Core-build\s5-fix\{red-run-final,green-run,merge-list}.txt, manual-matrix\matrix-log.txt;
  method D:\Core-build\coordinator-round2\s5-fix-notes.md. DATA priority."
- Estado medido proposal: "installer_samples: 70 checks, merge label, ~5 s, real Core Setup.exe silent
  flow, synthetic fixtures; red 29/70 fail at a9a0384 unfixed, green 70/70 fixed (2 runs); merge suite
  registration 21 -> 22, no existing test removed/weakened."
- Limitations to carry: GUI window path shares copySamples/uninstall (read-only inference, not executed);
  version-B update not built (compile-time version; same decision branch exercised by reinstall of A);
  synthetic ~KB fixtures, not the ~7 GB real library (per-file decision, scale-independent; reinstall now
  hashes listed files — one sequential read each — instead of stat-only); --no-shortcuts used (no
  registry/shortcut side effects); running-from-target self-delete branch not exercised.

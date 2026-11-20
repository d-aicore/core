# S5 (DATA priority): Core Setup must never overwrite user-edited/user-owned content at a distributed
# sample path, and uninstall must never delete user content through samples.txt. Red measured by
# execution at a9a0384 (D:\Core-build\coordinator-round2\s5-notes.md, logs 00-17): the copy decision
# was size+date only, and removeSamples deleted every listed file without re-validating ownership.
# This test drives the REAL Core Setup.exe silent flow (--install/--uninstall, --no-shortcuts, no
# elevation) in temp targets under the build tree with synthetic fixture WAVs, and asserts sha256
# before/after, manifest content, and survival/deletion per case:
#   (i)   user-EDITED distributed sample        -> reinstall keeps the edit; uninstall keeps the file
#   (ii)  USER-OWNED file at a distributed path -> reinstall keeps it; uninstall keeps it
#   (iii) INTACT distributed sample             -> reinstall skips the copy; uninstall removes it
#   (iv)  USER-OWNED, same size+date, path previously listed -> stays unlisted after reinstall,
#         survives uninstall (the Audit-27 boundary extended to listed paths)
#   (v)   never-listed USER-OWNED, exact size+date match -> not listed, survives (Audit-27 itself)
#   (vi)  user file at a NON-distributed path   -> never touched, survives
#   (vii) per-file isolation: listed user content vs non-listed user file in one uninstall run
# Plus the ownership-evidence contract sub-cases: hashed manifest entries, LEGACY plain-path entries
# re-validated against the distributed source at uninstall, hashed-entry re-validation at removal,
# and the conservative rule when the source library is unavailable at uninstall (legacy entries are
# then never deleted). On the UNFIXED installer the data-loss cases (i, ii, iv, vii and the removal
# re-validation sub-cases) must FAIL; (iii), (v), (vi) must PASS.
#   cmake -DSETUP=<Core Setup.exe> -DWORK=<temp dir under the build tree> -P installer_samples.cmake

if(NOT DEFINED SETUP OR NOT DEFINED WORK)
  message(FATAL_ERROR "installer_samples: pass -DSETUP=<Core Setup.exe> -DWORK=<temp dir>")
endif()
if(NOT EXISTS "${SETUP}")
  message(FATAL_ERROR "installer_samples: installer not found: ${SETUP}")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(LIB "${WORK}/library-A")
set(FIXED_MTIME "2026-11-01T10:00:00")  # the distributed source date; CopyFile preserves it on copy

# ---------------------------------------------------------------- check helpers (failures accumulate
# so one run reports every case, not only the first)
set(failures "")
set(checks 0)

macro(pass label)
  math(EXPR checks "${checks} + 1")
  message(STATUS "PASS ${label}")
endmacro()

macro(fail label detail)
  math(EXPR checks "${checks} + 1")
  list(APPEND failures "${label}: ${detail}")
  message(STATUS "FAIL ${label}: ${detail}")
endmacro()

macro(expect_hash label path expected)
  if(NOT EXISTS "${path}")
    fail("${label}" "file is gone: ${path}")
  else()
    file(SHA256 "${path}" actual_hash)
    if(actual_hash STREQUAL "${expected}")
      pass("${label}")
    else()
      fail("${label}" "${path} sha256=${actual_hash}, expected ${expected}")
    endif()
  endif()
endmacro()

macro(expect_exists label path)
  if(EXISTS "${path}")
    pass("${label}")
  else()
    fail("${label}" "missing: ${path}")
  endif()
endmacro()

macro(expect_absent label path)
  if(EXISTS "${path}")
    fail("${label}" "should be gone but exists: ${path}")
  else()
    pass("${label}")
  endif()
endmacro()

# The relative paths listed in samples.txt, tab+hash suffix stripped (legacy plain lines pass through).
macro(manifest_rels outvar folder)
  set(${outvar} "")
  if(EXISTS "${folder}/samples.txt")
    file(READ "${folder}/samples.txt" manifest_text)
    string(REPLACE "\r" "" manifest_text "${manifest_text}")
    string(REPLACE "\n" ";" manifest_lines "${manifest_text}")
    foreach(manifest_line IN LISTS manifest_lines)
      if(manifest_line STREQUAL "")
        continue()
      endif()
      string(REGEX REPLACE "\t.*$" "" manifest_rel "${manifest_line}")
      list(APPEND ${outvar} "${manifest_rel}")
    endforeach()
  endif()
endmacro()

macro(expect_listed label folder rel)
  manifest_rels(listed_rels "${folder}")
  if("${rel}" IN_LIST listed_rels)
    pass("${label}")
  else()
    string(REPLACE ";" ", " listed_text "${listed_rels}")
    fail("${label}" "${rel} is not listed in ${folder}/samples.txt (listed: ${listed_text})")
  endif()
endmacro()

macro(expect_not_listed label folder rel)
  manifest_rels(listed_rels "${folder}")
  if("${rel}" IN_LIST listed_rels)
    fail("${label}" "${rel} must not be listed in ${folder}/samples.txt (user content)")
  else()
    pass("${label}")
  endif()
endmacro()

# The new manifest format: '<rel>\t<sha256 of the content Core installed>' with the right hash.
macro(expect_hashed_entry label folder rel expected)
  if(NOT EXISTS "${folder}/samples.txt")
    fail("${label}" "no samples.txt in ${folder}")
  else()
    file(READ "${folder}/samples.txt" manifest_text)
    string(REPLACE "\r" "" manifest_text "${manifest_text}")
    string(REPLACE "." "\\." rel_escaped "${rel}")
    if(manifest_text MATCHES "(^|\n)${rel_escaped}\t([0-9a-f]+)(\n|$)")
      string(LENGTH "${CMAKE_MATCH_2}" hash_len)
      if(hash_len EQUAL 64 AND CMAKE_MATCH_2 STREQUAL "${expected}")
        pass("${label}")
      else()
        fail("${label}" "${rel} entry hash '${CMAKE_MATCH_2}' != installed content ${expected}")
      endif()
    else()
      fail("${label}" "samples.txt has no tab+sha256 entry for ${rel} (legacy plain line?)")
    endif()
  endif()
endmacro()

macro(expect_kept_message label needle)
  string(REPLACE "\\" "/" kept_out "${setup_out}")  # the installer prints Windows backslash paths
  string(FIND "${kept_out}" "${needle}" kept_at)
  if(kept_at GREATER -1)
    pass("${label}")
  else()
    fail("${label}" "installer output does not name the preserved file ('${needle}') -- got: ${setup_out}")
  endif()
endmacro()

macro(run_setup label)
  execute_process(COMMAND "${SETUP}" ${ARGN}
                  OUTPUT_VARIABLE setup_out ERROR_VARIABLE setup_err
                  RESULT_VARIABLE setup_rc TIMEOUT 120)
  if(NOT setup_rc EQUAL 0)
    message(FATAL_ERROR "${label}: Core Setup exited ${setup_rc}: ${setup_err}${setup_out}")
  endif()
endmacro()

macro(set_mtime path)
  execute_process(COMMAND powershell -NoProfile -Command
                    "(Get-Item -LiteralPath '${path}').LastWriteTime = [datetime]::Parse('${FIXED_MTIME}')"
                  RESULT_VARIABLE mtime_rc OUTPUT_QUIET ERROR_VARIABLE mtime_err)
  if(NOT mtime_rc EQUAL 0)
    message(FATAL_ERROR "cannot set the mtime of ${path}: ${mtime_err}")
  endif()
endmacro()

# ---------------------------------------------------------------- synthetic fixtures (same sizes as
# the s5-red measurement; content is deterministic ASCII, hashes computed from the exact bytes)
string(REPEAT "CoreFixtureKick0123456789ABCDEFG" 128 kick_src)     # 4096 B
string(REPEAT "CoreFixtureSnare0123456789ABCDEF" 160 snare_src)   # 5120 B
string(REPEAT "CoreFixtureBass0123456789ABCDEFG" 192 bass_src)    # 6144 B
string(REPEAT "CoreFixtureKeys0123456789ABCDEFG" 144 keys_src)    # 4608 B
string(REPEAT "USER-EDIT-kick-tail-0123456789ab" 8 kick_tail)     # +256 B user edit
set(kick_user "${kick_src}${kick_tail}")                          # 4352 B
string(REPEAT "USER-OWNED-snare-content-01234" 100 snare_user)    # 3000 B user-owned
string(REPEAT "USER-OWNED-keys-content-01234567" 144 keys_user)   # 4608 B user-owned, same size
string(REPEAT "USER-FRESH-snare-content-0123456" 160 snare_fresh) # 5120 B user-owned, same size
string(REPEAT "USER-TAKE-non-distributed-012345" 64 user_take)    # 2048 B non-distributed path
string(REPEAT "EVIDENCE-user-take-0123456789ABC" 64 evidence_take)# 2048 B non-listed contrast
string(REPEAT "USER-EDIT-bass-tail-0123456789ab" 4 bass_tail)     # +128 B user edit
set(bass_user "${bass_src}${bass_tail}")                          # 6272 B

string(SHA256 h_kick_src "${kick_src}")
string(SHA256 h_snare_src "${snare_src}")
string(SHA256 h_bass_src "${bass_src}")
string(SHA256 h_keys_src "${keys_src}")
string(SHA256 h_kick_user "${kick_user}")
string(SHA256 h_snare_user "${snare_user}")
string(SHA256 h_keys_user "${keys_user}")
string(SHA256 h_snare_fresh "${snare_fresh}")
string(SHA256 h_user_take "${user_take}")
string(SHA256 h_evidence_take "${evidence_take}")
string(SHA256 h_bass_user "${bass_user}")

file(WRITE "${LIB}/Drums/kick-a.wav" "${kick_src}")
file(WRITE "${LIB}/Drums/snare-a.wav" "${snare_src}")
file(WRITE "${LIB}/Bass/bass-a.wav" "${bass_src}")
file(WRITE "${LIB}/Keys/keys-a.wav" "${keys_src}")
file(WRITE "${LIB}/_core-plan/plan.txt" "_core-plan is never copied by the installer\n")
foreach(fixture Drums/kick-a.wav Drums/snare-a.wav Bass/bass-a.wav Keys/keys-a.wav)
  set_mtime("${LIB}/${fixture}")
endforeach()
# The written bytes must hash exactly like the in-script strings (guards the fixture method itself).
expect_hash("fixture kick hash" "${LIB}/Drums/kick-a.wav" "${h_kick_src}")
expect_hash("fixture snare hash" "${LIB}/Drums/snare-a.wav" "${h_snare_src}")
expect_hash("fixture bass hash" "${LIB}/Bass/bass-a.wav" "${h_bass_src}")
expect_hash("fixture keys hash" "${LIB}/Keys/keys-a.wav" "${h_keys_src}")

# ================================================================ target-main: cases (i)-(iv)
message(STATUS "=== target-main: (i) edited distributed, (ii) user-owned at distributed path, (iii) intact, (iv) user-owned same size+date previously listed ===")
set(MAIN "${WORK}/target-main")
run_setup("main install A" --install "${MAIN}" --no-shortcuts --samples "${LIB}")
expect_hash("main install kick" "${MAIN}/Samples/Drums/kick-a.wav" "${h_kick_src}")
expect_hash("main install snare" "${MAIN}/Samples/Drums/snare-a.wav" "${h_snare_src}")
expect_hash("main install bass" "${MAIN}/Samples/Bass/bass-a.wav" "${h_bass_src}")
expect_hash("main install keys" "${MAIN}/Samples/Keys/keys-a.wav" "${h_keys_src}")
expect_absent("main _core-plan never copied" "${MAIN}/Samples/_core-plan")
expect_hashed_entry("main manifest kick hash" "${MAIN}" "Drums/kick-a.wav" "${h_kick_src}")
expect_hashed_entry("main manifest snare hash" "${MAIN}" "Drums/snare-a.wav" "${h_snare_src}")
expect_hashed_entry("main manifest bass hash" "${MAIN}" "Bass/bass-a.wav" "${h_bass_src}")
expect_hashed_entry("main manifest keys hash" "${MAIN}" "Keys/keys-a.wav" "${h_keys_src}")

# User mutations between installs (the s5-red matrix):
file(WRITE "${MAIN}/Samples/Drums/kick-a.wav" "${kick_user}")    # (i) edited: +256 B, new date
file(WRITE "${MAIN}/Samples/Drums/snare-a.wav" "${snare_user}")  # (ii) user-owned: 3000 B, new date
file(WRITE "${MAIN}/Samples/Keys/keys-a.wav" "${keys_user}")     # (iv) user-owned, same size...
set_mtime("${MAIN}/Samples/Keys/keys-a.wav")                     # ...and the source date restored
# (iii) bass stays intact.
expect_hash("main pre-reinstall user kick" "${MAIN}/Samples/Drums/kick-a.wav" "${h_kick_user}")
expect_hash("main pre-reinstall user snare" "${MAIN}/Samples/Drums/snare-a.wav" "${h_snare_user}")
expect_hash("main pre-reinstall user keys" "${MAIN}/Samples/Keys/keys-a.wav" "${h_keys_user}")

run_setup("main reinstall A" --install "${MAIN}" --no-shortcuts --samples "${LIB}")
expect_hash("(i) reinstall keeps the user edit" "${MAIN}/Samples/Drums/kick-a.wav" "${h_kick_user}")
expect_hash("(ii) reinstall keeps the user-owned snare" "${MAIN}/Samples/Drums/snare-a.wav" "${h_snare_user}")
expect_hash("(iii) intact bass unchanged" "${MAIN}/Samples/Bass/bass-a.wav" "${h_bass_src}")
expect_hash("(iv) reinstall keeps the user-owned keys" "${MAIN}/Samples/Keys/keys-a.wav" "${h_keys_user}")
expect_not_listed("(i) edited kick dropped from manifest" "${MAIN}" "Drums/kick-a.wav")
expect_not_listed("(ii) user snare dropped from manifest" "${MAIN}" "Drums/snare-a.wav")
expect_not_listed("(iv) user keys dropped from manifest" "${MAIN}" "Keys/keys-a.wav")
expect_listed("(iii) intact bass stays listed" "${MAIN}" "Bass/bass-a.wav")
expect_hashed_entry("(iii) bass entry keeps the installed hash" "${MAIN}" "Bass/bass-a.wav" "${h_bass_src}")
expect_kept_message("(i) message names the preserved kick" "${MAIN}/Samples/Drums/kick-a.wav")
expect_kept_message("(ii) message names the preserved snare" "${MAIN}/Samples/Drums/snare-a.wav")
expect_kept_message("(iv) message names the preserved keys" "${MAIN}/Samples/Keys/keys-a.wav")

run_setup("main uninstall" --uninstall "${MAIN}" --no-shortcuts --samples "${LIB}")
expect_hash("(i) uninstall keeps the user edit" "${MAIN}/Samples/Drums/kick-a.wav" "${h_kick_user}")
expect_hash("(ii) uninstall keeps the user-owned snare" "${MAIN}/Samples/Drums/snare-a.wav" "${h_snare_user}")
expect_hash("(iv) uninstall keeps the user-owned keys" "${MAIN}/Samples/Keys/keys-a.wav" "${h_keys_user}")
expect_absent("(iii) uninstall removes the intact distributed bass" "${MAIN}/Samples/Bass/bass-a.wav")
expect_absent("main samples.txt removed" "${MAIN}/samples.txt")
expect_exists("main Samples stays (user content)" "${MAIN}/Samples")
expect_exists("main folder stays (user content)" "${MAIN}")

# ================================================================ target-fresh: cases (v), (vi)
message(STATUS "=== target-fresh: (v) never-listed exact size+date match (Audit-27), (vi) non-distributed path ===")
set(FRESH "${WORK}/target-fresh")
file(WRITE "${FRESH}/Samples/Drums/snare-a.wav" "${snare_fresh}")  # (v) pre-placed, same size...
set_mtime("${FRESH}/Samples/Drums/snare-a.wav")                    # ...and same date as the source
file(WRITE "${FRESH}/Samples/MyRecordings/user-take.wav" "${user_take}")  # (vi)
run_setup("fresh install A" --install "${FRESH}" --no-shortcuts --samples "${LIB}")
expect_hash("(v) never-listed user snare not overwritten" "${FRESH}/Samples/Drums/snare-a.wav" "${h_snare_fresh}")
expect_not_listed("(v) never-listed user snare stays unlisted" "${FRESH}" "Drums/snare-a.wav")
expect_hash("(vi) non-distributed user take untouched" "${FRESH}/Samples/MyRecordings/user-take.wav" "${h_user_take}")
expect_listed("fresh bass listed" "${FRESH}" "Bass/bass-a.wav")
expect_listed("fresh kick listed" "${FRESH}" "Drums/kick-a.wav")
expect_listed("fresh keys listed" "${FRESH}" "Keys/keys-a.wav")
run_setup("fresh uninstall" --uninstall "${FRESH}" --no-shortcuts --samples "${LIB}")
expect_hash("(v) uninstall keeps the never-listed user snare" "${FRESH}/Samples/Drums/snare-a.wav" "${h_snare_fresh}")
expect_hash("(vi) uninstall keeps the non-distributed user take" "${FRESH}/Samples/MyRecordings/user-take.wav" "${h_user_take}")
expect_absent("fresh distributed bass removed" "${FRESH}/Samples/Bass/bass-a.wav")
expect_absent("fresh distributed kick removed" "${FRESH}/Samples/Drums/kick-a.wav")
expect_absent("fresh distributed keys removed" "${FRESH}/Samples/Keys/keys-a.wav")
expect_absent("fresh samples.txt removed" "${FRESH}/samples.txt")
expect_exists("fresh folder stays (user content)" "${FRESH}")

# ================================================================ target-evidence: case (vii) + LEGACY
# uninstall re-validation. The manifest is rewritten to plain lines (the format an older installer
# wrote) before uninstall: legacy entries are deleted only when their content still matches the
# distributed source; the listed user-content keys file must survive beside the non-listed user take.
message(STATUS "=== target-evidence: (vii) listed user content vs non-listed user file; legacy manifest at uninstall ===")
set(EVID "${WORK}/target-evidence")
run_setup("evidence install A" --install "${EVID}" --no-shortcuts --samples "${LIB}")
file(WRITE "${EVID}/samples.txt" "Bass/bass-a.wav\nDrums/kick-a.wav\nDrums/snare-a.wav\nKeys/keys-a.wav\n")
file(WRITE "${EVID}/Samples/Keys/keys-a.wav" "${keys_user}")
set_mtime("${EVID}/Samples/Keys/keys-a.wav")
file(WRITE "${EVID}/Samples/MyRecordings/user-take.wav" "${evidence_take}")
run_setup("evidence uninstall" --uninstall "${EVID}" --no-shortcuts --samples "${LIB}")
expect_hash("(vii) uninstall keeps the listed user-content keys" "${EVID}/Samples/Keys/keys-a.wav" "${h_keys_user}")
expect_hash("(vii) non-listed user take survives byte-identical" "${EVID}/Samples/MyRecordings/user-take.wav" "${h_evidence_take}")
expect_absent("(vii) legacy intact bass removed" "${EVID}/Samples/Bass/bass-a.wav")
expect_absent("(vii) legacy intact kick removed" "${EVID}/Samples/Drums/kick-a.wav")
expect_absent("(vii) legacy intact snare removed" "${EVID}/Samples/Drums/snare-a.wav")
expect_absent("evidence samples.txt removed" "${EVID}/samples.txt")
expect_kept_message("(vii) message names the preserved keys" "${EVID}/Samples/Keys/keys-a.wav")
expect_exists("evidence MyRecordings stays" "${EVID}/Samples/MyRecordings")

# ================================================================ target-revalidate: hashed-entry
# re-validation at removal. Everything intact through a reinstall (hashed entries), then the user
# edits the bass AFTER the reinstall: uninstall must re-hash and keep it, not trust the listing.
message(STATUS "=== target-revalidate: hashed manifest entry re-validated at uninstall ===")
set(REVAL "${WORK}/target-revalidate")
run_setup("revalidate install A" --install "${REVAL}" --no-shortcuts --samples "${LIB}")
run_setup("revalidate reinstall A" --install "${REVAL}" --no-shortcuts --samples "${LIB}")
expect_hashed_entry("revalidate bass hashed entry" "${REVAL}" "Bass/bass-a.wav" "${h_bass_src}")
file(APPEND "${REVAL}/Samples/Bass/bass-a.wav" "${bass_tail}")  # user edit after the last install
run_setup("revalidate uninstall" --uninstall "${REVAL}" --no-shortcuts --samples "${LIB}")
expect_hash("revalidate uninstall keeps the edited bass" "${REVAL}/Samples/Bass/bass-a.wav" "${h_bass_user}")
expect_absent("revalidate intact kick removed" "${REVAL}/Samples/Drums/kick-a.wav")
expect_absent("revalidate intact snare removed" "${REVAL}/Samples/Drums/snare-a.wav")
expect_absent("revalidate intact keys removed" "${REVAL}/Samples/Keys/keys-a.wav")
expect_absent("revalidate samples.txt removed" "${REVAL}/samples.txt")
expect_kept_message("revalidate message names the preserved bass" "${REVAL}/Samples/Bass/bass-a.wav")

# ================================================================ target-legacy-nosrc: conservative
# legacy rule. Plain-line manifest and NO source library available at uninstall: ownership cannot be
# proven, so nothing listed may be deleted (documented conservative choice).
message(STATUS "=== target-legacy-nosrc: legacy manifest, source unavailable at uninstall -> all preserved ===")
set(NOSRC "${WORK}/target-legacy-nosrc")
run_setup("nosrc install A" --install "${NOSRC}" --no-shortcuts --samples "${LIB}")
file(WRITE "${NOSRC}/samples.txt" "Bass/bass-a.wav\nDrums/kick-a.wav\nDrums/snare-a.wav\nKeys/keys-a.wav\n")
run_setup("nosrc uninstall" --uninstall "${NOSRC}" --no-shortcuts --samples "${WORK}/no-such-library")
expect_hash("nosrc legacy bass preserved" "${NOSRC}/Samples/Bass/bass-a.wav" "${h_bass_src}")
expect_hash("nosrc legacy kick preserved" "${NOSRC}/Samples/Drums/kick-a.wav" "${h_kick_src}")
expect_hash("nosrc legacy snare preserved" "${NOSRC}/Samples/Drums/snare-a.wav" "${h_snare_src}")
expect_hash("nosrc legacy keys preserved" "${NOSRC}/Samples/Keys/keys-a.wav" "${h_keys_src}")
expect_absent("nosrc samples.txt removed (entries dropped)" "${NOSRC}/samples.txt")
expect_exists("nosrc folder stays (samples preserved)" "${NOSRC}")
expect_kept_message("nosrc message names the preserved bass" "${NOSRC}/Samples/Bass/bass-a.wav")

# ---------------------------------------------------------------- verdict
list(LENGTH failures nfail)
if(nfail GREATER 0)
  string(REPLACE ";" "\n  " failure_text "${failures}")
  message(FATAL_ERROR "installer_samples: ${nfail} of ${checks} checks FAILED:\n  ${failure_text}")
endif()
message(STATUS "installer_samples: all ${checks} checks passed (7-case matrix + legacy/revalidation sub-cases)")

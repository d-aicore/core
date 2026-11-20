# S4 FIX notes — invalid float WAV rejection (owner-authorized, DATA priority)

- Agent: s4-fix. Repo: D:\CORE\CORE. HEAD verified = a9a038430d781959829f8510067ce59afffaad62 (git rev-parse, matches mandate).
- Working tree at start: pre-existing modifications CMakeLists.txt, app/installer/Setup.cpp, docs/relatorios/README.md, tests/CMakeLists.txt (concurrent teammate s5-fix per coordinator) + untracked docs/evidence/environment.yml/tests/installer_samples.cmake. engine/src/io/audio_file.cpp and engine/src/core/assets.cpp UNMODIFIED vs HEAD. Write scope: audio_file.cpp, assets.cpp only if needed, tests/ (own registration line only in tests/CMakeLists.txt).

## Step 1 — read red notes + sources (done)

- Read D:\Core-build\coordinator-round2\s4-red-notes.md in full. Defect measured RED at this SHA: f32 NaN/±Inf accepted raw (audio_file.cpp:139 memcpy), f64 finite 1e300 → ±Inf via (float) cast (:140), f64 NaN accepted; NO file-identifying message; invalid files FULLY registered+cached (assets.cpp:49-55). Header-rejection path (f32_badbits) throws with path prefix and registers nothing — the model to follow.
- audio_file.cpp read: conversion point :135-145 inner loop; header-rejection message style :84-88 = `path + ": <reason>."` thrown as std::runtime_error from parseWavHeader; readWav itself throws same style at :117/:119. readWav signature (audio_file.hpp per red notes): `AudioData readWav(const std::string& path, int64_t maxFrames)` — no signature change planned.
- assets.cpp read (:8-61): :49 `AudioData source = readWav(path, room);` inside try; :52 resample only if rate differs; :53 make_shared; :54 memory re-check; :55 `files_[key] = data;` single insertion AFTER all work succeeds; :57-60 catch sets *error=e.what(), returns nullptr, never inserts. Verdict (code-cited): a throw from readWav propagates out before any registration — NO assets.cpp change needed. Will be proven by the regression (AssetStore bytes()==0 + nullptr + error contains file name after invalid get). Caveat kept from red notes: :30 erases stale entry before reload (cache eviction, not partial registration).
- /fp:fast constraint: engine/CMakeLists.txt:9 `add_compile_options(/W3 /utf-8 /fp:fast)` unconditional for MSVC (Debug AND Release). Validation design = integer bit-pattern checks on raw bytes BEFORE any float math:
  - float32: exponent field all-ones ((bits & 0x7F800000u) == 0x7F800000u) → NaN (mantissa != 0) or Infinity (mantissa == 0). Pure uint32 ops, immune to /fp:fast.
  - float64: read raw uint64 bits; exponent all-ones → NaN/Inf; else finite double range check BEFORE the (float) cast: compare magnitude bits against FLT_MAX-as-double bit pattern (0x47EFFFFFE0000000 = 3.4028234663852886e38) using integer comparison of the absolute-value bits (works because IEEE-754 positive doubles order monotonically as integers). |d| > FLT_MAX → "overflow" rejection; no float comparison of d itself, no isfinite.
  - Throw style: `path + ": invalid float sample at frame N (channel C): NaN/Infinity/overflow ..."` — file + sample index + kind, matching header-rejection style.
- Fixtures + probe exist at D:\Core-build\s4-red\ (fixtures incl. f32_badbits; probe wav_probe.cpp links core_engine_lib.lib, /MTd /std:c++20). Permanent regression will generate its own fixtures in-test (no dependency on s4-red dir).

## Step 2 — tests/ coverage check + harness (done)

- Targeted grep (readWav|wavInfo|AssetStore|\.wav over tests/*.cpp): existing coverage is PCM roundtrips + AssetStore caching/memory (engine_tests.cpp:163-186 readWav/writeWav roundtrip PCM, :186 missing-file throw; audit_tests.cpp:231 AssetStore; hunt_tests.cpp:122 render readWav frames). NO float-WAV sample-value validation coverage anywhere → new suite justified: tests/wav_float_tests.cpp.
- Harness: tests/support/check.hpp — TEST("name") registration, CHECK/CHECK_EQ/CHECK_THROWS, CORE_TEST_MAIN; exit 0 = green; per-suite counts printed ("N testes, N verificacoes, N falhas" — existing harness strings, not touched; no translation sweep in this fix). Harness rule: no compiler-tied bit-exact goldens → the new suite writes fixtures from exact-representable constants and compares read-back bits to the SAME written patterns (true roundtrip, not goldens).
- tests/CMakeLists.txt read (208 lines): s5-fix's installer_samples block already present (:119-129, guarded WIN32+APP+CoreSetup). Shared LABELS-merge list at :131-132. To avoid touching the shared list (concurrent edits), my registration will be a self-contained block with its OWN set_tests_properties(wav_float_tests ... LABELS merge TIMEOUT 300), inserted after the engine_tests block. File will be RE-READ immediately before the edit.
- Test design (fixtures generated in-test into fs::temp_directory_path()/"core-wav-float-tests", canonical RIFF/fmt/data layout like the red fixtures, mono 48 kHz 64 frames unless noted):
  * valid f32 + valid f64 controls: exact-representable base values (0, ±0.5, ±0.25, 0.125, ±1, 0.75...), assert every sample's bits == written pattern (memcpy roundtrip; f64 values exactly representable as float so the cast is exact by IEEE, compiler-independent).
  * invalid f32: NaN@5 (0x7FC00000), +Inf@5 (0x7F800000), -Inf@5 (0xFF800000), sNaN@5 (0x7F800001 bonus), mixed NaN@3/+Inf@7/-Inf@11 (first offender frame 3).
  * invalid f64: NaN@5 (0x7FF8000000000000), +Inf@5, overflow +1e300@5/-1e300@9 (0x7E37E43C8800759C / 0xFE37E43C8800759C, finite doubles > FLT_MAX).
  * f64 boundary pair: exactly FLT_MAX (0x47EFFFFFE0000000) MUST import (L[5] bits 0x7F7FFFFF); next double above (…0001) MUST be refused as overflow. Matches the owner-specified |d| > FLT_MAX pre-cast range rule (conservative at the rounding boundary, per contract).
  * stereo NaN in RIGHT channel @2 → throw names file + frame 2 + right channel.
  * header-rejection control: float 8-bit badbits → existing path-identifying throw (style guard).
  * PCM16 control: exact values (0, ±0.5, -1) → PCM path untouched.
  * AssetStore: invalid get → nullptr + error contains file name + bytes()==0 + second get still nullptr (nothing cached); then valid get registers (512 B) and second get returns same pointer.
- Planned message format (fix): `path + ": invalid float sample at frame N" + (" (left)"/" (right)" if stereo) + ": NaN." / ": Infinity." / ": overflow (value exceeds float range)."` — header-rejection style (path prefix, std::runtime_error).
- Planned fix shape (audio_file.cpp only): anonymous-namespace helpers float32Defect(uint32 bits) / float64Defect(uint64 bits) — pure integer ops: exponent-all-ones test; f64 magnitude-bits compare vs 0x47EFFFFFE0000000 (FLT_MAX as double; positive IEEE doubles order monotonically as integers) BEFORE the (float) cast. No isfinite/isnan, no float comparisons → /fp:fast-proof in Debug AND Release. assets.cpp: no change (step 1 verdict; proven by AssetStore test cases).

## Step 3 — regression written + registered (done)

- NEW tests/wav_float_tests.cpp (16 TEST cases, English): valid f32/f64 bit-identical controls (per-sample CHECK_EQ of read-back bits vs written patterns, L and R); f32 NaN/+Inf/-Inf/sNaN/mixed refusals (file name + "frame N" + kind asserted in the runtime_error message); f64 NaN/+Inf/overflow(+1e300/-1e300) refusals; f64 boundary pair (exactly FLT_MAX 0x47EFFFFFE0000000 imports with L[5]=0x7F7FFFFF; …0001 refused "overflow"); stereo NaN in right channel (asserts "right" + "frame 2"); header-rejection control (8-bit float badbits, "unsupported sample format"); PCM16 control (exact values); AssetStore refusal (nullptr + error names file + bytes()==0 + second attempt still refused, f32 NaN and f64 overflow); AssetStore valid-after-refusal caching (512 B, same pointer on second get).
- Fixtures generated in-test into %TEMP%\core-wav-float-tests (canonical RIFF/fmt16/data, mono 48 kHz 64 frames; no dependency on D:\Core-build\s4-red). CHECK_REFUSED helper: missing throw = failure (never silent pass), message parts asserted.
- tests/CMakeLists.txt RE-READ immediately before edit (unchanged, 208 lines, s5-fix installer block intact at :119-129). Added ONLY a self-contained wav_float_tests block after engine_tests (add_executable/include dirs engine/src+tests/link core_engine_lib/add_test/own set_tests_properties LABELS merge TIMEOUT 300). Shared LABELS list at (old) :131 NOT touched → no conflict surface with s5-fix.
- Expected RED shape on unfixed engine: the 10 refusal cases + 2 AssetStore cases FAIL ("no exception thrown" / non-null get), valid/bit-identical + PCM16 + badbits cases PASS.

## Step 4 — OWN Debug build, UNFIXED, RED measured (done)

- Toolchain (from s1-red cache, verified): VS 18 BuildTools vcvars64.bat; cmake 4.4.4 + ninja via WinGet paths; VSLANG=1033 + chcp 65001 in D:\Core-build\s4-fix\configure-build-debug.bat.
- Configure: D:\Core-build\s4-fix\build, -G Ninja, CMAKE_BUILD_TYPE=Debug, CORE_BUILD_TESTS=ON, CORE_BUILD_APP=OFF (root CMakeLists:59-124 confirms APP=OFF skips FetchContent/JUCE entirely). Build UNFIXED (audio_file.cpp untouched at this point): 91/91 targets, EXIT=0. Log: D:\Core-build\s4-fix\build-debug-unfixed.log.
- RED run: D:\Core-build\s4-fix\build\tests\wav_float_tests.exe → EXIT=1, "16 testes, 360 verificacoes, 22 falhas". Log: D:\Core-build\s4-fix\red-wav-float-debug.log.
- Per-case: ok = valid f32 bit-identical, valid f64 bit-identical, header rejection (badbits), PCM16 control (4 pass). FAIL = f32 NaN/+Inf/-Inf/sNaN/mixed, f64 NaN/+Inf/overflow, f64 boundary (import half passes, refusal half fails), stereo right-channel NaN, AssetStore refusal, AssetStore valid-after-refusal (12 fail).
- Red excerpt (stderr): `FALHOU wav_float_tests.cpp:145: expected readWav to refuse C:\Users\vk_do\AppData\Local\Temp\core-wav-float-tests\f32_nan.wav (no exception thrown)`; AssetStore: `store.get(nanPath, kRate, &err) == nullptr` failed, `store.bytes() == 0: medido 512, esperado 0` (invalid asset FULLY registered, matching the s4-red probe measurement); overflow file registered 1024 B cumulative.
- RED-FIRST satisfied: the new permanent suite fails on the unfixed engine at the target SHA, exactly on the invalid-fixture cases; valid-file behaviour already green.

## Step 5 — fix applied (done)

- engine/src/io/audio_file.cpp ONLY (+39/-2, git diff reviewed):
  * :16 new u64(p) LE byte reader (next to existing u32/u16).
  * :18-36 anonymous-namespace float32Defect(uint32)/float64Defect(uint64): pure integer bit-pattern tests. f32: exponent-all-ones (bits & 0x7F800000 == 0x7F800000) → mantissa!=0 ? "NaN" : "Infinity"; everything else (incl. ±0, subnormals) finite. f64: magnitude bits (sign masked); exponent-all-ones → NaN/Infinity; else finite-double range check mag > 0x47EFFFFFE0000000 (FLT_MAX as double) → "overflow (value exceeds float range)" BEFORE the (float) cast. No isfinite/isnan, no float comparisons anywhere → /fp:fast-proof (integer ops cannot be optimized away by fp assumptions).
  * :150-160 readWav rejectSample lambda: throws std::runtime_error `path + ": invalid float sample at frame N" + (" (left)"/" (right)" when stereo) + ": " + kind + "."` — same style as header rejections (:105-109 region post-edit).
  * :171-177 conversion point: f32 branch checks float32Defect(u32(p)) before the unchanged memcpy; f64 branch checks float64Defect(u64(p)) before the unchanged memcpy+(float)d cast. PCM branches untouched. Valid samples: identical code path as before (check returns nullptr, conversion unchanged) → bit-identical import preserved.
- engine/src/core/assets.cpp: NO CHANGE (git status clean for it). Registration order verified in step 1: :49 readWav inside try → :55 single files_[key]=data insertion only after success → :57-60 catch sets *error, returns nullptr, never inserts. The new throw therefore propagates before any registration; proven by the AssetStore test cases in step 6. audio_file.hpp: NO signature change.
- Boundary semantics note: the owner contract specifies |d| > FLT_MAX → reject. IEEE round-to-nearest would actually cast doubles in (FLT_MAX, 2^128−2^103) down to FLT_MAX (finite); the implemented rule is the specified conservative one (reject anything above FLT_MAX), and the boundary test pins exactly FLT_MAX accepted / next double ulp rejected.

## Step 6 — Debug incremental rebuild + GREEN (done)

- cmake --build D:\Core-build\s4-fix\build --parallel 6 (vcvars64/VSLANG=1033/chcp 65001): 15/15 targets (audio_file.cpp.obj recompiled, core_engine_lib.lib + all test exes relinked), BUILD_EXIT=0. Log: build-debug-fixed.log.
- wav_float_tests.exe (fixed Debug lib, /fp:fast active): **16 testes, 360 verificacoes, 0 falhas**, TEST_EXIT=0 — ALL 16 cases ok, including the 12 that were RED in step 4. Log: green-wav-float-debug.log.
- Valid-file bit-identity preserved (both controls ok, per-sample L+R bit checks); AssetStore refusal leaves bytes()==0 and second attempt still refused; valid-after-refusal caches 512 B with same pointer. assets.cpp no-change verdict PROVEN by execution.

## Step 7 — Release proof GREEN (done, owner requirement)

- D:\Core-build\s4-fix\release configured (Release, CORE_BUILD_TESTS=ON, CORE_BUILD_APP=OFF, Ninja, vcvars64/VSLANG=1033/chcp 65001) + built 91/91, BUILD_EXIT=0. Log: build-release.log. engine/CMakeLists.txt:9 /fp:fast confirmed active in this Release build (same unconditional line).
- wav_float_tests.exe (Release): **16 testes, 360 verificacoes, 0 falhas**, TEST_EXIT=0 — identical green to Debug. Log: green-wav-float-release.log. The integer bit-pattern validation survives /fp:fast optimization in BOTH configs (measured, not assumed).

## Step 8 — ctest -L merge, Debug full suite (done)

- ctest --test-dir D:\Core-build\s4-fix\build -L merge -V --parallel 6 → **100% tests passed out of 20**, CTEST_EXIT=0, total 17.84 s real. Log: D:\Core-build\s4-fix\merge-debug.log.
- Per-suite counts (cases / checks / fails, from each exe's own summary line):
  | suite | cases | checks | fails | ctest |
  |---|---|---|---|---|
  | engine_tests | 39 | 221 | 0 | Passed 10.35 s |
  | wav_float_tests (NEW) | 16 | 360 | 0 | Passed 4.07 s |
  | domain_tests | 33 | 568 | 0 | Passed 16.96 s |
  | modules_tests | 6 | 128 | 0 | Passed 2.44 s |
  | role_tests | 21 | 215 | 0 | Passed 3.01 s |
  | midi_tests | 20 | 402 | 0 | Passed 2.32 s |
  | bug_tests | 48 | 462 | 0 | Passed 1.87 s |
  | bitwig_tests | 2 | 25 | 0 | Passed 1.14 s |
  | audit_tests | 7 | 65 | 0 | Passed 1.39 s |
  | audit2_tests | 15 | 364 | 0 | Passed 1.11 s |
  | hunt_tests | 11 | 106 | 0 | Passed 1.85 s |
  | round3_tests | 7 | 82 | 0 | Passed 1.72 s |
  | calls_tests | 12 | 69 | 0 | Passed 0.94 s |
  | menu_tests | 7 | 301 | 0 | Passed 0.70 s |
  | window_tests | 10 | 70 | 0 | Passed 0.44 s |
  | engine_process_tests | 3 | 98 | 0 | Passed 7.33 s |
  | cli_version / cli_demo_render / cli_measure / cli_unicode | — | — | — | all Passed |
- Sum: 257 cases / 3538 checks / 0 fails across the 16 exe suites + 4 cli tests = 20 ctest entries, all green. Nothing broke.
- engine_process_tests: PASSED here (3 cases — APP=OFF builds it without the JUCE crash-plugin cases). The known Debug ENV-03 exit-259 issue did NOT occur in this run; nothing to record separately.
- Scope note: APP=OFF engine+tests build — desktop_self_test, installer_samples, cli_package_stale and the plugin-crash render suites are not registered here (they need the app/JUCE/installer targets); this is the fast scope the mandate specified, not the full Windows gate.

## Step 9 — s4-red probe relinked vs FIXED Debug lib (done, second confirmation)

- D:\Core-build\s4-red\probe\wav_probe.cpp compiled UNCHANGED (cl /std:c++20 /EHsc /utf-8 /Zi /MTd, default /fp:precise probe so its isfinite scan is trustworthy) and linked against D:\Core-build\s4-fix\build\engine\core_engine_lib.lib (fixed, /fp:fast Debug). Exe: D:\Core-build\s4-fix\probe\wav_probe_fixed.exe; run on D:\Core-build\s4-red\fixtures. Logs: probe-relink.log, probe-fixed-output.txt. PROBE_EXIT=0.
- readWav results (vs red measurement at same SHA):
  | fixture | RED (unfixed) | GREEN (fixed) |
  |---|---|---|
  | f32_sine_valid | ACCEPTED, L[1]=3cebc842 | ACCEPTED, L[1]=3cebc842 (bit-identical), NONFINITE_TOTAL=0 |
  | f32_nan | ACCEPTED raw NaN, no message | REJECTED "D:\Core-build\s4-red\fixtures\f32_nan.wav: invalid float sample at frame 5: NaN." |
  | f32_posinf | ACCEPTED raw +Inf | REJECTED "...f32_posinf.wav: invalid float sample at frame 5: Infinity." |
  | f32_neginf | ACCEPTED raw -Inf | REJECTED "...f32_neginf.wav: invalid float sample at frame 5: Infinity." |
  | f32_mixed | ACCEPTED raw NaN/+Inf/-Inf | REJECTED "...f32_mixed.wav: invalid float sample at frame 3: NaN." (first offender) |
  | f64_valid | ACCEPTED, L[1]=3cebc842 | ACCEPTED, L[1]=3cebc842 (bit-identical), NONFINITE_TOTAL=0 |
  | f64_overflow | ACCEPTED, +1e300→+Inf @5, -1e300→-Inf @9 | REJECTED "...f64_overflow.wav: invalid float sample at frame 5: overflow (value exceeds float range)." |
  | f64_nan | ACCEPTED raw NaN | REJECTED "...f64_nan.wav: invalid float sample at frame 5: NaN." |
  | f32_badbits | REJECTED header, path named | REJECTED "...f32_badbits.wav: unsupported sample format (8-bit)." (unchanged) |
- AssetStore: every invalid fixture REFUSED ptr=null bytes_delta=0, error carries the full path + frame + kind; both valid fixtures REGISTERED 512 B, second_get_same_ptr=1 (cached), NONFINITE_TOTAL=0. No partial registration anywhere.

## Step 10 — final state, proposed ledger lines, limitations

- Files changed (write scope respected; git diff reviewed):
  * engine/src/io/audio_file.cpp (+39/-2): u64 reader :16; float32Defect :23-26; float64Defect :27-36 (FLT_MAX double pattern 0x47EFFFFFE0000000 integer compare); rejectSample lambda :150-160 (message :154); conversion-point checks :172 (f32) and :175 (f64), before the unchanged memcpy/cast.
  * tests/wav_float_tests.cpp (NEW, 293 lines, 16 TEST cases, fixtures generated in-test).
  * tests/CMakeLists.txt: ONLY the self-contained S4 block inserted after engine_tests (:17-27 region; add_executable :22, add_test :25, own LABELS merge :26). Shared LABELS list NOT touched by me. s5-fix installer_samples block intact (:139-149); s1-fix export_plugin_failure_tests block + shared-list edit arrived concurrently and are preserved (final git diff reviewed line by line).
  * engine/src/core/assets.cpp: UNCHANGED (registration order already correct; proven by test + probe). engine/src/io/audio_file.hpp: UNCHANGED (no signature change).
- HEAD still a9a038430d781959829f8510067ce59afffaad62; changes are in the working tree, NOT committed (no commit instruction in the mandate).
- Evidence index (all under D:\Core-build\s4-fix\): configure-build-debug.bat, build-debug-unfixed.log, red-wav-float-debug.log, build-debug.bat, build-debug-fixed.log, green-wav-float-debug.log, configure-build-release.bat, build-release.log, green-wav-float-release.log, merge-debug.log, relink-probe.bat, probe-relink.log, probe-fixed-output.txt, probe\wav_probe_fixed.exe.
- Proposed ESTADO.md lines (PROPOSAL ONLY — escrivão writes, D72/D80; bug namespace BUG-, D75):
  * Estado medido: «BUG-S4 import float WAV: NaN/±Inf e float64 finito-fora-do-alcance-do-float eram aceites em bruto e totalmente registados no AssetStore (vermelho medido em a9a0384, s4-red-notes.md). Corrigido no working tree sobre a9a0384: readWav recusa amostras float não-finitas por padrões de bits inteiros (à prova de /fp:fast, Debug+Release), mensagem identifica ficheiro+frame+tipo; AssetStore nada regista na recusa (assets.cpp inalterado). Regressão permanente tests/wav_float_tests.cpp: 16 casos/360 verificações — vermelho 12 casos/22 falhas antes da correção, verde Debug e Release depois; ctest -L merge Debug 20/20 (257 casos/3538 verificações/0 falhas); probe s4-red re-ligado à lib corrigida: 7 fixtures inválidos recusados com mensagem, válidos bit-idênticos (L[1]=3cebc842). Fontes: D:\Core-build\coordinator-round2\s4-fix-notes.md + D:\Core-build\s4-fix\logs.»
  * Pendência: commit + relatório formal docs/relatorios/ + indexação fora do write-scope desta entrega (coordenador/escrivão); gate Windows completo (APP=ON) não corrido aqui.
- Limitations (declared):
  1. Merge run scope = APP=OFF engine+tests (mandate's fast scope): desktop_self_test, installer_samples, cli_package_stale, plugin-crash render suites not registered; full Windows gate not run here. Counts are as of my configure time (s1-fix's export_plugin_failure_tests registration landed concurrently afterwards, not in my 20).
  2. engine_process_tests PASSED (3 cases; APP=OFF builds it without the JUCE crash-plugin cases). Known Debug ENV-03 exit-259 did NOT occur — nothing to conflate.
  3. Overflow boundary is the owner-specified conservative rule |d| > FLT_MAX → reject: IEEE round-to-nearest would cast doubles in (FLT_MAX, 2^128−2^103) down to finite FLT_MAX; those are refused by contract. Pinned by the boundary test (exactly FLT_MAX accepted, next double ulp refused).
  4. Fixtures use normal-range values; subnormal float samples are finite by the bit-pattern rule and pass (f32 path is pure memcpy), but subnormal f64→f32 cast behaviour under /fp:fast was not measured (out of contract scope).
  5. No commit made; formal docs/relatorios report + ESTADO.md entry are proposals above (write scope limited to audio_file.cpp / tests/ by the mandate).
- VERDICT: owner contract met and measured — invalid float WAV rejected at import with file-identifying message (file + frame + kind), valid WAV bit-identical, nothing partially registered; /fp:fast-proof in Debug AND Release; red-first permanent regression registered under LABELS merge.

# S4 RED measurement notes — invalid float WAV acceptance

- Repo: D:\CORE\CORE, HEAD verified = a9a038430d781959829f8510067ce59afffaad62 (target SHA).
- Working tree: only root CMakeLists.txt + docs modified/untracked; engine/src/io/audio_file.cpp, engine/src/core/assets.cpp, engine/CMakeLists.txt are UNMODIFIED vs HEAD (git status --porcelain). Read-only: no repo file edited by this agent.
- Scope: Debug measurement only. Release /fp:fast regression is a FUTURE-fix requirement, not run here (declared).
- Writes confined to D:\Core-build\s4-red\ + this notes file (+ transient build .bat in %TEMP% per task instruction).

## Step 1 — code reading (audio_file.cpp:60-169, assets.cpp:1-76)

Header validation (parseWavHeader, audio_file.cpp):
- :84 format must be 1 (PCM) or 3 (IEEE float), else throw "<path>: only PCM or float WAV files are supported."
- :85 channels must be 1 or 2, else throw (message includes path + channel count).
- :86 rate must be 8000..384000, else throw (path + rate in message).
- :87-88 bits gate: PCM 16/24/32; FLOAT 32 **and 64**. => 64-bit float IS accepted. Anything else throws "<path>: unsupported sample format (N-bit)."
- :77 WAVE_FORMAT_EXTENSIBLE (0xFFFE) sub-format read when fmt size >= 26.
- All header-rejection messages identify the file (path prefix). No sample-value validation exists anywhere.

readWav (audio_file.cpp:115-156):
- :139 float32: `std::memcpy(&v, p, 4)` — raw bits into float, NO isfinite/NaN check.
- :140 float64: `double d; memcpy(&d,p,8); v = (float)d;` — C cast, no range check; a finite double > FLT_MAX (e.g. 1e300) becomes +inf in the float buffer.
- :141-143 PCM paths (not in scope).
- Malformed *sample data* (NaN/±Inf) never throws: readWav returns AudioData with the raw non-finite floats. Only header/IO errors throw, and those messages carry the path.
- :117 open failure: "Cannot open <path>."; :119 maxFrames guard throws with path.

AssetStore::get (assets.cpp:8-61):
- :49 `AudioData source = readWav(path, room);` inside try — NO finite check before/after.
- :52 resample only if rate differs; :53 make_shared; :54 memory re-check; :55 `files_[key] = data;` — registration happens ONLY after readWav+conversion succeed.
- :57-60 catch: sets *error = e.what(), returns nullptr. No map insertion on the throw path => code-cited verdict: a load that THROWS leaves no partially registered asset (insertion is a single statement after all work). Caveat: :30 erases a stale cache entry before reload; if reload then throws, the old entry is gone (cache eviction, not partial registration).
- THE DEFECT: a NaN/Inf WAV does NOT throw, so it is FULLY registered as a valid asset with raw non-finite samples; no rejection and no file-identifying error is produced. To be measured by probe (non-null return + cached second get + bytes() growth).

## Step 2 — headers, lib location, build configuration

- Namespace/signatures (engine/src/io/audio_file.hpp, engine/src/core/assets.hpp): `namespace core`;
  `AudioData readWav(const std::string& path, int64_t maxFrames)` (throws std::runtime_error);
  `AudioData { double sampleRate; int channels; std::vector<float> L, R; frames(); bytes(); }` (mono: R == L copy);
  `AssetStore(size_t maxBytes = 2<<30)`, `get(path, sampleRate, std::string* error) -> shared_ptr<const AudioData>`, `bytes()`, `prune()`.
- Lib found: D:\Core-build\s1-red\build\engine\core_engine_lib.lib (55,347,162 bytes, LastWriteTime 2026-11-20 00:17:18).
  Freshness: audio_file.cpp.obj 00:17:18 and assets.cpp.obj 00:17:16 (same day, ~16 min before this session) vs source mtime 2026-11-17 17:33 => objects newer than sources; HEAD = target SHA; sources unmodified. Fit to measure this SHA.
  miniaudio_impl.lib also exists but readWav/assets TUs do not use miniaudio (audio_file.cpp = ifstream only); expect no need.
- Build type verified: D:\Core-build\s1-red\build\CMakeCache.txt CMAKE_BUILD_TYPE:STRING=Debug; CMAKE_CXX_FLAGS_DEBUG=/Zi /Ob0 /Od /RTC1.
- engine/CMakeLists.txt:9 (verbatim): `add_compile_options(/W3 /utf-8 /fp:fast)` — UNCONDITIONAL for MSVC, i.e. applies to Debug AND Release (task brief said Release; correction recorded). The lib's readWav was therefore compiled /fp:fast even in this Debug measurement.
- engine/CMakeLists.txt:10: static CRT `MultiThreaded$<$<CONFIG:Debug>:Debug>` => lib is /MTd. Probe must use /MTd (NOT the /MDd in the task template) to avoid runtime mismatch. Engine is C++20 (CMakeLists:3) => probe uses /std:c++20 (template said c++17; deviation recorded).
- /fp:fast risk for the FUTURE fix: MSVC /fp:fast assumes no NaN/Inf and may optimize away isfinite/isnan guards; the fix's regression must run in Release (and Debug, since :9 hits both) on Windows. NOT done here — Debug measurement only, declared.

## Step 3 — fixtures generated (D:\Core-build\s4-red\fixtures, gen_fixtures.py, bundled Python+numpy)

Canonical layout, all files: RIFF + u32 size + 'WAVE' + 'fmt ' 16-byte chunk + 'data'. Mono, 48000 Hz, 64 frames.
fmt fields (LE): format=3 (IEEE float), channels=1, rate=48000 (0xBB80), byterate=rate*blockalign, blockalign=bits/8, bits=32 or 64.
- float32 files: 300 bytes total (RIFF size 292; data 256 = 64*4; samples start at file offset 44).
- float64 files: 556 bytes total (data 512 = 64*8; samples start at offset 44).
Base signal: 0.5*sin(2*pi*440*t), finite everywhere except the specials below.

| fixture | bits | specials (frame index → value, LE bytes hex) |
|---|---|---|
| f32_sine_valid.wav | 32 | none (control) |
| f32_nan.wav | 32 | 5 → quiet NaN `00 00 c0 7f` (verified @offset 64) |
| f32_posinf.wav | 32 | 5 → +Inf `00 00 80 7f` |
| f32_neginf.wav | 32 | 5 → -Inf `00 00 80 ff` |
| f32_mixed.wav | 32 | 3 → NaN, 7 → +Inf, 11 → -Inf |
| f64_valid.wav | 64 | none (control; 64-bit accepted per audio_file.cpp:87) |
| f64_overflow.wav | 64 | 5 → +1e300 `9c 75 00 88 3c e4 37 7e` (verified @84), 9 → -1e300 `...fe` (verified @116); finite doubles > FLT_MAX |
| f64_nan.wav | 64 | 5 → NaN (bonus: NaN via float64 path) |

manifest.json alongside records layout + specials. Header hex of f32_nan verified: fmt format=03 00, bits=20 00, data size 00 01 00 00.

## Step 4 — probe build (no full engine build)

- Probe: D:\Core-build\s4-red\probe\wav_probe.cpp (calls core::readWav + core::AssetStore::get; scans L for non-finite; prints values + IEEE bit patterns; second get checks cache registration).
- Extra rejection-path fixture: f32_badbits.wav = copy of f32_sine_valid.wav with fmt bits field (offset 34) patched 32→8 (float 8-bit is invalid per audio_file.cpp:87).
- Build .bat (unique guid, %TEMP%): C:\Users\vk_do\AppData\Local\Temp\s4red-d37173237412498ba895a08c14cb782e.bat — VSLANG=1033, chcp 65001, vcvars64.bat (VS 18 BuildTools), then:
  cl /nologo /std:c++20 /EHsc /utf-8 /Zi /MTd /I D:\CORE\CORE\engine\src /I D:\CORE\CORE\engine\third_party wav_probe.cpp /Fe:wav_probe.exe /link D:\Core-build\s1-red\build\engine\core_engine_lib.lib
  Deviations from task template (recorded): /MTd not /MDd (lib is static-CRT Debug per engine/CMakeLists.txt:10); /std:c++20 not c++17 (engine is C++20). Probe itself compiles default /fp:precise, so its isfinite checks are trustworthy; the lib code under test was compiled /fp:fast (engine/CMakeLists.txt:9, all MSVC configs).
- Result: compiled and linked FIRST TRY against the existing Debug lib; no missing symbols; miniaudio_impl.lib NOT needed; no source-fallback needed. Exe: D:\Core-build\s4-red\probe\wav_probe.exe. Full run log: D:\Core-build\s4-red\probe\probe_output.txt.

## Step 5 — measured results (Debug, SHA a9a03843, engine lib built 2026-11-20 00:17)

readWav (maxFrames=1048576); AssetStore::get at 48000 == file rate (no resample; assets.cpp:52 raw pass-through):

| fixture | readWav accepted? | raw values in engine buffer | message identifying file? | AssetStore |
|---|---|---|---|---|
| f32_sine_valid.wav (control) | YES | all finite; L[1]=0.028782 (3cebc842) | none (no error) | REGISTERED, 512 B, cached |
| f32_nan.wav | YES | idx5 NaN bits=7fc00000, raw | NONE — no error at all | REGISTERED, 512 B, cached, error unset |
| f32_posinf.wav | YES | idx5 +Inf bits=7f800000, raw | NONE | REGISTERED, cached |
| f32_neginf.wav | YES | idx5 -Inf bits=ff800000, raw | NONE | REGISTERED, cached |
| f32_mixed.wav | YES | idx3 NaN, idx7 +Inf, idx11 -Inf, raw | NONE | REGISTERED, cached |
| f64_valid.wav (control) | YES | all finite; same L[1] bits as f32 control | none | REGISTERED, cached |
| f64_overflow.wav | YES | finite double +1e300 → +Inf (7f800000) @5; -1e300 → -Inf (ff800000) @9 via (float) cast :140 | NONE | REGISTERED, cached |
| f64_nan.wav (bonus) | YES | idx5 NaN via float64 path | NONE | REGISTERED, cached |
| f32_badbits.wav (malformed header) | NO (throws) | — | YES: "D:\...\f32_badbits.wav: unsupported sample format (8-bit)." | REFUSED null, bytes_delta=0, same path-identifying error |

DEFECT CONFIRMED (measured, RED): invalid float WAVs (NaN, +Inf, -Inf, mixed, and finite-double overflow through the 64-bit path) enter the engine RAW at this SHA. No rejection, no error, no file-identifying message. Finite samples pass through bit-identical across all fixtures (L[1]=3cebc842 everywhere), proving raw memcpy/cast pass-through, not conversion artifacts.

Partial-registration verdict (MEASURED + code-cited):
- Rejection path: AssetStore::get on f32_badbits returned nullptr, store.bytes() delta = 0, error carries the full path. No partial registration: files_[key] insertion is a single statement (assets.cpp:55) after readWav+conversion succeed; the catch (:57-60) never inserts.
- Invalid-but-accepted path (the actual defect): worse than partial — the invalid asset is FULLY registered and cached (bytes_delta=512 each; second get returns the same pointer for every invalid fixture; error string stays "<unset>").
- Code-cited caveat: assets.cpp:30 erases a stale cache entry BEFORE reload; if the reload then throws, the old entry is gone (cache eviction, not partial registration).

/fp:fast limitation (declared): engine/CMakeLists.txt:9 `add_compile_options(/W3 /utf-8 /fp:fast)` is unconditional for MSVC — it hits Debug AND Release (brief said Release only; corrected). Under /fp:fast MSVC assumes no NaN/Inf and may optimize away future isfinite guards. This measurement is Debug-only; the FUTURE fix's regression must run in Release on Windows too (and Debug, since :9 applies to both). Not done here — no fix implemented, repo untouched.

Artifacts: fixtures + manifest D:\Core-build\s4-red\fixtures\; probe + log D:\Core-build\s4-red\probe\; notes D:\Core-build\coordinator-round2\s4-red-notes.md. Repo writes: NONE (verified read-only; git status unchanged apart from pre-existing modifications).
Postscript: final git status shows `M tests/CMakeLists.txt` which was NOT present at session start — concurrent teammate activity in the shared checkout, not this agent (this agent wrote only D:\Core-build\s4-red\, this notes file, and the %TEMP% build .bat). HEAD still a9a03843; engine/src/io/audio_file.cpp, engine/src/core/assets.cpp, engine/CMakeLists.txt unmodified at both checks, so the measurement stands for the target SHA.

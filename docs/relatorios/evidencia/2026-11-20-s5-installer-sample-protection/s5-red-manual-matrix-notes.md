# S5 red-reproduction — installer overwrites user-edited/user-owned samples; manifest deletes them at uninstall

- Agent: `s5-red` (DATA priority, measurement only). Date: 2026-11-20 (Europe/Lisbon).
- Repo: `D:\CORE\CORE` @ `a9a038430d781959829f8510067ce59afffaad62` (`main` = `origin/main`). No repository file was edited; HEAD and the pre-existing uncommitted `CMakeLists.txt` sanitizer wiring verified unchanged after the run.
- Method: **real installer flow, confirmed by execution** — the built `Core Setup.exe` supports a silent `--install <folder>` target override with `--no-shortcuts` and needs **no elevation** (per-user design). No probe build was needed. All targets/sources were synthetic fixtures under `D:\Core-build\s5-installer\`; the user's real `D:\Samples` and real install locations were never read as a copy source nor written (`--samples` override short-circuits the `D:\Samples` fallback, Setup.cpp:55–60).
- Installer binary: `D:\Core-build\core-review\debug\app\installer\CoreSetup_artefacts\Debug\Core Setup.exe` (46,130,688 bytes, built 2026-11-19 23:16 at this SHA). Installed version string: `0.7.0` (measured via `current.txt`).
- Evidence: `D:\Core-build\s5-installer\logs\00…17` (snapshots, exact commands, exit codes); tools in `D:\Core-build\s5-installer\tools\`.

## Compact final answer

**Defect CONFIRMED by execution (red), both halves:**

1. **Copy side** — `copySamples` decides "same" by **size + last-modification time only** (Setup.cpp:89). Any destination whose size or date differs — including a sample the user edited or a user-owned file placed at a distributed path — is **unconditionally overwritten** (`from.copyFileTo(to)`, Setup.cpp:91 → `juce::File::copyFileTo` deletes the destination first, juce_File.cpp:317–321 → `CopyFile(..., false)`, juce_Files_windows.cpp:360–364). Measured: edited kick and user-owned snare hashes became byte-identical to the distributed source after reinstall.
2. **Manifest side** — `samples.txt` listing is never re-validated against content. A path listed by an earlier install **stays listed** even when the file now holds user content (Setup.cpp:93), and `removeSamples` **deletes every listed file that exists** at uninstall (Setup.cpp:108–111). Measured twice: user-owned keys content (same size/date as source, so the reinstall correctly skipped the copy) was still deleted at uninstall; in an isolated run the listed user-content file was deleted while a non-listed user file in the same `Samples\` tree survived byte-identical.

**Current behaviour table** (target-main unless noted; hashes truncated to 8 hex, full values in §4):

| case | before reinstall | after reinstall A | after uninstall | verdict |
|---|---|---|---|---|
| (i) distributed kick, user EDITED (+256 B) | `fac4d371` 4352 B | `b294a261` 4096 B = distributed | removed | **RED** — user edit overwritten (Setup.cpp:89–91) |
| (ii) USER-OWNED snare at distributed path (3000 B, new date) | `8d19a295` | `a2781ffc` 5120 B = distributed | removed | **RED** — user file overwritten, then deleted (Setup.cpp:89–91, 108–111) |
| (iii) distributed bass, INTACT | `8f272eb8` | `8f272eb8` (skipped, stays listed) | removed | **EXPECTED** — intact distributed sample updates/removes correctly (Setup.cpp:89, 93, 111) |
| (iv) USER-OWNED keys, same size+date, path previously listed | `539794f7` | `539794f7` (copy skipped, **stays listed**) | **removed** | **RED** — manifest deletes user content (Setup.cpp:93 → 108–111) |
| (v) target-fresh: USER-OWNED snare, same size+date, **never listed** | `26318c48` | `26318c48` (not copied, not listed) | **survives** | GREEN contrast — Audit-27 protection works, but only for this narrow case (Setup.cpp:76–80, 89, 93) |
| (vi) target-fresh: user file at NON-distributed path | `05957142` | `05957142` | **survives** | GREEN contrast — non-distributed paths never touched |
| (vii) target-evidence: listed keys = user content vs non-listed user-take, same run | `539794f7` / `56087d2d` | (no reinstall) | keys **removed**, user-take **survives** `56087d2d` | **RED**, per-file isolation of the manifest deletion |

**What a fix must satisfy** (each tied to a measured case):
1. Never overwrite user-edited or user-owned content at a distributed path: the `!same` branch (Setup.cpp:90–91) must distinguish "our own outdated copy" from "user modification" — size/date alone cannot (cases i, ii). Ownership evidence such as a recorded hash of the content actually installed is needed; a legitimate update (our outdated copy) must still be replaced.
2. Manifest removal must not delete user content: a `samples.txt` entry alone is insufficient at uninstall (Setup.cpp:108–111). Ownership must be re-validated at removal, or entries must be dropped when user modification is detected at install time — case (iv)/(vii) shows that fixing only the copy side still loses user data at uninstall, exactly the owner's "previously listed in the manifest" concern.
3. Intact distributed samples must still skip-copy on reinstall and be removed at uninstall (case iii), and the existing Audit-27 boundary must not regress: never-listed + exact size/date match stays unlisted and survives (case v); non-distributed user paths are never touched (case vi).

**Limitations:** real silent flow used, not the GUI window (both call the same `copySamples`, Setup.cpp:276 vs 383 — read-only inference); `--no-shortcuts` skipped shortcut/registry integration (Setup.cpp:38–51, 197, 230–234) — irrelevant to the sample defect; uninstall ran from the build-dir exe, so the running-from-target self-delete branch (Setup.cpp:247–249) was not exercised (sample/app removal logic identical — read-only inference); version B not built (version is compile-time baked, app/installer/CMakeLists.txt:31; a second variant would need repo edits, forbidden) — reinstall of A exercises the identical size/date decision; synthetic ~KB fixtures instead of the real ~7 GB library (decision is per-file, scale-independent); no elevation needed or used (all 7 invocations exit 0 as the current user).

---

## 1. Code documentation (read-only, confirmed in code)

### 1.1 Copy decision — `copySamples`, app/installer/Setup.cpp:68–102
- Source selection `samplesSource` (Setup.cpp:55–61): `--samples <folder>` wins; else `Samples` beside the exe; else `D:\Samples`. Source is only read (Setup.cpp:13). `_core-plan` entries skipped (Setup.cpp:63, applied at :75) — **verified by execution**: fixture `_core-plan\plan.txt` never appeared in any target or manifest.
- Manifest read **before** copying (Setup.cpp:80–81): `listed = fromLines(<folder>\samples.txt)`, empty lines removed.
- Per file (Setup.cpp:84–95):
  - **:89** `same = to.existsAsFile() && to.getSize() == from.getSize() && to.getLastModificationTime() == from.getLastModificationTime()` — the entire decision. No hash, no ownership record, no content check.
  - **:90–92** `if (!same)`: create parent dir, `from.copyFileTo(to)`; on success add rel to `ours`. **This is the overwrite defect** — a user-edited file (size/date changed) or a user-owned file at the same path is replaced without any check.
  - **:93** `else if (listed.contains(rel)) ours.add(rel)` — same size/date + previously listed → stays listed, no copy. This is how a path holding user content (case iv) remains marked for removal.
- Overwrite mechanism (JUCE 9.0.5, `D:\Core-deps\JUCE-9.0.5`): `File::copyFileTo` = `newFile.deleteFile() && copyInternal(newFile)` (modules/juce_core/files/juce_File.cpp:317–321); Windows `copyInternal` = `CopyFile(src, dest, false)` (modules/juce_core/native/juce_Files_windows.cpp:360–364) — `bFailIfExists=FALSE`. `CopyFile` preserves the source last-write time: measured, every overwritten file came back with mtime `2026-11-01T10:00:00.000` equal to the source.
- Manifest write (Setup.cpp:96–99): `listed.addArray(ours); removeDuplicates(true); samples.txt = join("\n") + "\n"`. Earlier listings are kept unconditionally ("Earlier installs' files stay listed too", :96).
- The comment at Setup.cpp:65–66 ("Files Core added there (Save as sample) are never touched") holds **only for non-distributed paths** (case vi); it is false for user content at distributed paths (cases i, ii, iv — measured).
- Audit-27 comment (Setup.cpp:76–79) documents the one existing protection: a pre-existing file matching path+size+date that was **never listed** is not added to `ours` → not removed at uninstall. Verified green (case v). It does not cover size/date divergence nor previously-listed paths.

### 1.2 Manifest format/location
- `<install folder>\samples.txt` (Setup.cpp:11, 80, 99, 106): plain text, one relative path per line, backslashes normalized to `/` (Setup.cpp:88), trailing newline. Measured content after install A:
  ```
  Bass/bass-a.wav
  Drums/kick-a.wav
  Drums/snare-a.wav
  Keys/keys-a.wav
  ```
- Separate app manifest `<install>\app-<version>\.core-installed-files.txt` (Setup.cpp:125, 181–182) lists the ZIP entries (50 lines measured); it governs app-file removal only, not samples.

### 1.3 Uninstall removal logic
- `removeSamples` (Setup.cpp:105–121): for every non-empty line of `samples.txt` without `..` (:108–109), **delete the file if it exists** (:110–111) — no content/ownership re-validation. Then delete empty subdirectories deepest-first (:113–117), then `samples.txt` itself if all deletions succeeded (:119).
- `uninstall` (Setup.cpp:228–254): refuses non-Core folders via `isCoreFolder` (:229, current.txt + app-<v>\Core.exe must exist, :213–216); with shortcuts, removes Start-menu/desktop links + HKCU uninstall key (:230–234); calls `removeSamples` (:236); removes only `isOurs` entries (:219–223: `app-*`, current.txt, previous.txt, Uninstall.exe, samples.txt) — app folders via the app manifest, never recursively blind (:152–166, comment :123–124); unknown entries set `othersLeft` and stay (:238); folder itself deleted only when empty (:250–251); running-from-target self-delete via delayed cmd (:247–249, not exercised here).

### 1.4 CLI modes and elevation — `SetupApplication::initialise`, Setup.cpp:372–390
- No args → GUI window (`SetupWindow`, :389; folder fixed to `%LOCALAPPDATA%\Programs\Core`, :25–27, :345).
- `--install <folder> [--no-shortcuts] [--samples <library> | --no-samples]` → silent install then `copySamples` (:381–383); documented "silent (tests); exit 0 when installed" (:3).
- `--uninstall <folder> [--no-shortcuts]` → silent uninstall (:381). Folder omitted → exe's own parent if it is a Core folder, else default (:378–380).
- `--no-shortcuts` (:374) skips `integrate()` at install (:197) and shortcut/registry removal at uninstall (:230) — used in every run here, so **zero registry/shortcut side effects**.
- Exit code: 0 success, 1 with problem text on stderr (:384–385). All 7 invocations measured exit 0, empty stderr.
- **Elevation: none.** Per-user installer (:1), HKCU-only registry (:23), arbitrary target via `--install`. Confirmed by execution into `D:\Core-build\...` temp targets as the current user.
- Single instance: `moreThanOneInstanceAllowed() = false` (:371); runs were sequential and each verified by filesystem state.
- Build: app/installer/CMakeLists.txt:24–32 (`juce_add_gui_app(CoreSetup)`, version from `CoreApp`'s `JUCE_VERSION` via `JUCE_APPLICATION_VERSION_STRING`, :31; package ZIP of Core.exe/core-engine.exe/demo/lang/patterns/pcore, :10–22). Gate conventions (ci/gate.ps1:21–56): vcvars64 (VS 18 BuildTools path first), `VSLANG=1033`, `chcp 65001`, Ninja, `--parallel` (gate default 8; task convention 6) — **not needed**, no probe was built.

## 2. Feasibility determination (task 2)

Real flow **feasible and used**: `Core Setup.exe --install <temp folder> --no-shortcuts --samples <temp library>` ran without elevation, exit 0, ~1 s per invocation (debug build, small fixture library). The probe fallback (compile Setup.cpp's real functions under `D:\Core-build\s5-installer\build` with gate conventions) was therefore **not required**; no second build was produced. Version B was not built: `kVersion` is compile-time (Setup.cpp:22, CMakeLists.txt:31) and changing it needs a repo edit, which is forbidden; reinstalling A exercises the identical size/date branch, and the defect is version-independent.

## 3. Execution log (exact commands, exit codes)

Exe for every step: `D:\Core-build\core-review\debug\app\installer\CoreSetup_artefacts\Debug\Core Setup.exe`.
Each step's full record (args, exit, seconds, stdout/stderr) is in `logs\<NN>-*.cmd.txt`; all stdout/stderr empty, all exits 0.

| # | command (after the exe path) | exit | log |
|---|---|---|---|
| 01 | `--install D:\Core-build\s5-installer\target-main --no-shortcuts --samples D:\Core-build\s5-installer\library-A` | 0 | 01-install-A.cmd.txt |
| 05 | same as 01 (reinstall A over the mutations) | 0 | 05-reinstall-A.cmd.txt |
| 07 | `--uninstall D:\Core-build\s5-installer\target-main --no-shortcuts` | 0 | 07-uninstall.cmd.txt |
| 10 | `--install D:\Core-build\s5-installer\target-fresh --no-shortcuts --samples D:\Core-build\s5-installer\library-A` | 0 | 10-fresh-install-A.cmd.txt |
| 12 | `--uninstall D:\Core-build\s5-installer\target-fresh --no-shortcuts` | 0 | 12-fresh-uninstall.cmd.txt |
| 14 | `--install D:\Core-build\s5-installer\target-evidence --no-shortcuts --samples D:\Core-build\s5-installer\library-A` | 0 | 14-evidence-install.cmd.txt |
| 16 | `--uninstall D:\Core-build\s5-installer\target-evidence --no-shortcuts` | 0 | 16-evidence-uninstall.cmd.txt |

Mutations were applied by `tools\mutate-main.ps1` (record: `logs\03-mutations.txt`), `tools\setup-fresh.ps1` (`logs\09-fresh-preinstall.txt`), and an inline snippet for target-evidence (`logs\15-evidence-pre-uninstall.txt`). Snapshots: `tools\snapshot.ps1` → logs 02, 04, 06, 08, 11, 13, 15, 17.

## 4. Full hash matrix (SHA-256, confirmed by execution)

### 4.0 Distributed source `library-A` (synthetic fixtures; all mtime 2026-11-01T10:00:00.000) — logs\00
| rel | bytes | sha256 |
|---|---|---|
| Bass\bass-a.wav | 6144 | `8f272eb8c67a2dc7edec91433293f133f1174bb2bf0190a90422a028badc9e6e` |
| Drums\kick-a.wav | 4096 | `b294a261901b161b79963488004a6c15f0a811a327ae085fc5fc2461ff04ead1` |
| Drums\snare-a.wav | 5120 | `a2781ffcc655b495402087979e45e9c489fa81738c1c30fb2793c10c7be7c2e2` |
| Keys\keys-a.wav | 4608 | `831fe10beae0477bde590dd9e3507b0551c97f3b0143540706862acb984c2626` |
| _core-plan\plan.txt | 71 | `8b253f75fd80ba515c7ad129fca79f3d2281712af6941a00e162bab2322941cc` (skipped, never copied — Setup.cpp:63) |

### 4.1 target-main — install A (logs\02)
All four samples present with the source hashes and source mtimes above; `samples.txt` = the 4 lines of §1.2; `current.txt` = `0.7.0`; `app-0.7.0\Core.exe` present, app manifest 50 lines; `Uninstall.exe` copied (Setup.cpp:196). No `previous.txt` (first install, :190).

### 4.2 target-main — user mutations (logs\03, 04)
| rel | bytes | mtime | sha256 | mutation |
|---|---|---|---|---|
| Drums\kick-a.wav | 4352 | 2026-11-20T00:15:17.790 | `fac4d3718e795fce4249c997720fb9cd163e124a664f6d40af6447c6aa72eebb` | (i) appended 256 B user edit |
| Drums\snare-a.wav | 3000 | 2026-11-20T00:15:17.793 | `8d19a29538480795d07ba96201a282ad4daa379a47321be367ce8e436e22e38f` | (ii) replaced with user-owned content |
| Bass\bass-a.wav | 6144 | 2026-11-01T10:00:00.000 | `8f272eb8…` (unchanged) | (iii) intact |
| Keys\keys-a.wav | 4608 | 2026-11-01T10:00:00.000 | `539794f7cb92856bd1e26e84a3603d9da2fe8fd419c7761c05d7b42ee1481e53` | (iv) user-owned content, same size, mtime restored to source value; path already listed |

`samples.txt` unchanged (4 lines).

### 4.3 target-main — after reinstall A (logs\06), exit 0
| rel | bytes | mtime | sha256 | outcome / code path |
|---|---|---|---|---|
| Drums\kick-a.wav | 4096 | 2026-11-01T10:00:00.000 | `b294a261…` = **distributed** | **OVERWRITTEN** — `same=false` (4352≠4096), Setup.cpp:89→90–91; user edit `fac4d371…` destroyed |
| Drums\snare-a.wav | 5120 | 2026-11-01T10:00:00.000 | `a2781ffc…` = **distributed** | **OVERWRITTEN** — `same=false` (3000≠5120, date differs), Setup.cpp:89→90–91; user file `8d19a295…` destroyed |
| Bass\bass-a.wav | 6144 | 2026-11-01T10:00:00.000 | `8f272eb8…` unchanged | skipped — `same=true`, Setup.cpp:89; stays listed via :93 |
| Keys\keys-a.wav | 4608 | 2026-11-01T10:00:00.000 | `539794f7…` = **user content kept** | copy skipped — `same=true`; **stays listed** via Setup.cpp:93 (`listed.contains`) → still marked for removal |

`samples.txt` after: identical 4 lines (Setup.cpp:97–99 union+dedup). `current.txt` still `0.7.0`; no `previous.txt` (same version, :190).

### 4.4 target-main — after uninstall (logs\08), exit 0
**Target folder absent entirely.** All four listed samples deleted by `removeSamples` (Setup.cpp:108–111) — including `Keys\keys-a.wav` whose content at uninstall was the **user-owned** `539794f7…` (never re-copied, still listed). Empty dirs removed (:113–117), `samples.txt` deleted (:119), app folder via app manifest (:152–166), `current.txt`/`Uninstall.exe` via `isOurs` (:219–223, :244), folder deleted when empty (:250–251). **RED: user content deleted via the manifest path.**

### 4.5 target-fresh — contrast (logs\09, 11, 13), installs/uninstalls exit 0
Pre-placed before first install: `Samples\Drums\snare-a.wav` 5120 B, mtime 2026-11-01T10:00:00.000, user hash `26318c486d0ebca10ca7d0370fd75520f85fe171a75677787050e86b235bea6f`; `Samples\MyRecordings\user-take.wav` 2048 B, `05957142b7655f7d583af24be21872f7fafe9473affcaa8812fe42343c5346db`.
- After install A: snare **unchanged and NOT listed** (`samples.txt` = Bass/bass-a.wav, Drums/kick-a.wav, Keys/keys-a.wav only) — `same=true` + empty `listed` → neither copied nor claimed (Setup.cpp:89, 93; Audit-27 comment :76–80). user-take untouched (non-distributed path).
- After uninstall: **snare survives** (`26318c48…`, same mtime) and **user-take survives** (`05957142…`); the 3 listed distributed samples, `samples.txt`, app folder, `current.txt`, `Uninstall.exe` all removed; `Samples\` and the target folder remain because user content is left (Setup.cpp:113–117 only removes *empty* dirs; :238 `othersLeft`). GREEN — the existing protection's exact boundary.

### 4.6 target-evidence — per-file isolation of the manifest red (logs\15, 17), exits 0
After install A: replaced `Keys\keys-a.wav` with user content `539794f7cb92856bd1e26e84a3603d9da2fe8fd419c7761c05d7b42ee1481e53` (4608 B, mtime restored to source) — listed; added non-listed `MyRecordings\user-take.wav` 2048 B `56087d2def1ec94d87dd52356fb7672e7e2edb9e8b2b6586503cee390788f65d`. No reinstall.
After uninstall: **`Keys\keys-a.wav` GONE** (user content deleted solely because its path was listed — Setup.cpp:108–111), **`user-take.wav` survives byte-identical** (`56087d2d…`), `samples.txt` gone, only `Samples\MyRecordings\` remains. This isolates the manifest-side defect from the copy-side defect in a single run.

## 5. Confirmed-by-execution vs read-only inference

**Confirmed by execution:** every hash/size/mtime/manifest value in §4; all exit codes in §3; no-elevation temp-target install/uninstall; `_core-plan` skip; CopyFile mtime preservation; Audit-27 never-listed protection (case v); non-distributed-path safety (case vi); both reds (copy overwrite: i, ii; manifest deletion of user content: iv, vii).

**Read-only inference (not executed):** GUI window path hits the same `copySamples` (Setup.cpp:276 vs :383); shortcut/registry integration behaviour (skipped via `--no-shortcuts`); running-from-target self-delete branch (Setup.cpp:247–249); behaviour at real-library scale (~7 GB) and with real user audio; version-B update path (same :89–93 branch by code reading).

## 6. Notes for the ledger scribe (proposed lines, not written to ESTADO.md)

- Proposed BUG- entry (namespace per D75): "Core Setup `copySamples` overwrites user-edited/user-owned samples at distributed paths (size/date-only decision, Setup.cpp:89–91) and `removeSamples` deletes previously-listed paths holding user content at uninstall (Setup.cpp:93, 108–111). Red measured by execution at a9a0384 with the real silent installer in temp targets; evidence D:\Core-build\s5-installer\logs\00–17, matrix D:\Core-build\coordinator-round2\s5-notes.md. DATA priority (owner order: data > real time > performance > compatibility)."
- No repository file was changed by this measurement; the uncommitted `CMakeLists.txt` sanitizer wiring was preserved.

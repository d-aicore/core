# run-all.ps1 - Core: environment for code review, debug, sanitization and JUCE UI/UX work.
# Prepared 2026-11-19; validation hardened 2026-11-19 (stage 0.2): every .done marker is bound to
#   fp1 sha=<HEAD> local=<hash of tracked build-affecting diff> juce=<dep HEAD/diff hash> script=<this file hash> cfg=<configuration>
# A step is reused ONLY when that whole fingerprint matches. Changing the revision, a tracked
# source/CMake change (docs excluded), the vendored JUCE state, the script, or the step's
# configuration invalidates the affected steps. Legacy markers without fp1 are always re-run.
# Environment/tooling only: no product, DSP or UI code is modified.
# Conventions from ci/gate.ps1: vcvars64 from a unique .bat, VSLANG=1033 + chcp 65001 (Ninja header
# dependency tracking), out-of-tree builds, ctest -L merge, canaries MUST fail before a green suite
# is trusted (D92/D93). Writes only under $Root; the source tree is never modified. Logs: $Root\NN-*.txt.
# Runs under Windows PowerShell 5.1 (no PS7-only syntax).
param(
  [string]$Src = "D:\CORE\CORE",
  [string]$Root = "D:\Core-build\core-review",
  [string]$Juce = "D:\Core-deps\JUCE-9.0.5",
  [string]$Node = "C:\Users\vk_do\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\node\bin\node.exe",
  [string]$Clangd = "C:\Users\vk_do\AppData\Local\Programs\clangd_23.1.0\bin\clangd.exe",
  [string]$ProbeDir = "",
  [int]$Jobs = 8,
  [string]$Only = "",
  [string]$AsanFlags = "address",
  [string]$AsanTests = "ON"
)
$ErrorActionPreference = "Continue"
$NL = [string][char]13 + [string][char]10
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if ($ProbeDir -eq "") { $ProbeDir = Join-Path $PSScriptRoot "probe" }
New-Item -ItemType Directory -Force -Path $Root | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Root "probe") | Out-Null

# ---------- context fingerprint: SHA + local changes that enter the build + JUCE state + script ----------
# Untracked files do not enter this build (CMake lists every source explicitly); tracked modifications
# do, so they are hashed. docs/ never enters the build and is excluded. The vendored JUCE patch lives
# as a working-tree modification in $Juce (D113), so its HEAD and diff are part of the context too.
function HashFile([string]$p) { if (Test-Path $p) { return (Get-FileHash $p -Algorithm SHA256).Hash } else { return "MISSING" } }
$SrcSha = (git -C $Src rev-parse HEAD 2>$null)
if (-not $SrcSha) { Write-Output "FATAL: cannot read HEAD of $Src"; exit 1 }
$tmpDiff = Join-Path $env:TEMP ("core-review-ctx-" + [guid]::NewGuid().ToString("N") + ".diff")
git -C $Src diff --output=$tmpDiff -- . ":(exclude)docs" 2>$null
$LocalHash = HashFile $tmpDiff
Remove-Item -Force -ErrorAction SilentlyContinue $tmpDiff
$JuceSha = (git -C $Juce rev-parse HEAD 2>$null)
if ($JuceSha) {
  $tmpJuce = Join-Path $env:TEMP ("core-review-ctx-" + [guid]::NewGuid().ToString("N") + ".diff")
  git -C $Juce diff --output=$tmpJuce 2>$null
  $JuceHash = HashFile $tmpJuce
  Remove-Item -Force -ErrorAction SilentlyContinue $tmpJuce
} else {
  # The pinned JUCE is an extracted tree (no .git). The vendored crash patch (D113, tracked at
  # ci/patches/juce-failed-plugin-restart.patch) lives as file modifications there; bind the
  # fingerprint to the patch file AND every file it touches, so re-fetching or reverting JUCE
  # invalidates the affected steps (the false-green risk recorded in ESTADO.md).
  $JuceSha = "no-git"
  $patchFile = Join-Path $Src "ci\patches\juce-failed-plugin-restart.patch"
  $parts = @()
  if (Test-Path $patchFile) {
    $parts += (Get-FileHash $patchFile -Algorithm SHA256).Hash
    $touched = @(Select-String -Path $patchFile -Pattern '^diff --git a/(\S+) b/' | ForEach-Object { $_.Matches[0].Groups[1].Value })
    foreach ($rel in $touched) {
      $f = Join-Path $Juce ($rel -replace '/', '\')
      if (Test-Path $f) { $parts += (Get-FileHash $f -Algorithm SHA256).Hash } else { $parts += ("MISSING:" + $rel) }
    }
  } else { $parts += "NO-PATCH-FILE" }
  $tmpJuce = Join-Path $env:TEMP ("core-review-ctx-" + [guid]::NewGuid().ToString("N") + ".juce")
  [System.IO.File]::WriteAllText($tmpJuce, ($parts -join "`n"))
  $JuceHash = HashFile $tmpJuce
  Remove-Item -Force -ErrorAction SilentlyContinue $tmpJuce
}
$ScriptHash = HashFile $PSCommandPath
$Context = "fp1 sha=" + $SrcSha + " local=" + $LocalHash + " juce=" + $JuceSha + "/" + $JuceHash + " script=" + $ScriptHash
Write-Output ("context: " + $Context)

# RunStep exit codes: 0 = ran and passed, 1 = ran and failed, 2 = skipped (filter or fingerprint reuse).
# A skipped step must never be mistaken for a passing one (false green).
function Q([string]$s) { return ('"' + $s + '"') }
function Fwd([string]$s) { return ($s -replace "\\", "/") }
function FP([string]$cfg) { return ($Context + " cfg=" + $cfg) }
function StepDone([string]$n, [string]$fp) {
  $p = Join-Path $Root ($n + ".done")
  if (-not (Test-Path $p)) { return $false }
  $first = @(Get-Content $p -TotalCount 1)[0]
  return ($first -eq $fp)
}
# MarkDone uses the fingerprint of the RunStep that just ran (callers mark immediately after it).
function MarkDone([string]$n, [string]$why) {
  Set-Content -Encoding UTF8 -Path (Join-Path $Root ($n + ".done")) -Value (($script:LastFp + $NL + $why))
}
function RunStep([string]$n, [string[]]$lines, [string]$cfg) {
  if ($Only -ne "" -and $n -notlike $Only) { Write-Host ("SKIP(filter) " + $n); return 2 }
  $fp = FP $cfg
  $script:LastFp = $fp
  if (StepDone $n $fp) { Write-Host ("SKIP(reuse) " + $n); return 2 }
  if (Test-Path (Join-Path $Root ($n + ".done"))) {
    Write-Host ("INVALIDATED " + $n + " - recorded result belongs to a different SHA/local-change/JUCE/script/config")
  }
  Write-Host ("RUN " + $n)
  $bat = Join-Path $env:TEMP ("core-review-" + [guid]::NewGuid().ToString("N") + ".bat")
  $log = Join-Path $Root ($n + ".txt")
  $head = @("@echo off", "set VSLANG=1033", "chcp 65001 >nul", ("call " + (Q $vcvars) + " >nul 2>&1"))
  Set-Content -Encoding ASCII -Path $bat -Value (($head + $lines) -join $NL)
  $out = & cmd.exe /c ((Q $bat) + " 2>&1")
  $rc = $LASTEXITCODE
  $out | Set-Content -Encoding UTF8 -Path $log
  Remove-Item -Force -ErrorAction SilentlyContinue $bat
  Write-Host ("RC " + $n + " = " + $rc + "   log=" + $log)
  return $rc
}

# ---------- 00 environment record: branch, SHA, local changes, toolchain ----------
$rc = RunStep "00-env" @(
  "echo === git branch / SHA / local changes ===",
  ("git -C " + (Q $Src) + " rev-parse --abbrev-ref HEAD"),
  ("git -C " + (Q $Src) + " rev-parse HEAD"),
  ("git -C " + (Q $Src) + " status --porcelain=v1 -uno"),
  ("git -C " + (Q $Src) + " stash list"),
  "echo === build-affecting local diff (docs excluded) ===",
  ("git -C " + (Q $Src) + " diff --stat -- . " + (Q ":(exclude)docs")),
  "echo === vendored JUCE state ===",
  ("git -C " + (Q $Juce) + " rev-parse HEAD"),
  ("git -C " + (Q $Juce) + " diff --stat"),
  "echo === toolchain ===",
  "cl 2>&1 | findstr Version",
  "cmake --version",
  "ninja --version",
  ((Q $Clangd) + " --version"),
  "echo === host ===",
  "ver"
) "env"
if ($rc -eq 0) { MarkDone "00-env" ("rc=" + $rc) }

# ---------- 01 Debug configure of a normal Core build (app + JUCE + tests) + compile db ----------
$dbg = Join-Path $Root "debug"
$juceDef = "-DFETCHCONTENT_SOURCE_DIR_JUCE=" + (Fwd $Juce)
$cfgLine = "cmake -S " + (Q $Src) + " -B " + (Q $dbg) + " -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCORE_BUILD_TESTS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON " + (Q $juceDef)
$rc = RunStep "01-configure-debug" @($cfgLine) "debug"
$cc = Join-Path $dbg "compile_commands.json"
if ($rc -eq 0 -and (Test-Path $cc)) {
  Copy-Item $cc (Join-Path $Root "compile_commands-debug.json") -Force
  MarkDone "01-configure-debug" ("rc=" + $rc + "; compile db exported and copied to compile_commands-debug.json")
}

# ---------- 02 Debug build (out of the source tree; includes the JUCE app for UI/UX work) ----------
$rc = RunStep "02-build-debug" @("cmake --build " + (Q $dbg) + " --parallel " + $Jobs) "debug"
if ($rc -eq 0) { MarkDone "02-build-debug" ("rc=" + $rc) }
Get-ChildItem $dbg -Recurse -Filter *.pdb -ErrorAction SilentlyContinue |
  ForEach-Object { $_.FullName + [char]9 + $_.Length } | Set-Content -Encoding UTF8 (Join-Path $Root "02b-pdbs.txt")

# ---------- 03 ctest (merge) on the Debug build, verbose for complete logs ----------
# engine_process_tests is excluded from the green run and measured separately by 03b: in Debug it fails
# with "The engine exited (code 259)" (STILL_ACTIVE, the client waits only 2 s in the disconnect path)
# while the same test passes in Release (ENV-03, re-measured at a9a0384 on 2026-11-19; stage 3A owns it).
$rc = RunStep "03-ctest-debug" @("ctest --test-dir " + (Q $dbg) + " -L merge -V --timeout 600 --no-tests=error -E engine_process_tests") "debug"
if ($rc -eq 0) { MarkDone "03-ctest-debug" ("rc=" + $rc + "; engine_process_tests excluded (Debug-only 259, see 03b)") }

# ---------- 03b engine_process_tests: characterise the one merge test that fails in Debug ----------
$ept = Join-Path $dbg "tests\engine_process_tests.exe"
if ((Test-Path $ept) -and -not (StepDone "03b-engine-process" (FP "debug"))) {
  $cl4 = @()
  for ($i = 1; $i -le 3; $i++) {
    $runLog = Join-Path $Root ("03b-engine-process-run" + $i + ".txt")
    $cl4 += ((Q $ept) + " > " + (Q $runLog) + " 2>&1")
    $cl4 += ("echo ENGINE_PROCESS_RUN" + $i + "_EXIT=%ERRORLEVEL%")
  }
  $rc = RunStep "03b-engine-process" $cl4 "debug"
  $log3b = Join-Path $Root "03b-engine-process.txt"
  $codes = @(Select-String -Path $log3b -Pattern "ENGINE_PROCESS_RUN[123]_EXIT=[0-9]+" -ErrorAction SilentlyContinue | ForEach-Object { $_.Line.Trim() })
  $codes | Set-Content -Encoding UTF8 (Join-Path $Root "03b-engine-process-summary.txt")
  Write-Host ("03b engine_process_tests: " + ($codes -join " | "))
  if ($codes.Count -ge 3) { MarkDone "03b-engine-process" (($codes -join " | ")) }
}

# ---------- 04 debugger symbols: stack probe linked against the real core_engine_lib ----------
if (-not (StepDone "04-symbols" (FP "debug"))) {
  $probeSrc = Join-Path $ProbeDir "stack_probe.cpp"
  $probeCopy = Join-Path $Root "probe\stack_probe.cpp"
  if ((Test-Path $probeSrc) -and ($probeSrc -ne $probeCopy)) { Copy-Item $probeSrc $probeCopy -Force }
  $lib = Get-ChildItem $dbg -Recurse -Filter core_engine_lib.lib -ErrorAction SilentlyContinue | Select-Object -First 1
  $exe = Join-Path $Root "probe\stack_probe.exe"
  if ($lib -and (Test-Path $probeCopy)) {
    # /Fo takes a full file name: a trailing backslash before the closing quote escapes it in cmd.
    $obj = Join-Path $Root "probe\stack_probe.obj"
    $pdb = Join-Path $Root "probe\stack_probe.pdb"
    $clLine = "cl /nologo /MTd /Zi /Fd" + (Q $pdb) + " /EHsc /utf-8 /W3 /I " + (Q ($Src + "\engine\src")) + " " + (Q $probeCopy) + " /Fo" + (Q $obj) + " /Fe:" + (Q $exe) + " /link /DEBUG " + (Q $lib.FullName) + " dbghelp.lib"
    $rc = RunStep "04-symbols" @($clLine, (Q $exe)) "debug"
    $symlog = Join-Path $Root "04-symbols.txt"
    if (Select-String -Path $symlog -Pattern "tempo_map.cpp:" -Quiet -ErrorAction SilentlyContinue) {
      MarkDone "04-symbols" "stack frames resolved to engine/src/core/tempo_map.cpp:<line> from the Debug build PDBs"
    } else { Write-Output "WARN 04-symbols: no tempo_map.cpp:<line> frame in 04-symbols.txt" }
  } else { Write-Output "SKIP 04-symbols (core_engine_lib.lib or stack_probe.cpp missing)" }
} else { Write-Output "SKIP(reuse) 04-symbols" }

# ---------- 05/06 ASan (MSVC): CORE_SANITIZE=$AsanFlags, app OFF (engine + domain + tests) ----------
$asan = Join-Path $Root "asan"
$asanCfg = "asan-" + $AsanFlags + "-tests" + $AsanTests
$asanCfgLine = "cmake -S " + (Q $Src) + " -B " + (Q $asan) + " -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCORE_BUILD_TESTS=" + $AsanTests + " -DCORE_BUILD_APP=OFF -DCORE_SANITIZE=" + $AsanFlags + " -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
$rc = RunStep "05-configure-asan" @($asanCfgLine) $asanCfg
$ccA = Join-Path $asan "compile_commands.json"
if ($rc -eq 0 -and (Test-Path $ccA)) {
  Copy-Item $ccA (Join-Path $Root "compile_commands-asan.json") -Force
  MarkDone "05-configure-asan" ("rc=" + $rc)
}
function Get-AsanFailedTargets {
  $log = Join-Path $Root "06-build-asan.txt"
  if (-not (Test-Path $log)) { return @() }
  return @(Select-String -Path $log -Pattern "FAILED: \[code=" -ErrorAction SilentlyContinue | ForEach-Object {
    $m = [regex]::Match($_.Line, '([A-Za-z0-9_]+)\.(exe|lib)\s*$')
    if ($m.Success) { $m.Groups[1].Value }
  } | Select-Object -Unique)
}
$rc = RunStep "06-build-asan" @("cmake --build " + (Q $asan) + " --parallel " + $Jobs + " -- -k 0") $asanCfg
# Strict since the ENV-01 wiring fix (legacy_guard_tests/menu_tests at 8c6837e, extended with
# modules_tests/window_tests at a9a0384): every target must build. A link failure here means a new
# target links instrumented libraries without being in CORE_SANITIZED_TARGETS - record, do not tolerate.
$failedTargets = Get-AsanFailedTargets
if ($failedTargets.Count -gt 0) {
  @(
    ("targets that failed to build in the ASan tree: " + ($failedTargets -join ", ")),
    "",
    "likely root cause (LNK2038): a target links instrumented libraries but is not in CORE_SANITIZED_TARGETS",
    "(CMakeLists.txt). Instrumented objects carry annotate_string/annotate_vector/annotate_optional = 1;",
    "the uninstrumented object has 0, so the linker refuses the mix. Build configuration fix only."
  ) | Set-Content -Encoding UTF8 (Join-Path $Root "06b-asan-wiring-gap.txt")
  Write-Host ("06b ASan wiring gap recorded: " + ($failedTargets -join ", "))
}
if ($rc -eq 0 -and $failedTargets.Count -eq 0) { MarkDone "06-build-asan" ("rc=" + $rc) }

# ---------- 07 ASan canary must trip 3/3 (exit != 0 + report) ----------
$canaryExe = Join-Path $asan "tests\asan_canary.exe"
if ((Test-Path $canaryExe) -and -not (StepDone "07-asan-canary" (FP $asanCfg))) {
  $cl2 = @()
  for ($i = 1; $i -le 3; $i++) {
    $cl2 += ((Q $canaryExe) + " 2>&1")
    $cl2 += ("echo CANARY_RUN" + $i + "_EXIT=%ERRORLEVEL%")
  }
  $rc = RunStep "07-asan-canary" $cl2 $asanCfg
  $log7 = Join-Path $Root "07-asan-canary.txt"
  $exits = @(Select-String -Path $log7 -Pattern "CANARY_RUN[123]_EXIT=1" -ErrorAction SilentlyContinue).Count
  $hasReport = Select-String -Path $log7 -Pattern "ERROR: AddressSanitizer: heap-use-after-free" -Quiet -ErrorAction SilentlyContinue
  $hasLine = Select-String -Path $log7 -Pattern "canary.cpp:10" -Quiet -ErrorAction SilentlyContinue
  if ($exits -eq 3 -and $hasReport -and $hasLine) {
    MarkDone "07-asan-canary" ("3/3 runs exit=1; heap-use-after-free at tests/canary.cpp:10; step rc=" + $rc)
  } else { Write-Output ("WARN 07-asan-canary: exits=" + $exits + " report=" + $hasReport + " line=" + $hasLine) }
}

# ---------- 07b proof that the sanitizer flags are really in the compile db ----------
$ccJson = Join-Path $Root "compile_commands-asan.json"
if (Test-Path $ccJson) {
  try {
    # NB: ConvertFrom-Json emits a JSON array as ONE pipeline item, so never wrap the pipeline in @();
    # assign first, then let the pipeline enumerate the array.
    $db = (Get-Content $ccJson -Raw | ConvertFrom-Json)
    $db = @($db | ForEach-Object { $_ })
    $inst = @($db | Where-Object { $_.command -match "/fsanitize=address" })
    $miss = @($db | Where-Object { $_.command -notmatch "/fsanitize=address" } | ForEach-Object { Split-Path $_.file -Leaf })
    @(
      ("compile db: " + $ccJson),
      ("total translation units: " + $db.Count),
      ("with /fsanitize=address: " + $inst.Count),
      ("without the flag: " + ($miss -join ", "))
    ) | Set-Content -Encoding UTF8 (Join-Path $Root "07b-asan-flags.txt")
    Write-Output ("07b asan flags: " + $inst.Count + "/" + $db.Count + " translation units instrumented")
  } catch { Write-Output ("WARN 07b: " + $_.Exception.Message) }
}

# ---------- 08 ASan ctest (merge, verbose) + sanitizer report counters ----------
$rc = RunStep "08-asan-ctest" @("ctest --test-dir " + (Q $asan) + " -L merge -V --timeout 900 --no-tests=error") $asanCfg
if ($rc -eq 0) { MarkDone "08-asan-ctest" ("rc=" + $rc) }
$log8 = Join-Path $Root "08-asan-ctest.txt"
if (Test-Path $log8) {
  $o = @("ctest result lines:")
  $o += @(Select-String -Path $log8 -Pattern "tests passed|tests failed|Total Test time" -ErrorAction SilentlyContinue | ForEach-Object { $_.Line })
  $o += ""
  $o += "sanitizer report counters (all must be 0 for a clean suite):"
  foreach ($pat in @("ERROR: AddressSanitizer", "runtime error:", "LeakSanitizer", "ThreadSanitizer", "FATAL")) {
    $c = @(Select-String -Path $log8 -SimpleMatch $pat -ErrorAction SilentlyContinue).Count
    $o += ("  " + $pat + " = " + $c)
  }
  $o | Set-Content -Encoding UTF8 (Join-Path $Root "08b-asan-summary.txt")
}

# ---------- 09 clangd smoke test: cross-file definition + references on the real compile db ----------
$smoke = Join-Path $ProbeDir "lsp-smoke.mjs"
if ((Test-Path $smoke) -and -not (StepDone "09-lsp-smoke" (FP "debug"))) {
  $smokeCopy = Join-Path $Root "probe\lsp-smoke.mjs"
  if ($smoke -ne $smokeCopy) { Copy-Item $smoke $smokeCopy -Force }
  # Emulate the app's environment for clangd: the dsh-cpp-lsp provider does NOT inherit vcvars,
  # so no INCLUDE/LIB; clangd must find the MSVC system headers through --query-driver=cl.exe.
  $queryDriver = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe"
  $rc = RunStep "09-lsp-smoke" @(
    "set ""INCLUDE=""",
    "set ""LIB=""",
    ("set ""CLANGD_QUERY_DRIVER=" + $queryDriver + """"),
    ((Q $Node) + " " + (Q $smokeCopy) + " --clangd " + (Q $Clangd) + " --ccdir " + (Q $dbg) + " --src " + (Q $Src))
  ) "debug"
  if ($rc -eq 0) { MarkDone "09-lsp-smoke" "clangd resolved cross-file go-to-definition and find-references from compile_commands-debug.json" }
}

# ---------- 10 JUCE UI/UX path: the app self-test opens the window and writes a screenshot ----------
# Tooling proof for design work (docs/plan/ux-pulse-spec.md): CoreApp --self-test on the null device.
# It is a test run, not a product change. Screenshot: <debug>\tests\self-test.png
$rc = RunStep "10-desktop-selftest" @("ctest --test-dir " + (Q $dbg) + " -L desktop -V --timeout 600 --no-tests=error") "debug"
if ($rc -eq 0) { MarkDone "10-desktop-selftest" ("rc=" + $rc + "; JUCE window opened and self-test.png written") }

# ---------- 99 summary ----------
$marks = @(Get-ChildItem $Root -Filter "*.done" -ErrorAction SilentlyContinue | Sort-Object Name | ForEach-Object { $_.BaseName })
@(
  "Core review environment - Windows/MSVC run (run-all.ps1, SHA-bound validation)",
  ("src:  " + $Src + " @ " + $SrcSha),
  ("root: " + $Root),
  ("context: " + $Context),
  ("steps done: " + ($marks -join ", "))
) | Set-Content -Encoding UTF8 (Join-Path $Root "99-summary.txt")
Get-Content (Join-Path $Root "99-summary.txt")
Write-Output "run-all.ps1 finished"

# verify.ps1 - read-only health check of the Core review/debug/sanitize environment.
# Prepared 2026-11-19. Prints OK/MISSING per item; exits 0 only when every core item is present.
# It never builds and never writes into the source tree. The ASan canary is expected to FAIL with a
# report - that failure is the proof that the build is instrumented.
param(
  [string]$Src = 'D:\CORE\CORE',
  [string]$Root = 'D:\Core-build\core-review',
  [switch]$Lsp
)
$ErrorActionPreference = 'Continue'
function Q([string]$s) { return ('"' + $s + '"') }
$bad = 0
function Check([string]$name, [bool]$ok, [string]$detail) {
  if ($ok) { Write-Host ('OK      ' + $name + '  ' + $detail) } else { Write-Host ('MISSING ' + $name + '  ' + $detail); $script:bad++ }
}
$vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat'

$sha = (git -C $Src rev-parse --short HEAD 2>$null)
$dirty = @(git -C $Src status --porcelain=v1 2>$null | Where-Object { $_ -notmatch '^\?\?' })
Check 'source tree' ($null -ne $sha) ('HEAD=' + $sha + '; modified tracked files=' + $dirty.Count + ' (CMakeLists.txt carries the sanitizer wiring fix)')

# SHA-bound marker freshness (stage 0.2, 2026-11-19): the recorded pipeline context must match the
# current tree, else the .done markers describe a DIFFERENT revision/local state and a full re-run
# is required. This is the same fingerprint run-all.ps1 binds its markers to (sha + build-affecting
# local diff + vendored JUCE state + script version). Read-only: nothing is built here.
$fullSha = (git -C $Src rev-parse HEAD 2>$null)
$tmpD = Join-Path $env:TEMP ('core-verify-ctx-' + [guid]::NewGuid().ToString('N') + '.diff')
git -C $Src diff --output=$tmpD -- . ':(exclude)docs' 2>$null
$lh = 'MISSING'; if (Test-Path $tmpD) { $lh = (Get-FileHash $tmpD -Algorithm SHA256).Hash }
Remove-Item -Force -ErrorAction SilentlyContinue $tmpD
$juceDir = 'D:\Core-deps\JUCE-9.0.5'
$jsha = (git -C $juceDir rev-parse HEAD 2>$null)
if ($jsha) {
  $tmpJ = Join-Path $env:TEMP ('core-verify-ctx-' + [guid]::NewGuid().ToString('N') + '.diff')
  git -C $juceDir diff --output=$tmpJ 2>$null
  $jh = 'MISSING'; if (Test-Path $tmpJ) { $jh = (Get-FileHash $tmpJ -Algorithm SHA256).Hash }
  Remove-Item -Force -ErrorAction SilentlyContinue $tmpJ
} else {
  # Extracted JUCE tree (no .git): same binding as run-all.ps1 - patch file + every file it touches.
  $jsha = 'no-git'
  $patchFile = Join-Path $Src 'ci\patches\juce-failed-plugin-restart.patch'
  $parts = @()
  if (Test-Path $patchFile) {
    $parts += (Get-FileHash $patchFile -Algorithm SHA256).Hash
    $touched = @(Select-String -Path $patchFile -Pattern '^diff --git a/(\S+) b/' | ForEach-Object { $_.Matches[0].Groups[1].Value })
    foreach ($rel in $touched) {
      $f = Join-Path $juceDir ($rel -replace '/', '\')
      if (Test-Path $f) { $parts += (Get-FileHash $f -Algorithm SHA256).Hash } else { $parts += ('MISSING:' + $rel) }
    }
  } else { $parts += 'NO-PATCH-FILE' }
  $tmpJ = Join-Path $env:TEMP ('core-verify-ctx-' + [guid]::NewGuid().ToString('N') + '.juce')
  [System.IO.File]::WriteAllText($tmpJ, ($parts -join "`n"))
  $jh = (Get-FileHash $tmpJ -Algorithm SHA256).Hash
  Remove-Item -Force -ErrorAction SilentlyContinue $tmpJ
}
$runAll = Join-Path $Root 'run-all.ps1'
$sh = 'MISSING'; if (Test-Path $runAll) { $sh = (Get-FileHash $runAll -Algorithm SHA256).Hash }
$expectedCtx = 'fp1 sha=' + $fullSha + ' local=' + $lh + ' juce=' + $jsha + '/' + $jh + ' script=' + $sh
$envMarker = Join-Path $Root '00-env.done'
$recCtx = ''; if (Test-Path $envMarker) { $recCtx = [string](@(Get-Content $envMarker -TotalCount 1)[0]) }
$fresh = $recCtx.StartsWith($expectedCtx)
$recSha = 'none'; if ($recCtx -match 'sha=([0-9a-f]{40})') { $recSha = $Matches[1].Substring(0, 7) }
Check 'marker freshness' $fresh ('recorded sha=' + $recSha + ' vs HEAD=' + $sha + '; full fingerprint (SHA+local diff+JUCE+script) ' + $(if ($fresh) { 'matches' } else { 'DIFFERS - re-run run-all.ps1 / run-wsl.sh' }))

$dbg = Join-Path $Root 'debug'
$cc = Join-Path $dbg 'compile_commands.json'
$tus = 0
if (Test-Path $cc) { $tus = @((Get-Content $cc -Raw | ConvertFrom-Json) | ForEach-Object { $_ }).Count }
Check 'compile database' ($tus -gt 400) ('' + $tus + ' translation units')
Check 'Debug build' (Test-Path (Join-Path $dbg 'tests\engine_tests.exe')) 'tests/engine_tests.exe'
Check 'JUCE app' (Test-Path (Join-Path $dbg 'app\desktop\CoreApp_artefacts\Debug\Core.exe')) 'Core.exe (Debug)'
Check 'PDBs' (@(Get-ChildItem $dbg -Recurse -Filter *.pdb -ErrorAction SilentlyContinue).Count -gt 10) 'debug symbols present'
Check 'symbols probe' (Test-Path (Join-Path $Root 'probe\stack_probe.exe')) 'probe/stack_probe.exe'

$asan = Join-Path $Root 'asan'
$canary = Join-Path $asan 'tests\asan_canary.exe'
Check 'ASan build' (Test-Path $canary) 'asan/tests/asan_canary.exe'
if (Test-Path $canary) {
  $bat = Join-Path $env:TEMP ('core-verify-' + [guid]::NewGuid().ToString('N') + '.bat')
  $log = Join-Path $env:TEMP ('core-verify-' + [guid]::NewGuid().ToString('N') + '.txt')
  $lines = @('@echo off', 'set VSLANG=1033', ('call ' + (Q $vcvars) + ' >nul 2>&1'), ((Q $canary) + ' 2>&1'))
  Set-Content -Encoding ASCII -Path $bat -Value ($lines -join ([string][char]13 + [string][char]10))
  $null = & cmd.exe /c ((Q $bat) + ' > ' + (Q $log) + ' 2>&1')
  $rc = $LASTEXITCODE
  $rep = (Select-String -Path $log -Pattern 'AddressSanitizer: heap-use-after-free' -Quiet -ErrorAction SilentlyContinue)
  Check 'ASan canary' (($rc -ne 0) -and $rep) ('exit=' + $rc + ' report=' + $rep + ' (both must be true)')
  Remove-Item -Force -ErrorAction SilentlyContinue $bat, $log
}

$marks = @(Get-ChildItem $Root -Filter '*.done' -ErrorAction SilentlyContinue | Sort-Object Name | ForEach-Object { $_.BaseName })
Check 'pipeline steps' ($marks.Count -gt 0) ($marks -join ', ')

$wsl = (& wsl.exe -e bash -lc 'command -v gcc clang-tidy cmake ninja >/dev/null && echo WSL_OK; command -v gdb >/dev/null && echo GDB || echo NO_GDB' 2>$null) -join ' '
Check 'WSL toolchain' ($wsl -match 'WSL_OK') ('gcc/clang-tidy/cmake/ninja; gdb=' + ($wsl -match '(^|\s)GDB(\s|$)') + ' (known: no gdb and no network in this WSL)')
$wslBuilds = ((& wsl.exe -e bash -lc 'ls -d $HOME/core-review-build/*/ 2>/dev/null | wc -l' 2>$null) -join '').Trim()
Check 'WSL builds' ($wslBuilds -match '[1-9]') ('build directories: ' + $wslBuilds)

if ($Lsp) {
  $node = 'C:\Users\vk_do\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\node\bin\node.exe'
  $clangd = 'C:\Users\vk_do\AppData\Local\Programs\clangd_23.1.0\bin\clangd.exe'
  $env:INCLUDE = ''; $env:LIB = ''
  $env:CLANGD_QUERY_DRIVER = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe'
  $out = & $node (Join-Path $Root 'probe\lsp-smoke.mjs') --clangd $clangd --ccdir $dbg --src $Src 2>&1
  Check 'LSP smoke' (($LASTEXITCODE -eq 0) -and (($out -join [char]10) -match 'LSP SMOKE: PASS')) 'cross-file definition + references (takes ~1 min: clangd index warm-up)'
}

Write-Host ''
if ($bad -eq 0) { Write-Host 'VERIFY: environment ready'; exit 0 }
Write-Host ('VERIFY: ' + $bad + ' item(s) missing - run D:\Core-build\core-review\run-all.ps1 first')
exit 1
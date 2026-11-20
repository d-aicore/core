# proof-windows.ps1 - stage 0.2 proof: SHA-bound marker validation in run-all.ps1.
# Fresh folder (D:\Core-build\core-review\proof); the historical reference is never touched.
# Proves: (P1/P2) same context reuses results; (P3) a different revision invalidates;
# (P4/P5) configure reuse; (P6) a configuration flag change invalidates only that configuration;
# (P7) a tracked build-affecting local change invalidates at the SAME revision.
$ErrorActionPreference = 'Continue'
$root  = 'D:\Core-build\core-review'
$proof = Join-Path $root 'proof'
$run   = Join-Path $root 'run-all.ps1'
$old   = Join-Path $root 'proof-src-old'   # detached scratch worktree at 8c6837e (removed after proof)
$log   = Join-Path $proof 'proof-windows-log.txt'
Set-Content -Encoding UTF8 -Path $log -Value ("proof-windows.ps1 " + (Get-Date -Format o))
function Invoke-Proof([string]$label, [string[]]$scriptArgs, [string]$snapshot) {
  $out = & powershell -NoProfile -ExecutionPolicy Bypass -File $run @scriptArgs 2>&1
  $rc = $LASTEXITCODE
  $block = @("", "=== $label (invocation rc=$rc) ===") + @($out | ForEach-Object { [string]$_ })
  Add-Content -Encoding UTF8 -Path $log -Value ($block -join ([string][char]13 + [string][char]10))
  Write-Host ("--- " + $label + " rc=" + $rc)
  $out | Where-Object { $_ -match 'RUN |SKIP\(reuse\)|INVALIDATED|RC ' } | ForEach-Object { Write-Host ("    " + $_) }
  if ($snapshot -ne '') {
    $src = Join-Path $proof $snapshot
    if (Test-Path $src) { Copy-Item $src (Join-Path $proof ($label + '-' + $snapshot)) -Force }
  }
  return $rc
}
$NL = [string][char]13 + [string][char]10

# P1: first run of 00-env in the fresh proof root -> must RUN
Invoke-Proof 'P1' @('-Root', $proof, '-Only', '00-env') '00-env.done' | Out-Null
# P2: identical context -> must SKIP(reuse)
Invoke-Proof 'P2' @('-Root', $proof, '-Only', '00-env') '00-env.done' | Out-Null
# P3: same step, source at the OLD revision (8c6837e worktree) -> must INVALIDATE and RUN
Invoke-Proof 'P3' @('-Root', $proof, '-Src', $old, '-Only', '00-env') '00-env.done' | Out-Null
# P4: ASan configure, default flags (address, tests ON) -> must RUN
Invoke-Proof 'P4' @('-Root', $proof, '-Only', '05-configure-asan') '05-configure-asan.done' | Out-Null
# P5: identical configuration -> must SKIP(reuse)
Invoke-Proof 'P5' @('-Root', $proof, '-Only', '05-configure-asan') '05-configure-asan.done' | Out-Null
# P6: configuration flag changed (tests OFF) -> must INVALIDATE and RUN (debug markers unaffected)
Invoke-Proof 'P6' @('-Root', $proof, '-Only', '05-configure-asan', '-AsanTests', 'OFF') '05-configure-asan.done' | Out-Null
# P7: tracked build-affecting local change at the SAME old revision -> must INVALIDATE and RUN
Add-Content -Encoding ASCII -Path (Join-Path $old 'CMakeLists.txt') -Value '# proof-only local change (stage 0.2 P7); removed after the proof'
Invoke-Proof 'P7' @('-Root', $proof, '-Src', $old, '-Only', '00-env') '00-env.done' | Out-Null
git -C $old checkout -- CMakeLists.txt 2>$null

# Summary: expected vs observed, from the marker snapshots and the invocation log
$txt = Get-Content $log -Raw
$summary = @(
  'stage 0.2 Windows proof summary (expected -> observed)',
  ('P1 first 00-env runs:            ' + $(if ($txt -match '(?s)=== P1 .*?RUN 00-env') { 'PASS' } else { 'FAIL' })),
  ('P2 same context reuses:          ' + $(if ($txt -match '(?s)=== P2 .*?SKIP\(reuse\) 00-env') { 'PASS' } else { 'FAIL' })),
  ('P3 old revision invalidates:     ' + $(if ($txt -match '(?s)=== P3 .*?INVALIDATED 00-env.*?RUN 00-env') { 'PASS' } else { 'FAIL' })),
  ('P4 asan configure runs:          ' + $(if ($txt -match '(?s)=== P4 .*?RUN 05-configure-asan') { 'PASS' } else { 'FAIL' })),
  ('P5 same config reuses:           ' + $(if ($txt -match '(?s)=== P5 .*?SKIP\(reuse\) 05-configure-asan') { 'PASS' } else { 'FAIL' })),
  ('P6 flag change invalidates:      ' + $(if ($txt -match '(?s)=== P6 .*?INVALIDATED 05-configure-asan.*?RUN 05-configure-asan') { 'PASS' } else { 'FAIL' })),
  ('P7 local change invalidates:     ' + $(if ($txt -match '(?s)=== P7 .*?INVALIDATED 00-env.*?RUN 00-env') { 'PASS' } else { 'FAIL' })),
  '',
  'marker fingerprints (first line of each snapshot):'
)
foreach ($f in @('P1-00-env.done','P3-00-env.done','P4-05-configure-asan.done','P6-05-configure-asan.done','P7-00-env.done')) {
  $p = Join-Path $proof $f
  if (Test-Path $p) { $summary += ($f + ': ' + @(Get-Content $p -TotalCount 1)[0]) }
}
$summary | Set-Content -Encoding UTF8 (Join-Path $proof 'proof-windows-summary.txt')
Add-Content -Encoding UTF8 -Path $log -Value (($summary -join $NL))
Get-Content (Join-Path $proof 'proof-windows-summary.txt')
Write-Host 'proof-windows.ps1 finished'

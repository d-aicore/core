# release-probe.ps1 - is engine_process_tests failing only in Debug?
# Configures a Release tree at the same SHA (app ON: the plugin-crash cases need the test VST3s),
# builds it and runs engine_process_tests three times, logging exit codes. Environment diagnostics only.
param(
  [string]$Src = "D:\CORE\CORE",
  [string]$Root = "D:\Core-build\core-review\release",
  [string]$Juce = "D:\Core-deps\JUCE-9.0.5",
  [int]$Jobs = 8
)
$ErrorActionPreference = "Continue"
$NL = [string][char]13 + [string][char]10
function Q([string]$s) { return ('"' + $s + '"') }
New-Item -ItemType Directory -Force -Path $Root | Out-Null
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$juceDef = "-DFETCHCONTENT_SOURCE_DIR_JUCE=" + ($Juce -replace "\\", "/")
$exe = Join-Path $Root "tests\engine_process_tests.exe"
$lines = @(
  "@echo off",
  "set VSLANG=1033",
  "chcp 65001 >nul",
  ("call " + (Q $vcvars) + " >nul 2>&1"),
  ("cmake -S " + (Q $Src) + " -B " + (Q $Root) + " -G Ninja -DCMAKE_BUILD_TYPE=Release -DCORE_BUILD_TESTS=ON " + (Q $juceDef)),
  ("cmake --build " + (Q $Root) + " --parallel " + $Jobs),
  "setlocal EnableDelayedExpansion",
  ("for /L %%i in (1,1,3) do (" + (Q $exe) + " & echo RELEASE_RUN%%i_EXIT=!ERRORLEVEL!)"),
  "endlocal"
)
$bat = Join-Path $env:TEMP ("core-release-" + [guid]::NewGuid().ToString("N") + ".bat")
Set-Content -Encoding ASCII -Path $bat -Value ($lines -join $NL)
$out = & cmd.exe /c ((Q $bat) + " 2>&1")
$rc = $LASTEXITCODE
$out | Set-Content -Encoding UTF8 -Path (Join-Path $Root "release-probe.txt")
Remove-Item -Force -ErrorAction SilentlyContinue $bat
Write-Output ("release-probe rc=" + $rc)
Get-Content (Join-Path $Root "release-probe.txt") -Tail 30

param(
  [string]$Exe = "D:/Core-build/ux-pulse-A/app/desktop/CoreApp_artefacts/Release/Core.exe",
  [string]$Pic = "D:/Core-build/ux-pulse-A-evidence/ui/burst-selftest.png",
  [string]$OutDir = "D:/Core-build/ux-pulse-A-evidence/ui/burst",
  [string]$Clicks = "",
  [int]$Frames = 44,
  [int]$EveryMs = 600
)
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W6 {
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint f);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
}
"@
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$clickList = @()
if ($Clicks -ne "") { foreach ($c in ($Clicks -split ",")) { $a = $c -split ":"; $clickList += ,@([int]$a[0], [int]$a[1], [int]$a[2]) } }

$proc = Start-Process -FilePath $Exe -ArgumentList @("--self-test", $Pic) -PassThru
Write-Output ("self-test pid {0}" -f $proc.Id)
Start-Sleep -Milliseconds 2500
$p = Get-Process -Name Core -ErrorAction SilentlyContinue | Sort-Object StartTime | Select-Object -Last 1
if ($p) { [W6]::SetWindowPos($p.MainWindowHandle, [IntPtr](-1), 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040) | Out-Null; Write-Output ("topmost set on pid {0}" -f $p.Id) }

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$ci = 0
for ($i = 0; $i -lt $Frames; $i++) {
  while ($ci -lt $clickList.Count -and $sw.ElapsedMilliseconds -ge $clickList[$ci][0]) {
    [W6]::SetCursorPos($clickList[$ci][1], $clickList[$ci][2]) | Out-Null
    Start-Sleep -Milliseconds 120
    [W6]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)
    [W6]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)
    Write-Output ("t={0}ms clicked ({1},{2})" -f $sw.ElapsedMilliseconds, $clickList[$ci][1], $clickList[$ci][2])
    $ci++
    Start-Sleep -Milliseconds 250
  }
  $b = [System.Windows.Forms.SystemInformation]::VirtualScreen
  $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size)
  $f = "{0}/f{1:D2}.png" -f $OutDir, $i
  $bmp.Save($f, [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  Start-Sleep -Milliseconds $EveryMs
}
if ($p) { [W6]::SetWindowPos($p.MainWindowHandle, [IntPtr](-2), 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040) | Out-Null; Write-Output "topmost cleared" }
Write-Output ("done at t={0}ms, proc exited={1}" -f $sw.ElapsedMilliseconds, $proc.HasExited)

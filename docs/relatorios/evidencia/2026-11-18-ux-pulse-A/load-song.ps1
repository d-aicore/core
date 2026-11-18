param(
  [string]$Exe = "D:/Core-build/ux-pulse-A/app/desktop/CoreApp_artefacts/Release/Core.exe",
  [string]$Song = "D:\Core-build\ux-pulse-A\app\desktop\CoreApp_artefacts\Release\demo.core-document.json",
  [string]$Out = "D:/Core-build/ux-pulse-A-evidence/ui/10-song-loaded.png"
)
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class W4 {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
}
"@

function Get-DialogHandles([int]$pid_) {
  $found = New-Object System.Collections.Generic.List[IntPtr]
  $cb = [W4+EnumProc]{
    param($h, $l)
    $p = 0
    [W4]::GetWindowThreadProcessId($h, [ref]$p) | Out-Null
    if ($p -eq $pid_) {
      $sb = New-Object System.Text.StringBuilder 256
      [W4]::GetClassName($h, $sb, 256) | Out-Null
      if ($sb.ToString() -eq "#32770" -and [W4]::IsWindowVisible($h)) { $found.Add($h) }
    }
    return $true
  }
  [W4]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
  return $found
}

$p = Get-Process -Name Core -ErrorAction SilentlyContinue | Sort-Object StartTime | Select-Object -Last 1
if (-not $p) { $p = Start-Process -FilePath $Exe -PassThru; Write-Output ("started pid {0}" -f $p.Id) } else { Write-Output ("reusing pid {0}" -f $p.Id) }
Start-Sleep -Milliseconds 5000
$p.Refresh()
$h = $p.MainWindowHandle
[W4]::ShowWindow($h, 3) | Out-Null
function Take-Foreground([IntPtr]$w) {
  $fgH = [W4]::GetForegroundWindow()
  $fgT = [uint32]0
  [W4]::GetWindowThreadProcessId($fgH, [ref]$fgT) | Out-Null
  $myT = [W4]::GetCurrentThreadId()
  [W4]::AttachThreadInput($myT, $fgT, $true) | Out-Null
  [W4]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)
  [W4]::ShowWindow($w, 3) | Out-Null
  [W4]::BringWindowToTop($w) | Out-Null
  [W4]::SetForegroundWindow($w) | Out-Null
  [W4]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
  [W4]::AttachThreadInput($myT, $fgT, $false) | Out-Null
}
$fg = $false
for ($t = 0; $t -lt 8; $t++) {
  Take-Foreground $h
  Start-Sleep -Milliseconds 300
  if ([W4]::GetForegroundWindow() -eq $h) { $fg = $true; break }
}
Write-Output ("main foreground={0}" -f $fg)
if (-not $fg) { Write-Output "ABORT: main window not foreground"; exit 3 }

[System.Windows.Forms.SendKeys]::SendWait("^o")
$dlg = $null
for ($t = 0; $t -lt 20; $t++) {
  Start-Sleep -Milliseconds 250
  $d = Get-DialogHandles $p.Id
  if ($d.Count -gt 0) { $dlg = $d[0]; break }
}
if (-not $dlg) { Write-Output "ABORT: no native open dialog of my process; nothing typed"; exit 4 }
Write-Output ("dialog hwnd {0} verified; typing the path" -f $dlg)
[System.Windows.Forms.SendKeys]::SendWait($Song)
Start-Sleep -Milliseconds 600
[System.Windows.Forms.SendKeys]::SendWait("{ENTER}")
$loaded = $false
for ($t = 0; $t -lt 20; $t++) {
  Start-Sleep -Milliseconds 400
  $p.Refresh()
  if ($p.MainWindowTitle -notlike "*Untitled*") { $loaded = $true; break }
}
Write-Output ("loaded={0} title='{1}'" -f $loaded, $p.MainWindowTitle)
Take-Foreground $p.MainWindowHandle
Start-Sleep -Milliseconds 800
$b = [System.Windows.Forms.SystemInformation]::VirtualScreen
$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output ("saved {0}" -f $Out)

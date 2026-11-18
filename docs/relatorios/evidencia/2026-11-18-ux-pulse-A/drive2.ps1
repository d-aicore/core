param(
  [string]$Exe = "D:/Core-build/ux-pulse-A/app/desktop/CoreApp_artefacts/Release/Core.exe",
  [Parameter(Mandatory=$true)][string]$Out,
  [string[]]$Seq = @(),
  [string]$SeqText = "",
  [int]$WaitMs = 3000,
  [int]$BetweenMs = 1400,
  [switch]$New,
  [switch]$NoActivate,
  [int]$ClickX = -1,
  [int]$ClickY = -1,
  [int]$ClickBefore = -1
)
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W2 {
  public struct PT { public int X; public int Y; }
  public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(PT p);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
}
"@

if ($SeqText -ne "") { $Seq = $SeqText -split "\|" }
if ($New) { $p = Start-Process -FilePath $Exe -PassThru; Write-Output ("started pid {0}" -f $p.Id) }
else {
  $p = Get-Process -Name Core -ErrorAction SilentlyContinue | Select-Object -First 1
  if (-not $p) { Write-Output "NO CORE PROCESS"; exit 2 }
}
Start-Sleep -Milliseconds $WaitMs
$p.Refresh()
$h = $p.MainWindowHandle
function Take-Foreground([IntPtr]$w) {
  $fgH = [W2]::GetForegroundWindow()
  $fgT = [uint32]0
  [W2]::GetWindowThreadProcessId($fgH, [ref]$fgT) | Out-Null
  $myT = [W2]::GetCurrentThreadId()
  [W2]::AttachThreadInput($myT, $fgT, $true) | Out-Null
  [W2]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)
  [W2]::ShowWindow($w, 3) | Out-Null
  [W2]::BringWindowToTop($w) | Out-Null
  [W2]::SetForegroundWindow($w) | Out-Null
  [W2]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
  [W2]::AttachThreadInput($myT, $fgT, $false) | Out-Null
}
if ($h -ne [IntPtr]::Zero -and -not $NoActivate) {
  for ($t = 0; $t -lt 8; $t++) {
    Take-Foreground $h
    Start-Sleep -Milliseconds 300
    if ([W2]::GetForegroundWindow() -eq $h) { break }
  }
  $fg = ([W2]::GetForegroundWindow() -eq $h)
  Write-Output ("foreground={0}" -f $fg)
  if (-not $fg) { Write-Output "REFUSING TO SEND KEYS: window is not foreground"; exit 3 }
}
Write-Output ("title '{0}'" -f $p.MainWindowTitle)

for ($i = 0; $i -lt $Seq.Count; $i++) {
  if ($i -eq $ClickBefore -and $ClickX -ge 0) {
    [W2]::SetCursorPos($ClickX, $ClickY) | Out-Null
    Start-Sleep -Milliseconds 200
    [W2]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)
    [W2]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)
    Write-Output ("clicked {0},{1}" -f $ClickX, $ClickY)
    Start-Sleep -Milliseconds $BetweenMs
  }
  [System.Windows.Forms.SendKeys]::SendWait($Seq[$i])
  Write-Output ("sent: {0}" -f $Seq[$i])
  Start-Sleep -Milliseconds $BetweenMs
}
if ($ClickBefore -eq $Seq.Count -and $ClickX -ge 0) {
  [W2]::SetCursorPos($ClickX, $ClickY) | Out-Null
  Start-Sleep -Milliseconds 200
  [W2]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)
  [W2]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)
  Write-Output ("clicked {0},{1}" -f $ClickX, $ClickY)
  Start-Sleep -Milliseconds $BetweenMs
}

$b = [System.Windows.Forms.SystemInformation]::VirtualScreen
$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output ("saved {0} {1}x{2}" -f $Out, $b.Width, $b.Height)

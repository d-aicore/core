param(
  [Parameter(Mandatory=$true)][string]$Out,
  [switch]$FullWindow,
  [int]$ClickX = -1,
  [int]$ClickY = -1,
  [string]$Keys = "",
  [int]$AfterMs = 600
)
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W5 {
  public struct RECT { public int L, T, R, B; }
  public struct PT { public int X; public int Y; }
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref PT p);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
}
"@

function Take-Foreground([IntPtr]$w) {
  $fgH = [W5]::GetForegroundWindow()
  $fgT = [uint32]0
  [W5]::GetWindowThreadProcessId($fgH, [ref]$fgT) | Out-Null
  $myT = [W5]::GetCurrentThreadId()
  [W5]::AttachThreadInput($myT, $fgT, $true) | Out-Null
  [W5]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)
  [W5]::ShowWindow($w, 3) | Out-Null
  [W5]::BringWindowToTop($w) | Out-Null
  [W5]::SetForegroundWindow($w) | Out-Null
  [W5]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
  [W5]::AttachThreadInput($myT, $fgT, $false) | Out-Null
}

$p = Get-Process -Name Core -ErrorAction SilentlyContinue | Sort-Object StartTime | Select-Object -Last 1
if (-not $p) { Write-Output "NO CORE PROCESS"; exit 2 }
$p.Refresh()
$h = $p.MainWindowHandle
Take-Foreground $h
$fg = ([W5]::GetForegroundWindow() -eq $h)
Write-Output ("pid {0} foreground={1} title='{2}'" -f $p.Id, $fg, $p.MainWindowTitle)

if ($ClickX -ge 0) {
  $pt = New-Object W5+PT
  $pt.X = $ClickX; $pt.Y = $ClickY
  [W5]::ClientToScreen($h, [ref]$pt) | Out-Null
  [W5]::SetCursorPos($pt.X, $pt.Y) | Out-Null
  Start-Sleep -Milliseconds 150
  [W5]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)
  [W5]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)
  Write-Output ("clicked client ({0},{1}) -> screen ({2},{3})" -f $ClickX, $ClickY, $pt.X, $pt.Y)
  Start-Sleep -Milliseconds $AfterMs
}
if ($Keys -ne "") {
  if (-not $fg) { Write-Output "REFUSING KEYS: not foreground"; exit 3 }
  [System.Windows.Forms.SendKeys]::SendWait($Keys)
  Write-Output ("sent: {0}" -f $Keys)
  Start-Sleep -Milliseconds $AfterMs
}

$cr = New-Object W5+RECT
if ($FullWindow) { [W5]::GetWindowRect($h, [ref]$cr) | Out-Null; $flag = [uint32]0 }
else { [W5]::GetClientRect($h, [ref]$cr) | Out-Null; $flag = [uint32]1 }
$w = $cr.R - $cr.L; $ht = $cr.B - $cr.T
$bmp = New-Object System.Drawing.Bitmap $w, $ht
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
$ok = [W5]::PrintWindow($h, $hdc, $flag)
$g.ReleaseHdc($hdc)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output ("printwindow ok={0} saved {1} {2}x{3}" -f $ok, $Out, $w, $ht)

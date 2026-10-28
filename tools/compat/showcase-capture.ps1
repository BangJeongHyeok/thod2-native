# Captures the game window once per -Every seconds on a fixed timeline, for the
# original-versus-thod2-native comparison pictures in README.md
# (tools/compat/showcase-compose.ps1 puts two captures side by side).
#
# Without -Play nothing is pressed (title, attract demo). With -Play, Enter is
# pressed twice at 8 s and 10 s (title -> Arcade, chapter 1 cut scene), the
# cursor is held at -Aim, and the right button skips the cut scene at -SkipAt.
# Works for the original game (answers the "Attention" dialog with Yes,
# captures its 640x480 full screen) and for thod2-native (window capture).
# Do not use the PC during the run.
#
#   .\showcase-capture.ps1 -GameDir <game> -OutDir <folder> -Tag orig -Seconds 60
#   .\showcase-capture.ps1 -GameDir <game> -OutDir <folder> -Tag orig_play -Play
param(
  [Parameter(Mandatory = $true)][string]$GameDir,
  [Parameter(Mandatory = $true)][string]$OutDir,
  [string]$Tag = 'shot',
  [int]$Seconds = 60,
  [double]$Every = 1.0,
  [switch]$Play,
  [double]$SkipAt = 34,
  [double[]]$Aim = @(0.62, 0.45)
)
. "$PSScriptRoot\winprobe.ps1"
Add-Type -AssemblyName System.Drawing
$src = @'
using System; using System.Runtime.InteropServices; using System.Threading;
public static class ShowInput {
  [StructLayout(LayoutKind.Sequential)] struct MOUSEINPUT { public int dx, dy; public uint data, flags, time; public IntPtr extra; }
  [StructLayout(LayoutKind.Sequential)] struct KEYBDINPUT { public ushort vk, scan; public uint flags, time; public IntPtr extra; public int pad1, pad2; }
  [StructLayout(LayoutKind.Explicit)] struct INPUTUNION { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; }
  [StructLayout(LayoutKind.Sequential)] struct INPUT { public uint type; public INPUTUNION u; }
  [DllImport("user32.dll")] static extern uint SendInput(uint n, INPUT[] inputs, int size);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern void SwitchToThisWindow(IntPtr h, bool altTab);
  [DllImport("user32.dll")] static extern int GetSystemMetrics(int i);
  static void Send(INPUT i) { SendInput(1, new INPUT[] { i }, Marshal.SizeOf(typeof(INPUT))); }
  public static void Tap(int dik) {
    INPUT i = new INPUT(); i.type = 1; i.u.ki.scan = (ushort)dik; i.u.ki.flags = 8; Send(i);
    Thread.Sleep(80); i.u.ki.flags = 8 | 2; Send(i);
  }
  public static void RightClick() {
    INPUT i = new INPUT(); i.type = 0; i.u.mi.flags = 0x8; Send(i); Thread.Sleep(60); i.u.mi.flags = 0x10; Send(i);
  }
  // Absolute position on the primary screen, x and y in 0..1.
  public static void MoveTo(double x, double y) {
    INPUT i = new INPUT(); i.type = 0; i.u.mi.dx = (int)(x * 65535); i.u.mi.dy = (int)(y * 65535); i.u.mi.flags = 0x8001; Send(i);
  }
}
'@
Add-Type -TypeDefinition $src -ErrorAction SilentlyContinue
$GameDir = (Resolve-Path $GameDir).Path
New-Item -ItemType Directory -Force $OutDir | Out-Null

function Capture-Window([IntPtr]$h, [string]$path) {
  $r = New-Object WP+RECT
  [WP]::GetWindowRect($h, [ref]$r) | Out-Null
  $w = [Math]::Max(1, $r.R - $r.L); $hgt = [Math]::Max(1, $r.B - $r.T)
  $b = New-Object System.Drawing.Bitmap $w, $hgt
  $g = [System.Drawing.Graphics]::FromImage($b)
  $g.CopyFromScreen($r.L, $r.T, 0, 0, $b.Size)
  $b.Save($path); $g.Dispose(); $b.Dispose()
}

Get-Process Hod2 -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 1
$p = Start-Process -FilePath (Join-Path $GameDir 'Hod2.exe') -WorkingDirectory $GameDir -PassThru
$start = Get-Date
# The original asks "Attention" (use this Direct3D device?) after creating
# its window: Yes. thod2-native answers it itself, so give up after 8 s.
for ($i = 0; $i -lt 16 -and -not $p.HasExited; $i++) {
  Start-Sleep -Milliseconds 500
  $dialog = [WP]::Wins($p.Id) | Where-Object { ([WP]::Desc($_)) -match "#32770" -and ([WP]::Desc($_)) -match 'vis=True' } | Select-Object -First 1
  if ($dialog) { [WP]::PostMessage([WP]::GetDlgItem($dialog, 6), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null; $start = Get-Date; 'answered the device dialog'; Start-Sleep 1; break }
}
$main = $null
for ($i = 0; $i -lt 20 -and -not $main; $i++) {
  Start-Sleep -Milliseconds 250
  $main = [WP]::Wins($p.Id) | Where-Object { ([WP]::Desc($_)) -match "class='The House" } | Select-Object -First 1
}
for ($i = 0; $i -lt 10 -and [ShowInput]::GetForegroundWindow() -ne $main; $i++) { [ShowInput]::SwitchToThisWindow($main, $true); Start-Sleep -Milliseconds 300 }
[ShowInput]::MoveTo($Aim[0], $Aim[1])

$n = 0; $pressed = 0; $skipped = $false
while (-not $p.HasExited) {
  $t = ((Get-Date) - $start).TotalSeconds
  if ($t -ge $Seconds) { break }
  if ($Play) {
    if ($pressed -eq 0 -and $t -ge 8) { [ShowInput]::Tap(0x1C); $pressed = 1 }
    if ($pressed -eq 1 -and $t -ge 10) { [ShowInput]::Tap(0x1C); $pressed = 2 }
    if (-not $skipped -and $t -ge $SkipAt) { [ShowInput]::RightClick(); $skipped = $true }
    [ShowInput]::MoveTo($Aim[0], $Aim[1])
  }
  if ($t -ge $n * $Every) {
    Capture-Window $main (Join-Path $OutDir ('{0}_{1:D3}.png' -f $Tag, $n)); $n++
  }
  Start-Sleep -Milliseconds 50
}
Stop-Process -Id $p.Id -ErrorAction SilentlyContinue
"$Tag : $n captures in $OutDir"

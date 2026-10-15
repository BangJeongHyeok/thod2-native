# Starts the game in the test copy with [debug] trace_screen_quads = 1, presses
# Enter (P1 start) -Presses times every -PressEvery seconds and captures the
# primary screen after each step. Two presses reach the chapter 1 cut scene
# (title -> Arcade mode). Restores thod2-native.ini afterwards and prints the
# trace lines. Do not touch the PC during the run.
#
#   .\intro-capture.ps1 -Seconds 24 -Presses 2 -OutDir <folder>
#   then: Select-String <game>\thod2-native.log -Pattern 'model|stack returns'
param([int]$Seconds = 50, [int]$Presses = 1000, [double]$PressEvery = 2.0, [Parameter(Mandatory = $true)][string]$OutDir)
. "$PSScriptRoot\winprobe.ps1"
Add-Type -AssemblyName System.Drawing
$src = @'
using System; using System.Runtime.InteropServices; using System.Threading;
public static class FakeInput {
  [StructLayout(LayoutKind.Sequential)] struct MOUSEINPUT { public int dx, dy; public uint data, flags, time; public IntPtr extra; }
  [StructLayout(LayoutKind.Sequential)] struct KEYBDINPUT { public ushort vk, scan; public uint flags, time; public IntPtr extra; public int pad1, pad2; }
  // The union starts at 4 (x86) or 8 (x64): let the marshaller align it.
  [StructLayout(LayoutKind.Explicit)] struct INPUTUNION {
    [FieldOffset(0)] public MOUSEINPUT mi;
    [FieldOffset(0)] public KEYBDINPUT ki;
  }
  [StructLayout(LayoutKind.Sequential)] struct INPUT { public uint type; public INPUTUNION u; }
  [DllImport("user32.dll", SetLastError = true)] static extern uint SendInput(uint n, INPUT[] inputs, int size);
  public static int Failures = 0;
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern void SwitchToThisWindow(IntPtr h, bool altTab);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int l, t, r, b; }
  [StructLayout(LayoutKind.Sequential)] public struct PT { public int x, y; }
  [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] static extern bool ClientToScreen(IntPtr h, ref PT p);
  [DllImport("user32.dll")] static extern int GetSystemMetrics(int i);
  // Moves the cursor to a point inside the client area of `h` (fx, fy in 0..1).
  public static void MoveInside(IntPtr h, double fx, double fy) {
    RECT r; GetClientRect(h, out r); PT p = new PT(); ClientToScreen(h, ref p);
    double x = p.x + fx * (r.r - r.l), y = p.y + fy * (r.b - r.t);
    MoveTo((int)(x * 65535 / GetSystemMetrics(0)), (int)(y * 65535 / GetSystemMetrics(1)));
  }
  const uint KEYEVENTF_KEYUP = 2, KEYEVENTF_SCANCODE = 8, KEYEVENTF_EXTENDEDKEY = 1;
  static void Send(INPUT i) { if (SendInput(1, new INPUT[] { i }, Marshal.SizeOf(typeof(INPUT))) != 1) Failures++; }
  // scan: DIK code; codes >= 0x80 are extended keys (0xE0 prefix).
  public static void Key(int dik, bool down) {
    INPUT i = new INPUT(); i.type = 1;
    i.u.ki.scan = (ushort)(dik & 0x7F);
    i.u.ki.flags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP) | ((dik & 0x80) != 0 ? KEYEVENTF_EXTENDEDKEY : 0);
    Send(i);
  }
  public static void Tap(int dik, int ms) { Key(dik, true); Thread.Sleep(ms); Key(dik, false); }
  // button: 0 left, 1 right, 2 middle
  public static void Button(int button, bool down) {
    uint[] flags = { 0x2, 0x8, 0x20 };
    INPUT i = new INPUT(); i.type = 0; i.u.mi.flags = down ? flags[button] : flags[button] << 1;
    Send(i);
  }
  public static void Click(int button, int ms) { Button(button, true); Thread.Sleep(ms); Button(button, false); }
  // Absolute move in 0..65535 screen coordinates.
  public static void MoveTo(int x, int y) {
    INPUT i = new INPUT(); i.type = 0; i.u.mi.dx = x; i.u.mi.dy = y; i.u.mi.flags = 0x8001; Send(i);
  }
}
'@
Add-Type -TypeDefinition $src -ErrorAction SilentlyContinue
$GameDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\test-game\THOTD2')).Path
$ini = Join-Path $GameDir 'thod2-native.ini'; $log = Join-Path $GameDir 'thod2-native.log'
$backup = [IO.File]::ReadAllText($ini)
New-Item -ItemType Directory -Force $OutDir | Out-Null
function Capture([string]$path) { $b = New-Object System.Drawing.Bitmap 1920, 1080; $g = [System.Drawing.Graphics]::FromImage($b); $g.CopyFromScreen(0, 0, 0, 0, $b.Size); $b.Save($path); $g.Dispose(); $b.Dispose() }
try {
  $text = $backup
  if ($text -match '(?m)^\s*trace_screen_quads\s*=') { $text = $text -replace '(?m)^\s*trace_screen_quads\s*=.*$', 'trace_screen_quads = 1' }
  elseif ($text -match '(?m)^\[debug\]') { $text = $text -replace '(?m)^\[debug\]\s*$', "[debug]`r`ntrace_screen_quads = 1" }
  else { $text = $text.TrimEnd() + "`r`n`r`n[debug]`r`ntrace_screen_quads = 1`r`n" }
  [IO.File]::WriteAllText($ini, $text)
  Get-Process Hod2 -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 1
  $p = Start-Process -FilePath (Join-Path $GameDir 'Hod2.exe') -WorkingDirectory $GameDir -PassThru
  Start-Sleep 8
  $main = [WP]::Wins($p.Id) | Where-Object { ([WP]::Desc($_)) -match "class='The House" } | Select-Object -First 1
  for ($i = 0; $i -lt 10 -and [FakeInput]::GetForegroundWindow() -ne $main; $i++) { [FakeInput]::SwitchToThisWindow($main, $true); Start-Sleep -Milliseconds 500 }
  $start = Get-Date; $n = 0
  while (((Get-Date) - $start).TotalSeconds -lt $Seconds -and -not $p.HasExited) {
    if ([FakeInput]::GetForegroundWindow() -ne $main) { "lost foreground"; break }
    if ($n -lt $Presses) { [FakeInput]::Tap(0x1C, 80) } else { Start-Sleep -Milliseconds 80 }
    Start-Sleep -Milliseconds ([int]($PressEvery * 1000) - 80)
    Capture (Join-Path $OutDir ("nav_{0:D2}.png" -f $n)); $n++
    $p.Refresh()
  }
  Stop-Process -Id $p.Id -ErrorAction SilentlyContinue
} finally { [IO.File]::WriteAllText($ini, $backup) }
Select-String -Path $log -Pattern 'screen_quad|widescreen' | ForEach-Object { $_.Line -replace '^\[[^\]]*\] \[INFO\] ', '' }

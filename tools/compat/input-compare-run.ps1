# Runs Hod2.exe in the test copy with [debug] input_compare = 1, feeds it
# scripted keyboard and mouse input (SendInput with scan codes, so DirectInput
# sees it), closes the game the normal way and prints the input_compare lines
# of thod2-native.log. The ini in the game folder is restored afterwards.
# Do not touch the PC during the run: the game must keep the focus. Scripted
# input is only sent while the game window is in the foreground, the cursor is
# only moved inside its client area, and Alt+Tab is never sent.
#
#   .\input-compare-run.ps1 -RunSeconds 60
#   .\input-compare-run.ps1 -Manual -RunSeconds 600   # no scripted input, play yourself
param(
  [int]$RunSeconds = 60,
  [switch]$Manual,
  [string]$GameDir = (Join-Path $PSScriptRoot '..\..\..\test-game\THOTD2')
)
. "$PSScriptRoot\winprobe.ps1"
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

$GameDir = (Resolve-Path $GameDir).Path
$ini = Join-Path $GameDir 'thod2-native.ini'
$log = Join-Path $GameDir 'thod2-native.log'
$backup = "$ini.input-compare-backup"
Copy-Item $ini $backup -Force
$text = Get-Content $ini -Raw
if ($text -match '(?m)^\s*input_compare\s*=') {
  $text = $text -replace '(?m)^\s*input_compare\s*=.*$', 'input_compare = 1'
} elseif ($text -match '(?m)^\[debug\]') {
  $text = $text -replace '(?m)^\[debug\]\s*$', "[debug]`r`ninput_compare = 1"
} else {
  $text = $text.TrimEnd() + "`r`n`r`n[debug]`r`ninput_compare = 1`r`n"
}
[IO.File]::WriteAllText($ini, $text)

try {
  "Hod2.ini [Controller]:"; Get-Content (Join-Path $GameDir 'Hod2.ini') | Select-Object -First 5
  Stop-Process -Name Hod2 -ErrorAction SilentlyContinue; Start-Sleep 1
  $p = Start-Process -FilePath (Join-Path $GameDir 'Hod2.exe') -WorkingDirectory $GameDir -PassThru
  Start-Sleep -Seconds 8
  $main = [WP]::Wins($p.Id) | Where-Object { ([WP]::Desc($_)) -match "class='The House" } | Select-Object -First 1
  # A background process may not take the foreground with SetForegroundWindow alone.
  for ($i = 0; $i -lt 10 -and [FakeInput]::GetForegroundWindow() -ne $main; $i++) {
    [FakeInput]::SwitchToThisWindow($main, $true); Start-Sleep -Milliseconds 500
  }
  "game window: $([WP]::Desc($main))"
  "foreground:  $([WP]::Desc([FakeInput]::GetForegroundWindow()))"

  $end = (Get-Date).AddSeconds($RunSeconds)
  $rng = New-Object Random 2026
  # DIK codes: layout 0 and layout 1 keys, Enter/Space, Tab with and without Alt.
  $keys = 0xC8, 0xD0, 0xCB, 0xCD, 0x1C, 0x39, 0x4E, 0x48, 0x50, 0x4B, 0x4D, 0x11, 0x1E, 0x20, 0x2D, 0x2A, 0x1D, 0x0F
  $sent = 0
  while ((Get-Date) -lt $end -and -not $p.HasExited) {
    if (-not $Manual) {
      if ([FakeInput]::GetForegroundWindow() -ne $main) {
        "game lost the foreground, scripted input stopped after $sent actions"; $Manual = $true; continue
      }
      [FakeInput]::MoveInside($main, 0.05 + 0.9 * $rng.NextDouble(), 0.05 + 0.9 * $rng.NextDouble())
      $sent++
      switch ($rng.Next(5)) {
        0 { [FakeInput]::Click(0, $rng.Next(20, 120)) }
        1 { [FakeInput]::Click(1, $rng.Next(20, 120)) }
        2 { [FakeInput]::Click(2, $rng.Next(20, 60)) }
        3 { [FakeInput]::Tap($keys[$rng.Next($keys.Count)], $rng.Next(20, 150)) }
        4 { [FakeInput]::Button(0, $true); [FakeInput]::Button(1, $true); Start-Sleep -Milliseconds 40
            [FakeInput]::Button(0, $false); [FakeInput]::Button(1, $false) }
      }
      Start-Sleep -Milliseconds $rng.Next(30, 200)
    } else {
      Start-Sleep -Seconds 1
    }
    $p.Refresh()
  }

  if (-not $p.HasExited) {
    [WP]::PostMessage($main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    for ($i = 0; $i -lt 20 -and -not $p.HasExited; $i++) {
      Start-Sleep -Milliseconds 500
      foreach ($h in [WP]::Wins($p.Id)) {
        $d = [WP]::Desc($h)
        if ($d -match '#32770' -and $d -match 'vis=True') {
          [WP]::PostMessage([WP]::GetDlgItem($h, 6), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
        }
      }
      $p.Refresh()
    }
    if (-not $p.HasExited) { "still running, killing"; Stop-Process -Id $p.Id }
  }
} finally {
  Move-Item $backup $ini -Force
}
"scripted actions: $sent, SendInput failures: $([FakeInput]::Failures)"
"=== input_compare log lines"
Select-String -Path $log -Pattern 'input_compare|input_update' | ForEach-Object { $_.Line }

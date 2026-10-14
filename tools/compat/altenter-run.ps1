# Starts Hod2.exe in the test copy, presses Alt+Enter twice and records the
# window/display state after each press, then kills the game.
#
#   .\altenter-run.ps1 -RunSeconds 10 -WaitSeconds 3
param(
  [int]$RunSeconds = 10,
  [int]$WaitSeconds = 3,
  [string]$GameDir = (Join-Path $PSScriptRoot '..\..\..\test-game\THOTD2')
)
. "$PSScriptRoot\winprobe.ps1"
$k = @'
using System; using System.Runtime.InteropServices;
public static class AltEnterKeys {
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte sc, int f, IntPtr e);
  public static void Send() {
    keybd_event(0x12, 0, 0, IntPtr.Zero); keybd_event(0x0D, 0, 0, IntPtr.Zero);
    System.Threading.Thread.Sleep(80);
    keybd_event(0x0D, 0, 2, IntPtr.Zero); keybd_event(0x12, 0, 2, IntPtr.Zero);
  }
}
'@
Add-Type -TypeDefinition $k -ErrorAction SilentlyContinue
$GameDir = (Resolve-Path $GameDir).Path
Stop-Process -Name Hod2 -ErrorAction SilentlyContinue; Start-Sleep 1
$p = Start-Process -FilePath (Join-Path $GameDir 'Hod2.exe') -WorkingDirectory $GameDir -PassThru
Start-Sleep -Seconds $RunSeconds
"=== before"; Probe
[AltEnterKeys]::Send(); Start-Sleep -Seconds $WaitSeconds
"=== after Alt+Enter #1"; Probe
[AltEnterKeys]::Send(); Start-Sleep -Seconds $WaitSeconds
"=== after Alt+Enter #2"; Probe
Stop-Process -Id $p.Id -ErrorAction SilentlyContinue

# Starts Hod2.exe in the test copy, lets it run, switches away with Alt+Tab and back,
# then kills it. Used together with [debug] trace_ddraw = 1 to record what the game
# does with DirectDraw when it loses and regains the screen.
#
#   .\alttab-run.ps1 -RunSeconds 12 -AwaySeconds 3 -BackSeconds 5
param(
  [int]$RunSeconds = 12,
  [int]$AwaySeconds = 3,
  [int]$BackSeconds = 5,
  [string]$GameDir = (Join-Path $PSScriptRoot '..\..\..\test-game\THOTD2')
)
. "$PSScriptRoot\winprobe.ps1"
$k = @'
using System; using System.Runtime.InteropServices;
public static class AltTabKeys {
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte sc, int f, IntPtr e);
  public static void Send() {
    keybd_event(0x12, 0, 0, IntPtr.Zero); keybd_event(0x09, 0, 0, IntPtr.Zero);
    System.Threading.Thread.Sleep(80);
    keybd_event(0x09, 0, 2, IntPtr.Zero); keybd_event(0x12, 0, 2, IntPtr.Zero);
  }
}
'@
Add-Type -TypeDefinition $k -ErrorAction SilentlyContinue
$GameDir = (Resolve-Path $GameDir).Path
Stop-Process -Name Hod2 -ErrorAction SilentlyContinue; Start-Sleep 1
$p = Start-Process -FilePath (Join-Path $GameDir 'Hod2.exe') -WorkingDirectory $GameDir -PassThru
# Answer the device dialog if it is shown (skip_device_dialog = 0).
for ($i = 0; $i -lt 10; $i++) {
  Start-Sleep -Milliseconds 500
  foreach ($h in [WP]::Wins($p.Id)) {
    $d = [WP]::Desc($h)
    if ($d -match '#32770' -and $d -match 'vis=True') {
      [WP]::PostMessage([WP]::GetDlgItem($h, 6), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    }
  }
}
Start-Sleep -Seconds $RunSeconds
"=== running"; Probe
[AltTabKeys]::Send(); Start-Sleep -Seconds $AwaySeconds
"=== away"; Probe
[AltTabKeys]::Send(); Start-Sleep -Seconds $BackSeconds
"=== back"; Probe
Stop-Process -Id $p.Id -ErrorAction SilentlyContinue

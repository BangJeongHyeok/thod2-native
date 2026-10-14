# Starts Hod2.exe in the test copy, lets it run, then closes it the normal way:
# WM_CLOSE to the game window and "Yes" on the quit confirmation, if one appears.
# Reports whether the process exited by itself and whether exception.log was written.
#
#   .\graceful-exit-run.ps1 -RunSeconds 12
param(
  [int]$RunSeconds = 12,
  [string]$GameDir = (Join-Path $PSScriptRoot '..\..\..\test-game\THOTD2')
)
. "$PSScriptRoot\winprobe.ps1"
$GameDir = (Resolve-Path $GameDir).Path
$exceptionLog = Join-Path $GameDir 'exception.log'
$before = if (Test-Path $exceptionLog) { (Get-Item $exceptionLog).LastWriteTime } else { $null }

Stop-Process -Name Hod2 -ErrorAction SilentlyContinue; Start-Sleep 1
$p = Start-Process -FilePath (Join-Path $GameDir 'Hod2.exe') -WorkingDirectory $GameDir -PassThru
Start-Sleep -Seconds $RunSeconds

$main = [WP]::Wins($p.Id) | Where-Object { ([WP]::Desc($_)) -match "class='The House" } | Select-Object -First 1
"WM_CLOSE to $main"
[WP]::PostMessage($main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null

for ($i = 0; $i -lt 20 -and -not $p.HasExited; $i++) {
  Start-Sleep -Milliseconds 500
  foreach ($h in [WP]::Wins($p.Id)) {
    $d = [WP]::Desc($h)
    if ($d -match '#32770' -and $d -match 'vis=True') {
      "dialog: $d"; [WP]::Kids($h)
      [WP]::PostMessage([WP]::GetDlgItem($h, 6), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null  # IDYES
    }
  }
  $p.Refresh()
}
if ($p.HasExited) { "exited by itself, exit code $($p.ExitCode)" } else { "still running, killing"; Stop-Process -Id $p.Id }
$after = if (Test-Path $exceptionLog) { (Get-Item $exceptionLog).LastWriteTime } else { $null }
if ($after -and $after -ne $before) { "exception.log written:"; Get-Content $exceptionLog | Select-Object -First 8 } else { "no new exception.log" }

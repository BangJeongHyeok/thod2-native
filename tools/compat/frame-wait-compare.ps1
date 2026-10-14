# Runs the game twice in the test copy, with [timing] frame_wait_sleep = 0 and = 1,
# with the profiler and DirectDraw trace enabled, and prints process CPU usage,
# main thread CPU, flips per second and frame interval statistics for each run.
# Restores thod2-native.ini afterwards.
#
#   .\frame-wait-compare.ps1 -Seconds 22
#   .\frame-wait-compare.ps1 -Modes 1 -Margins 1.0,2.0,3.0   # compare margins only
param(
  [int]$Seconds = 22,
  [int[]]$Modes = @(0, 1),
  [string[]]$Margins = @('1.0'),
  [string]$GameDir = (Join-Path $PSScriptRoot '..\..\..\test-game\THOTD2')
)
$GameDir = (Resolve-Path $GameDir).Path
$ini = Join-Path $GameDir 'thod2-native.ini'
$log = Join-Path $GameDir 'thod2-native.log'
$backup = Get-Content $ini -Raw

function Set-IniValue([string]$text, [string]$section, [string]$key, [string]$value) {
  $pattern = "(?ms)(^\[$section\][^\[]*?^)$key\s*=.*?$"
  if ($text -match $pattern) { return [regex]::Replace($text, $pattern, "`${1}$key = $value") }
  if ($text -match "(?m)^\[$section\]") { return [regex]::Replace($text, "(?m)^\[$section\]\s*$", "[$section]`r`n$key = $value") }
  return $text.TrimEnd() + "`r`n`r`n[$section]`r`n$key = $value`r`n"
}

try {
  $runs = foreach ($mode in $Modes) { if ($mode -eq 0) { ,@(0, '1.0') } else { foreach ($m in $Margins) { ,@(1, $m) } } }
  foreach ($run in $runs) {
    $mode = $run[0]; $margin = $run[1]
    $text = $backup
    $text = Set-IniValue $text 'timing' 'frame_wait_sleep' "$mode"
    $text = Set-IniValue $text 'timing' 'frame_wait_margin_ms' "$margin"
    $text = Set-IniValue $text 'debug' 'trace_ddraw' '1'
    $text = Set-IniValue $text 'debug' 'profile' '1'
    Set-Content -Path $ini -Value $text -Encoding ASCII -NoNewline

    Stop-Process -Name Hod2 -ErrorAction SilentlyContinue; Start-Sleep 1
    $p = Start-Process -FilePath (Join-Path $GameDir 'Hod2.exe') -WorkingDirectory $GameDir -PassThru
    Start-Sleep -Seconds 6
    $p.Refresh(); $c0 = $p.CPU; $t0 = Get-Date
    Start-Sleep -Seconds ($Seconds - 6)
    $p.Refresh(); $cpu = ($p.CPU - $c0) / ((Get-Date) - $t0).TotalSeconds
    Stop-Process -Id $p.Id -ErrorAction SilentlyContinue; Start-Sleep 1

    "########## frame_wait_sleep = $mode, frame_wait_margin_ms = $margin"
    "process cpu (after 6 s warm-up): {0:P0} of one core" -f $cpu
    Get-Content $log | Select-String 'frame_wait_sleep|Surface::Flip:|frame interval|\(main\) cpu' |
      Select-Object -Skip 0 | ForEach-Object { $_.Line -replace '^\[[^\]]*\] \[INFO\] ', '' }
  }
} finally {
  Set-Content -Path $ini -Value $backup -Encoding ASCII -NoNewline
}

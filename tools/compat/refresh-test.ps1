# Simulates other monitor refresh rates with [debug] flip_override and measures
# whether the game's frame rate and game speed change.
#
#   flip_override 0 = unchanged (vsync, 60 Hz here)
#                 1 = Flip without vsync (like an unlimited refresh rate)
#                 2 = Flip every second vsync (30 Hz on a 60 Hz monitor)
#
# Game speed is measured as the time from loading the title camera
# (cam\cp_title.bin) to loading the demo camera (cam\cp_tuto.bin): a fixed
# amount of game time, so it stretches if the game runs slower.
#
#   .\refresh-test.ps1 -Overrides 0,1,2 -Seconds 26
param(
  [int[]]$Overrides = @(0, 1, 2),
  [int]$Seconds = 26,
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

function Time-Of([string]$line) {
  if ($line -match '^\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3})\]') {
    return [datetime]::ParseExact($Matches[1], 'yyyy-MM-dd HH:mm:ss.fff', $null)
  }
}

try {
  foreach ($override in $Overrides) {
    $text = $backup
    $text = Set-IniValue $text 'debug' 'trace_ddraw' '1'
    $text = Set-IniValue $text 'debug' 'trace_files' '1'
    $text = Set-IniValue $text 'debug' 'flip_override' "$override"
    Set-Content -Path $ini -Value $text -Encoding ASCII -NoNewline

    Stop-Process -Name Hod2 -ErrorAction SilentlyContinue; Start-Sleep 1
    $p = Start-Process -FilePath (Join-Path $GameDir 'Hod2.exe') -WorkingDirectory $GameDir -PassThru
    Start-Sleep -Seconds $Seconds
    Stop-Process -Id $p.Id -ErrorAction SilentlyContinue; Start-Sleep 1

    $lines = Get-Content $log
    $title = $lines | Where-Object { $_ -match 'CreateFileA "cam\\cp_title\.bin"' } | Select-Object -First 1
    $demo = $lines | Where-Object { $_ -match 'CreateFileA "cam\\cp_tuto\.bin"' } | Select-Object -First 1
    "########## flip_override = $override"
    if ($title -and $demo) {
      "title -> demo: {0:N2} s" -f ((Time-Of $demo) - (Time-Of $title)).TotalSeconds
    } else {
      "title -> demo: not reached (title: $([bool]$title), demo: $([bool]$demo))"
    }
    $lines | Select-String 'Surface::Flip:|frame interval|Flip on .*\(primary\)' | Select-Object -First 8 |
      ForEach-Object { $_.Line -replace '^\[[^\]]*\] \[INFO\] ', '' }
  }
} finally {
  Set-Content -Path $ini -Value $backup -Encoding ASCII -NoNewline
}

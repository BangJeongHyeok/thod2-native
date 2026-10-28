# Compares [display] interpolate off and on: game speed, ticks and presents per
# second, and screenshots of the title and attract demo.
#
# Game speed is the time from loading the title camera (cam\cp_title.bin) to
# loading the demo camera (cam\cp_tuto.bin), as in refresh-test.ps1. The
# presents per second come from the frame_interp statistics ([debug]
# trace_interp) and from the DirectDraw Flip counter ([debug] trace_ddraw).
#
#   .\interp-test.ps1 -Modes 0,1 -Seconds 30 -OutDir <folder>
param(
  [int[]]$Modes = @(0, 1),
  [int]$Seconds = 30,
  [int]$MaxFps = 0,
  [string]$OutDir = '',
  [string]$GameDir = (Join-Path $PSScriptRoot '..\..\..\test-game\THOTD2')
)
. "$PSScriptRoot\winprobe.ps1"
Add-Type -AssemblyName System.Drawing
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

function Capture([string]$path) {
  $b = New-Object System.Drawing.Bitmap 1920, 1080
  $g = [System.Drawing.Graphics]::FromImage($b)
  $g.CopyFromScreen(0, 0, 0, 0, $b.Size)
  $b.Save($path)
  $g.Dispose(); $b.Dispose()
}

if ($OutDir) { New-Item -ItemType Directory -Force $OutDir | Out-Null }
try {
  foreach ($mode in $Modes) {
    $text = $backup
    $text = Set-IniValue $text 'display' 'interpolate' "$mode"
    $text = Set-IniValue $text 'display' 'max_fps' "$MaxFps"
    $text = Set-IniValue $text 'debug' 'trace_interp' '1'
    $text = Set-IniValue $text 'debug' 'trace_ddraw' '1'
    $text = Set-IniValue $text 'debug' 'trace_files' '1'
    Set-Content -Path $ini -Value $text -Encoding ASCII -NoNewline

    Stop-Process -Name Hod2 -ErrorAction SilentlyContinue; Start-Sleep 1
    $p = Start-Process -FilePath (Join-Path $GameDir 'Hod2.exe') -WorkingDirectory $GameDir -PassThru
    Start-Sleep -Seconds 6
    # Record the monitor modes while the game runs, so every result states its refresh rate.
    $state = @([WP]::Mode('\\.\DISPLAY1'), [WP]::Mode('\\.\DISPLAY2'))
    $elapsed = 6
    $shot = 0
    while ($elapsed -lt $Seconds) {
      Start-Sleep -Seconds 4
      $elapsed += 4
      if ($OutDir -and -not $p.HasExited) {
        Capture (Join-Path $OutDir ("interp{0}_{1:D2}.png" -f $mode, $shot)); $shot++
      }
    }
    $exited = $p.HasExited
    Stop-Process -Id $p.Id -ErrorAction SilentlyContinue; Start-Sleep 1

    $lines = Get-Content $log
    $title = $lines | Where-Object { $_ -match 'CreateFileA "cam\\cp_title\.bin"' } | Select-Object -First 1
    $demo = $lines | Where-Object { $_ -match 'CreateFileA "cam\\cp_tuto\.bin"' } | Select-Object -First 1
    "########## interpolate = $mode (max_fps $MaxFps)$(if ($exited) { ', GAME EXITED EARLY' })"
    $state | ForEach-Object { "  $_" }
    if ($title -and $demo) {
      "title -> demo: {0:N2} s" -f ((Time-Of $demo) - (Time-Of $title)).TotalSeconds
    } else {
      "title -> demo: not reached (title: $([bool]$title), demo: $([bool]$demo))"
    }
    $lines | Select-String 'frame_interp|Surface::Flip:|\[ERROR\]|\[WARN\]' | Select-Object -First 14 |
      ForEach-Object { $_.Line -replace '^\[[^\]]*\] ', '' }
  }
} finally {
  Set-Content -Path $ini -Value $backup -Encoding ASCII -NoNewline
}

# Widescreen comparison: runs the game with the [display] widescreen settings of
# each variant (the core then sets dgVoodoo2 ScalingMode itself), and captures the primary screen at
# fixed times after the title camera is loaded (same game frames every run,
# see quality-compare.ps1). Restores thod2-native.ini and dgVoodoo.conf after.
#
#   .\aspect-capture.ps1 -OutDir <folder>
#   .\aspect-capture.ps1 -Variants base,proj_2d -CaptureAt 1.5,9.5,20 -OutDir <folder>
#
# Variants: base       = widescreen off (4:3 with side bars)
#           proj       = widescreen on, [debug] widescreen_2d = 0 (projection only, 2D stretched)
#           proj_2d    = widescreen on (the feature)
# A "stretched without widescreen" variant is no longer possible: the core
# turns ScalingMode back to stretched_ar when widescreen is off.
param(
  [string[]]$Variants = @('base', 'proj', 'proj_2d'),
  [double[]]$CaptureAt = @(1.5, 9.5),   # seconds after the title camera is loaded
  [string]$GameDir = (Join-Path $PSScriptRoot '..\..\..\test-game\THOTD2'),
  [Parameter(Mandatory = $true)][string]$OutDir
)
. "$PSScriptRoot\winprobe.ps1"
Add-Type -AssemblyName System.Drawing
$GameDir = (Resolve-Path $GameDir).Path
New-Item -ItemType Directory -Force $OutDir | Out-Null
$ini = Join-Path $GameDir 'thod2-native.ini'
$conf = Join-Path $GameDir 'dgVoodoo.conf'
$log = Join-Path $GameDir 'thod2-native.log'
$iniBackup = [IO.File]::ReadAllText($ini)
$confBackup = [IO.File]::ReadAllBytes($conf)

$settings = @{
  'base'    = @{ Widescreen = 0; TwoD = 1 }
  'proj'    = @{ Widescreen = 1; TwoD = 0 }
  'proj_2d' = @{ Widescreen = 1; TwoD = 1 }
}

function Set-IniValue([string]$text, [string]$section, [string]$key, [string]$value) {
  $pattern = "(?ms)(^\[$section\][^\[]*?^)$key\s*=.*?$"
  if ($text -match $pattern) { return [regex]::Replace($text, $pattern, "`${1}$key = $value") }
  if ($text -match "(?m)^\[$section\]") { return [regex]::Replace($text, "(?m)^\[$section\]\s*$", "[$section]`r`n$key = $value") }
  return $text.TrimEnd() + "`r`n`r`n[$section]`r`n$key = $value`r`n"
}

function Capture([string]$path) {
  $b = New-Object System.Drawing.Bitmap 1920, 1080
  $g = [System.Drawing.Graphics]::FromImage($b)
  $g.CopyFromScreen(0, 0, 0, 0, $b.Size)
  $b.Save($path); $g.Dispose(); $b.Dispose()
}

try {
  foreach ($variant in $Variants) {
    $s = $settings[$variant]
    Get-Process Hod2 -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-Process Hod2 -ErrorAction SilentlyContinue | Wait-Process -Timeout 10 -ErrorAction SilentlyContinue
    $text = Set-IniValue $iniBackup 'debug' 'trace_files' '1'
    $text = Set-IniValue $text 'display' 'widescreen' "$($s.Widescreen)"
    $text = Set-IniValue $text 'debug' 'widescreen_2d' "$($s.TwoD)"
    [IO.File]::WriteAllText($ini, $text)
    [IO.File]::WriteAllBytes($conf, $confBackup)

    Remove-Item $log -ErrorAction SilentlyContinue
    $p = Start-Process -FilePath (Join-Path $GameDir 'Hod2.exe') -WorkingDirectory $GameDir -PassThru
    $start = Get-Date
    $title = $null
    while (-not $title -and ((Get-Date) - $start).TotalSeconds -lt 15) {
      Start-Sleep -Milliseconds 20
      if ((Test-Path $log) -and (Select-String -Path $log -Pattern 'cp_title\.bin' -Quiet -ErrorAction SilentlyContinue)) { $title = Get-Date }
    }
    if (-not $title) { $title = Get-Date; "  (title camera not seen in the log, timing from now)" }
    foreach ($t in $CaptureAt) {
      while (((Get-Date) - $title).TotalSeconds -lt $t) { Start-Sleep -Milliseconds 5 }
      Capture (Join-Path $OutDir ("aspect_{0}_t{1}.png" -f $variant, $t))
    }
    $mode = [WP]::Mode('\\.\DISPLAY2')
    Stop-Process -Id $p.Id -ErrorAction SilentlyContinue; Start-Sleep 1
    "########## $variant (widescreen $($s.Widescreen), widescreen_2d $($s.TwoD))  monitor: $mode"
    Select-String -Path $log -Pattern 'widescreen' | ForEach-Object { '  ' + $_.Line }
  }
} finally {
  Get-Process Hod2 -ErrorAction SilentlyContinue | Stop-Process -Force
  Get-Process Hod2 -ErrorAction SilentlyContinue | Wait-Process -Timeout 10 -ErrorAction SilentlyContinue
  [IO.File]::WriteAllText($ini, $iniBackup)
  [IO.File]::WriteAllBytes($conf, $confBackup)
  "restored thod2-native.ini and dgVoodoo.conf"
}

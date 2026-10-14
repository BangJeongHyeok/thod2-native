# Compares dgVoodoo2 image quality options on top of the recommended preset.
# For each variant: reinstall dgVoodoo2 + preset, apply the variant's keys, run
# the game, capture the screen at fixed times after the title camera is loaded
# (same game frames: the game runs on a fixed 60 fps timer and the title starts
# when cam\cp_title.bin is read), and measure GPU usage, flips per second and
# frame intervals. Leaves the recommended preset installed afterwards.
#
#   .\quality-compare.ps1 -OutDir <folder>
#   .\quality-compare.ps1 -Variants base,msaa8 -OutDir <folder>
param(
  [string[]]$Variants = @('base', 'msaa4', 'msaa8', 'aniso16', 'msaa8_aniso16_mip'),
  [double[]]$CaptureAt = @(1.5, 9.5),   # seconds after the title camera is loaded
  [int]$Seconds = 24,
  [string]$DgVoodooDir = (Join-Path $PSScriptRoot '..\..\..\external\dgVoodoo2_87_5'),
  [string]$GameDir = (Join-Path $PSScriptRoot '..\..\..\test-game\THOTD2'),
  [Parameter(Mandatory = $true)][string]$OutDir
)
. "$PSScriptRoot\winprobe.ps1"
Add-Type -AssemblyName System.Drawing
$GameDir = (Resolve-Path $GameDir).Path
$DgVoodooDir = (Resolve-Path $DgVoodooDir).Path
$setup = Join-Path $PSScriptRoot '..\dgvoodoo-setup.py'
$ini = Join-Path $GameDir 'thod2-native.ini'
$log = Join-Path $GameDir 'thod2-native.log'
$backup = Get-Content $ini -Raw

# Keys applied on top of config/dgvoodoo-recommended.ini
$keys = @{
  'base'              = @()
  'msaa4'             = @('DirectX.Antialiasing=4x')
  'msaa8'             = @('DirectX.Antialiasing=8x')
  'aniso16'           = @('DirectX.Filtering=16')
  'msaa8_aniso16_mip' = @('DirectX.Antialiasing=8x', 'DirectX.Filtering=16', 'DirectX.Mipmapping=autogen_bilinear')
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

function Gpu-Percent([int]$processId) {
  # Sum of 3D engine utilization of the game process (GPU performance counters).
  try {
    $samples = (Get-Counter '\GPU Engine(*engtype_3D)\Utilization Percentage' -ErrorAction Stop).CounterSamples
    return ($samples | Where-Object { $_.InstanceName -match "pid_$($processId)_" } | Measure-Object CookedValue -Sum).Sum
  } catch { return $null }
}

try {
  $text = Set-IniValue $backup 'debug' 'trace_ddraw' '1'
  $text = Set-IniValue $text 'debug' 'trace_files' '1'
  Set-Content -Path $ini -Value $text -Encoding ASCII -NoNewline
  foreach ($variant in $Variants) {
    # The game must be gone before dgVoodoo2's DLLs can be replaced.
    Get-Process Hod2 -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-Process Hod2 -ErrorAction SilentlyContinue | Wait-Process -Timeout 10 -ErrorAction SilentlyContinue
    python $setup uninstall $GameDir | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "dgvoodoo-setup uninstall failed for $variant" }
    python $setup install $DgVoodooDir $GameDir | Out-Null
    python $setup preset $GameDir | Out-Null
    if ($keys[$variant].Count -gt 0) { python $setup set $GameDir @($keys[$variant]) | Out-Null }

    Stop-Process -Name Hod2 -ErrorAction SilentlyContinue; Start-Sleep 1
    # A log left over from the previous run would make the title camera look loaded already.
    Remove-Item $log -ErrorAction SilentlyContinue
    $p = Start-Process -FilePath (Join-Path $GameDir 'Hod2.exe') -WorkingDirectory $GameDir -PassThru
    $start = Get-Date
    $gpu = @()
    # Wait until the game reads the title camera; captures are timed from there.
    $title = $null
    while (-not $title -and ((Get-Date) - $start).TotalSeconds -lt 15) {
      Start-Sleep -Milliseconds 20
      if ((Test-Path $log) -and (Select-String -Path $log -Pattern 'cp_title\.bin' -Quiet -ErrorAction SilentlyContinue)) { $title = Get-Date }
    }
    if (-not $title) { $title = Get-Date; "  (title camera not seen in the log, timing from now)" }
    foreach ($t in $CaptureAt) {
      while (((Get-Date) - $title).TotalSeconds -lt $t) { Start-Sleep -Milliseconds 5 }
      Capture (Join-Path $OutDir ("quality_{0}_t{1}.png" -f $variant, $t))
    }
    while (((Get-Date) - $start).TotalSeconds -lt $Seconds) {
      $v = Gpu-Percent $p.Id
      if ($null -ne $v) { $gpu += $v }
    }
    $mode = [WP]::Mode('\\.\DISPLAY2')
    Stop-Process -Id $p.Id -ErrorAction SilentlyContinue; Start-Sleep 1

    "########## $variant  ($($keys[$variant] -join ', '))"
    "  monitor: $mode"
    if ($gpu.Count -gt 0) { "  GPU 3D: avg {0:N1}%  max {1:N1}%  ({2} samples)" -f ($gpu | Measure-Object -Average).Average, ($gpu | Measure-Object -Maximum).Maximum, $gpu.Count }
    Get-Content $log | Select-String 'Surface::Flip:|frame interval' | Select-Object -Last 2 |
      ForEach-Object { '  ' + ($_.Line -replace '^\[[^\]]*\] \[INFO\] ', '') }
  }
} finally {
  Get-Process Hod2 -ErrorAction SilentlyContinue | Stop-Process -Force
  Get-Process Hod2 -ErrorAction SilentlyContinue | Wait-Process -Timeout 10 -ErrorAction SilentlyContinue
  Set-Content -Path $ini -Value $backup -Encoding ASCII -NoNewline
  python $setup uninstall $GameDir | Out-Null
  python $setup install $DgVoodooDir $GameDir | Out-Null
  python $setup preset $GameDir | Out-Null
  "restored recommended preset"
}

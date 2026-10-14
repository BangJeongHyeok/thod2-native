# Starts Hod2.exe in the test copy, answers "Yes" on the startup device dialog ("Attention"),
# then records window/display state, a screen capture and CPU usage, and kills the game.
#
# Usage:
#   .\runtest.ps1 -Tag baseline
#   .\runtest.ps1 -Tag fps -Ini "[HGLCORE]`r`nDispFps=true`r`n"
#
# With -Ini, hglCore.ini in the game folder is written with that content; without it, it is deleted.
param(
  [string]$Tag = "run",
  [int]$Seconds = 12,
  [string]$Ini = "",
  [string]$GameDir = (Join-Path $PSScriptRoot '..\..\..\test-game\THOTD2'),
  [string]$OutDir = $env:TEMP
)
. "$PSScriptRoot\winprobe.ps1"
Add-Type -AssemblyName System.Drawing
$GameDir = (Resolve-Path $GameDir).Path
$iniPath = Join-Path $GameDir 'hglCore.ini'
if ($Ini) { Set-Content -Path $iniPath -Value $Ini -Encoding ASCII; "ini written:"; Get-Content $iniPath } else { Remove-Item $iniPath -ErrorAction SilentlyContinue; "no ini" }
Stop-Process -Name Hod2 -ErrorAction SilentlyContinue; Start-Sleep 1
$p = Start-Process -FilePath (Join-Path $GameDir 'Hod2.exe') -WorkingDirectory $GameDir -PassThru
$clicked = $false
for ($i = 0; $i -lt 20 -and -not $p.HasExited; $i++) {
  Start-Sleep -Milliseconds 500
  foreach ($h in [WP]::Wins($p.Id)) {
    $d = [WP]::Desc($h)
    if ($d -match "#32770" -and $d -match "vis=True") {
      "dialog: $d"; [WP]::Kids($h)
      # IDYES = 6, BM_CLICK = 0x00F5
      [WP]::PostMessage([WP]::GetDlgItem($h, 6), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
      $clicked = $true
    }
  }
  if ($clicked) { break }
}
Start-Sleep -Seconds $Seconds
"=== state"; Probe
# Capture the game window (fallback: 640x480 at the primary monitor's top-left)
$r = New-Object WP+RECT; $r.L = 0; $r.T = 0; $r.R = 640; $r.B = 480
foreach ($h in [WP]::Wins($p.Id)) { if (([WP]::Desc($h)) -match "class='The House" -and [WP]::IsWindowVisible($h)) { [WP]::GetWindowRect($h, [ref]$r) | Out-Null } }
$w = [Math]::Max(1, $r.R - $r.L); $hgt = [Math]::Max(1, $r.B - $r.T)
$b = New-Object System.Drawing.Bitmap $w, $hgt; $g = [System.Drawing.Graphics]::FromImage($b); $g.CopyFromScreen($r.L, $r.T, 0, 0, $b.Size)
$shot = Join-Path $OutDir "$Tag.png"; $b.Save($shot); $g.Dispose(); $b.Dispose()
"saved $shot ($w x $hgt)"
if (-not $p.HasExited) { $c1 = $p.CPU; Start-Sleep 2; $p.Refresh(); "cpu: {0:P0}" -f (($p.CPU - $c1) / 2) } else { "exited code=$($p.ExitCode)" }
Stop-Process -Id $p.Id -ErrorAction SilentlyContinue

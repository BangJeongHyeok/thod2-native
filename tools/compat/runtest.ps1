# 테스트 사본의 Hod2.exe를 실행하고, 시작 확인창("Attention")에서 "예"를 누른 뒤
# 창/디스플레이 상태와 화면 캡처, CPU 사용률을 기록하고 종료한다.
#
# 사용 예:
#   .\runtest.ps1 -Tag baseline
#   .\runtest.ps1 -Tag fps -Ini "[HGLCORE]`r`nDispFps=true`r`n"
#
# -Ini 를 주면 게임 폴더에 hglCore.ini를 그 내용으로 만들고, 주지 않으면 지운다.
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
$r = New-Object WP+RECT; [WP]::GetWindowRect([WP]::GetForegroundWindow(), [ref]$r) | Out-Null
$w = [Math]::Max(640, [Math]::Min(1920, $r.R)); $hgt = [Math]::Max(480, [Math]::Min(1080, $r.B))
$b = New-Object System.Drawing.Bitmap $w, $hgt; $g = [System.Drawing.Graphics]::FromImage($b); $g.CopyFromScreen(0, 0, 0, 0, $b.Size)
$shot = Join-Path $OutDir "$Tag.png"; $b.Save($shot); $g.Dispose(); $b.Dispose()
"saved $shot ($w x $hgt)"
if (-not $p.HasExited) { $c1 = $p.CPU; Start-Sleep 2; $p.Refresh(); "cpu: {0:P0}" -f (($p.CPU - $c1) / 2) } else { "exited code=$($p.ExitCode)" }
Stop-Process -Id $p.Id -ErrorAction SilentlyContinue

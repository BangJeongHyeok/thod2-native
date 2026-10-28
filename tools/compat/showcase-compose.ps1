# Puts an original capture (left) and a thod2-native capture (right) side by
# side at the same height, with a label bar, as a JPEG for README.md
# (docs/images/compare-*.jpg). Captures come from showcase-capture.ps1.
#
#   .\showcase-compose.ps1 -Left orig_play_040.png -Right ours_play_040.png -Out compare-gameplay.jpg
param(
  [Parameter(Mandatory = $true)][string]$Left,
  [Parameter(Mandatory = $true)][string]$Right,
  [Parameter(Mandatory = $true)][string]$Out,
  [int]$Height = 540,
  [string]$LeftLabel = 'Original',
  [string]$RightLabel = 'thod2-native',
  [long]$Quality = 88
)
Add-Type -AssemblyName System.Drawing
$a = [System.Drawing.Image]::FromFile((Resolve-Path $Left).Path)
$b = [System.Drawing.Image]::FromFile((Resolve-Path $Right).Path)
$wa = [int]($a.Width * $Height / $a.Height)
$wb = [int]($b.Width * $Height / $b.Height)
$gap = 8; $bar = 44
$canvas = New-Object System.Drawing.Bitmap ($wa + $gap + $wb), ($Height + $bar)
$g = [System.Drawing.Graphics]::FromImage($canvas)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$g.Clear([System.Drawing.Color]::FromArgb(24, 24, 24))
$g.DrawImage($a, 0, $bar, $wa, $Height)
$g.DrawImage($b, $wa + $gap, $bar, $wb, $Height)
$font = New-Object System.Drawing.Font 'Malgun Gothic', 17, ([System.Drawing.FontStyle]::Bold)
$g.DrawString($LeftLabel, $font, [System.Drawing.Brushes]::Gainsboro, 12, 8)
$g.DrawString($RightLabel, $font, [System.Drawing.Brushes]::Gold, $wa + $gap + 12, 8)
$g.Dispose(); $a.Dispose(); $b.Dispose()
$codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$params = New-Object System.Drawing.Imaging.EncoderParameters 1
$params.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), $Quality
$canvas.Save($Out, $codec, $params)
$canvas.Dispose()
'{0} ({1:N0} bytes)' -f $Out, (Get-Item $Out).Length

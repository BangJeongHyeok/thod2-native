# Test images for [assets] replace_textures (docs/work/assets.md G1-4): a few
# UI sprites redrawn with a vector font at 4x the original size, in yellow so
# they are recognisable on screen. Only the size and the visible area of each
# original are used, not its pixels.
#
#   .\make-test-textures.ps1 -Src <tex_extract.py output folder>
#
# -Src must contain the scr_common.bin and scr_title.bin output. Writes
# <Out>\<texture id>.png (default: the test copy's data\tex).
param([Parameter(Mandatory = $true)][string]$Src,
      [string]$Out = (Join-Path $PSScriptRoot '..\..\..\test-game\THOTD2\data\tex'))
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Force $Out | Out-Null
$items = @(
  @{ File = 'scr_common_058_tex10B1.png'; Id = '10B1'; Text = 'PRESS START BUTTON' },
  @{ File = 'scr_common_057_tex10B0.png'; Id = '10B0'; Text = 'PRESS START BUTTON' },
  @{ File = 'scr_title_076_texDF9.png';  Id = 'DF9';  Text = 'Arcade mode' },
  @{ File = 'scr_title_000_tex78B.png';  Id = '78B';  Text = "$([char]0xA9) SEGA CORPORATION, 1998,1999" },
  @{ File = 'scr_common_059_tex11E2.png'; Id = '11E2'; Text = 'RELOAD' },
  @{ File = 'scr_common_061_tex11E4.png'; Id = '11E4'; Text = "SHOOT OUTSIDE OF`nTHE SCREEN!" },
  @{ File = 'scr_common_032_texBC3.png';  Id = 'BC3';  Text = '1P' }
)
$scale = 4
foreach ($it in $items) {
  $orig = New-Object System.Drawing.Bitmap (Join-Path $Src $it.File)
  # Bounding box of visible pixels in the original.
  $x0 = $orig.Width; $y0 = $orig.Height; $x1 = -1; $y1 = -1
  for ($y = 0; $y -lt $orig.Height; $y++) { for ($x = 0; $x -lt $orig.Width; $x++) {
    if ($orig.GetPixel($x, $y).A -gt 0) { if ($x -lt $x0) {$x0=$x}; if ($x -gt $x1) {$x1=$x}; if ($y -lt $y0) {$y0=$y}; if ($y -gt $y1) {$y1=$y} } } }
  $w = $orig.Width * $scale; $h = $orig.Height * $scale
  $bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'AntiAliasGridFit'
  $g.Clear([System.Drawing.Color]::Transparent)
  $box = New-Object System.Drawing.RectangleF ($x0 * $scale), ($y0 * $scale), (($x1 - $x0 + 1) * $scale), (($y1 - $y0 + 1) * $scale)
  $path = New-Object System.Drawing.Drawing2D.GraphicsPath
  $family = New-Object System.Drawing.FontFamily 'Arial Black'
  $fmt = [System.Drawing.StringFormat]::GenericTypographic.Clone(); $fmt.Alignment = 'Center'
  $path.AddString($it.Text, $family, 0, 100, (New-Object System.Drawing.PointF 0, 0), $fmt)
  $b = $path.GetBounds()
  $m = New-Object System.Drawing.Drawing2D.Matrix
  $m.Translate($box.X, $box.Y); $m.Scale($box.Width / $b.Width, $box.Height / $b.Height); $m.Translate(-$b.X, -$b.Y)
  $path.Transform($m)
  $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 20, 30, 120)), ([single](1.5 * $scale))
  $pen.LineJoin = 'Round'
  $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush $box, ([System.Drawing.Color]::FromArgb(255, 255, 250, 140)), ([System.Drawing.Color]::FromArgb(255, 255, 150, 20)), 90.0
  $g.DrawPath($pen, $path); $g.FillPath($brush, $path)
  $bmp.Save((Join-Path $Out "$($it.Id).png"), [System.Drawing.Imaging.ImageFormat]::Png)
  "$($it.Id): $($orig.Width)x$($orig.Height) -> ${w}x${h}, box $x0,$y0-$x1,$y1"
  $g.Dispose(); $bmp.Dispose(); $orig.Dispose()
}

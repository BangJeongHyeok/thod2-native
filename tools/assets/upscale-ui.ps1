# Upscales textures extracted by tex_extract.py with Real-ESRGAN (ncnn-vulkan).
#
#   .\upscale-ui.ps1 -Exe <realesrgan-ncnn-vulkan.exe> -Src <tex_extract.py output> -Work <work folder> -Out <out folder>
#
# Result: <Out>\<bank>\<texture id>.png (bank = file name without .bin), the
# layout of the game's data\tex folder ([assets] replace_textures). Texture
# ids are not unique across banks, so replacements are kept per bank.
#
# All textures of a bank are packed into atlas pages (one per model) and every
# page is upscaled by one Real-ESRGAN run per model, then cut apart again. A
# font is one bank of glyph textures, so all its glyphs get the same treatment
# and the same border handling. Each texture sits in a cell with $Pad pixels
# of margin that repeats its edge pixels (alpha included); fully transparent
# pixels get the colour of the nearest visible pixel of the same texture (the
# game's transparent pixels are black, which Real-ESRGAN otherwise mixes into
# thin or small sprites). Alpha is not changed.
#
# Model per texture: VQ (layout 0x03) or fully opaque ones (backgrounds and photos) ->
# realesrgan-x4plus; ones with transparency (text, icons, logos) ->
# realesrgan-x4plus-anime. Tiles of full-screen pictures are put back together
# before upscaling (PhotoUnits). Scale 4; the largest UI texture is 512 px, so
# results stay within the 2048 px texture limit measured with dgVoodoo2.
#
# Input and output contain game assets: keep them out of the repository.
param([Parameter(Mandatory = $true)][string]$Exe, [Parameter(Mandatory = $true)][string]$Src,
      [Parameter(Mandatory = $true)][string]$Work, [Parameter(Mandatory = $true)][string]$Out,
      [int]$Pad = 8, [int]$PageSize = 1536)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class UiAtlas {
  static int[] Read(Bitmap b) {
    var d = b.LockBits(new Rectangle(0, 0, b.Width, b.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
    var px = new int[b.Width * b.Height];
    for (int y = 0; y < b.Height; y++) Marshal.Copy(d.Scan0 + y * d.Stride, px, y * b.Width, b.Width);
    b.UnlockBits(d);
    return px;
  }
  static Bitmap Make(int w, int h, int[] px) {
    var b = new Bitmap(w, h, PixelFormat.Format32bppArgb);
    var d = b.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
    for (int y = 0; y < h; y++) Marshal.Copy(px, y * w, d.Scan0 + y * d.Stride, w);
    b.UnlockBits(d);
    return b;
  }
  // Cell = texture with pad pixels around it. The margin repeats the nearest
  // edge pixel, alpha included (Real-ESRGAN scales alpha by interpolation, so
  // a transparent margin would make the edge of an opaque picture partly
  // transparent: a dark line where tiles or mirrored copies meet). Then
  // pixels with alpha 0 take the colour of the nearest visible pixel; alpha
  // stays 0.
  static int[] Cell(string file, int pad, out int cw, out int ch) {
    int[] src; int w, h;
    using (var b = new Bitmap(file)) { w = b.Width; h = b.Height; src = Read(b); }
    cw = w + 2 * pad; ch = h + 2 * pad;
    var cell = new int[cw * ch];
    var done = new bool[cw * ch];
    var queue = new Queue<int>();
    for (int y = 0; y < ch; y++)
      for (int x = 0; x < cw; x++) {
        int sx = Math.Min(Math.Max(x - pad, 0), w - 1), sy = Math.Min(Math.Max(y - pad, 0), h - 1);
        int i = y * cw + x, c = src[sy * w + sx];
        cell[i] = c;
        if ((c >> 24) != 0) { done[i] = true; queue.Enqueue(i); }
      }
    int W = cw, H = ch;
    while (queue.Count > 0) {
      int i = queue.Dequeue(), x = i % W, y = i / W, rgb = cell[i] & 0xFFFFFF;
      for (int k = 0; k < 4; k++) {
        int nx = x + (k == 0 ? -1 : k == 1 ? 1 : 0), ny = y + (k == 2 ? -1 : k == 3 ? 1 : 0);
        if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
        int j = ny * W + nx;
        if (done[j]) continue;
        done[j] = true;
        cell[j] = (cell[j] & unchecked((int)0xFF000000)) | rgb;
        queue.Enqueue(j);
      }
    }
    return cell;
  }
  // Packs files (already sorted by height, tallest first) into pages of at
  // most pageSize x pageSize, writes <prefix>_<page>.png and returns
  // (page, x, y) per file, x/y of the texture inside its page.
  public static int[] Pack(string[] files, int pad, int pageSize, string prefix) {
    var result = new int[files.Length * 3];
    var cells = new List<int[]>(); var sizes = new List<int[]>();
    for (int i = 0; i < files.Length; i++) { int cw, ch; cells.Add(Cell(files[i], pad, out cw, out ch)); sizes.Add(new[] { cw, ch }); }
    int page = 0, x = 0, y = 0, rowH = 0, pageW = 0, pageH = 0;
    var placed = new List<int>();
    Action flush = () => {
      if (placed.Count == 0) return;
      var px = new int[pageW * pageH];
      foreach (int i in placed) {
        int[] c = cells[i]; int cw = sizes[i][0], ch = sizes[i][1], ox = result[i * 3 + 1] - pad, oy = result[i * 3 + 2] - pad;
        for (int r = 0; r < ch; r++) Array.Copy(c, r * cw, px, (oy + r) * pageW + ox, cw);
      }
      using (var b = Make(pageW, pageH, px)) b.Save(prefix + "_" + page + ".png", ImageFormat.Png);
      placed.Clear();
    };
    for (int i = 0; i < files.Length; i++) {
      int cw = sizes[i][0], ch = sizes[i][1];
      if (x + cw > pageSize) { x = 0; y += rowH; rowH = 0; }
      if (y + ch > pageSize && placed.Count > 0) { flush(); page++; x = 0; y = 0; rowH = 0; pageW = 0; pageH = 0; }
      result[i * 3] = page; result[i * 3 + 1] = x + pad; result[i * 3 + 2] = y + pad;
      placed.Add(i);
      x += cw; rowH = Math.Max(rowH, ch);
      pageW = Math.Max(pageW, x); pageH = Math.Max(pageH, y + ch);
    }
    flush();
    return result;
  }
  // True if every pixel is fully opaque (backgrounds and photos).
  public static bool IsOpaque(string file) {
    using (var b = new Bitmap(file)) {
      foreach (int c in Read(b)) if ((c >> 24 & 0xFF) != 0xFF) return false;
      return true;
    }
  }
  // Puts tiles (row-major, cols per row, all w x h) together into one picture.
  // Returns the mean colour difference across the tile seams divided by the
  // mean difference between neighbouring pixels next to the seams (about 1
  // when the tiles continue each other); writes the picture to outPath.
  public static double Assemble(string[] tiles, int cols, string outPath) {
    int rows = tiles.Length / cols, w, h;
    using (var b = new Bitmap(tiles[0])) { w = b.Width; h = b.Height; }
    int W = w * cols, H = h * rows;
    var px = new int[W * H];
    for (int i = 0; i < tiles.Length; i++) {
      int[] t; using (var b = new Bitmap(tiles[i])) t = Read(b);
      int ox = i % cols * w, oy = i / cols * h;
      for (int r = 0; r < h; r++) Array.Copy(t, r * w, px, (oy + r) * W + ox, w);
    }
    Func<int, int, int> diff = (a, b) => Math.Abs((a >> 16 & 255) - (b >> 16 & 255)) + Math.Abs((a >> 8 & 255) - (b >> 8 & 255)) + Math.Abs((a & 255) - (b & 255));
    double seam = 0, inner = 0; long n = 0;
    for (int y = 0; y < H; y++)
      for (int k = 1; k < cols; k++) { int x = k * w; seam += diff(px[y * W + x - 1], px[y * W + x]); inner += diff(px[y * W + x - 2], px[y * W + x - 1]) + diff(px[y * W + x], px[y * W + x + 1]); n++; }
    for (int k = 1; k < rows; k++)
      for (int x = 0; x < W; x++) { int y = k * h; seam += diff(px[(y - 1) * W + x], px[y * W + x]); inner += diff(px[(y - 2) * W + x], px[(y - 1) * W + x]) + diff(px[y * W + x], px[(y + 1) * W + x]); n++; }
    using (var b = Make(W, H, px)) b.Save(outPath, ImageFormat.Png);
    return n == 0 ? double.MaxValue : (seam / n) / (inner / (2.0 * n) + 1.0);
  }
  // Cuts rects (x, y, w, h in atlas input pixels) out of an upscaled page.
  public static void Cut(string page, int scale, int[] rects, string[] outs) {
    int[] px; int w;
    using (var b = new Bitmap(page)) { w = b.Width; px = Read(b); }
    for (int i = 0; i < outs.Length; i++) {
      int x = rects[i * 4] * scale, y = rects[i * 4 + 1] * scale, cw = rects[i * 4 + 2] * scale, ch = rects[i * 4 + 3] * scale;
      var part = new int[cw * ch];
      for (int r = 0; r < ch; r++) Array.Copy(px, (y + r) * w + x, part, r * cw, cw);
      using (var b = Make(cw, ch, part)) b.Save(outs[i], ImageFormat.Png);
    }
  }
}
'@

$models = [ordered]@{ photo = 'realesrgan-x4plus'; graphic = 'realesrgan-x4plus-anime' }
$scale = 4; $maxSize = 2048
$srcFull = (Resolve-Path $Src).Path
New-Item -ItemType Directory -Force $Work, $Out | Out-Null
$workFull = (Resolve-Path $Work).Path; $outFull = (Resolve-Path $Out).Path
$rows = Import-Csv (Join-Path $srcFull 'manifest.csv')
$jobs = @()
foreach ($kind in $models.Keys) {
  $in = Join-Path $workFull "$kind\in"; $res = Join-Path $workFull "$kind\out"
  foreach ($d in $in, $res) { if (Test-Path $d) { Remove-Item -Recurse -Force $d }; New-Item -ItemType Directory -Force $d | Out-Null }
}
$pictures = Join-Path $workFull 'pictures'
if (Test-Path $pictures) { Remove-Item -Recurse -Force $pictures }
New-Item -ItemType Directory -Force $pictures | Out-Null

# A unit is one image in an atlas page: a texture, or a picture put together
# from tiles (members with their offset inside the unit).
function Unit($file, $w, $h, $members) { [pscustomobject]@{ file = $file; w = $w; h = $h; members = $members } }
function Member($row, $dx, $dy) { [pscustomobject]@{ id = $row.texture_id; dx = $dx; dy = $dy; w = [int]$row.width; h = [int]$row.height } }

# Full-screen pictures are stored as tiles of one size, row-major, 640 px per
# row (title and stage result backgrounds, loading screens). Upscaled one by
# one, every tile edge shows as a seam, so runs of such tiles are put together
# first and kept as a picture when the tiles continue each other across the
# seams (Assemble score below $MaxSeam).
$MaxSeam = 2.0
function PhotoUnits($bank, $items) {
  $units = @(); $i = 0
  while ($i -lt $items.Count) {
    $j = $i
    while ($j + 1 -lt $items.Count -and [int]$items[$j + 1].index -eq [int]$items[$j].index + 1 -and
           $items[$j + 1].width -eq $items[$i].width -and $items[$j + 1].height -eq $items[$i].height) { $j++ }
    $w = [int]$items[$i].width; $h = [int]$items[$i].height; $run = @($items[$i..$j])
    $cols = if (640 % $w -eq 0) { 640 / $w } else { 0 }
    $perPicture = $cols * [math]::Ceiling(480 / $h)
    $k = 0
    while ($cols -ge 2 -and $k -lt $run.Count) {
      $left = $run.Count - $k
      $take = if ($left -ge $perPicture) { $perPicture } elseif ($left -ge 2 * $cols) { $left - $left % $cols } else { 0 }
      if ($take -eq 0) { break }
      $tiles = @($run[$k..($k + $take - 1)])
      $file = Join-Path $pictures ("{0}_{1:D3}.png" -f $bank, [int]$tiles[0].index)
      $score = [UiAtlas]::Assemble([string[]]($tiles | ForEach-Object { Join-Path $srcFull $_.file }), $cols, $file)
      $ok = $score -le $MaxSeam
      "  $bank #$($tiles[0].index)..#$($tiles[-1].index): $cols x $($take / $cols) tiles of $($w)x$($h), seam score {0:N2} -> {1}" -f $score, $(if ($ok) { 'picture' } else { 'tiles' })
      if ($ok) {
        $members = for ($t = 0; $t -lt $take; $t++) { Member $tiles[$t] ($t % $cols * $w) ([math]::Floor($t / $cols) * $h) }
        $script:units += Unit $file ($cols * $w) ($take / $cols * $h) $members
      } else {
        foreach ($t in $tiles) { $script:units += Unit (Join-Path $srcFull $t.file) $w $h @(Member $t 0 0) }
      }
      $k += $take
    }
    for (; $k -lt $run.Count; $k++) { $script:units += Unit (Join-Path $srcFull $run[$k].file) $w $h @(Member $run[$k] 0 0) }
    $i = $j + 1
  }
}

foreach ($group in ($rows | Group-Object bank)) {
  $bank = $group.Name -replace '\.bin$', ''
  $usable = @($group.Group | Where-Object { [int]$_.width * $scale -le $maxSize -and [int]$_.height * $scale -le $maxSize } |
              Sort-Object { [int]$_.index })
  $opaque = @{}
  # Backgrounds and photos: VQ textures, and opaque tiles that form a
  # full-screen picture (title, date and branch screens). Opaque textures that
  # are not part of a picture (for example credits text on black) and
  # everything with transparency (text, icons, logos) are graphics.
  $vq = @{}; $candidates = @()
  foreach ($r in $usable) {
    $vq[$r.file] = (([Convert]::ToInt32($r.format, 16) -shr 8) -band 0xFF) -eq 3
    if ($vq[$r.file] -or [UiAtlas]::IsOpaque((Join-Path $srcFull $r.file))) { $candidates += $r }
  }
  $script:units = @()
  if ($candidates.Count) { PhotoUnits $bank $candidates }
  $photoUnits = @($script:units | Where-Object { $_.members.Count -gt 1 -or $vq[(Split-Path $_.file -Leaf)] })
  $photoIds = @{}; foreach ($u in $photoUnits) { foreach ($m in $u.members) { $photoIds[$m.id] = 1 } }
  $graphicUnits = @($usable | Where-Object { -not $photoIds.ContainsKey($_.texture_id) } |
                    ForEach-Object { Unit (Join-Path $srcFull $_.file) ([int]$_.width) ([int]$_.height) @(Member $_ 0 0) })
  foreach ($kind in $models.Keys) {
    $units = @($(if ($kind -eq 'photo') { $photoUnits } else { $graphicUnits }) | Sort-Object { $_.h }, { $_.w } -Descending)
    if ($units.Count -eq 0) { continue }
    $prefix = Join-Path $workFull "$kind\in\$bank"
    $layout = [UiAtlas]::Pack([string[]]($units | ForEach-Object { $_.file }), $Pad, $PageSize, $prefix)
    for ($i = 0; $i -lt $units.Count; $i++) {
      foreach ($m in $units[$i].members) {
        $jobs += [pscustomobject]@{ kind = $kind; bank = $bank; id = $m.id; page = "$($bank)_$($layout[$i * 3]).png"
                                    x = $layout[$i * 3 + 1] + $m.dx; y = $layout[$i * 3 + 2] + $m.dy; w = $m.w; h = $m.h }
      }
    }
  }
}
foreach ($kind in $models.Keys) {
  $in = Join-Path $workFull "$kind\in"; $res = Join-Path $workFull "$kind\out"
  $pages = (Get-ChildItem $in -Filter *.png).Count
  "$($models[$kind]): $(@($jobs | Where-Object kind -eq $kind).Count) textures in $pages pages"
  if ($pages -eq 0) { continue }
  # One Real-ESRGAN process for all pages of this model.
  $log = Join-Path $workFull "$kind\realesrgan.log"
  $p = Start-Process -FilePath $Exe -ArgumentList "-i `"$in`" -o `"$res`" -n $($models[$kind]) -s $scale -f png" `
                     -NoNewWindow -Wait -PassThru -RedirectStandardError $log
  if ($p.ExitCode -ne 0) { throw "realesrgan-ncnn-vulkan failed ($($p.ExitCode)), see $log" }
}
$written = 0
foreach ($pageJobs in ($jobs | Group-Object kind, page)) {
  $first = $pageJobs.Group[0]
  $page = Join-Path $workFull "$($first.kind)\out\$($first.page)"
  if (-not (Test-Path $page)) { "missing output $page"; continue }
  $folder = Join-Path $outFull $first.bank
  New-Item -ItemType Directory -Force $folder | Out-Null
  $rects = [int[]]($pageJobs.Group | ForEach-Object { $_.x, $_.y, $_.w, $_.h })
  $outs = [string[]]($pageJobs.Group | ForEach-Object { Join-Path $folder "$($_.id).png" })
  [UiAtlas]::Cut($page, $scale, $rects, $outs)
  $written += $outs.Count
}
"$written of $($rows.Count) textures written to $outFull"

# Checks the result of upscale_ui.py: every upscaled image is shrunk back to
# the original size and compared with the original texture. A wrong image (for
# example another texture's result) or a broken one gives a large difference.
#
#   .\verify-upscale.ps1 -Orig <tex_extract.py output> -Up <upscale_ui.py output> [-Threshold 12]
#
# Writes <Up>\verify.csv (file, bank, texture id, size check, mean difference
# per channel 0..255, ok) and prints the textures above -Threshold. The score is
# the larger of the alpha difference and the colour difference weighted by the
# original's opacity.
param([Parameter(Mandatory = $true)][string]$Orig, [Parameter(Mandatory = $true)][string]$Up,
      [double]$Threshold = 12)
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class UpscaleCheck {
  static byte[] Pixels(Bitmap b) {
    var d = b.LockBits(new Rectangle(0, 0, b.Width, b.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
    var buf = new byte[b.Width * b.Height * 4];
    for (int y = 0; y < b.Height; y++)
      Marshal.Copy(d.Scan0 + y * d.Stride, buf, y * b.Width * 4, b.Width * 4);
    b.UnlockBits(d);
    return buf;
  }
  // Returns -1 if the upscaled image is not an integer multiple of the original,
  // otherwise the mean absolute difference (0..255) over B, G, R of visible
  // pixels and over alpha of all pixels.
  public static double Compare(string orig, string up) {
    using (var o = new Bitmap(orig)) using (var u = new Bitmap(up)) {
      if (u.Width % o.Width != 0 || u.Height % o.Height != 0 || u.Width / o.Width != u.Height / o.Height) return -1;
      using (var small = new Bitmap(o.Width, o.Height, PixelFormat.Format32bppArgb))
      using (var g = Graphics.FromImage(small)) {
        g.CompositingMode = CompositingMode.SourceCopy;
        g.InterpolationMode = InterpolationMode.HighQualityBilinear;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        using (var attr = new ImageAttributes()) {
          attr.SetWrapMode(WrapMode.TileFlipXY);
          g.DrawImage(u, new Rectangle(0, 0, o.Width, o.Height), 0, 0, u.Width, u.Height, GraphicsUnit.Pixel, attr);
        }
        byte[] a = Pixels(o), b = Pixels(small);
        // Colour differences count by the original's opacity, so the colour of
        // nearly transparent edge pixels hardly matters.
        double colour = 0, alpha = 0, visible = 0; long n = a.Length / 4;
        for (long i = 0; i < a.Length; i += 4) {
          alpha += Math.Abs(a[i + 3] - b[i + 3]);
          double weight = a[i + 3] / 255.0;
          visible += weight;
          for (int c = 0; c < 3; c++) colour += weight * Math.Abs(a[i + c] - b[i + c]);
        }
        double cm = visible > 0 ? colour / (visible * 3) : 0;
        return Math.Max(cm, alpha / n);
      }
    }
  }
  // Compare for every pair, on all cores; -2 when the upscaled file is missing.
  public static double[] CompareAll(string[] origs, string[] ups) {
    var result = new double[origs.Length];
    System.Threading.Tasks.Parallel.For(0, origs.Length, i => {
      result[i] = System.IO.File.Exists(ups[i]) ? Compare(origs[i], ups[i]) : -2;
    });
    return result;
  }
}
'@
$rows = Import-Csv (Join-Path $Orig 'manifest.csv')
$origFull = (Resolve-Path $Orig).Path
$upFull = (Resolve-Path $Up).Path
$origs = [string[]]($rows | ForEach-Object { Join-Path $origFull $_.file })
$ups = [string[]]($rows | ForEach-Object { Join-Path (Join-Path $upFull ($_.bank -replace '\.bin$', '')) "$($_.texture_id).png" })
$scores = [UpscaleCheck]::CompareAll($origs, $ups)
$result = for ($i = 0; $i -lt $rows.Count; $i++) {
  $r = $rows[$i]; $score = $scores[$i]
  [pscustomobject]@{ file = $r.file; bank = $r.bank; texture_id = $r.texture_id
                     difference = [math]::Round($score, 2); ok = ($score -ge 0 -and $score -le $Threshold) }
}
$result | Export-Csv (Join-Path $Up 'verify.csv') -NoTypeInformation
$bad = @($result | Where-Object { -not $_.ok })
"$($result.Count) checked, $($bad.Count) above $Threshold (-1 = wrong size, -2 = missing)"
$bad | Sort-Object difference -Descending | Select-Object -First 40 | Format-Table -AutoSize | Out-String -Width 200

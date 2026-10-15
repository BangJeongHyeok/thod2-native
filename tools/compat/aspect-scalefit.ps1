# For each variant image, finds the horizontal scale s (about the screen centre)
# that best maps the 4:3 base image onto it: base pixel x -> 960 + (x - 960) * s.
# s = 1.00 means the variant shows the base picture at the same proportions.
#
#   .\aspect-scalefit.ps1 -Dir <aspect-capture.ps1 OutDir> -Time 9.5 [-Y0 750 -Y1 1050]
#
# Expected for a correct Hor+ result: best s = 1.00 (the centre 1440x1080 of the
# 16:9 picture equals the 4:3 picture). Plain stretching gives s = 1.33.
param([Parameter(Mandatory = $true)][string]$Dir, [Parameter(Mandatory = $true)][string]$Time, [int]$Y0 = 0, [int]$Y1 = 1080)
$src = @'
using System; using System.Drawing; using System.Drawing.Imaging; using System.Runtime.InteropServices;
public static class ScaleFit {
  static byte[] Gray(Bitmap b, out int w, out int h) {
    w = b.Width; h = b.Height;
    var d = b.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb);
    var raw = new byte[d.Stride * h]; Marshal.Copy(d.Scan0, raw, 0, raw.Length); b.UnlockBits(d);
    var g = new byte[w * h];
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { int i = y * d.Stride + x * 3; g[y * w + x] = (byte)((raw[i] + raw[i + 1] * 2 + raw[i + 2]) / 4); }
    return g;
  }
  public static string Fit(string basePath, string otherPath, int y0, int y1) {
    int w, h, w2, h2;
    var a = Gray(new Bitmap(basePath), out w, out h);
    var b = Gray(new Bitmap(otherPath), out w2, out h2);
    double best = 1e9, bestS = 0; var sb = new System.Text.StringBuilder();
    for (double s = 0.80; s <= 1.401; s += 0.01) {
      double sum = 0; int n = 0;
      for (int y = y0; y < y1; y += 3) for (int x = 250; x < 1670; x += 3) {
        int x2 = (int)Math.Round(960 + (x - 960) * s);
        if (x2 < 0 || x2 >= w2) continue;
        sum += Math.Abs(a[y * w + x] - b[y * w2 + x2]); n++;
      }
      double mad = sum / n;
      if (mad < best) { best = mad; bestS = s; }
      if (Math.Abs(s - 1.0) < 0.001 || Math.Abs(s - 1.33) < 0.001) sb.AppendFormat(" s={0:F2}:{1:F1}", s, mad);
    }
    return String.Format("best s={0:F2} (mean abs diff {1:F1}),{2}", bestS, best, sb);
  }
}
'@
Add-Type -TypeDefinition $src -ReferencedAssemblies System.Drawing -ErrorAction SilentlyContinue
foreach ($v in 'base', 'stretch', 'proj', 'proj_2d') {
  "{0,-8} {1}" -f $v, [ScaleFit]::Fit("$Dir\aspect_base_t$Time.png", "$Dir\aspect_${v}_t$Time.png", $Y0, $Y1)
}

$src = @'
using System; using System.Text; using System.Runtime.InteropServices; using System.Collections.Generic;
public static class WP {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h, int msg, IntPtr w, StringBuilder l);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int i);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr d, int id);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, int m, IntPtr w, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] public struct DEVMODE {
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst=32)] public string dmDeviceName; public short dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra; public int dmFields; public int dmPositionX, dmPositionY; public int dmDisplayOrientation, dmDisplayFixedOutput; public short dmColor, dmDuplex, dmYResolution, dmTTOption, dmCollate;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst=32)] public string dmFormName; public short dmLogPixels; public int dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags, dmDisplayFrequency; }
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern bool EnumDisplaySettings(string dev, int mode, ref DEVMODE dm);
  public static string Desc(IntPtr h) { var t=new StringBuilder(256); var c=new StringBuilder(256); GetWindowText(h,t,256); GetClassName(h,c,256); RECT r; GetWindowRect(h,out r); return String.Format("hwnd={0:X} vis={1} class='{2}' title='{3}' rect=({4},{5})-({6},{7}) style={8:X8} ex={9:X8}", h.ToInt64(), IsWindowVisible(h), c, t, r.L,r.T,r.R,r.B, GetWindowLong(h,-16), GetWindowLong(h,-20)); }
  public static List<IntPtr> Wins(uint target) { var res = new List<IntPtr>(); EnumWindows((h,l) => { uint pid; GetWindowThreadProcessId(h, out pid); if (pid==target) res.Add(h); return true; }, IntPtr.Zero); return res; }
  public static List<string> Kids(IntPtr parent) { var r=new List<string>(); EnumChildWindows(parent,(h,l)=>{ var c=new StringBuilder(64); GetClassName(h,c,64); var t=new StringBuilder(2048); SendMessage(h,0x000D,(IntPtr)2048,t); r.Add(String.Format("  child class={0} text='{1}'", c, t.ToString().Trim())); return true; }, IntPtr.Zero); return r; }
  public static string Mode(string dev) { var d = new DEVMODE(); d.dmSize=(short)Marshal.SizeOf(typeof(DEVMODE)); EnumDisplaySettings(dev, -1, ref d); return String.Format("{0} {1}x{2} {3}bpp {4}Hz pos=({5},{6})", dev, d.dmPelsWidth, d.dmPelsHeight, d.dmBitsPerPel, d.dmDisplayFrequency, d.dmPositionX, d.dmPositionY); }
}
'@
Add-Type -TypeDefinition $src -ErrorAction SilentlyContinue

function Probe {
  $p = Get-Process Hod2 -ErrorAction SilentlyContinue
  if (-not $p) { "Hod2 not running"; } else {
    "pid=$($p.Id) cpu=$([math]::Round($p.CPU,1))s ws=$([math]::Round($p.WorkingSet64/1MB))MB"
    foreach ($h in [WP]::Wins($p.Id)) { $d = [WP]::Desc($h); if ($d -match 'vis=True' -or $d -match "class='The House") { $d; if ($d -match '#32770') { [WP]::Kids($h) } } }
    "foreground: " + [WP]::Desc([WP]::GetForegroundWindow())
  }
  [WP]::Mode('\\.\DISPLAY1'); [WP]::Mode('\\.\DISPLAY2')
}

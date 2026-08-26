# Captures the Raze window to a PNG.
#
# The VR mirror shows eye 0, so this is a way to look at what the headset is
# being shown without wearing it - which is what makes it possible to iterate on
# rendering without spending a testing round.
#
#   powershell -File tools/grab-window.ps1 out.png

param([string]$Out = "shot.png")

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win {
    [DllImport("user32.dll")] public static extern IntPtr FindWindow(string c, string n);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@

$p = Get-Process -Name raze -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $p) { Write-Error "raze is not running"; exit 1 }

$h = $p.MainWindowHandle
[void][Win]::SetForegroundWindow($h)
Start-Sleep -Milliseconds 700

$r = New-Object Win+RECT
[void][Win]::GetClientRect($h, [ref]$r)
$pt = New-Object Win+POINT
[void][Win]::ClientToScreen($h, [ref]$pt)

$w = $r.R - $r.L
$hgt = $r.B - $r.T
if ($w -le 0 -or $hgt -le 0) { Write-Error "bad client rect"; exit 1 }

$bmp = New-Object System.Drawing.Bitmap $w, $hgt
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($pt.X, $pt.Y, 0, 0, $bmp.Size)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()

Write-Output ("{0}: {1}x{2}" -f $Out, $w, $hgt)

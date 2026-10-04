# Drives the presenter window the way a player's mouse does: finds the
# "TSPresenter" window, then posts mouse moves and clicks at points given in
# GAME coordinates (the 640x400 menus), converted to the window's client area
# with the same letterboxing present.c uses. -Snap saves the presenter
# window's own picture (PrintWindow on its handle: nothing else on the
# desktop) at the given second. docs/presenter.md.
#
#   powershell -File tools\present_drive.ps1 -Clicks "15:470,200" -Snap "40:work\present.png"
#   (seconds after the window appears : game x,y; one string, since -File binds
#   only the first value of an array parameter)
param([string]$Clicks = "", [string]$Snap = "", [int]$GameW = 640, [int]$GameH = 400)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class W {
  [DllImport("user32.dll", CharSet = CharSet.Ansi)] public static extern IntPtr FindWindow(string c, IntPtr n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
"@
[void][W]::SetProcessDPIAware()   # the presenter's real pixels, not a 96-dpi view of them

$h = [IntPtr]::Zero
$t0 = Get-Date
while ($h -eq [IntPtr]::Zero -and ((Get-Date) - $t0).TotalSeconds -lt 60) {
  Start-Sleep -Milliseconds 200
  $h = [W]::FindWindow("TSPresenter", [IntPtr]::Zero)   # a $null string marshals as "" and matches only untitled windows
}
if ($h -eq [IntPtr]::Zero) { Write-Output "no presenter window"; exit 1 }
$t0 = Get-Date
Write-Output "presenter window $h"

$events = @()
foreach ($c in ($Clicks -split '[ ;]+' | Where-Object { $_ })) { $events += ,@('click', $c) }
if ($Snap) { $events += ,@('snap', $Snap) }
foreach ($e in ($events | Sort-Object { [double](($_[1] -split ':')[0]) })) {
  $at, $rest = $e[1] -split ':', 2
  while (((Get-Date) - $t0).TotalSeconds -lt [double]$at) { Start-Sleep -Milliseconds 100 }
  $r = New-Object W+RECT
  [void][W]::GetClientRect($h, [ref]$r)
  $cw = $r.R; $ch = $r.B
  if ($e[0] -eq 'snap') {
    $bmp = New-Object System.Drawing.Bitmap $cw, $ch
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $dc = $g.GetHdc()
    [void][W]::PrintWindow($h, $dc, 3)                        # PW_CLIENTONLY | PW_RENDERFULLCONTENT
    $g.ReleaseHdc($dc); $g.Dispose()
    $bmp.Save($rest, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
    Write-Output ("{0:N1}s snap {1}x{2} -> {3}" -f ((Get-Date) - $t0).TotalSeconds, $cw, $ch, $rest)
    continue
  }
  $gx, $gy = ($rest -split ',') | ForEach-Object { [int]$_ }
  # the game's picture letterboxed into the client area (present.c, fit)
  if ($cw * $GameH -gt $ch * $GameW) { $ph = $ch; $pw = [int]($ch * $GameW / $GameH) } else { $pw = $cw; $ph = [int]($cw * $GameH / $GameW) }
  $ox = [int](($cw - $pw) / 2); $oy = [int](($ch - $ph) / 2)
  $x = $ox + [int]($gx * $pw / $GameW); $y = $oy + [int]($gy * $ph / $GameH)
  $lp = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
  [void][W]::PostMessage($h, 0x200, [IntPtr]0, $lp)          # WM_MOUSEMOVE
  Start-Sleep -Milliseconds 300
  [void][W]::PostMessage($h, 0x201, [IntPtr]1, $lp)          # WM_LBUTTONDOWN, MK_LBUTTON
  Start-Sleep -Milliseconds 300
  [void][W]::PostMessage($h, 0x202, [IntPtr]0, $lp)          # WM_LBUTTONUP
  Write-Output ("{0:N1}s click game {1},{2} -> client {3},{4}" -f ((Get-Date) - $t0).TotalSeconds, $gx, $gy, $x, $y)
}

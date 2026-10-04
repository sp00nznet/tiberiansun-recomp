# Drives the presenter window the way a player's mouse does, for testing it on
# an offstage virtual monitor: finds the "TSPresenter" window, then posts mouse
# moves and clicks at points given in GAME coordinates (800x600 menus),
# converted to the window's client area with the same letterboxing present.c
# uses. docs/presenter.md.
#
#   powershell -File tools\present_drive.ps1 -Clicks "15:700,100 20:700,140"
#   (seconds after the window appears : game x,y; one string, since -File binds
#   only the first value of an array parameter)
param([string]$Clicks = "")

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class W {
  [DllImport("user32.dll", CharSet = CharSet.Ansi)] public static extern IntPtr FindWindow(string c, IntPtr n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
}
"@

$h = [IntPtr]::Zero
$t0 = Get-Date
while ($h -eq [IntPtr]::Zero -and ((Get-Date) - $t0).TotalSeconds -lt 60) {
  Start-Sleep -Milliseconds 200
  $h = [W]::FindWindow("TSPresenter", [IntPtr]::Zero)   # a $null string marshals as "" and matches only untitled windows
}
if ($h -eq [IntPtr]::Zero) { Write-Output "no presenter window"; exit 1 }
$t0 = Get-Date
Write-Output "presenter window $h"

foreach ($c in ($Clicks -split '[ ;]+' | Where-Object { $_ })) {
  $at, $xy = $c -split ':'
  $gx, $gy = ($xy -split ',') | ForEach-Object { [int]$_ }
  while (((Get-Date) - $t0).TotalSeconds -lt [double]$at) { Start-Sleep -Milliseconds 100 }
  $r = New-Object W+RECT
  [void][W]::GetClientRect($h, [ref]$r)
  $cw = $r.R; $ch = $r.B
  # the 800x600 picture letterboxed into the client area (present.c, fit)
  if ($cw * 600 -gt $ch * 800) { $ph = $ch; $pw = [int]($ch * 800 / 600) } else { $pw = $cw; $ph = [int]($cw * 600 / 800) }
  $ox = [int](($cw - $pw) / 2); $oy = [int](($ch - $ph) / 2)
  $x = $ox + [int]($gx * $pw / 800); $y = $oy + [int]($gy * $ph / 600)
  $lp = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
  [void][W]::PostMessage($h, 0x200, [IntPtr]0, $lp)          # WM_MOUSEMOVE
  Start-Sleep -Milliseconds 300
  [void][W]::PostMessage($h, 0x201, [IntPtr]1, $lp)          # WM_LBUTTONDOWN, MK_LBUTTON
  Start-Sleep -Milliseconds 300
  [void][W]::PostMessage($h, 0x202, [IntPtr]0, $lp)          # WM_LBUTTONUP
  Write-Output ("{0:N1}s click game {1},{2} -> client {3},{4}" -f ((Get-Date) - $t0).TotalSeconds, $gx, $gy, $x, $y)
}

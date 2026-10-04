# Presenter

The display is redalert2-recomp's presenter (its docs/presenter.md has the
design): the game draws into a virtual primary surface in system memory, and
a Direct3D 11 window shows it scaled. F12 cycles the scalings, F11 is
borderless fullscreen, F10 opens the settings menu; the window class is
`TSPresenter` and its settings live in `build\ts.ini`.

What differs for Tiberian Sun:

- **The game locks the primary to draw** (Red Alert 2 blits into it). The
  presenter's copy takes the same lock as the game's Lock/Unlock, and only
  memcpys inside it (bringup.md 3).
- **The menus are 640x400**, not 800x600, and drawn by the game; above
  640x400 the game centres them in its larger screen.
- **The resolution entry** writes `SUN.INI` `[Video]` and the options object
  at `0x007E4720` (hires.md).

## Checking it

`tools/present_drive.ps1` finds the presenter window, posts clicks to it in
game coordinates (letterboxed the way present.c fits the picture) and saves
the window's own picture with PrintWindow on its handle, nothing else on the
screen:

```
start build\ts.exe --run --mute --watchdog 60
powershell -File tools\present_drive.ps1 -Clicks "26:470,200" -Snap "45:work\present.png"
```

That run, clicking Firestorm on the choice screen, ends on Firestorm's main
menu in a 2338x1754 window (README's screenshot); the presenter logs the
click it handed the game (`[present] click at game 470,199`).

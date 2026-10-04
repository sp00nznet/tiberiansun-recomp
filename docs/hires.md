# High resolution

Tiberian Sun reads its resolution from `SUN.INI`:

```
[Video]
ScreenWidth=1920
ScreenHeight=1080
```

and runs at it: the menus stay 640x400, centred, and the battlefield and
sidebar take the whole screen. The game keeps the values in its options
object at `0x007E4720` (width `+0x1C`, height `+0x20`; `sub_00589C10` reads
them and `sub_0058A140` writes them back). The presenter's settings menu
(F10) writes both, as it does in redalert2-recomp; that path has not been
checked against this game in a window yet.

The playtest suite runs a skirmish at 1280x720, 1920x1080, 2560x1440 and
3840x2160 (`skirmish-720p` .. `skirmish-4k`).

## The sidebar's buttons

Each of the sidebar's two strips keeps its cameo buttons in a static array at
`0x0080B950`, 20 per strip (index = strip * 20 + row, 52 bytes each;
`sub_005F42A0` lays them out). How many rows it lays out is the strip's height
over the cameo height, and from about 1440 lines that is past 20: at 4K, 40
rows wrote over what follows the array, including the cameo shape pointers
at `0x0080C3A8..0x0080C3B0`, and the game faulted building the sidebar.

Every place that divides by the cameo height loads it the same way,
`mov r, [0x0080C3AC]; movsx r2, word [r + 4]; cdq; idiv r2`, 34 of them, so
`sidebar_rows_patches` in run_lift.py finds each by that shape and caps its
quotient at 20 right after the `idiv`: a row count, or the row under the
mouse, past 20 is a row there is no button for. Up to 1080 lines nothing
changes.

Left as it is: above 20 rows the strip's background is still drawn to the
bottom of the screen, below the scroll buttons.

## Speed

The game draws the battlefield in software into the locked primary, and its
frame rate follows the pixel count: about 20 frames a second recorded at
1080p, 10 at 1440p and 3 at 4K on the development machine. Making that
cheaper is on the ROADMAP.

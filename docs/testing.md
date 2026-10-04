# Testing

Two tools, both from redalert2-recomp:

- `tools/conformance.py`: a headless boot scored by milestones (entry point,
  DirectDraw, `Game Init Completed`, the picture moving, the choice screen's
  music) and the lift's health, against the committed `conformance.json`.
- `tools/playtest.py`: scripted cases, run headless in parallel, each in its
  own game folder (hard links, `SUN.INI` copied with the intro off), leaving
  `run.log`, `run.mp4` and a contact sheet in `work/tests/<case>/`.

```
py -3 tools/playtest.py --list
py -3 tools/playtest.py --jobs 3
py -3 tools/playtest.py skirmish-start --original   # the shipping code, same script
```

## How the cases drive the game

The choice screen and the main menus are drawn by the game, so cases click
them by position, in the 640x400 menus' pixels; above 640x400 the game
centres its menus and the script adds the offset (`Script(screen)`). The
screens behind the menus are Win32 dialogs from `Language.dll`, pressed by
caption through `tools/dialogs.py`'s map (`py -3 tools/dialogs.py --show
0xB4` lists the skirmish screen).

What the game logs is how a case knows where it is:

| | log line |
|---|---|
| the choice screen | `Theme::PlaySong(30)` |
| a main menu is up | `VQ audio handler closed OK` (the movie into it ends; there is no line of its own) |
| in game | `Tooltips are on.` |
| a movie starts | `Opening VQ audio handler` |
| an order | `Adding event PRODUCE`, `PLACE`, `DEPLOY`, `SCATTER` (move orders print none) |

## The cases

| case | checks |
|---|---|
| `boot` | the choice screen's music starts |
| `menu-firestorm`, `menu-tibsun` | each game's main menu |
| `menu-skirmish` | the skirmish setup dialog opens and Cancel leaves it |
| `skirmish-start` | a skirmish with the defaults: in game, the picture moving |
| `skirmish-720p` .. `skirmish-4k` | the same at each resolution (docs/hires.md) |
| `campaign-fs-gdi`, `campaign-fs-nod`, `campaign-ts-gdi`, `campaign-ts-nod` | each campaign to its first mission: the campaign dialog (`0x94`, its list), OK, Escape once the first movie opens |
| `campaign-build` | on the first Nod mission: a power plant and a Hand of Nod built from the sidebar and placed, then a light infantry; at least 3 `PRODUCE` and 2 `PLACE` events |

"The picture moving" counts distinct checksums the recorder prints every 100
recorded frames; at 4K the game draws about 3 frames a second, so those cases
run 180 seconds.

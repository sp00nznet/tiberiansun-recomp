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
| out of a game | `Tooltips are off.` |

## The cases

| case | checks |
|---|---|
| `boot` | the choice screen's music starts |
| `menu-firestorm`, `menu-tibsun` | each game's main menu |
| `menu-skirmish` | the skirmish setup dialog opens and Cancel leaves it |
| `menu-lan` | LAN's game list (`0xBB`) opens and Cancel leaves it |
| `menu-internet`, `menu-wdt` | Internet and World Domination Tour: Westwood Online is not installed, and the game says so (`0xD0`); OK leaves it |
| `menu-exit` | Exit: the game ends, exit code 0 |
| `menu-back` | Back: the choice screen and its music again |
| `options-game`, `-display`, `-sound`, `-keyboard`, `-network` | Options (`0xD5`) and each of its screens (`0xF5`, `0xD8`, `0xD6`, `0xA3`, `0xD7`), opened and left again |
| `skirmish-start` | a skirmish with the defaults: in game, the picture moving |
| `skirmish-720p` .. `skirmish-4k` | the same at each resolution (docs/hires.md) |
| `campaign-fs-gdi`, `campaign-fs-nod`, `campaign-ts-gdi`, `campaign-ts-nod` | each campaign to its first mission: the campaign dialog (`0x94`, its list), OK, Escape once the first movie opens |
| `load-mission` | on the first Nod mission: the in-game menu (`0xB5`), Save Game (`0x2B4`) into the first slot, Abort Mission (`0xB6`), then the main menu's Load Mission (`0xB7`) and the save, back in game |
| `skirmish-hd` | a skirmish with HD voxels on (`--hd-voxels`) |
| `skirmish-hd-debris` | as Nod (`--seed 1`), the cyborgs set on their own Attack Cycle (Ctrl+click); its tyre, a voxel animation, is drawn at 2x (`[hdvox] the first voxel animation at 2x`) |
| `campaign-build` | on the first Nod mission: a power plant and a Hand of Nod built from the sidebar and placed, then a light infantry; at least 3 `PRODUCE` and 2 `PLACE` events |
| `skirmish-build` | a skirmish with Unit Count at its least and `--seed 1`: the MCV selected, moved out of its group and deployed (D), then a power plant, a barracks and a light infantry; a `DEPLOY`, 3 `PRODUCE` and 2 `PLACE` events |
| `skirmish-forcefire` | the same start: the group band-selected (`--drag`), then Ctrl+click (force-fire) on open ground; the game logs no event for either, so the sheet is the check: every unit boxed, a crater where they fired |

"The picture moving" counts distinct checksums the recorder prints every 100
recorded frames; the hi-res cases run 180 seconds.

## TS against TS over the LAN

Two PCs on one subnet, each with the game and its own player name
(`SUN.INI` `[MultiPlayer] Handle`), each playing a script in the game's own
input:

```
build\ts.exe --run --mute --args tools\lan\host.args      # the PC that hosts
build\ts.exe --run --mute --args tools\lan\joiner.args    # the PC that joins
```

Both click Firestorm and its menu's Lan; the host goes New (the host screen,
`0xBC`) and starts with Go! once the joiner is in; the joiner picks the
host's game in the lobby's list (`0xBB`, item 1), Join and Accept (`0xBD`).
The dialogs are Red Alert 2's, same IDs. A script file's words are split on
whitespace, so its `--waitlog` texts are single words (`closed`, from "VQ
audio handler closed OK"; `Tooltips`, in game).

Checked so far: the host side, to its host screen. Two copies on one PC
cannot play each other: IPXEmu (the game folder's `wsock32.dll`) binds one
port, and the second copy's bind fails ("IPX socket bind failed"), so the
joining side needs a second machine.

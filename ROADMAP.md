# Roadmap

Parity with [redalert2-recomp](https://github.com/sp00nznet/redalert2-recomp),
in order; each is done when its check is in the suite or the docs.

1. **The presenter's settings** in a window: F10's resolution entry into a
   skirmish at that size, fullscreen, the scalings. (The window, its
   scaling, bars and click mapping are checked: docs/presenter.md.)
2. **Play, not just reach.** Done in a mission (`campaign-build`) and in a
   skirmish (`skirmish-build`: the MCV deployed, a power plant, a barracks
   and a light infantry). A skirmish starts the same every run with
   `--seed`; the MCV was boxed in by its own units, so the case moves it
   out first.
3. **The campaigns.** Done to the first mission, all four
   (`campaign-*`). Next: a mission played to its win.
4. **The rest of the menus.** Done: every main-menu entry (`menu-*`),
   Options and its five screens (`options-*`), saving and loading
   (`load-mission`). Internet and World Domination Tour end at the game's
   own "Westwood online support library is missing" message: as far as
   they go without servers. Serial / Modem is not in the suite (it probes
   the PC's real modems).
5. **HD voxels.** Done for units and their shadows (docs/voxels.md).
   Left: voxel projectiles (`0x004472C0`, BulletClass), voxel animations
   and debris, each with a test that puts one on screen.
6. **Speed at 4K**. Done: it was the recorder encoding 4K video, not the
   game (docs/hires.md).
7. **Setup.cmd end to end** from a clean folder. Done (a fresh clone with
   pcrecomp `main` beside it: catalog, lift, build, 8/8 conformance).
8. **LAN multiplayer**: TS against TS between two PCs. The scripts are in
   `tools/lan/` and the host side reaches its host screen (docs/testing.md);
   a run with a second PC is next.
9. **Toolkit**: the catalog misses code addresses that only appear as
   immediates (bringup.md 5); the native bridge leaves stale game frames
   where Windows' would be (bringup.md 4). Both belong in pcrecomp.
   `--original` needs pcrecomp #51 (the image loader cut `.text` at its
   VirtualSize, and this exe has a patch's code past it) to reach a skirmish.
10. **Release**: v0.1.0, private.

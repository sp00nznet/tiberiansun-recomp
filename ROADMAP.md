# Roadmap

Parity with [redalert2-recomp](https://github.com/sp00nznet/redalert2-recomp),
in order; each is done when its check is in the suite or the docs.

1. **The presenter's settings** in a window: F10's resolution entry into a
   skirmish at that size, fullscreen, the scalings. (The window, its
   scaling, bars and click mapping are checked: docs/presenter.md.)
2. **Play, not just reach.** Done on the first Nod mission
   (`campaign-build`: a power plant, a Hand of Nod, a light infantry). Left:
   the same in a skirmish (RA2's `skirmish-build`). Found so far: band selection and move orders by mouse
   work; the MCV is placed (a probe on its Unlimbo), but the start differs
   per run (the random seed is not fixed) and move orders print no event
   line. Band-selecting everything and pressing D deploys what can deploy
   (`Adding event DEPLOY`: the Juggernauts became artillery), but no MCV is
   among the starting units at first sight. It is there: the generator places
   it (`0x005DEC07`, at a start waypoint) and it is the long vehicle with a
   grey crane in the middle of the group, boxed in by infantry. D with
   everything selected orders it to deploy, but it has no room; scatter (X)
   and moving the group did not clear enough. Next: select the MCV alone
   (its cell from the generator, or SelectSameType), on open ground.
3. **The campaigns.** Done to the first mission, all four
   (`campaign-*`). Next: a mission played to its win.
4. **The rest of the menus.** Done: Options and its five screens
   (`options-*`), saving in a mission and loading it from the main menu
   (`load-mission`). Left: World Domination Tour as far as it goes without
   servers.
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

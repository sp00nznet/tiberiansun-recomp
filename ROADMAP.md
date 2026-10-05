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
4. **The rest of the menus**: Options, Load Mission with a save present,
   LAN, World Domination Tour as far as it goes without servers.
5. **HD voxels.** Done for units' bodies (docs/voxels.md). Left: their
   shadows (`0x00635E20`), checking the other voxel bodies (`0x004472C0`),
   voxel animations and debris.
6. **Speed at 4K**: the battlefield is drawn in software into the locked
   primary at about 3 frames a second at 3840x2160 (docs/hires.md).
7. **Setup.cmd end to end** from a clean folder.
8. **LAN multiplayer**: TS against TS between two PCs, scripted on both
   sides, as redalert2-recomp has.
9. **Toolkit**: the catalog misses code addresses that only appear as
   immediates (bringup.md 5); the native bridge leaves stale game frames
   where Windows' would be (bringup.md 4). Both belong in pcrecomp.
   `--original` needs pcrecomp #51 (the image loader cut `.text` at its
   VirtualSize, and this exe has a patch's code past it) to reach a skirmish.
10. **Release**: v0.1.0, private.

## Open

- Under load, now and then (1 of 14 in a full suite run three at a time),
  the display driver (`nvd3dum.dll`) faults inside a Lock the game makes on
  the movie's 640x400 system-memory surface (`DDLOCK_WAIT`, no rect) while a
  campaign's first movie plays. The crash report records the Lock in flight.
  The timer fix (bringup.md 8) cured the DirectSound crash at the same
  moment, not this one.

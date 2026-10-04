# Roadmap

Parity with [redalert2-recomp](https://github.com/sp00nznet/redalert2-recomp),
in order; each is done when its check is in the suite or the docs.

1. **The presenter's settings** in a window: F10's resolution entry into a
   skirmish at that size, fullscreen, the scalings. (The window, its
   scaling, bars and click mapping are checked: docs/presenter.md.)
2. **Play, not just reach**: deploy, build and train in a skirmish (RA2's
   `skirmish-build`). Found so far: band selection and move orders by mouse
   work; the MCV is placed (a probe on its Unlimbo), but the start differs
   per run (the random seed is not fixed) and move orders print no event
   line, so the case needs a fixed seed or a way to find the MCV, and a
   check other than the event log.
3. **The campaigns**: GDI and Nod for Tiberian Sun and for Firestorm, each
   to its first mission in game.
4. **The rest of the menus**: Options, Load Mission with a save present,
   LAN, World Domination Tour as far as it goes without servers.
5. **HD voxels**: redalert2-recomp's four half-pixel passes, found again in
   this renderer by shape (its docs/voxels.md).
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

# Roadmap

Parity with [redalert2-recomp](https://github.com/sp00nznet/redalert2-recomp),
in order; each is done when its check is in the suite or the docs.

1. **The presenter, checked in a window**: scaling, F10 settings, the
   game's resolution from the menu, the menus framed in a widescreen window.
2. **Play, not just reach**: deploy, build and train in a skirmish, checked
   in the game's event log (RA2's `skirmish-build`); orders by mouse.
3. **The campaigns**: GDI and Nod for Tiberian Sun and for Firestorm, each
   to its first mission in game.
4. **The rest of the menus**: Options, Load Mission with a save present,
   LAN, World Domination Tour as far as it goes without servers.
5. **HD voxels**: redalert2-recomp's four half-pixel passes, found again in
   this renderer by shape (its docs/voxels.md).
6. **Speed at 4K**: the battlefield is drawn in software into the locked
   primary at about 3 frames a second at 3840x2160 (docs/hires.md).
7. **Setup.cmd end to end** from a clean folder.
8. **recomp-netlab**: a recipe and a LAN scenario, as redalert2-recomp has.
9. **Toolkit**: the catalog misses code addresses that only appear as
   immediates (bringup.md 5); the native bridge leaves stale game frames
   where Windows' would be (bringup.md 4). Both belong in pcrecomp.
10. **Release**: v0.1.0, private.

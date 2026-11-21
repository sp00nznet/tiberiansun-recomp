# Contributors

Thank you to everyone who has contributed code, fixes, testing, or a hard-won
debugging insight. This file is the canonical record of who did what; the
CHANGELOG tells the story release by release, but credit lives here.

If you've contributed and aren't listed (or a line is wrong), open a PR against
this file. We want every name right.

---

## Maintainer

### Ned Heller ([@sp00nznet](https://github.com/sp00nznet))
Project creator and maintainer. The recompiled host, its presenter, HD voxels,
the playtest suite, and the pcrecomp toolkit it is built with.

---

## Contributors

### Chris Pressland ([@cpressland](https://github.com/cpressland))
**The macOS and Linux builds' design.** The cross build here (the clang-cl and
xwin toolchain file, `build.sh`, `play.sh`, `setup.sh`, and the playtests run
through Wine) is ported from the one he wrote for Red Alert 2
(sp00nznet/redalert2-recomp#1). It runs at all because of his native32 fix
for callbacks under Wine (sp00nznet/pcrecomp#55): Wine leaves DEP off, so the
game's window and dialog procedures quietly ran its original machine code, and
under Rosetta it reports an instruction fetch as a read.
- **The x87 ring and the mouse at the window's edges**, from Red Alert 2
  (redalert2-recomp#66, pcrecomp#195).

### [@twessman-9](https://github.com/twessman-9)
- **The Options resolution list and undoing `--mute`**, ported from their Red
  Alert 2 fix (redalert2-recomp#43).

### Vlad Artyomov ([@vlad-artyomov](https://github.com/vlad-artyomov))
- **Profiled the lifted code** in big Red Alert 2 battles and found the
  indirect-call lookups (redalert2-recomp#65); the cache is on here too.

### [@lynx8989](https://github.com/lynx8989)
- **The BSurface pixel fast path** (redalert2-recomp#67), ported here: the
  voxel plotter's pixel reads answered natively.

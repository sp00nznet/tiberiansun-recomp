# Changelog

All notable changes to this project. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow [SemVer](https://semver.org/).

## [Unreleased]

## [0.1.0-alpha.2] - 2026-10-21

### Added
- **The F10 settings menu on Linux**, the Windows presenter's, drawn over the
  picture by pcrecomp's win32hle: scaling, the bars, fullscreen, HD vehicles,
  the game's resolution and the mod. HD vehicles in a window are F10's to
  switch, on unless `ts.ini` says `hdvoxels=0`, as on Windows. Needs a
  pcrecomp with `hle_screen_desktop`; `setup.sh` updates an older clone.
- **More resolutions** in F10's list, on both hosts
  (`src/runtime/resolutions.h`): 21:9 (1720x720, 2560x1080, 3440x1440,
  3840x1600) and 16:10 (1680x1050, 2560x1600); on Linux the display's own
  size too. Each is a skirmish in the playtest suite.
- **The game's own Options screen** lists the large resolutions on Linux:
  the host adds `[Video] AllowHiResModes=yes` to `SUN.INI` when it is missing
  (a `no` stays). Without it the game lists only 640, 800 and 1024 wide.

### Fixed
- **A freeze in battle**: the line clipper (`0x006A8870`) could loop forever
  on a line through a corner of its rect, one rounding error outside one edge
  then the other. A clip now gives up after 64 rounds and the line goes
  undrawn for that frame (`run_lift.py` PATCHES). The shipping game has the
  same hang; found in Red Alert 2 by @andersbusk (redalert2-recomp #20).

## [0.1.0-alpha.1] - 2026-10-19

The first tagged build, for the installer.

### Added
- **The installer** (`TS-Recomp-Installer`, on the releases for Windows,
  Linux and macOS): finds the game in Steam, or you point it at the folder,
  fetches this repo and pcrecomp, runs the setup with a progress bar and
  leaves a Play button. It is
  [recomp-launcher](https://github.com/sp00nznet/recomp-launcher) with
  `launcher/launcher.json`, built by `.github/workflows/installer.yml`.
  `tools/setup.ps1` takes `-Yes` and `-Game`, and `setup.sh` takes `--yes` and
  `--game-dir` and numbers its steps (`[n/N]`) as `Setup.cmd` does. Both find
  pcrecomp in `../pcrecomp` as well as `../tools`; `setup.sh` stops at a
  Python venv it could not make, naming `python3-venv`; `Setup.cmd` installs
  the Visual Studio Build Tools if they are missing, as redalert2-recomp's does.
- **Mods**: a mod is a folder in `mods/`, laid over the game's folder without
  changing it (its files read first, what the game writes going into it, its
  maps and `expand*.mix` listed with the game's). `--mod NAME`; the settings
  menu (F10) on Windows and F9 in the menus on Linux switch mods by
  restarting the game; the choice is remembered (`src/runtime/mods.c`,
  `src/linux/mods.c`, `mods/README.md`).
- **Readable lifted C**: `run_lift.py` names functions from what the binary
  says (pcrecomp's `tools/lift/name_lift.py`: RTTI classes and vtable slots,
  constructors and destructors, debug messages) and gives each a header (its
  strings, Windows calls, `this` class, callers); 5,602 of 18,559 are named.
  An older pcrecomp leaves the address names.
- **Linux, natively** (no Wine): `build-linux/ts`, the same lifted C on
  pcrecomp's `runtime/win32hle` (pcrecomp #62) — its own kernel32, a window
  manager with the dialogs and controls the menus use, DirectDraw and
  DirectSound on SDL2, Winsock with IPXEmu's IPX over UDP, COM, and compound
  files for saves. `src/linux`: the host (`main.c`), Blowfish.dll's cipher
  for the MIX headers (`blowfish.c`, its tables read from the game's DLL),
  and the Windows host's scripted input (`script.c`); `src/runtime/hdvox.c`
  builds for it. `build-linux.sh` and `cmake/linux-i386.cmake` (gcc -m32,
  SDL2, SDL2_ttf); `setup.sh` builds it on Linux by default (`--wine` for the
  Wine build). The window scales as the presenter does (F12, F11, `ts.ini`).
  `tools/playtest.py` runs it when it is built.
- **Linux and macOS**, under Wine (CrossOver on a Mac): `ts.exe` cross-built
  with clang-cl and xwin (`cmake/clang-cl-x86.cmake`, `build.sh`), played with
  `play.sh`; `setup.sh` finds the tools, the game (CrossOver bottles, Steam
  libraries) and runs the pipeline. The playtests and conformance run the host
  through Wine (`TS_WINE`), load the game's IPXEmu `wsock32.dll`, and run the
  LAN cases one at a time. 29 of 29 under Wine 10.0 on Debian 13. Ported from
  [@cpressland](https://github.com/cpressland)'s macOS build for Red Alert 2
  (sp00nznet/redalert2-recomp#1).
- `run_lift.py` and the host refuse a `Game.exe` that is not the Steam build
  (PE timestamp `0x393C1B12`): another build lifts, but the patches land in
  the wrong code and it crashes in game.
- `CONTRIBUTORS.md`.
- HD voxel animations and debris (`0x0065E050`, VoxelAnimClass's Draw_It:
  its own finish, `0x00666500`, three more times, and its shadow and body
  blits watched), on with HD voxels; `skirmish-hd-debris` puts a tyre on
  screen. Voxel projectiles (`0x004472C0`) the same way, opt-in
  (`TS_HD_VOXEL_PROJECTILES=1`) until a test fires a missile silo.
- `skirmish-build`: in a skirmish, the MCV moved out of its group and
  deployed, then a power plant, a barracks and a light infantry. `--seed N`
  fixes the game's random seed (a patch after its GetTickCount at
  `0x004E3A61`), so the start is the same every run; `--select` sets a
  slider (Unit Count).
- The rest of the main menu: `menu-lan`, `menu-internet`, `menu-wdt`
  (Westwood Online missing, as the game reports it), `menu-exit`,
  `menu-back`.
- `skirmish-forcefire`: band selection and Ctrl+click force-fire, on the
  same seeded start.
- clang-cl (x86) builds, from pcrecomp `main`: the suite passes 21 of 21.
- CONTRIBUTING.md, and the README's *What the remaster adds*.
- `--record`'s checksum line prints the game's own frame count, its frame
  rate (docs/hires.md: about 31 a second in a skirmish, 1080p and 4K alike).
- HD vehicles: units and their shadows drawn at 2x from four half-pixel-offset renders,
  Red Alert 2's method found again in this renderer (docs/voxels.md); the
  voxel cache is off while they are on. `skirmish-hd` runs with them.
- `TS_PROFILE=1`: a sampling profile of the lifted functions; the frame dump
  (`--hd-voxels-dump`) writes each 2x frame's 1x frame beside it.
- All four campaigns to their first mission (Tiberian Sun and Firestorm,
  GDI and Nod), their VQA movies included, and `campaign-build`: a power
  plant and a Hand of Nod built and placed and a light infantry trained on
  the first Nod mission.
- The recompilation: `Game.exe` catalogued (18,552 functions) and lifted
  (18,559, 0 errors) on pcrecomp, with redalert2-recomp's host, presenter,
  scripted input and test tools.
- In game: the choice screen, both main menus, the skirmish setup and a
  skirmish, headless.
- High resolution to 4K: the sidebar's cameo rows capped at its 20 buttons
  per strip (`sidebar_rows_patches`, docs/hires.md); the presenter writes the
  game's options object at 0x007E4720.
- `tools/playtest.py`: 9 cases, menus by position and log line, dialogs by
  caption (`tools/dialogs.py`, from Language.dll); `tools/conformance.py`:
  8 boot milestones.
- Crash reports dump the guest stack; `TS_PROBE_ALL=1` makes `--probe`
  report every call.

### Fixed
- `setup.sh` no longer offers pcrecomp's unmerged win32hle branch: that work
  is on pcrecomp's `main` now, and an older clone is offered a `git pull`.
- Visual Studio 2026 (MSVC 19.50 and later) miscompiles the lifted C: its
  optimiser drops the sign test of a 16-bit value moved to the top of a
  register, which crashed Red Alert 2's skirmishes. The lifted C is built
  with its older optimiser there (`/d2SSAOptimizer-`).
- The menus had no cursor, on Windows and on Linux: they are Win32 dialogs
  that use the Windows cursor the game sets (the arrow, and the no-entry
  sign), and the presenter hid it everywhere. It now shows the game's cursor
  while the mouse is not captured (`WWMouseClass`'s +0x14 byte) and leaves the
  battle to the game's own, as Red Alert 2's presenter does.
- `setup.sh` on Linux stopped at "predates win32hle's DirectDraw" with a fresh
  pcrecomp: #62 is not merged yet, and it now offers the pull request's
  branch. Fedora's packages are named right (`sdl2-compat-devel`, `libgcc` and
  `libatomic` for i686), its 32-bit libraries are found, and the check counts
  only 32-bit ones.
- HD voxel shadows (and anything blitted straight onto the battlefield)
  were read 16 pixels too high: `0x0047CC10`'s point is in its window,
  the tactical view below the top bar (its fourth argument).
- 4K looked like 3 frames a second: `--record` opened its video at the
  game's mode and x264 could not keep up. Recordings are at most 1280 wide.
- Crashes now and then while a movie played (the display driver inside a
  Lock, or heap corruption): the game's VQA decoder writes up to 4.6 KB past
  the bottom of its surface, into slack on Windows and into the heap here.
  System-memory surfaces get their pixels from the host with a 64 KB tail
  (docs/bringup.md 9).
- Crashes under load as a movie closed (in DirectSound, or in the display
  driver): a timer callback ran after its timer was killed. The host owns
  the game's multimedia timers and waits out callbacks in flight
  (docs/bringup.md 8). DirectDraw is asked for `DDSCL_MULTITHREADED`.
- The crash report names the module a native fault is in, and the last
  surface Lock.
- A crash on about a quarter of runs after the first mouse move: a menu
  gadget's rect the game never initialised (docs/bringup.md 4).
- The skirmish screen's window procedure, never lifted (bringup.md 5), and
  its dialog template, written in a read-only DLL (bringup.md 6).
- The game's locks of the primary failing while the host copied it; the copy
  now converts outside the lock.
- Scripted runs saw the real cursor until their first move; it starts at the
  screen's centre.
- `run_lift.py` patches after an instruction that lifts to several lines
  (`idiv`) now go after the whole block.

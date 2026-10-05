# Changelog

All notable changes to this project. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow [SemVer](https://semver.org/).

## [Unreleased]

### Added
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

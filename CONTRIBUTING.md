# Contributing

Issues and pull requests are welcome.

## The one rule

**Nothing from the game goes in the repository.** No game files, no lifted C
(`src/recomp/gen/`), no catalog or RTTI dumps (`work/`), no disassembly
listings, nothing generated from `Game.exe`. The tool ships; its output is
made on each person's machine from their own copy. `.gitignore` covers the
usual paths; check `git status` before you commit anyway. Screenshots of the
game running are fine.

## Before a pull request

1. Build from a clean lift: `py -3 run_lift.py --all` (expected: `errors 0`),
   then `build.cmd`.
2. Run the conformance harness and the suite:
   ```
   py -3 tools\conformance.py
   py -3 tools\playtest.py --jobs 3
   ```
   Both must pass. A new behaviour comes with a case that shows it.
3. Where the lift and the shipping code disagree, say which:
   `py -3 tools\playtest.py <case> --original` runs the same script on the
   original machine code.

## Where a fix belongs

- A bug in the lifter, the catalog or the native bridge belongs in
  [pcrecomp](https://github.com/sp00nznet/pcrecomp), not here.
- A fix to the game itself is a patch in `run_lift.py`, found by the shape of
  the code where it can be, with a comment that says what it fixes and why.
- A wall you got past goes in `docs/bringup.md`: what it looked like, what it
  was, what moved it.

# Bring-up log

Each wall between `Game.exe` and a playing game, and what moved it. The host
started as redalert2-recomp's (its docs/bringup.md has the walls this engine
shares); this file is what Tiberian Sun added.

## 1. Static constructors the catalog did not have

The first runs faulted before `WinMain`. Of the 3,597 entries in the CRT's
`_initterm` tables (0x006EE000..0x006F1850), three were not in the function
catalog, so their objects were never built. They are `RUN_SEEDS` in
`run_lift.py`.

## 2. The scripts never started

Red Alert 2's scripted input starts its clock when the main menu's dialog
opens. Tiberian Sun's main menu is drawn by the game: two dialog resources in
the exe, against 98. The clock starts on the game's own log line `Game Init
Completed` instead (src/runtime/input.c), and the cases click the menus by
position and wait on log lines.

## 3. The primary, locked from two sides

Tiberian Sun draws by locking the primary surface itself; Red Alert 2 blits
into it. The host copies the primary out from other threads (presenter,
recorder), and two locks at once fail: in a 4K skirmish, 2,806 of the game's
Locks came back `DDERR_SURFACEBUSY`. The game's lock of the primary now holds
the host's primary lock from Lock to Unlock (host.c, `hl_Lock`), and the
host's copy only memcpys inside it and converts outside, so a 4K frame does
not keep the game waiting.

## 4. A rect nobody wrote (the crash on the first mouse move)

A quarter of runs faulted in the span copier `sub_006A8D00`, reading from an
address like `0xFC3A75E8`, soon after the first scripted mouse move on the
choice screen. It looked like a race: any change to the host, even code that
never ran, could make it vanish or come back, and it happened on `--original`
too (2 of 10 runs).

The crash report learned to dump the guest stack, and the blit's rectangle
was `x = 0x7610D720, y = -1, 640x400`: `0x7610D720` is `Sleep`'s address and
`-1` is what the menu loop (`sub_00571220`) keeps in `ebp` and `ebx`. A probe
on the menu's dirty-rect list showed the same entry. `sub_00570C80` draws a
menu's gadgets and adds each one's rect, a local at `esp+0xC`, to the dirty
list, including a gadget that drew nothing and never wrote it. On Windows that
slot held whatever the last native call's frames left there. Under the native
bridge, native calls run on the host's stack, so the guest stack keeps the
game's own stale frames, here the menu loop's saved registers, and the flush
blitted 640x400 pixels from x = 1,980,815,136.

The fix zeroes the rect at the function's start (`PATCHES` in run_lift.py):
an empty rect is what such a gadget meant. 0 faults in 20 runs, against 5 in
20 before. (The real cursor leaked into scripted runs until the first move,
too; it is parked at the screen's centre on the mode change now. That was
not this crash, but it made runs depend on where the player's mouse was.)

## 5. A window procedure only ever named as a number

The skirmish button faulted executing `0x00596CB0`: `CallWindowProcA` handed
control to a subclassed window procedure that was never lifted. The only
reference to it is an immediate (`0x0059153E: mov ebp, 0x596cb0`) handed to
`SetWindowLong`, and it sits right after a switch table the catalog took for
data. A scan of every immediate in the lifted code that lands on code outside
the catalog found it and nothing else; it is a `RUN_SEEDS` entry.

## 6. Dialog templates in a read-only DLL

The screens behind the main menu are Win32 dialogs, 74 of them, in
`Language.dll` rather than the exe. The host's dialog shim clears
`WS_VISIBLE` in the template for the call, and Red Alert 2's templates were
in its writable image; these are in the DLL's read-only pages, and the write
faulted. The shim unprotects that one dword around the edit.

## 7. The sidebar above 1080 lines

See [hires.md](hires.md): 4K faulted laying out the sidebar.

## 8. A timer callback after its timer was killed

Under load (other runs on the machine), the campaign cases crashed now and
then while their first movie closed: in the display driver
(`nvd3dum.dll`) inside one of the game's surface Locks, or in DirectSound,
called from `sub_004072D0`, the movie's audio timer callback reading its
sound buffer's position. Escape closes the movie: the game calls
`timeKillEvent`, deletes the critical section and frees the buffer.

On Windows a callback that is already running finishes on its own thread
while the game's thread goes on. Here a guest callback runs on winmm's
thread only once it gets the machine, and the game's thread gives the
machine up at its next native call, which is `timeKillEvent` itself. A
callback that was already due ran after the kill, against freed objects, and
the busier the machine, the wider that window.

The host now owns the game's multimedia timers (`shim_timeSetEvent`,
`shim_timeKillEvent`): each counts its callbacks in flight, and
`timeKillEvent` returns once they are done, giving up the machine while it
waits. The campaign cases three at a time went from a crash in one or two of
five each round to 15 of 15. (DirectDraw is also asked for
`DDSCL_MULTITHREADED` now, since the presenter and the recorder lock the
primary from their own threads; that alone did not stop the crashes.)

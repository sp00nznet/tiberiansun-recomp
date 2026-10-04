# Recon

The build: the Steam release of *Command & Conquer: Tiberian Sun* with
*Firestorm* (part of *The Ultimate Collection*), `Game.exe`.

| | Tiberian Sun `Game.exe` | Yuri's Revenge `gamemd.exe` (redalert2-recomp) |
|---|---|---|
| Built | 2000-06-05 | 2001-10-31 |
| Image | 0x00400000, entry 0x006B7E21 | 0x00400000, entry 0x007CD80F |
| `.text` | 0x00401000, 2,918,925 bytes | 0x00401000, 4,063,232 bytes |
| Protection | none (`SUN.EXE` is the launcher, with an encrypted `.bind` loader) | none |
| RTTI | 807 classes, 895 vtables, 5,226 virtual methods | 954 classes, 6,665 virtual methods |
| Classes in common | 744 | 744 |
| Dialog resources | **2** | 98 |
| Movies | VQA (its own player, DirectSound) | Bink |
| Default resolution | 640x400 (`SUN.INI` `[Video]`) | 800x600 menus |

```
$ py -3 ..\tools\tools\cpp\rtti.py game\Game.exe -o work\rtti.json --seeds work\rtti_seeds.json
[*] complete object locators : 895
[*] vtables          : 895
[*] classes          : 807
[*] virtual methods  : 5,226  (5,225 attributable to one class)
```

Imports: ADVAPI32, COMCTL32, DDRAW, DSOUND, GDI32, KERNEL32, OLEAUT32,
SHELL32, USER32, VERSION, WINMM, WSOCK32 (IPXEmu's, in the game folder, for
LAN play), ole32. The folder also carries DDrawCompat as `ddraw.dll`, and
`BLOWFISH.DLL`, a COM server, as Red Alert 2's does.

## What carries over from Red Alert 2, and what does not

The engine is the same lineage, so the host, the presenter, the scripted
input and the test tools come from redalert2-recomp. What does not carry
over is anything tied to an address, and the menus:

- **The debug printf** is at `0x004082D0`: a bare `ret` with 1,354 callers
  passing format strings ("Opening VQ audio handler", "Audio.Lock_Mutex").
  It is the lift's one hook, and `--debuglog` prints the game's own log.
- **The menus are the game's own**, drawn with its gadgets: two dialog
  resources in the exe, against Red Alert 2's 98 Win32 dialogs. The playtest
  cases click by position and wait on log lines, not on dialog IDs.
- **Patches** (Red Alert 2's sidebar rows, HD voxels) are found again by the
  same shapes before they are applied here.

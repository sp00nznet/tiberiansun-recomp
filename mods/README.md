# Mods

Put a mod in a folder of its own here: `mods/<name>/`, with its files as they
would go in the game's folder (rules.ini, firestrm.ini, art.ini, expand*.mix,
maps...). Tiberian Sun and Firestorm are one program, so one folder serves both.

The game's own folder is never changed: the game reads a mod's file where the
mod has one and its own where it does not, and what it writes while a mod is
on (settings, saves) goes into the mod's folder.

To play one:

- **Linux**: `--mod <name>` on the command line, or **F9 in the menus**, which
  restarts the game with the next mod (and after the last, with none). The
  window's title names the mod; the choice is remembered.
- **Windows**: the settings menu (**F10**) has a **Mod** list; choosing one
  restarts the game with it. `--mod <name>` works too.

`--mod none` plays the game as it shipped.

A mod made for a DLL that patches `Game.exe`'s machine code (started by
Syringe or a launcher) does not work here: the recompiled game is not that
machine code. A mod that ships its own `Game.exe` is played with this one's,
and a MIX file that its own exe loads by another name needs renaming to an
`expand*.mix` name the game looks for. Rules, art, maps, sounds and MIX files
are what a mod can bring.

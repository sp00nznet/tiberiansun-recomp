/* The presenter: the game's picture in our own Direct3D 11 window (present.c). */
#pragma once

/* sharp, smooth, crt, nearest, integer -> 0..4, or -1. */
int  present_mode_from_name(const char* name);

/* Open the window on a thread of its own and keep it showing the game.
 * mode and fullscreen of -1 take what ts.ini (beside ts.exe) remembers. */
void present_start(int mode, int fullscreen);

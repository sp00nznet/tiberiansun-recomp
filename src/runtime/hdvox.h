/* HD voxels: vehicles drawn at twice the resolution (hdvox.c). */
#pragma once
#include <stdint.h>

/* on: draw voxel units at 2x; dump_dir (or NULL): also write the first 40
 * renders as BMPs, 1x and 2x. */
void hdvox_configure(int on, const char* dump_dir);
/* ts_vox_hd_on: the machinery, and the game's voxel caches off (the lift's
 * patches read it); ts_vox_hd_show: the player's switch (F10). The
 * presenter keeps the first on for the whole run, so the caches never fill:
 * in Red Alert 2, caches filled while HD was off and then bypassed again
 * drew every turret facing the camera (redalert2-recomp#8). */
extern int ts_vox_hd_on, ts_vox_hd_show;

/* The presenter's frame at 2x: frame16 the 1x frame (16-bit, pitch in bytes),
 * out 2w x 2h BGRX, with the units' 2x pixels where they still show. */
void hdvox_compose(const uint8_t* frame16, int pitch, int w, int h, uint32_t* out);

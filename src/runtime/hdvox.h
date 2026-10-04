/* HD voxels: vehicles drawn at twice the resolution (hdvox.c). */
#pragma once
#include <stdint.h>

/* on: draw voxel units at 2x; dump_dir (or NULL): also write the first 40
 * renders as BMPs, 1x and 2x. */
void hdvox_configure(int on, const char* dump_dir);
extern int ts_vox_hd_on;

/* The presenter's frame at 2x: frame16 the 1x frame (16-bit, pitch in bytes),
 * out 2w x 2h BGRX, with the units' 2x pixels where they still show. */
void hdvox_compose(const uint8_t* frame16, int pitch, int w, int h, uint32_t* out);

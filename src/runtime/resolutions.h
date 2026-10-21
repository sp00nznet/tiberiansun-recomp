/* The F10 menu's game resolutions, for both hosts (present.c, src/linux/menu.c).
 *
 * The game's own resolution: SUN.INI [Video] for the next start, and the
 * options in memory (OptionsClass at 0x7E4720, width +0x1C and height +0x20:
 * sub_00589C10 reads them from [Video] and sub_0058A140 writes them back) so
 * the next game opens at it and the game's own save on exit does not put the
 * old one back. The memory is only written while it holds a plausible
 * resolution. */
#pragma once

#define TS_OPT_W_VA (0x007E4720u + 0x1C)
#define TS_OPT_H_VA (TS_OPT_W_VA + 4)

/* 21:9 too (1720x720, 2560x1080, 3440x1440, 3840x1600): an ultrawide screen at
 * its own size, not a 16:9 game with bars. 1720x720 is half of 3440x1440, so it
 * scales by a whole 2x there. Nothing wider than the presenter's 4096x2160
 * frame, so no 5120x2160 or 32:9. 16:10 as well (1680x1050, 2560x1600). */
static const int k_res[][2] = { { 800, 600 }, { 1024, 768 }, { 1280, 720 }, { 1366, 768 }, { 1600, 900 },
                                { 1680, 1050 }, { 1720, 720 }, { 1920, 1080 }, { 2560, 1080 }, { 2560, 1440 },
                                { 2560, 1600 }, { 3440, 1440 }, { 3840, 1600 }, { 3840, 2160 } };
#define NRES ((int)(sizeof k_res / sizeof k_res[0]))

static inline int res_plausible(int w, int h) { return w >= 320 && w <= 8192 && h >= 200 && h <= 8192; }

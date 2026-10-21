/*
 * menu.c - F10, the settings menu: the Windows presenter's (src/runtime/
 * present.c, settings_menu), drawn by win32hle over the picture
 * (hle_screen_menu). Scaling, the bars, fullscreen and HD vehicles take effect
 * at once and are remembered in ts.ini; the game's resolution is for the next
 * game; a mod restarts the program with it.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <SDL2/SDL.h>
#include "win32hle.h"
#include "recomp_types.h"
#include "resolutions.h"
#include "hdvox.h"
#include "mods.h"

#define MAX_MENU_MODS 64
enum { ID_SCALE = 100, ID_BARS = 200, ID_FULL = 300, ID_RES = 400, ID_DISPLAY = 499, ID_HDVOX = 500, ID_MOD = 600 };

static const char *const k_scale[] = { "sharp", "smooth", "crt", "nearest", "integer" };
static const char *const k_scale_label[] = { "Sharp (default)", "Smooth", "CRT", "Nearest", "Integer" };
static hle_menu_item_t g_main[8], g_scale[5], g_bars[2], g_res[NRES + 1], g_mods[MAX_MENU_MODS + 1];
static char g_res_label[NRES + 1][40], g_mod_names[MAX_MENU_MODS][128];
static int g_nmods, g_display_w, g_display_h;
static void (*g_save)(void);
static int (*g_next_key)(int sdl_scancode, int down);   /* mods.c's F9 */

/* key=val in [sec] of an INI file, in place: its line replaced (with keep,
 * left as it is), else added at the section's end, else the section added at
 * the end. */
static void ini_set(const char *path, const char *sec, const char *key, const char *val, int keep) {
    FILE *f = fopen(path, "rb");
    char *in = NULL;
    size_t n = 0;
    if (f) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        rewind(f);
        in = (char *)malloc(size > 0 ? (size_t)size + 1 : 1);
        n = in ? fread(in, 1, size > 0 ? (size_t)size : 0, f) : 0;
        if (in) in[n] = 0;
        fclose(f);
    }
    FILE *o = fopen(path, "wb");
    if (!o) { free(in); fprintf(stderr, "[menu] cannot write %s\n", path); return; }
    size_t kl = strlen(key), sl = strlen(sec);
    int in_sec = 0, done = 0;
    for (char *p = in; p && *p;) {
        char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p + 1) : strlen(p);
        if (*p == '[') {
            if (in_sec && !done) fprintf(o, "%s=%s\r\n", key, val), done = 1;
            in_sec = !strncasecmp(p + 1, sec, sl) && p[1 + sl] == ']';
        }
        if (in_sec && !done && !strncasecmp(p, key, kl) && p[kl] == '=') {
            if (keep) fwrite(p, 1, len, o); else fprintf(o, "%s=%s\r\n", key, val);
            done = 1;
        } else fwrite(p, 1, len, o);
        p += len;
    }
    if (!done) {
        if (n && in[n - 1] != '\n') fputs("\r\n", o);
        if (!in_sec) fprintf(o, "[%s]\r\n", sec);
        fprintf(o, "%s=%s\r\n", key, val);
    }
    fclose(o);
    free(in);
}

static void set_game_resolution(int w, int h) {
    char path[PATH_MAX], v[16];
    hle_host_path_for_write("SUN.INI", path, sizeof path, 1);   /* the game runs in its folder; a mod's, over it */
    snprintf(v, sizeof v, "%d", w), ini_set(path, "Video", "ScreenWidth", v, 0);
    snprintf(v, sizeof v, "%d", h), ini_set(path, "Video", "ScreenHeight", v, 0);
    if (res_plausible((int)MEM32(TS_OPT_W_VA), (int)MEM32(TS_OPT_H_VA)))
        MEM32(TS_OPT_W_VA) = (uint32_t)w, MEM32(TS_OPT_H_VA) = (uint32_t)h;
    fprintf(stderr, "[menu] game resolution %dx%d (the next game opens at it)\n", w, h);
}

static void switch_mod(const char *name) {
    char ask[256];
    if (!strcasecmp(name, mods_active())) return;
    if (!mods_can_switch()) {
        fprintf(stderr, "[menu] mods switch in the menus, not in a battle\n");
        return;
    }
    snprintf(ask, sizeof ask, "Restart with %s?", name[0] ? name : "no mod");
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No" },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes" },
    };
    const SDL_MessageBoxData box = { SDL_MESSAGEBOX_INFORMATION, NULL, "Tiberian Sun (recomp)", ask, 2, buttons, NULL };
    int yes = 0;
    if (SDL_ShowMessageBox(&box, &yes) == 0 && yes) mods_restart_with(name);
}

static void chosen(int id) {
    if (id >= ID_SCALE && id < ID_SCALE + 5) hle_screen_scale(k_scale[id - ID_SCALE]);
    else if (id == ID_BARS || id == ID_BARS + 1) hle_screen_bars(id - ID_BARS);
    else if (id == ID_FULL) { int full; hle_screen_settings(NULL, &full); hle_screen_fullscreen(!full); return; }   /* saves, as F11 */
    else if (id == ID_HDVOX) ts_vox_hd_show = !ts_vox_hd_show;   /* takes effect next frame */
    else if (id >= ID_RES && id < ID_RES + NRES) { set_game_resolution(k_res[id - ID_RES][0], k_res[id - ID_RES][1]); return; }
    else if (id == ID_DISPLAY) { set_game_resolution(g_display_w, g_display_h); return; }
    else if (id >= ID_MOD && id <= ID_MOD + g_nmods) { switch_mod(id == ID_MOD ? "" : g_mod_names[id - ID_MOD - 1]); return; }
    if (g_save) g_save();
}

static void open_menu(void) {
    int bars, full, n = 0, nm = 0;
    const char *scale = hle_screen_settings(&bars, &full);
    for (int i = 0; i < 5; i++) g_scale[i] = (hle_menu_item_t){ k_scale_label[i], NULL, ID_SCALE + i, !strcmp(scale, k_scale[i]), NULL, 0 };
    g_bars[0] = (hle_menu_item_t){ "Blurred", NULL, ID_BARS + 1, bars, NULL, 0 };
    g_bars[1] = (hle_menu_item_t){ "Black", NULL, ID_BARS, !bars, NULL, 0 };
    int w = (int)MEM32(TS_OPT_W_VA), h = (int)MEM32(TS_OPT_H_VA), nr = 0, dw = 0, dh = 0;
    /* this display's own size first, when it isn't one of the list's; no
     * larger than 4K, the largest tested */
    if (hle_screen_desktop(&dw, &dh) && dw >= 800 && dh >= 600 && dw <= 3840 && dh <= 2160) {
        int listed = 0;
        for (int i = 0; i < NRES; i++) listed |= dw == k_res[i][0] && dh == k_res[i][1];
        if (!listed) {
            g_display_w = dw, g_display_h = dh;
            snprintf(g_res_label[NRES], sizeof g_res_label[NRES], "%d x %d (this display)", dw, dh);
            g_res[nr++] = (hle_menu_item_t){ g_res_label[NRES], NULL, ID_DISPLAY, w == dw && h == dh, NULL, 0 };
        }
    }
    for (int i = 0; i < NRES; i++) {
        snprintf(g_res_label[i], sizeof g_res_label[i], "%d x %d%s", k_res[i][0], k_res[i][1],
                 dw == k_res[i][0] && dh == k_res[i][1] ? " (this display)" : "");
        g_res[nr++] = (hle_menu_item_t){ g_res_label[i], NULL, ID_RES + i, w == k_res[i][0] && h == k_res[i][1], NULL, 0 };
    }
    g_nmods = mods_list(g_mod_names, MAX_MENU_MODS);
    g_mods[nm++] = (hle_menu_item_t){ "None (the game as it shipped)", NULL, ID_MOD, !mods_active()[0], NULL, 0 };
    for (int i = 0; i < g_nmods; i++)
        g_mods[nm++] = (hle_menu_item_t){ g_mod_names[i], NULL, ID_MOD + 1 + i, !strcasecmp(mods_active(), g_mod_names[i]), NULL, 0 };
    if (!g_nmods) g_mods[nm++] = (hle_menu_item_t){ "(put mods in the mods folder: mods/README.md)", NULL, 0, 0, NULL, 0 };
    g_main[n++] = (hle_menu_item_t){ "Scaling", "F12", 0, 0, g_scale, 5 };
    g_main[n++] = (hle_menu_item_t){ "Bars beside the picture", NULL, 0, 0, g_bars, 2 };
    g_main[n++] = (hle_menu_item_t){ "Fullscreen", "F11", ID_FULL, full, NULL, 0 };
    g_main[n++] = (hle_menu_item_t){ "HD vehicles (voxels at 2x)", NULL, ID_HDVOX, ts_vox_hd_show, NULL, 0 };
    g_main[n++] = (hle_menu_item_t){ NULL, NULL, 0, 0, NULL, 0 };
    g_main[n++] = (hle_menu_item_t){ "Game resolution (next game)", NULL, 0, 0, g_res, nr };
    g_main[n++] = (hle_menu_item_t){ "Mod (restarts the game)", NULL, 0, 0, g_mods, nm };
    hle_screen_menu(g_main, n, chosen);
}

static int on_key(int scancode, int down) {
    if (scancode != SDL_SCANCODE_F10) return g_next_key ? g_next_key(scancode, down) : 0;
    if (down) open_menu();
    return 1;
}

void menu_start(void (*save)(void)) {
    /* The game's own Options screen lists only 640, 800 and 1024 wide unless
     * [Video] AllowHiResModes is on; then every mode DirectDraw offers, which
     * win32hle caps at 4K (as redalert2-recomp's issue #23). A player's own
     * setting stays. Windows is left alone: its DirectDraw is the display's,
     * and can offer more than the presenter shows. */
    char path[PATH_MAX];
    hle_host_path_for_write("SUN.INI", path, sizeof path, 1);
    ini_set(path, "Video", "AllowHiResModes", "yes", 1);
    fprintf(stderr, "[menu] F10 for the settings; %s has AllowHiResModes\n", path);
    g_save = save;
    g_next_key = hle_screen_key_hook;
    hle_screen_key_hook = on_key;
}

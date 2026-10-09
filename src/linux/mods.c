/*
 * mods.c - play a mod from a folder, and switch mods from the main menu.
 *
 * A mod is a folder of the game's own kind of files: rules (rules.ini,
 * firestrm.ini), art, AI, MIX files (expand*.mix), maps. It goes in
 * mods/<name>/, next to this repository's launchers (Tiberian Sun and
 * Firestorm are one program, so one folder serves both). The game folder is never changed: the mod's
 * folder is laid over it (win32hle's overlay), so the game reads a mod's file
 * where the mod has one and its own where it does not, and finds the mod's
 * maps and expand*.mix beside its own. What the game writes while a mod is
 * on (settings, saves) goes into the mod's folder.
 *
 * The mod to play is --mod NAME, else the last one chosen (ts.ini, [mods]
 * active). F9 in the menus switches to the next one, then back to the game
 * without a mod: the program restarts with it, since a game reads its rules
 * once, at start. In a battle F9 does nothing (the battle would be lost).
 *
 * A mod made for Ares-style DLLs that patch Game.exe's machine code does not
 * work on a recompiled game: what it needs from the DLL is missing.
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <SDL_scancode.h>
#include "win32hle.h"
#include "mods.h"

#define MODS_TITLE "Tiberian Sun"
#define MAX_MODS 64

static char g_root[PATH_MAX];              /* mods, beside the build directory */
static char g_active[128];                 /* "" for the game as it shipped */
static int g_argc;
static char **g_argv;
static void (*g_save)(void);
static int (*g_can_switch)(void);
static char g_start_dir[PATH_MAX];         /* where the program started: a restart starts there too */

/* Before main, which moves into the game's folder: relative arguments
 * (--game game, --dump-frames DIR) are relative to here. */
__attribute__((constructor)) static void remember_start_dir(void) {
    if (!getcwd(g_start_dir, sizeof g_start_dir)) g_start_dir[0] = 0;
}

const char *mods_active(void) { return g_active; }

/* The mods there are now, sorted: each a folder in mods/<game>. */
static int list_mods(char names[][128], int max) {
    DIR *d = opendir(g_root);
    int n = 0;
    struct dirent *e;
    while (d && (e = readdir(d)) && n < max) {
        char path[PATH_MAX];
        struct stat st;
        snprintf(path, sizeof path, "%s/%s", g_root, e->d_name);
        if (e->d_name[0] != '.' && stat(path, &st) == 0 && S_ISDIR(st.st_mode))
            snprintf(names[n++], 128, "%s", e->d_name);
    }
    if (d) closedir(d);
    qsort(names, (size_t)n, 128, (int (*)(const void *, const void *))strcasecmp);
    return n;
}

static void show_title(void) {
    char title[256];
    if (g_active[0]) snprintf(title, sizeof title, "%s - %s (F9 in the menus: next mod)", MODS_TITLE, g_active);
    else snprintf(title, sizeof title, "%s", MODS_TITLE);
    hle_screen_title(title);
}

/* Restart with --mod name: the same command line without its own --mod. */
static void restart_with(const char *name) {
    char *argv[256];
    int n = 0;
    for (int i = 0; i < g_argc && n < 252; i++) {
        if (!strcmp(g_argv[i], "--mod") && i + 1 < g_argc) { i++; continue; }
        argv[n++] = g_argv[i];
    }
    argv[n++] = "--mod";
    argv[n++] = (char *)(name[0] ? name : "none");
    argv[n] = NULL;
    fprintf(stderr, "[mods] restarting with %s\n", name[0] ? name : "the game as it shipped");
    fflush(NULL);
    if (g_start_dir[0] && chdir(g_start_dir) != 0) fprintf(stderr, "[mods] cannot go back to %s\n", g_start_dir);
    execv("/proc/self/exe", argv);
    fprintf(stderr, "[mods] could not restart: %s\n", strerror(errno));
}

/* F9: the next mod (after the last, none), in the menus only. */
static int on_key(int scancode, int down) {
    if (scancode != SDL_SCANCODE_F9) return 0;
    if (!down) return 1;
    if (!g_can_switch()) {
        fprintf(stderr, "[mods] F9 switches mods in the menus, not in a battle\n");
        return 1;
    }
    static char names[MAX_MODS][128];
    int n = list_mods(names, MAX_MODS), at = -1;
    for (int i = 0; i < n; i++)
        if (!strcasecmp(names[i], g_active)) at = i;
    snprintf(g_active, sizeof g_active, "%s", at + 1 < n ? names[at + 1] : "");
    if (g_save) g_save();                     /* remembered for the next start */
    restart_with(g_active);
    return 1;
}

void mods_start(const char *game_dir, const char *requested, const char *remembered,
                int argc, char **argv, void (*save)(void), int (*can_switch)(void)) {
    g_argc = argc, g_argv = argv, g_save = save, g_can_switch = can_switch;
    char exe[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof exe - 1);
    exe[len > 0 ? len : 0] = 0;
    char *slash = strrchr(exe, '/');
    if (slash) *slash = 0;                     /* the build directory; mods/ is beside it */
    snprintf(g_root, sizeof g_root, "%s/../mods", exe);
    hle_screen_key_hook = on_key;

    const char *want = requested ? requested : remembered ? remembered : "";
    if (!strcasecmp(want, "none")) want = "";
    snprintf(g_active, sizeof g_active, "%s", want);
    if (g_active[0]) {
        char dir[PATH_MAX];
        struct stat st;
        snprintf(dir, sizeof dir, "%s/%s", g_root, g_active);
        if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
            fprintf(stderr, "[mods] no mod \"%s\" in %s: playing the game as it shipped\n", g_active, g_root);
            g_active[0] = 0;
        } else {
            hle_set_overlay(game_dir, dir);
            fprintf(stderr, "[mods] playing %s (%s over the game's folder)\n", g_active, dir);
        }
    }
    show_title();
}

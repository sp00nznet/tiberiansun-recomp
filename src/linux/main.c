/*
 * Tiberian Sun and Firestorm - the native Linux host.
 *
 * The same lifted C as the Windows build (src/recomp/gen), on pcrecomp's
 * runtime/win32hle instead of runtime/native32: every Win32 import the game
 * makes is answered by win32hle's own implementation (files, windows and
 * dialogs, DirectDraw and DirectSound on SDL2), so no Windows and no Wine.
 * What is here is what is specific to this game: where the image goes, the
 * command line, the hooks the lift calls, and the fault report.
 *
 * Linked non-PIE at the default 0x08048000, so the guest's
 * 0x00400000..0x00B7A000 is free for the image.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <sys/stat.h>

#include "win32hle.h"
#include "pe_loader.h"
#include "host.h"

extern const uint32_t ts_entry_va;    /* recomp_dispatch.c */

#define TS_IMAGE_BASE 0x00400000u
#define TS_EXE_STAMP  0x393C1B12u     /* Game.exe's PE timestamp: the build lifted (run_lift.py) */

static int g_debuglog, g_headless, g_fullscreen;
static unsigned g_watchdog_s;

/* ---- what the lift calls into ---------------------------------------------
 * run_lift.py PATCHES and HOOKS. */
uint32_t ts_seed;                     /* --seed N: the game's random seed */

/* The game's debug printf, compiled out of the retail build: --debuglog
 * prints it (arguments straight off the guest stack: a cdecl va_list on x86
 * is a pointer to the first variadic slot, and the guest is 32-bit). */
int  script_arg(int argc, char **argv, int i);   /* script.c */
int  script_active(void);
void script_log_line(const char *line);
void script_start(void);

void ts_hook_004082D0(void) {
    if (g_debuglog || script_active()) {
        char buf[1024];
        const char *fmt = (const char *)(uintptr_t)MEM32(g_esp + 4);
        /* the guest's varargs are 4-byte slots, as i386's are: va_list is a
         * pointer to the first */
        va_list ap;
        char *first = (char *)(uintptr_t)(g_esp + 8);
        memcpy(&ap, &first, sizeof ap);
        vsnprintf(buf, sizeof buf, fmt, ap);
        size_t n = strlen(buf);
        script_log_line(buf);
        if (g_debuglog) fprintf(stderr, "[game] %s%s", buf, n && buf[n - 1] == '\n' ? "" : "\n");
    }
    g_esp += 4;                        /* ret */
}

/* HD voxels (src/runtime/hdvox.c) are not on this host yet: the patches the
 * lift carries for them find them off and do nothing. */
int ts_vox_hd_on;
int16_t ts_vox_dx, ts_vox_dy;
int ts_vox_hd_begin(uint32_t rect) { (void)rect; return 0; }
int ts_vox_hd_begin_at(uint32_t save, uint32_t len) { (void)save; (void)len; return 0; }
void ts_vox_hd_pass(int k) { (void)k; }
uint32_t ts_vox_hd_end(void) { return 0; }
void ts_vox_hd_blit(uint32_t d, uint32_t c, uint32_t e) { (void)d; (void)c; (void)e; }
void ts_vox_hd_blitted(void) {}
void ts_vox_unit_copy(uint32_t d, uint32_t e) { (void)d; (void)e; }
void ts_vox_unit_copied(void) {}
void ts_vox_shadow_blit(uint32_t d, uint32_t e) { (void)d; (void)e; }
void ts_vox_shadow_blitted(void) {}
int ts_vox_bullet_begin(uint32_t rect) { (void)rect; return 0; }
void ts_vox_bullet_blit(uint32_t d, uint32_t c, uint32_t e) { (void)d; (void)c; (void)e; }
void ts_vox_anim_blit(uint32_t d, uint32_t c, uint32_t e) { (void)d; (void)c; (void)e; }
void ts_vox_frame_blit(uint32_t d, uint32_t a) { (void)d; (void)a; }

/* ---- game-specific answers ------------------------------------------------ */

/* Message boxes print; headless, a question is answered No. */
static void ts_MessageBoxA(void) {
    uint32_t type = A32(3), buttons = type & 0xF;
    uint32_t answer = g_headless && (buttons == 4 || buttons == 3) ? 7u : buttons == 4 || buttons == 3 ? 6u : 1u;
    fprintf(stderr, "[messagebox] %s: %s -> %u\n", ASTR(2) ? ASTR(2) : "", ASTR(1) ? ASTR(1) : "", answer);
    RET(answer, 4);
}

static const win32hle_shim g_ts_shims[] = {
    { "MessageBoxA", ts_MessageBoxA },
    { 0, 0 }
};

/* WinMain checks that Blowfish.dll, the Westwood cipher's COM server, can be
 * created, and the MIX archives' headers are decrypted through it: its object
 * is served by blowfish.c. */
static const uint8_t CLSID_Blowfish[16] = { 0x10, 0xAD, 0x40, 0x14, 0xA8, 0x6A, 0xD1, 0x11,
                                            0xB6, 0xF9, 0x00, 0xA0, 0x24, 0xDD, 0xAF, 0xD1 };
uint32_t ts_blowfish_create(const uint8_t *iid, uint32_t *out);

/* ---- faults and the watchdog ---------------------------------------------- */
static void report(const char *why) {
    fprintf(stderr, "\n=== %s ===\n  in lifted sub_%08X, last import %s\n", why, g_cur_func,
            g_cur_import ? g_cur_import : "(none)");
    fprintf(stderr, "  eax=%08X ecx=%08X edx=%08X ebx=%08X esp=%08X ebp=%08X esi=%08X edi=%08X\n",
            g_eax, g_ecx, g_edx, g_ebx, g_esp, g_ebp, g_esi, g_edi);
    fprintf(stderr, "last indirect calls (newest first):\n");
    for (int i = 1; i <= 40 && i <= (int)g_icall_trace_idx; i++) {
        uint32_t k = (g_icall_trace_idx - i) & (ICALL_TRACE_SIZE - 1);
        const char *nm = hle_name(g_icall_trace[k]);
        fprintf(stderr, "  0x%08X  from 0x%08X  %s\n", g_icall_trace[k], g_icall_from[k], nm ? nm : "");
    }
    fprintf(stderr, "guest stack at %08X:", g_esp);
    for (int k = 0; k < 64; k++) fprintf(stderr, "%s%08X", k % 8 ? " " : "\n  ", MEM32(g_esp + 4 * k));
    fprintf(stderr, "\n");
}

static void on_fault(int sig, siginfo_t *si, void *uc) {
    static volatile int once;
    (void)uc;
    if (__sync_lock_test_and_set(&once, 1)) _exit(3);
    char why[96];
    snprintf(why, sizeof why, "fault: signal %d, address %p", sig, si->si_addr);
    report(why);
    fflush(stderr);
    _exit(3);
}

static void *watchdog(void *arg) {
    (void)arg;
    sleep(g_watchdog_s);
    fprintf(stderr, "\n[watchdog] %u s: in sub_%08X, last import %s, %u indirect calls\n",
            g_watchdog_s, g_cur_func, g_cur_import ? g_cur_import : "(none)", g_icall_count);
    fflush(stderr);
    hle_exit(4);
    return NULL;
}

/* Every 100th frame shown, a checksum: a run's frames can be told apart
 * (the playtests count distinct ones) with no picture taken. */
static long g_shown;
static const char *g_dump_dir;        /* --dump-frames DIR: those frames as BMPs too */
static void dump_bmp(const uint8_t *px, int w, int h, int pitch, int bpp, long n) {
    char path[1024];
    snprintf(path, sizeof path, "%s/frame_%05ld.bmp", g_dump_dir, n);
    FILE *f = fopen(path, "wb");
    if (!f || bpp != 16) { if (f) fclose(f); return; }
    uint32_t row = (uint32_t)(w * 3 + 3) & ~3u, size = 54 + row * (uint32_t)h;
    uint8_t hdr[54] = { 'B', 'M' };
    memcpy(hdr + 2, &size, 4);
    hdr[10] = 54, hdr[14] = 40;
    memcpy(hdr + 18, &w, 4);
    memcpy(hdr + 22, &h, 4);
    hdr[26] = 1, hdr[28] = 24;
    fwrite(hdr, 1, 54, f);
    uint8_t *line = (uint8_t *)calloc(1, row);
    for (int y = h - 1; y >= 0; y--) {
        const uint16_t *s = (const uint16_t *)(px + (size_t)y * pitch);
        for (int x = 0; x < w; x++) {
            uint16_t p = s[x];
            line[3 * x] = (uint8_t)((p & 31) << 3), line[3 * x + 1] = (uint8_t)((p >> 5 & 63) << 2), line[3 * x + 2] = (uint8_t)((p >> 11) << 3);
        }
        fwrite(line, 1, row, f);
    }
    free(line);
    fclose(f);
}
static uint32_t checksum(const uint8_t *px, int w, int h, int pitch, int bpp) {
    uint32_t sum = 0;
    for (int y = 0; y < h; y += 4)
        for (int x = 0; x < w * bpp / 8; x += 16) sum = sum * 31 + px[(size_t)y * pitch + x];
    return sum;
}

/* --record out.mp4: the primary, 30 frames a second, piped to ffmpeg as
 * BGRX, as the Windows host records it: at most 1280 wide, the size it
 * started with, a later mode letterboxed into it; every 100th frame's
 * checksum logged (the playtests count distinct ones). --frames N stops
 * after N frames and closes the file properly. */
static const char *g_record;
static long g_record_frames, g_recorded;
static FILE *g_ffmpeg;
static pthread_mutex_t g_rec_lock = PTHREAD_MUTEX_INITIALIZER;

static void record_close(void) {
    pthread_mutex_lock(&g_rec_lock);
    if (g_ffmpeg) {
        pclose(g_ffmpeg);
        g_ffmpeg = NULL;
        fprintf(stderr, "[record] %ld frames -> %s\n", g_recorded, g_record);
    }
    pthread_mutex_unlock(&g_rec_lock);
}
static void on_exit_close(int code) { (void)code; record_close(); }

static void *recorder(void *unused) {
    static uint32_t frame[4096 * 2160], row[4096];
    static uint16_t raw[4096 * 2160];
    int rec_w = 0, rec_h = 0;
    uint32_t next = hle_ticks_ms();
    (void)unused;
    for (;;) {
        next += 33;
        int32_t wait = (int32_t)(next - hle_ticks_ms());
        if (wait > 0) usleep((useconds_t)wait * 1000u);
        const uint8_t *px;
        const uint32_t *pal;
        int w, h, pitch, bpp, ok;
        mach_enter();                                    /* the primary can go away under a mode change */
        ok = hle_dd_frame(&px, &w, &h, &pitch, &bpp, &pal, NULL) && bpp == 16 && w <= 4096 && h <= 2160;
        for (int y = 0; ok && y < h; y++) memcpy(raw + (size_t)y * w, px + (size_t)y * pitch, (size_t)w * 2);
        mach_leave();
        if (!ok) continue;
        uint32_t sum = checksum((const uint8_t *)raw, w, h, w * 2, 16);
        for (size_t i = 0, n = (size_t)w * h; i < n; i++) {
            uint16_t p = raw[i];
            uint32_t r = (p >> 11) & 31, g = (p >> 5) & 63, b = p & 31;
            frame[i] = (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2);
        }
        pthread_mutex_lock(&g_rec_lock);
        if (!g_ffmpeg && g_recorded) { pthread_mutex_unlock(&g_rec_lock); return NULL; }   /* closed */
        if (!g_ffmpeg) {
            char cmd[PATH_MAX + 256];
            int k = (w + 1279) / 1280;
            rec_w = (w / k) & ~1, rec_h = (h / k) & ~1;
            snprintf(cmd, sizeof cmd, "ffmpeg -y -loglevel error -f rawvideo -pix_fmt bgr0 -s %dx%d -r 30 -i - "
                     "-c:v libx264 -pix_fmt yuv420p '%s'", rec_w, rec_h, g_record);
            g_ffmpeg = popen(cmd, "w");
            fprintf(stderr, "[record] %dx%d -> %s\n", rec_w, rec_h, g_record);
            if (!g_ffmpeg) { pthread_mutex_unlock(&g_rec_lock); return NULL; }
        }
        if (g_recorded % 100 == 0)
            fprintf(stderr, "[record] frame %ld checksum %08X game frame %u\n", g_recorded, sum, MEM32(0x007E4924u));
        int fw = rec_w, fh = (int)((long long)rec_w * h / w);
        if (fh > rec_h) fh = rec_h, fw = (int)((long long)rec_h * w / h);
        int ox = (rec_w - fw) / 2, oy = (rec_h - fh) / 2;
        for (int y = 0; y < rec_h; y++) {
            memset(row, 0, (size_t)rec_w * 4);
            if (y >= oy && y < oy + fh) {
                const uint32_t *src = frame + (size_t)((y - oy) * h / fh) * w;
                for (int x = 0; x < fw; x++) row[ox + x] = src[x * w / fw];
            }
            fwrite(row, 4, (size_t)rec_w, g_ffmpeg);
        }
        if (ferror(g_ffmpeg)) {                          /* no ffmpeg, or it stopped: the run goes on unrecorded */
            fprintf(stderr, "[record] ffmpeg is not taking frames: not recording\n");
            pclose(g_ffmpeg);
            g_ffmpeg = NULL;
            pthread_mutex_unlock(&g_rec_lock);
            return NULL;
        }
        pthread_mutex_unlock(&g_rec_lock);
        if (++g_recorded == g_record_frames) {
            record_close();
            hle_exit(0);
        }
    }
}

/* Without a recording, the same checksum lines at the same rate, from the
 * pump; and --dump-frames writes those frames as BMPs. */
static void ts_pump(void) {
    static uint32_t next;
    const uint8_t *px;
    const uint32_t *pal;
    int w, h, pitch, bpp;
    hle_screen_pump();
    uint32_t now = hle_ticks_ms();
    if ((int32_t)(now - next) < 0 || !hle_dd_frame(&px, &w, &h, &pitch, &bpp, &pal, NULL)) return;
    next = now + 3333;
    if (!g_record)
        fprintf(stderr, "[record] frame %ld checksum %08X game frame %u\n", g_shown * 100, checksum(px, w, h, pitch, bpp),
                MEM32(0x007E4924u));
    if (g_dump_dir) dump_bmp(px, w, h, pitch, bpp, g_shown);
    g_shown++;
}

static uint32_t exe_stamp(const char *path) {
    uint8_t head[4096] = { 0 };
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(head, 1, sizeof head, f);
    fclose(f);
    uint32_t nt = n >= 0x40 ? *(const uint32_t *)(head + 0x3C) : 0;
    return nt && nt < sizeof head - 12 ? *(const uint32_t *)(head + nt + 8) : 0;
}

int main(int argc, char **argv) {
    const char *game = "game";
    int run = 0;
    setvbuf(stderr, NULL, _IONBF, 0);
    for (int i = 1; i < argc; i++) {
        int n = script_arg(argc, argv, i);
        if (n) { i += n - 1; continue; }
        if (!strcmp(argv[i], "--run")) run = 1;
        else if (!strcmp(argv[i], "--headless")) g_headless = 1;
        else if (!strcmp(argv[i], "--fullscreen")) g_fullscreen = 1;
        else if (!strcmp(argv[i], "--debuglog")) g_debuglog = 1;
        else if (!strcmp(argv[i], "--mute")) hle_dsound_set_master(0.0f);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) ts_seed = (uint32_t)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--game") && i + 1 < argc) game = argv[++i];
        else if (!strcmp(argv[i], "--watchdog") && i + 1 < argc) g_watchdog_s = (unsigned)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--trace")) win32hle_trace = 1;
        else if (!strcmp(argv[i], "--record") && i + 1 < argc) {
            static char path[PATH_MAX];                  /* absolute: the run chdirs into the game folder */
            const char *p = argv[++i];
            if (p[0] == '/') snprintf(path, sizeof path, "%s", p);
            else { char cwd[PATH_MAX]; snprintf(path, sizeof path, "%s/%s", getcwd(cwd, sizeof cwd) ? cwd : ".", p); }
            g_record = path;
        }
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) g_record_frames = strtol(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--exe") && i + 1 < argc) i++;   /* the Windows host's: here it is the game folder's Game.exe */
        else if (!strcmp(argv[i], "--dump-frames") && i + 1 < argc) {
            static char dir[PATH_MAX];
            if (!realpath(argv[++i], dir)) { fprintf(stderr, "no folder %s\n", argv[i]); return 1; }
            g_dump_dir = dir;                /* the run chdirs into the game folder */
        }
        else {
            printf("usage: ts [--run] [--headless | --fullscreen] [--game DIR] [--debuglog] [--mute] [--seed N] [--watchdog S] [--trace] [--dump-frames DIR] [--record out.mp4] [--frames N]\n"
                   "           [--press DLG:CTRL@s] [--select DLG:CTRL=N@s] [--waitlog TEXT@s] [--move|--click x,y@s]\n"
                   "           [--key [c][s][a]+vk@s] [--wait VA@s] [--drag x1,y1,x2,y2@s]\n");
            return !strcmp(argv[i], "--help") || !strcmp(argv[i], "-h") ? 0 : 1;
        }
    }
    char game_full[PATH_MAX], exe_host[PATH_MAX + 16];
    if (!realpath(game, game_full)) { fprintf(stderr, "no game folder at %s\n", game); return 1; }
    hle_set_drive('C', game_full);
    if (!hle_host_path("C:\\Game.exe", exe_host, sizeof exe_host)) {
        fprintf(stderr, "no Game.exe in %s\n", game_full);
        return 1;
    }
    uint32_t stamp = exe_stamp(exe_host);
    if (stamp != TS_EXE_STAMP) {
        fprintf(stderr, "%s is not the build this host was lifted from (PE timestamp 0x%08X, wanted 0x%08X). "
                "Lift and build from the Steam release's Game.exe (README, Getting Started).\n", exe_host, stamp, TS_EXE_STAMP);
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);           /* a recorder whose ffmpeg went away gets an error, not a signal */
    {   /* faults report on their own stack: a guest stack overflow is one */
        static uint8_t alt[1 << 16];
        stack_t ss = { .ss_sp = alt, .ss_size = sizeof alt };
        sigaltstack(&ss, NULL);
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_sigaction = on_fault;
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sigaction(SIGSEGV, &sa, NULL);
        sigaction(SIGBUS, &sa, NULL);
        sigaction(SIGFPE, &sa, NULL);
        sigaction(SIGILL, &sa, NULL);
    }

    win32hle_register(g_ts_shims);    /* first: the game's own answers win */
    recomp_host_init();
    hle_com_register_class(CLSID_Blowfish, ts_blowfish_create);
    printf("Tiberian Sun recomp host (native Linux)\n  lifted functions in dispatch: %u\n", recomp_dispatch_count);

    pe_image img;
    if (recomp_pe_map(exe_host, &img) != 0 || img.base != TS_IMAGE_BASE) {
        fprintf(stderr, "cannot map %s at 0x%08X\n", exe_host, TS_IMAGE_BASE);
        return 1;
    }
    hle_module_add("C:\\Game.exe", img.base, img.resource_rva, img.resource_size, 1);
    int unresolved = recomp_pe_bind(&img, hle_resolve_or_stub);
    printf("  mapped %s: 0x%08X-0x%08X, %d imports not implemented yet\n", exe_host, img.base, img.base + img.span, unresolved);
    hle_set_command_line("\"C:\\Game.exe\"");

    if (!run) {
        printf("\n(dry run: image mapped and bound; --run enters 0x%08X)\n", ts_entry_va);
        return 0;
    }
    if (chdir(game_full) != 0) { fprintf(stderr, "cannot enter %s\n", game_full); return 1; }
    if (hle_screen_open("Tiberian Sun", g_fullscreen, g_headless) != 0) return 1;
    hle_set_pump_hook(ts_pump);
    hle_on_exit = on_exit_close;
    if (g_record) {
        pthread_t t;
        pthread_create(&t, NULL, recorder, NULL);
        pthread_detach(t);
    }
    script_start();
    if (g_watchdog_s) {
        pthread_t t;
        pthread_create(&t, NULL, watchdog, NULL);
        pthread_detach(t);
    }
    printf("  entering 0x%08X\n\n", ts_entry_va);
    fflush(stdout);
    uint32_t rc = hle_call_guest(ts_entry_va, 0, NULL);
    printf("\nentry returned eax=%08X\n", rc);
    hle_exit((int)rc);
    return 0;
}

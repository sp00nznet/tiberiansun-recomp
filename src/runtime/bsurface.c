/* BSurface pixel read, native fast path (run_lift.py PATCHES, 0x006A7420).
 * From @lynx8989's patch for redalert2-recomp (its issue #67).
 *
 * Get_Pixel(POINT*) (Surface slot 10, 0x006A7420) is what the voxel plotter
 * calls twice per sample. Lifted, it is a chain of virtual calls: Lock (which
 * calls Bytes_Per_Pixel and Get_Pitch, which calls Get_Width),
 * Bytes_Per_Pixel again, Unlock. For a plain BSurface those are field reads:
 * width [this+4], the lock count [this+0xC] (up and down again), bpp
 * [this+0x10], buffer [this+0x14]. Only when the vtable still holds
 * BSurface's own methods (slots 22, 23, 27, 28, 30; one lower than Red Alert
 * 2's) does this answer; otherwise, or with a null buffer, it returns 0 and
 * the lifted code runs unchanged. */
#include <stdint.h>

/* ponytail: guest addresses are host addresses on both hosts (Windows x86,
 * i386 Linux with RECOMP_FLAT_MEMORY); a 64-bit host would need hle_ptr. */
#define G32(va) (*(volatile uint32_t *)(uintptr_t)(va))
#define G16(va) (*(volatile uint16_t *)(uintptr_t)(va))
#define G8(va)  (*(volatile uint8_t *)(uintptr_t)(va))

#define BSURF_LOCK   0x00406E50u   /* slot 22, +0x58 */
#define BSURF_UNLOCK 0x00406DD0u   /* slot 23, +0x5C */
#define BSURF_BPP    0x00406E90u   /* slot 27, +0x6C */
#define BSURF_PITCH  0x00406EA0u   /* slot 28, +0x70 */
#define BSURF_WIDTH  0x00406DA0u   /* slot 30, +0x78 */

int ts_bsurf_getpixel(uint32_t self, uint32_t pt, uint32_t *out) {
    uint32_t vt = G32(self), bpp, base, addr;
    if (G32(vt + 0x58) != BSURF_LOCK || G32(vt + 0x5C) != BSURF_UNLOCK ||
        G32(vt + 0x6C) != BSURF_BPP || G32(vt + 0x70) != BSURF_PITCH ||
        G32(vt + 0x78) != BSURF_WIDTH)
        return 0;
    base = G32(self + 0x14);
    if (!base) return 0;
    bpp = G32(self + 0x10);
    addr = base + G32(pt) * bpp + G32(pt + 4) * (G32(self + 4) * bpp);
    *out = bpp == 2 ? (uint32_t)G16(addr) : (uint32_t)G8(addr);
    return 1;
}

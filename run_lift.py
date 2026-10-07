#!/usr/bin/env python3
"""Tiberian Sun (with Firestorm) lift driver: work/functions.json -> src/recomp/gen/.

Drives pcrecomp's shared `tools/lift/generate.py` + `lift32`, the same shape
as The Movies' and Force Commander's run_lift.py (docs/architecture.md):

* **Closure-limited lifting** (`generate.closure`). Lift the call-graph closure
  from the OEP and give every other catalogued function a stub that aborts
  naming itself, so a run says exactly what to lift next. Bring-up uses --all
  (Game.exe is ~0.7M instructions, which compiles in minutes); the closure is
  there for bisecting a bad lift.
* **Extents by walking the branches** (`generate.true_extent`), capped by
  reach rather than the catalog's clamp; its docstring has why.

Every vtable slot RTTI names is injected as an entry (work/rtti_seeds.json):
MSVC's vtordisp adjustor thunks are reachable only through a vtable, so no
disassembler pass names them, and an unresolved slot answers eax = 0.

    py -3 run_lift.py                      # OEP closure, 3000 functions
    py -3 run_lift.py --max 20000
    py -3 run_lift.py --roots 0x007CD80F,0x004A1C30
    py -3 run_lift.py --virtual --max 60000   # plus every vtable method
    py -3 run_lift.py --all
"""
import argparse
import json
import os
import sys
import time

_HERE = os.path.dirname(os.path.abspath(__file__))
# PCRECOMP picks another toolkit checkout, the same knob CMakeLists.txt has:
# the lifter and the runtime header must come from the same tree.
_TOOLS = os.path.join(os.environ.get('PCRECOMP', os.path.join(_HERE, '..', 'tools')), 'tools')
sys.path.insert(0, os.path.join(_TOOLS, 'lift'))
sys.path.insert(0, os.path.join(_TOOLS, 'pe'))

from capstone import Cs, CS_ARCH_X86, CS_MODE_32          # noqa: E402
from generate import (EXTENT_REACH, closure, find_splits, true_extent,  # noqa: E402
                      linear_disassemble_function, lift_function_linear, write_chunk)
from lift32 import Lifter                                  # noqa: E402
from pe_analyze import analyze_pe, build_iat_map           # noqa: E402

EXE = os.path.join(_HERE, 'game', 'Game.exe')
EXE_STAMP = 0x393C1B12   # Game.exe's PE timestamp, the Steam release (docs/RECON.md)
CATALOG = os.path.join(_HERE, 'work', 'functions.json')
SEEDS = os.path.join(_HERE, 'work', 'rtti_seeds.json')
OUT = os.path.join(_HERE, 'src', 'recomp', 'gen')
STATS = os.path.join(_HERE, 'work', 'lift_stats.json')

# Entries the catalog does not find, so a clean checkout lifts them too
# (docs/bringup.md 1 and 11). The CRT's static constructors are reachable only
# through the _initterm table; two window procedures are named only by a
# `mov reg, imm` behind a jump table. An unresolved ICALL answers eax = 0 and
# carries on, so a missing one shows up far from its cause.
RUN_SEEDS = [
    # static constructors: of the 3,597 entries in the CRT's _initterm tables
    # (0x006EE000..0x006F1850), the three the catalog does not have
    0x0044E750, 0x004937D0, 0x004FBAE0,
    # the skirmish screen's subclassed window procedure: only ever an
    # immediate (0x0059153E: mov ebp, 0x596cb0) handed to SetWindowLong, after
    # a switch table the catalog took for data
    0x00596CB0,
]

# Functions whose body is the host's instead of the lift: the host defines
# ts_hook_XXXXXXXX(void) and it runs like an import shim (arguments from
# g_esp, pops its own return address). src/runtime/host.c.
HOOKS = {
    # The debug printf: compiled out of the retail build (a bare `ret` with
    # 1,354 callers passing format strings), so the host gives it a body and
    # --debuglog prints the game's own log.
    0x004082D0,
}

# ---- remaster patches: C emitted after one instruction ---------------------
# The lift stays faithful; a patch is a line of C added after an instruction
# the game repo names, for a fix the original game cannot have. The --original
# oracle runs the shipping code unpatched, so it remains the reference.



# Patches are found again for this exe, by the same shapes as Red Alert 2's
# (redalert2-recomp: the sidebar's rows, HD voxels). None yet.
# Fixes to the game itself: C run after the instruction at the address.
PATCHES = {
    # 0x00570C80 draws a menu's gadgets and adds each one's rect, at esp+0xC,
    # to the dirty list -- including a gadget that drew nothing and never wrote
    # it. On Windows that slot held whatever the last native call's frames left;
    # here native calls run on the host's stack, so it held the menu loop's
    # saved ebp and ebx (Sleep's address and -1), and the flush blitted from
    # x = 0x7610D720 and faulted (docs/bringup.md). An empty rect instead.
    0x00570C91: ('MEM32(esp + 0xC) = MEM32(esp + 0x10) = MEM32(esp + 0x14) = MEM32(esp + 0x18) = 0; '
                 '/* fix: run_lift.py PATCHES */'),
    # The game's random seed is GetTickCount() (stored at 0x007E4934, logged
    # as "Seed is %08x"), so a skirmish starts differently every run. The
    # host's --seed N fixes it, for tests that click on a unit.
    0x004E3A61: '{ extern uint32_t ts_seed; if (ts_seed) eax = ts_seed; } /* host --seed */',
}


# HD voxels (docs/voxels.md): Red Alert 2's, found again in this renderer.
# A unit's body is rendered by 0x00635B00 into the 256x256 voxel buffer
# (0x00822740): its sections, then the finish stage (0x00666720) that turns
# their records into pixels, each span through a rasterizer from the table at
# 0x00713878; the image goes straight onto the battlefield through 0x0047CC10.
# The finish stage runs three more times with every span half a pixel further
# left, up, or both; the four images interleave into one at 2x. Gated at run
# time by the host (src/runtime/hdvox.c): off, nothing here does anything.
TAG = '/* remaster: HD voxels, run_lift.py HD_VOXEL_PATCHES */'


def _hd_passes(arg, begin='ts_vox_hd_begin'):
    """C for the three extra runs of 0x00666720, ecx = esp + arg each time
    (it fills the 6-dword rect at ecx and returns it in eax)."""
    return ('{ extern int %s(uint32_t); extern void ts_vox_hd_pass(int); '
            'extern uint32_t ts_vox_hd_end(void); '
            'if (%s(eax)) { for (int _k = 1; _k < 4; _k++) { ts_vox_hd_pass(_k); '
            'ecx = esp + 0x%X; RECOMP_CALL(sub_00666720); } eax = ts_vox_hd_end(); } } ' % (begin, begin, arg) + TAG)


def _hd_blit():
    """C just before 0x0047CC10: ecx the battlefield, edx the converter, then
    on the stack the source surface, its rect and the destination point."""
    return '{ extern void ts_vox_hd_blit(uint32_t, uint32_t, uint32_t); ts_vox_hd_blit(ecx, edx, esp); } ' + TAG


def _hd_blitted():
    return '{ extern void ts_vox_hd_blitted(void); ts_vox_hd_blitted(); } ' + TAG



def _hd_passes_666500():
    """C for the three extra runs of 0x00666500, the finish voxel animations
    and debris use: ecx = esp+0x44 (its rect), edx = esp+0x20, one stack
    argument esp+0x1C (ret 4). Its outputs, esp+0x1C to esp+0x5C, are put
    back afterwards."""
    return ('{ extern int ts_vox_hd_begin_at(uint32_t, uint32_t); extern void ts_vox_hd_pass(int); '
            'extern uint32_t ts_vox_hd_end(void); uint32_t _eax = eax; '
            'if (ts_vox_hd_begin_at(esp + 0x1C, 0x40)) { for (int _k = 1; _k < 4; _k++) { ts_vox_hd_pass(_k); '
            'ecx = esp + 0x44; edx = esp + 0x20; esp -= 4; MEM32(esp) = esp + 4 + 0x1C; '
            'RECOMP_CALL(sub_00666500); } ts_vox_hd_end(); } eax = _eax; } ' + TAG)


def _hd_copy():
    return '{ extern void ts_vox_unit_copy(uint32_t, uint32_t); ts_vox_unit_copy(ecx, esp); } ' + TAG


def _hd_copied():
    return '{ extern void ts_vox_unit_copied(void); ts_vox_unit_copied(); } ' + TAG


HD_VOXEL_PATCHES = {
    # 0x00668730: the span record handed to the rasterizer is at esp+0x20
    # (eax, just pushed); its starts, x at +0x18 and y at +0x1A, are 8.8.
    0x006689E2: ('{ extern int16_t ts_vox_dx, ts_vox_dy; '
                 'MEM16(eax + 0x18) += ts_vox_dx; MEM16(eax + 0x1A) += ts_vox_dy; } ' + TAG),
    # 0x006354E0 draws a unit's body from its voxel cache: a key of -1 (its
    # third argument, [esp+0x60] once the prologue is done) renders it
    # straight onto the battlefield instead. With HD voxels on, always: every
    # unit is rendered (and at 2x) each frame, as Red Alert 2's patch does.
    0x006354EC: '{ extern int ts_vox_hd_on; if (ts_vox_hd_on) MEM32(esp + 0x60) = 0xFFFFFFFFu; } ' + TAG,
    # A unit's body (0x00635B00): the extra passes just after its finish
    # stage, then its blit onto the battlefield watched.
    0x00635BF9: _hd_passes(0x4C),
    0x00635DE9: _hd_blit(),
    0x00635DEB: _hd_blitted(),
    # 0x00651F50 (a unit's Draw_It, after its parts are in the 160x160
    # staging surface [0x0080FA54]) copies staging onto the battlefield with
    # the house palette, lighting and the Z-buffer through 0x00423530: ecx
    # the battlefield, on the stack the destination rect, the staging
    # surface, its rect. Around each of its three calls the host looks at
    # the battlefield, to place the unit's 2x image where its 1x one went.
    0x00652219: _hd_copy(),
    0x0065221B: _hd_copied(),
    0x006522A8: _hd_copy(),
    0x006522AA: _hd_copied(),
    0x006522FC: _hd_copy(),
    0x00652303: _hd_copied(),
    # Shadows. 0x00635860 draws a unit's shadow from its own cache (key in
    # ebx, [esp+0x58]); -1 renders and blits it every time. 0x00635E20
    # renders it: sections, then the same finish stage (ecx = esp+0x28),
    # whose shadow records are plotted by 0x00668A00 from an 8.8 start at
    # [esp+0x30] (x) and [esp+0x32] (y) once both are stored; then it is
    # blitted onto the battlefield by 0x0047CC10, the shadow converter in edx.
    0x00635898: '{ extern int ts_vox_hd_on; if (ts_vox_hd_on) { ebx = 0xFFFFFFFFu; MEM32(esp + 0x58) = ebx; } } ' + TAG,
    0x00668A94: ('{ extern int16_t ts_vox_dx, ts_vox_dy; '
                 'MEM16(esp + 0x30) += ts_vox_dx; MEM16(esp + 0x32) += ts_vox_dy; } ' + TAG),
    0x00635ECF: _hd_passes(0x28),
    0x00635F6E: '{ extern void ts_vox_shadow_blit(uint32_t, uint32_t); ts_vox_shadow_blit(ecx, esp); } ' + TAG,
    0x00635F6F: '{ extern void ts_vox_shadow_blitted(void); ts_vox_shadow_blitted(); } ' + TAG,
    # Voxel projectiles: BulletClass's Draw_It (0x00445C00) draws one through
    # 0x004472C0, the same finish (ecx = esp+0x34) and a blit by 0x0047CC10
    # straight onto the battlefield. Opt-in (TS_HD_VOXEL_PROJECTILES=1).
    0x00447387: _hd_passes(0x34, 'ts_vox_bullet_begin'),
    0x0044741C: '{ extern void ts_vox_bullet_blit(uint32_t, uint32_t, uint32_t); ts_vox_bullet_blit(ecx, edx, esp); } ' + TAG,
    0x00447422: _hd_blitted(),
    # Voxel animations and debris (0x0065E050, their Draw_It): a shadow, then
    # the body, each finished by 0x00666500 and blitted onto the battlefield.
    0x0065E1C5: _hd_passes_666500(),
    0x0065E236: '{ extern void ts_vox_shadow_blit(uint32_t, uint32_t); ts_vox_shadow_blit(ecx, esp); } ' + TAG,
    0x0065E237: '{ extern void ts_vox_shadow_blitted(void); ts_vox_shadow_blitted(); } ' + TAG,
    0x0065E2AC: _hd_passes_666500(),
    0x0065E39D: '{ extern void ts_vox_anim_blit(uint32_t, uint32_t, uint32_t); ts_vox_anim_blit(ecx, edx, esp); } ' + TAG,
    0x0065E39F: _hd_blitted(),
    # 0x0048B590, DSurface's copy from another surface: into the primary,
    # it ends a frame, and the host publishes the 2x layer built during it.
    0x0048B590: '{ extern void ts_vox_frame_blit(uint32_t, uint32_t); ts_vox_frame_blit(ecx, esp + 4); } ' + TAG,
}

SIDEBAR_ROWS_MAX = 20


def sidebar_rows_patches(code, cs):
    """The sidebar's cameo rows, capped at 20 so 1440 and 4K fit its buttons.

    Each of the sidebar's two strips keeps its cameo buttons in a static array
    at 0x0080B950, 20 per strip (index = strip * 20 + row, 52 bytes each;
    0x005F42A0 lays them out). How many rows it lays out is the strip's height
    over the cameo height, and from about 1440 lines that is past 20: at 4K,
    40 rows wrote over what follows the array and the game faulted building
    the sidebar (docs/hires.md). Every place that divides by the cameo height
    loads it the same way -- mov r, [0x0080C3AC]; movsx r2, word [r + 4]; cdq;
    idiv r2 -- so each is found by that shape and its quotient capped right
    after the idiv: a row count, or the row under the mouse, past 20 means a
    row there is no button for. Up to 1080 lines nothing changes.
    """
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    ref = (0x0080C3AC).to_bytes(4, "little")
    out, i = {}, code.find(ref)
    while i != -1:
        ins = list(md.disasm(code[i - 2:i + 0x30], cs + i - 2))
        if ins and ins[0].op_str.endswith('[0x80c3ac]'):
            for k in ins[:9]:
                if k.mnemonic == 'idiv':
                    out[k.address] = ('if ((int32_t)eax > %d) eax = %d; /* remaster: sidebar rows, '
                                      'run_lift.py sidebar_rows_patches */' % (SIDEBAR_ROWS_MAX, SIDEBAR_ROWS_MAX))
                    break
        i = code.find(ref, i + 1)
    return out


def apply_patches(body, patches):
    """Add each patch's C after the lines that lift its instruction.

    Some instructions lift to a block over several lines (idiv's `{ ... }`):
    the patch goes after the block closes, not inside it."""
    for va, c in patches.items():
        tag = '/* 0x%08X:' % va
        k = body.find(tag)
        if k != -1:
            s = body.rfind('\n', 0, k) + 1
            e = body.index('\n', k)
            depth = body[s:e].count('{') - body[s:e].count('}')
            while depth > 0:
                s, e = e + 1, body.index('\n', e + 1)
                depth += body[s:e].count('{') - body[s:e].count('}')
            body = body[:e + 1] + '    ' + c + '\n' + body[e + 1:]
    return body


def exe_stamp(path):
    """The PE header's TimeDateStamp: which build of the game an exe is."""
    with open(path, 'rb') as f:
        head = f.read(4096)
    nt = int.from_bytes(head[0x3C:0x40], 'little')
    return int.from_bytes(head[nt + 8:nt + 12], 'little')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default=EXE)
    ap.add_argument('--catalog', default=CATALOG)
    ap.add_argument('--out', default=OUT)
    ap.add_argument('--roots', default='', help='comma-separated extra root VAs')
    ap.add_argument('--max', type=int, default=3000, help='closure size cap')
    ap.add_argument('--all', action='store_true', help='lift every function')
    ap.add_argument('--virtual', action='store_true',
                    help='root the closure at every RTTI vtable method too')
    ap.add_argument('--split', type=int, default=400, help='functions per .c file')
    # More entries a run found, on top of RUN_SEEDS (seed_from_log.py JSON).
    ap.add_argument('--seeds', default=os.path.join(_HERE, 'work', 'run_seeds.json'),
                    help='seed_from_log.py JSON of unresolved targets from runs')
    args = ap.parse_args()
    if not os.path.exists(args.catalog):
        sys.exit('no catalog at %s -- run disasm32.py first (README, Step by step)' % args.catalog)

    # The patches are at this build's addresses; another build lifts, but
    # they land in the wrong code, and it crashes in game.
    stamp = exe_stamp(args.exe)
    if stamp != EXE_STAMP:
        sys.exit('%s is not the build this project supports: its PE timestamp is 0x%08X, '
                 'the Steam release\'s is 0x%08X (docs/RECON.md). A different release '
                 '(the 2010 freeware, a CD, a CnCNet- or mod-patched exe) needs its own '
                 'addresses; use the Steam build\'s Game.exe.' % (args.exe, stamp, EXE_STAMP))
    info = analyze_pe(args.exe)
    iat = build_iat_map(info)
    cs, ce = info.code_start, info.code_end
    print('[*] base=0x%08X code=0x%08X-0x%08X IAT=%d' % (info.image_base, cs, ce, len(iat)))

    cat = json.load(open(args.catalog))
    byaddr = {f['address']: f for f in cat['functions'] if cs <= f['address'] < ce}
    print('[*] catalog: %d functions inside .text' % len(byaddr))

    # Vtable slots. Bound each by the next known entry: a slot landing mid-code
    # handed `ce` makes the extent walk descend the whole of .text.
    want = {a for a in RUN_SEEDS if cs <= a < ce and a not in byaddr}
    for path in [SEEDS] + ([args.seeds] if args.seeds else []):
        if os.path.exists(path):
            want |= {e['address'] for e in json.load(open(path))
                     if cs <= e['address'] < ce and e['address'] not in byaddr}
    known = sorted(set(byaddr) | want)
    nxt = {a: (known[i + 1] if i + 1 < len(known) else ce) for i, a in enumerate(known)}
    for a in want:
        byaddr[a] = {'address': a, 'end': min(nxt[a], a + 0x100), 'calls_to': [],
                     'entry_kind': 'start'}
    print('[*] vtable slots not in the catalog: %d injected' % len(want))

    entry = info.image_base + info.entry_point_rva
    roots = [entry] + [int(x, 0) for x in args.roots.split(',') if x.strip()]
    if args.virtual and os.path.exists(SEEDS):
        roots += sorted(e['address'] for e in json.load(open(SEEDS)))
    if args.all:
        chosen = set(byaddr)
    else:
        chosen = set(closure(byaddr, roots, args.max))
    print('[*] lifting %d of %d functions (%s)'
          % (len(chosen), len(byaddr), 'all' if args.all else 'closure, cap %d' % args.max))

    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    text = [s for s in info.sections if s.name == '.text'][0]
    code = open(args.exe, 'rb').read()[text.raw_offset:text.raw_offset + text.raw_size]

    # Known targets become RECOMP_CALL; anything else a decoded `call` names
    # becomes RECOMP_ICALL, which reports at run time instead of failing the build.
    # precise carry: adc/sbb take their carry from the flag state that set it.
    # The default reads a `_cf` that add/sub/cmp never write, which breaks
    # every 64-bit add and subtract. The Movies lost its menu video to it
    # (its docs/bringup.md); off by default in lift32 only because Fury3 leans
    # on the imprecision.
    patches = sidebar_rows_patches(code, cs)
    patches.update(HD_VOXEL_PATCHES)
    patches.update(PATCHES)
    print('[*] remaster patches: %d' % len(patches))
    lifter = Lifter(iat_map=iat, lifted=set(byaddr), precise_carry=True, precise_sbb=True)
    os.makedirs(args.out, exist_ok=True)
    for fn in os.listdir(args.out):                 # a smaller lift must not leave stale chunks
        if fn.startswith('recomp_') and fn.endswith('.c'):
            os.remove(os.path.join(args.out, fn))
    entries, chunk, idx, errors, dirty = [], [], 0, 0, 0
    ordered = sorted(byaddr)
    starts = {a for a in ordered if byaddr[a].get('entry_kind') != 'alias'}
    t_split = time.time()
    splits = find_splits(md, code, cs, ce, starts)
    starts -= splits          # still dispatchable; just not a wall inside its parent
    print('[*] split entries (the middle of the function before them): %d, %.0fs'
          % (len(splits), time.time() - t_split))
    t0 = time.time()

    def flush(force=False):
        nonlocal chunk, idx
        if chunk and (force or len(chunk) >= args.split):
            write_chunk(args.out, idx, chunk)
            idx += 1
            chunk = []

    def lift_one(addr, name, end, reached):
        nonlocal errors

        if addr in HOOKS:
            chunk.append(('extern void ts_hook_%08X(void);\nvoid %s(void) { ts_hook_%08X(); }\n'
                          % (addr, name, addr), addr, name))
            entries.append((addr, name))
            return
        try:
            lo = min(reached) if reached else addr   # a chunk can sit below the entry
            insns, leaders = (linear_disassemble_function(md, code, cs, lo, end, reached=reached)
                              if end > addr else ([], None))
            if leaders is not None:
                leaders.add(addr)
            body = (lift_function_linear(lifter, name, insns, leaders, addr) if insns
                    else 'void %s(void) { }\n' % name)
            body = apply_patches(body, {va: c for va, c in patches.items() if reached and va in reached})
        except Exception as e:                      # noqa: BLE001 -- counted, not hidden
            body = '/* ERROR %s: %s */\nvoid %s(void) { }\n' % (name, e, name)
            errors += 1
        chunk.append((body, addr, name))
        entries.append((addr, name))
        if len(chunk) >= args.split:
            flush()
            print('[*]   %d/%d (%d err)' % (len(entries), len(chosen), errors), flush=True)

    # A direct branch that leaves a body backward to an address nothing
    # catalogued is a tail call the catalog missed (__mtterm: jmp 0x00AD6FB7)
    # or a jump into shared code in a neighbour (hand-written x87 math:
    # __ffexpm1 jne 0x00AE01F0). A direct call can name one too: the catalog
    # dropped 0x00C10170, called directly, because a false start inside the
    # jump table before it decoded over it. Either way the target has to be dispatchable,
    # or the RECOMP_ITAIL cannot resolve. So each round's outside targets
    # become entries and are lifted in the next round, until none are new.
    todo, added = sorted(chosen), 0
    while todo:
        outside = set()
        for addr in todo:
            name = 'sub_%08X' % addr
            reached, behind, called = set(), set(), set()
            end, clean = true_extent(md, code, cs, addr, min(addr + EXTENT_REACH, ce), starts,
                                     reached=reached, behind=behind, called=called)
            outside |= {t for t in behind if t not in reached} | called
            dirty += not clean
            lift_one(addr, name, end, reached)
        todo = sorted(t for t in outside if cs <= t < ce and t not in byaddr)
        for t in todo:
            byaddr[t] = {'address': t, 'end': ce, 'calls_to': [], 'entry_kind': 'start'}
            chosen.add(t)
        added += len(todo)
    print('[*] branch and call targets outside the catalog, added as entries: %d' % added)

    stubs = [a for a in ordered if a not in chosen]
    for a in stubs:
        name = 'sub_%08X' % a
        chunk.append(('void %s(void) { RECOMP_NOT_LIFTED(0x%08Xu); }\n' % (name, a), a, name))
        entries.append((a, name))
        flush()
    flush(force=True)

    with open(os.path.join(args.out, 'recomp_funcs.h'), 'w', newline='\n') as f:
        f.write('/* Game.exe - AUTO-GENERATED by run_lift.py */\n#pragma once\n'
                '#include <stdint.h>\n\n'
                'void recomp_not_lifted(uint32_t va);\n'
                '#define RECOMP_NOT_LIFTED(va) recomp_not_lifted(va)\n\n')
        for a, n in entries:
            f.write('void %s(void);\n' % n)
    with open(os.path.join(args.out, 'recomp_dispatch.c'), 'w', newline='\n') as f:
        f.write('/* Game.exe - AUTO-GENERATED by run_lift.py */\n'
                '#include "recomp_types.h"\n#include "recomp_funcs.h"\n\n'
                'const recomp_dispatch_entry_t recomp_dispatch_table[] = {\n')
        for a, n in sorted(entries):
            f.write('    { 0x%08Xu, %s },\n' % (a, n))
        f.write('};\nconst uint32_t recomp_dispatch_count = %d;\n'
                'const uint32_t ts_entry_va = 0x%08Xu;\n' % (len(entries), entry))

    lines = sum(sum(1 for _ in open(os.path.join(args.out, fn), encoding='utf-8', errors='replace'))
                for fn in os.listdir(args.out))
    stats = {'lifted': len(chosen), 'stubs': len(stubs), 'errors': errors,
             'no_terminator': dirty, 'files': idx, 'lines': lines}
    json.dump(stats, open(STATS, 'w'), indent=1)
    print('=' * 60)
    print('  lifted %d   not-lifted stubs %d   errors %d   no terminator %d'
          % (len(chosen), len(stubs), errors, dirty))
    print('  %s lines of C in %d files, %.1fs' % (format(lines, ','), idx, time.time() - t0))
    print('=' * 60)


if __name__ == '__main__':
    main()

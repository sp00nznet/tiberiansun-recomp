#!/usr/bin/env python3
"""Scripted play tests: walk every menu and game mode headless, in parallel, and keep the evidence.

Red Alert 2's runner (redalert2-recomp tools/playtest.py) adapted to
Tiberian Sun. A case is a list of host arguments. The choice screen and the
main menu are drawn by the game, so a case clicks them by position (in the
640x400 menus) and waits on the game's own log; the screens behind them are
Win32 dialogs in Language.dll, and a case presses those by caption, which the
runner resolves to a control ID with tools/dialogs.py's map
(docs/testing.md).

A run leaves, in work/tests/<case>/:

    run.log      everything the host printed
    run.mp4      the recording
    sheet.png    a contact sheet, one frame every --every seconds

and the summary line says how it ended: the exit code (4 = the watchdog, the
normal end of a timed run), any fault or not-lifted report, the dialogs that
opened, and the frames blitted in game. Reading the sheets is the actual test:
a pass says a case reached what it expected and ran clean, not that every
pixel was right.

    py -3 tools/playtest.py --list
    py -3 tools/playtest.py menu-firestorm
    py -3 tools/playtest.py --jobs 3
    py -3 tools/playtest.py skirmish-start --original   # the shipping code, same script

--original runs the shipping machine code under the same host, shims and
script (src/runtime/oracle.c), into work/tests/<case>.original/: where a case
fails on the lift and passes there, the lift is wrong; where both fail, the
host is.
"""
import argparse
import fnmatch
import json
import os
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# TS_EXE: another build of the host to test (a clang-cl build, an A/B variant).
HOST = os.environ.get('TS_EXE') or os.path.join(ROOT, 'build', 'ts.exe')
OUT = os.path.join(ROOT, 'work', 'tests')
DIALOGS = os.path.join(ROOT, 'work', 'dialogs.json')

# The dialog screens (py -3 tools/dialogs.py --show 0xB4 lists one).
SKIRMISH = 0xB4

# The game-drawn screens, in 640x400 pixels: the choice between Tiberian Sun
# and Firestorm, then the main menu's items.
CHOICE_TS, CHOICE_FS = (160, 190), (470, 200)
NEW_CAMPAIGN, MENU_LAN, MENU_SKIRMISH, MENU_OPTIONS = (318, 158), (318, 216), (318, 306), (318, 368)
# The main menu has no log line of its own: the movie into it ends with this.
MENU_OPEN = 'VQ audio handler closed OK'
# In game, and nowhere in the menus.
INGAME = '[game] Tooltips are on.'

_dialogs = None


def control(dlg, label):
    """Control ID of the control captioned <label> in dialog `dlg`."""
    global _dialogs
    if _dialogs is None:
        if not os.path.exists(DIALOGS):
            subprocess.run([sys.executable, os.path.join(ROOT, 'tools', 'dialogs.py')], check=True)
        _dialogs = json.load(open(DIALOGS))
    for c in _dialogs['0x%X' % dlg]['controls']:
        if c['text'] == label:
            return c['id']
    raise KeyError('no %s in dialog 0x%X' % (label, dlg))


# Event times are seconds after the main menu first opened; --press and
# --select wait for their dialog on top of that, so they only need to come in
# the right order. One second apart is plenty.
class Script:
    def __init__(self, screen=(640, 400)):
        self.args, self.t, self.expect = [], 1.0, []
        # Above 640x400 the game centres its menus: clicks are in menu pixels.
        self.org = ((screen[0] - 640) // 2, (screen[1] - 400) // 2)

    def press(self, dlg, label, opens=None):
        """`label` is a caption, or a control ID for an uncaptioned one."""
        cid = label if isinstance(label, int) else control(dlg, label)
        self.args += ['--press', '0x%X:%d@%g' % (dlg, cid, self.t)]
        self.t += 1
        if opens is not None:
            self.expect.append(opens)
        return self

    def select(self, dlg, ctrl, n):
        self.args += ['--select', '0x%X:%d=%d@%g' % (dlg, ctrl, n, self.t)]
        self.t += 1
        return self

    def waitlog(self, text):
        """Hold until the game's debug log prints `text` (Capture_Mouse: in game)."""
        self.args += ['--waitlog', '%s@%g' % (text, self.t)]
        self.t += 1
        return self

    def move(self, x, y, after=0):
        self.t += after
        self.args += ['--move', '%d,%d@%g' % (x, y, self.t)]
        self.t += 1
        return self

    def click(self, x, y, after=0, mods=''):
        """A left click at game pixel x, y; mods 'c', 's', 'a' held with it
        (Ctrl+click is force-fire)."""
        self.t += after
        self.args += ['--click', '%s%d,%d@%g' % (mods + '+' if mods else '', x + self.org[0], y + self.org[1], self.t)]
        self.t += 1
        return self

    def drag(self, x1, y1, x2, y2, after=0):
        """A band selection from x1, y1 to x2, y2 (game pixels)."""
        self.t += after
        self.args += ['--drag', '%d,%d,%d,%d@%g' % (x1, y1, x2, y2, self.t)]
        self.t += 2
        return self

    def key(self, vk, after=0):
        self.t += after
        self.args += ['--key', '%s@%g' % (vk, self.t)]
        self.t += 1
        return self


def S():
    return Script()


# name: (script, seconds the run lasts, what it must show)
#   expect 'dialogs': every listed dialog opened
#   expect 'ingame':  the game started (its own "Tooltips are on." line) and
#                     the picture kept changing (5+ distinct sampled frames)
#   expect 'alive':   no human player was defeated
#   expect 'exit':    the exit code
#   expect 'ini':     SUN.INI values for the case's game folder, {section: {key: value}}
#   expect 'size':    the picture in game is this (w, h)
CASES = {}


def case(name, script, seconds=60, **expect):
    expect.setdefault('dialogs', script.expect)
    CASES[name] = (script.args, seconds, expect)


def firestorm(screen=(640, 400)):
    """The choice screen's Firestorm, and the main menu once it is in."""
    return Script(screen).click(*CHOICE_FS, after=2).waitlog(MENU_OPEN)


# The boot, to the choice screen and its music.
case('boot', S(), 60, dialogs=[], log={'Theme::PlaySong(30)': 1})
# Into each game's main menu.
case('menu-firestorm', firestorm(), 40, dialogs=[], log={MENU_OPEN: 3})
case('menu-tibsun', S().click(*CHOICE_TS, after=2).waitlog(MENU_OPEN), 40, dialogs=[], log={MENU_OPEN: 3})
# The skirmish setup screen, and back out of it.
case('menu-skirmish', firestorm().click(*MENU_SKIRMISH, after=4).press(SKIRMISH, 'Cancel'), 60,
     dialogs=[SKIRMISH])
# A skirmish with the defaults: in game, the picture moving.
case('skirmish-start', firestorm().click(*MENU_SKIRMISH, after=4).press(SKIRMISH, 'OK'), 90,
     dialogs=[SKIRMISH], ingame=True)
# Each campaign to its first mission: the campaign screen (a dialog, its
# list: 0 the GDI act, 1 the Nod act), OK, then Escape once the first movie
# opens skips the movies.
CAMPAIGN, CAMPAIGN_LIST = 0x94, 1109
for game, choice in (('fs', CHOICE_FS), ('ts', CHOICE_TS)):
    for n, side in ((0, 'gdi'), (1, 'nod')):
        case('campaign-%s-%s' % (game, side),
             S().click(*choice, after=2).waitlog(MENU_OPEN).click(*NEW_CAMPAIGN, after=4)
             .select(CAMPAIGN, CAMPAIGN_LIST, n).press(CAMPAIGN, 'OK')
             .waitlog('Opening VQ audio handler').key('0x1B', after=2).waitlog('Tooltips are on.'),
             150, dialogs=[CAMPAIGN], ingame=True)
# Building, on the first Nod mission's base: a power plant and a Hand of Nod
# from the sidebar, each placed by trying spots around the base (Home centres
# the view on it), then a light infantry. Checked in the game's event log.
# The base sits at the view's left edge: the spots start beside it.
PLACES = [(60, 300), (110, 300), (60, 360), (150, 370), (40, 230), (120, 250), (190, 330),
          (170, 250), (236, 300), (150, 160), (300, 250), (300, 300), (236, 360), (340, 340)]
PP, HAND, LIGHT_INF = (527, 245), (527, 300), (590, 195)


def place(script):
    script = script.key('0x24', after=1)           # Home: the view on the base
    for x, y in PLACES:
        script = script.click(x, y, after=1)
    return script


case('campaign-build',
     place(place(S().click(*CHOICE_TS, after=2).waitlog(MENU_OPEN).click(*NEW_CAMPAIGN, after=4)
                 .select(CAMPAIGN, CAMPAIGN_LIST, 1).press(CAMPAIGN, 'OK')
                 .waitlog('Opening VQ audio handler').key('0x1B', after=2).waitlog('Tooltips are on.')
                 .click(*PP, after=3)                          # power plant
                 .click(*PP, after=30))                        # ready: pick it up
           .click(*HAND, after=3)                              # Hand of Nod
           .click(*HAND, after=60))                            # ready: pick it up
     .click(*LIGHT_INF, after=3),                              # a light infantry
     240, dialogs=[CAMPAIGN], ingame=True, log={'Adding event PRODUCE': 3, 'Adding event PLACE': 2})

# HD voxels on (the presenter's default; --hd-voxels headless): a skirmish
# and the build case, the game unchanged by it.
_hd = firestorm().click(*MENU_SKIRMISH, after=4).press(SKIRMISH, 'OK')
_hd.args.append('--hd-voxels')
case('skirmish-hd', _hd, 90, dialogs=[SKIRMISH], ingame=True)

# High resolution: SUN.INI [Video], what the presenter's settings menu writes.
for w, h, tag in ((1280, 720, '720p'), (1920, 1080, '1080p'), (2560, 1440, '1440p'), (3840, 2160, '4k')):
    case('skirmish-' + tag, firestorm((w, h)).click(*MENU_SKIRMISH, after=4).press(SKIRMISH, 'OK'), 180,
         dialogs=[SKIRMISH], ingame=True, size=(w, h),
         ini={'Video': {'ScreenWidth': str(w), 'ScreenHeight': str(h)}})


def set_ini(text, section, key, value):
    """SUN.INI with [section] key=value set, the section added if missing."""
    lines = text.split(b'\r\n') if b'\r\n' in text else text.split(b'\n')
    want = b'[' + section.encode() + b']'
    out, in_sec, done = [], False, False
    for l in lines:
        if l.strip().startswith(b'['):
            if in_sec and not done:
                out.append(b'%s=%s' % (key.encode(), str(value).encode()))
                done = True
            in_sec = l.strip().lower() == want.lower()
        elif in_sec and l.split(b'=')[0].strip().lower() == key.lower().encode():
            if not done:
                out.append(b'%s=%s' % (key.encode(), str(value).encode()))
                done = True
            continue
        out.append(l)
    if not done:
        if not in_sec:
            out += [b'', want]
        out.append(b'%s=%s' % (key.encode(), str(value).encode()))
    return b'\r\n'.join(out)


def farm(d, ini=None):
    """A private game folder for one case, made of hard links to game/.

    The game writes into its folder (SUN.INI, saves, debug files), so cases
    sharing one would clobber each other; links cost no space. SUN.INI is
    copied, with the intro off: four minutes of cinematic before every case
    is a waste, and the intro has its own case in tools/conformance.py.
    """
    src = os.path.join(ROOT, 'game')
    for dirpath, dirs, files in os.walk(src):
        out = os.path.join(d, os.path.relpath(dirpath, src))
        os.makedirs(out, exist_ok=True)
        for f in files:
            t = os.path.join(out, f)
            if f.upper() == 'SUN.INI':
                if os.path.exists(t):
                    os.remove(t)
                text = open(os.path.join(dirpath, f), 'rb').read()
                text = re.sub(rb'(?im)^Play=\w+', b'Play=no', text)
                for sec, kv in (ini or {}).items():
                    for k, v in kv.items():
                        text = set_ini(text, sec, k, v)
                open(t, 'wb').write(text)
            elif not os.path.exists(t):
                os.link(os.path.join(dirpath, f), t)
    return d


def run(name, args, seconds, expect, every, original=False):
    d = os.path.join(OUT, name + ('.original' if original else ''))
    if os.path.isdir(os.path.join(d, 'game')):
        shutil.rmtree(os.path.join(d, 'game'))      # a fresh folder: no saves left over
    os.makedirs(d, exist_ok=True)
    game = farm(os.path.join(d, 'game'), expect.get('ini'))
    mp4, log = os.path.join(d, 'run.mp4'), os.path.join(d, 'run.log')
    cmd = [HOST, '--headless', '--run', '--mute', '--debuglog', '--watchdog', str(seconds),
           '--record', mp4, '--exe', os.path.join(game, 'Game.exe'), '--game', game] + args
    cmd += os.environ.get('TS_HOST_ARGS', '').replace('{case}', d).split()  # extra host flags; {case} is the case's folder
    if original:
        cmd.append('--original')
    with open(log, 'w', errors='replace') as f:
        try:
            code = subprocess.run(cmd, cwd=ROOT, stdout=f, stderr=subprocess.STDOUT,
                                  timeout=seconds + 300).returncode
        except subprocess.TimeoutExpired:
            code = 'timeout'
    text = open(log, errors='replace').read()
    if os.path.exists(mp4):
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', mp4, '-vf',
                        'fps=1/%g,scale=320:-1,tile=4x4' % max(every, seconds / 16.0), '-frames:v', '1',
                        os.path.join(d, 'sheet.png')], cwd=ROOT)
    lines = text.splitlines()
    # An unresolved ICALL returns 0 and the game carries on: the voxel
    # rasterizer was one, and every vehicle was invisible (docs/voxels.md).
    bad = [l for l in lines if l.startswith(('===', '[not-lifted]', 'ITAIL', 'ICALL', '[messagebox]'))]
    bad += [l for l in lines if l.startswith('[input]') and ('never opened' in l or 'no such control' in l)]
    opened = []
    for m in re.finditer(r'\[dialog\] open 0x([0-9A-F]+)', text):
        if not opened or opened[-1] != m.group(1):
            opened.append(m.group(1))
    distinct = len(set(re.findall(r'\[record\] frame \d+ (?:at \S+ )?checksum ([0-9A-F]{8})', text)))
    ingame = INGAME in text
    modes = re.findall(r'\[headless\] SetDisplayMode\((\d+)x(\d+)x\d+\)', text)
    defeated = re.search(r'\[game\] MPlayer_Defeated\(\) - Player <human player> has been defeated', text)
    seen = ['dialogs ' + ' '.join(opened)] if opened else []
    if ingame:
        seen.append('in game, %d distinct frames' % distinct)
    if defeated:
        seen.append('human defeated')
    for dlg in expect.get('dialogs', []):
        if '%X' % dlg not in opened:
            bad.append('dialog 0x%X never opened' % dlg)
    if expect.get('ingame') and not (ingame and distinct >= 5):
        bad.append('never in game' if not ingame else 'picture stopped (%d frames)' % distinct)
    if 'size' in expect and (not modes or tuple(map(int, modes[-1])) != tuple(expect['size'])):
        bad.append('in game at %s, wanted %dx%d' % ('x'.join(modes[-1]) if modes else 'no mode', *expect['size']))
    if modes and ingame:
        seen.append('%sx%s' % modes[-1])
    if expect.get('alive') and defeated:
        bad.append('the human player was defeated')
    # log: {text: at least n}, lines the game's own debug log must print
    # ("Adding event PRODUCE" for each order to build or train).
    for text, n in expect.get('log', {}).items():
        got = sum(text in l for l in lines)
        if got < n:
            bad.append('"%s" %d times, wanted %d' % (text, got, n))
        elif n:
            seen.append('%s x%d' % (text.split()[-1].lower(), got))
    if 'exit' in expect and code != expect['exit']:
        bad.append('expected exit %d' % expect['exit'])
    elif 'exit' not in expect and code not in (0, 4):
        bad.append('exit %s' % code)
    return name, code, bad, seen


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('cases', nargs='*', help='case names or globs (default: all)')
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--jobs', type=int, default=3)
    ap.add_argument('--every', type=float, default=5, help='contact-sheet spacing, seconds')
    ap.add_argument('--original', action='store_true', help='run the shipping code instead (oracle.c)')
    a = ap.parse_args()
    if a.list:
        for n, (args, s, e) in CASES.items():
            print('%-20s %4ds  %s' % (n, s, ' '.join(args)))
        return 0
    if not (os.path.exists(HOST) and os.path.isdir(os.path.join(ROOT, 'game'))):
        print('playtest: skipped -- needs game/ (your copy) and build/ts.exe (README)')
        return 0
    names = [n for n in CASES if not a.cases or any(fnmatch.fnmatch(n, p) for p in a.cases)]
    failed = 0
    os.makedirs(OUT, exist_ok=True)
    summary = open(os.path.join(OUT, 'summary%s.txt' % ('.original' if a.original else '')), 'w')
    with ThreadPoolExecutor(a.jobs) as ex:
        for name, code, bad, seen in ex.map(lambda n: run(n, *CASES[n], a.every, a.original), names):
            name += '.original' if a.original else ''
            line = '%s %-20s exit %-7s %s%s' % ('FAIL' if bad else 'pass', name, code, '; '.join(seen) or '-',
                                                ('  !! ' + ' | '.join(bad[:3])) if bad else '')
            print(line, flush=True)
            summary.write(line + '\n')
            summary.flush()
            failed += bool(bad)
    print('%d of %d failed' % (failed, len(names)))
    summary.write('%d of %d failed\n' % (failed, len(names)))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())

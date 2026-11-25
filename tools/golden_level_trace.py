#!/usr/bin/env python3
"""golden_level_trace.py - read the ORIGINAL's live object state inside a
playable level (OI-129). The first tool here that reads the game's RAM
instead of its screen.

Rig: the disk-D recipe of tools/golden_build_rig.sh (DDIR=1: no CD medium,
no driver, D: is a second FAT disk with \\LEO\\), started with the same
QEMU argv as golden_qemu_run.py and driven through the same QMP client
(golden_qemu_run.HMP). What makes a RAM read possible at all: the .LE
image is loaded flat, so the addresses the image ledger uses (tools/re,
the leo.sqlite index) are the guest's addresses 1:1. That is not assumed -
it is measured at run start, by finding the two runtime strings (image
0x95c20 "pure virtual function called!", 0x95c40 "undefined constructor
or destructor called!") in a `pmemsave` dump and reporting the base skew
they give. Every later address is base + <image address>.

From there the ledger gives the object map directly:
  * [obj+0x8] is the class table. The authored Zaria lion uses the
    TLewZaria table (PR-380 names it 0x96960); --class-va selects a
    table and --trace-object can follow one object found in an earlier pass;
  * [obj+0x00]/[obj+0x04] are the LIVE POSITION: PR-357 read the
    land/leave pair 0x42980/0x429d0 as writes to [edx]/[edx+4], and at
    0x54f74 that edx is the object itself;
  * [obj+0xa9] is the unaligned held-surface handle (PR-345/PR-346).
    PR-390 measured an ordinary jump: this handle clears when leaving the
    floor and is set to one provider element when landing, while
    [obj+0xB6] bit 0x01 stays clear. The B6 bit remains useful as the
    separate flag PR-354 read, but it is not a reliable normal-landing
    detector;
  * [obj+0x89]/[obj+0xd6] are the two aim sample points, whose changing
    distance is the live element offset measured by PR-364.
So OI-129's question - does the ~31 px step shipped in the .SEQ data
really move the live position at a landing? - is observable, without
modelling anything.

Phases: boot (text screen shown if the game is not up), attract (optionally
sends one Esc into the opening movie, then starts the highlighted chapter
with Down alone after the map menu is confirmed), level-wait
(polls RAM for a new class-word object set: that IS the level-start
detector, the state the geometry classifier cannot tell from a 640x480
.IS movie), then the trace itself, optionally driven (--drive walks the
lion right, then lets go).

Output: a JSONL trace of sampled object windows plus a console summary
(the held-surface handle transitions, the standing-bit transitions, the
position delta across each, the two sample points [obj+0x89]/[obj+0xd6],
and the live level descriptor). Nothing here is a fixture: it is a reading
of the original, and it is only honest if the report says what it saw.
"""

import argparse
import collections
import json
import os
import re
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from golden_qemu_run import (HMP, _xp_bytes, ppm_size, read_tick, sha256,
                             text_grid)

MARKS = ((0x95c20, b'pure virtual function called!'),
         (0x95c40, b'undefined constructor or destructor called!'))
LION = 0x96874            # the lion class table (PR-238, PR-387)

# The game/world anchor the ledger names (OI-127, PR-343): [anchor+0x74] is
# the surface-entry array and [anchor+0x80] its count, so reading this one
# dword says whether a level exists - without ever stopping the guest.
WORLD_ANCHOR = 0xcef40

# Measured on this rig in every boot so far (2026-11-24, three boots): where
# the .LE image's data block lands in guest memory. Used only as the first
# address to CHECK - the two marker strings are read there - never as an
# assumption.
EXPECT_BASE = 0x16c000

# Scratch file for `pmemsave`. QEMU writes it as the emulator's user, so it
# goes somewhere both sides can reach; the tool's workdir default.
RAMWIN = '/tmp/opencode/golden/ram_window.bin'
CLASS_OFF = 0x8           # [obj+0x8] = the class table (PR-388)
CLASS_LO, CLASS_HI = 0x9637d, 0xa6b7d   # the class-table span (PR-387)
FIELDS = [                # name -> offset in the object, per the ledger
    ('x', 0x00), ('y', 0x04), ('plane', 0x24), ('contact', 0x2c),
    ('asked', 0x3c), ('surface_handle', 0xa9),
    ('sample_a_x', 0x89), ('sample_a_y', 0x8d),
    ('sample_b_x', 0xd6), ('sample_b_y', 0xda),
    # PR-392: the ask's answer fields. 0x45400 writes the winning ground
    # container at [obj+0xe6], its interpolated clearance at [obj+0xea] and
    # the separator flag byte of the chosen record at [obj+0xe2]; 0x45250
    # writes the clamp delta at [obj+0xca]/[obj+0xce].
    ('answer_flag', 0xe2), ('answer_surface', 0xe6), ('answer_gap', 0xea),
    ('clamp_dx', 0xca), ('clamp_dy', 0xce),
    # PR-514: the two intent words. 0x549f9/0x54a08 store the caller's
    # input-block words 0/1 into them EVERY iteration, unconditional on
    # the object's state (PR-147 F3, PR-188 F1); the kind-1/4 contact
    # chain may then overwrite them with +/-10. This is the x-force's
    # s factor (0x542ab) - the field the port only ever seeds for
    # BY_X states.
    ('intent_x', 0xba), ('intent_y', 0xbe),
]
WIN = 0x140               # sampled window: covers every FIELDS offset
MENU_AT = (640, 400)      # the chapter map menu's CRTC geometry (PLAN S6)
# The game's opening contains short 640x400 movie frames too. The one
# measured attract-to-map transition was at t=423.6 s; do not inject menu
# keys into an earlier movie frame (a false detection did exactly that at
# t=25.9 s on 2026-11-24).
MENU_EARLIEST_T = 300.0


def qemu_argv(wd, floppy, hd, ddisk, ram, display='none', accel='tcg',
              icount_shift=None):
    """Same argv as golden_qemu_run.py: std-VGA, SB16 (the audio clock the
    original rig had), QMP on a unix socket, and stdin=/dev/null for the
    reason documented there. --display only changes the window: the RAM
    reading works the same either way, so a scripted run and a hand-driven
    one are directly comparable."""
    argv = ['qemu-system-i386', '-m', str(ram), '-boot', 'a',
            '-fda', floppy, '-hda', hd,
            '-machine', 'pc', '-vga', 'std', '-display', display,
            '-audiodev', 'none,id=a0', '-device', 'sb16,audiodev=a0',
            '-qmp', 'unix:' + os.path.join(wd, 'mon.sock') + ',server,nowait']
    if accel == 'kvm':
        # Reading the guest through the monitor stalls the vCPU for a moment
        # (a screendump most of all). Under TCG the emulated PIT then replays
        # the ticks it missed back to back, and the original died of it:
        # "transfer stack overflow on interrupt 08h" from DOS/4GW, and DOS's
        # own "Internal stack overflow / System halted" (both 2026-11-24).
        # KVM's in-kernel PIT with lost_tick_policy=discard drops them.
        argv[1:1] = ['-accel', 'kvm', '-cpu', 'host',
                     '-global', 'kvm-pit.lost_tick_policy=discard']
    if icount_shift is not None:
        # QEMU's instruction counter sets a maximum virtual CPU rate of
        # 1e9 / 2**shift instructions per second. Align it to host time and
        # make the RTC follow VM time so pauses don't inject accumulated
        # wall-clock ticks into this DOS/4GW guest.
        argv[1:1] = ['-icount', 'shift=%d,align=on' % icount_shift,
                     '-rtc', 'clock=vm']
    if ddisk:
        i = argv.index('-hda')
        argv[i + 2:i + 2] = ['-hdb', ddisk]
    return argv


def ram_window(h, addr, size, path):
    """Guest memory to a file, without stopping the guest. `pmemsave` is the
    right tool and its earlier failure was mine: the file argument has to be
    QUOTED, or HMP's expression parser chokes on the path ("invalid char 't'
    in expression", measured 2026-11-24 - the same command with the path in
    double quotes returns no error and writes the file). Unlike
    `dump-guest-memory` it does not pause the VM, which is what a running
    game needs: the original is a DOS/4GW program and has been seen to die
    ("Internal stack overflow, System halted") when it was stopped under a
    memory dump."""
    h.cmd('pmemsave 0x%x 0x%x "%s"' % (addr, size, path))
    try:
        with open(path, 'rb') as f:
            return f.read(size)
    except OSError:
        return None


def flat_view(path, cap=64 * 1024 * 1024):
    """Rebuild a guest-physical view of a `dump-guest-memory` ELF: QEMU
    writes ELF64, and its PT_LOAD entries give the physical ranges. Returns
    (bytes, segments); separated from ram_dump so a captured dump can be
    re-checked offline, which is how the block-relocation fact was found.

    Only the low `cap` bytes are assembled. This machine's dump has a
    segment at 0xfd000000 - the 440BX chipset's vendor window - and a view
    that reached it would be 4 GB of Python bytes and every scan over it
    would take a minute, which is what made a hand-driven session look
    frozen. The game lives at 0x16c000..0x240000, far below the cap."""
    b = open(path, 'rb').read()
    if b[:4] != b'\x7fELF' or b[4] != 2:
        raise SystemExit('dump-guest-memory did not write an ELF64: %r'
                         % b[:8])
    e_phoff, = struct.unpack_from('<Q', b, 0x20)
    e_phentsize, e_phnum = struct.unpack_from('<HH', b, 0x36)
    top = 0
    segs = []
    for i in range(e_phnum):
        o = e_phoff + i * e_phentsize
        p_type = struct.unpack_from('<I', b, o)[0]
        p_offset, p_vaddr, p_paddr, p_filesz = struct.unpack_from('<QQQQ',
                                                                  b, o + 8)
        if p_type == 1 and p_filesz:
            segs.append((p_paddr, p_vaddr, p_filesz, p_offset))
            if p_paddr < cap:
                top = max(top, min(p_paddr + p_filesz, cap))
    flat = bytearray(top)
    ident = 0
    for paddr, vaddr, fsz, off in segs:
        if paddr == vaddr:
            flat[paddr:paddr + fsz] = b[off:off + fsz]
            ident += 1
    if not ident:
        raise SystemExit('no identity-mapped PT_LOAD segment: guest paging '
                         'is not a flat map, this tool would need the '
                         'segment bases instead')
    return bytes(flat), segs


def ram_dump(h, path):
    """One guest-RAM view through the monitor. `pmemsave` is unusable here:
    QEMU 10's HMP parses its file argument as an expression
    ("invalid char 't' in expression", measured 2026-11-24), so the dump
    comes from `dump-guest-memory`. Used instead of chunked `xp` because
    the first scan wants megabytes."""
    if os.path.exists(path):
        os.unlink(path)
    h.cmd('dump-guest-memory ' + path)
    # dump-guest-memory STOPS the guest and does not start it again: measured
    # 2026-11-24, a hand-driven session "froze" at whatever frame the last
    # dump caught. Resume it here, and keep the polls coarse (see --census-
    # every); the per-tick reads below use `xp`, which leaves it running.
    h.cmd('cont')
    if not os.path.exists(path):
        return None, []
    return flat_view(path)


def loaded_extent(segs, base):
    """What the dump's PT_LOAD segments say about the guest's address map.
    Reported, not used: on this rig the dump hands back one segment covering
    0xc0000-0x2000000, i.e. all of RAM, so the segment map cannot bound the
    image. Separating the game's static data from its live objects is done
    by the baseline delta in the level-wait loop instead."""
    lo = hi = None
    for paddr, _, fsz, _ in segs:
        if paddr <= base < paddr + fsz:
            lo, hi = paddr, paddr + fsz
    return lo, hi


def image_base(blob):
    """Guest address of image VA 0, from the runtime strings. None until
    the game is loaded."""
    hits = [blob.find(s) - va for va, s in MARKS if blob.find(s) >= 0]
    if len(hits) == len(MARKS) and len(set(hits)) == 1:
        return hits[0]
    return None


def image_base_live(h, guess):
    """The same answer without stopping the guest: read the two runtime
    strings at the candidate address with `xp` and accept it if they are
    there. Measured on this rig the image has loaded at 0x16c000 in every
    boot so far, so that is the candidate; a miss is not an assumption
    failure, it just means the caller has to scan. Worth doing because the
    scan is a `dump-guest-memory`, which stops the guest - and the ORIGINAL
    is a DOS/4GW program that has been seen to die ("Internal stack
    overflow, System halted", measured 2026-11-24) when it is stopped and
    restarted under it."""
    for va, s in MARKS:
        got = ram_window(h, guess + va, len(s), RAMWIN)
        if got != s:
            return None
    return guess


ANCHORS = (0x23a10, 0x23b50)   # the two vtable anchors PR-380/PR-386 measured


def class_tables(blob, base):
    """The class tables of the LIVE image, by PR-380's rule (186 tables hold
    the vtable anchor 0x23a10 at +0x6c, 33 hold 0x23b50). The anchor values
    in the running game are NOT base + 0x23a10, and one base is not enough:
    measured 2026-11-24, the lion table at base+0x96874 holds 0x19ca10 where
    the image file holds 0x23a10 there. The .LE image is relocated BLOCK BY
    BLOCK with the pointers fixed up per block - the block carrying the
    strings and the class tables sits at `base`, the one carrying code and
    vtables 0xd000 above it - which the same dump confirms a second way: the
    16 code sites that store the lion's class table hold 0x202874 =
    0x16c000 + 0x96874 and themselves sit at 0x197d0b, i.e. 0x179000 +
    0x1ed0b, inside the code block.

    So the vtable block's base is read off one table whose anchor the ledger
    already names, and the census is then the plain walk. Getting 186 and 33
    back is the check that the model is right."""
    known = LION                       # PR-238/PR-387: the lion class table
    if not (base + known + 0x70 < len(blob)):
        return set(), []
    v = struct.unpack_from('<I', blob, base + known + 0x6c)[0]
    block = v - ANCHORS[0]             # where the vtable block was loaded
    anchors = (block + ANCHORS[0], block + ANCHORS[1])
    lo = (base + CLASS_LO + 0x6c) & ~3
    hi = min((base + CLASS_HI + 0x6c) & ~3, len(blob) - 4)
    counts = collections.Counter()
    tables = set()
    for a in range(lo, hi, 4):
        v = struct.unpack_from('<I', blob, a)[0]
        if v in anchors:
            counts[v] += 1
            tables.add(a - 0x6c - base)
    info = [{'file_anchor': hex(x), 'runtime_anchor': hex(v),
             'tables': counts[v], 'vtable_block_base': hex(block)}
            for x, v in zip(ANCHORS, anchors)]
    return tables, info


def dword_at(h, addr):
    """One guest dword through the monitor. `xp` leaves the guest running,
    which `dump-guest-memory` does not - the whole reason a hand-driven
    session needs reads like this instead of scans."""
    raw = _xp_bytes(h, addr, 4, 4)
    return struct.unpack('<I', raw)[0] if len(raw) == 4 else None


def world_state(h, base):
    """Is a level loaded? Read the answer through the ledger's own pointer,
    not by scanning RAM: `0xcef40` is the game/world anchor the ledger names
    (OI-127, PR-343: `[anchor+0x74]` is the surface-entry array, `[anchor+
    +0x80]` its count), and these reads do not stop the guest, unlike a
    memory dump. Measured at the chapter map menu the anchor word is 0, so a
    non-zero value means the world object exists - the level boundary.

    Returns None while there is no world object, else the anchor and the
    fields the standing question needs."""
    w = dword_at(h, base + WORLD_ANCHOR)
    if not w:
        return None
    out = {'anchor': hex(w)}
    for name, off in (('scene', 0x64), ('surface_array', 0x74),
                      ('surface_count', 0x80)):
        v = dword_at(h, w + off)
        out[name] = hex(v) if v is not None else None
    return out


def surface_entry(surface_array, handle):
    """Describe a held-surface handle. The live measurement shows that
    [obj+0xa9] is an unaligned dword naming one 12-byte provider element
    (class pointer, record array, record count - PR-391), not the record's
    payload. The provider array and its count come from the world object."""
    handle &= 0xFFFFFFFF
    if not surface_array or not handle:
        return None
    off = handle - (surface_array & 0xFFFFFFFF)
    if off < 0 or off % 12:
        return {'handle': hex(handle), 'array': hex(surface_array),
                'outside_array': True}
    return {'handle': hex(handle), 'array': hex(surface_array),
            'element_index': off // 12, 'element_offset': off}


def find_base_live(h, lo=0x100000, hi=0x900000, step=0x80000):
    """image_base() without ever stopping the guest: read the low megabytes
    with `pmemsave` and look for the two runtime strings. 16 KB of window
    overlap so a mark cannot fall on a boundary."""
    for start in range(lo, hi, step):
        blob = ram_window(h, start, step + 0x4000, RAMWIN)
        off = image_base(blob) if blob else None
        if off is not None and start <= start + off < start + step:
            return start + off
    return None


def class_tables_live(h, base):
    """The same census as class_tables(), reading only the table span with
    `xp`: 71 KB in five reads, and the guest is never stopped. That matters
    because the ORIGINAL is a DOS/4GW program that has been seen to die
    ("Internal stack overflow, System halted") when a full memory dump
    stopped it mid-run."""
    known = LION                       # PR-238/PR-387: the lion class table
    v = dword_at(h, base + known + 0x6c)
    if v is None:
        return set(), []
    block = v - ANCHORS[0]             # where the vtable block was loaded
    anchors = (block + ANCHORS[0], block + ANCHORS[1])
    lo = (base + CLASS_LO + 0x6c) & ~3
    hi = (base + CLASS_HI + 0x6c) & ~3
    span = ram_window(h, lo, hi - lo, RAMWIN)
    if span is None:
        return set(), []
    counts = collections.Counter()
    tables = set()
    for i in range(0, len(span) - 3, 4):
        v = struct.unpack_from('<I', span, i)[0]
        if v in anchors:
            counts[v] += 1
            tables.add(lo + i - 0x6c - base)
    info = [{'file_anchor': hex(x), 'runtime_anchor': hex(v),
             'tables': counts[v], 'vtable_block_base': hex(block)}
            for x, v in zip(ANCHORS, anchors)]
    return tables, info


def census_live(h, base, tables, lo_off=0xc0000, hi_off=0x600000):
    """class_census() over the range where the game allocates, read with
    `xp`, so the guest is never stopped. The image's own static class words
    live below that range - measured at the chapter map menu, all 246 of
    them sat inside the image and its .bss and none in the heap - so
    searching above it is both cheaper and a better question: what did the
    game build?"""
    lo, hi = base + lo_off, base + hi_off
    blob = ram_window(h, lo, hi - lo, RAMWIN)
    if blob is None:
        return []
    return [(lo + x, c) for x, c in class_census(blob, base, tables)]


def class_census(blob, base, tables, scan_from=0):
    """An object inventory: every aligned dword whose value is one of the
    live image's class tables. A hit is a class word, because [obj+0x8] is
    where an object keeps its class (PR-388). This is the level detector:
    PR-238 showed an authored object can carry a class other than its
    constructor's default, so searching for one class (the lion's) cannot
    say whether a level is loaded - and static class-pointer tables exist
    in the image and its .bss, so the caller decides what is new by taking
    a baseline before the level starts.
    The byte pattern is only a fast pre-filter: every class table address
    shares its high two bytes with the search range."""
    v0, v1 = base + CLASS_LO, base + CLASS_HI
    b0, b1 = (v0 >> 16) & 0xFF, (v1 >> 16) & 0xFF
    if b0 > b1 or (v1 >> 24) != (v0 >> 24):
        raise SystemExit('class span %s-%s not covered by one byte pattern'
                         % (hex(v0), hex(v1)))
    pat = re.compile(b'(?s)..[' + bytes([b0]) + b'-' + bytes([b1]) + b']\x00')
    hits = []
    for m in pat.finditer(blob, scan_from):
        i = m.start()
        if i % 4 or i + 4 > len(blob):
            continue
        v = struct.unpack_from('<I', blob, i)[0]
        if v - base in tables:
            hits.append((i - CLASS_OFF, v - base))
    return hits


def load_names(path):
    """Optional token -> class-table names (the PR-380 token map, whose last
    column is the class table) so the log says KAMIEN and not 0x96a88."""
    names = {}
    try:
        with open(path) as f:
            for line in f:
                cols = line.rstrip('\n').split('\t')
                if len(cols) < 2:
                    continue
                try:
                    va = int(cols[-1], 16)
                except ValueError:
                    continue
                names.setdefault(va, cols[0])
    except OSError:
        return {}
    return names


def unpack_window(blob, obj):
    v = {'class': struct.unpack_from('<I', blob, obj + CLASS_OFF)[0]}
    for name, off in FIELDS:
        v[name] = struct.unpack_from('<i', blob, obj + off)[0]
    v['flags_b6'] = blob[obj + 0xb6]
    return v


def small_reads(h, objs):
    """Per-tick read: one small window per object, no full dump."""
    out = {}
    for o in objs:
        blob = _xp(h, o, WIN)
        if blob is not None:
            out[o] = unpack_window(blob, 0)
    return out


def batch_reads(h, objs, path):
    """Read a set of nearby objects with one non-pausing `pmemsave`, rather
    than one monitor round-trip per object. The original trace's 7 objects
    took about 1.3 s per sample on this throttled QEMU, too slow for a jump
    transition; batching the span keeps the requested 4 Hz cadence."""
    objs = sorted(set(objs))
    if not objs:
        return {}
    lo, hi = objs[0], objs[-1] + WIN
    blob = ram_window(h, lo, hi - lo, path)
    if blob is None or len(blob) != hi - lo:
        return {}
    return {o: unpack_window(blob, o - lo) for o in objs}


def _xp(h, addr, n):
    blob = bytearray()
    r = h.cmd('xp /%db 0x%x' % (n, addr))
    for line in r.splitlines():
        _, _, rest = line.partition(':')
        for tok in rest.split():
            tok = tok[2:] if tok.startswith('0x') else tok
            if re.fullmatch(r'[0-9a-fA-F]{2}', tok):
                blob.append(int(tok, 16))
    return bytes(blob) if len(blob) == n else None


def descriptor(h, base):
    """The level context of PR-389: the global at image 0xcef40, its
    +0x390 pointer, and the plane-scale descriptor 0x57500 reads. The
    indexing 0x57500/0x57532 actually use (re-read, /tmp/opencode/
    pr390_scale.py): value = [d+0x10] * T[plane] / T[[d+0x1c]] with
    T = [d+0x20], and the second form [d+0x14] * U[plane] / U[[d+0x1c]]
    + W[plane] with U = [d+0x24], W = [d+0x28]. So [d+0x1c] is a
    REFERENCE PLANE INDEX - PR-389 printed it as a table and this reads
    the arrays the corrected way."""
    g = _xp(h, base + 0xcef40, 4)
    if not g:
        return None
    ctx = struct.unpack('<I', g)[0]
    p = _xp(h, ctx + 0x390, 4)
    if not p:
        return {'ctx': ctx}
    d = struct.unpack('<I', p)[0]
    blk = _xp(h, d, 0x30)
    if not blk:
        return {'ctx': ctx, 'desc': d}
    hdr = struct.unpack('<%dI' % (len(blk) // 4), blk)
    out = {'ctx': hex(ctx), 'desc': hex(d), 'hdr': [hex(x) for x in hdr],
           'scale_a': hdr[4], 'scale_b': hdr[5], 'ref_plane': hdr[7],
           'table_a': hdr[8], 'table_b': hdr[9], 'table_c': hdr[10],
           'arrays': {}}
    for tag, ptr in (('table_a', hdr[8]), ('table_b', hdr[9]),
                     ('table_c', hdr[10])):
        if 0x1000 < ptr < 0x4000000:
            arr = _xp(h, ptr, 0x40)
            out['arrays'][tag] = ([hex(x) for x in
                                   struct.unpack('<16I', arr)] if arr else None)
        else:
            out['arrays'][tag] = None
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--workdir', default='/tmp/opencode/golden')
    ap.add_argument('--floppy', help='default: <workdir>/boot_floppy.img')
    ap.add_argument('--hd', help='default: <workdir>/leo_hd.qcow2')
    ap.add_argument('--ddisk', help='default: <workdir>/leo_d.qcow2')
    ap.add_argument('--ram', type=int, default=32)
    ap.add_argument('--class-va', default='0x%x' % LION,
                    help='class table to look for, image VA')
    ap.add_argument('--names', default='/tmp/opencode/pr380m_pairs.tsv',
                    help='token->class-table map for the log (the PR-380 '
                         'map); missing file just means unnamed output')
    ap.add_argument('--max-seconds', type=int, default=900,
                    help='stop if no playable level was found by then')
    ap.add_argument('--require-class', action='store_true',
                    help='do not start the trace on a scene that lacks '
                         'class-va: a chapter flyover is a live scene with '
                         'KAMIEN/RYBA objects but no player (measured '
                         '2026-11-25); keep waiting until the player class '
                         'appears in the census instead of tracing the '
                         'flyover. Use with --class-va for the level '
                         "you're measuring.")
    ap.add_argument('--trace-seconds', type=int, default=90)
    ap.add_argument('--sample-hz', type=float, default=4.0)
    ap.add_argument('--census-every', type=float, default=None,
                    help='seconds between full RAM scans while waiting for a '
                         'level. Each one STOPS the guest, and the original '
                         'has been seen to die when it does, so the default '
                         'is -1 (never; the world anchor and the class-table '
                         'span are read with `xp` instead) when --show is '
                         'used, and 10 s otherwise')
    ap.add_argument('--sendkey-at', default='',
                    help='scripted input, same form as golden_qemu_run.py')
    ap.add_argument('--drive', default='right:12,rest:4,right:12,rest:8',
                    help='driving script during the trace: KEY:SECONDS, '
                         'comma list. "rest" sends nothing. "" = no input.')
    ap.add_argument('--no-esc', action='store_true',
                    help='do not send Esc into the opening movie')
    ap.add_argument('--no-keys', action='store_true',
                    help='read only: send no input at all (use with --show '
                         'or --attach, where a human drives the game)')
    ap.add_argument('--manual-menu', action='store_true',
                    help='do not auto-select when the chapter menu is '
                         'detected; wait for keys from --keys-from instead')
    ap.add_argument('--trace-all-level-objects', action='store_true',
                    help='trace every new class-pointer object in the loaded '
                         'level instead of choosing one class; useful when '
                         'the visible player class is not yet identified')
    ap.add_argument('--trace-object', action='append', default=[],
                    metavar='ADDR',
                    help='trace an explicitly identified object instead of '
                         'choosing one from the class census; repeatable. '
                         'Use this for a high-rate second pass once the '
                         'player object has been measured')
    ap.add_argument('--show', action='store_true',
                    help='run QEMU with its own window (gtk, or --display '
                         'sdl). On this desktop it goes black in the game\'s '
                         'VESA mode; use --live-frames plus '
                         'tools/golden_live_view.py instead')
    ap.add_argument('--accel', choices=('kvm', 'tcg'), default='kvm',
                    help='kvm (default): in-kernel PIT that DROPS ticks lost '
                         'while the monitor stalls the vCPU; under tcg the '
                         'replayed ticks crashed the original (DOS/4GW '
                         '"transfer stack overflow on interrupt 08h")')
    ap.add_argument('--icount-shift', type=int, default=None,
                    help='slow TCG with QEMU instruction counting; shift N '
                         'sets a 1e9/2**N instruction/s ceiling (6 is about '
                         '15.6 MIPS). Requires a fresh QEMU start')
    ap.add_argument('--display', default='gtk,gl=off',
                    help='QEMU display for --show. Plain "gtk" rendered a '
                         'black window on this Xwayland desktop; gl=off is '
                         'the first thing to try if the window stays black')
    ap.add_argument('--attach', action='store_true',
                    help='do not start QEMU: attach to the monitor socket of '
                         'a running rig (the one `make golden_play` starts) '
                         'and leave it running on exit')
    ap.add_argument('--keep-running', action='store_true',
                    help='leave QEMU up when the trace ends (--show implies '
                         'this: closing the window is the user\'s choice)')
    ap.add_argument('--live-frames', metavar='PPM',
                    help='keep writing the screen here for '
                         'tools/golden_live_view.py. Needed because QEMU\'s '
                         'own GTK window goes black on this desktop as soon '
                         'as the game sets its VESA mode, while the frame '
                         'the monitor captures is fine')
    ap.add_argument('--keys-from', metavar='FILE',
                    help='forward lines found in this file to the game as '
                         'sendkey; tools/golden_live_view.py writes there, '
                         'so a person drives while this tool reads RAM')
    ap.add_argument('--smoke', action='store_true',
                    help='stop after the image base and the object scan '
                         '(no menu driving, no trace)')
    ap.add_argument('--out', help='default: <workdir>/level_trace')
    a = ap.parse_args()
    if a.census_every is None:
        # A person driving the game must not have the guest stopped under
        # them; the class-span and anchor reads do not stop it, so no full
        # scan is needed unless someone asks for one.
        a.census_every = -1.0 if a.show else 10.0

    wd = a.workdir
    os.makedirs(wd, exist_ok=True)
    floppy = a.floppy or os.path.join(wd, 'boot_floppy.img')
    hd = a.hd or os.path.join(wd, 'leo_hd.qcow2')
    ddisk = a.ddisk if a.ddisk is not None else os.path.join(wd, 'leo_d.qcow2')
    for p in (floppy, hd) + ((ddisk,) if ddisk else ()):
        if not os.path.exists(p):
            raise SystemExit('missing rig image: %s (run '
                             'tools/golden_build_rig.sh with DDIR=1)' % p)
    out_prefix = a.out or os.path.join(wd, 'level_trace')
    mon = os.path.join(wd, 'mon.sock')
    if os.path.exists(mon) and not a.attach:
        os.unlink(mon)
    class_va = int(a.class_va, 0)

    if a.no_keys:                      # read-only: a human drives the game
        a.sendkey_at, a.drive, a.no_esc = '', '', True
    sched = []
    for item in a.sendkey_at.split(','):
        item = item.strip()
        if item:
            t, _, k = item.partition('=')
            sched.append((float(t), k))
    sched.sort()
    drive = []
    for item in a.drive.split(','):
        item = item.strip()
        if item:
            k, _, s = item.partition(':')
            drive.append((k, float(s)))

    log = open(out_prefix + '.qemu.log', 'wb')
    proc = None
    if not a.attach:
        proc = subprocess.Popen(
            qemu_argv(wd, floppy, hd, ddisk, a.ram,
                      a.display if a.show else 'none', a.accel,
                      a.icount_shift),
            stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
    names = load_names(a.names)
    man = {'inputs': {'floppy': {'path': floppy, 'sha256': sha256(floppy)},
                      'hd': {'path': hd, 'sha256': sha256(hd)},
                      'ddisk': ({'path': ddisk, 'sha256': sha256(ddisk)}
                                if ddisk else None)},
           'class_va': hex(class_va), 'phases': [], 'events': [],
           'transitions': [], 'contact_transitions': [], 'ask_answers': [],
           'summary': {}}

    def note(msg, **kw):
        el = time.time() - t0 if t0 else 0.0
        print('[t=%6.1fs] %s %s' % (el, msg,
                                    json.dumps(kw) if kw else ''), flush=True)
        man['events'].append(dict(t=round(el, 1), msg=msg, **kw))

    dump_path = os.path.join(wd, 'ram_dump.bin')
    trace = open(out_prefix + '.jsonl', 'w')
    t0 = None
    try:
        h = HMP(mon)
        t0 = time.time()
        key_pos = [0]

        def forward_keys():
            """Anything a person typed into tools/golden_live_view.py, in
            the order they typed it. The QMP socket serves one client, so
            the viewer writes to a file and this forwards it."""
            if not a.keys_from:
                return
            try:
                with open(a.keys_from) as f:
                    f.seek(key_pos[0])
                    new = f.read()
                    key_pos[0] = f.tell()
            except OSError:
                return
            for item in new.split():
                k, sep, hold = item.partition(':')
                try:
                    hold_ms = max(50, min(1500, int(hold))) if sep else 100
                except ValueError:
                    note('ignored malformed viewer key', item=item)
                    continue
                note('key from the viewer', key=k, hold_ms=hold_ms)
                h.cmd('sendkey %s %d' % (k, hold_ms))

        def live_frame():
            if a.live_frames:
                h.cmd('screendump ' + a.live_frames)
        # 1. wait for the game to be loaded, and measure the image base.
        # Through `xp` reads, which leave the guest running: the ORIGINAL is
        # a DOS/4GW program and has been seen to die ("Internal stack
        # overflow, System halted", 2026-11-24) when a full memory dump
        # stopped it. The known address is checked first - it is not
        # trusted, its two marker strings are read there - and only a miss
        # falls back to scanning the low megabytes.
        base = None
        blob, segs = None, []
        tries = 0
        while base is None:
            if proc is not None and proc.poll() is not None:
                raise SystemExit('qemu exited (rc %s)' % proc.returncode)
            base = image_base_live(h, EXPECT_BASE)
            how_found = 'markers read at the address measured before'
            if base is None:
                base = find_base_live(h)
                how_found = 'xp scan of the low megabytes'
            if base is None and a.census_every >= 0:
                blob, segs = ram_dump(h, dump_path)
                base = image_base(blob) if blob else None
                how_found = 'full memory dump'
            if base is None:
                if tries < 3 or time.time() - t0 > 180:
                    note('no image yet; text screen: %r'
                         % text_grid(h)[:120])
                tries += 1
                time.sleep(5)
        note('image base in guest RAM', base=hex(base),
             marks=[hex(v) for v, _ in MARKS], how=how_found)
        man['summary']['image_base'] = hex(base)

        objs = []
        # The live image's own class tables first: without that list no
        # "object" claim means anything.
        tables, how = class_tables_live(h, base)
        note('live class tables (PR-380 anchor rule)', tables=len(tables),
             anchors=how)
        man['summary'].update(class_tables=len(tables), anchors=how)
        if blob is None:
            hits = []
        else:
            hits = class_census(blob, base, tables)
        note('class words present before any level', hits=len(hits),
             classes=len(set(c for _, c in hits)),
             first=[(hex(x), hex(c)) for x, c in hits[:6]])
        man['summary']['class_words_at_boot'] = len(hits)
        if a.smoke:
            return
        # 2. attract: Esc into the opening movie, then the chapter menu.
        esc_t = None
        menu_t = None
        menu_streak = 0
        baseline = None
        census_next = 0.0
        world_note = None
        blob = None
        while True:
            t = time.time() - t0
            if t > a.max_seconds:
                raise SystemExit('no playable level within %d s' % a.max_seconds)
            while sched and t >= sched[0][0]:
                _, k = sched.pop(0)
                note('sendkey', key=k)
                h.cmd('sendkey ' + k)
            if not a.no_esc and esc_t is None and t > 25:
                esc_t = t
                note('sendkey esc (opening movie)', key='esc')
                h.cmd('sendkey esc')
            forward_keys()
            # geometry: the map menu is the only 400-line state (PLAN S6).
            # One screendump per pass, shared with the viewer: each one
            # stalls the vCPU, and two per pass was enough to help kill it.
            probe = a.live_frames or os.path.join(wd, 'probe.ppm')
            h.cmd('screendump ' + probe)
            size = ppm_size(probe)
            # A movie frame flashes through 640x400 for an instant; the map
            # menu holds it. Require the geometry to persist across several
            # reads before calling it the menu - one match is noise (measured
            # 2026-11-24: a false menu fired at t=38s on a single frame).
            menu_streak = (menu_streak + 1
                           if t >= MENU_EARLIEST_T and size == MENU_AT
                           else 0)
            if menu_streak >= 3 and menu_t is None:
                menu_t = t
                note('chapter map menu', at=round(t, 1),
                     why='640x400 held across %d reads after %.0f s'
                         % (menu_streak, MENU_EARLIEST_T))
                if not a.no_keys and not a.manual_menu:
                    # PLAN.md:283: Down alone selects and starts the
                    # highlighted chapter. Do not follow it with Enter:
                    # Enter is not part of the attract-menu launch path.
                    h.cmd('sendkey down')
                    note('sendkey', key='down')
                elif a.manual_menu:
                    note('waiting for chapter selection from live view')
                # Select only once; repeated Down events would walk chapters.
            # 3. level detector, through `xp` reads only. The world anchor at
            # 0xcef40 is one read and the class census covers the allocation
            # range above the image, and neither stops the guest - which
            # matters, because the original is a DOS/4GW program that has been
            # seen to die ("Internal stack overflow, System halted") when a
            # full memory dump stopped it.
            ws = world_state(h, base)
            due = time.time() >= census_next
            if not (due or (ws is not None and world_note is None)):
                # The attract-loop chapter card can be visible for about a
                # second. A 1 Hz screen probe can miss it entirely; poll at
                # 4 Hz so the sustained-geometry check can still confirm it
                # without treating one transition frame as a menu.
                time.sleep(0.25)
                continue
            # Without an explicit cadence, avoid rescanning the heap while
            # there is no world object. Once the world exists, check every
            # 2 s until its live objects appear; pmemsave does not stop the
            # vCPU, and this attached trace showed that a single scan at the
            # world boundary can be too early to find the object set.
            scan_period = a.census_every if a.census_every >= 0 else (
                2.0 if ws is not None else 300.0)
            census_next = time.time() + scan_period
            if ws is None:
                note('no world object at %s yet - falling back to the RAM '
                     'census' % hex(base + WORLD_ANCHOR))
            if ws is not None and world_note is None:
                world_note = ws
                note('world object exists (level boundary)', **ws)
            if a.trace_object and world_note:
                objs = [int(x, 0) for x in a.trace_object]
                note('tracing explicit object addresses',
                     objs=[hex(x) for x in objs],
                     why='identified in an earlier census pass')
                break
            hits = census_live(h, base, tables)
            if baseline is None and ws is None:
                baseline = {x for x, _ in hits}
                note('baseline before any level', pointers=len(hits),
                     classes=len(set(c for _, c in hits)),
                     why='what already exists at the menu; only class words '
                         'appearing later count as level objects')
                continue
            if baseline is None:
                baseline = set()
            new = [(x, c) for x, c in hits if x not in baseline]
            # PR-392: the player class is NOT new at level start - the menu
            # already holds a lion - so the baseline filter hides exactly the
            # object the ask fields matter for. Always re-look for the
            # requested class once the census runs.
            player = [(x, c) for x, c in hits if c == class_va]
            if player:
                new = player + [(x, c) for x, c in new if c != class_va]
            inv = collections.Counter(c for _, c in new)
            if a.require_class and inv and not inv.get(class_va):
                # PR-514: a chapter flyover/cinematic is itself a live
                # scene (KAMIEN boulders, RYBA fish at plane 0) and the
                # old first-world break traced exactly those. The player
                # class is the level boundary; wait for it.
                note('scene without the player class - still waiting',
                     classes=[(names.get(cv, '?'), hex(cv), n)
                              for cv, n in inv.most_common(6)],
                     want=names.get(class_va, '?'), want_va=hex(class_va))
                time.sleep(2)
                continue
            if inv:
                note('new objects in RAM (level start)', total=len(new),
                     classes=len(inv),
                     inventory=[(names.get(cv, '?'), hex(cv), n)
                                for cv, n in inv.most_common(12)])
                if a.trace_all_level_objects:
                    candidates = sorted({x for x, _ in new})
                    # Some support/contact records do not have plausible
                    # position fields. Keep them in this diagnostic pass:
                    # PR-354's B6 standing bit may belong to the contacted
                    # element rather than the visible TLewZaria object.
                    objs = candidates[:128]
                    note('tracing all level class-pointer candidates',
                         count_candidates=len(candidates), count=len(objs),
                         objs=[hex(x) for x in objs],
                         includes_out_of_range_positions=True)
                    break
                want = class_va if inv.get(class_va) else inv.most_common(1)[0][0]
                cand = [x for x, cv in new if cv == want]
                others = [x for x, cv in new if cv != want]
                seen = small_reads(h, cand[:24])
                live = [o for o, v in seen.items()
                        if abs(v['x']) < 0x10000 and abs(v['y']) < 0x20000
                        and 0 <= v['plane'] < 16]
                if not live:
                    live = cand[:8]
                    note('no candidate passed the position sanity check, '
                         'tracing them anyway', count=len(live))
                # a few neighbours too: the landing answer needs the object
                # the lion stands on, not only the lion.
                live = (live + others)[:12]
                note('tracing class', cls=names.get(want, '?'),
                     va=hex(want), objs=[hex(x) for x in live[:8]],
                     first=small_reads(h, live[:1]).get(live[0])
                     if live else None)
                objs = live
                break
            time.sleep(3)
        if a.smoke:
            return
        man['summary']['objects'] = [hex(x) for x in objs]
        man['summary']['level_descriptor'] = descriptor(h, base)
        surface_array = (int(world_note['surface_array'], 16)
                         if world_note and world_note.get('surface_array')
                         else None)

        # 4. the trace.
        period = 1.0 / a.sample_hz
        # A hand-driven session keeps recording until you close the window
        # (or --max-seconds passes); a scripted one traces its fixed window.
        deadline = time.time() + (a.max_seconds if a.show else a.trace_seconds)
        di, dt_next = 0, 0.0
        prev = None
        frame_next = tick_next = 0.0
        tick = None
        while time.time() < deadline:
            if proc is not None and proc.poll() is not None:
                note('qemu window closed; trace ends here')
                break
            t = time.time() - t0
            forward_keys()
            if t >= frame_next:
                live_frame()
                frame_next = t + 1.0
            while sched and t >= sched[0][0]:
                _, k = sched.pop(0)
                note('sendkey', key=k)
                h.cmd('sendkey ' + k)
            # driving script: press (repeat) or rest
            if drive and di < len(drive):
                key, dur = drive[di]
                dt_next += period
                if dt_next >= dur:
                    di += 1
                    dt_next = 0.0
                elif key != 'rest':
                    h.cmd('sendkey ' + key)
            if t >= tick_next:
                tick = read_tick(h)
                tick_next = t + 1.0
            got = batch_reads(h, objs, RAMWIN)
            for o, w in got.items():
                rec = {'t': round(t, 2), 'tick': tick, 'obj': hex(o), **w}
                trace.write(json.dumps(rec) + '\n')
                if prev and o in prev:
                    p = prev[o]
                    # The PR-345/PR-346 ask writes the current surface
                    # answer into [obj+0xa9] (0x5478e, 0x547bc).  That is
                    # the live ground/contact handle; watch it independently
                    # of PR-354's scripted standing bit.
                    ph, ch = p['surface_handle'], w['surface_handle']
                    if ph != ch:
                        event = {
                            't': round(t, 2), 'obj': hex(o),
                            'surface': '%s->%s' % (hex(ph & 0xFFFFFFFF),
                                                   hex(ch & 0xFFFFFFFF)),
                            'dx': w['x'] - p['x'], 'dy': w['y'] - p['y'],
                            'before_entry': surface_entry(surface_array, ph),
                            'after_entry': surface_entry(surface_array, ch),
                            'before': p, 'after': w}
                        man['contact_transitions'].append(event)
                        note('surface handle changed', obj=hex(o),
                             surface=event['surface'],
                             dx=event['dx'], dy=event['dy'],
                             before=event['before_entry'],
                             after=event['after_entry'])
                    # PR-354: 0x54f40 sets [obj+0xb6] bit 0x01 (mask 0x01)
                    # right after running the element's LAND method.
                    pb, cb = p['flags_b6'] & 1, w['flags_b6'] & 1
                    if pb != cb:
                        event = {
                            't': round(t, 2), 'obj': hex(o),
                            'standing': '%d->%d' % (pb, cb),
                            'dx': w['x'] - p['x'], 'dy': w['y'] - p['y'],
                            'surface_before': surface_entry(surface_array,
                                                           ph),
                            'surface_after': surface_entry(surface_array, ch),
                            'before': p, 'after': w}
                        man['transitions'].append(event)
                        note('standing bit flipped', obj=hex(o),
                             standing=event['standing'],
                             dx=event['dx'], dy=event['dy'])
                    # PR-392: 0x45400 keeps the ask's answer - the winning
                    # ground container at [obj+0xe6], the interpolated
                    # clearance at [obj+0xea], and the separator flag byte
                    # of the record it chose at [obj+0xe2]. Watching that
                    # triple is how the mode/flag gate at 0x45417 and the
                    # branches at 0x54766/0x547c8 get tested on live data.
                    pa, ca = p.get('answer_surface'), w['answer_surface']
                    if pa != ca:
                        event = {
                            't': round(t, 2), 'obj': hex(o),
                            'answer': '%s->%s' % (hex(pa & 0xFFFFFFFF),
                                                  hex(ca & 0xFFFFFFFF)),
                            'gap': w['answer_gap'],
                            'answer_flag': w['answer_flag'],
                            'held': surface_entry(surface_array,
                                                 w['surface_handle']),
                            'entry': surface_entry(surface_array, ca),
                            'before': p, 'after': w}
                        man['ask_answers'].append(event)
                        note('ask answer changed', obj=hex(o),
                             answer=event['answer'], gap=event['gap'],
                             answer_flag=event['answer_flag'],
                             entry=event['entry'])
            if got:
                prev = got
            trace.flush()
            time.sleep(period)
    finally:
        trace.close()
        mon_cli = locals().get('h')
        if mon_cli is not None:
            try:
                mon_cli.cmd('info status')
            except Exception:
                pass
        if proc is not None and not (a.keep_running or a.show):
            proc.terminate()
            try:
                proc.wait(timeout=15)
            except subprocess.TimeoutExpired:
                proc.kill()
        with open(out_prefix + '.json', 'w') as f:
            json.dump(man, f, indent=1, sort_keys=True)

    print('\n== OI-129 reading ==', flush=True)
    print('surface-handle transitions seen: %d'
          % len(man['contact_transitions']))
    for x in man['contact_transitions']:
        print('  %s obj %s: %s dx=%d dy=%d (before x=%d y=%d, '
              'after x=%d y=%d)'
              % (x['t'], x['obj'], x['surface'], x['dx'], x['dy'],
                 x['before']['x'], x['before']['y'], x['after']['x'],
                 x['after']['y']))
    tr = man['transitions']
    print('standing-bit transitions seen: %d' % len(tr))
    for x in tr:
        print('  %s obj %s: %s dx=%d dy=%d (before x=%d y=%d, after x=%d y=%d)'
              % (x['t'], x['obj'], x['standing'], x['dx'], x['dy'],
                 x['before']['x'], x['before']['y'], x['after']['x'],
                 x['after']['y']))
    aa = man['ask_answers']
    print('ask-answer changes seen: %d' % len(aa))
    for x in aa:
        print('  %s obj %s: %s gap=%d answer_flag=%d element=%s'
              % (x['t'], x['obj'], x['answer'], x['gap'], x['answer_flag'],
                 (x['entry'] or {}).get('element_index')))
    print('trace: %s.jsonl   manifest: %s.json' % (out_prefix, out_prefix))


if __name__ == '__main__':
    main()

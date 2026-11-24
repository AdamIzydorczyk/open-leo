#!/usr/bin/env python3
"""golden_surface_read.py - read the ORIGINAL's live ground-surface table out
of a running level and compare it, vertex by vertex, with the authored `.PLT`
text that produced it (the F6/OI-128 thread; continues PR-390).

What the image says the structure is, read again for this tool with the
correct `leo.img` addressing (offset = VA - 0x10000 for BOTH objects, the
image base is 0x10000; `VA - 0xf000` is the raw LEO.EXE file's text rule
only, and using it for data strings gives nonsense - see PR-391):

  0x4dc00 (eax = the world object, edx = the level's `.PLT` text):
      0x4dc30 sscanf(text, "%d", [obj+0x80])      - the file's header count
      0x4dc55 allocates count*12 + 4 bytes, 12 bytes per element
      0x4dc7f stores the array at [obj+0x74]
      0x4dc88 loop, 12 bytes a step: `call [element_class + 0x20]` with
              edx = the running text pointer          - one line per element
  0x45120 (eax = one 12-byte element, edx = the text):
      0x45136 sscanf(text, "%d", [element+8])     - the line's vertex count
      0x45149 allocates (count+1)*13 + 4 bytes in 13-byte records whose
              element class base is image 0xa0c64, stores it at [element+4]
      0x4a0b0 reads one `x,y` pair into a record (`%d,%d` at image 0x91d0f,
              into record+0x0 and record+0x4)
      0x45130/0x4519d scans the one-character separator after each pair:
              ';' writes flag 1, '#' writes flag 2, anything else 0, at
              that record's +0xc. The LAST record of a line therefore
              carries no flag byte from the parser: the live read of
              ZARIA.01 shows the byte of record i belongs to the separator
              after point i, and the last record of all 45 lines holds an
              unmotivated byte (136, 50, 102, 87, ...)
      0x451e2 then consumes the terminating ';' (0x498f0, image 0x91834)

So the class word the elements carry is the method array at image 0xa0cb4
read at base +0xc (that base's +0x20 IS 0x45120), and a record's own class
word is that array at its own base. Both were read at 0xa0cb4/0xa0cc0 in
the image and this tool prints what the live objects actually hold.

Which file feeds which field is decided by the level-description parser
itself: it matches the keys "platformy"/"blokady"/"sufity" (literals at
0x91e1e/0x91e2c/0x91e38, compared by 0x76d20) and keeps the names at
[ebp+0x7a]/[ebp+0x8a]/[ebp+0x9a]; the same function then opens them with
0x4a910 and calls 0x4dc00 (ground table, [world+0x74]/[world+0x80]),
0x4db50 (20-byte elements, [world+0x70]/[world+0x7c]) and 0x4da90 (header
into [world+0x84]). So reading [world+0x70..0x84] next to the ground table
says which level is running without asking the game - the ZAR.BLK header
count 6 appears at [world+0x7c].

Nothing here is a fixture or a port change: it is the original's own RAM.

Usage:
  python3 tools/golden_surface_read.py [--mon PATH] [--out OUT.json]
                                       [--plt PLT.txt] [--base ADDR]
                                       [--wait SECONDS]
  --plt compares against a `.PLT` text dumped with
  `tools/re/llrs_cat1 ETAPY <LEVEL> <NAME>.PLT`. Run it against the
  PR-390 rig while a level is loaded; the world object exists a few
  seconds before the ground table does, so --wait covers that gap.
"""

import argparse
import json
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from golden_qemu_run import HMP
import golden_level_trace as T

ENTRY_SIZE = 12                  # 0x4dc55: count*12 + 4
RECORD_SIZE = 13                 # 0x45149: (count+1)*13 + 4
SCRATCH = '/tmp/opencode/golden/pr391_surface_window.bin'


def world_window(h, anchor):
    """The world object's own words around the table fields. 0x4dc00 writes
    the ground table at [obj+0x74] and its count at [obj+0x80]; its two
    sibling loaders write [obj+0x70]/[obj+0x7c] (0x4db50) and a third pair
    (0x4da90). Printing the span is how a sibling is recognised - an authored
    header count (ZAR.BLK says 6) shows up here."""
    blob = T.ram_window(h, anchor + 0x60, 0x50, SCRATCH + '.world')
    if blob is None:
        return {}
    return {0x60 + 4 * i: struct.unpack_from('<I', blob, 4 * i)[0]
            for i in range(20)}


def read_elements(h, array, count, out_dir=SCRATCH):
    """Every element of the live table, then every record of every element.
    Read with `pmemsave` windows, one per element, so the guest is never
    stopped (PR-390's rule)."""
    els = []
    blob = T.ram_window(h, array, ENTRY_SIZE * count, out_dir + '.array')
    if blob is None or len(blob) < ENTRY_SIZE * count:
        return None
    for i in range(count):
        cls, recs, cnt = struct.unpack_from('<III', blob, ENTRY_SIZE * i)
        e = {'index': i, 'class': hex(cls), 'records': hex(recs),
             'count': cnt}
        els.append(e)
        if not recs or not cnt or cnt > 4096:
            continue
        # (count+1) records plus the +4 the allocator adds.
        need = RECORD_SIZE * (cnt + 1) + 4
        win = T.ram_window(h, recs, need, out_dir + '.%d' % i)
        if win is None or len(win) < need:
            e['error'] = 'short read at %s' % hex(recs)
            continue
        e['records_read'] = []
        for r in range(cnt + 1):
            o = RECORD_SIZE * r
            x, y, rcls = struct.unpack_from('<iiI', win, o)
            e['records_read'].append({'x': x, 'y': y, 'class': hex(rcls),
                                      'flag': win[o + 12]})
    return els


def parse_plt(text):
    """The authored `.PLT` text under the grammar 0x45120 reads: a header
    count, then per line a vertex count and that many+1 `x,y` pairs with one
    separator character after each pair. Returns [(count, [(x, y, sep), ...])].
    sep is the literal character that followed the pair; the engine's flag is
    1 for ';', 2 for '#', 0 otherwise, and it lands on the NEXT record."""
    toks = text.replace('\r', '').replace('\n', ' ')
    i, n = 0, len(toks)

    def skip():
        nonlocal i
        while i < n and toks[i] in ' \t':
            i += 1
        return i

    def num():
        nonlocal i
        i = skip()
        j = i
        if j < n and toks[j] in '+-':
            j += 1
        while j < n and toks[j].isdigit():
            j += 1
        if j == i:
            raise ValueError('number expected at %d: %r' % (i, toks[i:i + 20]))
        v = int(toks[i:j])
        i = j
        return v

    hdr = num()
    lines = []
    for _ in range(hdr):
        cnt = num()
        pts = []
        for k in range(cnt + 1):
            x = num()
            i = skip()
            if i < n and toks[i] == ',':
                i += 1
            y = num()
            i = skip()
            sep = toks[i] if i < n else ''
            i += 1
            pts.append((x, y, sep))
        lines.append((cnt, pts))
    return hdr, lines


def flags_as_engine(pts):
    """The flag bytes 0x45120 writes for one authored line. Record i carries
    the byte for the separator that FOLLOWS point i - 1 for ';', 2 for '#',
    anything else 0 - and the LAST record's flag byte is never written: the
    live read of ZARIA.01 (PR-391) shows record 0 of a ';' line carrying 1
    and record 0 of a '!' line carrying 0, while the last record of every
    line carries an unmotivated byte (136, 50, 102, 87, ...)."""
    out = []
    for k in range(len(pts) - 1):
        sep = pts[k][2]
        out.append(1 if sep == ';' else 2 if sep == '#' else 0)
    out.append(None)
    return out


def compare(els, lines):
    """Element-by-element diff of the live table against the authored text."""
    rep = {'elements_live': len(els), 'elements_authored': len(lines),
           'exact': 0, 'diff': []}
    for i, e in enumerate(els):
        if i >= len(lines):
            rep['diff'].append({'index': i, 'why': 'no authored line'})
            continue
        cnt, pts = lines[i]
        live = e.get('records_read') or []
        why = []
        if cnt != e['count']:
            why.append('count %d != authored %d' % (e['count'], cnt))
        if len(live) != len(pts):
            why.append('records %d != authored %d' % (len(live), len(pts)))
        for k, p in enumerate(pts):
            if k >= len(live):
                break
            if (live[k]['x'], live[k]['y']) != (p[0], p[1]):
                why.append('vertex %d live (%d,%d) != authored (%d,%d)'
                           % (k, live[k]['x'], live[k]['y'], p[0], p[1]))
            want = flags_as_engine(pts)[k]
            if want is not None and live[k]['flag'] != want:
                why.append('flag %d live %d != authored %d'
                           % (k, live[k]['flag'], want))
        if why:
            rep['diff'].append({'index': i, 'why': why})
        else:
            rep['exact'] += 1
    return rep


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--mon', default=T.MON if hasattr(T, 'MON') else
                    '/tmp/opencode/golden/mon.sock')
    ap.add_argument('--out', default='/tmp/opencode/pr391_surface.json')
    ap.add_argument('--plt', help='authored .PLT text to compare against')
    ap.add_argument('--base', type=lambda s: int(s, 0), default=None)
    ap.add_argument('--wait', type=int, default=300,
                    help='seconds to wait for the level ground table')
    a = ap.parse_args()

    h = HMP(a.mon)
    if a.base is not None:
        blob = T.ram_window(h, a.base + T.MARKS[0][0], 0x100, SCRATCH + '.m')
        if not blob or all(m not in blob for _, m in T.MARKS):
            raise SystemExit('no image markers at base %s' % hex(a.base))
        base = a.base
    else:
        # The markers are 0x95c20/0x95c40 ABOVE the base, so the candidate is
        # checked by reading the strings at base+mark (`image_base_live`), not
        # by scanning a window that starts at the candidate.
        base = T.image_base_live(h, T.EXPECT_BASE)
        if base is None:
            base = T.find_base_live(h)
    if base is None:
        raise SystemExit('the image base could not be measured')
    w = T.world_state(h, base)
    waited = 0.0
    while (not w or not w.get('surface_array') or
           w.get('surface_array') == '0x0') and waited < a.wait:
        # The world object appears at the level boundary BEFORE the ground
        # table is loaded: measured at t=304.5s the anchor existed with
        # surface_array 0, and the level's objects appeared at t=313.8s.
        time.sleep(5)
        waited += 5
        w = T.world_state(h, base)
    if not w or not w.get('surface_array') or w['surface_array'] == '0x0':
        raise SystemExit('no surface table: %r' % w)
    array = int(w['surface_array'], 16)
    count = int(w['surface_count'], 16)
    print('image base %s, world %s, table %s count %d'
          % (hex(base), w['anchor'], hex(array), count))
    els = read_elements(h, array, count)
    if els is None:
        raise SystemExit('the table window could not be read')
    span = world_window(h, int(w['anchor'], 16))

    rep = {'base': hex(base), 'world': w, 'world_span':
           {hex(k): hex(v) for k, v in span.items()},
           'array': hex(array),
           'count': count, 'elements': els}
    if a.plt:
        hdr, lines = parse_plt(open(a.plt, 'rb').read().decode('latin1'))
        rep['plt_file'] = a.plt
        rep['plt_header'] = hdr
        rep['compare'] = compare(els, lines)
    with open(a.out, 'w') as f:
        json.dump(rep, f)

    vtotal = flags = 0
    byflag = {}
    for e in els:
        rr = e.get('records_read') or []
        vtotal += len(rr)
        for r in rr:
            byflag[r['flag']] = byflag.get(r['flag'], 0) + 1
        flags += sum(1 for r in rr if r['flag'])
        first = rr[0] if rr else None
        print('elem %2d cnt=%-3d recs=%-10s cls=%s first=%s last=%s' % (
            e['index'], e['count'], e['records'], e['class'],
            ('%d,%d f%d' % (first['x'], first['y'], first['flag']))
            if first else '-',
            ('%d,%d f%d' % (rr[-1]['x'], rr[-1]['y'], rr[-1]['flag']))
            if rr else '-'))
    print('elements %d, records %d, flag bytes nonzero %d, by flag %s'
          % (len(els), vtotal, flags, sorted(byflag.items())))
    rcls = sorted({r['class'] for e in els for r in (e['records_read'] or [])})
    print('record class words: %s' % rcls)
    print('world words: %s' % sorted((hex(k), hex(v)) for k, v in span.items()
                                     if v))
    if 'compare' in rep:
        c = rep['compare']
        print('authored header %d, authored lines %d, exact %d, differing %d'
              % (rep['plt_header'], c['elements_authored'], c['exact'],
                 len(c['diff'])))
        for d in c['diff'][:10]:
            print('  elem %s: %s' % (d['index'], d['why']))


if __name__ == '__main__':
    main()

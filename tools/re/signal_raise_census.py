#!/usr/bin/env python3
"""Who raises which engine signal: a candidate census over the whole .text sweep.

The engine keeps a signal bitset of three dwords (PR-88: the name
comparator at 0x48000 maps a name to bit N, the name block at
0x919ec..0x91c68 is the table, AC_EV_MAX = 68 = the table's length). Bit N
therefore lives at byte N/8 of the set, bit N%8 - so a raiser is one of

    or  BYTE PTR [reg+DISP],mask      one byte of a set
    or  DWORD PTR [reg+DISP],mask     a whole dword of a set

and the set is reached in exactly two shapes:

    DISP = 0..0xb    an answer set: the 12-byte buffer an ask verb fills
                     (0x749a0 builds it with ebx=0xc, 0x49580 ORs it into
                     the asker's live set - PR-312/PR-313);
    DISP = 0x60..0x6b the object's own live set at [obj+0x60].

SCOPE, and what it is not: this is a CANDIDATE census over immediate-mask
OR writes only. The shape test cannot know that a given `[ecx+0x8]` is a
class pointer rather than an answer set, so every printed site is listed
with its enclosing function and must be read before it is quoted as a
raiser; and writes whose source is a register - which is how the engine
merges a whole set (the three-dword `or [eax],edx / or [eax+4],ecx /
or [eax+8],ebx` at 0x49580, PR-313, and 0x749a0) - are out of scope by
design. The signal names come from this port's own LL_SIG_* enum in
ported/ll_play.h, which PR-88 verified 68/68 against the engine's block,
so the census cannot drift from the port's table.

usage: signal_raise_census.py [SIGNAL ...]   (default: every candidate site)
"""
import re
import sqlite3
import sys

HDR = "/home/aizy/Projects/LL_RECOMPILE/ported/ll_play.h"
DB = "/tmp/opencode/leo.sqlite"

# name -> bit, from the port's enum (the same table the engine's names index)
src = open(HDR).read()
block = src[src.index("enum {\n    LL_SIG_AC_BLOK = 0,"):
            src.index("LL_SIG_AC_EV_MAX")]
bit_of, name_of, pos = {}, {}, 0
for nm, val in re.findall(r"LL_SIG_([A-Z0-9_]+)(?:\s*=\s*(\d+))?", block):
    pos = int(val) if val else pos
    bit_of[nm] = pos
    name_of[pos] = nm
    pos += 1
assert max(name_of) == 67, name_of       # AC_EV_MAX - 1 = 67

BYTE_RE = re.compile(r"^BYTE PTR \[([a-z0-9]+)(?:\+0x([0-9a-f]+))?\],0x([0-9a-f]+)$")
DWORD_RE = re.compile(r"^DWORD PTR \[([a-z0-9]+)(?:\+0x([0-9a-f]+))?\],0x([0-9a-f]+)$")


def shape(disp):
    """Which set a displacement addresses, or None if it is not a set."""
    if 0 <= disp <= 0xb:
        return "answer"
    if 0x60 <= disp <= 0x6b:
        return "live"
    return None


d = sqlite3.connect(DB)
rows = []
for va, mnem, ops in d.execute(
        "select va, mnem, ops from insn where mnem='or' and ops like 'BYTE"
        " PTR [%' union all select va, mnem, ops from insn where mnem='or'"
        " and ops like 'DWORD PTR [%' order by va"):
    ops = ops.strip()
    m = BYTE_RE.match(ops)
    if m:
        disp = int(m.group(2) or "0", 16)
        mask, wide = int(m.group(3), 16), 8
    else:
        m = DWORD_RE.match(ops)
        if not m:
            continue
        disp = int(m.group(2) or "0", 16)
        mask, wide = int(m.group(3), 16), 32
    kind = shape(disp)
    if kind is None:
        continue
    bits = [name_of.get(disp * 8 + b, "bit%d" % (disp * 8 + b))
            for b in range(wide) if mask & (1 << b)]
    if not bits:
        continue
    owner = d.execute("select name from func where va<=? order by va desc "
                      "limit 1", (va,)).fetchone()
    rows.append((va, ops, kind, bits, owner[0] if owner else "?"))

want = set(a.upper() for a in sys.argv[1:])
per = {}
for va, ops, kind, bits, owner in rows:
    if want and not (set(bits) & want):
        continue
    if not want:
        per[tuple(bits)] = per.get(tuple(bits), 0) + 1
    print("  %05x  %-34s %-7s %-16s raises %s" % (
        va, ops, kind, owner, ",".join(bits)))
if not want:
    print("== candidate sites per signal group")
    for bits, n in sorted(per.items(), key=lambda kv: -kv[1]):
        print("  %-58s %d" % (",".join(bits), n))
print("total candidate sites:", len(rows))

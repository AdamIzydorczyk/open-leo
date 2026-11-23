#!/usr/bin/env python3
"""command_post_census.py - who posts a command to an object, and which one.

The engine's object command bus has two ends, both read in PR-319:

  the POSTER 0x52b10  eax = the object, edx = the command opcode,
                      ebx, ecx = the two payload ints. It builds a
                      4-word message - [msg+0x4] = the opcode,
                      [msg+0x8] = the first int, [msg+0xc] = the
                      second - and calls the object's class +0x10 verb;
  the BASE HANDLER 0x54560 - the +0x10 verb most object classes
                      inherit, which switches on [msg+0x4].

This script is a census of the poster end: for every `call 0x52b10` in
the image it prints the opcode constant the code loads into edx for
that call, and the enclosing function. HEURISTIC, stated plainly: the
opcode is taken as the LAST `mov edx, ...` within the 48 bytes before
the call, which is what the compiler emits for this call shape; a site
whose opcode is computed rather than constant is printed as the
register, not guessed at. Where two `mov edx` appear in that window the
later one wins - the run over the image found 66 sites and no site
where that heuristic is contradicted by the disassembly of the sites
quoted in PR-319's record.

usage: command_post_census.py [DB]      (default /tmp/opencode/leo.sqlite)
"""
import collections
import sqlite3
import sys

DB = sys.argv[1] if len(sys.argv) > 1 else "/tmp/opencode/leo.sqlite"
POSTER = 0x52b10

d = sqlite3.connect(DB)
sites = [r[0] for r in d.execute(
    "select va from insn where mnem='call' and target=? order by va",
    (POSTER,))]
if not sites:
    sys.exit("%s: no call sites of 0x%x - build the index first "
             "(tools/re/leodb.py build)" % (DB, POSTER))

per = collections.Counter()
rows = []
for s in sites:
    op = None
    for _va, mn, ops in d.execute(
            "select va,mnem,ops from insn where va<? and va>=? order by va",
            (s, s - 48)):
        if mn == "mov" and ops.startswith("edx,"):
            op = ops[4:]
    owner = d.execute("select name from func where va<=? order by va desc "
                      "limit 1", (s,)).fetchone()
    rows.append((s, op, owner[0] if owner else "?"))
    per[op] += 1

print("call sites of the poster 0x%x: %d" % (POSTER, len(sites)))
print("== opcodes (edx at the call)")
for op, n in sorted(per.items(), key=lambda kv: (kv[0] is None,
                                                int(kv[0], 16)
                                                if kv[0] and
                                                kv[0].startswith("0x")
                                                else -1)):
    print("  edx=%-10s %d" % (op, n))
print("== sites")
for s, op, owner in rows:
    print("  %05x  edx=%-10s in %s" % (s, op, owner))

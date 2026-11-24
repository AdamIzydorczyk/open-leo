#!/usr/bin/env python3
"""jq.py - thin query CLI over build/journal.db. PROTOTYPE (not committed).

WHAT THIS IS. A step-by-step search tool for the ledger. It turns the
5 query shapes re_db.py stores into one line per hit:

    PR-354 [read]  NEXT_STEPS.md:48942-49146  title...

so the agent feeds the `file:start-end` straight to Read instead of
loading or grepping the whole 49k-line journal. It is a CLIENT of the
generated DB, nothing more: it adds no vocabulary and no parsing - the
parsing lives once in doc_census / re_index (see re_db.py).

STALE BY DESIGN? NO. If NEXT_STEPS.md is newer than the DB (or the DB
is missing), it rebuilds first, by calling re_db in-process. An answer
can never come from an out-of-date index - the same drift guarantee
that makes `make verify-index` cmp the markdown index.

usage:
  python3 tools/jq.py <verb> <arg> [flags]   |  python3 tools/jq.py <arg>
  verbs:
    addr  <0xNNNN>     records citing an engine address
    field <[reg+0xNN]> records citing a field token
    name  <A_*/AC_*/K_*/word=> records naming an authored token
    free  "<phrase>"   ranked full-text (FTS5; AND/OR and "quoted" ok)
    oi    <OI-nnn>     records referencing an open item: prose mentions
           (body text) plus structured OPENS/CLOSES; OI-128 and 128 both
           resolve to the same row
    corr  <PR-nn>      correction edges in and out of a record
    show  <PR-nn>      one record: kind, subsystem, Read range, corrects
    all   <PR-nn>      like show plus the record's own field block
  A bare arg auto-routes: 0x(4-6hex)->addr; short 0x..->free, because a
  field offset is not an address key (doc_census.ADDR_RE needs 4-6 hex);
  [..]->field, OI-..->oi, PR-..->show, otherwise ->free.
  flags: --kind read|port|gate|docs   --unfixed (drop superseded)
         --limit N (default 15)        --snippet   --json   --rebuild
  Run from the repository root (like the other tools/ scripts).
"""
import datetime
import json
import os
import re
import sqlite3
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import doc_census as dc  # noqa: E402  (for the PR-/OI- shape regexes)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = 'NEXT_STEPS.md'
DB = 'build/journal.db'


def ensure_db(rebuild):
    """(Re)build the DB in-process when it is missing or older than the
    ledger. Keeps this tool from ever answering from a stale index."""
    try:
        fresh = (os.path.getmtime(DB) >= max(
            os.path.getmtime(SRC),
            os.path.getmtime(os.path.join(ROOT, 'tools', 're_db.py'))))
    except OSError:
        fresh = False
    if rebuild or not fresh:
        import re_db
        sys.stderr.write('jq: ledger newer than index, rebuilding...\n')
        re_db.main(['re_db', DB])


def out_pr(pr, kind, start, end, title, file=SRC):
    """The one line an agent pastes into Read: PR + kind + file:range +
    title (truncated so the range stays visible)."""
    return 'PR-%-4d [%-4s]  %s:%d-%d  %s' % (
        pr, kind or '-', file, start, end, (title or '')[:66])


def emit(rows, args, header, snippets=None):
    if args.json:
        json.dump(rows, sys.stdout, ensure_ascii=False)
        sys.stdout.write('\n')
        return
    sys.stdout.write('# %s\n' % header)
    for i, r in enumerate(rows):
        sys.stdout.write(out_pr(*r[:5]) + '\n')
        if args.snippet and snippets and snippets[i]:
            sys.stdout.write('    … %s\n' % snippets[i].replace('\n', ' '))
    sys.stdout.write('# %d records\n' % len(rows))


# ---- verbs ---------------------------------------------------------------

def v_addr(db, arg, a):
    cond = ['a.addr = ?']
    p = [arg.lower()]
    if a.kind:
        cond.append('r.kind = ?')
        p.append(a.kind)
    if a.unfixed:
        cond.append("('PR-'||r.pr) NOT IN (SELECT corrects FROM corrections)")
    q = ("SELECT r.pr,r.kind,r.start_line,r.end_line,r.title "
         "FROM addr_mentions a JOIN records r ON r.rid=a.rid "
         "WHERE %s GROUP BY r.pr ORDER BY r.pr DESC LIMIT ?" % ' AND '.join(cond))
    rows = db.execute(q, p + [a.limit]).fetchall()
    tot = db.execute("SELECT COUNT(DISTINCT pr) FROM addr_mentions "
                     "WHERE addr=?", (arg.lower(),)).fetchone()[0]
    emit(rows, a, 'addr %s - %d records cite it (showing %d)' % (
        arg.lower(), tot, len(rows)))
    if tot == 0 and re.fullmatch(r'(?i)0x[0-9a-f]{1,3}', arg):
        sys.stdout.write('# hint: %s is a field OFFSET, not an engine '
                         'address (doc_census.ADDR_RE needs 4-6 hex). '
                         'Use: jq free "+0xNN" (substring) or jq field '
                         '"[reg+0xNN]".\n' % arg)


def _simple_mentions(db, table, col, arg, a, label):
    q = ("SELECT r.pr,r.kind,r.start_line,r.end_line,r.title "
         "FROM %s m JOIN records r ON r.rid=m.rid "
         "WHERE m.%s = ? GROUP BY r.pr ORDER BY r.pr DESC LIMIT ?"
         % (table, col))
    rows = db.execute(q, (arg, a.limit)).fetchall()
    emit(rows, a, '%s %s - %d records' % (label, arg, len(rows)))


def v_field(db, arg, a):
    _simple_mentions(db, 'field_mentions', 'field', arg, a, 'field')


def v_name(db, arg, a):
    _simple_mentions(db, 'name_mentions', 'name', arg, a, 'name')


def v_free(db, arg, a):
    def run(match):
        return db.execute(
            "SELECT r.pr,r.kind,r.start_line,r.end_line,r.title, "
            "snippet(record_fts,1,'','',' … ',12) FROM record_fts f "
            "JOIN records r ON r.rid=f.rowid WHERE record_fts MATCH ? "
            "ORDER BY rank LIMIT ?", (match, a.limit)).fetchall()
    try:
        rows = run(arg)
    except sqlite3.OperationalError:
        rows = run('"%s"' % arg.replace('"', ' '))   # fall back to a phrase
    snips = [r[5] for r in rows]
    rows = [r[:5] for r in rows]
    emit(rows, a, 'free-text %r - %d records (ranked)' % (arg, len(rows)),
         snips)


def v_oi(db, arg, a):
    d = re.sub(r'\D', '', arg)
    if not d:
        sys.stdout.write('# no open item id in %r\n' % arg)
        return
    # PR-358: oi_* store the numeric id; match by integer so OI-128,
    # OI-128 and 128 all resolve (the old exact-string compare missed the
    # 'OI-' prefix entirely), and read BOTH sources: the structured
    # OPENS/CLOSES graph AND the prose mentions (oi_mentions).
    struct = set()
    st = {}
    for pr, kind, sl, el, ti, verbs in db.execute(
            "SELECT r.pr,r.kind,r.start_line,r.end_line,r.title,"
            "group_concat(o.verb,',') FROM oi_refs o JOIN records r "
            "ON r.rid=o.rid WHERE CAST(o.oi AS INTEGER)=? GROUP BY r.pr",
            (int(d),)):
        struct.add(pr)
        st[pr] = (pr, kind, sl, el, ti + '  <%s>' % verbs)
    prose = set()
    for pr, kind, sl, el, ti in db.execute(
            "SELECT r.pr,r.kind,r.start_line,r.end_line,r.title "
            "FROM oi_mentions m JOIN records r ON r.rid=m.rid "
            "WHERE CAST(m.oi AS INTEGER)=? GROUP BY r.pr", (int(d),)):
        prose.add(pr)
        st.setdefault(pr, (pr, kind, sl, el, ti + '  <mention>'))
    rows = [st[p] for p in sorted(st, reverse=True)][:a.limit]
    emit(rows, a, '%s -> %d record(s) reference it (%d in OPENS/CLOSES; '
         '<opens/closes>=structured, <mention>=prose)' % (
             arg, len(st), len(struct)))


def v_corr(db, arg, a):
    pr = re.sub(r'\D', '', arg)
    up = db.execute("SELECT corrects FROM corrections WHERE pr=?",
                    (int(pr),)).fetchall()
    down = db.execute("SELECT pr FROM corrections WHERE corrects=?",
                      ('PR-%s' % pr,)).fetchall()
    sys.stdout.write('# PR-%s CORRECTS: %s\n' % (
        pr, ', '.join(u[0] for u in up) or 'none'))
    sys.stdout.write('# PR-%s CORRECTED-BY: %s\n' % (
        pr, ', '.join('PR-%d' % d[0] for d in down) or 'none'))
    if up:
        tgt = [int(re.sub(r'\D', '', u[0])) for u in up if re.search(r'\d', u[0])]
        rng = db.execute("SELECT pr,kind,start_line,end_line,title FROM "
                         "records WHERE pr IN (%s)" % ','.join('?' * len(tgt)),
                         tgt).fetchall()
        for r in rng:
            sys.stdout.write(out_pr(*r) + '\n')


def v_show(db, arg, a, full=False):
    pr = int(re.sub(r'\D', '', arg))
    r = db.execute("SELECT pr,kind,start_line,end_line,title,corrects,gate "
                   "FROM records WHERE pr=?", (pr,)).fetchone()
    if not r:
        sys.stdout.write('# no record PR-%d\n' % pr)
        return
    sys.stdout.write('# PR-%d  [%s]  %s\n' % (r[0], r[1] or '-', r[4][:70]))
    sys.stdout.write('# READ: %s:%d-%d  (%d lines)\n' % (SRC, r[2], r[3],
                                                         r[3] - r[2] + 1))
    sys.stdout.write('# kind=%s gate=%s corrects=%s\n' % (
        r[1] or '-', r[6] or '-', r[5] or 'none'))
    if full:                                   # print the raw block inline
        with open(SRC, encoding='utf-8', errors='replace') as f:
            for l in f.read().split('\n')[r[2] - 1:r[3]]:
                sys.stdout.write(l + '\n')


# ---- routing -------------------------------------------------------------

def route(db, arg, a):
    low = arg.lower()
    if low.startswith('0x'):
        # A real engine address is 4-6 hex (doc_census.ADDR_RE, shared
        # with the pinned index). Short hex is a field OFFSET, never an
        # address key, so it goes to the substring door (free), where
        # "0x33"/"+0x33" actually match, not the empty addr index. Keep
        # this shape in sync with ADDR_RE; v_addr's zero-hit hint is the
        # safety net if the two ever drift.
        if re.fullmatch(r'(?i)0x[0-9a-f]{4,6}', arg):
            v_addr(db, arg, a)
        else:
            v_free(db, arg, a)
    elif arg.startswith('['):
        v_field(db, arg, a)
    elif arg.upper().startswith('OI-'):
        v_oi(db, arg, a)
    elif re.fullmatch(r'(?i)PR-?\d+', arg):
        v_show(db, arg, a)
    else:
        v_free(db, arg, a)


VERBS = {'addr': v_addr, 'field': v_field, 'name': v_name, 'free': v_free,
         'oi': v_oi, 'corr': v_corr, 'show': lambda d, x, a: v_show(d, x, a),
         'all': lambda d, x, a: v_show(d, x, a, full=True)}


def main(argv):
    os.chdir(ROOT)                       # the ledger tools run from the repo root
    argv = argv[1:]
    flags = {'kind': None, 'unfixed': False, 'limit': 15, 'snippet': False,
             'json': False, 'rebuild': False}
    args = []
    i = 0
    while i < len(argv):
        t = argv[i]
        if t == '--kind':
            flags['kind'] = argv[i + 1]; i += 2
        elif t == '--limit':
            flags['limit'] = int(argv[i + 1]); i += 2
        elif t in ('--unfixed', '--snippet', '--json', '--rebuild'):
            flags[t[2:]] = True; i += 1
        else:
            args.append(t); i += 1
    if not args:
        print(__doc__.split('usage:')[1], file=sys.stderr)
        return 2

    class A:
        pass
    a = A()
    for k, v in flags.items():
        setattr(a, k, v)
    ensure_db(a.rebuild)
    db = sqlite3.connect(DB)
    if args[0] in VERBS:
        if len(args) < 2:
            print('jq: verb %s needs an argument' % args[0], file=sys.stderr)
            return 2
        VERBS[args[0]](db, args[1], a)
    else:
        route(db, ' '.join(args), a)
    db.close()
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))

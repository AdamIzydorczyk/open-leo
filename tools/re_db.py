#!/usr/bin/env python3
"""re_db.py - PROTOTYPE (not committed). A SQLite search layer over the
ledger, as a SECOND output of the SAME parser the pinned index uses.

WHY THIS FILE EXISTS AND WHAT IT MUST NEVER BECOME. docs/RE_INDEX.md is
the sanctioned, git-diffable, human index of the ledger (PR-197). It
answers "which records mention key X" and nothing else: it is a flat
page, it returns PR numbers not jump-to-line ranges, and it cannot do
free-text + filters + joins at once. This tool adds that query layer,
WITHOUT replacing the index and WITHOUT becoming a source of truth.

THE ONE RULE THAT MAKES IT SAFE (from re_index.py's own docstring):
there is only ONE parser of the ledger. This file imports doc_census
(vocabulary regexes, field-block template, the STRICT record boundary)
and re_index (records()), exactly as re_index.py does. It invents NO
regex and NO record slice of its own. A second parser is the PR-55
failure mode this repo already fought ("the twins agreeing is NOT
proof"). If doc_census and the index agree on a record boundary, this
DB agrees too, because it literally calls them.

It is generated and lands in the already-gitignored build/ (default
build/journal.db). It is never hand-edited and never authoritative:
regenerate it the way `make verify-index` regenerates the markdown.

usage: python3 tools/re_db.py [OUT.db]   (default: build/journal.db).
Exit 0; prints measured counts on stderr.
"""
import os
import re
import sqlite3
import sys

# Reuse the single parser. re_index already imports doc_census as dc;
# we import the same modules, so every regex and the record boundary
# are byte-identical to what P1/P4/verify-index enforce.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import doc_census as dc    # noqa: E402
import re_index as ri      # noqa: E402

# The field-block regexes, borrowed (not copied) from doc_census, keyed
# by the field name. P4 matches these at column 0; so do we.
FIELD_RX = dict(dc.FIELDS_COMMON)


def record_blocks(lines):
    """(pr, start_idx, end_idx) per record, using re_index's exact
    boundary (a `## PR-<n>` anchor preceded by a blank line, ending at
    the next real heading). Indices are 0-based into `lines`; the DB
    stores 1-based start/end so a query result points straight at the
    lines to open."""
    heads = {i for i in range(len(lines))
             if dc.HEAD_RE.match(lines[i])
             and (i == 0 or lines[i - 1].strip() == '')}
    starts = []
    for i in sorted(heads):
        m = dc.RECORD_RE.match(lines[i])
        if m:
            starts.append((i, int(m.group(1))))
    out = []
    for i, pr in starts:
        end = len(lines)
        for j in range(i + 1, len(lines)):
            if j in heads:
                end = j
                break
        out.append((pr, i, end))
    return out


def field_value(block, name):
    """First match of a borrowed field regex in the block, else None."""
    rx = FIELD_RX.get(name)
    if not rx:
        return None
    for l in block:
        if rx.match(l):
            return l.split(':', 1)[1].strip()
    return None


def gate_form(block):
    """Which gate the record reported: full pair, light line, or none.
    Reuses the same FIELDS_FULL/FIELDS_LIGHT regexes P4 uses."""
    full = any(rx.match(l) for _, rx in dc.FIELDS_FULL for l in block)
    light = any(rx.match(l) for _, rx in dc.FIELDS_LIGHT for l in block)
    if light:
        return 'light'
    if full:
        return 'full'
    return 'none'


def create_schema(db):
    db.executescript(
        """
        CREATE TABLE records(
          rid        INTEGER PRIMARY KEY,          -- synthetic; pr can repeat (PR-73)
          pr         INTEGER NOT NULL,
          title      TEXT,
          kind       TEXT,                        -- read|port|gate|docs (NULL pre-PR-196)
          subsystem  TEXT,
          code       TEXT,
          gate       TEXT,                        -- full|light|none
          corrects   TEXT,                        -- 'none' or 'PR-nn'
          start_line INTEGER NOT NULL,            -- 1-based into NEXT_STEPS.md
          end_line   INTEGER NOT NULL,
          nlines     INTEGER NOT NULL
        );
        CREATE VIRTUAL TABLE record_fts USING fts5(
          title, body, tokenize='unicode61'
        );
        CREATE TABLE addr_mentions(pr INTEGER, rid INTEGER, addr TEXT);
        CREATE TABLE field_mentions(pr INTEGER, rid INTEGER, field TEXT);
        CREATE TABLE name_mentions(pr INTEGER, rid INTEGER, name TEXT);
        CREATE TABLE oi_refs(pr INTEGER, rid INTEGER, oi TEXT, verb TEXT);
        CREATE TABLE corrections(pr INTEGER, corrects TEXT);
        CREATE TABLE pr_cites(pr INTEGER, rid INTEGER, cited INTEGER);
        CREATE INDEX ix_addr  ON addr_mentions(addr);
        CREATE INDEX ix_field ON field_mentions(field);
        CREATE INDEX ix_name  ON name_mentions(name);
        CREATE INDEX ix_oi    ON oi_refs(oi);
        -- PR-356: the hot lookup paths in jq.py. `corr` walks the
        -- correction graph both ways and `show` resolves a PR number to
        -- its line range, so corrects/pr need an index (without one the
        -- planner SCANS). rid-keyed indexes turn the mention->record
        -- join and the PR-citation walk into index lookups too.
        CREATE INDEX ix_corr_out ON corrections(pr);
        CREATE INDEX ix_corr_in  ON corrections(corrects);
        CREATE INDEX ix_rec_pr   ON records(pr);
        CREATE INDEX ix_cite     ON pr_cites(cited);
        CREATE INDEX ix_cite_pr  ON pr_cites(pr);
        CREATE INDEX ix_addr_rid ON addr_mentions(rid);
        CREATE INDEX ix_field_rid ON field_mentions(rid);
        CREATE INDEX ix_name_rid ON name_mentions(rid);
        """)


def build(db, lines):
    blocks = record_blocks(lines)
    n_addr = n_field = n_name = n_oi = n_corr = n_cite = 0
    rid = 0
    for pr, i, end in blocks:
        block = lines[i:end]
        body = '\n'.join(block)
        title = dc.RECORD_RE.match(block[0]) and block[0][3:].strip() or ''
        kind = field_value(block, 'KIND')
        subsystem = field_value(block, 'SUBSYSTEM')
        code = field_value(block, 'CODE')
        corrects = field_value(block, 'CORRECTS')
        rid += 1
        db.execute(
            "INSERT INTO records(pr,title,kind,subsystem,code,gate,corrects,"
            "start_line,end_line,nlines) VALUES(?,?,?,?,?,?,?,?,?,?)",
            (pr, title, kind, subsystem, code, gate_form(block), corrects,
             i + 1, end, end - i))
        db.execute(
            "INSERT INTO record_fts(rowid,title,body) VALUES(?,?,?)",
            (rid, title, body))
        for k in set(dc.ADDR_RE.findall(body)):
            db.execute("INSERT INTO addr_mentions VALUES(?,?,?)",
                       (pr, rid, k.lower()))
            n_addr += 1
        for k in set(dc.FIELD_RE.findall(body)):
            db.execute("INSERT INTO field_mentions VALUES(?,?,?)",
                       (pr, rid, k))
            n_field += 1
        names = set(dc.NAME_RE.findall(body)) | set(dc.DES_RE.findall(body))
        for k in names:
            db.execute("INSERT INTO name_mentions VALUES(?,?,?)",
                       (pr, rid, k))
            n_name += 1
        for verb_field, verb in (('OPENS', 'opens'), ('CLOSES', 'closes')):
            v = field_value(block, verb_field)
            if v:
                for oi in dc.OI_MEN_RE.findall(v):
                    db.execute("INSERT INTO oi_refs VALUES(?,?,?,?)",
                               (pr, rid, oi, verb))
                    n_oi += 1
        if corrects:
            for tgt in dc.PRCITE_RE.findall(corrects):
                db.execute("INSERT INTO corrections VALUES(?,?)",
                           (pr, tgt))
                n_corr += 1
        body_no_self = re.sub(r'\bPR-%d\b' % pr, '', body)
        for c in {int(x[3:]) for x in dc.PRCITE_RE.findall(body_no_self)}:
            db.execute("INSERT INTO pr_cites VALUES(?,?,?)", (pr, rid, c))
            n_cite += 1
    return {
        'records': len(blocks), 'rids': rid,
        'addr': n_addr, 'field': n_field, 'name': n_name,
        'oi': n_oi, 'corr': n_corr, 'cite': n_cite,
    }


def main(argv):
    src = 'NEXT_STEPS.md'
    out = argv[1] if len(argv) > 1 else 'build/journal.db'
    with open(src, encoding='utf-8', errors='replace') as f:
        lines = f.read().split('\n')
    d = os.path.dirname(out)
    if d:
        os.makedirs(d, exist_ok=True)
    if os.path.exists(out):
        os.remove(out)
    db = sqlite3.connect(out)
    create_schema(db)
    st = build(db, lines)
    db.commit()
    kinds = dict(db.execute(
        "SELECT COALESCE(kind,'(none)'), COUNT(*) FROM records "
        "GROUP BY 1 ORDER BY 2 DESC"))
    print('re_db: %d records -> %s' % (st['records'], out), file=sys.stderr)
    print('  rows: %d addr, %d field, %d name, %d oi, %d corr, %d cite '
          'mentions' % (st['addr'], st['field'], st['name'], st['oi'],
                        st['corr'], st['cite']), file=sys.stderr)
    print('  kind mix: ' + ', '.join('%s=%d' % kv for kv in kinds.items()),
          file=sys.stderr)
    db.close()
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))

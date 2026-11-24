#!/usr/bin/env python3
"""doc_census.py - THE ENGINE-DOCUMENTATION STRUCTURE PIN (PR-195;
   P4 added by PR-196, the record template).

WHY. The owner's documentation audit (2026-11-19) measured the doc set
and found that the only structural checks in this repo were two greps
run by hand: a heading census that is a PREFIX PROXY (`grep -c
'^## PR-1'` sees 107 of the 184 records in NEXT_STEPS.md) and a frozen
set of over-79-column lines. Neither one can see a record heading
disappear, a title stop matching the record anchor, or the plan section
be duplicated. This tool turns those hand-checks into a build leg. It
reads nothing but the tracked documents: no fixtures, no data, no
window, so it runs in both fixture modes like stage_selftest.

WHAT IT PINS - any of these fails the build:
  P1 record headings. A record starts with a `## PR-<n>` heading
     followed by a space or `(` (PR-41/42/77 do not use a plain dash
     title, so no title form may be demanded). The count must not FALL
     below RECORD_MIN (records are appended, never rewritten away), and
     every PR number up to the highest one must either have a record or
     be in MISSING_OK - the gaps that history already accepted, frozen
     here so a NEW gap (a heading that went missing or stopped matching
     the anchor) is loud.
  P2 (PR-203, inverted) the journal NEXT_STEPS.md carries NONE of
     `## Next`, `## Deferred big items`, `## Standing rules`, `##
     Status as of` - it is journal-only. The one-homes must each
     exist exactly once in HANDOFF.md: `## 2. Ground rules` and
     `## 8. Plan:` (open threads' one-home is the register, P6).
  P3 the width ratchet. Over-79-column lines per tracked document may
     not exceed WIDTH_MAX. Lowering one is always welcome; RAISING one
     needs a commit that says why. The floors carry the audit's
     measures as corrected by PR-202: HANDOFF 71 - 30 (its sections
     5/6/7: 25 table rows, 4 re-wrapped lines, 1 frozen line; the
     audit had claimed all 41 table rows sat in section 6, measured
     10/6/25 across sections 1/3/6 - the 16 outside those sections
     belong to a status/map step, not this one), NEXT_STEPS 71 ->
     62 (PR-203 removed the journal's status/plan/deferred sections,
     nine over-79 lines with them; 22 of the remainder are wrapped
     record titles), README 12.
  P4 the record template (PR-196, HANDOFF section 10a). A record whose
     number is >= TEMPLATE_FROM must carry the field block - the eleven
     lines (ten fields, the gate in two lines) each present once, at
     line start, in the agreed order. Only FIELDS_EXEMPT is exempt of
     the requirement (it may still use the format - PR-196 carries its
     two gate lines - and is reported as a NOTE); no other record may
     carry a partial block. From the next record on,
     the template cannot decay back into prose: this is what makes it
     a contract. Position is NOT tested ("immediately after the title"
     is section 10a's style, not a rule); presence, order and
     exactly-one-each are.

   P5 the .md INVENTORY (PR-200, doc-work step 6). The set of TRACKED
      .md files must equal the declared document standard, exactly:
      the four living root docs, the generated index and the
      register under docs/, the directory
      and tool READMEs, and files under docs/archive/ (frozen
      history, see docs/archive/README.md). A new root-level .md -
      the exact failure mode this repo showed on 2026-11-19: eight
      superseded work orders and reports sitting in the root, four
      thousand lines no tool could see - fails the build HERE, with
      the message saying where content belongs instead. A REQUIRED
      file going missing fails it too. The rule deliberately covers
      tracked files only: vendored material (vendor/SDL2's docs) is
      not ours to standardize and is not tracked.
   P6 the open-items REGISTER (PR-201, doc-work step 3).
      docs/OPEN_ITEMS.md defines rows `OI-<nnn>:` at column 0; every
      OI id mentioned anywhere in the live docs must be DEFINED
      there, ids are contiguous and ascending from OI-001 (stable:
      never renumber, never delete - closing flips the status line),
      every row carries a status line, the register has no over-79
      line, its row count never falls below OI_MIN, and no run of 4+
      one-or-two-column lines (the signature of a note inserted as a
      character list: a paragraph becomes 60 vertical characters and
      swallows the row header under it).
   report-only: the archive count, so drift in the frozen set shows.

WHAT IT ONLY REPORTS (no rule attached yet, they are the audit's
measurement points so a future PR can show a change): wrapped-title
`##` continuation lines, distinct addresses and mentions, `PR-nn`
citations, `[field+0xNN]` tokens, authored-name tokens.

usage: python3 tools/doc_census.py [FILE ...]   (default: the tracked
docs). Exit 0 + one "(ok)" line, or exit 1 with every FAILED reason.
P5 runs only in default mode (it measures the repo, not the files
handed to it).
"""
import collections
import io
import re
import sys

DOCS = ('NEXT_STEPS.md', 'HANDOFF.md', 'README.md')

# P5: the .md standard (PR-200). The four root docs: README is the
# entry, NEXT_STEPS the journal, HANDOFF the current map, PLAN the
# RE-facts reference the CODE cites ("PLAN 8b" / "PLAN 8g" appear in
# ported/ comments, so it is living, not archive). The archive is a
# prefix rule because adding frozen history is a normal act - listing
# every file by hand would make the check nag about appends while
# staying blind about real additions elsewhere.
ALLOW_ROOT = {'README.md', 'NEXT_STEPS.md', 'HANDOFF.md', 'PLAN.md'}
ALLOW_GEN = {'docs/RE_INDEX.md', 'docs/OPEN_ITEMS.md'}
ALLOW_ARCHIVE = 'docs/archive/'
ALLOW_DIR = {'original/README.md', 'ported/README.md'}
ALLOW_TOOL_RE = re.compile(r'^tools/[^/]+/README\.md$')
P5_REQUIRED = sorted(ALLOW_ROOT | ALLOW_GEN | ALLOW_DIR |
                    {'docs/archive/README.md'})
# P6 (PR-201, doc-work step 3): the open-items REGISTER
# (docs/OPEN_ITEMS.md). A row is DEFINED by an `OI-<nnn>: <title>`
# line at column 0; the id must then carry a `  status:` line inside
# its block. EVERY OI id mentioned anywhere in the live docs must be
# defined - that is what stops "open item" prose from drifting into
# a dozen restatements again, the failure mode the doc audit
# measured. IDS ARE STABLE: definitions must be strictly ascending
# and CONTIGUOUS from OI-001 (no renumbering, no deletion - closing
# a row flips its status to CLOSED, exactly like the record floor
# P1: history only grows). OI_MIN is the row floor, set to the
# number this pass seeded; rows may only be added.
REGISTER = 'docs/OPEN_ITEMS.md'
OI_DEF_RE = re.compile(r'^OI-(\d{3,}): ')
OI_MEN_RE = re.compile(r'\bOI-(\d{3,})\b')
OI_STATUS_RE = re.compile(r'^  status: '
                          r'(OPEN|PARTIAL|CLOSED BY PR-\d+.*|'
                          r'FROZEN \(owner\).*|DEFERRED \(owner\).*|'
                          r'DEFERRED \(ruled\).*)')
OI_MIN = 106

# P1: measured 2026-11-19 by this tool. 184 record headings = 183
# distinct PR numbers (PR-73 has two records, which is correct history:
# two separate steps were shipped that day). The anchor is a `## PR-<n>`
# heading followed by a space or `(`: PR-41/PR-42 are `## PR-41
# (SHELVED 2026-11-16) ...` and PR-77 is `## PR-77 investigation ...`,
# so neither the dash nor a fixed title form may be required. Lines
# like `## PR-138/141/142 precedent)` are wrapped-title continuations,
# and they are rejected because a `/` follows the number.
RECORD_MIN = 184
# PR numbers up to the highest one that have NO record heading. They are
# frozen as accepted history: PR-1..PR-6 predate the record convention,
# PR-22/23/24 were described inside the old 2026-11-14 status block
# (PR-203 replaced that block with a pointer; its text and what it
# described stay in git history and in the PR-7..PR-19 records), PR-80
# is written inside PR-79's record, PR-113 has no record at all.
MISSING_OK = frozenset(list(range(1, 7)) + [22, 23, 24, 80, 113])

# P3: the width ratchet. PR-202 (doc step 4) measured the audit's
# attribution: the "41 over-79 table rows" of the 2026-11-19 audit
# are NOT all section 6 - they sit 10 in HANDOFF section 1, 6 in
# section 3, 25 in section 6 - and the §5/6/7 step removes only
# what is in those three sections: 25 table rows (reflowed into
# ESTIMATE blocks), 4 section-5 lines (re-wrapped) and 1 section-7
# line (frozen into the archive). 71-30=41 is the new floor; the
# section 1/3 table rows belong to a status/map refresh step, not
# this one, and stay under the ratchet. NEXT_STEPS 71 -> 62
# (PR-203: nine long status/Next/Deferred lines left with the
# sections the journal-only rule deleted).
WIDTH_MAX = {'NEXT_STEPS.md': 62, 'HANDOFF.md': 41, 'README.md': 12}

# P4: the record template (PR-196), HANDOFF section 10a. It applies to
# every record whose number is TEMPLATE_FROM or higher. The one
# exemption is PR-196 itself: the record that WRITES this contract.
# The exemption skips the BLOCK requirement - that record does carry
# its two gate lines in the new format, which is legal for an exempt
# record and reported as a NOTE - and no other record may carry a
# partial block. The number is a constant on purpose - bumping
# TEMPLATE_FROM would silently let records skip the block again.
TEMPLATE_FROM = 196
FIELDS_EXEMPT = frozenset([196])
# The field lines, in order (ten fields, the gate in two lines =
# eleven lines), matched at line start only (a wrapped
# record title cannot produce a false hit: its lines never start with
# these names at column 0). The names are exact; prose variants are
# what the audit measured (86 gate-marker lines in 4 forms: `- GATES.`
# / `- GATES:` / `- GATES (` / `- TESTS AND GATES`, 12 distinct count
# claims) - so the field name is checked as a literal, not a stem. The
# gate line's wording is short on purpose and split in two: measured,
# one line carrying BOTH gate halves with their counts is 107 columns
# (control N7: merged, it fails P3 AND stops being the GATE-FREE
# field), so `GATE:` is the with-fixtures half and
# `GATE-FIXTURE-FREE:` the fixture-free half (section 2 rule 4
# requires both, so both are fields). A record may continue either
# line with wrapped prose. The counts are the point of
# the format - the audit measured 12 distinct count claims floating
# around 86 prose gate markers, which is where the 18/19/20 drift
# came from.
FIELDS_COMMON = (
    ('KIND', re.compile(r'^KIND: (read|port|gate|docs) *$')),
    ('SUBSYSTEM', re.compile(r'^SUBSYSTEM: \S')),
    ('ADDRESSES', re.compile(r'^ADDRESSES: [a-z0-9.,;+ /\[\]x_-]+')),
    ('NAMES', re.compile(r'^NAMES: [A-Za-z0-9.,;+ /()\[\]_-]+')),
    ('CODE', re.compile(r'^CODE: (no|yes): \S')),
    ('HOOKS', re.compile(r'^HOOKS: \S')),
    ('OPENS', re.compile(r'^OPENS: [A-Za-z0-9,;+ ._-]+')),
    ('CLOSES', re.compile(r'^CLOSES: [A-Za-z0-9,;+ ._-]+')),
    ('CORRECTS', re.compile(r'^CORRECTS: (none\b|no\b|nothing\b|PR-\d+)')),
)
# PR-232 (owner ruling): the gate FOLLOWS THE KIND. port/gate
# records carry the two-line full gate; read/docs records run only
# `make verify-index verify-docs` (no build, no verify-all - the
# diff touches no code that a fixture can read) and carry the
# single light line instead. The full pair stays ACCEPTED on a
# read/docs record (the PR-196..PR-231 history carries it, and a
# read PR may still run the full suite; it must then report it).
FIELDS_FULL = (
    ('GATE', re.compile(r'^GATE: make 0 warnings; verify-all fixtures '
                        r'rc=0 \d+ \(ok\);')),
    ('GATE-FREE', re.compile(r'^GATE-FIXTURE-FREE: rc=0 \d+ \(ok\) \+ '
                             r'\d+ named')),
)
FIELDS_LIGHT = (
    ('GATE-LIGHT', re.compile(r'^GATE: light: verify-index '
                              r'verify-docs rc=0 \(ok\);')),
)
FIELDS = FIELDS_COMMON + FIELDS_FULL  # the historical full block

RECORD_RE = re.compile(r'^## PR-(\d+)[\s(]')
OWNER_RE = re.compile(r'^## OWNER')
HEAD_RE = re.compile(r'^## ')
# P2 (PR-203, doc step 5, INVERTED from PR-195's rule): the journal
# must carry NONE of these section headings - plan, status, rules and
# open threads each have exactly one home elsewhere (HANDOFF sections
# 8/1/2, docs/OPEN_ITEMS.md). The old rule ("exists exactly once in
# NEXT_STEPS") measured the drift this removes: the status block went
# stale at PR-19 and `## Next` drifted into the middle of the file.
JOURNAL_SECTIONS = {
    '## Next': re.compile(r'^## Next'),
    '## Deferred big items': re.compile(r'^## Deferred big items'),
    '## Standing rules': re.compile(r'^## Standing rules'),
    '## Status as of': re.compile(r'^## Status as of'),
}
# And those one-homes must exist, exactly once, where they are claimed:
HOME_SECTIONS = {
    '## 2. Ground rules': re.compile(r'^## 2\. Ground rules'),
    '## 8. Plan:': re.compile(r'^## 8\. Plan'),
}
ADDR_RE = re.compile(r'0x[0-9a-fA-F]{4,6}')
PRCITE_RE = re.compile(r'\bPR-\d+\b')
FIELD_RE = re.compile(r'\[[a-z]+(?:_[a-z]+)?\+0x[0-9a-f]{1,3}\]')
NAME_RE = re.compile(r'\b(?:A_[A-Z0-9_]{3,}|AC_[A-Z0-9_]{2,}|'
                     r'K_[A-Z0-9_]{2,}|[a-z_]{4,20}=)\b')
# The `.des`-key half of NAME_RE (`\b[a-z_]{4,20}=\b`) is DEAD: there
# is no \b between a word char and `=`, so it never matches, and the
# authored key names (control=, access=, ...) were invisible to the
# audit's name count and to the report-only measure below. Caught while
# building the index (PR-197), which needs those keys. DES_RE is the
# working pattern, guarded so operators (>=, !=, <=) are not mistaken
# for keys. NAME_RE is kept so the historical report number stays
# comparable to the audit; the name measure counts both together.
DES_RE = re.compile(r'\b[a-z][a-z0-9_]{3,20}=(?![=<>!])')


def read(path):
    try:
        return io.open(path, encoding='utf-8', errors='replace').read()
    except OSError as e:
        print('docs structure: FAILED cannot read %s (%s)' % (path, e))
        return None


def check_template(lines, starts, heads, bad):
    """P4: every record at or above TEMPLATE_FROM carries the field
    block - ten fields, the gate in two lines, per HANDOFF section
    10a. A record's text runs from its heading to the NEXT REAL
    HEADING (the `## PR-<n>` anchor matches wrapped-title
    continuation lines too, and stopping at one of those cut this
    check's own first run of PR-196 down to a single line - the bug
    the real record caught, where the controls could not see it);
    fields are matched at column 0 only, so a quoted example
    of the template has to be indented to stay an example."""
    have, exempt = [], []
    for i, n in starts:
        if n < TEMPLATE_FROM or i not in heads:
            continue
        end = len(lines)
        for j in range(i + 1, len(lines)):
            if j in heads:
                end = j
                break
        block = lines[i:end]
        fields_here = [l for l in block
                       if any(rx.match(l) for _, rx in FIELDS)]
        if n in FIELDS_EXEMPT:
            exempt.append(n)
            if fields_here and len(fields_here) < len(FIELDS):
                print('docs structure: NOTE the exempt record PR-%d '
                      'carries %d field line(s) anyway (it is exempt '
                      'from the block, not forbidden from using the '
                      'format)' % (n, len(fields_here)))
            continue
        # PR-232 (owner ruling): the gate block FOLLOWS THE KIND.
        # port/gate records must carry the full two-line gate;
        # read/docs records may carry the full pair (the history
        # PR-196..PR-231 does) or the single light line - they run
        # only make verify-index verify-docs. found is built in
        # LINE order, so a swap or a duplicated field is visible:
        # wanted == found only for exactly-one-each, in the agreed
        # order.
        allf = FIELDS + FIELDS_LIGHT
        want_full = [f for f, _ in FIELDS]
        want_light = ([f for f, _ in FIELDS_COMMON] +
                      [f for f, _ in FIELDS_LIGHT])
        found = []
        for l in block:
            for f, rx in allf:
                if rx.match(l):
                    found.append(f)
        kind = ''
        kr = FIELDS_COMMON[0][1]
        for l in block:
            m = kr.match(l)
            if m:
                kind = m.group(1)
                break
        wants = [want_full]
        if kind in ('read', 'docs'):
            wants.append(want_light)
        if any(found == w for w in wants):
            have.append(n)
            continue
        want = wants[0]
        cnt = collections.Counter(found)
        miss = [f for f in want if not cnt[f]]
        dup = [f for f in want if cnt[f] > 1]
        msg = []
        if len(found) != len(want) and not miss and not dup:
            msg.append('extra field line(s) beyond the template')
        if miss:
            msg.append('missing %s' % ','.join(miss))
        if dup:
            msg.append('duplicated %s' % ','.join(dup))
        if not msg:
            at = next(k for k in range(len(want)) if found[k] != want[k])
            msg.append('%s before %s (the template order is %s before '
                       '%s)' % (found[at], want[at], want[at],
                                found[at]))
        bad.append('PR-%d does not carry the record template: %s'
                   % (n, '; '.join(msg)))
    print('docs structure: template check (P4, from PR-%d): %d records '
          'carry the field block, %d without (accepted)'
          % (TEMPLATE_FROM, len(have), len(exempt)))
    if exempt:
        print('docs structure: NOTE exempt from the template: %s'
              % ', '.join('PR-%d' % n for n in exempt))
    return have


def check_inventory(bad):
    """P5 (PR-200): the tracked .md set must equal the standard. Uses
    git ls-files (the repo, not the argv), so vendored/untracked
    material is out of scope by definition. Measured on the FIRST CI
    run (PR-200): a CI checkout trips git's safe.directory guard -
    "fatal: detected dubious ownership in repository at
    /__w/open-leo/open-leo" - because the runner's checkout and the
    step process do not share a uid, so plain git REFUSES to run
    there. Listing tracked paths is read-only and confined to this
    repository, so git is invoked with -c safe.directory='*', the
    documented escape hatch for exactly this case; it widens nothing
    outside this one call."""
    import subprocess
    try:
        out = subprocess.run(
            ['git', '-c', 'safe.directory=*',
             'ls-files', '-z', '--', '*.md'],
            capture_output=True, text=True, timeout=20)
    except (OSError, subprocess.SubprocessError) as e:
        bad.append('P5 cannot list tracked .md files (%s)' % e)
        return
    if out.returncode != 0:
        bad.append('P5: git ls-files failed: %s' % out.stderr.strip())
        return
    tracked = sorted(p for p in out.stdout.split('\0') if p)

    def allowed(p):
        return (p in ALLOW_ROOT or p in ALLOW_GEN or p in ALLOW_DIR
                or p.startswith(ALLOW_ARCHIVE)
                or ALLOW_TOOL_RE.match(p))

    extra = [p for p in tracked if not allowed(p)]
    have = set(tracked)
    missing = [p for p in P5_REQUIRED if p not in have]
    n_arch = sum(1 for p in tracked if p.startswith(ALLOW_ARCHIVE))
    print('docs structure: tracked .md inventory (P5): %d file(s), '
          '%d in the standard, %d archived (frozen), %d outside it%s'
          % (len(tracked), len(tracked) - len(extra), n_arch,
             len(extra),
             ': ' + ', '.join(extra) if extra else ''))
    if extra:
        bad.append('%d tracked .md file(s) outside the document '
                   'standard: %s - living content belongs in '
                   'NEXT_STEPS/HANDOFF/README/PLAN, superseded work '
                   'orders and reports go to docs/archive/ (listed in '
                   'its README), tool docs to tools/*/README.md; the '
                   'set itself lives in tools/doc_census.py, change '
                   'it there on purpose, never by accident'
                   % (len(extra), ', '.join(extra)))
    if missing:
        bad.append('document standard incomplete: required file(s) '
                   'missing: %s' % ', '.join(missing))


def check_register(text, bad):
    """P6 (PR-201): the open-items register and its id resolution.
    Runs in default mode (it measures the repository, like P5).
    `text` is the already-read live docs (main's dict)."""
    reg = read(REGISTER)
    if reg is None:
        bad.append('the open-items register %s is missing: it is the '
                   'ONLY place an OI-id may be defined' % REGISTER)
        return
    lines = reg.split('\n')
    wide = sum(1 for l in lines if len(l) > 79)
    if wide:
        bad.append('%s has %d line(s) over 79 columns; register rows '
                   'wrap by hand, there is no ratchet for it'
                   % (REGISTER, wide))
    runs, run = [], []
    for i, l in enumerate(lines):
        if 1 <= len(l) <= 2:
            run.append(i)
        else:
            if len(run) >= 4:
                runs.append((run[0] + 1, run[-1] + 1))
            run = []
    if len(run) >= 4:
        runs.append((run[0] + 1, run[-1] + 1))
    if runs:
        bad.append('%s has %d run(s) of 4+ one-or-two-column line(s) '
                   '(%s): that is what inserting a note as a list of '
                   'characters does to a paragraph, and it swallows '
                   'the row header under it - reflow the text and '
                   'restore the row'
                   % (REGISTER, len(runs),
                      ', '.join('%d-%d' % r for r in runs[:4])))
    defs = [int(OI_DEF_RE.match(l).group(1)) for l in lines
            if OI_DEF_RE.match(l)]
    ids = list(range(1, len(defs) + 1))
    if defs != ids:
        at = next((k for k in range(max(len(defs), len(ids)))
                   if k >= len(defs) or defs[k] != ids[k]), len(ids))
        bad.append('register ids are not contiguous and ascending from '
                   'OI-001 (first mismatch at position %d: got %s); '
                   'ids are stable - never renumber, never delete a '
                   'row, flip its status line instead'
                   % (at + 1,
                      'missing' if at >= len(defs) else 'OI-%03d'
                      % defs[at]))
    ends = [i for i, l in enumerate(lines) if OI_DEF_RE.match(l)]
    no_status = []
    for k, i in enumerate(ends):
        end = ends[k + 1] if k + 1 < len(ends) else len(lines)
        if not any(OI_STATUS_RE.match(l) for l in lines[i + 1:end]):
            no_status.append('OI-%03d' % defs[k])
    if no_status:
        bad.append('register rows without a status line (%s): every '
                   'row answers OPEN / PARTIAL / CLOSED BY PR-nn / '
                   'FROZEN (owner) / DEFERRED (owner)'
                   % ', '.join(no_status))
    kinds = {}
    for k, i in enumerate(ends):
        end = ends[k + 1] if k + 1 < len(ends) else len(lines)
        for l in lines[i + 1:end]:
            m = OI_STATUS_RE.match(l)
            if m:
                key = m.group(1).split(' ')[0]
                kinds[key] = kinds.get(key, 0) + 1
                break
    defined = set(defs)
    men = collections.Counter()
    for t in list(text.values()) + [reg]:
        for m in OI_MEN_RE.finditer(t):
            men[int(m.group(1))] += 1
    undef = sorted(n for n in men if n not in defined)
    if undef:
        bad.append('OI id(s) mentioned in the live docs but NOT '
                   'defined in %s: %s - add a row (with a status) or '
                   'stop citing the id'
                   % (REGISTER, ', '.join('OI-%03d' % n for n in undef)))
    if len(defs) < OI_MIN:
        bad.append('register has %d rows, the floor is %d: rows are '
                   'never deleted' % (len(defs), OI_MIN))
    print('docs structure: open-items register (P6): %d row(s) '
          '(floor %d), status kinds %s; %d OI-id mention(s) across '
          'the live docs, %d unresolved'
          % (len(defs), OI_MIN,
             ', '.join('%s=%d' % kv for kv in sorted(kinds.items())),
             sum(men.values()), len(undef)))


def main(argv):
    files = argv[1:] or list(DOCS)
    bad = []
    ns_path = 'NEXT_STEPS.md'
    text = {}
    for f in files:
        t = read(f)
        if t is None:
            bad.append('unreadable ' + f)
        else:
            text[f] = t

    # ---- P1 record headings -----------------------------------------
    ids = []
    owner = 0
    heads = 0
    starts = {}
    heads_at = {}
    for f, t in text.items():
        starts[f] = []
        fl = t.split('\n')
        heads_at[f] = {ln for ln in range(len(fl))
                       if HEAD_RE.match(fl[ln])
                       and (ln == 0 or fl[ln - 1].strip() == '')}
        for ln, line in enumerate(fl):
            if HEAD_RE.match(line):
                heads += 1
            m = RECORD_RE.match(line)
            if m:
                ids.append(int(m.group(1)))
                starts[f].append((ln, int(m.group(1))))
            elif OWNER_RE.match(line):
                owner += 1
    uniq = set(ids)
    top = max(uniq) if uniq else 0
    print('docs structure: records=%d (%d distinct PR numbers, up to '
          'PR-%d) + %d OWNER blocks; %d "## " lines across the files '
          'read (wrapped-title continuations are reported below)'
          % (len(ids), len(uniq), top, owner, heads))
    if len(ids) < RECORD_MIN and ns_path in text:
        bad.append('only %d record headings, expected at least %d: a '
                   'record went missing or stopped matching the '
                   '"## PR-<n>" anchor' % (len(ids), RECORD_MIN))
    if top:
        gaps = {n for n in range(1, top + 1) if n not in uniq}
        new = sorted(gaps - set(MISSING_OK))
        if new:
            bad.append('PR numbers with no record heading, NOT in the '
                       'accepted-gap list: %s'
                       % ', '.join('PR-%d' % n for n in new))
        dups = [n for n, c in collections.Counter(ids).items()
                if c > 1 and n != 73]
        if dups:
            print('docs structure: NOTE repeated record headings: %s'
                  % ', '.join('PR-%d x%d' % (n, collections.Counter(
                      ids)[n]) for n in sorted(dups)))

    # ---- P2 the journal carries no plan/status/rule sections; the
    #         one-homes exist exactly once in HANDOFF (PR-203) ------
    if ns_path in text:
        for label, rx in JOURNAL_SECTIONS.items():
            n = sum(1 for l in text[ns_path].split('\n') if rx.match(l))
            if n:
                bad.append('%s appears %d times in %s: the journal is '
                           'journal-only - the plan lives at HANDOFF '
                           'section 8, rules at HANDOFF section 2, open '
                           'threads at %s, standing at README/HANDOFF '
                           'section 1' % (label, n, ns_path, REGISTER))
        print('docs structure: journal carries none of the plan/status/'
              'rule sections (%s)'
              % ', '.join(sorted(JOURNAL_SECTIONS)))
    if 'HANDOFF.md' in text:
        for label, rx in HOME_SECTIONS.items():
            n = sum(1 for l in text['HANDOFF.md'].split('\n')
                    if rx.match(l))
            if n != 1:
                bad.append('%s appears %d times in HANDOFF.md, expected '
                           'exactly 1 (it is the one home)' % (label, n))
        print('docs structure: the plan/rules one-homes present once in '
              'HANDOFF (%s)' % ', '.join(sorted(HOME_SECTIONS)))

    # ---- P3 the width ratchet --------------------------------------
    parts = []
    for f in sorted(text):
        over = [l for l in text[f].split('\n') if len(l) > 79]
        limit = WIDTH_MAX.get(f, 0)
        parts.append('%s %d/%d' % (f, len(over), limit))
        if len(over) > limit:
            worst = max(over, key=len)
            bad.append('%s has %d lines over 79 columns, the ratchet is '
                       '%d (longest now %d columns: %r)'
                       % (f, len(over), limit, len(worst), worst[:60]))
    print('docs structure: over-79-column lines (count/ratchet): %s'
          % ', '.join(parts))

    # ---- P4 the record template (PR-196) ----------------------------
    if ns_path in text:
        check_template(text[ns_path].split('\n'), starts[ns_path],
                       heads_at[ns_path], bad)

    # ---- P5 the .md inventory (PR-200), default mode only ----------
    if not argv[1:]:
        check_inventory(bad)
        check_register(text, bad)

    # ---- report only ----------------------------------------------
    ns = text.get(ns_path, '')
    addrs = ADDR_RE.findall(ns)
    fields = FIELD_RE.findall(ns)
    names = NAME_RE.findall(ns) + DES_RE.findall(ns)
    cites = PRCITE_RE.findall(ns)
    cont = 0
    if ns:
        prev = False
        for l in ns.split('\n'):
            if RECORD_RE.match(l) or OWNER_RE.match(l):
                prev = True
            elif HEAD_RE.match(l) and prev:
                cont += 1
            else:
                prev = False
    print('docs structure: report only (the audit\'s measures, no rule): '
          'addresses %d distinct/%d mentions, PR-nn citations %d, '
          'field tokens %d distinct/%d mentions, name tokens %d '
          'distinct (NAME_RE + the fixed DES_RE; NAME_RE alone saw '
             '%d), wrapped-title "##" lines %d'
          % (len(set(addrs)), len(addrs), len(cites), len(set(fields)),
             len(fields), len(set(names)), len(set(NAME_RE.findall(ns))),
             cont))

    if bad:
        for b in bad:
            print('docs structure: FAILED %s' % b)
        return 1
    print('docs structure: record headings, plan sections, the width '
          'ratchet, the record template, the .md inventory and the '
          'open-items register hold (ok)')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))

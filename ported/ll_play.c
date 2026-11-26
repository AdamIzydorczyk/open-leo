/* ll_play.c - playable-level core (M4-B, PR-43). See ll_play.h for the
 * data provenance and the documented simplifications.
 *
 * Format facts this code is built on (all verified on real payloads,
 * 2026-11-16):
 *   RAN.PLT (HD ETAPY RAN.01, 4935 B): "153" count line, then
 *     "1 126,314;302,314;" one-segment shelves and multi-vertex terrain
 *     lines, world coords (y down). TUNGA.PLT likewise 7 lines.
 *   LEW.MOV (HD ANIMACJE RAN.01/LEW.RAN, 8666 B): state preamble list,
 *     then [STATE] blocks: control= flags (BY_TIME/BY_X/CHECK_LN/CH_DIR/
 *     CHECK_ALL/CHECK_ALL_PLANE/FRAME_REV), F=/T=/G= numbers, per-state
 *     seq= art, transitions like "A_STAY(1)=!K_FWD", frames "n,ms[, M
 *     dx,dy]". Walk: F=400 T=90; grounded G=0 + CHECK_LN; real fall
 *     state A_START_LAD1 G=0,200 with A_LADOW(1)=K_GEN2 on landing.
 */
#include "ll_play.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ------------------------------------------------------------- .plt ---- */

void ll_plt_free(ll_plt *o)
{
    for (int k = 0; k < o->n; k++)
        free(o->line[k].p);
    free(o->line);
    o->line = NULL;
    o->n = 0;
}

int ll_plt_parse(const char *text, ll_plt *out)
{
    out->line = NULL;
    out->n = 0;
    const char *p = text;
    int header_skipped = 0;
    int declared = -1;            /* the header line's polyline count     */
    while (*p) {
        const char *eol = p;
        while (*eol && *eol != '\n' && *eol != '\r')
            eol++;
        /* the first non-empty line is the polyline count header (real
         * files lead with "153"/"7"); everything after it is geometry */
        if (!header_skipped && eol > p) {
            int probe = 0;
            if (sscanf(p, "%d", &probe) == 1 && probe >= 0) {
                header_skipped = 1;
                declared = probe;
                p = eol;
                while (*p == '\n' || *p == '\r')
                    p++;
                continue;
            }
            header_skipped = 1;         /* malformed: don't eat lines forever */
        }
        if (eol > p) {
            /* "n x,y;x,y;..." - n counts SEGMENTS (points = n+1); we scan
             * every x,y pair and ignore n, so a wrong count can't cut
             * usable geometry (same lesson as the PR-36 sparse tables). */
            ll_xy pts[LL_PLT_MAXPT];
            int npts = 0;
            const char *q = p;
            char *end = NULL;
            strtoll(q, &end, 10);               /* skip the segment count   */
            q = end;
            while (q < eol && npts < LL_PLT_MAXPT) {
                long a = strtol(q, &end, 10);
                if (end == q || *end != ',')
                    break;
                long b = strtol(end + 1, &end, 10);
                if (end == q || b > 0x7fffffff || a > 0x7fffffff)
                    break;
                pts[npts].x = (int)a;
                pts[npts].y = (int)b;
                npts++;
                q = end;
                /* point terminators: ';' is the normal one, '!' appears in
                 * real files as an equivalent separator inside the SAME
                 * polyline (ENA.PLT: "1 1,560!7500,570;", ZARIA_A.PLT has
                 * 10 of them) - the segment count prefix counts SEGMENTS
                 * either way, so it is the same separator with a different
                 * keystroke; treated identically. */
                while (q < eol && (*q == ';' || *q == '!' || *q == ',' ||
                                   *q == ' '))
                    q++;
            }
            if (npts >= 2) {
                ll_poly *nl = realloc(out->line,
                                      sizeof(ll_poly) * (size_t)(out->n + 1));
                if (nl) {
                    /* commit the grown array BEFORE allocating points: if
                     * the point malloc fails the (empty) array stays
                     * reachable through out->line for ll_plt_free */
                    out->line = nl;
                    ll_poly *pl = &nl[out->n];
                    pl->p = malloc(sizeof(ll_xy) * (size_t)npts);
                    pl->n = 0;
                    if (pl->p) {
                        memcpy(pl->p, pts, sizeof(ll_xy) * (size_t)npts);
                        pl->n = npts;
                        out->n++;
                    }
                }
            }
        }
        p = eol;
        while (*p == '\n' || *p == '\r')
            p++;
    }
    /* A file that DECLARES zero polylines is complete and valid, not
     * garbage: JUNCTION's platformy=none.des carries literally "0" -
     * that level simply has no floor. Reporting it as n==0 (rc 0) keeps
     * the difference between "authored empty" and "nothing usable here"
     * (declared<0, or a count with no geometry) visible to the caller. */
    return out->n > 0 || declared == 0 ? 0 : 1;
}

/* interpolate y of segment k at x (integer math, deterministic: the real
 * .plt coordinates are all small ints, products fit in long long) */
static int seg_y_at(const ll_poly *pl, int k, int x)
{
    int x1 = pl->p[k].x, x2 = pl->p[k + 1].x;
    if (x2 == x1)
        return pl->p[k].y < pl->p[k + 1].y ? pl->p[k].y : pl->p[k + 1].y;
    long long dy = (long long)pl->p[k + 1].y - pl->p[k].y;
    return pl->p[k].y + (int)(dy * (x - x1) / (x2 - x1));
}

/* does any segment cover x? (inclusive endpoints) */
static int seg_cover(const ll_poly *pl, int k, int x)
{
    int lo = pl->p[k].x < pl->p[k + 1].x ? pl->p[k].x : pl->p[k + 1].x;
    int hi = pl->p[k].x < pl->p[k + 1].x ? pl->p[k + 1].x : pl->p[k].x;
    return x >= lo && x <= hi;
}

int ll_plt_surface_at(const ll_plt *o, int x, int want_y, int tol, ll_xy *hit)
{
    int found = 0;
    int best = 0x7fffffff;
    for (int li = 0; li < o->n; li++) {
        const ll_poly *pl = &o->line[li];
        for (int k = 0; k + 1 < pl->n; k++) {
            if (!seg_cover(pl, k, x))
                continue;
            int y = seg_y_at(pl, k, x);
            int d = y > want_y ? y - want_y : want_y - y;
            if (d <= tol && d < best) {
                best = d;
                found = 1;
                if (hit) {
                    hit->x = x;
                    hit->y = y;
                }
            }
        }
    }
    return found;
}

int ll_plt_first_below(const ll_plt *o, int x, int from_y, ll_xy *hit)
{
    int found = 0;
    int best = 0x7fffffff;
    for (int li = 0; li < o->n; li++) {
        const ll_poly *pl = &o->line[li];
        for (int k = 0; k + 1 < pl->n; k++) {
            if (!seg_cover(pl, k, x))
                continue;
            int y = seg_y_at(pl, k, x);
            if (y >= from_y && y < best) {
                best = y;
                found = 1;
                if (hit) {
                    hit->x = x;
                    hit->y = y;
                }
            }
        }
    }
    return found;
}

/* ------------------------------------------------------------------ .blk
 * The `blokady=` registry the physics step's blockade query walks
 * (0x4db50 builds [world+0x70], count [world+0x7c], stride 0x14; the
 * query is 0x4b430 and the test is 0x45620 - PR-313 named the
 * registry, PR-394 read the query, PR-395 read the record bytes).
 *
 * The record's three authored numbers are PARSED, not copied: 0x455e0
 * moves its `eax` to rec+4 and its `ecx` to rec+0x10 and calls
 * 0x4a0b0, which scans the format "%d,%d" (0x91d0f) out of the row
 * into rec+0x4 and rec+0x8; then 0x455e0 scans "%d" (0x9183d) into
 * rec+0x10 and probes for a "," (0x91840). rec+0x0 and rec+0xc never
 * come from the file - they are the method table 0xa2178 and the shared
 * class table 0x98278, written by the record's constructor 0x4fb70 (the
 * first field of the class record 0xa14c0, which keeps its fields at
 * C+1/C+5/C+9/C+0xd), and that constructor is also what zeroes
 * rec+0x4, rec+0x8 and rec+0x10 before the scan writes them; 0x45620
 * never reads +0x0 or +0xc. So a record is a VERTICAL WALL: its face x
 * at rec+0x4, its top y at rec+0x8, and how far down it goes at
 * rec+0x10. That is what the authored files are: ZAR.BLK's six rows
 * are the walls x=0 and x=2240 over the whole 700 px height - the two
 * world edges - and four short ones at 1299/341+118, 1462/244+28,
 * 1540/194+27 and 1620/0+200.
 *
 * The file is read as ONE token stream, not as lines: 0x4db7b reads up
 * to 0x400 bytes into the engine's token cursor and 0x4db8e scans the
 * header count (the format "%d" at 0x91f24) from it, and then
 * 0x4dbcf-0x4dbe4 runs the record scan EXACTLY count times. So the
 * count is authoritative in both directions - extra rows are never
 * read, and a file that runs out of numbers leaves the rest of the
 * records at the zero their constructor gave them. */
#define LL_BLK_MAX 64          /* records one file may own; the measured
                                * maximum across the shipped set is 49 */

/* The next number in the engine's token stream, shared by both registry
 * loaders: 1 and *out set, or 0 at the end. What separates is exactly
 * what the loaders' own scans accept - 0x4a0b0's "%d,%d" (0x91d0f)
 * carries its separator argument " , " (0x91d15, through 0x498f0 at
 * 0x4a0c4), and after the third number 0x455e0 (the `.BLK` row) probes
 * a bare "," (0x91840) while 0x454d0 (the `.SUF` row) probes " , "
 * (0x91839) - and the authored rows use commas, CR, LF and spaces. The
 * port treats all of them as separators anywhere. */
static int reg_next(const char **pp, int *out)
{
    const char *p = *pp;
    char *end = NULL;
    long v;

    while (*p == ',' || *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    v = strtol(p, &end, 10);
    if (end == p)
        return 0;
    *out = (int)v;
    *pp = end;
    return 1;
}

int ll_blkreg_parse(const char *text, ll_blkreg *out)
{
    const char *p = text;
    int count = 0, i, k;
    ll_blk_rec *v;

    out->r = NULL;
    out->n = 0;
    if (!reg_next(&p, &count))
        return 1;                   /* no header: not a blokady file */
    if (count < 0 || count > LL_BLK_MAX)
        return 1;                   /* the port will not hold it */
    v = count ? calloc((size_t)count, sizeof *v) : NULL;
    if (count && !v)
        return 1;
    for (i = 0; i < count; i++) {
        int f[3] = { 0, 0, 0 };     /* the zero the record ctor writes */
        for (k = 0; k < 3; k++)
            if (!reg_next(&p, &f[k]))
                break;
        v[i].x = f[0];
        v[i].y = f[1];
        v[i].ly = f[2];
    }
    out->r = v;
    out->n = count;
    return 0;
}

void ll_blkreg_free(ll_blkreg *o)
{
    free(o->r);
    o->r = NULL;
    o->n = 0;
}

int ll_block_hit(const ll_blk_rec *rec, int bx0, int bx1, int by0, int by1,
                 int *dx)
{
    int target, dist;

    *dx = 0;
    /* 0x45626-0x45634: `cmp [rec+4], [block+0]` / `jl` and
     * `cmp [rec+4], [block+8]` / `jg` - the wall's face x must lie
     * between the requester's two x, both ends inclusive. */
    if (rec->x < bx0 || rec->x > bx1)
        return 0;
    /* 0x4563a-0x45649 and 0x4564b-0x4565a: the wall is the interval
     * [y, y+ly] (rec+8 and rec+8 + rec+0x10, and an authored ly may be
     * negative) and the requester is [block+4, block+0xc]. The record
     * is refused only when BOTH ends of one of those intervals lie
     * outside the other, which is an OVERLAP test, not a point test:
     * `cmp y, hi / jle` skips the length check, and only `y + ly > hi`
     * (0x45647) or `y + ly < lo` (0x45658) is a miss. */
    if (rec->y > by1 && rec->y + rec->ly > by1)
        return 0;
    if (rec->y < by0 && rec->y + rec->ly < by0)
        return 0;
    /* 0x4565c-0x45676: which side to move to. The comparison is
     * `([rec+4] - [block+0]) >= ([block+8] - [rec+4])`, so the wall's
     * distance from the requester's NEAR x against its distance from
     * the FAR x, with the tie (jge) taking the far side. The two
     * targets are the REQUESTER's own edges plus one - [block+0] - 1 or
     * [block+8] + 1 - never a face of the record: the engine is
     * resolving an overlap, not aiming at the wall. */
    target = (rec->x - bx0 >= bx1 - rec->x) ? bx1 + 1 : bx0 - 1;
    /* 0x45677-0x45686: d = [rec+4] - target, and its absolute value
     * (0x4567e-0x45681 is the `cdq/xor/sub` idiom) is refused when it
     * is <= 1, which is exactly the case where the requester is already
     * flush against the wall: it is TOUCHING, not overlapping, and the
     * engine leaves it alone and never writes [obj+0xca] at all. */
    dist = rec->x - target;
    if (dist < 0)
        dist = -dist;
    if (dist <= 1)
        return 0;
    *dx = rec->x - target;          /* 0x4568e ADDs this, signed, to
                                     * [obj+0xca] - so the requester's
                                     * near or far edge lands on the
                                     * wall's x minus/plus one. */
    return 1;
}

int ll_block_walk(const ll_blkreg *reg, int bx0, int bx1, int by0, int by1,
                  int *dx, int *idx)
{
    int i, v;

    *dx = 0;
    *idx = -1;
    for (i = 0; i < reg->n; i++) {
        /* 0x4b494-0x4b4bc: the count is [world+0x7c], the stride is
         * 0x14, and 0x4b4af `test eax,eax / jne 0x4b4d6` leaves the
         * walk AT THE FIRST record that answers. Its tail clears the
         * request block's flag byte and 0x4b4ec `or BYTE [result],0x1`
         * raises AC_BLOK once. */
        if (ll_block_hit(&reg->r[i], bx0, bx1, by0, by1, &v)) {
            *dx = v;
            *idx = i;
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ .suf
 * The `sufity=` ceiling registry: 0x4da90 fills [world+0x78] (count
 * [world+0x84], stride 0x14) and 0x4b7f0 walks it through the test
 * 0x45500 (PR-396; PR-394's OPENS row named the registry, PR-391 read
 * the `sufity=` key arm at 0x4cfc7 and the chain 0x4d484-0x4d4a0 that
 * calls this loader).
 *
 * The record class is the one at 0xa14d8, and it explains the question
 * PR-395 left open about where a record's method table comes from. This
 * binary's class records keep their fields one byte in: [C+1] is the
 * constructor, [C+5] the destructor, [C+0xd] the record size - 0x14
 * here - which is what 0x76ab0 reads when it constructs an array
 * (`call [class+1]` per element, `add ecx,[class+0xd]` between them).
 * For the ceiling record that constructor is 0x4fbd0: it writes 0 at
 * rec+0x0, rec+0x4 and rec+0xc, the shared class table 0x98278 at
 * rec+0x8 and
 * the method table 0xa218c at rec+0x10. So [[rec+0x10]] is 0xa218c[0]
 * = 0x454d0, the row parser, and the walk's [[rec+0x10]+4] is
 * 0xa218c[4] = 0x45500, the test - the tables the loader and the query
 * call are the record's own, seeded by its constructor, not a table the
 * file can reach.
 *
 * 0x454d0 parses the row: 0x4a0b0 scans "%d,%d , " (0x91d0f, the pair
 * plus its separator argument at 0x91d15) into rec+0x0 and rec+0x4,
 * then "%d" (0x91836) into rec+0xc, then it probes " , " (0x91839).
 * rec+0x8 and rec+0x10 are never touched from the file, and a ceiling
 * record is a HORIZONTAL LINE: from x along +x for lx px, at y. That is
 * what the shipped files are - KADMAR.07/KADMAR.SUF is one row,
 * `0,0,3000`, the whole map's top; RAN.01/RAN.SUF is `20,100,28800`;
 * KARA.07/KARA.SUF is three: `0,0,1600`, `125,290,10`, `1535,40,10`.
 *
 * The token stream is the same one `.BLK` reads (the shared scanner
 * above), so the grammar and the header-count rule carry over
 * unchanged: one read, the count first, then EXACTLY count triples. */
int ll_sufreg_parse(const char *text, ll_sufreg *out)
{
    const char *p = text;
    int count = 0, i, k;
    ll_suf_rec *v;

    out->r = NULL;
    out->n = 0;
    if (!reg_next(&p, &count))
        return 1;                   /* no header: not a sufity file */
    if (count < 0 || count > LL_SUF_MAX)
        return 1;                   /* the port will not hold it */
    v = count ? calloc((size_t)count, sizeof *v) : NULL;
    if (count && !v)
        return 1;                   /* the port will not hold it */
    for (i = 0; i < count; i++) {
        int f[3] = { 0, 0, 0 };     /* the zero the record ctor writes */
        for (k = 0; k < 3; k++)
            if (!reg_next(&p, &f[k]))
                break;
        v[i].x = f[0];
        v[i].y = f[1];
        v[i].lx = f[2];
    }
    out->r = v;
    out->n = count;
    return 0;
}

void ll_sufreg_free(ll_sufreg *o)
{
    free(o->r);
    o->r = NULL;
    o->n = 0;
}

int ll_ceiling_hit(const ll_suf_rec *rec, int ax, int ay, int top, int *dy)
{
    int d;

    *dy = 0;
    /* 0x45553-0x45564: `cmp [rec+0], ax / jl` and `cmp [rec+0] +
     * [rec+0xc], ax / jl` - the anchor's x must lie on the line, from
     * its start to its start plus its length, both ends inclusive. */
    if (ax < rec->x || rec->x + rec->lx < ax)
        return 0;
    /* 0x45566-0x4556e: `cmp [esi+4], [rec+4] / jl` where esi is the
     * body box from the body's own getter slot +0x68 and [esi+4] is
     * its near y. The line must not be ABOVE the body's top: this is
     * not a ceiling for a body that is wholly under it. */
    if (top < rec->y)
        return 0;
    /* 0x45570-0x4557a: d = rec.y - the anchor's absolute y, refused
     * when it is <= 0 - a body whose anchor is already at or below the
     * line is left where it is, and the engine never writes [obj+0xce]
     * at all. */
    d = rec->y - ay;
    if (d <= 0)
        return 0;
    *dy = d;                        /* 0x45582-0x45584 ADDs this, signed,
                                     * to [obj+0xce], which lands the
                                     * anchor exactly on the line. */
    return 1;
}

int ll_ceiling_walk(const ll_sufreg *reg, int ax, int ay, int top,
                    int *dy, int *idx)
{
    int i, v;

    *dy = 0;
    *idx = -1;
    for (i = 0; i < reg->n; i++) {
        /* 0x4b806-0x4b818: the count is [world+0x84], re-read at
         * 0x4b80d, the stride is 0x14, and 0x4b82d `test eax,eax / je
         * 0x4b80c` moves to the NEXT record only while the answer is
         * no - the first record that answers leaves the walk and the
         * rest are never asked. Its tail clears the result block
         * (0x4b83b-0x4b83f) and 0x4b844 raises `or BYTE
         * [result+3],0x2`. */
        if (ll_ceiling_hit(&reg->r[i], ax, ay, top, &v)) {
            *dy = v;
            *idx = i;
            return 1;
        }
    }
    return 0;
}


/* ------------------------------------------------------- .MOV parse ---- */

int ll_mov_find(const ll_mov_t *m, const char *name)
{
    for (int k = 0; k < m->nst; k++)
        if (strcasecmp(m->st[k].name, name) == 0)
            return k;
    return -1;
}

/* PR-107: the row a transition's `( N ) ` starts the TARGET state at, as
 * a 0-based index into its frame list. One rule for both sides that read
 * it - the lion (ll_player_step) and the scene objects (obj_transition) -
 * so they cannot drift apart again. The engine's own, verbatim: the
 * transition line parse matches `" ( %[^) ] %*[)] = "` (the format at
 * 0x9225c, called from 0x51e12), tries `"%ld"` on that token (0x57db0,
 * which calls the scanner at 0x74cc8 with the format at 0x92310) and on
 * a number stores `strtol` MINUS ONE (0x57fd0 + `dec eax` at 0x51e3b);
 * an absent or non-numeric token stores -1 (0x51e3e). That value is the
 * transition entry's row field at +0x11, and it reaches the live row
 * counter of the TARGET state record by both of the engine's paths. The
 * walk 0x52000 hands it out (0x52046-0x5204d) to the transition apply
 * step 0x535a0 (0x535a2 is listed one byte in), which takes the
 * target state index in edx and the start row in ebx: a same-index
 * line writes the row into the current record straight out (0x53616,
 * and only if it differs - 0x53612 skips the
 * write otherwise); a cross-state line reaches the same field through
 * the state record's method table, because every record stores the fixed
 * table 0xa25d4 at rec+0x44 (its constructors, 0x524cf and 0x52508) and
 * that step calls slot +0x1c of it on the NEW record (0x535e7-0x535ea),
 * and +0x1c IS the row-set method 0x52360: it writes rec+0x4 raw
 * (0x523cb) after refusing a negative value (`test edx,edx` + `jl`,
 * 0x52381). The value stays live in edx through the methods in between
 * - they save and restore it themselves (`push edx` 0x52660 /
 * `pop edx` 0x5267a, and 0x52072 / 0x520b4), the way this binary
 * carries its second register argument.
 * rec+0x4 IS the row index: the row-advance method 0x520c0 steps it
 * (`[rec+0x4] = ecx+1`, 0x521d7) and wraps it at the last row
 * (`[rec+0x4] = 0`, 0x521c5). The engine has NO upper test on
 * the value, and mirrors it (`[rec+0x36] - n - 1`, 0x523bc-0x523c5) when
 * the SEQ_REV reversal of PR-95 applies - neither of which the port
 * models: an over-long row is clamped to the top here (the rule the
 * player side shipped in PR-86, and what keeps a state with a short
 * frame list from being skipped whole), and -1 (the port's entry 0:
 * absent or `"(x)"`) starts from the top instead of leaving the row
 * where it was. Both are MODEL CHOICES, recorded in NEXT_STEPS PR-107. */
int ll_mov_row_start(const ll_mov_st *s, int entry)
{
    int i = entry > 0 ? entry - 1 : 0;      /* the authored N is 1-based */

    if (i >= s->nfr)
        return 0;                           /* over-long row: from the top */
    return i;
}

/* PR-287: a row's `Wait <clause>` tail, ACTED ON. The engine's row step
 * 0x521f0 asks the row node's test slot +0x8 at 0x52235 and hands the
 * set it returns to 0x495b0 at 0x5223c - three dwords, any one non-zero
 * answers 0 (0x495bd-0x495d5), so that call is the EMPTY test - and takes
 * the plain admit path 0x522fc, the one that runs the row's execute slot
 * and compares the threshold against [obj+0xa1] at 0x52329, ONLY when the
 * set is empty. The `Wait` node (class 0xa11c8, PR-217) answers +0x8 with
 * 0x45970, which copies the node's own 12-byte clause at [node+0x8] and
 * never calls its operand, so a `Wait` row takes the other branch:
 * 0x52251 copies the caller's key set (0x5226f) and the object's
 * [obj+0x60] (0x5229d), 0x49580 merges them at 0x522bc, and 0x47e40 at
 * 0x522e6 matches the clause against that union - and the `je 0x52354`
 * at 0x522ed returns from the whole step. A gated row that does not match
 * admits NO row: no frame advance, no list wrap, no list-end bit, no
 * signal, no threshold consumed. The operands are the comparator's own:
 * K_ANY is the mask operand (0x48037-0x48048 OR-s 0x2001f8 into dword 1)
 * and always matches, K_NEVER carries no bit and never does, and a plain
 * name matches exactly when its bit is live (a `!` clause shares NOTHING,
 * the same arm the transition evaluator of PR-87 uses). The one tail with
 * no token (`Wait ;`, 1 of the 255 measured) compiles to an EMPTY node
 * set, which is the plain path, so it gates nothing. What is NOT modelled
 * here: the gated path skips the threshold compare at 0x52329 altogether
 * (its row admits on the clause alone, with the accumulator left alone) -
 * every gated row in the ladder chain this PR is about has threshold 0,
 * where the two readings agree; the general case is counted at OI-111. */
static unsigned long long ll_mov_fr_mask(const ll_mov_fr *f)
{
    if (f->gop == LL_GOP_ANY)
        return LL_SIG_K_ANY_MASK;     /* PR-263, re-read by PR-332: the
                                       * resolver's answer for K_ANY is
                                       * this mask (0x48037-0x48048),
                                       * the six arrows plus bit 53 -
                                       * NOT a wildcard */
    if (f->gop == LL_GOP_NEVER)
        return LL_SIG_K_NEVER_MASK;   /* and for K_NEVER it is 0x60 in
                                       * mask word 1 = K_UP and K_DOWN
                                       * together (0x4806f-0x4807d) */
    if (f->gop == LL_GOP_NAME && f->gsig_id >= 0)
        return 1ull << (unsigned)f->gsig_id;
    return 0ull;                      /* a name in NO table, or the one
                                       * tokenless `Wait ;`: the answer
                                       * set is EMPTY, and 0x495b0
                                       * answers 12 for an empty set -
                                       * that row takes the clock path,
                                       * so it is not gated at all */
}

static int ll_mov_row_gated(const ll_mov_fr *f)
{
    return ll_mov_fr_mask(f) != 0ull;
}

int ll_mov_row_gate(const ll_mov_fr *f, unsigned long long live)
{
    unsigned long long mask = ll_mov_fr_mask(f);

    if (!mask)
        return 1;                       /* not this path's business */
    if (f->gneg)                        /* mode 1, the `!` prefix: the
                                       * clause shares NOTHING with the
                                       * live set (0x47e40's arm 0x47edc,
                                       * the mode byte read from
                                       * [set+0xc] at 0x47e4b) */
        return (mask & live) == 0ull;
    return (mask & ~live) == 0ull;      /* mode 0, the plain prefix: the
                                       * clause must be a SUBSET of the
                                       * live set (the arm 0x47e63). So
                                       * plain K_ANY wants all six arrows
                                       * at once and plain K_NEVER wants
                                       * UP and DOWN held together: in
                                       * play, never - which is what an
                                       * author writing `WAIT K_NEVER`
                                       * means, and the mode, not the
                                       * name, is what would change it */
}

/* PR-333: the row step has TWO admission paths and the engine picks one
 * by asking the row node's test slot +0x8 (0x52235) and testing the
 * answer for EMPTY with 0x495b0 (0x5223c: it answers 12 when all three
 * data dwords are zero and 0 the moment one is not). The `test eax,eax
 * / sete al / and eax,0xff / je 0x522fc` at 0x52241-0x5224b sends the
 * EMPTY answer - a row with no `Wait` clause, or a bit-less one - to the
 * clock path: run the node's execute slot, and admit it only when the
 * authored threshold is BELOW the row clock, the difference carried
 * forward as the budget (0x52320-0x52334). A row whose clause carries
 * bits instead merges the caller's key set with the object's [obj+0x60]
 * (0x52251-0x522bc) and matches its clause against that at 0x522e6:
 * NO answer leaves the whole step through 0x522ed - no execute slot, no
 * cursor step, nothing consumed - and an answer takes `xor ebx,ebx` at
 * 0x522f8 and jumps to the run with a ZERO budget, so the threshold
 * compare at 0x52329 never runs and the clock is left at zero. `live`
 * is that merged set: this port keeps the object's own bits in p->sig /
 * o->sig and composes the keys onto them the way cond_true does. */
int ll_mov_row_admit(const ll_mov_fr *f, unsigned long long live, int walk,
                     unsigned *row_t, double *facc)
{
    if (ll_mov_row_gated(f)) {
        if (!ll_mov_row_gate(f, live))
            return 0;
        if (walk)
            *facc = 0.0;
        else
            *row_t = 0;
        return 1;
    }
    if ((walk ? *facc : (double)*row_t) < (double)f->thresh)
        return 0;
    if (walk)
        *facc -= (double)f->thresh;
    else
        *row_t -= f->thresh;
    return 1;
}

/* PR-88: name -> LL_SIG_* index. The table is the engine's own (name
 * block 0x919ec, bit constants from the comparison chain at 0x48000 -
 * see the ll_play.h enum comment). K_ANY / K_NEVER and their AC_ aliases
 * are NOT single bits there (the resolver answers them with WHOLE masks,
 * PR-263), and our cond_id already models them, so they return -1 here. */
int ll_sig_id(const char *name)
{
    static const struct { const char *n; int id; } tbl[] = {
        { "AC_BLOK", LL_SIG_AC_BLOK },        { "AC_BLOK_Y", LL_SIG_AC_BLOK_Y },
        { "AC_BREAK_NEXT", LL_SIG_AC_BREAK_NEXT },
        { "AC_BREAK_PREV", LL_SIG_AC_BREAK_PREV },
        { "AC_DUP", LL_SIG_AC_DUP },          { "AC_OFF", LL_SIG_AC_OFF },
        { "AC_DEADDUP", LL_SIG_AC_DEADDUP },  { "AC_ENEMY", LL_SIG_AC_ENEMY },
        { "AC_EDGE", LL_SIG_AC_EDGE },        { "AC_TIME", LL_SIG_AC_TIME },
        { "AC_DRAB", LL_SIG_AC_DRAB },        { "AC_DRABK", LL_SIG_AC_DRABK },
        { "AC_DRAB_CATCH", LL_SIG_AC_DRAB_CATCH },
        { "AC_DUP_CD", LL_SIG_AC_DUP_CD },    { "AC_STONE", LL_SIG_AC_STONE },
        { "AC_PUSH", LL_SIG_AC_PUSH },        { "AC_ENEMY_ATC", LL_SIG_AC_ENEMY_ATC },
        { "AC_RECT", LL_SIG_AC_RECT },        { "AC_ONAIR", LL_SIG_AC_ONAIR },
        { "AC_DRECT", LL_SIG_AC_DRECT },      { "AC_DRECT2", LL_SIG_AC_DRECT2 },
        { "AC_LINA", LL_SIG_AC_LINA },        { "AC_LINA_CATCH", LL_SIG_AC_LINA_CATCH },
        { "AC_LEAVE", LL_SIG_AC_LEAVE },      { "AC_ATAK", LL_SIG_AC_ATAK },
        { "AC_UP_DUP", LL_SIG_AC_UP_DUP },    { "AC_SPOKUJ", LL_SIG_AC_SPOKUJ },
        { "AC_BUTLA", LL_SIG_AC_BUTLA },      { "AC_BECZKA", LL_SIG_AC_BECZKA },
        { "AC_START", LL_SIG_AC_START },      { "AC_MRECT", LL_SIG_AC_MRECT },
        { "AC_TABLICZKA", LL_SIG_AC_TABLICZKA },
        { "AC_KILOF", LL_SIG_AC_KILOF },      { "AC_KOREK", LL_SIG_AC_KOREK },
        { "AC_KOLO", LL_SIG_AC_KOLO },        { "K_BKWD", LL_SIG_K_BKWD },
        { "K_FWD", LL_SIG_K_FWD },            { "K_UP", LL_SIG_K_UP },
        { "K_DOWN", LL_SIG_K_DOWN },          { "K_FIRE", LL_SIG_K_FIRE },
        { "K_ALT", LL_SIG_K_ALT },            { "K_F1", LL_SIG_K_F1 },
        { "K_F2", LL_SIG_K_F2 },              { "K_F3", LL_SIG_K_F3 },
        { "K_F4", LL_SIG_K_F4 },              { "K_F5", LL_SIG_K_F5 },
        { "K_F6", LL_SIG_K_F6 },              { "K_F7", LL_SIG_K_F7 },
        { "K_F8", LL_SIG_K_F8 },              { "K_F9", LL_SIG_K_F9 },
        { "K_F10", LL_SIG_K_F10 },            { "K_F11", LL_SIG_K_F11 },
        { "K_F12", LL_SIG_K_F12 },            { "K_JUMP", LL_SIG_K_JUMP },
        { "K_GEN1", LL_SIG_K_GEN1 },          { "K_GEN2", LL_SIG_K_GEN2 },
        { "K_GEN3", LL_SIG_K_GEN3 },          { "K_GEN4", LL_SIG_K_GEN4 },
        { "K_GEN5", LL_SIG_K_GEN5 },          { "AC_PLAT_OFF", LL_SIG_AC_PLAT_OFF },
        { "AC_PLAT_OUT", LL_SIG_AC_PLAT_OUT },
        { "AC_XCHG_LEVEL", LL_SIG_AC_XCHG_LEVEL },
        { "AC_SHOW_CROWN", LL_SIG_AC_SHOW_CROWN },
        { "AC_MSG_PUSH", LL_SIG_AC_MSG_PUSH },
        { "AC_WALK_UP", LL_SIG_AC_WALK_UP },  { "AC_WALK_DOWN", LL_SIG_AC_WALK_DOWN },
        { "AC_APPEAR", LL_SIG_AC_APPEAR },    { "AC_DISAPPEAR", LL_SIG_AC_DISAPPEAR },
    };
    for (unsigned i = 0; i < sizeof tbl / sizeof tbl[0]; i++)
        if (strcasecmp(tbl[i].n, name) == 0)
            return tbl[i].id;
    return -1;
}

static int cond_id(const char *tok)
{
    static const struct { const char *n; int id; } tbl[] = {
        { "K_FWD", LL_COND_K_FWD },
        { "K_BKWD", LL_COND_K_BKWD },
        { "K_UP", LL_COND_K_UP },
        { "K_DOWN", LL_COND_K_DOWN },
        { "K_GEN1", LL_COND_K_GEN1 },
        { "K_GEN2", LL_COND_K_GEN2 },
        { "K_GEN3", LL_COND_K_GEN3 },
        { "K_GEN4", LL_COND_K_GEN4 },
        { "K_GEN5", LL_COND_K_GEN5 },
        { "K_JUMP", LL_COND_K_JUMP },
        { "K_FIRE", LL_COND_K_FIRE },
        { "K_F1", LL_COND_K_F1 },
        { "K_F2", LL_COND_K_F2 },
        { "K_F3", LL_COND_K_F3 },
        { "K_F4", LL_COND_K_F4 },
        { "K_ALT", LL_COND_K_ALT },
        { "AC_TIME", LL_COND_AC_TIME },
        { "K_ANY", LL_COND_K_ANY },
        { "AC_BREAK_NEXT", LL_COND_AC_BREAK_NEXT },
    };
    for (unsigned k = 0; k < sizeof tbl / sizeof tbl[0]; k++)
        if (strcasecmp(tbl[k].n, tok) == 0)
            return tbl[k].id;
    return LL_COND_OTHER;
}

static void ctrl_flag(const char *tok, unsigned *f, int *other)
{
    static const struct { const char *n; unsigned bit; } tbl[] = {
        { "BY_TIME", LL_CTRL_BY_TIME },
        { "BY_X", LL_CTRL_BY_X },
        { "BY_Y", LL_CTRL_BY_Y },
        { "BY_TY", LL_CTRL_BY_TY },   /* PR-148: the mode-4 feed guard,
                                       * ll_play.c section 7 (the engine's
                                       * 0x5441b); BY_TX (wire mode 3) has
                                       * zero authored uses and still no
                                       * bit (PR-147's byte census). */
        { "CHECK_LN", LL_CTRL_CHECK_LN },
        { "CH_DIR", LL_CTRL_CH_DIR },
        { "CHECK_ALL_PLANE", LL_CTRL_CHECK_ALL_PLANE },  /* before CHECK_ALL */
        { "CHECK_ALL", LL_CTRL_CHECK_ALL | LL_CTRL_CHECK_ANI
                     | LL_CTRL_CHECK_ZW | LL_CTRL_CHECK_LN },
                                         /* PR-231 (read PR-230): the
                                          * engine's keyword 0x921c6
                                          * maps to 0x1c0 = LN|ANI|ZW
                                          * - CHECK_ALL IMPLIES the
                                          * CHECK_LN rule (and
                                          * ZARIA.01's lion [A_WALK]
                                          * uses bare CHECK_ALL) */
        { "CHECK_ANI", LL_CTRL_CHECK_ANI },
        { "CHECK_ZW", LL_CTRL_CHECK_ZW },
        { "CHECK_LEW", LL_CTRL_CHECK_LEW },
        { "FRAME_REV", LL_CTRL_FRAME_REV },
        { "SOLID", LL_CTRL_SOLID },
        { "SOLID2", LL_CTRL_SOLID | LL_CTRL_SOLID2 },
        { "SEMI_SOLID", LL_CTRL_SOLID | LL_CTRL_SEMI_SOLID },
        { "ANTI_SOLID", LL_CTRL_ANTI_SOLID },
        { "ANTI_POCISK", LL_CTRL_ANTI_POCISK },
        { "DANGER", LL_CTRL_DANGER },
    };
    for (unsigned k = 0; k < sizeof tbl / sizeof tbl[0]; k++)
        if (strcasecmp(tbl[k].n, tok) == 0) {
            *f |= tbl[k].bit;
            return;
        }
    /* Named in the ll_play.h vocabulary but without a bit (SEQ_REV_X,
     * INV_V, ...), or a name we have never seen:
     * counted so a missing behaviour is a known gap, never a silent one.
     * PR-99/100/101 read what SEQ_REV_X (bit 4) and SEQ_REV_Y (bit 5)
     * of the state's control word do in the engine: they are ROW-ORDER
     * flags, not motion. Each is only in force while an owner-side
     * guard holds ([[obj+8]+0x70]() == -1 for bit 4, [obj+0x71] == -1
     * for bit 5) and the row handler XORs the two, together with
     * `[obj+0x99] == -1`, to pick forward or mirrored row order
     * (0x5214b-0x52193, and 0x52360's mirror index `[[rec+0x2e]+8] -
     * row - 1`). Not modelled here: no shipped sample pairs them with a
     * row list whose order a video could tell apart. */
    if (other)
        (*other)++;
}

/* "a,b" -> two ints; scalar form ("G=400", seen in real data) duplicates
 * the value - the only scalar seen is G=400 in a scripted balloon state */
static void parse_pair(const char *v, int *a, int *b)
{
    char *end = NULL;
    *a = (int)strtol(v, &end, 10);
    *b = *a;
    if (end && *end == ',')
        *b = (int)strtol(end + 1, NULL, 10);
}

/* "A|!B|C" -> OR parts into tr->c (exact-match tokens, '|' separated;
 * an empty part from a leading '|' is skipped - seen as "A(66)=|K_ANY") */
/* PR-217: a `Wait`/condition token in NO table (LL_COND_OTHER with no
 * signal-table hit). LL_SIG_* indexes never collide with the LL_COND_*
 * ids (every LL_SIG_* value is >= 0 and the LL_COND_* enum starts at 0),
 * so the two share the -2 slot: it means "no bit to test", never a bit. */
#define LL_COND_NOBIT  LL_COND_OTHER   /* LL_SIG_NOBIT is in ll_play.h */

/* PR-264: `opp`, when given, takes the clause's OPERATION byte exactly as
 * the engine's clause parser writes it - 0x47d73 clears it, and a token
 * that begins with '!' writes 1 (0x47dd0-0x47dc8), '*' writes 2
 * (0x47dc2), '|' writes 3 (0x47dbc); every operator sets the byte for the
 * WHOLE clause, the last one seen wins, and the name after it folds into
 * the same mask. A ',' writes nothing (0x47dfd). The row-clause capture
 * passes NULL: it records tokens, not a match. */
static void parse_cond(ll_mov_gate *c, int *ncp, int max,
                       const char *expr, unsigned char *opp)
{
    const char *q = expr;
    int grp = 0;
    while (*q && *ncp < max) {
        const char *bar = strchr(q, '|');
        const char *bend = bar ? bar : q + strlen(q);
        /* PR-87: a bar segment is one AND-group; ',' splits its tokens
         * (see the ll_mov_tr.c comment). ';' inside stays inside one
         * token -> OTHER (counted, never true). PR-264: the groups are
         * kept for the capture only; what matches is mask + op. */
        for (;;) {
            const char *comma = strchr(q, ',');
            const char *end = (comma && comma < bend) ? comma : bend;
            while (q < end && *q == ' ')
                q++;
            int neg = 0;
            if (q < end && *q == '!') {
                neg = 1;
                if (opp)
                    *opp = LL_TOP_NOT;
                q++;
            } else if (q < end && *q == '*') {
                if (opp)
                    *opp = LL_TOP_EXACT;
                q++;
            }
            size_t n = 0;
            char tok[24];
            while (q + n < end && n + 1 < sizeof tok) {
                tok[n] = q[n];
                n++;
            }
            tok[n] = '\0';
            if (n) {
                c[*ncp].cond = cond_id(tok);
                c[*ncp].neg = neg;
                c[*ncp].grp = grp;
                /* PR-88: remember the signal-table index of names the
                 * model does not model as direct conditions (AC_ONAIR,
                 * AC_EDGE, ...): the evaluator tests them against the
                 * object's own signal set instead of never-true OTHER.
                 * PR-217: the SAME name is a BIT in the engine's signal
                 * table, so a modeled condition token gets its LL_SIG_*
                 * index too (K_DOWN -> 38); the transition evaluator never
                 * reads sig for those (its switch answers first), only the
                 * `Wait`-clause capture does. */
                c[*ncp].sig = (short)ll_sig_id(tok);
                if (c[*ncp].sig < 0)
                    c[*ncp].sig = (short)LL_SIG_NOBIT;
                /* PR-218: name the engine's three no-bit operands apart.
                 * Its comparator 0x48000 answers the name pairs
                 * AC_K_ANY/K_ANY (0x919ec/0x919f5) with the marker word
                 * 0x2001f8 and AC_K_NEVER/K_NEVER (0x919fb/0x91a06) with
                 * an EMPTY data set, and gives a name in neither table
                 * nothing at all - three different values, so the port
                 * keeps which one it parsed. */
                c[*ncp].gop = LL_GOP_NAME;
                if (c[*ncp].sig == (short)LL_SIG_NOBIT) {
                    if (!strcasecmp(tok, "K_NEVER"))
                        c[*ncp].gop = LL_GOP_NEVER;
                    else if (!strcasecmp(tok, "K_ANY") ||
                             !strcasecmp(tok, "AC_K_ANY"))
                        c[*ncp].gop = LL_GOP_ANY;
                    else
                        c[*ncp].gop = LL_GOP_NONE;
                }
                (*ncp)++;
            }
            if (end == bend)
                break;
            q = end + 1;
        }
        grp++;
        if (bar) {
            if (opp)
                *opp = LL_TOP_ANY;       /* 0x47dbc: the pipe is the
                                          * whole clause's operation */
            q = bar + 1;
        } else {
            q = bend;
        }
    }
}

/* PR-330: the engine reads one .MOV line through ONE character cursor.
 * The state-block reader 0x51910 builds that cursor in its prologue
 * (0x51926-0x51929: the slot at [ebp-0x1c] gets the stream vtable
 * 0x961fc) and every scan after it consumes from where the previous one
 * stopped - the `time` arm proves the chaining outright, it reads "%hi"
 * (0x9211e) into [st+0x8] and then ",%hi" (0x92122) into [st+0xa] of the
 * same line. Its first step is the token `" %[^,| \r\n\t]"` (0x92162),
 * compared with the plain byte compare 0x76d20 - NOT the case-folded
 * 0x76a69 that names states - against the field names time (0x92119),
 * sample, time_scale, seq (0x9213f), s (0x92146), control (0x9215a).
 * `s` is the one field name that does NOT consume the line: after its
 * name scan `" %[^ ,]%*[ ,]"` (0x92148) at 0x51a6a and the record
 * lookup/creation through the level object's record table at
 * 0x51a7b-0x51a95, the SAME arm reads the frame with "%d," (0x92156) at
 * 0x51a9a, calls the row ctor 0x515e0 (its own class word 0xa2638 is
 * set at 0x5160a) at 0x51ac1 with the CONSTANT 0x3e8 pushed as the
 * threshold
 * divisor - the ordinary row fill at 0x51f37-0x51f3e pushes the
 * state's [st+0xe] time_scale into that same slot, so an `s`-led row
 * keeps its authored units - and appends the node to the state's row
 * vector [st+0x2e] at 0x51ac9-0x51af7. Inside the ctor, 0x51660 reads
 * the units with "%hi" from that same cursor (so it lands on the
 * number AFTER the name) and scales them; the frame paths then store
 * the sequence record at [node+0x8] on all three of them (0x5169e for
 * an authored negative, 0x516cf for a positive, and for an authored 0
 * the record comes from 0x44110 at 0x516af-0x516c2), with [node+0xc]
 * the frame reduced by one (0x516d2, and 0x516a3-0x516a8 for a
 * negative) and [node+0x14] its sign flag (0x51696) - the PR-107
 * mapping is this port's copy of that pair. 0x516d5 then walks the
 * rest of the line as the row's directive chain through 0x45df0 like
 * any other row. So
 * `s idle 1,300,GEN K_GEN1` IS a row of A_FALL_DOWN: frame 1, 300 units,
 * a GEN K_GEN1 tail, drawn from the "idle" sequence. MEASURED over the
 * 777 extracted texts (the same scan that reproduces the 18617 rows the
 * older records quote): 206 such lines in 26 of them, 36 states, in two
 * written forms - `s idle 1,300` (164) and `s idle,1,200` (42, the comma
 * that same `%*[ ,]` scan eats). The lion's A_FALL_DOWN (ZARIA.01/.02,
 * their LEWPL twins, TUNGA.11/.12, KADMAR.07's 0_0, which also carries
 * it on A_LISC_END) and his A_TO_BOR (5 `s borring` rows with
 * VOL_MUSIC), KARA's A_PUT_BOMB (27), LONA's T_TELEPORT trio and
 * A_WSP_LIN_DOWN2 (42), ENA's A_BORING4 (24), the six KADMAR.07
 * ninjas' A_DEAD, the three SCHODY A_RUN and seven SADES.06 objects'
 * A_DEAD. The port read NONE of them: its test was the number pair at
 * the line head, so 36 states' row lists were short, and ZARIA.01's
 * A_FALL_DOWN(3) landed on the wrong row - the reason a landed lion
 * stays in the fall pose once the surface-link raiser of the next PR
 * makes that state reachable. The sequence record itself stays
 * captured-not-drawn: OI-120. */
int ll_mov_row_line(const char *line, const char **body, int *seqsw,
                    int *frame, int *units)
{
    const char *q = line;
    int sw = 0;
    int f, ms;

    while (*q == ' ' || *q == '\t')            /* every scan skips blank */
        q++;
    if (q[0] == 's' && (q[1] == ' ' || q[1] == '\t' || q[1] == ',')) {
        const char *n = q + 1;
        while (*n == ' ' || *n == '\t')
            n++;
        if (*n && *n != ',') {                 /* " %[^ ,]" = the name   */
            while (*n && *n != ' ' && *n != '\t' && *n != ',')
                n++;
            while (*n == ' ' || *n == '\t' || *n == ',')
                n++;                           /* ... and `%*[ ,]`        */
            q = n;
            sw = 1;
        }
    }
    if (seqsw)
        *seqsw = sw;
    if (sscanf(q, "%d,%d", &f, &ms) != 2)
        return 0;
    if (body)
        *body = q;
    if (frame)
        *frame = f;
    if (units)
        *units = ms;
    return 1;
}

int ll_mov_parse(const char *text, ll_mov_t *m)
{
    memset(m, 0, sizeof *m);
    const char *p = text;
    while (*p) {
        const char *eol = p;
        while (*eol && *eol != '\n' && *eol != '\r')
            eol++;
        size_t len = (size_t)(eol - p);
        char line[160];
        if (len && len < sizeof line) {
            memcpy(line, p, len);
            line[len] = '\0';
            if (line[0] == '[') {                     /* [STATE] opens block */
                if (m->nst < LMOV_MAXST) {
                    ll_mov_st *s = &m->st[m->nst++];
                    s->block_pos = m->nst - 1;    /* PR-235: the ds:0xceff8
                                                   * record ID counter */
                    const char *gt = strchr(line, ']');
                    size_t nl = gt ? (size_t)(gt - line - 1) : 0;
                    if (nl >= sizeof s->name)
                        nl = sizeof s->name - 1;
                    memcpy(s->name, line + 1, nl);
                    s->name[nl] = '\0';
                    s->time_scale = 1000;   /* PR-110: the engine's row */
                    s->scale_at = 0;        /* ctor passes 1000 when no */
                                            /* time_scale line exists   */
                } else {
                    m->drop_st++;
                }
            } else if (m->nst == 0) {
                /* before the first [STATE]: the declared state list.
                 * Its first name is the state the object starts in,
                 * and its ORDER is the engine's state index space:
                 * 0x53905 reads it into the vector [obj+0x3c] and
                 * every state name afterwards is resolved through
                 * 0x53440, which returns its 1-based position (see
                 * ll_mov_st.decl_idx, PR-216). strcasecmp because the
                 * engine's comparator 0x76a69 folds A-Z (0x76a75). */
                if (line[0] != ';' && line[0] != '/' && !strchr(line, '=')) {
                    char nm[24];
                    size_t k = 0;
                    while (line[k] > ' ' && k < sizeof nm - 1) {
                        nm[k] = line[k];
                        k++;
                    }
                    nm[k] = '\0';
                    if (nm[0]) {
                        if (!m->first_decl[0])
                            memcpy(m->first_decl, nm, sizeof m->first_decl);
                        if (m->nddecl < LMOV_MAXST) {
                            memcpy(m->decl[m->nddecl], nm, sizeof m->decl[0]);
                            m->nddecl++;
                        }
                    }
                }
            } else if (m->nst > 0) {
                ll_mov_st *s = &m->st[m->nst - 1];
                int f, ms;
                const char *body = NULL;
                int seqsw = 0;
                /* PR-330: the shared row-line test, including the `s
                 * <seq>` prefix the engine's cursor consumes before the
                 * frame field. The prefix is not part of the row, so the
                 * line is rewritten to what follows it and everything
                 * below parses an ordinary row; the sequence record the
                 * field names is counted on the state, never drawn
                 * (OI-120). */
                if (ll_mov_row_line(line, &body, &seqsw, &f, &ms)) {
                    /* frame line */
                    if (seqsw) {
                        memmove(line, body, strlen(body) + 1);
                        s->seq_switch++;
                    }
                    if (s->nfr < LMOV_MAXFR) {
                        ll_mov_fr *mf = &s->fr[s->nfr++];
                        mf->frame = f;
                        /* M4-C (FIX-13): non-positive thresholds would
                         * make obj_advance / the player spin forever:
                         * clamp. Positive author values (walk rows say
                         * 6 units) are kept as-is before the block's
                         * time_scale is applied below, exactly as the
                         * engine reads %hi into node+0x10 first
                         * (0x51679), matching the viewer's mov_parse. */
                        mf->thresh = ms > 0 ? (unsigned)ms : 100u;
                        /* PR-330: an `s`-led row is NOT scaled by the
                         * block's time_scale: the engine's `s` arm
                         * pushes the constant 0x3e8 into the ctor slot
                         * that the ordinary row fill feeds with the
                         * state's [st+0xe] (0x51ab7 vs 0x51f37). */
                        mf->no_scale = seqsw;
                        mf->dx = mf->dy = 0;
                        mf->gsig = 0;
                        mf->atc = 0;
                        mf->has_i = 0;
                        mf->has_f = mf->fx = mf->fy = 0;   /* PR-253 tails */
                        mf->has_fc = mf->fcx = mf->fcy = 0;
                        mf->has_pos = mf->posx = mf->posy = 0;
                        mf->lockcmd = mf->dircmd = 0;  /* PR-321 words */
                        mf->gotocmd = mf->gotoval = 0; /* PR-323 words */
                        mf->endlv = 0;
                        mf->gsig_id = -1;   /* PR-217: no `Wait` tail */
                        mf->gneg = 0;
                        mf->gop = LL_GOP_NAME;
                        /* A frame line may carry further fields. The ones the
                         * model acts on are the authored motion of THIS frame,
                         * "N,ms, M dx,dy" (285 of the 18823 shipped frame
                         * lines carry it - the population as PR-330's
                         * row test counts it, 18617 before that PR read
                         * the `s <seq>` lines; no `s`-led line carries an
                         * M tail, so the 285 is the
                         * same count either way; there are no standalone
                         * "M" lines), the
                         * `GEN <signal>` tails (PR-88: the engine's own
                         * directive table is at 0x917c0, the names it can
                         * raise are the 0x919ec block) and ATC_ON/ATC_OFF
                         * (PR-89, the attack-connect window).
                         * Everything else the table names (ground_on/off,
                         * chdir, the `lock2`/
                         * `unlock2` pair, the `zero`/`zero_t` word,
                         * F/IC, MOVE, the smp and vol sound
                         * words, inline F=) is counted -
                         * measured 1568 such lines before this PR took
                         * the GEN and ATC ones out of that count, PLAN
                         * 8g - never dropped in silence. `lock`/`unlock`
                         * and `ldir`/`rdir` joined the modeled set in
                         * PR-321 (kept in ll_mov_fr.lockcmd/dircmd): the
                         * parse and apply are PR-320's read (the apply
                         * 0x45c50 to 0x54f40/0x54f90 setting and clearing
                         * value 1 of [obj+0xb6], the applies
                         * 0x459f0/0x45a10 writing [obj+0x6d]); the
                         * census over the 777 texts is lock 126
                         * (3 LOCK), unlock 73, ldir 18 (1 LDIR), rdir 58
                         * (9 RDIR), and ZARIA.01 brackets BOTH of its
                         * exit states with them. `goto N`/`gotorel
                         * N`/`goto_rel N` joined it in PR-323/PR-324
                         * (kept in
                         * ll_mov_fr.gotocmd/gotoval): the arms
                         * 0x46739/0x4679c/0x4680c and the advance verbs
                         * 0x45d90/0x45dc0 are PR-322's read, the census
                         * is 135 lines (89 goto, 35 GOTO_REL family, 11
                         * GOTOREL) in 102 states, and the verbs act at
                         * the ROW CLOSE by rewriting the cursor, on the
                         * player clock (PR-323) and the scene-object
                         * one (PR-324) alike - the one thing the engine
                         * never clamps. `end_level`
                         * joined the modeled set in PR-199 once PR-198
                         * had read its apply: it is 26 lines in the
                         * whole shipped set (three LEW.MOV members), so
                         * this parser meets it in those three only.
                         * `F x,y` and `FC x,y` joined the modeled set in
                         * PR-253 (kept in ll_mov_fr.fx/fy and fcx/fcy):
                         * PR-250/PR-251 read the node factory's channel
                         * byte at [node+0x10] and the merge 0x52680 that
                         * routes `F`/`I` into the GATED (one-shot at row
                         * entry) 0x54170 pull pair and `FC` into the same
                         * pair UNGATED, every iteration the row stays
                         * current. `IC` (id 3, the force pair) stays
                         * counted but unmodeled. `pos x,y` joined the modeled
                         * set in PR-317 (kept in ll_mov_fr.has_pos/posx/posy and
                         * applied as an absolute set at the row admit): its parse
                         * arm 0x466d6 and its apply 0x45a30 -> the object's class
                         * +0x4 verb 0x23850 were read in PR-316, and it is 4 rows
                         * in 3 files - BOTH spellings are shipped: `pos 1020,692`
                         * in ZAR_LEW and ZAR_LEWPL [A_WYJSCIE] (the level's own
                         * exit, PR-309) and `POS 220,140` / `POS 200,140` in
                         * RAN.01 [A_DEAD] and [A_DEAD1] - so this parser meets it
                         * in those three only.
                         * `Wait`/`WAIT <signal>` joined the CAPTURED set
                         * in PR-217 (255 tails in 133 files): the clause
                         * is kept in ll_mov_fr.gsig_id/gneg and NO model
                         * acts on it yet, so those rows leave the counted
                         * total and the report names them as captured,
                         * not as unread. */
                        const char *fc = strchr(line, ',');
                        const char *rest = fc ? strchr(fc + 1, ',') : NULL;
                        if (rest) {
                            const char *q = rest + 1;
                            int other_directive = 0;
                            int captured_clause = 0;   /* PR-217 census */
                            while (*q) {
                                while (*q == ' ' || *q == ',')
                                    q++;
                                if (!*q)
                                    break;
                                char word[16];
                                size_t wn = 0;
                                while (*q && *q != ',' && *q != ' ' &&
                                       wn + 1 < sizeof word)
                                    word[wn++] = *q++;
                                word[wn] = '\0';
                                if (!wn)
                                    continue;
                                if (wn == 1 && (word[0] == 'M' ||
                                                word[0] == 'm')) {
                                    while (*q == ' ')
                                        q++;
                                    sscanf(q, "%d,%d", &mf->dx, &mf->dy);
                                    continue;
                                }
                                int digits = 1;
                                for (size_t k = 0; k < wn; k++)
                                    if (word[k] < '0' || word[k] > '9')
                                        digits = 0;
                                if (digits)        /* tail of the M pair */
                                    continue;
                                if (wn == 1 && (word[0] == 'I' || word[0] == 'i')) {
                                    int ix, iy;
                                    while (*q == ' ')
                                        q++;
                                    if (sscanf(q, "%d,%d", &ix, &iy) == 2) {
                                        mf->has_i = 1;
                                        mf->ix = ix;
                                        mf->iy = iy;
                                        while (*q && *q != ' ' && *q != ',')
                                            q++;       /* tail of the I pair */
                                        continue;
                                    }
                                }
                                if (wn == 1 && (word[0] == 'F' ||
                                                word[0] == 'f')) {
                                    /* PR-253: `F x,y` - channel id 0 of
                                     * the factory 0x46878, summed with the
                                     * row's `I` pair into the GATED
                                     * 0x54170 pull target (PR-250 (c),
                                     * PR-251 (c)). Kept as data; the
                                     * admit applies it. */
                                    int fx, fy;
                                    while (*q == ' ')
                                        q++;
                                    if (sscanf(q, "%d,%d", &fx, &fy) == 2) {
                                        mf->has_f = 1;
                                        mf->fx = fx;
                                        mf->fy = fy;
                                        while (*q && *q != ' ' && *q != ',')
                                            q++;   /* tail of the F pair */
                                        continue;
                                    }
                                }
                                if (wn == 2 && (word[0] == 'F' ||
                                                word[0] == 'f') &&
                                    (word[1] == 'C' || word[1] == 'c')) {
                                    /* PR-253: `FC x,y` - channel id 1,
                                     * the UNGATED pull: the merge feeds
                                     * the same 0x54170 pair every
                                     * iteration the row stays CURRENT. */
                                    int cx, cy;
                                    while (*q == ' ')
                                        q++;
                                    if (sscanf(q, "%d,%d", &cx, &cy) == 2) {
                                        mf->has_fc = 1;
                                        mf->fcx = cx;
                                        mf->fcy = cy;
                                        while (*q && *q != ' ' && *q != ',')
                                            q++;    /* tail of the FC pair */
                                        continue;
                                    }
                                }
                                if (strcasecmp(word, "pos") == 0) {
                                    /* PR-317: `pos x,y` - an ABSOLUTE
                                     * position set on the object that owns
                                     * the row. The engine's parse arm
                                     * 0x466d6-0x46718 allocates 0x10 bytes,
                                     * writes the class word 0xa11b0 at
                                     * [node+0x4] and reads the pair through
                                     * the format at 0x91944 into [node+0x8]
                                     * and [node+0xc]; its apply 0x45a30
                                     * takes the object out of the context
                                     * argument ([[ctx]+0x8] is its class
                                     * word) and calls that verb with
                                     * edx = [node+0x8], ebx = [node+0xc],
                                     * which for the lion classes
                                     * 0x96874/0x96960 is 0x23850: write
                                     * [obj+0x0] and [obj+0x4] (the position
                                     * the physics step and the ask protocol
                                     * both read) then call [class+0x54],
                                     * which recomputes the presented point
                                     * at [obj+0xd6] through the class +0xb0
                                     * provider. No carry, no clearance, no
                                     * signal: it bypasses 0x546a0. Measured
                                     * census of the 777 extracted texts: 4
                                     * rows in 3 files, and BOTH spellings
                                     * are shipped - `pos 1020,692` in
                                     * ZAR_LEW and ZAR_LEWPL `[A_WYJSCIE]`
                                     * (PR-309: the level's own exit) and
                                     * `POS 200,140` / `POS 220,140` in
                                     * RAN.01 `[A_DEAD1]`/`[A_DEAD]`, so the
                                     * name match is case-insensitive like
                                     * the other word arms here. */
                                    int px, py;
                                    while (*q == ' ')
                                        q++;
                                    if (sscanf(q, "%d,%d", &px, &py) == 2) {
                                        mf->has_pos = 1;
                                        mf->posx = px;
                                        mf->posy = py;
                                        while (*q && *q != ' ' && *q != ',')
                                            q++;   /* tail of the pos pair */
                                        while (*q == ',')
                                            q++;
                                        while (*q && *q != ' ' && *q != ',')
                                            q++;
                                        continue;
                                    }
                                }
                                if (strcasecmp(word, "lock") == 0) {
                                    /* PR-321: the one-word `lock` tail.
                                     * The engine's apply 0x45c50 binds the
                                     * node to its owner through 0x457e0 and
                                     * calls 0x54f40, `or BYTE PTR
                                     * [obj+0xb6],0x1` - value 1 of the
                                     * physics flags. `unlock` is 0x54f90,
                                     * `and ...,0xfe`; the pair brackets
                                     * ZARIA.01's two exit states (PR-320,
                                     * and the census: lock 126 / unlock 73
                                     * lines, 3 spelled LOCK). The model
                                     * acts at the position store, see
                                     * ll_player_step's integration gate. */
                                    mf->lockcmd = 1;
                                    continue;
                                }
                                if (strcasecmp(word, "unlock") == 0) {
                                    mf->lockcmd = -1;
                                    continue;
                                }
                                if (strcasecmp(word, "ldir") == 0 ||
                                    strcasecmp(word, "rdir") == 0) {
                                    /* PR-321: the direction-word writes.
                                     * Node classes 0xa0f58/0xa0f70, applies
                                     * 0x459f0/0x45a10: `mov DWORD PTR
                                     * [obj+0x6d],0xffffffff` / `0x1` - the
                                     * engine's OWN writes to the word the
                                     * x-side scalers read (0x54340,
                                     * 0x54bb2). 18 ldir (1 LDIR) and 58
                                     * rdir (9 RDIR) rows in the corpus;
                                     * A_WYJSCIE row 0 carries `ldir` right
                                     * after its `lock`. */
                                    mf->dircmd =
                                        (word[0] == 'l' || word[0] == 'L')
                                        ? -1 : 1;
                                    continue;
                                }
                                if (strcasecmp(word, "goto") == 0 ||
                                    strcasecmp(word, "gotorel") == 0 ||
                                    strcasecmp(word, "goto_rel") == 0) {
                                    /* PR-323: the cursor-jump tails. The
                                     * engine's arms 0x46739/0x4679c/0x4680c
                                     * read '%d' (0x91952) into [node+8]
                                     * through 0x49920: `goto` DECREASES the
                                     * stored number (0x46777-0x46780) - the
                                     * authored row number is 1-based, the
                                     * cursor [rec+4] is 0-based - while
                                     * gotorel and goto_rel dec-inc it back
                                     * (0x467dd, 0x467ea-0x467ee), so the
                                     * delta is kept verbatim and the two
                                     * names are aliases. The verbs
                                     * 0x45d90/0x45dc0 act ONLY at the row
                                     * advance (PR-322); the census: 135
                                     * lines, GOTO 64 / goto 25 / GOTO_REL
                                     * 29+2 / goto_rel 6 / GOTOREL 10+1 -
                                     * case twins again, so match like the
                                     * other words here. */
                                    int gv;
                                    while (*q == ' ')
                                        q++;
                                    if (sscanf(q, "%d", &gv) == 1) {
                                        if (strcasecmp(word, "goto") == 0) {
                                            mf->gotocmd = 1;   /* absolute */
                                            mf->gotoval =
                                                (short)(gv - 1);
                                        } else {
                                            mf->gotocmd = 2;   /* relative */
                                            mf->gotoval = (short)gv;
                                        }
                                        while (*q && *q != ' ' && *q != ',')
                                            q++;    /* tail of the number  */
                                        continue;
                                    }
                                }
                                if (strcasecmp(word, "GEN") == 0) {
                                    /* PR-69: `GEN <signal>` = the frame raises
                                     * that signal when it completes.
                                     * PR-83: `GEN AC_TIME` is the same
                                     * mechanism for AC_TIME (the real
                                     * ZARIA A_WALK idle chain: 11 such
                                     * lines across that script).
                                     * PR-88: ANY name in the engine's
                                     * signal table (0x48000) can be
                                     * raised - the shipped scripts do:
                                     * K_GEN1 315x, AC_PLAT_OFF 181x,
                                     * K_GEN3 139x, AC_TIME 60x ... The
                                     * bits land at their LL_SIG_*
                                     * indexes; a name the table has
                                     * (but at index >= 64, none in the
                                     * data) or no name at all stays a
                                     * counted, unmodeled directive. */
                                    while (*q == ' ')
                                        q++;
                                    char nm[24];
                                    size_t nn = 0;
                                    while (q[nn] &&
                                           ((q[nn] >= 'A' && q[nn] <= 'Z') ||
                                            (q[nn] >= 'a' && q[nn] <= 'z') ||
                                            (q[nn] >= '0' && q[nn] <= '9') ||
                                            q[nn] == '_') &&
                                           nn + 1 < sizeof nm) {
                                        nm[nn] = q[nn];
                                        nn++;
                                    }
                                    nm[nn] = '\0';
                                    int sid = nn ? ll_sig_id(nm) : -1;
                                    if (sid >= 0 && sid < 64) {
                                        mf->gsig |= 1ull << sid;
                                        q += nn;
                                        continue;
                                    }
                                }
                                if (strcasecmp(word, "ATC_ON") == 0) {
                                    mf->atc |= LMOV_ATC_ON;
                                    continue;
                                }
                                if (strcasecmp(word, "ATC_OFF") == 0) {
                                    mf->atc |= LMOV_ATC_OFF;
                                    continue;
                                }
                                if (strcasecmp(word, "end_level") == 0) {
                                    /* PR-199: `end_level <n>` - this row
                                     * ENDS THE LEVEL RUN. Kept as the
                                     * authored number (ll_mov_fr.endlv);
                                     * the row admit below is what puts
                                     * it in the level's exit-word slot,
                                     * the copy of the engine's apply
                                     * 0x45750, and the only reader of
                                     * that slot is the level loop's
                                     * compare against zero (0x4ac55),
                                     * which returns the loop code 1 for
                                     * ANY value. The number is therefore
                                     * never interpreted here, and a tail
                                     * with no number after it stays a
                                     * counted directive, never a guess.
                                     * The shipped spelling is lowercase
                                     * in all 26 rows (JUNCTION, TUNGA.11,
                                     * TUNGA2.12); strcasecmp matches the
                                     * other word arms of this parser. */
                                    int v = 0;
                                    while (*q == ' ')
                                        q++;
                                    if (sscanf(q, "%d", &v) == 1) {
                                        mf->endlv = v;
                                        continue;
                                    }
                                }
                                if (strcasecmp(word, "Wait") == 0) {
                                    /* PR-217: `Wait <signal>` - the row's
                                     * own signal clause, kept as DATA
                                     * (ll_mov_fr.gsig_id/gneg/gop) and
                                     * ACTED ON by ll_mov_row_admit since
                                     * PR-333. The arm mirrors the engine's
                                     * 0x46b62-0x46ba2: a 0x15-byte node
                                     * (class word 0xa11c8) whose operand
                                     * the clause parser 0x47d60 reads into
                                     * the node's 12-byte TEventSet, the
                                     * same ctor and parser the GEN arm
                                     * above uses (0x46af4, class 0xa11e0).
                                     * Measured over the 777
                                     * shipped texts: 255 tails in 133
                                     * files, 254 exactly one token, 22
                                     * `!`-negated; see the ll_play.h
                                     * field note. A tail with no token
                                     * (1 of 255: `Wait ;`) stays a
                                     * counted directive, never a guess. */
                                    char arg[64];
                                    size_t an = 0;
                                    const char *qa = q;
                                    while (*qa == ' ' || *qa == ',')
                                        qa++;
                                    while (*qa && *qa != ';' &&
                                           an + 1 < sizeof arg)
                                        arg[an++] = *qa++;
                                    arg[an] = '\0';
                                    while (an && (arg[an - 1] == ' ' ||
                                                  arg[an - 1] == ','))
                                        an--;
                                    /* PR-218: the trim must NUL-terminate
                                     * where it stopped. Measured: 2 of the
                                     * 251 captured rows are
                                     * `WAIT K_NEVER ;,smpauto` (SADES.06
                                     * BUTLA_BL/BK `[A_RUN]` row 10) and
                                     * the copy above stops BEFORE the `;`
                                     * but leaves its leading space in the
                                     * buffer, so the old code handed the
                                     * token `K_NEVER ` to the name
                                     * resolver and the row was counted as
                                     * naming a token in NO table. The
                                     * engine's clause parser 0x47d60 reads
                                     * its operand with the token scan at
                                     * 0x57d30 and never carries a space
                                     * into the name it compares at
                                     * 0x48000. */
                                    arg[an] = '\0';
                                    if (an) {
                                        ll_mov_gate g[4];
                                        int gn = 0;
                                        parse_cond(g, &gn, 4, arg, NULL);
                                        if (gn == 1) {
                                            mf->gneg = (short)g[0].neg;
                                            mf->gsig_id = g[0].sig;
                                            mf->gop = g[0].gop;
                                            captured_clause = 1;
                                            continue;
                                        }
                                    }
                                }
                                other_directive = 1;
                            }
                            if (other_directive)
                                s->directive++;
                            if (captured_clause)
                                s->wait_cap++;
                        }
                    } else {
                        m->drop_fr++;
                    }
                } else if (strncmp(line, "seq=", 4) == 0) {
                    const char *v = line + 4;
                    while (*v == ' ')
                        v++;
                    size_t k = 0;
                    while (v[k] && k + 1 < sizeof s->seqkey) {
                        s->seqkey[k] = v[k];
                        k++;
                    }
                    s->seqkey[k] = '\0';
                    if (strlen(m->seqkey) < sizeof m->seqkey - 1)
                        strcpy(m->seqkey, s->seqkey);   /* viewer compat */
                } else if (strncmp(line, "control=", 8) == 0) {
                    char tmp[128];
                    snprintf(tmp, sizeof tmp, "%.*s", (int)sizeof tmp - 1,
                             line + 8);
                    char *save = NULL;
                    /* '|' is the separator (1389 control lines); ',' appears
                     * in 9 of them, so both split. */
                    for (char *tok = strtok_r(tmp, "|, \t", &save); tok;
                         tok = strtok_r(NULL, "|, \t", &save))
                        ctrl_flag(tok, &s->ctrl, &s->ctrl_other);
                } else if (line[0] == 'F' && line[1] == '=' &&
                           strlen(line) > 2) {
                    parse_pair(line + 2, &s->fx, &s->fy);
                } else if (line[0] == 'T' && line[1] == '=') {
                    parse_pair(line + 2, &s->tx, &s->ty);
                } else if (line[0] == 'G' && line[1] == '=') {
                    parse_pair(line + 2, &s->gx, &s->gy);
                } else if (line[0] != ';' && strchr(line, '(') &&
                           strchr(line, '=')) {
                    /* transition: TARGET[(p)]=COND (all 2911 shipped ones
                     * carry the parentheses) */
                    if (s->ntr < LMOV_MAXTR) {
                        ll_mov_tr *tr = &s->tr[s->ntr++];
                        tr->tidx = -1;
                        tr->entry = 0;
                        tr->op = 0;              /* PR-264: the clause
                             * parser clears the operation byte first
                             * (0x47d73); an authored line with no
                             * operator keeps the default 0. */
                        const char *eq = strchr(line, '=');
                        const char *par = strchr(line, '(');
                        size_t n = 0;
                        const char *te = par && par < eq ? par : eq;
                        for (const char *q = line;
                             q < te && n + 1 < sizeof tr->target; q++)
                            tr->target[n++] = *q;
                        tr->target[n] = '\0';
                        /* PR-86: the numeric (...) = START ROW (1-based)
                         * in the target's frame list; non-numeric ("(x)")
                         * starts from the top like an absent one. See the
                         * ll_mov_tr.entry evidence comment. */
                        if (par && par < eq) {
                            const char *pe = strchr(par + 1, ')');
                            if (pe && pe < eq)
                                tr->entry = (int)strtol(par + 1, NULL, 10);
                        }
                        parse_cond(tr->c, &tr->nc, LMOV_MAXCON, eq + 1,
                                   &tr->op);
                    } else {
                        m->drop_tr++;
                    }
                } else if (strncmp(line, "time=", 5) == 0) {
                    /* PR-83 parsed this as counted-only; PR-163 MODELS
                     * the engine law (PR-159 F2/PR-160 F2-F3): `time=<min>
                     * [,max]` is the AC_TIME BOUNDS PAIR - ENTER rolls the
                     * gate uniformly in it (Borland rand), and same-state
                     * transition matches raise AC_TIME when the count
                     * reaches the roll; no line = never. The engine's
                     * scanf pair ('%hi' / ',%hi') leaves max=min when the
                     * comma is absent (the single-value `time=350` form,
                     * ENA.03_SURPRISE) - parse_pair does the same, so the
                     * form needs no special case HERE; what the engine's
                     * lerp computes with max=min stays its open item (the
                     * port's reading: gate=min, a deterministic N-match
                     * wait; PR-163 selftest pins it). Still counted so no
                     * report number moves. */
                    int t1 = 0, t2 = 0;
                    parse_pair(line + 5, &t1, &t2);
                    if (t1 > 0) {
                        s->time_lo = t1;
                        s->time_hi = t2;
                    }
                    s->field_other++;
                } else if (strncmp(line, "time_scale", 10) == 0) {
                    /* PR-110 IMPLEMENTS this, it used to be counted.
                     * The engine stores the value at rec+0xe (its row-
                     * node ctor 0x51660 takes it as an argument) and
                     * multiplies EVERY row threshold by it at parse
                     * time: 0x51679 `mov ebp,[esi+0x10]` (the %hi
                     * value just read), `imul edx,ebp`, 0x51683/
                     * 0x5168d `idiv 1000`, 0x5168f `mov [esi+0x10],
                     * eax` - `threshold = value * time_scale / 1000`,
                     * and only rows constructed AFTER the line saw it
                     * (PR-104's ctor-order reading). The census of all
                     * 777 .MOV: 116 active `time_scale=` lines in 38
                     * files, and not one row sits before its line, so
                     * scale_at records the position rule without
                     * changing any shipped result; a commented line
                     * (`;time_scale=`) never matches. The values run
                     * 60..4000. Applied to the block at the end of
                     * ll_mov_parse below. The value starts right after
                     * the '=' (the shipped form is `time_scale=700` and
                     * its 16 spellings, every one of them `=`-joined). */
                    const char *sv = strchr(line, '=');

                    s->time_scale = sv ? (int)strtol(sv + 1, NULL, 10) : 1000;
                    s->scale_at = s->nfr;
                } else if (line[0] != ';' && strchr(line, '=')) {
                    /* state payload fields the model does not act on yet:
                     * sample= (487, sound ids). Counted, never dropped in
                     * silence (PLAN 8g).
                     * PR-102 RE note, no behaviour change here: the
                     * engine stores `time=N,M` as two 16-bit halves at
                     * record +0x8/+0xa and `time_scale` at +0xe
                     * (parser 0x51910), and every row's threshold is
                     * then pre-scaled at parse time by
                     * `T * time_scale / 1000` (0x51660). PR-110 CORRECTS
                     * the census sentence PR-102 left here ("all 120
                     * shipped values are multiples of 100, 400..2000"):
                     * 120 lines is right, but 4 of them are commented
                     * out (`;time_scale=`), and the 116 active values
                     * run 60..4000, 26 distinct values, 78 of them
                     * BELOW 1000 - the shipped scale mostly makes rows
                     * SHORTER, and it is not all round hundreds (150,
                     * 550, 650, 750, 820, 850 all appear). See the
                     * time_scale= branch above, modelled in PR-110.
                     * So this counter is an unmodelled SCALE on frame
                     * timing, not just an unused field. */
                    s->field_other++;
                }
                /* the bare state names before the first '[' stay ignored,
                 * matching mov_parse; ';' lines are the game's own comment
                 * marker (774 of them, including an author note) */
            }
        }
        p = eol;
        while (*p == '\n' || *p == '\r')
            p++;
    }
    for (int k = 0; k < m->nst; k++) {
        ll_mov_st *s = &m->st[k];
        /* PR-110: apply the block's time_scale to its row thresholds at
         * parse time, as the engine does inside each row node's ctor
         * (0x51679 imul / 0x5168d idiv 0x3e8): only rows read AFTER
         * the `time_scale=` line are scaled (scale_at). KADMAR.07's
         * balloon: authored 200 with time_scale=2000 stores 400, i.e.
         * FOUR iterations per row, not two. PR-330 adds the one
         * exception the same ctor shows: a row the `s <seq>` field
         * opens is built with the constant 1000 (0x51ab7), so its
         * units are stored as authored - LONA.05's T_TELEPORT trio
         * (time_scale=600) is the only place in the shipped set where
         * that is observable, 37 rows. */
        for (int r = s->scale_at; r < s->nfr; r++)
            if (!s->fr[r].no_scale)
                s->fr[r].thresh = (unsigned)((long)s->fr[r].thresh *
                                             s->time_scale / 1000);
        for (int t = 0; t < s->ntr; t++)
            s->tr[t].tidx = ll_mov_find(m, s->tr[t].target);
    }
    if (m->first_decl[0]) {
        int k = ll_mov_find(m, m->first_decl);
        m->start_st = k > 0 ? k : 0;
    }
    /* PR-216: the engine's state index of each block, [obj+0x5c]
     * while it runs = 1 + the position of its name in the declared
     * list (0x53440 returns exactly that, 0 when the name is not in
     * the list, and the loader stores the block's record at that slot
     * of [obj+0x50]: 0x534b6). Block order is NOT that order: 172 of
     * the 777 shipped scripts differ. */
    for (int i = 0; i < m->nst; i++) {
        int k = 0;

        for (int d = 0; d < m->nddecl; d++)
            if (strcasecmp(m->decl[d], m->st[i].name) == 0) {
                k = d + 1;
                break;
            }
        m->st[i].decl_idx = k;
    }
    return m->nst;
}

/* ------------------------------------------------------ player sim ---- */

void ll_player_start(ll_player *p, const ll_mov_t *mov, double x, double y,
                     int grounded)
{
    memset(p, 0, sizeof *p);
    p->mov = mov;
    p->x = x;
    p->y = y;
    p->grounded = grounded;
    p->walkdir = 1;
    p->face = 1;
    /* PR-83 (user-reported, ZARIA.01 started crouched in state index 0):
     * the .MOV grammar's declared state list is authoritative for the
     * starting state - its first name (ll_mov_parse's start_st; measured
     * 2026-11-16 over all 13 player scripts: ZARIA.01 block index 0 is
     * A_JUMP_DOWN but its declared first name is A_WALK, ZARIA.02's is
     * A_SCIAG vs A_WALK, KARA's A_WSP_LIN vs A_WALK). A_STAY still wins
     * where it exists (RAN.01's pinned standing spawn), because that
     * script's declared first name (A_FLY_PTAKI1, a glider state) is not
     * a standing state; whether the original forces standing or starts at
     * the declared name is OPEN (recorded in HANDOFF). */
    int stay = ll_mov_find(mov, "A_STAY");
    if (stay >= 0)
        p->sti = stay;
    else if (mov->start_st > 0 && mov->start_st < mov->nst)
        p->sti = mov->start_st;
    else
        p->sti = 0;
}

/* PR-97: see the ll_player.contact note. The viewer calls this with the
 * shape-box bottom of the animation it is drawing, every tick. */
void ll_player_set_contact(ll_player *p, int below_pos)
{
    p->contact = below_pos > 0 ? below_pos : 0;
}

/* PR-264: the mask a compiled condition carries, exactly as the engine
 * composes it into the 21-byte entry (PR-261). The clause parser calls
 * the name resolver 0x48000 once per name (0x47de5); that resolver
 *   - answers a name IN the signal table with the /32,bit idiom OR-ing
 *     its single bit into the mask (0x48087),
 *   - answers K_ANY by CLEARING the 12 mask bytes and OR-ing 0x2001f8
 *     into dword 1 (0x48029-0x48048, PR-263): a REPLACE, not a union,
 *   - answers K_NEVER by OR-ing 0x60 into mask byte 4 = K_UP,K_DOWN
 *     (0x4806f-0x4807d): a union,
 *   - answers a name in NO table by writing nothing at all (the not-
 *     found return 0x49541, and the resolver's value is discarded at
 *     0x47de5): contributes NO bit.
 * A ',' folds its name into the same mask (0x47dfd). No shipped clause
 * mixes K_ANY with other names (counted over the 777 texts), so the
 * replace step is only observable in fixtures; it is implemented in
 * source order anyway, because that is what the bytes do. */
unsigned long long ll_mov_tr_mask(const ll_mov_tr *tr)
{
    unsigned long long m = 0;

    for (int i = 0; i < tr->nc; i++) {
        if (tr->c[i].gop == LL_GOP_ANY)
            m = LL_SIG_K_ANY_MASK;
        else if (tr->c[i].gop == LL_GOP_NEVER)
            m |= LL_SIG_K_NEVER_MASK;
        else if (tr->c[i].sig >= 0)
            m |= 1ull << (unsigned)tr->c[i].sig;
    }
    return m;
}

/* PR-264: the set a transition clause is matched AGAINST. The engine
 * composes it in the object step: 0x536f0 OR-s two built sets into
 * [obj+0x60] (0x53705-0x5370c, 0x53725) before it calls the pass that
 * matches and clears (0x53520, PR-262), and one of those builders is
 * 0x54910, whose entire output is the seven input-key bits plus nothing
 * else. So the live set is the object's own raised bits plus the keys it
 * is holding plus the arms its rows raised this iteration - and the
 * clause's mask is tested against THAT, not against a per-token answer.
 * PR-85 MODEL CHOICE, kept here where the bits are made (recorded,
 * evidence-based): K_FWD/K_BKWD are FACING-RELATIVE - K_FWD = the arrow
 * in the direction the lion faces, K_BKWD = the opposite arrow. DATA
 * PROOF: ZARIA's LEW.ZAR has exactly ONE walk state (A_WALK, entered
 * only by K_FWD from A_DRAP/A_WALK) and NO backward-walk state
 * anywhere; under literal-key semantics its lion could never walk left
 * at all (BKWD lines lead only into A_TURN, whose only exits are
 * AC_BREAK_* back to the hub). On video the player obviously walks left
 * (PR-72 measured the facing-left screen band 200-230). RAN's
 * A_WALK_FWD/A_WALK_BKWD pair is consistent too: entered
 * facing-relative, CH_DIR keeps face aligned, the pair is the same state
 * written twice for a right-facing spawn. K_UP/K_DOWN stay literal
 * (ladders/vines do not mirror). */
static unsigned long long live_bits_with(const ll_player *p,
                                         const ll_play_keys *k,
                                         unsigned long long sig)
{
    int face = p->face >= 0 ? 1 : -1;
    int fwd  = face  > 0 ? k->k_fwd : k->k_bkwd;
    int bkwd = face  > 0 ? k->k_bkwd : k->k_fwd;
    unsigned long long m = sig;
    if (fwd)          m |= 1ull << LL_SIG_K_FWD;
    if (bkwd)         m |= 1ull << LL_SIG_K_BKWD;
    if (k->k_up)      m |= 1ull << LL_SIG_K_UP;
    if (k->k_down)    m |= 1ull << LL_SIG_K_DOWN;
    if (k->k_fire)    m |= 1ull << LL_SIG_K_FIRE;
    if (k->k_alt)     m |= 1ull << LL_SIG_K_ALT;
    if (k->k_jump)    m |= 1ull << LL_SIG_K_JUMP;
    if (k->k_f1)      m |= 1ull << LL_SIG_K_F1;
    if (k->k_f2)      m |= 1ull << LL_SIG_K_F2;
    if (k->k_f3)      m |= 1ull << LL_SIG_K_F3;
    if (k->k_f4)      m |= 1ull << LL_SIG_K_F4;
    /* PR-441, from PR-438's re-read of the default flag builder
     * 0x54910: the vertical-intent word [obj+0xbe] feeds K_UP
     * (negative) and K_DOWN (positive) into the matched set
     * (0x5499d-0x549b4: byte 4 bit 0x20 = index 37 = K_UP, bit 0x40
     * = index 38 = K_DOWN; the enum names PR-224 fixed from the
     * engine's own key senders, and PR-264's K_NEVER census
     * confirms byte 4 bits 5/6 = K_UP/K_DOWN). PR-224's note "the
     * port has no signal side of that word at all" is CLOSED here.
     * The word is the INTENT, not the velocity register (PR-148
     * proved [obj+0xbe] is the y-intent written by the held Up/Down
     * keys at 0x549f0, and PR-326 feeds it to the y force at
     * 0x542cd; the register is +0xc6, a different field): a first
     * draft of this change tested p->vy instead and MOVED the
     * ZARIA.01 climb leg, whose A_DRAP carries `A_LOOKDOWN(1)=
     * K_DOWN` - at the drop the intent is zero while the model's
     * vy is positive, and the leg re-routed through A_LOOKDOWN.
     * The author's chain (PR-192 line: the climb goes A_DRAP ->
     * A_FALL_DOWN on AC_ONAIR with no key) says that is WRONG: the
     * flags follow the intent. With this port's intent sources (the
     * keys only, PR-148) the two flag lines below are EQUIVALENT
     * to the key flags above - no shipped leg changes - and the
     * structure is now the engine's. What would distinguish them,
     * a non-key intent (the contact-chain word, PR-149's kind-4
     * arm at 0x54b16, or the ladder's), is named at OI-016; no
     * authored clause in the 777 corpus separates the two today. */
    if (p->vert_intent < 0) m |= 1ull << LL_SIG_K_UP;
    if (p->vert_intent > 0) m |= 1ull << LL_SIG_K_DOWN;
    if (p->broke)     m |= 1ull << LL_SIG_AC_BREAK_NEXT;
    if (p->ac_time)   m |= 1ull << LL_SIG_AC_TIME;
    return m;
}

/* The set as it stands RIGHT NOW: the field the raises OR into plus the
 * keys of this iteration. This is what a compiled condition is matched
 * against (PR-264). */
static unsigned long long live_bits(const ll_player *p,
                                   const ll_play_keys *k)
{
    return live_bits_with(p, k, p->sig);
}

/* PR-264: and the match itself. The engine has ONE mask and ONE
 * operation per compiled condition (PR-261) and 0x47e40 tests them with
 * the four cases of PR-260; the OR-of-AND-groups reading of PR-87/PR-88
 * is gone, because the engine has no second node - the ',' and '|' of an
 * authored line differ only in which operation the clause carries. */
static int cond_true(const ll_player *p, const ll_play_keys *k,
                     const ll_mov_tr *tr)
{
    unsigned long long mask = ll_mov_tr_mask(tr);
    unsigned long long live = live_bits(p, k);

    if (!mask)
        return 0;   /* PR-88 convention kept: a clause whose names are
                     * in NO table compiles to an EMPTY mask, and the
                     * engine's subset arm would take it VACUOUSLY. The
                     * port never does (counted: no shipped clause has
                     * such a name; the hermetic `AC_BOGUS` legs pin
                     * this side). Unmodelled engine half, recorded. */
    switch (tr->op) {
    case LL_TOP_NOT:                        /* '!' : share NOTHING (the
                                            * 0x47edc arm) */
        return (mask & live) == 0ull;
    case LL_TOP_EXACT:                      /* '*' : byte-identical (the
                                            * 0x47fb2 arm; no shipped
                                            * clause writes it) */
        return mask == live;
    case LL_TOP_ANY:                        /* '|' : share ANYTHING (the
                                            * 0x47f42 arm) */
        return (mask & live) != 0ull;
    default:                                /* the default 0: the clause
                                            * is a SUBSET of the live set
                                            * (the 0x47e63 arm) - every
                                            * name it names is up */
        return (mask & ~live) == 0ull;
    }
}

/* PR-256: this port keeps the velocity in a double of px per second,
 * because that is what the legs, the logs and the camera model read.
 * The engine keeps it as a signed integer in hundredths of a px per
 * iteration ([obj+0xc2] x, [obj+0xc6] y), and at LL_ITER_MS = 50 the
 * two differ by exactly the factor PR-253 measured for the authored
 * velocity words (LL_PLAY_RAW_VDIV). The arithmetic of 0x54250 and
 * 0x54170 is done on the INTEGER register, so these two conversions
 * bracket every place that arithmetic runs. A velocity this port holds
 * is always that integer divided by LL_PLAY_RAW_VDIV, so the round trip
 * is exact. */
/* PR-518: THE ROW ADMIT IS THE PHYSICS STEP. Read live (pr516i,
 * named windows, PR-517) the engine's physics pass runs once per row
 * ADMIT, not once per iteration: between admits the position and BOTH
 * velocity registers sit EXACTLY still while the row accumulator keeps
 * running at the 20 Hz iteration rate; at an admit the registers step
 * (force + drag + G), the body moves register//100, and the ground
 * queries run. A_DRAP admits ~21/s (indistinguishable from per-
 * iteration), a named A_JUMP ~5/s, an authored one-row `0,100` list
 * exactly 10/s. PR-523 makes THIS the default physics pass. The
 * selftest's per-iteration pins and the real-level golden pins are
 * re-derived on the closed-form F/T/G arithmetic and the live cadence,
 * and the constant is moved to the keyboard's 20 with it (PR-522 showed
 * a walk cannot cold-start at all at s=10, so the corrected physics and
 * s=10 cannot both be the default). The hook now OPTS OUT:
 * LEO_PLAY_ROW_PHYSICS=0 restores the old per-iteration pass; unset or
 * any other value runs the row-admit law. LEO_PLAY_ROW_PHYSICS=1 still
 * means "on", as PR-518 documented. */
static int ll_play_row_physics(void)
{
    static int cached = -1;
    if (cached < 0) {
        const char *v = getenv("LEO_PLAY_ROW_PHYSICS");
        cached = (v && v[0] == '0') ? 0 : 1;   /* PR-523: default on */
    }
    return cached;
}

static long long ll_play_v_raw(double v)
{
    double a = v * (double)LL_PLAY_RAW_VDIV;
    return (long long)(a >= 0.0 ? a + 0.5 : a - 0.5);
}

static double ll_play_v_px(long long r)
{
    return (double)r / (double)LL_PLAY_RAW_VDIV;
}

/* PR-256: 0x54250's velocity step for one axis, byte for byte.
 * The order is the one in the body, and the truncations are the ones
 * its idiv instructions make (idiv truncates TOWARD ZERO):
 *   drag = idiv(T * v, 100)              0x5427f-0x542a7, from the OLD v
 *   v    = v + idiv(F*s, 20) [+ G]      0x542ab-0x542fa, added first
 *   v   -= drag                          0x54304 (x), 0x54311 (y)
 *   if (post_add * post_drag < 0) v = 0  0x5430a-0x54337: the sign test
 *                                        compares the value BEFORE the
 *                                        drag with the value AFTER it,
 *                                        not with the old v
 * The engine also adds G_x * [obj+0x6d] on the x side (0x542c1) and
 * scales the row FC pair by the same word (0x54340). PR-320 read
 * that word: a direction multiplier, set to 1 at every state start
 * (0x50442) and written -1/+1 by the `ldir`/`rdir` nodes
 * (0x459f0/0x45a10) and by command opcode 6 (PR-319). PR-321 models
 * the authored writes and scales the row-pull/FC x sides with them
 * (ll_play_dir_word). PR-520 measured the G_x term's role live - the
 * authored A_WALK `G=30,350` of ZARIA.01 supplies G_x = 30 and it rides
 * EVERY register step, including the one where the key is let go - so
 * ll_play_x_force below adds it, on the row-admit path only. The old
 * per-iteration model's pins predate the term and stay as they were. */
static long long ll_play_v_step(long long v, int t_pct, long long add)
{
    long long drag = (v * (long long)t_pct) / 100LL;
    long long post = v + add;
    long long out = post - drag;
    if (post * out < 0)
        out = 0;
    return out;
}

/* PR-253's row-pull law, on the register: 0x54170 is not an assignment.
 * From rest or the same sign it SETS the axis to the target (one step of
 * |target|, clamped exactly at it); from the opposite sign it CLIMBS by
 * |target| and never past; when the axis is already at or past the
 * target it does nothing (bytes at 0x54174-0x541e2, read end-to-end by
 * PR-253). The pair is the register's own unit: the authored `I`/`F`
 * numbers need no conversion here, which is what LL_PLAY_RAW_VDIV
 * documents. PR-83: only a target that lifts (ty<0) leaves the ground -
 * ZARIA's A_TURN `I 100,0` is a push along the floor, and lifting for it
 * used to kill the turn state in one tick. */
static void ll_play_pull(long long *v, long long t)
{
    if (t > 0) {
        if (*v >= t)
            return;
        long long v2 = *v + t;
        *v = v2 > t ? t : v2;
    } else if (t < 0) {
        if (*v <= t)
            return;
        long long v2 = *v + t;
        *v = v2 < t ? t : v2;
    }
}

static long long ll_play_dir_word(const ll_player *p)
{   /* PR-321: the x-side scale word, the engine's [obj+0x6d]. An
     * authored `ldir`/`rdir` write (applies 0x459f0/0x45a10, taken at
     * the row admit) wins outright, and every state start rewrote the
     * word to 1 (0x50442) - that default is p->dirword == 0 here,
     * "nothing authored in force". With none in force the PR-74/PR-82
     * walkdir multiply keeps its role as the stand-in for whatever
     * ELSE writes that word in the engine; no read says the key-walk
     * path does, and the estimate stays named at OI-015. */
    if (p->dirword)
        return p->dirword;
    return p->walkdir ? (long long)p->walkdir : 1;
}

/* PR-521: the x-side force the row-admit physics adds at 0x542ab-0x542fa.
 * PR-520 closed this law exactly over a live walk from a standstill:
 *   v += idiv(F*s, 20) + G_x - idiv(v*T_x, 100)
 * The first term is the intent force PR-135 shipped. The second, G_x, is
 * the authored `G=` word's FIRST number scaled by the direction word - the
 * engine's 0x542c1 multiplies [obj+0x6d] by it, and the header's engine
 * note already writes the increment as `F_eff*s/20 + G=*facing`. ZARIA.01's
 * authored A_WALK `G=30,350` supplies G_x = 30, and PR-520 showed it rides
 * EVERY register step: the key-release step of that window is 541 with the
 * word and 511 without it, so G_x is NOT key-gated. `with_gx` is the row-
 * admit gate (ll_play_row_physics): the default per-iteration model and its
 * pins predate this term, so a leg that runs with the gate unset keeps the
 * force PR-135 shipped, byte for byte. The value of s is the caller's - it
 * is the keyboard's word, NOT the register unit (OI-011 keeps that constant
 * at LL_PLAY_INTENT_FULL until the legs are re-derived). */
static long long ll_play_x_force(const ll_player *p, const ll_mov_st *st,
                                 int s, int with_gx)
{
    long long addx = ((long long)st->fx * (long long)s) / 20LL;
    if (with_gx)
        addx += (long long)st->gx * ll_play_dir_word(p);
    return addx;
}

static void apply_row_pull(ll_player *p, const ll_mov_st *st, int fi)
{   /* PR-253: when a row BECOMES CURRENT - on state entry or on the
     * admit that advances to it - the engine's merge applies the row's
     * summed `F`/`I` pairs to the velocity through 0x54170, and the gate
     * [obj+0x95] (set only by the row-admission tail 0x5234a, cleared at
     * the head of every state step 0x52217) makes that ONE SHOT at row
     * entry (PR-250 c). The `FC` pair of the same record is a different
     * channel: see section 3's unguarded call. */
    if (fi < 0 || fi >= st->nfr)
        return;
    const ll_mov_fr *fr = &st->fr[fi];
    if (!fr->has_i && !fr->has_f)
        return;
    long long tx = (fr->has_i ? (long long)fr->ix : 0)
                 + (fr->has_f ? (long long)fr->fx : 0);
    long long ty = (fr->has_i ? (long long)fr->iy : 0)
                 + (fr->has_f ? (long long)fr->fy : 0);
    long long vx100 = ll_play_v_raw(p->vx);
    long long vy100 = ll_play_v_raw(p->vy);
    /* The engine scales the X pair by the direction word [obj+0x6d]
     * (0x54340) - PR-320 read the setters, PR-321 models the authored
     * ones: `ldir`/`rdir` at 0x459f0/0x45a10 and the 1 written at
     * every state start (0x50442). With no authored word in force the
     * walkdir stand-in keeps answering: ll_play_dir_word. */
    ll_play_pull(&vx100, tx * ll_play_dir_word(p));
    ll_play_pull(&vy100, ty);           /* 0x54170, byte-for-byte */
    p->vx = ll_play_v_px(vx100);
    p->vy = ll_play_v_px(vy100);
    if (ty < 0) {
        p->grounded = 0;
        p->jumping = 1;
    }
}

/* PR-86: does this state declare an AC_BREAK_NEXT exit of its own? The
 * frame-list wrap arms that condition (PR-83), so such a state ENDS BY
 * ITSELF and the landing must not force it out; a state without one
 * (minimal fixtures) keeps the FALLBACK viewer rule below the jump. */
static int st_has_break_next_exit(const ll_mov_st *st)
{
    for (int t = 0; t < st->ntr; t++) {
        if (st->tr[t].tidx < 0)
            continue;
        for (int i = 0; i < st->tr[t].nc; i++)
            if (!st->tr[t].c[i].neg &&
                st->tr[t].c[i].cond == LL_COND_AC_BREAK_NEXT)
                return 1;
    }
    return 0;
}

/* PR-135: ONE STATE STEP + ONE PHYSICS PASS per frame-loop iteration
 * (PR-133's chain: the state-step chain dispatches the record once per
 * iteration and these families pass at most once). ll_player_step below
 * is only the clock: it feeds dt_ms into the iteration accumulator and
 * runs this body once per completed LL_ITER_MS, carrying the remainder
 * Bresenham-style exactly like PR-110's row clock did - so the port is
 * grid-independent (dt == LL_ITER_MS is one step; 2*50+16 is two; the
 * selftests pin both). */
static int player_iterate(ll_player *p, const ll_plt *ground,
                          const ll_play_keys *keys, double xmin, double xmax)
{
    const ll_mov_st *st = &p->mov->st[p->sti];
    int ev = 0;
    /* PR-331: was the object ATTACHED to a surface when this iteration
     * began? The engine's surface-link tail reads exactly this: [obj+0xa9]
     * before the query results are applied. */
    const int was_att = p->grounded;
    /* PR-333: the live set as this iteration's transition pass saw it.
     * The engine's row step merges the object's OWN set, [obj+0x60],
     * into the clause match (0x52290-0x522bc) - the same field the
     * surface-link tail OR-s AC_ONAIR/AC_DUP into (PR-331), and a set
     * nothing in our reading clears per iteration. This port clears
     * p->sig between the transition pass and the row pass as its
     * one-tick-pulse convention, so the row pass reads the set as of
     * that clear: one iteration after the raise, exactly the delay the
     * transition path already has. The exact clear points of
     * [obj+0x60] are unread (OI-018). */
    unsigned long long row_live = 0ull;
    /* PR-256: this body is 0x54250 + the row clock + the position step,
     * and every number in it is in that pass's units: the velocity
     * register's (a signed integer, hundredths of a px per ITERATION) and
     * whole px for the position. Nothing here scales by the caller's dt:
     * one call of this body is one iteration, which is what the caller's
     * accumulator guarantees. */

    /* PR-526: the class +0x90 verb, called on EVERY transition pass
     * before anything matches (0x5357d runs after the match walk at
     * 0x5356b and before the state-change applier, so [obj+0x5c] is
     * still the RUNNING state when the level's end-of-stage rule
     * looks at it - the ZARIA arm 0x1e250 tests that index and the
     * live set's AC_BREAK_NEXT bit, and 0x535a0 changes the state
     * only afterwards). The hook answers 1 to raise end_req, the
     * port's [obj+0x116]: the play loop's first guard (the
     * 0x4ac3b-0x4ac50 leg) then takes the loop's code 3. Nothing
     * here is ZARIA-specific on purpose: only the driver knows which
     * lion class its level's `lew=` named. */
    if (p->end_rule && p->end_rule(p->end_rule_ctx, p))
        p->end_req = 1;
    /* 1) transitions, in the order the real .MOV lists them (first match
     * wins; K_FWD/K_BKWD carry the facing change the CH_DIR flag names) */
    for (int t = 0; t < st->ntr; t++) {
        if (!cond_true(p, keys, &st->tr[t]))
            continue;
        int has_dir = 0;
        for (int i = 0; i < st->tr[t].nc; i++) {
            if (st->tr[t].c[i].neg)
                continue;
            /* PR-85: the conditions are facing-relative (see cond_true),
             * so a matched K_FWD names the arrow ALONG the current face
             * and K_BKWD the opposite one; walkdir is the literal key
             * direction (that is what physics and impulses need), and
             * CH_DIR below re-aligns the draw with it. */
            int fd = p->face >= 0 ? 1 : -1;
            if (st->tr[t].c[i].cond == LL_COND_K_FWD) {
                p->walkdir = fd;
                has_dir = 1;
            } else if (st->tr[t].c[i].cond == LL_COND_K_BKWD) {
                p->walkdir = -fd;
                has_dir = 1;
            }
        }
        /* PR-83: a BY_X state whose CURRENT state was reached (or kept)
         * through a direction condition walks on the direction key (below);
         * one reached through anything else (K_GENn/AC_*: the cutscene
         * walkers) walks on its own. Re-evaluated every tick a transition
         * matches, self-targets included - the self-target line of a held
         * walk state ("A_WALK(x)=K_FWD") re-arms the key-gated mode. */
        p->auto_walk = !has_dir;
        /* A transition that targets the state it is written in is REAL
         * data (KADMAR2's marching state: "A_MASZERUJ(x)=K_FWD") and
         * means KEEP this state: re-entering it would restart the frame
         * list every tick (measured: the walk froze on frame 1 while the
         * lion slid along at the state's speed). So a self-target leaves
         * the STATE running untouched - no ENTER, no impulse, no event -
         * but PR-215 ships the one thing the engine's arm DOES do: it
         * writes the authored start row into the CURRENT state's row
         * counter. The applier's same-index entry (0x53602-0x53626,
         * re-read by PR-174 for the object side) opens with exactly
         * that - `mov esi,[eax+0x4]` (0x5360f), the skip-if-equal at
         * 0x53612, `mov [eax+0x4],edx` (0x53616) - and PR-172 settled
         * that this arm inlines ONLY the row write: the row feed and
         * its carry [obj+0xa1] survive the match (p->row_t, p->facc
         * untouched). The value is the raw authored one and the shared
         * ll_mov_row_start mapping is the engine's observable
         * behaviour, absent token and over-range included (PR-107).
         * WHY THIS IS THE ZARIA HALF: PR-174's own census counted the
         * 101 authored self-target seeks with entry >= 2 as 77
         * LION-class and 24 scene-object lines, and all 20 of
         * ZARIA.01's are in the lion's own scripts (A_BORRING(10),
         * A_DEAD(33), A_JUMP(11)=AC_DUP, A_KILOF(13)/(25)...) - the
         * object-side fix could not reach one of them. OI-092.
         * (Note the (...) token takes non-numeric values too: "(x)" -
         * PR-86 decoded the NUMERIC one as the start row, see
         * ll_mov_tr.entry. An ABSENT or non-numeric token is the
         * engine's -1 and the row-set method refuses a negative value
         * (0x52381): NO seek. PR-215's census settles it: 19 of the
         * 22 authored self-target lines that a HELD direction key
         * re-matches every iteration carry no numeric token, and the
         * video-measured walk gait does not restart its row list.) */
        if (st->tr[t].tidx == p->sti && st->tr[t].entry > 0)
            p->fi = ll_mov_row_start(&p->mov->st[p->sti],
                                     st->tr[t].entry);
        if (st->tr[t].tidx >= 0 && st->tr[t].tidx != p->sti) {
            p->sti = st->tr[t].tidx;
            const ll_mov_st *nt = &p->mov->st[p->sti];
            p->fi = ll_mov_row_start(nt, st->tr[t].entry);
                                     /* PR-107: the shared rule, now the same
                                      * one the scene objects use */
            p->row_t = 0;
            p->facc = 0.0;             /* PR-110: a state change zeroes
                                        * both feeds of the accumulator,
                                        * as obj_transition does */
            p->atc = 0;                /* PR-89: an attack window belongs *
                                        * to the state that opened it.    */
            p->atc_hit = 0;
            p->lock = 0;               /* PR-321: the state start clears  */
            p->dirword = 0;            /* value 1 of [obj+0xb6] - `and    */
                                       /* ah,0xc0` in 0x54e00 - and        */
                                       /* rewrites [obj+0x6d] to 1         */
                                       /* (0x50442); 0 here is that        */
                                       /* rewrite: no authored value in    */
                                       /* force.                           */
            ev |= LL_PLAY_EV_STATE;
            apply_row_pull(p, nt, p->fi);
        }
        break;
    }
    st = &p->mov->st[p->sti];
    /* PR-82: CH_DIR ("change direction") is the flag whose meaning PR-81
     * pinned for scene walkers - the DRAW turns with the movement
     * direction (video + art evidence there). Every player state that can
     * go either way carries it (measured: A_WALK_FWD/A_WALK_BKWD yes, with
     * all-positive frame lists; the direction-baked A_GO_TO_BALON_L no,
     * with its frames authored negative). So: while a CH_DIR state runs,
     * facing follows walkdir; states that bake their direction into the
     * frame sign never touch it, and facing stays as it was (the lion who
     * walked left and stopped in A_STAY - no CH_DIR - keeps facing left;
     * the .MOV data has no turn animation for that, A_TURN belongs to
     * SEQ_REV_X turns, not to stopping). */
    if (st->ctrl & LL_CTRL_CH_DIR)
        p->face = p->walkdir < 0 ? -1 : 1;
    row_live = live_bits_with(p, keys, p->sig);      /* PR-333, above  */
    p->sig = 0;                                      /* one-tick pulses */
    p->broke = 0;
    p->ac_time = 0;

    /* PR-518: THE PHYSICS PASS RUNS AT ROW ADMITS. The live trace
     * (pr516i, NAMED windows, PR-517) is unambiguous: between row
     * admits the position and BOTH velocity registers sit EXACTLY
     * still while the row accumulator keeps running at the 20 Hz
     * iteration rate; at an admit the registers step (drag + force +
     * G), the body moves register//100, and the ground queries run -
     * A_DRAP admits ~21/s (physics indistinguishable from per-
     * iteration), a named A_JUMP ~5/s (its authored row list), an
     * authored one-row `0,100` list exactly 10/s. So sections 2-6
     * below are gated on this iteration's admit, and the row clock
     * runs FIRST: its time-mode feed and the BY_X feed from the
     * PERSISTED step word [obj+0xca] (row_dx) belong to the
     * iteration loop, exactly the 0x543a0 dispatcher's shape - feed
     * every iteration, admit test every iteration, physics only when
     * the admit answers. The y-intent word is stored here too (the
     * engine writes [obj+0xbe] every frame loop, 0x549f0, and the
     * BY_TY feed gate reads it), and a state with NO row list keeps
     * physics per iteration: with no rows there is no admit, a
     * literal reading would freeze the body, and that degenerate
     * case is the nfr==0 item already open at OI-015. */
    p->vert_intent = keys->k_up ? -LL_PLAY_INTENT_FULL
                                : (keys->k_down ? LL_PLAY_INTENT_FULL : 0);
    int row_admit = 0;
    int row_park = 0;                  /* PR-523, the clause case below  */
    if (ll_play_row_physics() && st->nfr > 0) {
        int walk = (st->ctrl & LL_CTRL_BY_X) && st->fx != 0;
        int by_ty = (st->ctrl & LL_CTRL_BY_TY) && !p->vert_intent;
        if (walk)
            p->facc += p->row_dx < 0 ? -(double)p->row_dx
                                     : (double)p->row_dx;
        else if (!by_ty)
            p->row_t += 100;           /* one iteration = exactly one
                                        * feed of the engine's constant
                                        * (0x4c8b7), PR-110 */
        row_admit = ll_mov_row_admit(&st->fr[p->fi], row_live, walk,
                                     &p->row_t, &p->facc);
        /* PR-523: A CLAUSE THAT REFUSES DOES NOT GATE THE BODY. A row
         * whose clause answers NO leaves the row step through 0x522ed:
         * no execute slot, no cursor step, nothing consumed - the list
         * PARKS there (PR-333's reading, pinned by the A_DOWN and
         * A_DOSTAL selftest legs for the cursor half and by the PR-523
         * A_PARK leg for the body half). Read literally as the physics
         * gate
         * too, that parks the BODY: ZARIA.01's real [A_JUMP] row 10 is
         * `8,0,WAIT AC_DUP`, the AC_DUP being the surface link this
         * state is falling TOWARD, so with physics gated on the admit
         * the registers freeze at y 586 / vy -747 on tick 59 and the
         * surface that would raise AC_DUP is never reached - the lion
         * hangs in the air for the rest of the run. 0x522ed is the ROW
         * path's exit; nothing in it stops the motion path, and the
         * same "no row machine to gate" case is already made for a
         * state with no row list at all (the nfr==0 arm below,
         * OI-015). So a parked row runs the ordinary physics pass, and
         * the row cursor stays parked exactly as PR-333 pins it: the
         * walk states the PR-518 capture measured (distance-fed, never
         * clause rows) are untouched by this arm. */
        if (!row_admit && ll_mov_row_gated(&st->fr[p->fi]) &&
            !ll_mov_row_gate(&st->fr[p->fi], row_live))
            row_park = 1;
    }

    /* HISTORICAL RECORD OF THE FITTED MODEL (the model in force from
     * PR-79 until PR-135; the code that implements the engine's own law
     * is the block below, marked "2) horizontal physics: the ENGINE
     * LAW"). It is kept here because the paragraphs below carry the RE
     * addresses and the old measurements, and PR-98/99's corrections of
     * each other are only readable in order.
     *
     * 2) horizontal physics from the real F=/T= numbers: approach the
     * state's target velocity (BY_X states walk at +/-T; everything else
     * brakes toward 0 at its own F - A_STAY F=600 stops hard, matching
     * the real data).
     * PR-83 (user-reported: ZARIA kept walking forever after one press):
     * a key-driven BY_X state - one entered/kept through K_FWD or
     * K_BKWD - only has its target velocity while the matching key is
     * actually held. ZARIA's A_WALK is the game's combined idle+walk
     * state (it has NO !K_FWD exit anywhere; its F=100,0 is the authored
     * brake-to-standstill); RAN never reaches this gate at key release
     * because its !K_FWD transition to A_STAY wins the same tick.
     * auto_walk states (K_GEN1 cutscene walkers: GO_TO_BALON_*, EXIT,
     * CHANGE_LEVEL) keep walking with no key held.
     * PR-98 (RE, no behaviour changed): the ENGINE does not parameterize
     * these two numbers this way. Its velocity step at 0x54250 is, per tick,
     *   v -= state T= * v / 100      (a PERCENT drag, computed from the OLD
     *                                 v first; a sign flip across the
     *                                 update sets the component to 0;
     *                                 0x54266 also floors the x drag at
     *                                 20, and the .MOV parser caps the
     *                                 authored T= at 130 - 172 shipped
     *                                 components are above that cap)
     *   v += (state F= + that row's `FC` pairs, + its `IC` pairs when
     *         obj+0x95 is set) * [obj+0xba|be] / 20 + state G= * facing
     * and separately that row's `F`/`I` pairs (summed at 0x526df..0x5273d)
     * are a velocity TARGET: 0x54170 moves v to it and never past it (from
     * rest or the same sign it is a SET, from the opposite sign it climbs by
     * the target per tick until it lands on it), and it runs AFTER the drag,
     * so a row that carries a target pins that axis for as long as it runs.
     * x is multiplied by facing; y is not, and y is DOWN-positive there -
     * the jump rows aim `F 0,350` (falling) and `I 150,-1200` (upward).
     * So F= is a sustained force, T= is a drag, and `FC` is load-bearing:
     * ZARIA's A_FIGHT header is `F=0,0` and its rows carry `FC 100,0`,
     * `FC 90,0`..., i.e. without FC the punch has no authored motion at all.
     * NOT APPLIED then, deliberately: PR-98 and PR-99 carry the detail,
     * and the next lines are the reason. [APPLIED IN PR-135 - what was
     * missing here was the measurement the paragraph below asks for, a
     * BY_X object at T other than 40 on camera; PR-133/134 supply it.
     * See the engine-law block below and NEXT_STEPS PR-135.] That
     * loop's steady state is v_eq = 5*F_eff*s/T, so px/s =
     * F_eff*s*fps/(20*T) - a speed that depends on T, on the terrain
     * scalar s and on a tick rate this port does not run at (60 ms) -
     * while the walks this repo measured appeared to track F alone: 25
     * px/s for the lion's A_WALK (F=100,T=20), 25 px/s for the worm's
     * BY_X A_RUN (F=100,T=40), 40-50 px/s for the rhino (F=180,T=40);
     * PR-99 corrects what that proves, below. Note also that
     * the units half of the question may already be answered by this
     * file's own fitted constants: v is hundredths of a px per tick (the
     * /100 at 0x5435d), and at 25 such ticks/s an `I 150,-1200` pair is
     * (37.5, -300) px/s - "exactly what LL_PLAY_JUMP_DIV=4 below
     * produces" as this file said until PR-253 replaced that fit with
     * the measured /5 (LL_PLAY_RAW_VDIV), which is the same reading of
     * the same /100 register at THIS port's 20 iterations/s.
     * What is now settled, from the image itself (0x52590 parses the
     * header keys): `F=` = rec+0x50/+0x54, `T=` = +0x58/+0x5c, capped by
     * that parser at 130, `G=` = +0x48/+0x4c - and the merge copies
     * exactly those, so the header F= feeds ONLY the increment channel
     * (0x54170's target comes from the row-node sums, never from the
     * header). That much stands. What does NOT stand is PR-98's word
     * "refuted" - PR-99 corrects it (NEXT_STEPS PR-99, CORRECTION): the
     * two OBJECT samples share T=40, where this law and the F-only law
     * differ only by the constant PR-79 fitted as /4, and the T=20
     * sample is the lion, whose path through 0x54250 is unverified.
     * Worked the other way (s=10, 20 ticks/s) the same algebra lands on
     * both object samples AND their per-row distances. PR-99 also read
     * the `control=` dispatch, PR-98's open item 2: it is 0x543a0 (the
     * address PR-99 printed as 0x543a1; PR-100 corrects it from the
     * aligned prologue and the method-table entries at slot +0xa0) with
     * the jump table at 0x54380, mode 0 is BY_X and its branch feeds
     * the row distance accumulator [obj+0xa1] with [obj+0xca], i.e.
     * |v_x|/100 from the loop above. PR-100 has that feed's owner: the
     * state record's step at 0x521f0 calls this dispatcher as slot
     * +0xa0 of its owner object's method table, then compares the
     * current row's authored threshold with [obj+0xa1], admits at most
     * one row per step and hands the remainder to the row handler -
     * the same shape obj_walk/obj_advance have, with a different
     * per-tick feed. There is no separate position
     * mechanism, and PR-98's other escape (re-reading the header `F=`
     * as 0x54170's velocity target) stays closed: that target is built
     * only from the row-node sums. So this model stayed as it was: the
     * samples on hand did not decide between the two laws, and the
     * measurement that would is a BY_X object at T other than 40,
     * which the authored data supplies in quantity but ZARIA's footage
     * did not. [RESOLVED IN PR-133/134: that footage does put a BY_X
     * worm at T=40 and a lion stand-in at T=20 on camera, and their
     * rates decide in favour of this law; PR-135 applies it.] */
    /* 2) horizontal physics: the ENGINE LAW, one pass per iteration
     * (PR-135; the RE that describes it is PR-98's, quoted in the
     * historical note below and confirmed byte-for-byte by PR-129/132/
     * 133: the velocity step 0x54250 is, per iteration,
     *   v -= state T= * v / 100      (a PERCENT drag computed from the
     *                                 OLD v; T is the engine's drag
     *                                 percent word and 0x52590 clamps
     *                                 it at PARSE into [20,130]; a sign
     *                                 flip across the update sets the
     *                                 component to 0 - 0x5426x)
     *   v += (state F= + that row's FC pairs ...) * s / 20
     * with v in hundredths of a px per iteration and s the intent
     * factor at [obj+0xba], |s| <= 10, s = +/-10 exactly at a full
     * one-axis intent (PR-132, CLOSED). Steady state is therefore
     * v_eq = 5*F*s/T hundredths/iteration = F*s*1000/(20*T*LL_ITER_MS)
     * units/s - 50 u/s for ZARIA's [A_WALK] (F=100,T=20,s=10) and
     * 25 u/s for the worm's (F=100,T=40), inside PR-134's measured
     * brackets [38,76] and <=30.3 on the verified 50 ms clock.
     * The OLD model (PR-79/83: a target velocity of +/-T capped by a
     * clamp and an F-as-brake accel limit) is REPLACED here: T is the
     * drag, not a velocity cap, and the brake on key release IS the
     * drag with s = 0 (A_STAY's T=90 stops in a few iterations;
     * A_WALK's T=20 glides - the engine has no authored hard brake
     * for it). The row-node `F I` velocity-TARGET channel (0x54170)
     * and the FC sums were UNMODELED here until PR-253 read the verb
     * end-to-end and applied them (the `IC` force pair still is; 0x54170's
     * target is built only from the row-node sums - PR-98/99).
     * PR-98/99's full historical note, kept where it was written:
     * the merge copies the header F=/T=/G= into the record fields
     * [obj+0x50..0x5c] and the header F= feeds ONLY the increment
     * channel; the samples that once "refuted" the F/T law all share
     * T=40, where it and the F-only fit differ only by the constant
     * PR-79 fitted as /4.
     * PR-83 (user-reported: ZARIA kept walking forever after one
     * press): a key-driven BY_X state only gets its intent while the
     * matching key is held; auto_walk states (K_GENn cutscene
     * walkers: GO_TO_BALON_*, EXIT, CHANGE_LEVEL) walk with s at
     * full intent and no key. */
    int key_walk = p->walkdir > 0 ? keys->k_fwd : keys->k_bkwd;
    int s = ((st->ctrl & LL_CTRL_BY_X) && (key_walk || p->auto_walk))
                ? (p->walkdir < 0 ? -LL_PLAY_INTENT_FULL
                                  : LL_PLAY_INTENT_FULL)
                : 0;                       /* no key held: the engine's
                                            * [obj+0xba] intent IS zero,
                                            * so no force is added and
                                            * the drag does the braking */
    /* PR-523: THE INTENT-EDGE REGISTER-SEED. A BY_X walk entered at a
     * standstill deadlocks on the literal row-admit reading: its
     * accumulator feeds |idiv(v,100)|, which is 0 at v=0, so no row
     * ever admits and no physics ever runs. PR-522 measured what the
     * engine does instead, at every cold start in pr516i: the moment
     * the intent word [obj+0xba] goes non-zero, the registers hold for
     * a few frames (gate_95 armed) and then EXACTLY ONE register step
     * runs - a full idiv(F*s,20)+G_x, drag 0 for the tiny v - that is
     * NOT an accumulator admit (the accumulator is under the authored
     * threshold there), flips gate_95, and moves the body at the new
     * register//100; the row cursor and the accumulator are otherwise
     * left alone, and from there the walk's own v//100 feed reaches the
     * threshold and the per-admit loop begins. The trigger in the
     * capture is the INTENT EDGE, not the state transition: every cold
     * window shows both (the real state graph reaches a walk through a
     * transition, which also writes the word), but the warm re-presses
     * (t=341.49, 341.68: key 0->20 mid-walk, no state change) show the
     * same event keyed on the word alone. The same edge is what the
     * authored self-target grammar (`A_WALK(x)=K_FWD`, a held key in a
     * running state) produces, and a walk that resumes from a stand-
     * still inside its own state is the one thing that must not freeze.
     * So the port arms the seed on the edge of ITS intent word - the
     * first iteration the state's s is non-zero - and runs the ordinary
     * physics pass that iteration even when the accumulator would not
     * admit. No extra pass is ever run: if the same iteration is also
     * an admit, the one pass serves both, exactly as t=341.49 shows
     * (one step, the threshold consumed by the admit). The row cursor
     * stays gated on the admit (section 7), the threshold is NOT
     * consumed by the seed, and the engine's 0.06-0.16 s hold before
     * the step is NOT reproduced - the seed runs on the edge iteration,
     * the hold's writer being the unread half of OI-013.
     * THE REGISTER-ZERO RESTART, the same seed on its other edge. A
     * reversal taken at full speed zeroes the register on the engine's
     * OWN sign-cross rule (RAN.01's authored pair, F=400 against T=90:
     * 445 - idiv(400*20,20) - idiv(445*90,100) crosses zero, and
     * 0x5430a-0x54337 answers ZERO, PR-85's reading standing). At v=0
     * the BY_X feed is 0, no row ever admits again, and the literal
     * row-admit gate would freeze the lion standing mid-run - the
     * cold-start deadlock of PR-522 arriving by a different road, and
     * PR-522's evidence answers it the same way: with the intent word
     * non-zero the real register STEPS (the A_WALK windows step every
     * frame), so a held intent against a resting register restarts
     * exactly one pass, and from there the walk's own feed resumes.
     * This is the same one-pass seed, not an extra pass: the capture
     * names the register step, not the row admit, at those frames. */
    int intent_edge = s != 0 && !p->intent_on;
    int zero_restart = s != 0 && ll_play_v_raw(p->vx) == 0;
    p->intent_on = s != 0;
    if (ll_play_row_physics() && !(row_admit || row_park ||
                                   st->nfr == 0 ||
                                   intent_edge || zero_restart))
        goto physics_skipped;   /* PR-518, the helper above  */
    long long vx100 = ll_play_v_raw(p->vx); /* 0x54250's x register, and */
    long long vy100 = ll_play_v_raw(p->vy); /* its y register, from here */
    {
        int td = (int)st->tx;
        if (td > 130)                       /* 0x52590 caps T at PARSE */
            td = 130;
        /* No lower clamp is modeled: the engine's raise of T to >= 20
         * is conditional ("only when the +0xbc guard fires", PR-132's
         * reading of PR-129 step 1) and WHICH objects carry that guard
         * is still open, so an authored `T=0` - no drag word at all -
         * stays no drag here. Counted as an open item, not guessed.
         * PR-256: the raise at 0x54266-0x54275 tests [ebx+0x8] only, so
         * it is an X-side rule; the y drag keeps its authored percent. */
        vx100 = ll_play_v_step(vx100, td,       /* idiv(F*s, 20) is the */
                               ll_play_x_force(p, st, s,
                                              ll_play_row_physics()));
    }   /* PR-521: on the row-admit path the add also carries the authored
         * G_x word (0x542c1, the closed x law of PR-520); with the gate
         * off it is still exactly idiv(F*s,20) as 0x542ab-0x542c7 makes. */
    if (vx100 > 100000)                    /* the engine's own guard has */
        vx100 = 100000;                    /* no cap in this chain; this */
    else if (vx100 < -100000)              /* one only bounds runaway    */
        vx100 = -100000;                   /* authored F/T (no data does)*/

    /* PR-148: the engine's y-intent word, [obj+0xbe], set every
     * iteration from the held Up/Down keys exactly where 0x549f0 (the
     * frame loop's per-frame call of the lion's input method, 0x4aeca,
     * PR-147) stores the input block's word1 into it: scancode 0x248
     * (Up) writes the negative value, 0x250 (Down) the positive one -
     * y+ is DOWN, PR-99's sign, re-confirmed at byte level by the
     * capture reader 0x582f0. The magnitude question is named at
     * LL_PLAY_INTENT_FULL (the keyboard's own constant is 20, the
     * contact chain writes 10, the port's one validated scale is 10).
     * The contact-node override (the kind-4 arm, 0x54b16) is PR-149's
     * route. The consumer modelled here was section 7's mode-4 row
     * feed guard (the engine's 0x5441b); PR-259 adds the second
     * consumer, the y force of 0x542cd-0x542e3, whose parameter word
     * [ebx+0x4] is the authored F pair's SECOND number: the x side of
     * the same body multiplies [ebx+0x0] by the horizontal intent and
     * is the law PR-132 shipped as idiv(F*s,20). */

    /* 3) vertical: the y side of the same pass, PR-253/PR-256. The T_y
     * drag is computed from the OLD register, the authored G= word is
     * added first and the drag subtracted after, and the sign-cross zero
     * of 0x5430a-0x54337 compares the value before the drag with the one
     * after it. Then, as the integrator does AFTER the zero and BEFORE
     * its delta output, the CURRENT row's `FC` pair pulls both axes
     * UNGATED, every iteration the row stays current (PR-250/PR-251:
     * channel id 1 bypasses the [obj+0x95] one-shot gate). */
    /* PR-259: the second half of this guard is the y-intent force's
     * door, and the door is the STATE, not the key. The engine's block
     * is ONE unconditional stretch: the drag at 0x54292-0x542a7, the
     * adds at 0x542cd-0x542fa, the subtraction at 0x54311. What keeps
     * a standing body still there is its ground contact (0x54c00,
     * reached through the lion's vtable +0x74; PR-148's item 3), not
     * the arithmetic, and the port's `!p->grounded` stands in for that
     * contact. A stand-in cannot cover the case this PR needs: the
     * vertical-intent modes - the engine's BY_TY and BY_Y, the same
     * two the row-feed guard above is keyed to - author an F pair with
     * a SECOND number and no G_y for a ground pull to cancel it, so
     * one more term opens the door on the STATE's own control mode:
     * such a state runs the pass even with ground under it, and runs
     * the drag with it, key held or not, which is what stops a climb
     * when the key is let go. The shipped authoring bounds how few
     * states that is: of the 1376 state blocks in the 777 .MOV texts
     * that write an F= line, 59 give it a non-zero second number, 53
     * of those author G_y = 0, and only 15 of those are player states
     * in a vertical-intent mode - the 12 BY_TY climb blocks of
     * PR-108's census (ZARIA.01/.02 A_WSP and A_WSP_LIN, TUNGA.11's
     * two, TUNGA2.12's A_DRAB, KARA.07's A_WSP_LIN) and the 3 BY_Y
     * ones (ENA.03's A_UP/A_DOWN, ENA2's A_CLIMB). The other 25
     * player blocks are BY_TIME swim/fire/cutscene states and are NOT
     * reached by this term. MODEL CHOICE, counted at OI-078: the
     * guard is widened, not replaced, and the ground-contact path at
     * 0x54c00 itself stays unread. */
    if (!p->grounded ||
        ((st->ctrl & (LL_CTRL_BY_TY | LL_CTRL_BY_Y)) &&
         st->fy != 0 && st->gy == 0)) {
        /* PR-256: one rule, the engine's. 0x542cd-0x542fa add the state
         * record's G_y word to the y register EVERY iteration the body is
         * in this pass - there is no "while jumping" branch in the body,
         * and the old GMUL-only-while-jumping split was a unit mistake:
         * the authored word IS the per-iteration register add, which at
         * this clock is raw*4 px/s^2 on both paths. */
        long long ay = (long long)st->gy +
                       ((long long)st->fy *
                        (long long)p->vert_intent) / 20LL;
                                          /* PR-256: the authored G_y
                                           * word IS this add - 0x542ee
                                           * adds [ebx+0x14] to the
                                           * register, whole, once per
                                           * iteration (the px/s^2
                                           * reading is that word times
                                           * LL_PLAY_RAW_GMUL). And
                                           * PR-259: plus the intent
                                           * force of 0x542cd-0x542e3,
                                           * idiv([ebx+0x4] *
                                           * [obj+0xbe], 20), the F
                                           * pair's second word,
                                           * divided BEFORE G_y is
                                           * added, as the idiv does. */
        /* PR-285 narrows the stand-in to the states whose OWN words
         * leave the register open. A state that authors T_y >= 100
         * asks the engine to consume the register whole -
         * idiv(100*v,100) IS v, so `out = adds`, and with no add
         * there is no motion: that is how the game HOLDS a body,
         * and it is authored 39 times in the player's own scripts
         * (26 blocks at exactly 100, 13 more at 150/1000: A_DEAD,
         * A_SCIAG, A_PODCIAG, A_WPAD, the A_JUMP_DEAD family,
         * A_FINISH_LEVEL - see the selftest leg's census).
         * ZARIA.01's A_PODCIAG is one of them. The test runs on the
         * authored word, not on the 130-capped value below: a 150
         * asks for the same hold. A body in a state below 100 keeps
         * the PR-106 behaviour: the project constant, the same unit,
         * the same kind of word: see ll_play.h. The project
         * constant's own note bounds it to a CHECK_LN state that
         * LOSES its ground, and PR-108/PR-231 make CHECK_ALL imply
         * CHECK_LN - so without this term the stand-in would fall on
         * a lion standing on the ground with no key held, a case the
         * new guard opens. On the old path the extra term cannot
         * change anything: it was already true there. */
        if (ay == 0 && !p->grounded && (st->ctrl & LL_CTRL_CHECK_LN) &&
            st->ty < 100)
            ay = LL_PLAY_GRAVITY;
        int tyd = (int)st->ty;
        if (tyd > 130)
            tyd = 130;        /* the same 0x52590 parse cap the x
                               * drag applies; no lower clamp - the
                               * >=20 raise is x-only and conditional
                               * (PR-252, 0x54266-0x54275) */
        vy100 = ll_play_v_step(vy100, tyd, ay);
    }
    if (p->fi >= 0 && p->fi < st->nfr && st->fr[p->fi].has_fc) {
        /* 0x54348, inside the same pass, after the drag and the sign
         * zero and before the delta output: the CURRENT row's `FC` pair
         * pulls both axes UNGATED, every iteration the row stays current
         * (PR-250/PR-251: channel id 1 bypasses the [obj+0x95] one-shot
         * gate). Its x side is the one the engine scales by the
         * direction word (0x54340): since PR-321 an authored
         * `ldir`/`rdir` in force scales it outright, and with none in
         * force the walkdir stand-in keeps answering. */
        ll_play_pull(&vx100, (long long)st->fr[p->fi].fcx *
                             ll_play_dir_word(p));
        ll_play_pull(&vy100, (long long)st->fr[p->fi].fcy);
    }
    p->vx = ll_play_v_px(vx100);
    p->vy = ll_play_v_px(vy100);

    /* 4) integrate: the step the engine takes is the pair of words
     * 0x54358-0x54372 writes, [obj+0xca] = idiv(v_x,100) and
     * [obj+0xce] = idiv(v_y,100), and the position step adds those WHOLE
     * numbers (0x54827-0x54829: `x += [obj+0xca]`, `y += [obj+0xce] +
     * [obj+0xea]`). The +0xea term is the SEQ step motion written with
     * +0xe2/+0xe6 (a row's own move-over-N-iterations verb, seen set at
     * 0x4549a-0x454ba and consumed at 0x54759-0x547c8), NOT a sub-pixel
     * carry: nothing in the velocity path keeps the idiv remainder, so a
     * part of a px in one iteration is DROPPED. At the authored ZARIA
     * walk rate the register sits at 250 and the body moves 2 px per
     * iteration, not 2.5. The positions stay whole numbers, as the
     * engine's [obj]/[obj+0x4] words are, and the DRAW position in
     * ll_level.c is an int of the same number. */
    double dxv = (double)(vx100 / 100);   /* [obj+0xca], idiv semantics */
    /* PR-321: the store branch. The engine's step ends its integration
     * at 0x54ee0-0x54f20, and while value 1 of [obj+0xb6] is set - the
     * `lock` word of the row chain, 0x54f40 - that store keeps the
     * drawn body at the frozen anchor pair [obj+0x89]/[obj+0x8d] minus
     * the presented point instead of following the new position; the
     * anchor is refreshed (0x541f0) only while the bit is clear, so the
     * body stands still for the whole locked window while the velocity
     * register, the row clock and the signal path run on. ZARIA.01
     * parks its exit inside exactly such a window: A_WYJSCIE row 0
     * `lock`, `ldir`, `pos 1020,692` (PR-317), row 16 `unlock`. The
     * port has no separate anchor/presented pair, so the counterpart of
     * the branch is simply: no position write while p->lock is set.
     * What the engine's ground-contact pass writes while locked is not
     * read yet - named in PR-321's OPENS; no port leg leans on it. */
    if (!p->lock) {
        p->x += dxv;
        p->y += (double)(vy100 / 100);    /* [obj+0xce] */
    }
    /* PR-110: the row accumulator's feed depends on the state's control
     * mode, and this is the other half of it. The engine's dispatcher
     * 0x543a0 (PR-98 item 2, its owner read by PR-100) feeds mode 0 =
     * BY_X from |[obj+0xca]|, the distance the step travelled, and the
     * time modes from the level's 100-per-iteration constant;
     * obj_walk + obj_advance have modelled exactly that split on the
     * scene-object side since PR-98 (`walk ? facc : ...`). The player
     * side had only the time half, which was harmless while the clock
     * ran ms and is not harmless now: a real BY_X walk list (RAN.01
     * 12 rows x 6, ZARIA.01 the same) totals 66 units, BELOW the 100
     * one iteration feeds, so on the iteration feed alone it would
     * cycle inside a single iteration. Both accumulators run here, and
     * section 7's gate picks by mode, as obj_advance's does. The feed
     * is the velocity's own distance, before the bounds clamp in (6) -
     * [obj+0xca] is the velocity register, not the post-clip position. */
    p->row_dx = (long long)dxv;       /* PR-518: [obj+0xca] persists;
                                     * the mode-0 feed at the iteration
                                     * top adds THIS word every frame,
                                     * admits or not. */
    if ((st->ctrl & LL_CTRL_BY_X) && st->fx != 0 && !ll_play_row_physics())
        p->facc += dxv < 0 ? -dxv : dxv;   /* the OLD in-pass feed */

    /* 5) ground logic (.plt lines, the shape bottom on top).
     * PR-97: the tested point is `y + p->contact` - the contact line the
     * viewer resolved for the animation being shown (ll_player.contact,
     * ll_seq_shape_contact) - not the position point. DATA: the frame
     * anchors of an animation are authored so that its shape-box bottom sits
     * at one fixed offset below the position point over the whole cycle -
     * ZARIA LEW.ZAR WALK.SEQ 12 frames, f2 = 31 on every one and the shape
     * box bottom at pos+30..+32; the same lion's END.SEQ runs f2 from 5 to
     * 54 while the bottom stays at EXACTLY pos+58; ZARIA's worm WAZ.SEQ
     * swings f0 -2..-22 and f2 -9..-16 with its bottom fixed at pos+46..49.
     * Only the shape box is that stable, and the game's own terrain-side
     * geometry (the hit test FUN_00042a70, the push-out FUN_00042de0) is
     * shape-box geometry, so the contact line is the shape bottom.
     * The view-side arithmetic against the original: ZARIA.01's camera clamp
     * is cam_ymax 500 and its .plt main ground is world y=680-684, so that
     * line renders at view row 180 - the paw line PR-84 measured on the video
     * (paws at 175..180). Parking the SHAPE BOTTOM there keeps the lion the
     * same height as the original. Parking the POSITION POINT there (what
     * this port did until now) puts the same shape bottom at world 710 =
     * view 210, outside the 200-row view: the "Lew is below the grass, his
     * legs are cut" report. */
    int xi = (int)(p->x >= 0 ? p->x + 0.5 : p->x - 0.5);
    int cont = p->contact > 0 ? p->contact : 0;
    double ground_y = p->y + (double)cont;     /* where this frame touches */
    int gyi = (int)(ground_y >= 0 ? ground_y + 0.5 : ground_y - 0.5);
    if (ground) {
        if (p->grounded && (st->ctrl & LL_CTRL_CHECK_LN)) {
            ll_xy hit;
            if (ll_plt_surface_at(ground, xi, gyi, LL_PLAY_STEP, &hit) &&
                p->vy >= 0.0) {
                /* PR-135: this snap now also requires a body that is
                 * not moving up - the same sign test the landing branch
                 * below already makes, and the same reason the engine's
                 * ground contact is a CONTACT and not a leash: its
                 * purpose here is slope following and the bounding
                 * curbs, and every real .plt case in the measured set
                 * is a body travelling ALONG a surface, not away from
                 * it. The hazard that motivated it is arithmetic: on a
                 * 50 ms grid one take-off iteration moves 15 px
                 * (vy = -300 u/s), inside this branch's 16 px
                 * tolerance, so a snap that ignored the direction would
                 * glue a jumping lion back to the floor.
                 * HONEST LIMIT, from PR-135's own mutation run: NO leg
                 * in this repo distinguishes the guard. Dropping it
                 * leaves both selftests green AND the whole of
                 * verify-play and verify-level green
                 * (/tmp/opencode/pr135/mut2/M12_play_no_vy_guard.*.log).
                 * The reason is structural: a row's upward `I` clears
                 * p->grounded as it is applied (apply_row_pull), and a
                 * body in the air does not reach this branch at all, so
                 * `grounded && vy < 0` is not a state this model can
                 * currently produce. The guard is therefore defensive
                 * bookkeeping, counted as an open item; the fixture
                 * that would pin it is a CHECK_LN state that is walked
                 * off a step edge while a row's `I` points up. The
                 * second guard below, on `p->support`, is NOT covered by
                 * that mutation either. */
                p->y = (double)hit.y - (double)cont;  /* slope following */
                p->vy = 0;
            } else if (p->support && p->vy >= 0.0) {
                p->vy = 0;                    /* standing on a solid object */
            } else {
                p->grounded = 0;
                p->air_from = p->y;
                ev |= LL_PLAY_EV_AIR;
            }
        } else if (!p->grounded) {
            /* Landing search. The start row sits LL_PLAY_STEP ABOVE the
             * contact line (y-down: minus = up): a CHECK_LN walker that
             * briefly left the ground at a line END and sank a few px below
             * the continuation line (real RAN.PLT: shelf 126..302@314 ->
             * 323..485@313 across a 20 px gap) steps back UP onto it -
             * symmetric with the step-down tolerance in the grounded
             * branch. "A fall from higher up is unaffected: its surface
             * is below the contact line either way" - the claim this
             * comment carried until PR-272 - was measured FALSE: fast
             * falls CROSS a line unseen; the sweep is the PR-272
             * paragraph below. */
            ll_xy hit;
            /* the old code truncated away from zero here (one px lower
             * than the grounded branch's nearest rounding for a negative
             * y); keeping that difference, only the tested point moved */
            int start = (int)(ground_y >= 0 ? ground_y : ground_y - 1.0);
            /* PR-272: the sweep top is the contact line the PREVIOUS
             * iteration ended on, never lower. Below the line the
             * LL_PLAY_STEP reach is the old one, so the step-up case the
             * comment above names keeps its exact behaviour; what changes
             * is that a fall which CROSSED a line during this iteration
             * is seen on the iteration it crossed. The old top was this
             * iteration's line alone, and PR-271 measured the hole: the
             * ZARIA.01 main ground at y 682 was crossed UNSEEN at
             * ~25-33 px/tick (up2 120-139, cl3 117-129, red run
             * 124-151), the lion falling to the death line and
             * respawning. The engine cannot do that: its terrain side is
             * the shape-box contact of PR-97 (hit test 0x42a70,
             * push-out 0x42de0), and a box overlapping the line IS the
             * contact. */
            int from = start < p->prev_line ? start : p->prev_line;
            if (p->vy >= 0 && ll_plt_first_below(ground, xi,
                                                 from - LL_PLAY_STEP, &hit) &&
                ground_y >= (double)hit.y - 1.0) {
                p->y = (double)hit.y - (double)cont;
                p->vy = 0;
                p->grounded = 1;
                p->sig |= 1ull << LL_SIG_K_GEN2;        /* landing pulse  */
                ev |= LL_PLAY_EV_GROUND;
                if (p->jumping && !st_has_break_next_exit(st)) {
                    /* FALLBACK viewer rule ONLY when the state declares
                     * no AC_BREAK_NEXT exit of its own (the JM-style
                     * minimal file). PR-86: the authored jump states DO
                     * declare one - ZARIA/TUNGA's A_JUMP ends with
                     * A_WALK(7)=AC_BREAK_NEXT, A_JUMP_S with
                     * A_DRAP(1)=AC_BREAK_NEXT - and PR-86 decoded the
                     * (N) token as the target's START ROW, which makes
                     * those lines re-join the walk cycle MID-PHASE (row
                     * 7 of ZARIA's 12-row cycle) and the PRE_JUMP route
                     * skip its own impulse row. The old "landing ends
                     * the jump state, go idle" reading contradicted the
                     * data (no authored jump exits A_STAY anywhere); it
                     * stays only as this fallback. The jump frame-list
                     * WRAP arms AC_BREAK_NEXT (PR-83 mechanism) a beat
                     * after the physics landing - the long recovery rows
                     * (A_JUMP row 6: 900 ms) then play out inside the
                     * state, as authored. Whether the original cancels
                     * the recovery early (the undecoded `WAIT 0ms` row +
                     * `AC_DUP`) stays OPEN - see HANDOFF 7.8.
                     * jumping=0 stays cleared here either way: the flag
                     * says "this flight came from a row pull", and the
                     * fall itself has run on the state's own G= since
                     * PR-253 either way. */
                    p->jumping = 0;
                    int idle = ll_mov_find(p->mov, "A_STAY");
                    if (idle < 0)
                        idle = ll_mov_find(p->mov, "A_DRAP");
                    if (idle >= 0 && idle != p->sti) {
                        p->sti = idle;
                        p->fi = 0;
                        p->row_t = 0;
                        p->facc = 0.0;
                        p->vx = 0;
                        ev |= LL_PLAY_EV_STATE;
                        st = &p->mov->st[p->sti];   /* step 7 uses the NEW state */
                    }
                } else if (p->jumping) {
                    p->jumping = 0;   /* authored exit pending: clear the
                                       * flag, keep the state running    */
                }
            }
        }
    }

    /* 6) lew_rect bounds */
    if (p->x < xmin) {
        p->x = xmin;
        p->vx = 0;
    } else if (p->x > xmax) {
        p->x = xmax;
        p->vx = 0;
    }

    /* 7) frame advance on the ITERATION clock (PR-110; same semantics
     * here as obj_advance: wrap at the end, apply M dx,dy deltas;
     * AC_BREAK_NEXT is armed by a wrap and consumed by transitions next
     * tick). PR-109 measured the unit: the authored row numbers are
     * thresholds in 1/100-iteration units (the engine feeds its row
     * accumulator exactly 100 per frame-loop iteration, 0x4c8b7, and
     * its pacer 0x4b0e0 -> 0x4a120 -> 0x1dba0 holds one iteration to
     * two display frames), and on the owner's ladder clip the rows of
     * 200/300 ran 66-100 ms, NOT the 200/300 ms the old wall-ms clock
     * gave. So `row_t` now counts the same 1/100-iteration units the
     * thresholds are in, fed in whole batches of 100 whenever
     * `row_rem` - the same millisecond grid as before - completes an
     * iteration of LL_ITER_MS. The compare stays AFTER the add and the
     * threshold is subtracted on a hit, which is the engine's rule
     * (0x521f0 compares [obj+0xa1] with the current row's node+0x10
     * and the accumulator keeps the remainder): a row of 200 is
     * current for exactly 2 iterations whatever the step size, and
     * short rows (20, 40 - real A_JUMP rows) all land inside one
     * iteration, as they do there. The carry makes the port
     * grid-independent: at dt == LL_ITER_MS the feed is exactly 100
     * per step; at the CI's 20 ms grid it is 100 per 28 ms with the
     * leftover ms carried (Bresenham), never a drifted fraction.
     * PR-333: that subtract-and-carry rule is the engine's CLOCK path,
     * which a row with NO `Wait` clause takes; a row whose clause
     * carries bits takes the match path instead - it skips the compare
     * and is handed a zero budget (0x522f8), so its threshold is never
     * consumed and it can hold the list at that row indefinitely. Both
     * paths, and the `sete` that picks between them, are in
     * ll_mov_row_admit.
     * The BY_X branch (PR-110, and the reason `walk` is in the gate):
     * the engine's dispatcher 0x543a0 feeds the SAME accumulator by
     * mode - mode 0 (BY_X) adds the distance travelled, |[obj+0xca]|,
     * and ONLY the time modes add 100 per iteration (PR-98 item 2,
     * PR-100's owner reading; ll_level.c's obj_walk/obj_advance have
     * modelled that split since PR-98). So a walking state's row clock
     * does not run at all - its rows are fed by section 4's distance
     * alone - and a time-driven state's distance accumulator stays 0.
     * One accumulator, one feed source, chosen by the state's mode:
     * that is the whole gate below.
     * PR-83: `GEN` tails raise their signal as the frame completes
     * (same rule the object engine follows, ll_level.c obj_advance) -
     * that is the player's AC_TIME mechanism. The state time= timer is
     * NOT acted on: its unit is undetermined (pairs like 32,64 / 90,120
     * read as ranges; acting on it as ms pulled ENA.03's lion out of
     * A_SWIM two ticks after spawn), so it stays counted as a field
     * until RE pins it down. (An earlier model choice - arming AC_TIME
     * at the list WRAP - was wrong for the player: the real scripts
     * name their own source.) */
    if (st->nfr > 0) {
        if (!ll_play_row_physics()) {
            /* the OLD path, unchanged: feed and admit test inside the
             * pass after the physics, exactly as PR-110/PR-148 wrote
             * them; the row clock stays where they left it. */
            int walk = (st->ctrl & LL_CTRL_BY_X) && st->fx != 0;
            int by_ty = (st->ctrl & LL_CTRL_BY_TY) && !p->vert_intent;
            /* PR-148, the mode-4 feed guard, byte-exact at 0x5441b: the
             * dispatcher's BY_TY branch adds nothing while [obj+0xbe] is
             * zero, and the row compare runs on regardless - a BY_TY row
             * CLOSES ONLY ON ITERATIONS WHERE THE VERTICAL INTENT IS
             * HELD, and the accumulator carries the partial feed across
             * the gap exactly like the engine's [obj+0xa1]. The guard is
             * on the feed, not the admit. Not copied: the same branch's
             * cousin at 0x543f2 guards BY_TIME (mode 2) on the X-intent
             * word - PR-102 recorded mode 2 as unguarded, the model has
             * run that way through every video-verified leg since, and
             * PR-147's own reading of that guard's data source (word0 =
             * the arrows) makes it a behavioural regression study, not a
             * copy. Counted as open item 2 in the PR-148 record. */
            if (!walk && !by_ty)
                p->row_t += 100;      /* the engine's constant feed */
            row_admit = ll_mov_row_admit(&st->fr[p->fi], row_live,
                                         walk, &p->row_t, &p->facc);
        }
        if (row_admit) {              /* the row cursor and the chain */
            /* AT MOST ONE ROW PER ITERATION (PR-100's reading of the
             * state record's step at 0x521f0: compare, subtract the
             * authored threshold, hand the remainder over, and the step
             * ends). With the feed of 100 per iteration that is what
             * makes a row of 6 (the real A_FIGHT and A_STAY lists) run
             * one pose per iteration = 50 ms, and a row of 200/300 run
             * 2-3 iterations = 100-150 ms, which is the 66-100 ms
             * PR-109 measured on the ladder clip (its own clock was
             * then fitted at 28 ms; at the 50 ms iteration 2-3 of them
             * is 100-150 ms, inside the same retrace design point).
             * The gait arithmetic closes on the video the same way:
             * the lion's 12 poses x 6 units at 2.5 units/iteration is
             * 2.4 iterations per pose = 120 ms, and PR-134 measured
             * 72-110 video frames per pose cycle = 120-183 ms; the
             * worm's 9 x 4 at 1.25 is 3.2 iterations = 160 ms per pose
             * = 1.44 s per cycle, PR-79's measured period exactly.
             * A row's own `dx,dy` delta, its `GEN` signals and its
             * ATC window are applied on the admit, as before. */
            p->x += st->fr[p->fi].dx;
            p->y += st->fr[p->fi].dy;
            unsigned long long gf = st->fr[p->fi].gsig;
            p->sig |= gf;                            /* PR-88: any named  */
            p->gsig_last |= gf;                     /* PR-257: the level-bus
                                                     * copy of this admit's
                                                     * GEN bits (K_GEN* only
                                                     * - see ll_level.c)   */
            if (gf & (1ull << LL_SIG_AC_TIME))       /* signal raises on  */
                p->ac_time = 1;                      /* frame completion  */
            if (gf & (1ull << LL_SIG_AC_BREAK_NEXT))
                p->broke = 1;          /* TUNGA's A_FIGHT rows raise this
                                        * directly (`GEN AC_BREAK_NEXT`);
                                        * the frame-list wrap below keeps
                                        * arming it the PR-83 way. */
            unsigned char ac = st->fr[p->fi].atc;
            if (ac & LMOV_ATC_ON)                  /* PR-89: the frame's */
                p->atc = 1;                        /* `ATC_ON` opens the */
            if (ac & LMOV_ATC_OFF)                 /* attack-connect     */
                p->atc = 0;                        /* window. */
            /* PR-199: a row that carries `end_level <n>` is the engine's
             * apply method 0x45750 running on that node:
             * `mov ebx,ds:0xcef40; mov ecx,[node+0x8]; mov [ebx+0x4],
             * ecx`. The number goes to the level's exit-word slot and
             * NOTHING else happens - the apply ignores its second
             * argument and still walks the row's own chain, which is
             * what the two engine legs above already model. The value is
             * not examined here and never cleared here: PR-198 measured
             * that the loop body has no store to that slot and that the
             * slot's only test is `cmp [ecx+0x4],0x0` at the TOP of an
             * iteration, so the end lands one boundary after this admit. */
            if (st->fr[p->fi].endlv)
                p->exit_word = st->fr[p->fi].endlv;
            /* PR-317: a row that carries `pos x,y` is the engine's apply
             * 0x45a30 running on that node: it calls the OWNING object's
             * class +0x4 verb, 0x23850 for the lion, whose whole body is
             * `mov [eax],edx; mov [eax+0x4],ebx; call [class+0x54]`. An
             * ABSOLUTE set of the position, nothing else: no velocity term
             * (the register is untouched), no clearance, no signal, and it
             * does not go through the physics step at all. The engine runs
             * the row's directive chain in authored order, and `M dx,dy`
             * is a node of that same chain, so an `M`+`pos` row would have
             * an order - no shipped row carries both (4 rows in 3 files,
             * none with an M), so applying it after this frame's delta is
             * not a choice the data can contradict. The +0x54 refresh that
             * follows the set recomputes the object's presented point at
             * [obj+0xd6] with the class +0xb0 provider; this port
             * recomputes its ask line from the final position at the end
             * of every iteration (see prev_line below), so the refresh is
             * already there. */
            if (st->fr[p->fi].has_pos) {
                p->x = (double)st->fr[p->fi].posx;
                p->y = (double)st->fr[p->fi].posy;
            }
            /* PR-321: the one-word tails run at this same point of the
             * row chain - 0x45c50 (lock/unlock) and 0x459f0/0x45a10
             * (ldir/rdir) are ordinary apply methods walked when the
             * engine walks the row's chain, exactly like the `pos`
             * apply above (PR-316/PR-320). They write the object's own
             * words: lock/unlock mirror value 1 of [obj+0xb6]; the
             * not-attached guard the engine puts in front of that pair
             * (the class +0xbc verb, 0x23870 for the lion,
             * `cmp [obj+0xa9],0x0; setne`) has no port counterpart
             * because attachment - PR-319's opcode 0x1b block - is not
             * modelled, so the word applies unconditionally here.
             * PR-525: the word is not ONLY a bit. 0x54f40 puts its
             * trio - current-element getter 0x23aa0, the element's own
             * land method 0x42980 (through the element block's [el+0x33]
             * table, slot +0x4), flip getter 0x53e40 - BEHIND a
             * `test BYTE PTR [ecx+0xb6],0x1; jne`, and both paths land
             * on the `or ...,0x1`; 0x54f90 guards the other way (`je`,
             * LEAVE 0x429d0 runs only while the bit is set) and then
             * `and ...,0xfe`. The methods' write target is edx = the
             * object itself: [obj+0x0]/[obj+0x4], the live position.
             * The port reaches the .SEQ element through the driver hook
             * (the loaded blobs are ll_level.c's); no element for the
             * row answers 0, the sentinel case at 0x4298d/0x429dd. */
            if (st->fr[p->fi].lockcmd > 0) {
                if (!p->lock && p->anchor_shift) {
                    int sdx = 0, sdy = 0;
                    if (p->anchor_shift(p->anchor_ctx, p, 1, &sdx, &sdy)) {
                        p->x += sdx;
                        p->y += sdy;
                    }
                }
                p->lock = 1;
            } else if (st->fr[p->fi].lockcmd < 0) {
                if (p->lock && p->anchor_shift) {
                    int sdx = 0, sdy = 0;
                    if (p->anchor_shift(p->anchor_ctx, p, 0, &sdx, &sdy)) {
                        p->x += sdx;
                        p->y += sdy;
                    }
                }
                p->lock = 0;
            }
            if (st->fr[p->fi].dircmd)
                p->dirword = st->fr[p->fi].dircmd;
            /* PR-323: the cursor step. The engine's advance
             * verb 0x520c0 walks the row chain's +0x10 verbs
             * (ebx = &[rec+4], dl = 6) BEFORE stepping, and a
             * `goto`/`GOTO_REL` node answers 6 there
             * (0x45d90 writes the cursor outright, 0x45dc0
             * adds the delta) so 0x52140 skips the whole
             * normal step for that row's close: no +1, no
             * wrap, no list bit. The row entering through the
             * jump has its one-shot I/F pull applied at its
             * entry like any other admit (PR-253's gate law,
             * [obj+0x95]). The engine NEVER clamps the
             * written cursor - a negative or past-end cursor
             * reads the heap around the row table, an
             * undefined read this port does not reproduce:
             * the clamp below is a named port safety net, and
             * no shipped PLAYER row leaves the range (the
             * only out-of-range row, KADMAR2_SCHODY's
             * `1,0,GOTOREL -1`, is an object state). */
            if (st->fr[p->fi].gotocmd) {
                int nf = st->fr[p->fi].gotocmd == 1
                       ? (int)st->fr[p->fi].gotoval
                       : p->fi + (int)st->fr[p->fi].gotoval;
                if (nf < 0)
                    nf = 0;                          /* port safety net  */
                if (nf >= st->nfr)
                    nf = st->nfr - 1;                /* ... and here too */
                p->fi = nf;
                apply_row_pull(p, st, p->fi);
            } else if (p->fi + 1 >= st->nfr) {
                p->fi = 0;
                p->broke = 1;
            } else {
                p->fi++;
                apply_row_pull(p, st, p->fi);
            }
        }
    }
    /* PR-272: record the contact line THIS iteration ended on (see
     * ll_player.prev_line, the landing sweep). Recomputed from the final
     * position, not reusing gyi: the ground pass may have snapped p->y,
     * and section 7's `M` delta moved it again after that. */
    {
        double gl = p->y + (double)cont;
        p->prev_line = (int)(gl >= 0 ? gl + 0.5 : gl - 0.5);
    }
    /* PR-331: the surface-link raises of the engine's physics step, the
     * tail 0x54760-0x54803 that runs right after the ground queries
     * (0x4b430/0x4b600/0x4b7f0 through 0x49580, PR-312/313). Its input
     * is the query result [obj+0xe6] and the attachment [obj+0xa9]; the
     * port's counterpart of "the query found a surface" is p->grounded.
     * Read at the tail:
     *   no surface found (0x54760 je 0x547d7): if a surface was held,
     *     call its detach ([sur+0x0]+0x4), clear [obj+0xa9], call the
     *     class +0xb8 verb - and in BOTH cases 0x54800 raises AC_ONAIR
     *     (`or BYTE PTR [edi+0x2],0x4` = bit 18 of the signal mask the
     *     clause resolver 0x47d60 reads = LL_SIG_AC_ONAIR here). So
     *     AC_ONAIR is the ground query FAILING, posted every iteration
     *     it fails - the fact the authored `A_FALL_DOWN(1)=AC_ONAIR`
     *     waits on (109 such lines in 11 of the 777 texts; 208
     *     clause lines name AC_ONAIR, in 23 texts in all).
     *   a surface found: from NO attachment 0x54785 raises AC_DUP
     *     (`or BYTE PTR [edi],0x10` = bit 4 = LL_SIG_AC_DUP), stores
     *     [obj+0xe6] into the slot, and calls the surface's +0 verb and
     *     the class +0xb4 verb. A surface ALREADY held is compared at
     *     0x547a7: the same one changes nothing and raises no signal, a
     *     different one detaches through +0x4 and attaches, again with
     *     NO signal. Then the kind word the query leaves at
     *     [obj+0xe2]: non-zero posts AC_DUP_CD on every such step
     *     (0x547d1, bit 13). And a kind-2 surface together with the
     *     object's OWN y velocity register [obj+0xc6] - the pair
     *     PR-326 read - at or over 0x58c takes the NO-surface branch
     *     instead (0x54766-0x54779): falling fast, it does not hold.
     * This port's polyline ground carries no surface identity and no kind
     * word, so the SWITCH/NO-signal arm, AC_DUP_CD and the kind-2 fast-
     * fall exception have no input here - OI-121. What IS modelled is
     * the pair the port can see: AC_ONAIR while the query fails
     * (grounded == 0), and AC_DUP on the attach EDGE ([obj+0xa9] empty
     * -> filled), the fact behind the 13 `A_JUMP(11)=AC_DUP` lines in 7
     * texts and the 79 clause lines that name AC_DUP in 21. The
     * AC_DUP those
     * blocks' own ladder and rope rows wait on is a DIFFERENT producer -
     * an object's ask raising it through 0x3df00 -> the asked's +0xd8
     * verb -> 0x45320 with [asked+0xee] (PR-275) - and is not ported.
     */
    if (!p->grounded)
        p->sig |= 1ull << LL_SIG_AC_ONAIR;
    else if (!was_att)
        p->sig |= 1ull << LL_SIG_AC_DUP;
physics_skipped:
    /* PR-518: this is where an iteration that did NOT admit an empty-
     * handed row resumes: the state's transitions, signals and row
     * clock still ran (they are per frame), the physics pass did not.
     * The surface-link raises above are the tail of the engine's
     * physics step (0x54760-0x54803 runs right after the ground
     * queries), so they are inside the gate - a non-admitting frame
     * re-posts what the last physics step's ground query found, which
     * is the same signal, and AC_ONAIR after a ledge is delayed by at
     * most one state row. */
    return ev;
}

/* PR-135: the clock. dt_ms feeds an iteration accumulator of
 * LL_ITER_MS and each completed iteration runs one state step + one
 * physics pass (PR-133's rate answer); the remainder is carried
 * Bresenham-style so the port stays grid-independent - the fixed-grid
 * legs that step at dt == LL_ITER_MS get one step per call, a 116 ms
 * call gets two, and the 20/40 ms substeps of the live loop add up to
 * the same iteration boundaries. */
int ll_player_step(ll_player *p, const ll_plt *ground,
                   const ll_play_keys *keys, double xmin, double xmax,
                   unsigned dt_ms)
{
    int ev = 0;
    if (!p->mov || p->sti >= p->mov->nst || dt_ms == 0)
        return 0;
    p->row_rem += dt_ms;
    while (p->row_rem >= LL_ITER_MS) {
        p->row_rem -= LL_ITER_MS;
        ev |= player_iterate(p, ground, keys, xmin, xmax);
    }
    return ev;
}

int ll_player_mirror_of(const ll_player *p, int authored)
{
    return authored ^ (p->face < 0 ? 1 : 0);
}

int ll_player_frame(const ll_player *p)
{
    if (!p->mov || p->sti >= p->mov->nst)
        return 0;
    const ll_mov_st *s = &p->mov->st[p->sti];
    if (!s->nfr)
        return 0;
    if (p->fi >= s->nfr)
        return s->fr[s->nfr - 1].frame;
    return s->fr[p->fi].frame;
}

ll_frame_ref ll_mov_frame_ref(int n)
{
    ll_frame_ref r = { 0, 0, 0 };
    if (n > 0)
        r.idx = n - 1;
    else if (n < 0) {
        r.idx = -n - 1;
        r.mirror = 1;
    } else
        r.blank = 1;
    return r;
}

/* ---------------------------------------------------------- selftest --- */

/* Mini .MOV with the REAL RAN.01 walk constants (values copied from
 * LEW.MOV verbatim: F=400 cap T=90, STAY brake F=600, frame durations
 * clamped by M4-C) and the real transition grammar. */
static const char ST_MOV[] =
    "A_STAY\r\nA_WALK_FWD\r\nA_WALK_BKWD\r\n\r\n"
    "[A_STAY]\r\ncontrol=FRAME_REV|BY_TIME|CHECK_ALL|CHECK_LN\r\n"
    "F=600,0\r\nT=90,90\r\nG=0,0\r\n"
    "A_WALK_BKWD(1)=K_BKWD\r\nA_WALK_FWD(1)=K_FWD\r\nseq=uj09\r\n1,6\r\n\r\n"
    "[A_WALK_FWD]\r\ncontrol=FRAME_REV|BY_X|CHECK_ALL|CH_DIR|CHECK_LN\r\n"
    "F=400,0\r\nT=90,90\r\nG=0,0\r\nA_STAY(1)=!K_FWD\r\n"
    "A_WALK_FWD(x)=K_FWD\r\nseq=uj09\r\n"
    "1,6\r\n2,6\r\n\r\n"
    "[A_WALK_BKWD]\r\ncontrol=FRAME_REV|BY_X|CHECK_ALL|CH_DIR|CHECK_LN\r\n"
    "F=400,0\r\nT=90,90\r\nG=0,0\r\nA_STAY(1)=!K_BKWD\r\nseq=uj09\r\n"
    "1,2\r\n2,2\r\n";

/* two shelves: walk from x=500 to the edge at x=701, fall ~50 px onto the
 * lower shelf (landing ~x=765, well inside it), stop, short reverse
 * (geometry invented; the line format is copied from RAN.PLT) */
static const char ST_PLT[] =
    "2\r\n1 0,100;700,100;\r\n1 651,150;3000,150;\r\n";

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            fprintf(stderr, "ll_play selftest FAILED (line %d): %s\n", \
                    __LINE__, #cond);                                  \
            rc = 1;                                                    \
        }                                                              \
    } while (0)

/* PR-525, leg LK-S hook: a stand-in for the driver's anchor resolver,
 * carrying the REAL LIFT.SEQ element 0 anchor fields (f2-f6 = 40,
 * f0-f4 = 7) and element 3's (33, 4), through the arithmetic that
 * 0x42980/0x429d0 pin (ll_seq_anchor_land/ll_seq_anchor_leave): land
 * `*y -= (f2-f6)`, x minus for facing > 0 (0x429a0) plus for the
 * other sign (0x429bd); leave the pair exchanged (0x429ec/0x429fb/
 * 0x42a0d). A row with no element answers 0, like the 0x7fff
 * sentinel at 0x4298d/0x429dd. */
static int st_anchor_shift(void *ctx, const ll_player *p, int land,
                           int *dx, int *dy)
{
    static const short dyf[4] = { 40, 0, 0, 33 };   /* the f2-f6 fields */
    static const short dxf[4] = { 7, 0, 0, 4 };     /* the f0-f4 fields */
    int fr = ll_player_frame(p);
    ll_frame_ref r = ll_mov_frame_ref(fr);
    (void)ctx;
    if (fr == 0 || r.blank || r.idx > 3)
        return 0;
    *dy = land ? -(int)dyf[r.idx] : (int)dyf[r.idx];
    if (!ll_player_mirror_of(p, r.mirror))
        *dx = land ? -(int)dxf[r.idx] : (int)dxf[r.idx];
    else
        *dx = land ? (int)dxf[r.idx] : -(int)dxf[r.idx];
    return 1;
}

/* PR-526, leg EN-S hook: the TLewZaria arm (0x1e250) in miniature -
 * the running state is the one named A_WPAD and the list-wrap bit is
 * live in this pass. (The engine compares the numeric 9; 9 is that
 * state's position in ZARIA's own declared table, so the name is the
 * honest form of the same fact, and this leg's fixture puts A_WPAD
 * first so the numeric and the named forms agree.) */
static int st_end_rule(void *ctx, const ll_player *p)
{
    (void)ctx;
    return p->broke && !strcmp(p->mov->st[p->sti].name, "A_WPAD");
}

int ll_play_selftest(void)
{
    int rc = 0;
    ll_plt plt;
    ll_mov_t m;
    ll_xy hit;

    /* The port's rate, pinned (ll_play.h): one state step per
     * LL_ITER_MS, the PR-133/134 answer for the frame-loop pacer.
     * Every fixed-grid leg here, the PPM probe and the live loop's
     * substeps use this constant, so a change to it is a change to the
     * port's speed and gait arithmetic, not a refactor - which is why
     * the header says this selftest asserts it. */
    CHECK(LL_ITER_MS == 50u);

    /* .plt grammar: header, segment-count prefix, interpolation, queries */
    CHECK(ll_plt_parse(ST_PLT, &plt) == 0 && plt.n == 2);
    CHECK(ll_plt_surface_at(&plt, 250, 100, LL_PLAY_STEP, &hit) &&
          hit.y == 100);
    CHECK(!ll_plt_surface_at(&plt, 760, 100, LL_PLAY_STEP, &hit));
    CHECK(ll_plt_first_below(&plt, 760, 120, &hit) && hit.y == 150);
    CHECK(ll_plt_first_below(&plt, 250, 40, &hit) && hit.y == 100);
    ll_plt_free(&plt);

    /* '!' inside one polyline = the same separator (real ENA.PLT writes
     * "1 1,560!7500,570;", ZARIA_A.PLT uses it 10 times): without it the
     * ENA.03 ground parsed to 0 lines and the level looked data-less. */
    ll_plt plt2;
    CHECK(ll_plt_parse("1\r\n1 0,100!700,100;\r\n", &plt2) == 0 &&
          plt2.n == 1 && plt2.line[0].n == 2 &&
          plt2.line[0].p[1].x == 700 && plt2.line[0].p[1].y == 100);
    ll_plt_free(&plt2);

    /* an AUTHORED-EMPTY ground is data, not a parse failure: JUNCTION's
     * platformy=none.des carries literally "0" (its level has no floor).
     * A count with no geometry, or no count at all, stays a failure. */
    ll_plt plt3;
    CHECK(ll_plt_parse("0", &plt3) == 0 && plt3.n == 0);
    ll_plt_free(&plt3);
    CHECK(ll_plt_parse("3\r\n", &plt3) == 1 && plt3.n == 0);
    CHECK(ll_plt_parse("nope\r\n1 0,0;5,0;\r\n", &plt3) == 0 && plt3.n == 1);
    ll_plt_free(&plt3);

    /* .blk grammar and the blockade query, PR-395. The bytes are
     * ZARIA.01/ZAR.BLK exactly as the HD ETAPY container carries them
     * (/tmp/opencode/pr395_blk/ZAR.BLK, 76 bytes, sha256
     * 66a8c17cb0fe2738...), and a requester is written as the engine's
     * FOUR request-block fields - near x, far x, near y, far y -
     * because those are the four dwords 0x4b441-0x4b490 build and
     * shift, every one of them carrying the pending clamp delta. The
     * record's three numbers are x, TOP y and HEIGHT, which is what
     * 0x455e0's own formats say ("%d,%d" at 0x91d0f into rec+4 and
     * rec+8, "%d" at 0x9183d into rec+0x10) and what the authored
     * files are: ZAR.BLK's first two rows are the world's left and
     * right edges, x 0 and x 2240, both 700 px tall. */
    {
        static const char ST_BLK[] =
            "6\r\n0,0,700\r\n2240,0,700\r\n1299,341,118\r\n1462,244,28\r\n"
            "1540,194,27\r\n1620,0,200\r\n";
        ll_blkreg bl;
        int d, idx;
        CHECK(ll_blkreg_parse(ST_BLK, &bl) == 0 && bl.n == 6);
        CHECK(bl.r[0].x == 0 && bl.r[0].y == 0 && bl.r[0].ly == 700);
        CHECK(bl.r[1].x == 2240 && bl.r[1].y == 0 && bl.r[1].ly == 700);
        CHECK(bl.r[2].x == 1299 && bl.r[2].y == 341 && bl.r[2].ly == 118);
        CHECK(bl.r[3].x == 1462 && bl.r[3].y == 244 && bl.r[3].ly == 28);
        CHECK(bl.r[4].x == 1540 && bl.r[4].y == 194 && bl.r[4].ly == 27);
        CHECK(bl.r[5].x == 1620 && bl.r[5].y == 0 && bl.r[5].ly == 200);

        /* THE X GATE IS ON THE WALL, NOT ON THE BODY. 0x45629 and
         * 0x45631 leave unless [block+0] <= rec.x <= [block+8], so a
         * body fully inside the world never meets either world edge:
         * the wall's x is outside its box. */
        CHECK(!ll_block_hit(&bl.r[0], 20, 60, 100, 140, &d) && d == 0);
        CHECK(!ll_block_hit(&bl.r[1], 20, 60, 100, 140, &d) && d == 0);
        /* A body that really does cover x 0 is overlapped. The side
         * test of 0x4566a is `([rec+4]-[block+0]) >= ([block+8]-
         * [rec+4])` = 20 >= 20, and `jge` takes the far side, so the
         * target is [block+8] + 1 = 21 and dx = 0 - 21 = -21. Adding
         * that to [obj+0xca] leaves the body's FAR edge at -1: the
         * engine throws the body out of the wall on the side its box
         * overlapped it least. */
        CHECK(ll_block_hit(&bl.r[0], -20, 20, 100, 140, &d) && d == -21);
        /* The same wall 15 px the other way: 5 < 35 takes the near
         * side, target [block+0] - 1 = -6, dx = +6, so the body's NEAR
         * edge lands at 1. */
        CHECK(ll_block_hit(&bl.r[0], -5, 35, 100, 140, &d) && d == 6);
        /* TOUCHING IS NOT OVERLAPPING. A body whose near edge is at
         * the wall's x scores 0 >= 40 = false, target x - 1, distance
         * 1, and 0x45683 refuses it; a body whose far edge is at the
         * wall's x gets distance -1 and the same refusal. On a refusal
         * the engine does not write [obj+0xca] at all, which is why
         * these pins read d == 0. */
        CHECK(!ll_block_hit(&bl.r[0], 0, 40, 0, 40, &d) && d == 0);
        CHECK(!ll_block_hit(&bl.r[0], -40, 0, 0, 40, &d) && d == 0);
        /* One px past the right edge of the world is a real overlap,
         * and the throw-back is 2 px: 10 >= 1 takes the far side,
         * target 2242, dx = -2. */
        CHECK(ll_block_hit(&bl.r[1], 2230, 2241, 0, 40, &d) && d == -2);

        /* THE Y TEST IS AN OVERLAP OF TWO RANGES, not a point test:
         * the wall is [rec+8, rec+8 + rec+0x10] and it is refused only
         * when BOTH its ends are past one end of the body's range
         * (0x45640/0x45647 above, 0x45651/0x4565a below). ZAR.BLK's
         * edge walls stop at y 700, the floor, and 701 is already out -
         * the level's own height, read from the file. */
        CHECK(ll_block_hit(&bl.r[1], 2230, 2241, 700, 740, &d) && d == -2);
        CHECK(!ll_block_hit(&bl.r[1], 2230, 2241, 701, 740, &d) && d == 0);
        /* An interior wall: 1299,341,118 stands from y 341 to y 459,
         * so a body at y 100..140 is not asked at all, and one at
         * 400..500 is: 9 < 31 takes the near side, target 1289, and
         * dx = +10 puts the body's near edge at 1300, clear of x 1299. */
        CHECK(!ll_block_hit(&bl.r[2], 1290, 1330, 100, 140, &d) && d == 0);
        CHECK(ll_block_hit(&bl.r[2], 1290, 1330, 400, 500, &d) && d == 10);
        /* both ends of the wall's y range are inclusive: a body whose
         * FAR y is exactly the wall's top y is asked (0x45640 tests
         * `y > hi` before it looks at the height at all), and so is one
         * whose NEAR y is exactly the wall's bottom end. */
        CHECK(ll_block_hit(&bl.r[2], 1290, 1330, 300, 341, &d) && d == 10);
        CHECK(ll_block_hit(&bl.r[2], 1290, 1330, 459, 460, &d) && d == 10);
        CHECK(!ll_block_hit(&bl.r[2], 1290, 1330, 460, 461, &d) && d == 0);
        /* A wall with a height of 0 is a one-pixel band and its ends
         * are the same number: it answers at its own y and not one px
         * away. No shipped row is zero-height; this is the law. */
        {
            const ll_blk_rec band = { 100, 300, 0 };
            CHECK(ll_block_hit(&band, 90, 110, 300, 320, &d) && d == -11);
            CHECK(!ll_block_hit(&band, 90, 110, 301, 320, &d) && d == 0);
        }
        /* and the two flush cases on that wall: near edge at 1299,
         * distance +1; far edge at 1299, distance -1; both refused,
         * while one px of overlap moves the body by 2. */
        CHECK(!ll_block_hit(&bl.r[2], 1299, 1339, 400, 500, &d) && d == 0);
        CHECK(!ll_block_hit(&bl.r[2], 1200, 1299, 400, 500, &d) && d == 0);
        CHECK(ll_block_hit(&bl.r[2], 1200, 1300, 400, 500, &d) && d == -2);
        /* A record whose height is authored NEGATIVE is the interval
         * [y, y+ly] upside down and the engine handles it as one,
         * because both of its tests look at the two ENDS. 500,-100 is
         * the wall y 400..500: it answers at 450..460 and not at
         * 600..650. No shipped .BLK row is negative, so this is the
         * law, not the data. */
        {
            const ll_blk_rec inv = { 100, 500, -100 };
            CHECK(ll_block_hit(&inv, 90, 110, 450, 460, &d) && d == -11);
            CHECK(!ll_block_hit(&inv, 90, 110, 600, 650, &d) && d == 0);
        }
        /* THE WALK STOPS AT THE FIRST ANSWER. 0x4b4af
         * `test eax,eax / jne 0x4b4d6` leaves the loop over
         * [world+0x70] as soon as one record answers, and 0x4b4ec then
         * raises AC_BLOK once. A body that spans the whole world width
         * covers BOTH edge walls - the left one answers (10 >= 2250 is
         * false, target -11, dx +11) and the right one is never asked,
         * let alone added: the second wall would have answered -11 and
         * the two would have cancelled. */
        CHECK(ll_block_walk(&bl, -10, 2250, 100, 140, &d, &idx) &&
              idx == 0 && d == 11);
        /* File order, not nearest: the same six walls, asked by a body
         * only the LAST row reaches, answer at index 5. */
        CHECK(ll_block_walk(&bl, 1600, 1700, 100, 140, &d, &idx) &&
              idx == 5 && d == 21);
        CHECK(!ll_block_walk(&bl, 500, 600, 600, 650, &d, &idx) &&
              idx == -1 && d == 0);
        ll_blkreg_free(&bl);

        /* THE GRAMMAR, AS THE LOADER READS IT. One 0x400-byte read and
         * a token cursor (0x4db7b), the header count first (0x4db8e),
         * then the record scan run EXACTLY count times
         * (0x4dbcf-0x4dbe4) - so lines mean nothing and the count wins
         * in both directions. */
        ll_blkreg g;
        CHECK(ll_blkreg_parse("2\r\n0,0,700\r\n", &g) == 0 && g.n == 2 &&
              g.r[1].x == 0 && g.r[1].y == 0 && g.r[1].ly == 0);
        ll_blkreg_free(&g);
        CHECK(ll_blkreg_parse("1\r\n0,0,700\r\n5,5,5\r\n", &g) == 0 &&
              g.n == 1 && g.r[0].x == 0 && g.r[0].y == 0 &&
              g.r[0].ly == 700);
        ll_blkreg_free(&g);
        CHECK(ll_blkreg_parse("2 0,0,700 2240,0,700", &g) == 0 && g.n == 2 &&
              g.r[1].x == 2240 && g.r[1].ly == 700);
        ll_blkreg_free(&g);
        CHECK(ll_blkreg_parse("1\r\n0,\r\n 0, 700\r\n", &g) == 0 && g.n == 1 &&
              g.r[0].x == 0 && g.r[0].y == 0 && g.r[0].ly == 700);
        ll_blkreg_free(&g);
        CHECK(ll_blkreg_parse("1\r\n1,2,3,4\r\n", &g) == 0 && g.n == 1 &&
              g.r[0].x == 1 && g.r[0].y == 2 && g.r[0].ly == 3);
        ll_blkreg_free(&g);
        CHECK(ll_blkreg_parse("0\r\n", &g) == 0 && g.n == 0);
        ll_blkreg_free(&g);
        CHECK(ll_blkreg_parse("nope\r\n0,0,700\r\n", &g) == 1 && g.n == 0);
        CHECK(ll_blkreg_parse("65\r\n", &g) == 1 && g.n == 0);
        ll_blkreg_free(&g);
    }

    /* .suf grammar and the ceiling query, PR-396. The bytes are the
     * shipped ceiling registries exactly as the HD ETAPY container
     * carries them - KADMAR.07/KADMAR.SUF is 15 bytes, KARA.07/
     * KARA.SUF is 37, and SADES.06's SUFITY.DES sub-member is 76
     * (/tmp/opencode/pr396_suf_des.txt; six non-empty registries ship
     * in all, beside seven whose only token is `0`) - and the body is
     * written as the THREE numbers
     * 0x45500 actually reads: the anchor point it builds from the
     * body's own getter slot +0xb0 plus the pending delta
     * ([obj+0xca], [obj+0xce]) and the body box's near y from slot
     * +0x68, which the engine compares with NO delta added. Both getters
     * are the ones PR-305..PR-308 read: slot +0x68 is 0x23900
     * (`add eax,0xd6`), the point object at [obj+0xd6]/[obj+0xda] whose
     * fields PR-306 and PR-307 show are refreshed by the class's +0x1c
     * verb and not every step, and slot +0xb0 is 0x544f0, which copies
     * the LIVE [obj+0]/[obj+4] and then an attachment path for a body
     * flagged at [obj+0xb6]. Live and stored are different positions, so
     * these pins state the arithmetic of 0x45500, not a lion pose. */
    {
        static const char ST_SUF_KADMAR[] = "1\r\n0,0,3000\r\n\r\n";
        static const char ST_SUF_KARA[] =
            "3\r\n0,0,1600\r\n125,290,10\r\n1535,40,10\r\n";
        /* The other three shipped registries are not `.SUF` files at
         * all: they are the SUFITY.DES member of the level's own slot
         * (the sub-member shape PR-392 named for platformy=), and the
         * ten `none.des` names resolve to that slot's NONE.DES, whose
         * whole content is the token `0`. Bytes verbatim from
         * /tmp/opencode/pr396_suf_des.txt. */
        static const char ST_DES_SADES[] =
            "6\r\n0,185,1000\r\n47,997,241\r\n495,1384,178\r\n41,1817,53\r\n"
            "55,2621,272\r\n0,0,1000\r\n";
        static const char ST_DES_TUNGA2[] =
            "4\r\n0,613,358\r\n0,740,358\r\n0,-40,2600\r\n0,235,233\r\n";
        ll_sufreg su;
        int d, idx;
        CHECK(ll_sufreg_parse(ST_SUF_KADMAR, &su) == 0 && su.n == 1);
        CHECK(su.r[0].x == 0 && su.r[0].y == 0 && su.r[0].lx == 3000);
        /* KADMAR's one row is the whole map's top: a line at y 0 from
         * x 0 to x 3000 (0,0 and 3000 - the same number the level's
         * own width runs to). */
        ll_sufreg_free(&su);
        CHECK(ll_sufreg_parse(ST_SUF_KARA, &su) == 0 && su.n == 3);
        CHECK(su.r[0].x == 0 && su.r[0].y == 0 && su.r[0].lx == 1600);
        CHECK(su.r[1].x == 125 && su.r[1].y == 290 && su.r[1].lx == 10);
        CHECK(su.r[2].x == 1535 && su.r[2].y == 40 && su.r[2].lx == 10);
        ll_sufreg_free(&su);

        /* A CEILING IS A LINE, NOT A BAND. 0x45555 and 0x45562 keep
         * the record unless ax < rec.x or rec.x + rec.lx < ax, so both
         * ends are INCLUSIVE: the anchor at x 0 and at x 3000 is asked
         * (and pulled to y 0 by dy = 0 - ay), one px past either end
         * is not. */
        CHECK(ll_sufreg_parse(ST_SUF_KADMAR, &su) == 0);
        CHECK(ll_ceiling_hit(&su.r[0], 0, -5, 0, &d) && d == 5);
        CHECK(ll_ceiling_hit(&su.r[0], 3000, -5, 0, &d) && d == 5);
        CHECK(!ll_ceiling_hit(&su.r[0], -1, -5, 0, &d) && d == 0);
        CHECK(!ll_ceiling_hit(&su.r[0], 3001, -5, 0, &d) && d == 0);
        /* THE THIRD TEST IS ON THE BODY'S TOP, and it is a one-way
         * test: 0x4556c leaves when [esi+4] < rec.y, so a body whose
         * top is ABOVE the line is not asked at all - the line is not
         * above it. At top == rec.y it is. */
        CHECK(!ll_ceiling_hit(&su.r[0], 100, -5, -1, &d) && d == 0);
        CHECK(ll_ceiling_hit(&su.r[0], 100, -5, 0, &d) && d == 5);
        /* AND THE FOURTH IS STRICT: rec.y - ay must be >= 1
         * (0x45576 `sub`, 0x45578 `test / jle`). An anchor already on
         * the line, or below it, is left exactly where it is and
         * [obj+0xce] is never written - which is why these read d == 0.
         * There is no distance-of-1 exemption here as there is for a
         * blockade: the engine never refuses a move of 1. */
        CHECK(!ll_ceiling_hit(&su.r[0], 100, 0, 0, &d) && d == 0);
        CHECK(ll_ceiling_hit(&su.r[0], 100, -1, 0, &d) && d == 1);
        CHECK(!ll_ceiling_hit(&su.r[0], 100, 5, 5, &d) && d == 0);
        /* RAN.SUF's one row, 20,100,28800: the line runs from x 20 to
         * x 28820 at y 100. */
        ll_sufreg_free(&su);
        CHECK(ll_sufreg_parse("1\r\n20,100,28800\r\n", &su) == 0 && su.n == 1);
        CHECK(ll_ceiling_hit(&su.r[0], 20, 99, 100, &d) && d == 1);
        CHECK(ll_ceiling_hit(&su.r[0], 28820, 99, 100, &d) && d == 1);
        CHECK(!ll_ceiling_hit(&su.r[0], 19, 99, 100, &d) && d == 0);
        CHECK(!ll_ceiling_hit(&su.r[0], 28821, 99, 100, &d) && d == 0);
        CHECK(!ll_ceiling_hit(&su.r[0], 100, 100, 100, &d) && d == 0);
        ll_sufreg_free(&su);

        /* THE SIX-ROW REGISTRY, SADES.06. Its header is 6, the largest
         * authored count in the shipped set, and its rows cross the
         * whole height of the level - y 0, 185, 997, 1384, 1817, 2621 -
         * so this is the file that shows a ceiling is a set of lines at
         * DIFFERENT heights, not one map-top. Asked as the engine walks
         * it, the first row whose line the body straddles answers: at
         * ax 500, ay 1300, top 1400 that is row 2 (495..673 at y 1384)
         * and dy is 1384 - 1300 = 84, which lands the anchor on the
         * line. A body entirely below every line it spans (ay 1400,
         * top 1400) answers nothing, because rec.y - ay is negative. */
        CHECK(ll_sufreg_parse(ST_DES_SADES, &su) == 0 && su.n == 6);
        CHECK(su.r[2].x == 495 && su.r[2].y == 1384 && su.r[2].lx == 178);
        CHECK(su.r[5].y == 0 && su.r[5].lx == 1000);
        CHECK(ll_ceiling_walk(&su, 500, 1300, 1400, &d, &idx) && idx == 2 &&
              d == 84);
        CHECK(!ll_ceiling_walk(&su, 500, 1400, 1400, &d, &idx) && idx == -1);
        /* AND THE FIRST ANSWER WINS EVEN WHEN A LATER ROW IS CLOSER:
         * rows 0 and 5 both cover x 100 (0,185,1000 and 0,0,1000) and
         * row 5's line at y 0 is nearer a body at ay -300, but the walk
         * takes row 0, the earlier one in the file. */
        CHECK(ll_ceiling_walk(&su, 100, -300, 200, &d, &idx) && idx == 0 &&
              d == 485);
        ll_sufreg_free(&su);
        /* TUNGA2.12 AUTHORS A CEILING ABOVE THE WORLD: y -40. The y
         * gate is the ordinary one, so it answers for a body whose top
         * is at or below -40 and dy = -40 - ay lands it there. */
        CHECK(ll_sufreg_parse(ST_DES_TUNGA2, &su) == 0 && su.n == 4);
        CHECK(su.r[2].x == 0 && su.r[2].y == -40 && su.r[2].lx == 2600);
        CHECK(ll_ceiling_hit(&su.r[2], 0, -140, -40, &d) && d == 100);
        CHECK(!ll_ceiling_hit(&su.r[2], 0, -40, -40, &d));
        ll_sufreg_free(&su);
        /* NONE.DES, the ten levels' answer to the same key: a count of
         * 0, no allocation, and the walk's count test skips everything -
         * ZARIA.01 among them, which is why the OI-078 climb has no
         * ceiling to hit. */
        CHECK(ll_sufreg_parse("0\r\n", &su) == 0 && su.n == 0 && su.r == NULL);
        CHECK(!ll_ceiling_walk(&su, 100, 100, 200, &d, &idx) && idx == -1);

        /* KARA.SUF, THREE LINES, AND THE WALK STOPS AT THE FIRST
         * ANSWER: 0x4b82d `test eax,eax / je 0x4b80c` moves on only
         * while the answer is no, so a body that BOTH the wide y 0 line
         * and the y 290 line reach is answered by the one that comes
         * FIRST IN FILE ORDER - the second would have moved the body
         * 295 px and is never asked. */
        CHECK(ll_sufreg_parse(ST_SUF_KARA, &su) == 0);
        CHECK(ll_ceiling_walk(&su, 130, -5, 300, &d, &idx) && idx == 0 &&
              d == 5);
        /* The y 0 line refuses a body whose anchor is already below it
         * (0 - 100 <= 0), so the second row, x 125..135 at y 290, is
         * the one that answers: dy = 290 - 100 = 190. */
        CHECK(ll_ceiling_walk(&su, 130, 100, 300, &d, &idx) && idx == 1 &&
              d == 190);
        /* and the third row, x 1535..1545 at y 40. */
        CHECK(ll_ceiling_walk(&su, 1540, 30, 45, &d, &idx) && idx == 2 &&
              d == 10);
        /* Nothing reaches: the anchor is outside every line's x. */
        CHECK(!ll_ceiling_walk(&su, 2000, 500, 600, &d, &idx) && idx == -1 &&
              d == 0);
        ll_sufreg_free(&su);

        /* A ceiling with a length of 0 is ONE POINT, x == rec.x, and
         * one px either side is nothing. No shipped row is zero-length;
         * this is the law. */
        {
            const ll_suf_rec pt = { 7, 50, 0 };
            CHECK(ll_ceiling_hit(&pt, 7, 49, 50, &d) && d == 1);
            CHECK(!ll_ceiling_hit(&pt, 6, 49, 50, &d) && d == 0);
            CHECK(!ll_ceiling_hit(&pt, 8, 49, 50, &d) && d == 0);
        }
        /* A NEGATIVE length answers nowhere. The two x tests are
         * ORDERED - ax >= rec.x and rec.x + rec.lx >= ax - unlike the
         * blockade's y tests, which look at both ends symmetrically, so
         * the upside-down range that PR-395 pinned for `.BLK` is empty
         * here. No shipped row is negative; this is the law. */
        {
            const ll_suf_rec inv = { 100, 50, -40 };
            CHECK(!ll_ceiling_hit(&inv, 80, 49, 60, &d) && d == 0);
            CHECK(!ll_ceiling_hit(&inv, 100, 49, 60, &d) && d == 0);
            CHECK(!ll_ceiling_hit(&inv, 60, 49, 60, &d) && d == 0);
        }

        /* THE GRAMMAR IS THE ONE `.BLK` USES, because the loader is
         * the same shape: one 0x400-byte read and a token cursor
         * (0x4dabb), the header count first (0x4dac0), then the record
         * scan EXACTLY count times (0x4db12-0x4db2e). A count of 0
         * allocates nothing and leaves [world+0x78] at 0 (0x4dadf). */
        CHECK(ll_sufreg_parse("2\r\n0,0,10\r\n", &su) == 0 && su.n == 2 &&
              su.r[1].x == 0 && su.r[1].y == 0 && su.r[1].lx == 0);
        ll_sufreg_free(&su);
        CHECK(ll_sufreg_parse("1\r\n0,0,3000\r\n5,5,5\r\n", &su) == 0 &&
              su.n == 1 && su.r[0].lx == 3000);
        ll_sufreg_free(&su);
        CHECK(ll_sufreg_parse("1 0 , 0 , 3000 ", &su) == 0 && su.n == 1 &&
              su.r[0].x == 0 && su.r[0].y == 0 && su.r[0].lx == 3000);
        ll_sufreg_free(&su);
        CHECK(ll_sufreg_parse("1\r\n1,2,3,4\r\n", &su) == 0 && su.n == 1 &&
              su.r[0].x == 1 && su.r[0].y == 2 && su.r[0].lx == 3);
        ll_sufreg_free(&su);
        CHECK(ll_sufreg_parse("0\r\n", &su) == 0 && su.n == 0);
        ll_sufreg_free(&su);
        CHECK(ll_sufreg_parse("nope\r\n0,0,3000\r\n", &su) == 1 && su.n == 0);
        CHECK(ll_sufreg_parse("9\r\n", &su) == 1 && su.n == 0);
        ll_sufreg_free(&su);
        /* LL_SUF_MAX is the PORT's cap, and the boundary is pinned so
         * the cap cannot drift: the shipped set never goes past 3, but
         * a file at the cap parses and one past it is refused. */
        CHECK(ll_sufreg_parse("8\r\n0,0,1\r\n0,0,2\r\n0,0,3\r\n0,0,4\r\n"
                              "0,0,5\r\n0,0,6\r\n0,0,7\r\n0,0,8\r\n",
                              &su) == 0 && su.n == 8 && su.r[7].lx == 8);
        ll_sufreg_free(&su);
    }

    /* .MOV extended grammar: physics numbers, flags, transitions, per-state
     * seq, M4-C clamp (frames say 6 ms -> 100) */
    CHECK(ll_mov_parse(ST_MOV, &m) == 3);
    CHECK(ll_mov_find(&m, "A_WALK_FWD") == 1);
    CHECK(m.st[1].fx == 400 && m.st[1].tx == 90 &&
          (m.st[1].ctrl & LL_CTRL_BY_X) && (m.st[1].ctrl & LL_CTRL_CHECK_LN));
    CHECK(strcmp(m.st[1].seqkey, "uj09") == 0);
    CHECK(m.st[1].ntr == 2 && m.st[1].tr[0].tidx == 0 &&
          m.st[1].tr[0].c[0].cond == LL_COND_K_FWD && m.st[1].tr[0].c[0].neg == 1);
    /* the self-target line parses too (its (..) token is the non-numeric
     * "x" - real KADMAR2 data - so entry stays 0 = top), and matches its
     * own state */
    CHECK(m.st[1].tr[1].entry == 0);                 /* "(x)": not numeric */
    CHECK(m.st[0].tr[0].entry == 1);                 /* "(1)": start row   */
    CHECK(m.st[1].tr[1].tidx == 1 &&
          m.st[1].tr[1].c[0].cond == LL_COND_K_FWD &&
          m.st[1].tr[1].c[0].neg == 0);
    CHECK(m.st[1].fr[0].thresh == 6);                /* authored values kept */

    /* PR-87: ',' groups are AND ('|' alternatives stay as before): the
     * unmodeled token poisons only its own group; a leading '|' is an
     * empty group (no-op). The reading of ',' is argued in ll_play.h.
     * PR-264 REPLACES that grammar for the MATCH (one mask + one
     * operation, 0x47d60/0x47e40) but the group split stays in the
     * capture, and the operation byte the parser records is pinned
     * here too: a '|' writes 3, anything else leaves the cleared 0. */
    ll_mov_t mfg;
    ll_mov_parse("A_STAY\nA_FIGHT\n[A_STAY]\r\n"
                 "A_FIGHT(1)=K_FIRE,K_F1\r\n"
                 "A_FIGHT(2)=K_F2|K_FIRE,K_ZOO\r\n"
                 "A_FIGHT(3)=|K_ALT\r\n", &mfg);
    CHECK(mfg.st[0].ntr == 3);
    CHECK(mfg.st[0].tr[0].nc == 2 &&
          mfg.st[0].tr[0].c[0].cond == LL_COND_K_FIRE &&
          mfg.st[0].tr[0].c[1].cond == LL_COND_K_F1 &&
          mfg.st[0].tr[0].c[0].grp == mfg.st[0].tr[0].c[1].grp);
    CHECK(mfg.st[0].tr[1].nc == 3 &&
          mfg.st[0].tr[1].c[0].grp != mfg.st[0].tr[1].c[1].grp &&
          mfg.st[0].tr[1].c[1].cond == LL_COND_K_FIRE &&
          mfg.st[0].tr[1].c[2].cond == LL_COND_OTHER);
    CHECK(mfg.st[0].tr[2].nc == 1 &&
          mfg.st[0].tr[2].c[0].cond == LL_COND_K_ALT);
    /* PR-264: the operation byte the parser records per clause,
     * exactly as 0x47d60 writes it (clear at 0x47d73, '|' -> 3 at
     * 0x47dbc; ',' and bare names leave the byte alone). */
    CHECK(mfg.st[0].tr[0].op == LL_TOP_SUBSET);
    CHECK(mfg.st[0].tr[1].op == LL_TOP_ANY);
    CHECK(mfg.st[0].tr[2].op == LL_TOP_ANY);

    ll_mov_t m2;
    ll_mov_parse("X\n[A]\n0,0\n1,-5\n", &m2);
    CHECK(m2.nst == 1 && m2.st[0].fr[0].thresh == 100 &&
          m2.st[0].fr[1].thresh == 100);                 /* M4-C clamp */
    CHECK(strcmp(m.seqkey, "uj09") == 0);

    /* runtime: STAY -> WALK_FWD, the F/T steady state of the law, edge
     * fall, landing fires the K_GEN2 pulse, walk then reverse and stop.
     * PR-135 RE-DERIVED THE NUMBERS OF THIS LEG at the iteration grid
     * (one call = one iteration) and under the engine law; PR-256 re-
     * derives them for the arithmetic that body really runs (idiv at
     * every step, on the integer velocity register):
     *   A_WALK_FWD is F=400, T=90 (the real RAN LEW.MOV values), so per
     *   iteration the force adds idiv(400*10, 20) = 200 units of the
     *   register and the drag takes idiv(90*v, 100) of what is there.
     *   The fixed point is the largest v with idiv(90*v,100) = 200, i.e.
     *   223 units = 44.6 u/s (the clean algebra's 400*10/90 = 44.44 plus
     *   one register unit, because idiv rounds the drag down), inside
     *   PR-134's lion ground-run bracket [38,76] units/s; and the STEP
     *   is idiv(223,100) = 2 whole px per iteration, not 2.22 (the
     *   half-px is dropped, see section 4). ST_PLT's first line ends at
     *   x=700 - 200 units from the spawn at 500 - so the edge arrives
     *   about 100 iterations in, inside the leg's 400-step bound. */
    ll_plt pl;
    CHECK(ll_plt_parse(ST_PLT, &pl) == 0);
    ll_player p;
    ll_player_start(&p, &m, 500, 100, 1);
    CHECK(p.grounded && strcmp(m.st[p.sti].name, "A_STAY") == 0);

    ll_play_keys k = { 0 };
    k.k_fwd = 1;
    int ev = ll_player_step(&p, &pl, &k, 0, 2000, LL_ITER_MS);
    CHECK((ev & LL_PLAY_EV_STATE) && p.sti == 1);
    CHECK(p.face == 1);                                /* PR-82: walking right */
    int fell = 0, landed = 0, stev = 1, maxfi = 0;
    long long add = (long long)400 * LL_PLAY_INTENT_FULL / 20;
    long long vcap = ((add + 1) * 100 - 1) / 90;
    for (int t = 0; t < 400; t++) {                 /* walk until the pit */
        ev = ll_player_step(&p, &pl, &k, 0, 2000, LL_ITER_MS);
        if (ev & LL_PLAY_EV_AIR)
            fell = t;
        if (ev & LL_PLAY_EV_STATE)
            stev++;
        if (p.fi > maxfi)
            maxfi = p.fi;
        if (ev & LL_PLAY_EV_GROUND) {
            landed = t;
            break;
        }
        /* the law's own ceiling, in the register's unit: the force adds
         * idiv(F*s,20) = idiv(400*10,20) = 200 units per iteration, and
         * the steady state is the LARGEST register whose drag still
         * equals that add, idiv(90*v,100) == 200, i.e. v <= ((200+1) *
         * 100 - 1)/90 = 223 units. The truncation makes the ceiling one
         * unit of the register ABOVE the clean algebra's 400*10/90 =
         * 222.2: idiv rounds the drag DOWN, so it brakes a hair less. */
        CHECK(ll_play_v_raw(p.vx) <= vcap);
        if (p.grounded && t > 2)
            CHECK(p.y == 100);
    }
    CHECK(fell && landed > fell);
    /* KADMAR2's real "A_MASZERUJ(x)=K_FWD" pattern (a state that
     * re-targets ITSELF while its own key is held; here added to the
     * mini .MOV as A_WALK_FWD(x)=K_FWD): keeping the state must neither
     * restart its frame list nor report a transition - measured before
     * the fix: one A_MASZERUJ -> A_MASZERUJ event per tick, the walk
     * animation frozen on frame 1. */
    CHECK(stev == 1);
    CHECK(maxfi > 0);
    CHECK(p.grounded && p.y == 150);
    /* the momentum kept across the fall IS the walking steady state:
     * the x physics runs every iteration, in the air as on the ground,
     * and 90 of them converged on F*s/T = 44.44 u/s */
    CHECK(p.vx > 400.0 * LL_PLAY_INTENT_FULL / 90.0 - 0.05);
    CHECK((p.sig >> LL_SIG_K_GEN2) & 1);               /* landing pulse */

    k.k_fwd = 0;
    ev = ll_player_step(&p, &pl, &k, 0, 2000, LL_ITER_MS);
    CHECK((ev & LL_PLAY_EV_STATE) && p.sti == 0);   /* !K_FWD -> STAY   */
    for (int t = 0; t < 60; t++)
        ll_player_step(&p, &pl, &k, 0, 2000, LL_ITER_MS);
    CHECK(ll_play_v_raw(p.vx) / 100 == 0);
                               /* s = 0: the authored T=90 drag is the
                                * brake. It does NOT reach 0 in the
                                * register - below about 11 units the
                                * idiv(T*v,100) drag truncates to 0 and
                                * the residue stays (0x5430a-0x54337
                                * zeroes a SIGN cross, nothing zeroes a
                                * small magnitude). What does stop is the
                                * STEP: [obj+0xca] = idiv(v,100) = 0, so
                                * the body stands still, and that is the
                                * observable this check states. */

    k.k_bkwd = 1;
    ev = ll_player_step(&p, &pl, &k, 0, 2000, LL_ITER_MS);
    CHECK((ev & LL_PLAY_EV_STATE) && p.sti == 2 && p.walkdir == -1);
    CHECK(p.face == -1);                               /* PR-82: walking left  */
    double x0 = p.x;
    for (int t = 0; t < 20; t++)                    /* short reverse: stay
                                                     * on the lower shelf
                                                     * (edge sits at x=500,
                                                     * we land ~13 px past
                                                     * it) */
        ll_player_step(&p, &pl, &k, 0, 2000, LL_ITER_MS);
    CHECK(p.x < x0 - 10 && p.y == 150 && p.grounded);
    for (int t = 0; t < 40; t++) {                  /* frame cursor valid */
        int fi = p.fi;
        CHECK(fi >= 0 && fi < m.st[p.sti].nfr);
        CHECK(ll_player_frame(&p) >= 0);
        ll_player_step(&p, &pl, &k, 0, 2000, LL_ITER_MS);
    }
    /* PR-82: facing follows the walk (the fixture's control flags are
     * the REAL RAN LEW ones: both walk states carry CH_DIR, A_STAY does
     * not; all frames positive = the art faces right). While walking
     * left in a CH_DIR state face is -1, and it STAYS -1 in A_STAY
     * (instance state, like lvl_obj.face): a lion that walked left and
     * stopped keeps facing left - no state in the real data flips back.
     * The draw/collision mirror = authored sign XOR face. */
    CHECK(p.face == -1);
    CHECK(ll_player_mirror_of(&p, 0) == 1);          /* plain frame -> mirrored */
    CHECK(ll_player_mirror_of(&p, 1) == 0);          /* authored mir -> plain   */
    k.k_bkwd = 0;
    ev = ll_player_step(&p, &pl, &k, 0, 2000, LL_ITER_MS);   /* !K_BKWD -> A_STAY    */
    CHECK((ev & LL_PLAY_EV_STATE) && p.sti == 0);
    CHECK(p.face == -1);            /* no CH_DIR there: facing untouched    */
    /* PR-85 (facing-relative keys, see cond_true): the bear now FACES
     * LEFT, so the RIGHT key is his K_BKWD. A_STAY's line
     * A_WALK_BKWD(1)=K_BKWD matches: walkdir = -face = RIGHT (walkdir is
     * always the literal arrow), CH_DIR re-aligns face to the movement.
     * A frame later that key's MEANING flipped with the facing, so
     * A_WALK_BKWD's only line A_STAY(1)=!K_BKWD holds: the model
     * flickers through A_STAY and settles into A_WALK_FWD - the state
     * pair whose name tracks the facing, and the only one carrying the
     * self-sustaining line. That one-tick flicker is how RAN's data
     * turns around (it has no turn state); ZARIA authors A_TURN for it,
     * see the ZM fixture. The physics stay clean: the arrow walks him
     * along itself. */
    k.k_fwd = 1;
    ev = ll_player_step(&p, &pl, &k, 0, 2000, LL_ITER_MS);
    CHECK((ev & LL_PLAY_EV_STATE) && p.sti == 2);    /* A_WALK_BKWD         */
    CHECK(p.walkdir == 1 && p.face == 1);            /* RIGHT arrow, CH_DIR */
    ev = ll_player_step(&p, &pl, &k, 0, 2000, LL_ITER_MS);
    CHECK((ev & LL_PLAY_EV_STATE) && p.sti == 0);    /* !K_BKWD now TRUE    */
    ev = ll_player_step(&p, &pl, &k, 0, 2000, LL_ITER_MS);
    CHECK((ev & LL_PLAY_EV_STATE) && p.sti == 1);    /* settled A_WALK_FWD  */
    CHECK(p.walkdir == 1 && p.face == 1);
    double x1 = p.x;
    for (int t = 0; t < 25; t++)                     /* 0.5 s: the flicker  */
        ll_player_step(&p, &pl, &k, 0, 2000, LL_ITER_MS);    /* the drag brakes */
    CHECK(p.x > x1 + 10);           /* then re-accelerate: walks RIGHT,    */
    CHECK(p.face == 1);             /* facing RIGHT again                  */
    CHECK(ll_player_mirror_of(&p, 0) == 0);          /* plain frame, plain draw */
    CHECK(ll_player_mirror_of(&p, 1) == 1);          /* authored mir -> mirrored */
    /* Release + press LEFT: the mirror-symmetric flow (A_STAY's K_BKWD
     * line at face=+1 is now the literal LEFT key - one A_WALK_BKWD
     * tick, back through A_STAY, settled in A_WALK_FWD walking LEFT
     * with face=-1: the same state runs the walk either way, RAN's
     * state NAMES bake the author's frame polarity (frames positive =
     * art faces right) while the CH_DIR engine flag tracks the
     * MOVEMENT, exactly as PR-82 measured. A left-moving bear in
     * A_WALK_FWD draws left-facing: the plain frames render mirrored. */
    k.k_fwd = 0;
    ev = ll_player_step(&p, &pl, &k, 0, 2000, LL_ITER_MS);   /* !K_FWD -> A_STAY    */
    CHECK((ev & LL_PLAY_EV_STATE) && p.sti == 0);
    k.k_bkwd = 1;
    ev = ll_player_step(&p, &pl, &k, 0, 2000, LL_ITER_MS);
    CHECK((ev & LL_PLAY_EV_STATE) && p.sti == 2);    /* A_WALK_BKWD tick    */
    ev = ll_player_step(&p, &pl, &k, 0, 2000, LL_ITER_MS);
    CHECK((ev & LL_PLAY_EV_STATE) && p.sti == 0);    /* !K_BKWD holds now   */
    ev = ll_player_step(&p, &pl, &k, 0, 2000, LL_ITER_MS);
    CHECK((ev & LL_PLAY_EV_STATE) && p.sti == 1);    /* settled A_WALK_FWD  */
    CHECK(p.walkdir == -1 && p.face == -1);          /* walking LEFT        */
    CHECK(ll_player_mirror_of(&p, 0) == 1);          /* mirrored again      */
    CHECK(ll_player_mirror_of(&p, 1) == 0);          /* authored mir -> plain   */
    ll_plt_free(&pl);
    {   /* PR-253 RE-DERIVED THIS LEG at the iteration grid (one call =
         * one iteration). The fixture is ZARIA's own A_JUMP header:
         * `I 120,-1200` is the row-1 pull PAIR, and under the 0x54170
         * law it SETS v from rest to -1200/LL_PLAY_RAW_VDIV = -240 u/s
         * (and vx = +24, the authored I x=120, in the facing
         * direction); the vertical unit is no longer a fit. The
         * upward component of the pull clears `grounded` at the
         * moment it is applied - that is what makes a jump leave the
         * ground.
         * THE ROW PART runs on the iteration clock: A_JUMP's list is
         * 20-unit rows, and the first feed of 100 lands in the SAME
         * iteration that entered the state (transitions run first), so
         * row 0 closes and row 1's pull fires right there; with one
         * row admitted per iteration the 20-unit rows are one
         * iteration each, so under PR-523's default every row admit
         * IS an iteration and the physics pass runs per iteration
         * through the flight. (The list runs to 42 rows: the row
         * clock, not a fitted constant, keeps the state alive longer
         * than the air, and this fixture declares no AC_BREAK_NEXT
         * exit, so PR-86's fallback is the route.)
         * THE ARC, per physics pass = per row admit = per iteration
         * here (PR-256's exact arithmetic: the T_y=1
         * drag is idiv(v,100) of the OLD register, the authored G_y=55
         * is added first and that drag subtracted after, the sign test
         * compares the value before the drag with the one after it, and
         * the STEP is idiv(v,100) whole px - y grows down, the register
         * is the engine's own, hundredths of a px per iteration):
         *   step 0 (the call that enters JUMP): row 1 is admitted and
         *           its pull SETS the register to -1200; this step's
         *           add and drag leave -1133, and idiv(-1133,100) = -11
         *           moves y to 89                          air 1
         *   ... the register walks -1133, -1067, -1002, -937 ... and the
         *           steps walk -11, -10, -10, -9, -8, -8, -7, -6, -5,
         *           -5, -4, -3, -3, -2, -2, -1, 0, 0, 0: the sum is
         *           exactly 100 px, and y = 0 at step 16 is the APEX.
         *           Those last three steps are ZERO because idiv(v,100)
         *           truncates a register under 100 away - the body HANGS
         *           at the apex for whole iterations, which is what this
         *           arithmetic does and what PR-253's float model could
         *           not show. Nothing zeroes that register: 0x5430a and
         *           0x54325 zero a SIGN cross only, and there is none
         *           here - the drag is smaller than the add from the
         *           step where the register turns positive (+12) on
         *                                                        air 17..20
         *   ... the fall accelerates at the same authored +55 per
         *           iteration, the steps grow 0, 1, 1, 2, 2, 3, 4, 4, 5,
         *           5, ... and the landing test (vy >= 0, first
         *           surface below the contact line) snaps y back to
         *           100 and reports GROUND; the leg counts every
         *           ungrounded step: AIR = 39, the 40th step is
         *           grounded and ends the fallback into A_STAY.
         * The 20-unit rows admit on every feed, so the state stays
         * alive through the whole flight and the row list - not a
         * fitted constant - bounds it; the flight itself advances one
         * register step per admit, which for THIS list is every
         * iteration. The apex is arithmetic on the authored
         * words: no estimate remains in this leg, the PR-73/74 /4 +
         * 1650 fit is retired by this PR (see ll_play.h). */
        static const char JM[] =
            "A_STAY\nA_JUMP\n\n[A_STAY]\ncontrol=BY_TIME|CHECK_LN\n"
            "F=600,0\nT=90,90\nG=0,0\nA_JUMP(1)=K_JUMP\nseq=a\n1,100\n\n"
            "[A_JUMP]\ncontrol=BY_TIME\nF=180,0\nT=20,1\nG=0,55\nseq=a\n"
            "1,20\n2,20,I 120,-1200\n3,20\n4,20\n5,20\n6,20\n7,20\n"
             "8,20\n9,20\n10,20\n11,20\n12,20\n13,20\n14,20\n15,20\n"
             "16,20\n17,20\n18,20\n19,20\n20,20\n21,20\n22,20\n23,20\n"
             "24,20\n25,20\n26,20\n27,20\n28,20\n29,20\n30,20\n31,20\n"
             "32,20\n33,20\n34,20\n35,20\n36,20\n37,20\n38,20\n39,20\n"
             "40,20\n41,20\n42,20\n";
        static const char JP[] = "1\n1 0,100;2000,100;\n";
        ll_mov_t *jm = calloc(1, sizeof *jm);
        ll_plt jp;
        ll_player q;
        ll_play_keys jk = { 0 };
        double top = 100;
        int air = 0, back = 0;

        CHECK(jm && ll_plt_parse(JP, &jp) == 0);
        ll_mov_parse(JM, jm);
        CHECK(jm->st[1].fr[1].has_i && jm->st[1].fr[1].ix == 120 &&
              jm->st[1].fr[1].iy == -1200);
        ll_player_start(&q, jm, 500, 100, 1);
        jk.k_jump = 1;
        ll_player_step(&q, &jp, &jk, 0, 2000, LL_ITER_MS);       /* STAY -> JUMP */
        jk.k_jump = 0;
        CHECK(q.sti == 1);
        for (int t = 0; t < 80; t++) {
            ll_player_step(&q, &jp, &jk, 0, 2000, LL_ITER_MS);
            if (!q.grounded)
                air++;
            if (q.y < top)
                top = q.y;
            if (q.grounded && air > 3 && !back)
                back = t;
        }
        /* THE APEX, DERIVED IN THIS LEG rather than read off the run:
         * the register walk of 0x54250 on the fixture's own words
         * (T_y=1, G_y=55, the admitted row's I_y=-1200), with idiv at
         * every step. C's / on signed integers truncates toward zero
         * exactly as x86 idiv does, which is what 0x54290 and 0x5435d
         * execute. */
        {
            long long v = -1200, y = 0, apex = 0;
            while (v < 0) {
                long long drag = v / 100;          /* idiv(T_y * v, 100) */
                long long post = v + 55;           /* the authored G_y */
                v = post - drag;
                if (post * v < 0)
                    v = 0;                         /* the sign-cross zero */
                y += v / 100;                      /* [obj+0xce], whole px */
                if (-y > apex)
                    apex = -y;
            }
            CHECK(apex == 100);   /* the authored words' own number - the
                                   * height the same words give on the real
                                   * level, and what the play-side leg
                                   * tools/jump_law_measure.py derives from
                                   * them independently of this file. */
            CHECK(top == 100.0 - (double)apex);
        }
        CHECK(air == 39);
        CHECK(back == 39 && q.sti == 0 && q.y == 100 && q.vx == 0);
        ll_plt_free(&jp);
        free(jm);
    }
    {   /* PR-86: the (N) token = START ROW (1-based) of the TARGET's
         * frame list, and a state that DECLARES an AC_BREAK_NEXT exit
         * ends by itself - the landing does not cut it (that was the
         * old fallback viewer rule). A state whose list runs LONGER than
         * the physics air keeps playing through the landing, and only
         * the list WRAP arms AC_BREAK_NEXT -> back to the walk state
         * mid-phase ("A_WALK(7)" after ZARIA's 12-row cycle).
         * PR-135 RE-DERIVED EVERY NUMBER here for the iteration grid,
         * dt = LL_ITER_MS: one call is one feed, one row admit.
         * Row timeline (leg step: what closes) - the entry call itself
         * feeds, because the transition runs first inside the iteration.
         * The authored rows are 75,75,75,100,150,150,225,900,225,0,150
         * and M4-C raises that 0 to 100. PR-333 changed the last two:
         * the clause row carries `8,0,WAIT AC_DUP` - it never compares
         * the clock (0x52329 is not on its path) and it admits only
         * when the clause's mask, AC_DUP, is live. PR-523 amends the
         * middle: under the row-admit default a physics pass runs at an
         * ADMIT and nowhere else, so a list that stalls mid-air also
         * freezes the body mid-air - the arc would hang at y = 67 and
         * the landing that raises AC_DUP would never come. The leg
         * therefore inserts 21 filler rows of 100 units after the
         * 225-unit row (the real engine's held-row windows keep their
         * registers stepping - PR-523's capture note, OI-013 - and this
         * leg's OWNED grammar is the clause and the wrap exit, which
         * need the flight to reach its surface). The fillers admit one
         * row per step, so from step 21 every step carries a pass, and
         * the 30th pass of the arc lands the lion INSIDE the last
         * filler's admit: the row walk below, in list INDEX order:
         *   1 enter A_JUMP(1) = row index 0; closes row 0 (75) - the
         *     entry iteration's own feed; row 1 becomes CURRENT and
         *     its `F 0,350` head pulls vy = +350 raw
         *   2 closes row 1 (75) -> row 2 becomes CURRENT, and its
         *     `I 150,-1200` pull fires on that admit: vy = -850 raw,
         *     vx = +30 raw, grounded = 0 (PR-253: /LL_PLAY_RAW_VDIV)
         *   3 closes row 2 (75); 4 closes 3 (100); 5 closes 4 (150);
         *   7 closes 5 (150); 9 closes 6 (225) -> the 900-unit row
         *     (index 7) holds steps 9..17 - and the arc freezes with
         *     it, vy at -541 and y at 75;
         *   18 closes it (900); 20 closes 8 (225); the filler rows
         *     (indices 9..29, 100 units, row_t always leaves 25
         *     owed) admit one per step, 21 through 41.
         *   41 is the 30th pass of the arc: the surface-link tail
         *     clamps y to 100, zeroes the register, GROUNDS him, and
         *     the attach edge raises AC_DUP at the END of that step
         *     (PR-331); the airborne bit goes away; index 30, the
         *     clause row, is CURRENT.
         *   42 the row pass sees that bit - it reads the set as of the
         *     transition pass, one iteration after the raise - and the
         *     row admits with the ZERO budget of 0x522f8: the 25 units
         *     owed are dropped, not decremented.
         *   44 closes index 31 (150, from 200 with 50 carried), that
         *     being the last row, wraps the list, arms AC_BREAK_NEXT,
         *     and the `A_WALK(3)` match comes at the top of step 45 -
         *     on the ground, four steps after the surface.
         * Cumulative units closed by the clock path: 75, 150, 225, 325,
         * 475, 625, 850, 1750, 1975, +2100 (the fillers) - then a
         * clause that consumes nothing, then 150 more, and the 50
         * still owed.
         * The jump physics are that arithmetic (PR-256): the two
         * `F 0,350` heads fire on rows 0 and 1 while he is still
         * GROUNDED, the ground contact keeps the register out of the
         * y update until the `I 150,-1200` admit SETS the pair
         * (x +150 raw too), and from there the arc's passes - one per
         * admit, frozen between 9 and 17 by the 900-unit row - walk
         * -787, -725, -663, -602, -541, [held], -481, -422, -363,
         * -305, -247, -190, -134, -78, -23, +32, +87, +142, +196,
         * +250, +303, +355, +407, +458, +509, +559, +609, +658, +707;
         * the body reads +3, +3, -7, -7, -6, -6, -5, -4, -4, -3, -3,
         * -2, -1, then 0, 0, 0, 0 as the whole-px step truncates the
         * top away: the APEX is y = 57, reached at step 25 and held
         * flat to step 29 by the truncation, 43 px up, where PR-253's float model computed 48.13.
         * The truncation takes the fall's first steps too - but the
         * authored +55 per pass wins and, unlike the float model, HE
         * LANDS, at step 41, INSIDE A_JUMP, because the `WAIT AC_DUP`
         * row holds the list there. That is the real A_JUMP grammar
         * (ZARIA.01's own clause row, quoted in this fixture) and it
         * is what the engine does: the frames stop at the landing
         * pose until the surface arrives. The wrap exit at step 45
         * then moves him to A_WALK row 3, the `A_WALK(3)` seek, ON
         * the ground at y = 100. What this leg OWNS is the row
         * grammar - the (N) entry token, the wrap arming the state's
         * OWN exit at the seek's row, the carry across it - and since
         * PR-333 also the clause path: a row that waits for a signal,
         * the clock it never reads, and the zero budget that admit
         * carries. The older claim of this leg, that a state declaring
         * its own AC_BREAK_NEXT exit ends MID-AIR before the surface,
         * is gone with the gate: with a `WAIT AC_DUP` row the list
         * cannot reach its wrap while he is off the ground. A list
         * with no clause row still runs that way - the JM fixture
         * above keeps its own mid-air wrap exit, and its FALLBACK
         * branch (no declared exit, landing goes idle) too. */
        static const char JE[] =
            "A_WALK\nA_JUMP\n\n[A_WALK]\ncontrol=BY_TIME|CHECK_LN\n"
            "F=600,0\nT=90,90\nG=0,0\nA_JUMP(1)=K_JUMP\n"
            "A_WALK(3)=AC_BREAK_NEXT\nseq=a\n1,100\n2,100\n3,100\n4,100\n\n"
            "[A_JUMP]\ncontrol=BY_TIME\nF=180,0\nT=20,1\nG=0,55\nseq=a\n"
            "1,75,F 0,350\n2,75,F 0,350,smpauto\n2,75,I 150,-1200\n2,100\n"
            "3,150,FC 30,0\n4,150,GEN K_GEN1\n5,225,GEN K_GEN1\n"
            "6,900,GEN K_GEN1\n7,225,GEN K_GEN1\n"
            "9,100\n10,100\n11,100\n12,100\n13,100\n14,100\n15,100\n"
            "16,100\n17,100\n18,100\n19,100\n20,100\n21,100\n22,100\n"
            "23,100\n24,100\n25,100\n26,100\n27,100\n28,100\n29,100\n"
            "8,0,WAIT AC_DUP\n"
            "9,150,F 0,100\n"
            "A_WALK(3)=AC_BREAK_NEXT\n";
        ll_mov_t *je = calloc(1, sizeof *je);
        static const char JP2[] = "1\n1 0,100;2000,100;\n";
        ll_plt jp2;
        ll_player q;
        ll_play_keys jk = { 0 };
        double top2 = 100;
        int air2 = 0, land = 0, exit_t = 0, exit_fi = -1;

        CHECK(je && ll_plt_parse(JP2, &jp2) == 0);
        ll_mov_parse(JE, je);
        CHECK(je->st[1].nfr == 32);
        CHECK(je->st[1].fr[2].has_i && je->st[1].fr[2].ix == 150 &&
              je->st[1].fr[2].iy == -1200);
        CHECK(je->st[1].fr[7].thresh == 900u);      /* the 900-unit middle */
        CHECK(je->st[1].fr[30].thresh == 100u);     /* authored 0 -> 100   */
        CHECK(je->st[1].tr[0].tidx == 0 && je->st[1].tr[0].entry == 3);
        ll_player_start(&q, je, 500, 100, 1);
        CHECK(q.sti == 0);                           /* no A_STAY: the
                                                      * declared first   */
        jk.k_jump = 1;
        ll_player_step(&q, &jp2, &jk, 0, 2000, LL_ITER_MS); /* WALK -> JUMP(1)  */
        jk.k_jump = 0;
        CHECK(q.sti == 1);
        CHECK(q.fi == 1);           /* the entry iteration's own feed of
                                     * 100 closes row 0 (75) there */
        for (int t = 0; t < 55; t++) {              /* t = leg step - 2    */
            ll_player_step(&q, &jp2, &jk, 0, 2000, LL_ITER_MS);
            if (!q.grounded)
                air2++;
            if (q.y < top2)
                top2 = q.y;
            if (q.grounded && air2 > 3 && !land)
                land = t;                            /* first step with a
                                                      * surface under him */
            if (!exit_t && q.sti == 0) {             /* first re-entry   */
                exit_t = t + 2;
                exit_fi = q.fi;
            }
        }
        CHECK(top2 == 57.0);                /* the arc of this fixture's
                                             * own words: 43 px up */
        CHECK(air2 == 39 && land == 39);   /* the flight ENDS at the
                                             * surface, at loop step 41,
                                             * with the list held at its
                                             * AC_DUP row */
        CHECK(exit_t == 45 && exit_fi == 3);  /* the wrap, and the OWN
                                                * exit at the seek's row,
                                                * come AFTER that landing
                                                * - not mid-air as they
                                                * did before PR-333  */
        CHECK(land < exit_t);               /* ... 4 steps after the
                                             * surface: the clause row
                                             * cannot be passed while he
                                             * is airborne */
        CHECK(q.sti == 0 && q.grounded && q.y == 100.0);
        ll_plt_free(&jp2);
        free(je);
    }
    {   /* OI-092, PR-215: THE PLAYER-SIDE ROW SEEK. PR-174 shipped
         * the same-state match arm for scene OBJECTS; the engine arm
         * (0x53602-0x53626, re-read there) belongs to the record
         * applier and runs for the lion's own record exactly the same
         * way: the arm OPENS with the row write (`mov esi,[eax+0x4]`
         * 0x5360f, `cmp`, `je skip` 0x53612, `mov [eax+0x4],edx`
         * 0x53616) and calls NO ENTER, so the carry SURVIVES the
         * match (PR-172). PR-174's own census sized who this affects:
         * of the 101 authored self-target seeks with entry >= 2, 77
         * are LION-class lines and all 20 of ZARIA.01's are the
         * lion's own scripts (A_BORRING(10), A_DEAD(33), A_JUMP(11),
         * A_KILOF(13)/(25)...) - the object-path fix could not reach
         * a single one of them. So this is the half that can move
         * ZARIA. The port's player arm kept the state but never wrote
         * the row: the model choice this PR retires. */
        static const char RS[] =
            "A_ONE\nA_TWO\n\n[A_ONE]\ncontrol=BY_TIME\n"
            "F=600,0\nT=90,90\nG=0,0\n"
            "A_ONE()=K_GEN4\nA_ONE(3)=K_GEN2\nA_ONE(2)=K_GEN3\n"
            "A_ONE(9)=K_GEN5\nA_TWO(2)=K_GEN1\nseq=a\n"
            "1,70\n2,240\n3,70\n4,70\n5,70\n6,70\n\n[A_TWO]\n"
            "control=BY_TIME\nF=0,0\nT=0,0\nG=0,0\nseq=a\n"
            "1,300\n2,300\n";
        static const char RP[] = "1\n1 0,100;2000,100;\n";
        ll_mov_t *rm = calloc(1, sizeof *rm);
        ll_plt rp;
        ll_player q;
        ll_play_keys rk = { 0 };

        CHECK(rm && ll_plt_parse(RP, &rp) == 0);
        CHECK(ll_mov_parse(RS, rm) == 2 && rm->st[0].ntr == 5 &&
              rm->st[0].nfr == 6 && rm->st[1].nfr == 2);
        ll_player_start(&q, rm, 500, 100, 1);
        CHECK(q.sti == 0);
        /* EVERY STEP HERE IS ONE ITERATION (dt == LL_ITER_MS): the
         * legs pin exact carry arithmetic, which is the whole point
         * of the row write (the engine's arm writes the row and
         * hands the SAME accumulator over - PR-172). */
        ll_player_step(&q, &rp, &rk, 0, LL_ITER_MS, LL_ITER_MS);
        /* the opening iteration: row 0 (gate 70) closes, fi 1,
         * remainder 30 */
        CHECK(q.fi == 1 && q.row_t == 30u);
        /* LEG A: the ABSENT/NON-NUMERIC token is NO WRITE at all. The
         * engine stores -1 for it (0x51e3e) and its row-set method
         * REFUSES a negative value (`test edx,edx` + `jl` at 0x52381,
         * PR-107). The data settles which reading is the engine's: of
         * the 217 authored self-target lines, 22 carry a HELD
         * DIRECTION condition (K_FWD/K_BKWD) and 19 of those 22 have
         * no numeric token (census over HD ANIMACJE, this PR) - "write
         * the top" would restart the walk list on every iteration of a
         * held arrow, and the walk gait PR-79/PR-134 measured on
         * video (9-12 poses per cycle at ~120 ms per pose) does not
         * do that. So no numeric token, no seek; the row clock runs
         * on: fi 1, t 130. */
        q.sig = 1ull << LL_SIG_K_GEN4;
        ll_player_step(&q, &rp, &rk, 0, LL_ITER_MS, LL_ITER_MS);
        CHECK(q.sti == 0 && q.fi == 1 && q.row_t == 130u);
        /* LEG B: the seek to (3) lands on row 2 and KEEPS the state.
         * The row write is the arm's FIRST act (0x5360f-0x53616),
         * with no ENTER and no state-change bookkeeping: no event, no
         * impulse, no feed reset. The row clock runs on from there:
         * the carried 130 plus this iteration's 100 = 230 admits and
         * closes the SEEKED row (gate 70), so fi lands 3 with the
         * carry 160. WITHOUT the write the state still sits on row 1,
         * the 240-unit row: 230 < 240, and the step lands fi 1/t 230
         * (measured, this PR's mutation run). */
        q.sig = 1ull << LL_SIG_K_GEN2;
        ll_player_step(&q, &rp, &rk, 0, LL_ITER_MS, LL_ITER_MS);
        CHECK(q.sti == 0 && q.fi == 3 && q.row_t == 160u);
        /* LEG C: the seek is LIVE afterwards too - from row 3 (gate
         * 70) the carried 160 + 100 closes again, fi 4, t 190. A
         * state-change reset of the carry would land t 30; the
         * no-write port stood on row 1 and lands fi 2/t 90. */
        ll_player_step(&q, &rp, &rk, 0, LL_ITER_MS, LL_ITER_MS);
        CHECK(q.sti == 0 && q.fi == 4 && q.row_t == 190u);
        /* LEG D: a (2) re-seeks to row 1, the 240-unit row, from row
         * 4, and that row is LIVE at once: the carried 190 + 100 =
         * 290 closes it, so fi lands 2 with the carry 50. The
         * no-write port was on row 2 at t 90 and lands fi 3/t 120. */
        q.sig = 1ull << LL_SIG_K_GEN3;
        ll_player_step(&q, &rp, &rk, 0, LL_ITER_MS, LL_ITER_MS);
        CHECK(q.sti == 0 && q.fi == 2 && q.row_t == 50u);
        /* LEG E: an OVER-RANGE seek (9 with 6 rows) keeps the port's
         * recorded PR-107 clamp - the top. It is a real write here
         * too: from row 2 at t 50 it re-seeks to row 0, whose 70
         * this iteration closes (150-70 = 80), landing fi 1/t 80.
         * The no-write port was on row 3 and lands fi 4/t 150. */
        q.sig = 1ull << LL_SIG_K_GEN5;
        ll_player_step(&q, &rp, &rk, 0, LL_ITER_MS, LL_ITER_MS);
        CHECK(q.sti == 0 && q.fi == 1 && q.row_t == 80u);
        /* LEG F: the clamped row stays LIVE - held at fi 1, the
         * 240-unit row, because 80+100 = 180 does not reach it, so
         * the carry only accumulates to 180. The no-write port
         * carries THE SAME 180 here but on row 5: the row index is
         * the whole content of the difference. */
        ll_player_step(&q, &rp, &rk, 0, LL_ITER_MS, LL_ITER_MS);
        CHECK(q.sti == 0 && q.fi == 1 && q.row_t == 180u);
        /* LEG G: a CROSS-state seek still takes the TARGET's row
         * (the PR-107 rule: A_TWO(2) -> index 1, not its top) and
         * the cross-state arm's reset of the feed carry (PR-172). */
        q.sig = 1ull << LL_SIG_K_GEN1;
        ll_player_step(&q, &rp, &rk, 0, LL_ITER_MS, LL_ITER_MS);
        CHECK(q.sti == 1 && q.fi == 1 && q.row_t == 100u);
        ll_plt_free(&rp);
        free(rm);
    }
    {   /* PR-87: the ',' AND really ANDs. A_FIGHT(1)=K_FIRE,K_F1 is
         * ZARIA's exact attack-entry line: FIRE alone never matches;
         * plus F1 it does. (The same AND-gate on REAL ZARIA.01 data
         * - FIRE alone / F1 alone stay idle, FIRE,F1 fights - is
         * pinned by the verify-play zfight leg.) K_Fn and K_ALT are now
         * simulated keys, so the old "whole comma line is one OTHER,
         * never true" is gone for these tokens. */
        static const char FK[] =
            "A_STAY\nA_FIGHT\n\n[A_STAY]\ncontrol=BY_TIME|CHECK_LN\n"
            "F=600,0\nT=90,90\nG=0,0\nA_FIGHT(1)=K_FIRE,K_F1\nseq=a\n"
            "1,100\n\n[A_FIGHT]\ncontrol=BY_TIME\nF=0,0\nT=0,0\nG=0,0\n"
            "seq=a\n1,300\n2,300\n";
        ll_mov_t *fk = calloc(1, sizeof *fk);
        ll_player q;
        ll_play_keys kk = { 0 };
        CHECK(fk);
        ll_mov_parse(FK, fk);
        CHECK(fk->st[0].tr[0].nc == 2 &&
              fk->st[0].tr[0].c[1].cond == LL_COND_K_F1);
        ll_player_start(&q, fk, 500, 100, 1);
        kk.k_fire = 1;
        ll_player_step(&q, NULL, &kk, 0, 2000, LL_ITER_MS);
        CHECK(q.sti == 0);                  /* FIRE alone: no   */
        kk.k_f1 = 1;
        ll_player_step(&q, NULL, &kk, 0, 2000, LL_ITER_MS);
        CHECK(q.sti == 1);                  /* AND holds: fight */
        free(fk);
    }
    {   /* PR-285: THE FREEZE IS AN AUTHORING IDIOM, NOT A PHYSICS
         * HOLE. A state that authors `T=<x>,100` with no y force
         * holds its body where it is, and the project's gravity
         * stand-in must NOT add what the author chose not to author.
         * DERIVED FROM THE PORT'S OWN CLOSED FORM, NOT FROM ITS
         * OUTPUT: ll_play_v_step (0x54250, PR-256) computes
         * drag = idiv(T_y*v,100) from the OLD register, then
         * post = v + adds, then out = post - drag. With T_y = 100
         * the drag IS v, so out = adds; and with the authored F pair
         * second word 0, no intent, and G_y = 0 there IS no add:
         * the register is zero after the FIRST iteration from any
         * starting value, and section 4 moves the body by
         * idiv(register,100) = 0 px. The words themselves are the
         * hold. THE DATA SAYS THIS IS HOW THE GAME STOPS BODIES:
         * a census of the 777 .MOV texts (/tmp/opencode/mov, this
         * PR) counts 2318 state blocks - 683 author a non-zero G_y,
         * 697 author G=0,0 and 938 author no G line at all, and the
         * engine has NO default gravity anywhere: every state that
         * falls authors its own word (ZARIA.01 A_FALL_DOWN G=0,55
         * with T=30,1; A_DRAP G=0,250; A_WALK G=30,350). The blocks
         * that hold a body in the air are exactly the drag-100 +
         * zero-force idiom: 26 player (LEW) blocks author T_y = 100
         * with G zero or absent, and they are the hold states by
         * name - A_DEAD (11), A_SCIAG (5), A_PODCIAG (5), A_DEAD1,
         * A_FINISH_LEVEL, A_JUMP_DEAD/_BK/_FW. ZARIA.01's own
         * A_PODCIAG is one of them: F=0,0 T=100,100 G=0,0 and not
         * one moving row (PR-284 MEASURED (e)).
         * LEG 1 - the authored hold. The words are A_PODCIAG's own.
         * The register is seeded RISING with -60 px/iteration = the
         * -300 register units a real A_WSP climb carries (PR-259's
         * equilibrium), because that is the case the ladder shows:
         * the climb's velocity must DIE, and the body must stay put.
         * Derived: iteration 1 takes drag = idiv(100*-300,100) =
         * -300, post = -300 + 0, out = 0 (and the sign test's
         * post*out = 0 does not fire), so y never moves: 20
         * iterations, y = 100 exactly, register 0.
         * LEG 2 - the stand-in keeps its remaining scope. The same
         * state with T_y = 50 leaves the register OPEN, which is the
         * PR-106 case the stand-in was counted for. Under PR-523's
         * default a physics pass runs at each ROW ADMIT, and the
         * three 300-unit rows admit at iterations 3, 6 and 9, so the
         * leg steps 9 iterations to take exactly three passes.
         * Derived step by step with LL_PLAY_GRAVITY = 200: v = 0 +
         * 200 - 0 = 200 (y += idiv(200,100) = 2 -> 102), v = 200 +
         * 200 - 100 = 300 (y += 3 -> 105), v = 300 + 200 - 150 =
         * 350 (y += 3 -> 108). The fixed point of a 200 add against
         * a 50% drag is idiv(50*v,100) = 200, v = 400, i.e. 4 px a
         * pass; the leg checks the register is climbing toward it. */
        static const char HZ[] =
            "[A_PODCIAG]\ncontrol=BY_TIME|CHECK_ALL\nF=0,0\n"
            "T=100,100\nG=0,0\nseq=lift\n1,300\n2,300\n3,300\n"
            "4,200\n5,200\n6,200\n7,500,rdir\n8,300\n9,200\n";
        static const char OP[] =
            "[A_PODCIAG]\ncontrol=BY_TIME|CHECK_ALL\nF=0,0\n"
            "T=100,50\nG=0,0\nseq=lift\n1,300\n2,300\n3,300\n";
        ll_mov_t *hz = calloc(1, sizeof *hz), *op = calloc(1, sizeof *op);
        ll_player q, r;
        ll_play_keys hk = { 0 };
        CHECK(hz && op);
        ll_mov_parse(HZ, hz);
        ll_mov_parse(OP, op);
        CHECK(hz->st[0].ty == 100 && hz->st[0].gy == 0);

        ll_player_start(&q, hz, 500, 100, 0);
        CHECK(!q.grounded);                 /* NULL ground: in the air */
        q.vy = -60.0;                       /* -300 register units up  */
        for (int t = 0; t < 20; t++)
            ll_player_step(&q, NULL, &hk, 0, 2000, LL_ITER_MS);
        CHECK(q.y == 100);
        CHECK(ll_play_v_raw(q.vy) == 0);

        ll_player_start(&r, op, 500, 100, 0);
        for (int t = 0; t < 9; t++)
            ll_player_step(&r, NULL, &hk, 0, 2000, LL_ITER_MS);
        CHECK(r.y == 108);
        CHECK(ll_play_v_raw(r.vy) == 350);
        free(hz);
        free(op);
    }
    {   /* PR-263: K_ANY IS THE INPUT-KEY MASK, NOT "some key is held".
         * The engine's name resolver 0x48000 answers AC_K_ANY/K_ANY
         * (the strings at 0x919ec/0x919f5) by CLEARING the set and OR-ing
         * the constant 0x2001f8 into its second dword (0x48029-0x48048).
         * In the signal layout that is bits 3,4,5,6,7,8 and 21 of dword 1
         * = signals 35,36,37,38,39,40,53 = K_BKWD, K_FWD, K_UP, K_DOWN,
         * K_FIRE, K_ALT, K_JUMP - and those are exactly the seven bits the
         * input bridge 0x54910 raises out of [obj+0xa5] bits 0/4/5,
         * [obj+0xb6] bit 4 and the intent words [obj+0xba]/[obj+0xbe].
         * The weapon selectors K_F1..K_F4 (41..44, bits 9..12 of the same
         * dword) are NOT in the constant. So the fixture words below -
         * ZARIA.01's `[A_TO_BOR] A_DRAP(1)=|K_ANY` and SADES.06's
         * `[A_...] A_LAY(1)=!K_ANY`, both shipped - answer to K_F2 with
         * the OPPOSITE of what "any key" would: an any-row refuses it and
         * a not-any-row takes it. Nothing here is read off this port's
         * evaluator: the expectation is the mask against the keys. */
        static const char AM[] =
            "A_STAY\nA_WALK\n\n[A_STAY]\ncontrol=BY_TIME|CHECK_LN\n"
            "F=600,0\nT=90,90\nG=0,0\nA_WALK(1)=|K_ANY\nseq=a\n"
            "1,100\n\n[A_WALK]\ncontrol=BY_TIME\nF=0,0\nT=0,0\nG=0,0\n"
            "seq=a\n1,300\n2,300\n";
        static const char AN[] =
            "A_STAY\nA_LAY\n\n[A_STAY]\ncontrol=BY_TIME|CHECK_LN\n"
            "F=600,0\nT=90,90\nG=0,0\nA_LAY(1)=!K_ANY\nseq=a\n"
            "1,100\n\n[A_LAY]\ncontrol=BY_TIME\nF=0,0\nT=0,0\nG=0,0\n"
            "seq=a\n1,300\n2,300\n";
        ll_mov_t *am = calloc(1, sizeof *am), *an = calloc(1, sizeof *an);
        ll_player q;
        ll_play_keys kk = { 0 };
        CHECK(am && an);
        ll_mov_parse(AM, am);
        ll_mov_parse(AN, an);
        /* PR-264 capture pins: `=|K_ANY` carries operation 3 on the
         * resolver's whole mask, `=!K_ANY` operation 1 on the same
         * mask (0x47dbc / 0x47dc8 / 0x48029-0x48048). */
        CHECK(am->st[0].tr[0].op == LL_TOP_ANY);
        CHECK(an->st[0].tr[0].op == LL_TOP_NOT);
        CHECK(ll_mov_tr_mask(&am->st[0].tr[0]) == LL_SIG_K_ANY_MASK);
        ll_player_start(&q, am, 500, 100, 1);
        kk.k_f2 = 1;                       /* a selector key: not in the */
        ll_player_step(&q, NULL, &kk, 0, 2000, LL_ITER_MS);
        CHECK(q.sti == 0);                 /* mask, so `|K_ANY` refuses  */
        kk.k_jump = 1;                     /* K_JUMP (53) IS in it        */
        ll_player_step(&q, NULL, &kk, 0, 2000, LL_ITER_MS);
        CHECK(q.sti == 1);
        ll_player_start(&q, an, 500, 100, 1);
        kk = (ll_play_keys){ .k_f2 = 1 };  /* and `=!K_ANY` is the exact */
        ll_player_step(&q, NULL, &kk, 0, 2000, LL_ITER_MS);
        CHECK(q.sti == 1);                 /* mirror: it takes the same  */
        ll_player_start(&q, an, 500, 100, 1);
        kk = (ll_play_keys){ .k_alt = 1 }; /* key the any-row refused.  */
        ll_player_step(&q, NULL, &kk, 0, 2000, LL_ITER_MS);
        CHECK(q.sti == 0);                 /* K_ALT (40) is in the mask  */
        free(am);
        free(an);
    }
    {   /* PR-264 red: the comma is NOT an AND when the clause carries
         * an operation. The clause parser 0x47d60 compiles ONE mask and
         * ONE operation byte per condition - it clears the byte at
         * 0x47d73, a '|' token writes 3 (0x47dbc), '*' writes 2
         * (0x47dc2), '!' writes 1 (0x47dd0/0x47dc8), a ',' writes
         * nothing and folds its name into the SAME mask (0x47dfd-
         * 0x47de5) - and TEventSet::match 0x47e40 picks its comparator
         * from that byte: 3 is "the AND of the two sets is NOT empty"
         * (PR-260). So in `A_WALK(1)=|K_JUMP,K_UP`, the exact words of
         * ENA.03 line 551 and ENA2 line 213, BOTH names sit in one
         * mask and the operation is intersects: holding K_UP ALONE is
         * enough. The expectation is read from that table, not from
         * this port's answer. */
        static const char AP[] =
            "A_STAY\nA_WALK\n\n[A_STAY]\ncontrol=BY_TIME|CHECK_LN\n"
            "F=600,0\nT=90,90\nG=0,0\nA_WALK(1)=|K_JUMP,K_UP\nseq=a\n"
            "1,100\n\n[A_WALK]\ncontrol=BY_TIME\nF=0,0\nT=0,0\nG=0,0\n"
            "seq=a\n1,300\n2,300\n";
        ll_mov_t *ap = calloc(1, sizeof *ap);
        ll_player q;
        ll_play_keys kk = { 0 };
        CHECK(ap != NULL);
        ll_mov_parse(AP, ap);
        kk.k_up = 1;                       /* K_UP (38) alone: the AND  */
        ll_player_start(&q, ap, 500, 100, 1);
        ll_player_step(&q, NULL, &kk, 0, 2000, LL_ITER_MS);
        CHECK(q.sti == 1);                 /* intersects, not subset     */
        kk = (ll_play_keys){ .k_jump = 1 };/* K_JUMP (53) alone: the same*/
        ll_player_start(&q, ap, 500, 100, 1);
        ll_player_step(&q, NULL, &kk, 0, 2000, LL_ITER_MS);
        CHECK(q.sti == 1);                 /* answer                   */
        kk.k_jump = 1; kk.k_up = 1;        /* both: true in either      */
        ll_player_start(&q, ap, 500, 100, 1);
        ll_player_step(&q, NULL, &kk, 0, 2000, LL_ITER_MS);
        CHECK(q.sti == 1);                 /* reading - no divergence   */
        kk = (ll_play_keys){ 0 };          /* neither: the empty AND    */
        ll_player_start(&q, ap, 500, 100, 1);
        ll_player_step(&q, NULL, &kk, 0, 2000, LL_ITER_MS);
        CHECK(q.sti == 0);                 /* stays false               */
        free(ap);
    }
    {   /* PR-82: states whose DIRECTION IS AUTHORED stay out of facing.
         * The real shape is RAN LEW's A_GO_TO_BALON_L: control=BY_X with
         * NO CH_DIR and frames authored NEGATIVE (-1..-12), while the
         * walking states (A_WALK_FWD/BKWD, all-positive frames) all carry
         * CH_DIR. So the authored sign is what the author used to aim
         * those states, and facing is left exactly as the CH_DIR walks
         * set it (instance state, like lvl_obj.face).
         * ASSERTS THE CHOSEN MODEL's consequence, not a measured pixel:
         * the draw mirror is always authored XOR face (the one
         * video-pinned case, PR-81's worm, works that way - its states
         * all carry CH_DIR). Whether the engine skips the instance facing
         * ENTIRELY for a no-CH_DIR state is the open question recorded
         * in HANDOFF (test that would settle it: a mirrored-polarity
         * match on the RAN crown/balloon cutscene, or any left-facing
         * standing lion on video - this game video walks right-only). */
        static const char GL[] =
            "A_STAY\nA_WALK_BKWD\nA_GO_TO_BALON_L\n\n[A_STAY]\n"
            "control=BY_TIME\nF=600,0\nG=0,0\nA_WALK_BKWD(1)=K_BKWD\n"
            "seq=uj09\n1,100\n\n[A_WALK_BKWD]\ncontrol=BY_X|CH_DIR\n"
            "F=400,0\nT=90,90\nG=0,0\nA_GO_TO_BALON_L(1)=K_GEN1\n"
            "seq=uj09\n1,100\n2,100\n\n"
            "[A_GO_TO_BALON_L]\ncontrol=BY_X\nF=0,0\nT=90,90\nG=400,0\n"
            "seq=uj09\n-1,1\n-2,1\n-3,1\n";
        static const char GP[] = "1\n1 0,100;2000,100;\n";
        ll_mov_t *gm = calloc(1, sizeof *gm);
        ll_plt gp;
        ll_player q;
        ll_play_keys gk = { 0 };

        CHECK(gm && ll_plt_parse(GP, &gp) == 0);
        CHECK(ll_mov_parse(GL, gm) == 3);
        CHECK(!(gm->st[2].ctrl & LL_CTRL_CH_DIR) &&
              (gm->st[1].ctrl & LL_CTRL_CH_DIR));
        ll_player_start(&q, gm, 500, 100, 1);
        gk.k_bkwd = 1;
        ll_player_step(&q, &gp, &gk, 0, 2000, LL_ITER_MS);   /* -> walk left     */
        CHECK(q.sti == 1 && q.face == -1);
        gk.k_bkwd = 0;
        q.sig |= 1ull << LL_SIG_K_GEN1;                /* K_GEN1 trigger   */
        ll_player_step(&q, &gp, &gk, 0, 2000, LL_ITER_MS);   /* -> the baked state */
        CHECK(q.sti == 2);
        CHECK(q.walkdir == -1);                      /* the transition set it */
        CHECK(q.face == -1);                         /* but facing is untouched */
        {   /* and what gets drawn: authored mirror (1) XOR face (1) = 0
             * under the chosen always-XOR model. PR-135 re-derived the
             * frame index: this state's rows are `-1,1 / -2,1 / -3,1`,
             * so the entry iteration's own feed of 100 (a BY_X state
             * with F=0 is not a walking one, and its rows run on the
             * time feed - PR-114) closes row 0 right there and row 1's
             * authored -2 frame is what is current. */
            CHECK(ll_player_frame(&q) == -2);        /* the row-1 frame */
            ll_frame_ref fr = ll_mov_frame_ref(ll_player_frame(&q));
            CHECK(fr.idx == 1 && fr.mirror == 1 && !fr.blank);
            CHECK(ll_player_mirror_of(&q, fr.mirror) == 0);
        }
        ll_plt_free(&gp);
        free(gm);
    }
    {   /* PR-83 (the two user reports, data-free regression on the shapes
         * measured from ZARIA LEW.ZAR): the declared list - not block
         * order - names the spawn state; A_WALK is the combined
         * idle+walk (NO !K_FWD exit anywhere: stopping comes from the
         * key-gated target velocity + the authored F brake); BKWD plays
         * A_TURN once and it STAYS while the key is held; and the row
         * `I 100,0` horizontal pull pair (A_TURN row 0) never lifts
         * the feet. Also guards: `time=` is parsed but NOT acted on, and the
         * frame-list wrap does not arm AC_TIME.
         * PR-110 CHANGED THE ROW TIMING HERE, and the row values used
         * are now ZARIA's real ones (A_WALK: 12 rows of 6, `GEN
         * AC_TIME` on list indices 0,1,5,6,7; A_TURN: 5 rows of 200 =
         * 1000 units, the `I 100,0` on the first, F=0,0). A_WALK is
         * BY_X, so its rows are gated by DISTANCE (0x543a0 mode 0,
         * PR-98/PR-100 - the feed ll_player_step section 4 now has):
         * no distance, no row. That replaces the old ms-clock claim
         * "standing in A_WALK cycles the 100 ms frame list and the
         * GEN AC_TIME tail drops to the idle hub", which was a
         * property of the ms clock, not of the data: with the key held
         * the state's own `A_WALK(x)=K_FWD` line comes first in the
         * list and wins (first match wins), and with the key released
         * the rows do not advance at all. Whether AC_TIME has another
         * source in the engine, or those five ZARIA rows are
         * vestigial, is a new open item (NEXT_STEPS PR-110); what is
         * NOT open is the feed. */
        static const char ZM[] =
            "A_WALK\nA_DRAP\nA_TURN\nA_JUMP_DOWN\n\n"
            "[A_JUMP_DOWN]\ncontrol=BY_TIME\nF=0,0\nG=0,0\nseq=a\n1,100\n\n"
            "[A_WALK]\ncontrol=BY_X|CH_DIR\nF=100,0\nT=20,5\nG=0,0\n"
            "A_TURN(1)=K_BKWD\nA_WALK(x)=K_FWD\nA_DRAP(1)=AC_TIME\n"
            "seq=a\n10,6,GEN AC_TIME\n11,6,GEN AC_TIME\n12,6\n1,6\n2,6\n"
            "3,6,GEN AC_TIME\n4,6,GEN AC_TIME\n5,6,GEN AC_TIME\n6,6\n"
            "7,6\n8,6\n9,6\n\n"
            "[A_DRAP]\ncontrol=BY_TIME\nF=100,0\nT=60,0\nG=0,0\n"
            "A_TURN(1)=K_BKWD\nA_WALK(1)=K_FWD\nseq=a\n1,100\n\n"
            "[A_TURN]\ncontrol=BY_TIME|CH_DIR\nF=0,0\nG=0,0\n"
            "A_DRAP(1)=AC_BREAK_NEXT\nA_DRAP(1)=AC_BREAK_PREV\n"
            "seq=a\n-5,200,I 100,0\n-4,200\n3,200\n4,200\n5,200\n";
        static const char ZP[] = "1\n1 0,100;2000,100;\n";
        ll_mov_t *zm = calloc(1, sizeof *zm);
        ll_plt zp;
        ll_player s;
        ll_play_keys sk = { 0 };

        CHECK(zm && ll_plt_parse(ZP, &zp) == 0);
        CHECK(ll_mov_parse(ZM, zm) == 4);
        /* the header names A_WALK first but its .MOV block is SECOND here
         * (A_JUMP_DOWN block leads, exactly like ZARIA's file): start_st
         * follows the declaration, and there is no A_STAY to prefer. */
        CHECK(zm->start_st == 1 && ll_mov_find(zm, "A_JUMP_DOWN") == 0);
        ll_player_start(&s, zm, 500, 100, 1);
        CHECK(s.sti == 1 && strcmp(zm->st[1].name, "A_WALK") == 0);
        /* zero input: A_WALK is BY_X, so NOTHING feeds its row
         * accumulator (0x543a0 mode 0 adds the distance travelled, and
         * the lion is not travelling) - the 12-row list does not move,
         * no row completes, no `GEN AC_TIME` is raised, no idle hub is
         * reached, and he never drifts. PR-110's non-twin test for the
         * player-side mode feed: MEASURED against HEAD f86280c with the
         * scratch probe /tmp/opencode/zm_probe.c, that same fixture on
         * the ms clock ran 3 rows per 20 ms step (6 "ms" each), raised
         * AC_TIME inside tick 1 and entered A_DRAP at tick 2. Under the
         * mode feed the leg's 12 steps of 20 ms (240 ms) advance no row
         * at all AND accrue no wall time: the mode-0 branch of the
         * dispatcher replaces the 100-per-iteration feed, it does not
         * add to it, so the engine's one accumulator stays exactly 0 -
         * row_t and row_rem say so. A port that fed both clocks would
         * read 1200 units and a wrapped list here. */
        double zx = s.x;
        int idle_at = 0;
        for (int t = 1; t <= 12; t++) {
            ll_player_step(&s, &zp, &sk, 0, 2000, LL_ITER_MS);
            if (s.sti == 2)
                idle_at = t;
        }
        CHECK(idle_at == 0 && s.sti == 1 && s.fi == 0);
        CHECK(s.facc == 0.0 && s.row_t == 0u && s.row_rem == 0u);
        CHECK(s.x == zx);                              /* zero input never creeps */
        for (int t = 0; t < 5; t++)
            ll_player_step(&s, &zp, &sk, 0, 2000, LL_ITER_MS);
        CHECK(s.x == zx);
        CHECK(s.row_t == 0u && s.row_rem == 0u && s.facc == 0.0);
                                                       /* 17 steps = 340 ms
                                                        * in a mode-0 state:
                                                        * no wall feed, ever */
         /* PRESS FWD. The state's own A_WALK(x)=K_FWD line re-arms the
          * key-gated mode (a self-target, no reset). ZM's [A_WALK] is
          * the REAL ZARIA.01 [A_WALK]: F=100, T=20, twelve rows of 6
          * units. PR-135 makes this fixture the port's measured law leg,
          * because (F,T)=(100,20) is exactly the pair PR-134 measured on
          * video as the lion's ground run, [38,76] units/s.
          * THE ARITHMETIC, from the constants in this fixture, in the
          * engine's own register unit (PR-256, at the PR-523 intent
          * scale): the force adds idiv(100*20, 20) = +100 per PASS and
          * the drag takes idiv(20*v, 100) = idiv(v,5) of the OLD
          * register, so the register walks, one step per PASS,
          *   100, 180, 244, 296, ... -> 500 = F*s/T,
          * the fixed point of a 100 add against a 20% drag. THE PASS
          * IS THE ROW ADMIT (PR-523): one runs at the key edge - the
          * press call itself, with no admit - and one per admit after
          * that, and the BY_X feed is the persisting whole-px step of
          * the last pass. The STEP is idiv(v,100), so the feed is 1
          * an iteration while the register is under 200 and 2 from
          * 244 on, and the rows of 6 units close at
          *   N = 7, 13, 16, ...
          * At the fixed point (register 500, step 5) an admit comes
          * every 6/5 iterations: 5 px per 1.2 iterations = 83 u/s of
          * travel, ABOVE PR-134's measured [38,76]. The live capture
          * says why the port's gait runs hot - the real engine steps
          * its registers EVERY iteration (PR-523's A_WALK windows),
          * while the port's row-admit gate makes the row list the
          * physics clock too - and that is the same named open item,
          * OI-013, not a new one. What this half still OWNS, unchanged,
          * is that the authored row distance and the authored F/T,
          * through the engine's arithmetic, give the gait: every row
          * close below is derived from them. */
         sk.k_fwd = 1;
         ll_player_step(&s, &zp, &sk, 0, 2000, LL_ITER_MS);
         CHECK(s.sti == 1);
         zx = s.x;
         int badlaw = 0;
         long long vwant = 0, dwant = 0;    /* the register walk, derived
                                            * here from this fixture's own
                                            * words: the force adds
                                            * idiv(F*s,20) = idiv(100*20,20)
                                            * = 100 per PASS, the drag
                                            * takes idiv(T*v,100) = idiv(v,5)
                                            * of the OLD register, the step
                                            * is idiv(v,100) whole px, and a
                                            * pass is a row admit. C's /
                                            * truncates toward zero as x86
                                            * idiv does. */
         long long wfac = 0;                /* the leg's copy of the BY_X
                                            * feed: the persisting step */
         int wdx = 0, wclose = 0;           /* step of the last pass, and
                                            * the row closes */
         {   long long d1 = vwant / 5, p1 = vwant + 100;
             vwant = p1 - d1;                       /* the key-edge pass of
                                                     * the press call: +100
                                                     * and one px - which
                                                     * this leg took into
                                                     * zx already, so it is
                                                     * NOT counted in dwant;
                                                     * its step is what
                                                     * feeds the row clock */
             wdx = (int)(vwant / 100);
         }
         for (int t = 0; t < 17; t++) {      /* 18 iterations of the walk in
                                              * all: enough to show the gait
                                              * and to let the release slide
                                              * end on the list's FIFTH row,
                                              * the last plain one before
                                              * its GEN AC_TIME rows (see
                                              * the release note below) */
             wfac += wdx < 0 ? -wdx : wdx;   /* the row clock at the head */
             if (wfac >= 6) {                /* a row of 6 closes: THE pass */
                 wfac -= 6;
                 wclose++;
                 long long vd = vwant / 5, vp = vwant + 100;
                 vwant = vp - vd;
                 dwant += vwant / 100;       /* [obj+0xca], the only thing
                                              * that moves the body */
                 wdx = (int)(vwant / 100);
             }
             ll_player_step(&s, &zp, &sk, 0, 2000, LL_ITER_MS);
             if (ll_play_v_raw(s.vx) != vwant)
                 badlaw = 1;                 /* the closed form above, not a
                                              * copy of the model's recursion */
             CHECK(vwant <= 500);            /* the register's steady state:
                                              * idiv(v,5) equals the 100 the
                                              * force adds from v = 500 on;
                                              * this loop reaches 296 and a
                                              * long walk would settle at
                                              * 500 = 5 px a pass admitted
                                              * every 1.2 iterations - the
                                              * 83 u/s noted above. */
             if (s.row_t != 0u || s.row_rem != 0u)
                 badlaw = 1;             /* section 7 must NOT feed a walking
                                          * BY_X state: 0x543a2's mode-0
                                          * branch replaces the 100-per-
                                          * iteration feed */
             if (s.fi != wclose % 12)
                 badlaw = 1;                 /* the gait, row by row, from
                                              * the feed above: the rows of 6
                                              * close at iterations 7, 13, 16
                                              * at the step of 1 then 2 px */
         }
         CHECK(!badlaw);
         CHECK(s.sti == 1);                   /* the first two rows raised
                                              * AC_TIME, but this state's own
                                              * A_WALK(x) line is earlier in
                                              * the list and first match
                                              * wins (the engine's table
                                              * order, PR-86) */
         CHECK(dwant == 5 && s.x == zx + 5.0);
         CHECK(s.fi == 3);                    /* three of the twelve rows */
         /* RELEASE (this is where the SDL KEYUP fix enters the picture -
         * a stuck key would keep walking forever). NO !K_FWD exit
         * exists, so the key simply stops the force: the next row
         * admits run the drag alone, v - idiv(v,5). The carry
         * (wfac = 4 of the 6-unit row, a step of 2) survives the
         * release exactly as it survived the state clock, so the
         * slide's passes come at steps 1, 4 and 10 of this loop:
         *   237 (+2 px), 190 (+1 px), 152 (+1 px).
         * The third of those closes `3,6,GEN AC_TIME` - ZARIA.01's
         * own row (lines 136-143, copied into this fixture) - and
         * with no key held the state's own `A_DRAP(1)=AC_TIME` line
         * matches on the very next iteration. THAT is the game's
         * real brake, a STATE and not a term, and this leg now
         * measures it end to end: the walk routes to the idle hub
         * twelve steps after the key let go, with a register of 152
         * still moving.
         * The hub's own header is the real A_DRAP's `T=60,0` (PR-91
         * quoted it in the HR fixture's note: "a drag of 60% of the
         * register per iteration"): the hub's one 100-unit row admits
         * EVERY iteration, so the hub runs a pass every iteration and
         * the drag walks the register down - 61, 25, 10, 4, 2, 1.
         * It stops at the residue 1: below idiv(60*v,100) = 1 the
         * drag term itself truncates to zero, and nothing in
         * 0x54250 zeroes a small magnitude - only a sign cross.
         * The BODY already stopped one step earlier: idiv(25,100)
         * and below is 0 px. The old release note's "the slide is
         * 4 px in four iterations inside A_WALK" became this routed,
         * hub-braked stop when PR-523's gate made the gait fast
         * enough to reach a GEN AC_TIME row - the route PR-256 had
         * measured but not pinned is pinned here now.
         * (One row earlier, at the row `2,6`, the slide would have
         * stopped inside A_WALK instead - the fifth row the slide
         * closes carries no GEN tail; which row a slide lands on is
         * the authored list's business, and both halves of that
         * list are in this fixture on purpose.) */
        double xr = s.x;
        int rel_idle = 0;
        long long vbrk = vwant, dbrk = 0;   /* the A_WALK slide: the same
                                             * drag with the force's add at
                                             * s = 0, applied per ADMIT on
                                             * the carried row clock */
        long long rhub = 0;                 /* the register once the hub
                                             * owns it: T=60,0 decays it */
        sk.k_fwd = 0;
        for (int t = 1; t <= 30; t++) {     /* long enough to reach the
                                             * residue: 152 -> 1 takes the
                                             * hub six admitting steps */
            if (!rel_idle) {
                wfac += wdx < 0 ? -wdx : wdx;   /* the row clock carries */
                if (wfac >= 6) {                /* closes at steps 1, 4, 10 */
                    wfac -= 6;
                    wclose++;
                    vbrk -= vbrk / 5;
                    dbrk += vbrk / 100;
                    wdx = (int)(vbrk / 100);
                }
            } else {
                rhub -= rhub * 60 / 100;        /* the hub admits every step,
                                                 * so a pass is every step */
                dbrk += rhub / 100;
            }
            ll_player_step(&s, &zp, &sk, 0, 2000, LL_ITER_MS);
            if (s.sti == 2 && !rel_idle) {
                rel_idle = t;                   /* AC_TIME matched: the route */
                rhub = vbrk;
                rhub -= rhub * 60 / 100;        /* the entry iteration admits
                                                 * its own row at once */
                dbrk += rhub / 100;
            }
            if (ll_play_v_raw(s.vx) != (s.sti == 2 ? rhub : vbrk))
                badlaw = 1;
            if (s.sti == 1 && s.fi != wclose % 12)
                badlaw = 1;                     /* rows 4, 5, 6 close on the
                                                 * way down - 6 is the
                                                 * GEN AC_TIME row */
        }
        CHECK(rel_idle == 11 && s.sti == 2);    /* the route lands on hub row
                                                 * 1, one iteration after the
                                                 * GEN row closed */
        CHECK(!badlaw && vbrk == 152 && rhub == 1 && rhub / 100 == 0);
                                               /* the hub's residue stays in
                                                * the register; the step is
                                                * zero */
        CHECK(dbrk == 4 && s.x == xr + 4.0);    /* the A_WALK slide moves the
                                                 * body; the hub's decay moves
                                                 * it 0 px */
        double xs = s.x;
        for (int t = 0; t < 15; t++)
            ll_player_step(&s, &zp, &sk, 0, 2000, LL_ITER_MS);
        CHECK(s.sti == 2 && ll_play_v_raw(s.vx) == 1);
        CHECK(s.x == xs);                      /* the slide is over: a held
                                                * nothing means no motion.
                                                * The register still holds
                                                * its residue: nothing in
                                                * this arithmetic zeroes
                                                * it (see the release note
                                                * above), and PR-135's
                                                * LL_PLAY_V_EPS zeroing is
                                                * gone with PR-256 - the
                                                * idiv IS the model. */
        /* BKWD (the left key, the user's "turns but never walks left"):
         * A_TURN plays, and the REAL exit is AC_BREAK_NEXT - armed by
         * the frame-list WRAP. The real A_TURN list is 5 rows of 200 =
         * 1000 units = 10 iteration feeds, and on the iteration clock,
         * one row admitted per feed, that is exactly 10 iterations of
         * this state: the entry iteration's own feed is feed 1 (the
         * state change handed row_t = 0 over), the 200-unit rows close
         * on feeds 2, 4, 6, 8 and 10, the 10th wraps the list and arms
         * AC_BREAK_NEXT, and the transition matches at the top of feed
         * 11 - back to the A_DRAP hub, whose own single 100-unit row
         * closes in its entry iteration; with the key STILL held its
         * A_WALK(1)=K_FWD matches (facing-relative: LEFT is K_FWD once
         * face=-1) and the lion WALKS LEFT. Under the old literal-key
         * model this was a dead end: A_TURN's F=0 froze the turn slide
         * forever - exactly the reported "slides more than he walks,
         * and slow". The row-0 pull I 100,0 keeps the FEET DOWN
         * (iy==0 lifts nothing - a forced airborne state made
         * A_TURN/A_DRAP flicker while BKWD was held, the earlier
         * reported loop); wrap still arms AC_BREAK_NEXT but NOT
         * AC_TIME (the removed model choice; the time= guard is the PK
         * fixture below). PR-253: that row is now the 0x54170 pull -
         * it SETS the axis to 100/LL_PLAY_RAW_VDIV = 20 u/s in the
         * facing direction (the /4 = 25 fit is retired). */
        sk.k_bkwd = 1;
        ll_player_step(&s, &zp, &sk, 0, 2000, LL_ITER_MS);
        CHECK(s.sti == 3);                             /* A_TURN            */
        CHECK(s.walkdir == -1 && s.face == -1);        /* the LEFT key      */
        CHECK(s.grounded && !s.jumping && s.y == 100); /* horizontal pull  */
        /* THE PULL LAW, on the register this leg's release left behind:
         * the hub's own drag stopped at a residue of 1 (see the release
         * note), and 0x54170 from the opposite sign CLIMBS by |target|
         * and never past, so the -100 target lands at 1 - 100 = -99, not
         * at -100: idiv(-99,100) truncates to a step of ZERO px, so the
         * turn does not slide here. A_TURN's F=0,0 with no T= authored
         * takes NOTHING away (no force term, no drag term) - and under
         * PR-523 its passes are the row admits anyway, five of them
         * across the ten feeds - so the register runs through the whole
         * state at -99. The hub's entry pass is the one thing that
         * changes it: A_DRAP's own `T=60,0` is an authored drag, the
         * hub admits on its entry iteration, and one v_step takes
         * -99 to -99 - idiv(-99*60,100) = -40. From a body at a true
         * zero the same pull gives exactly -100, as PR-253 recorded. */
        CHECK(ll_play_v_raw(s.vx) == -99);
        for (int t = 1; t <= 9; t++)                   /* A_TURN feeds 2..10:
                                                        * the rows close on
                                                        * the even feeds and
                                                        * the 10th ends the
                                                        * list (its wrap arms
                                                        * AC_BREAK_NEXT) */
            ll_player_step(&s, &zp, &sk, 0, 2000, LL_ITER_MS);
        CHECK(s.sti == 3);                             /* no early exit:
                                                        * wrap != AC_TIME  */
        ll_player_step(&s, &zp, &sk, 0, 2000, LL_ITER_MS);
        CHECK(s.sti == 2);                             /* feed 11 matches the
                                                        * armed BREAK_NEXT */
        ll_player_step(&s, &zp, &sk, 0, 2000, LL_ITER_MS);
        CHECK(s.sti == 1);                             /* A_WALK, walking  */
        CHECK(s.walkdir == -1 && s.face == -1);        /* LEFT, drawn LEFT */
        /* The walk LEFT starts from the register the hub's own drag
         * left, -40 (see the pull note), with the sign of s reversed
         * (facing left). The call that ENTERS A_WALK is the intent
         * edge: one pass, no admit needed - and that pass IS the
         * entry's physics (a transition runs before it). In the
         * register's unit the pass is v_step(-40, 20, idiv(100*-20,
         * 20)): drag idiv(-40*20,100) = -8 off the OLD value, add
         * -100: -40 - 100 - (-8) = -132. The step is idiv(-132,100)
         * = -1 px - the walk STARTS, where PR-523's first model
         * would have frozen it: the walk-left deadlocks only when
         * the flip lands under a whole px (idiv |v| < 100), and
         * here -132 clears it.
         * From there a pass is a row admit again, the feed |step|
         * an iteration against the 6-unit rows: 1 unit an iteration
         * until the register passes 200 in magnitude, then 2, then
         * 3, and the register walks
         *   -132, -206, -265, -312, -350, -380, -404, -424, -440,
         *   -452, -462, ... -> -500 = F*s/T
         * - the same fixed point as the RIGHT walk, mirrored, and
         * the same 5 px per 1.2 iterations at the steady state
         * (OI-013 note above). */
        double xw = s.x;
        long long vl = -132, dl = 0;       /* the entry call's -1 px is
                                             * already in xw; its register
                                             * is what the loop below
                                             * continues from */
        long long lfac = 0;                /* the BY_X feed: the entry
                                             * pass left row_dx = -1 */
        int ldx = -1, lclose = 0;
        if (ll_play_v_raw(s.vx) != vl)
            badlaw = 1;
        for (int t = 0; t < 25; t++) {
            lfac += ldx < 0 ? -ldx : ldx;   /* the row clock at the head */
            if (lfac >= 6) {                /* the pass is the close      */
                lfac -= 6;
                lclose++;
                long long vld = vl / 5, vlp = vl - 100;   /* idiv(v,5), the
                                                          * facing-reversed
                                                          * add */
                vl = vlp - vld;
                dl += vl / 100;
                ldx = (int)(vl / 100);
            }
            ll_player_step(&s, &zp, &sk, 0, 2000, LL_ITER_MS);
            if (ll_play_v_raw(s.vx) != vl || s.x != xw + (double)dl)
                badlaw = 1;
            if (s.fi != lclose % 12)
                badlaw = 1;                 /* closes at loop indices 5, 8,
                                             * 11, 13, 15, 17, 19, 20, 22,
                                             * 23 - the feed's ladder, not
                                             * a fitted row */
        }
        CHECK(!badlaw);
        CHECK(dl == -33 && s.x == xw - 33.0);      /* the 25 calls after the
                                                     * entry; the entry's own
                                                     * 1 px is in xw */
        CHECK(s.fi == 10);                           /* ten of the twelve
                                                      * rows: the feed's
                                                      * ladder closing 6-unit
                                                      * rows while the body
                                                      * travels 33 whole px,
                                                      * the entry iteration's
                                                      * 1 px included */
        ll_plt_free(&zp);
        free(zm);
    }
    {   /* PR-135's second measured-law leg, RE-DERIVED by PR-256 for the
         * worm pair and by PR-523 for the intent scale and the row-admit
         * gate. ZARIA.01's ZWIERZ1 worm is (F,T) = (100,40) with a 9-row
         * list of 4-unit pitch. In the engine's own arithmetic (add =
         * idiv(F*s,20) = 100 per PASS at the PR-523 intent scale, drag =
         * idiv(T*v,100) = idiv(2v,5) of the OLD register, step =
         * idiv(v,100) whole px) the register walks, one step per PASS,
         *   100, 160, 196, 218, 231, 239, 244, 247, 249, 250, 250, ...
         * and stops at 250: idiv(2*250,5) = 100 equals the add, so 250 =
         * F*s/T at the new scale. A pass is NOT an iteration here: one
         * runs at the intent edge (t=1) and then one per ROW ADMIT, and
         * the BY_X feed is the whole-px step of the last pass - so the
         * feed is 1 unit an iteration while the register is under 200,
         * and 2 above it. The 4-unit rows close at iterations
         *   5, 9, 13, 15, 17, 19, 21, 23, 25
         * - 4 iterations per pose at first, 2 once the register passes
         * 200 - and the ninth row wraps the list back to row 0 at
         * iteration 25 with 17 px spent.
         * THE DISAGREEMENT WITH THE VIDEO, counted not smoothed: PR-79
         * timed a 1.44 s cycle for this creature = 3.2 iterations per
         * pose, and PR-134's table gives the (100,40) pair 1.25 px per
         * iteration. At the fixed point this arithmetic moves 2 px every
         * 2 iterations - a pose every 2 iterations, faster than the
         * video and slower than the table. The whole-px step is again
         * the lossy element (the register says 2.5 px a pass), and so
         * is treating the truncated step as the row feed: 0x54358-
         * 0x54372 and 0x54827-0x54829 show no carry, and PR-523's
         * live capture shows the real registers stepping every
         * iteration, not per row. Named open items (OI-013), recorded,
         * not fitted.
         * MUTATION: any change to the drag term, the force add or the
         * idiv step breaks the register walk and the row table derived
         * inside this loop from the fixture's own words. */
        static const char WM[] =
            "A_WORM\n\n[A_WORM]\ncontrol=BY_X|CH_DIR\nF=100,0\nG=0,0\n"
            "T=40,0\nseq=w\n1,4\n2,4\n3,4\n4,4\n5,4\n6,4\n7,4\n8,4\n9,4\n";
        ll_mov_t *wnm = calloc(1, sizeof *wnm);
        ll_player wp;
        ll_play_keys wk = { 0 };
        int badw = 0;
        long long wv = 0, wd = 0;           /* the register walk and the
                                             * distance, derived from the
                                             * words above */
        long long wfac = 0;                 /* the BY_X feed: the whole-px
                                             * step of the LAST pass, PR-
                                             * 518's persist rule */
        int wdx = 0, admits = 0;            /* step of the last pass, and
                                             * the row admits = the passes
                                             * after the intent edge */
        int wrap_at = 0;                    /* `broke` is cleared at the
                                             * iteration head (PR-83), so a
                                             * wrap is only observable in
                                             * the iteration that armed it */

        CHECK(wnm && ll_mov_parse(WM, wnm) == 1);
        CHECK(wnm->st[0].fx == 100 && wnm->st[0].tx == 40 &&
              wnm->st[0].nfr == 9 && wnm->st[0].fr[0].thresh == 4u);
        ll_player_start(&wp, wnm, 500, 100, 1);
        CHECK(wp.sti == 0 && wp.fi == 0);
        wk.k_fwd = 1;
        for (int t = 1; t <= 40; t++) {
            long long q6;
            if (t == 1) {                   /* the intent edge runs one
                                             * pass with no admit */
                q6 = wv * 40 / 100;         /* idiv(T*v,100); C's / is */
                wv = wv + 100 - q6;         /* x86 idiv, toward zero   */
                wd += wv / 100;             /* [obj+0xca], whole px     */
                wdx = (int)(wv / 100);
            } else {
                wfac += wdx;                /* the row clock at the head */
                if (wfac >= 4) {            /* the 4-unit row closes     */
                    wfac -= 4;
                    admits++;
                    q6 = wv * 40 / 100;
                    wv = wv + 100 - q6;
                    wd += wv / 100;
                    wdx = (int)(wv / 100);
                }
            }
            ll_player_step(&wp, NULL, &wk, 0, 200000, LL_ITER_MS);
            if (ll_play_v_raw(wp.vx) != wv || wp.x != 500.0 + (double)wd)
                badw = 1;
            if (wp.sti != 0 || wp.grounded != 1 || wp.y != 100.0)
                badw = 1;                     /* G=0,0: the walk stays on
                                               * its line */
            if (wp.fi != admits % 9)
                badw = 1;                     /* the gait table, row by row,
                                               * from the admits above */
            if (wp.broke && !wrap_at)
                wrap_at = t;
        }
        CHECK(!badw);
        CHECK(wv == 250 && wd == 31);         /* the register's fixed point
                                               * and the distance travelled */
        CHECK(wrap_at == 25 && wp.fi == 7);   /* nine rows closed by 17 px at
                                               * iteration 25, and the list
                                               * wrapped back to row 0 there;
                                               * six more rows close by
                                               * iteration 39 */
        CHECK(wp.x == 500.0 + 31.0);          /* 31 px over 16 admits */
        CHECK(wp.facc == 2.0);                /* 2 units owed on row 7 */
        free(wnm);
    }
    {   /* PR-83: a frame's `GEN K_GEN1` tail reaches the player's signal
         * bus exactly like the landing pulse does - AND the state it
         * names becomes an AUTO WALKER: entered with no direction
         * condition, its BY_X target velocity applies with no key held
         * (the cutscene walkers: RAN's A_GO_TO_BALON_*, A_EXIT). The
         * state also carries time=50 with an AC_TIME exit: it must NOT
         * act (unit undetermined; acting on it pulled ENA.03's lion out
         * of A_SWIM two ticks after spawn). */
        static const char PK[] =
            "A_RUN\nA_PULL\n\n[A_RUN]\ncontrol=BY_TIME\nF=0,0\nG=0,0\n"
            "A_PULL(1)=K_GEN1\nseq=a\n1,50,GEN K_GEN1\n2,50\n\n"
            "[A_PULL]\ncontrol=BY_X\nF=100,0\nT=30,30\nG=0,0\n"
            "time=50\nA_RUN(1)=AC_TIME\nseq=a\n1,10\n";
        static const char PP[] = "1\n1 0,100;2000,100;\n";
        ll_mov_t *pm = calloc(1, sizeof *pm);
        ll_plt pp;
        ll_player q;
        ll_play_keys qk = { 0 };

        CHECK(pm && ll_plt_parse(PP, &pp) == 0);
        CHECK(ll_mov_parse(PK, pm) == 2);
        CHECK(pm->st[1].time_lo == 50 && pm->st[1].time_hi == 50);
        /* PR-163: the single-value form pairs to (50,50) exactly as the
         * engine's scanf pair leaves max=min (the `time=350` shape) */
        ll_player_start(&q, pm, 500, 100, 1);
        /* PR-135 RE-DERIVED THE TIMING AND THE RATE: one call is one
         * iteration, every feed is 100 units, and [A_RUN]'s first row
         * is 50 units with the `GEN K_GEN1` tail. The entry iteration's
         * own feed closes that row and raises the pulse, the match
         * comes at the top of the next iteration - pull_at = 2, where
         * the 28 ms grid needed 3 feeds of 28 to reach 50 and the pin
         * was a window.
         * A_PULL is BY_X F=100, T=30 and AUTO (entered with no
         * direction condition, so the walker's intent is held), which
         * under the PR-523 engine law is v_n = 0.7 v_(n-1) + 100 ->
         * the register's fixed point is 334, idiv(3*334,10) = 100 =
         * idiv(F*s,20), 3.34 px a pass (F*s/T = 333.3 in exact
         * arithmetic; the truncation of the drag leaves the extra
         * unit). A pass is a ROW ADMIT under PR-523's default, so
         * this leg's one row is authored 10 units: the feed is the
         * whole-px step of the last pass, 1 then 2 as the register
         * climbs, and the row wraps on every admit - no AC_TIME
         * anywhere (that signal comes only from a row's GEN tail),
         * so the A_RUN(1)=AC_TIME exit stays dark through the 40
         * calls of this leg. */
        int pull_at = 0;
        for (int t = 1; t <= 8 && !pull_at; t++) {
            ll_player_step(&q, &pp, &qk, 0, 2000, LL_ITER_MS);
            if (q.sti == 1)
                pull_at = t;
        }
        CHECK(pull_at == 2);               /* the GEN row closes on the
                                            * entry feed, exit next */
        CHECK(q.auto_walk && !((q.sig >> LL_SIG_K_GEN1) & 1)); /* pulse consumed */
        double qx = q.x;
long long pv = 100, pd = 0;          /* A_PULL's own arithmetic,
                                              * derived from its header:
                                              * BY_X and AUTO, so the intent
                                              * is held at full and a pass
                                              * adds idiv(F*s,20) = 100
                                              * while the drag takes
                                              * idiv(3v,10) of the OLD
                                              * register. The call that
                                              * entered A_PULL (pull_at
                                              * above) already ran one such
                                              * pass on the intent edge -
                                              * idiv(100,100) = 1 px of it,
                                              * and qx is taken after that
                                              * step; that step is also the
                                              * row's current feed (PR-518's
                                              * persist). */
        long long pfac = 0;                  /* the BY_X feed from here on;
                                              * one physics pass per row
                                              * admit is PR-523's gate */
        int pdx = 1;                         /* whole-px step of the last
                                              * pass */
        for (int t = 0; t < 40; t++) {
            pfac += pdx;                     /* the row clock at the head */
            if (pfac >= 10) {                /* the 10-unit row closes     */
                pfac -= 10;
                pv = pv + 100 - pv * 30 / 100;
                pd += pv / 100;              /* [obj+0xca], whole px */
                pdx = (int)(pv / 100);
            }
            ll_player_step(&q, &pp, &qk, 0, 2000, LL_ITER_MS);
            CHECK(ll_play_v_raw(q.vx) == pv);
        }
        CHECK(pv == 307 && pd == 12);        /* seven passes in the 40
                                              * calls: the register walk
                                              * 100, 170, 219, 254, 278,
                                              * 295, 307 - still short of
                                              * the 334 fixed point, since
                                              * idiv(3*307,10) = 92 < 100.
                                              * The distance is the steps'
                                              * sum: 1+1+2+2+2+2+2 = 12. */
        CHECK(q.x == qx + (double)pd && q.sti == 1);  /* walks keylessly,
                                                       * AC_TIME exit stayed
                                                       * dark */
        ll_plt_free(&pp);
        free(pm);
    }
    {   /* PR-91 (T3 stage 2): an enemy attack raises AC_ENEMY_ATC on
         * the LION's own set; everything after that is authored data.
         * The reaction state and its exit line are ZARIA LEW.ZAR's own
         * (A_WALK's `A_DOSTAL(1)=AC_ENEMY_ATC`, A_DOSTAL's 9 rows of
         * 200 with the `I -320,-250` pullback on the first and
         * `A_DRAP(1)=AC_BREAK_NEXT`), the CURRENT state's line drives
         * the reaction, and the reaction ends at its OWN wrap - no new
         * mechanism anywhere (the raise itself is play_collide's,
         * covered by the Makefile play legs).
         * PR-135 RE-DERIVED EVERY NUMBER here; PR-256 re-derived them in
         * the engine's own arithmetic, because this fixture's header is
         * ZARIA's real A_DOSTAL one (`T=3,3`, `G=-50,55` - the T-as-cap
         * and G= questions of PR-98/PR-99 ARE the physics now, not an
         * open reading). The clock is unchanged: one call = one iteration
         * of 50 ms with a feed of 100 units and ONE row admit, so a
         * 200-unit row is exactly TWO iterations and the whole 9-row list
         * is 18 of them. And under PR-523's default every admit is a
         * physics pass and every step WITHOUT one runs none: the x and y
         * registers walk at the row cadence, not the iteration cadence.
         * The live capture disagrees for the real A_DOSTAL - its rows
         * are `20,200 / 5,200,WAIT !AC_ENEMY_ATC` and its velocity
         * registers keep stepping at ~100-unit intervals while the rows
         * hold - named at OI-013; this leg carries the port's model.
         * - idle 4 iterations: A_WALK is BY_X with no key held, so
         *   NOTHING feeds its rows (the mode-0 branch feeds the distance
         *   travelled and there is none); its 3 rows of 6 need 6 units of
         *   travel, row_t and row_rem stay 0, no row closes.
         * - leg step 5: the raise makes the transition match at the top
         *   of the iteration; the entry row (index 0) STAYS current, and
         *   its `I -320,-250` pair fires on the state entry (0x54170
         *   SETS both axes from rest, the x in the facing direction, and
         *   the negative y clears `grounded`). But this step admits
         *   nothing - the entry's own feed of 100 leaves row_rem = 100
         *   against the 200-unit row - and runs no key edge, so NO pass
         *   runs: the registers stay at the authored -320/-250 and the
         *   body does not move.
         * - THE X SIDE, per pass (the first at step 6): the authored
         *   G_x=-50 rides the add (PR-521, PR-520's closed x law) and
         *   T_x=3 is a 3% drag, truncated: -361, -401, -439, -476,
         *   -512, -547, -581, -614, -646. The pullback ACCELERATES:
         *   the fixed point of a -50 add against a 3% drag is -1667,
         *   and the reaction's nine passes only reach past half of the
         *   way there.
         * - THE Y SIDE: each PASS adds the authored G_y=55 word FIRST
         *   and subtracts idiv(3*v,100) of the old register: -188,
         *   -128, -70, -13, +42, +96; the STEP is idiv of that by 100,
         *   so the hop is 1 px + 1 px and then FOUR passes of zero
         *   motion while the register climbs through the truncation band
         *   - and at the next pass (+149, one px down to 99) the
         *   landing search re-seats him at step 17, INSIDE the state,
         *   one iteration before the 7th row close. With `grounded`
         *   back the y pass does not run, so the authored G_y is not a
         *   leash.
         * - rows close on 6, 8, 10, 12 and then on 13, 15, 17, 19, 21:
         *   row index 4 carries `5,200,WAIT !AC_ENEMY_ATC`, and a clause
         *   row takes the OTHER path of the engine's row step - no clock
         *   compare at all, and the zero budget of 0x522f8 on a match
         *   (PR-332, PR-333). AC_ENEMY_ATC is not live 9 steps after the
         *   hit that raised it, the shares-nothing clause answers YES,
         *   and the row closes a full iteration before its own 200 units
         *   are owed; every row after it inherits that one-step lead.
         *   The 9th close (step 21) wraps the list and arms
         *   AC_BREAK_NEXT, and the exit matches at the top of step 22 -
         *   17 iterations = 850 ms for the whole authored reaction.
         *   `A_DRAP(1)` = the hub's row 1 (index 0) and it stays: the
         *   state change zeroes row_t and that entry iteration's feed of
         *   100 is below its 200. This fixture's A_DRAP has no T= and no
         *   F=, so nothing is taken from the register or added to it:
         *   -646 coasts through the hub at idiv(-646,100) = -6 px a
         *   PASS - and with the gate on, a pass is a hub-row close, the
         *   200-unit rows admitting every two iterations (the
         *   dragless-hub behaviour PR-83 recorded, at the row cadence). */
        static const char HR[] =
            "A_WALK\nA_DOSTAL\nA_DRAP\n\n"
            "[A_WALK]\ncontrol=BY_X|CH_DIR\nF=100,0\nT=20,5\nG=0,0\n"
            "A_DOSTAL(1)=AC_ENEMY_ATC\nseq=walk\n1,6\n2,6\n3,6\n\n"
            "[A_DOSTAL]\ncontrol=BY_TIME\nF=0,0\nT=3,3\nG=-50,55\n"
            "A_DRAP(1)=AC_BREAK_NEXT\nseq=hit\n"
            "1,200,I -320,-250\n2,200\n3,200\n4,200\n"
            "5,200,WAIT !AC_ENEMY_ATC\n6,200\n7,200\n8,200\n9,200\n\n"
            "[A_DRAP]\ncontrol=BY_TIME\nF=0,0\nG=0,0\nseq=idle\n"
            "1,200\n2,200\n";
        static const char HP[] = "1\n1 0,100;2000,100;\n";
        ll_mov_t *hm = calloc(1, sizeof *hm);
        ll_plt hp;
        ll_player u;
        ll_play_keys uk = { 0 };

        CHECK(hm && ll_plt_parse(HP, &hp) == 0);
        CHECK(ll_mov_parse(HR, hm) == 3);
        CHECK(hm->st[1].nfr == 9 && hm->st[1].fr[0].thresh == 200u &&
              hm->st[1].fr[0].has_i && hm->st[1].fr[0].ix == -320 &&
              hm->st[1].fr[0].iy == -250);
        CHECK(hm->st[1].tr[0].tidx == 2 && hm->st[1].tr[0].entry == 1);
        ll_player_start(&u, hm, 500, 100, 1);
        CHECK(u.sti == 0);
        for (int t = 1; t <= 4; t++)                  /* nobody attacked:  */
            ll_player_step(&u, &hp, &uk, 0, 2000, LL_ITER_MS); /* the line stays dark */
        CHECK(u.sti == 0 && u.fi == 0);
        CHECK(u.facc == 0.0 && u.row_t == 0u && u.row_rem == 0u);
        u.sig |= 1ull << LL_SIG_AC_ENEMY_ATC;         /* the hit raises it */
        ll_player_step(&u, &hp, &uk, 0, 2000, LL_ITER_MS);    /* leg step 5       */
        CHECK(u.sti == 1 && u.fi == 0);               /* A_DOSTAL, row 1   */
        /* THE PULL, in the register's own unit, and the ABSENCE of a
         * pass: the entry row's `I -320,-250` SETS both axes from rest
         * (0x54170) and the negative pair clears `grounded` (PR-83's
         * rule, unchanged), but the step admits no row (row_rem = 100
         * of 200) and is no intent edge, so PR-523's gate skips every
         * physics section: the registers are exactly the authored
         * words, and neither x nor y has moved. */
        CHECK(ll_play_v_raw(u.vx) == -320 && ll_play_v_raw(u.vy) == -250);
        CHECK(u.x == 500.0 && u.y == 100.0 && !u.grounded);
        double ux0 = u.x;
        int rec_at = 0, badlaw = 0, land_at = 0;
        long long hx = -320;              /* the leg's own copy of the
                                           * authored x pass: the G_x=-50
                                           * add and the T_x=3 drag, */
        long long hy = -250;              /* v + 55 - idiv(3*v,100)     */
        double yd = 100.0;                /* and the position stepped by
                                           * idiv(vy,100) px */
        static const int close_at[] = { 6, 8, 10, 12, 13, 15, 17, 19, 21 };
        for (int t = 6; t <= 31; t++) {               /* rows are two feeds  */
            ll_player_step(&u, &hp, &uk, 0, 2000, LL_ITER_MS); /* each, so every even step */
            int pass = 0;
            for (int k = 0; k < 9; k++)               /* PR-523: a pass is */
                if (close_at[k] == t)                 /* a row admit, and  */
                    pass = 1;                         /* nothing else      */
            if (!land_at && pass) {
                hy = hy + 55 - hy * 3 / 100;
                yd += (double)(hy / 100);
                if (hy >= 0 && yd >= 99.0) {
                    /* the landing test of 0x54…: the register is not rising
                     * and the contact line is within a pixel of the .plt
                     * line at y=100. It fires HERE at t=17, one pass before
                     * the arithmetic would take him down to 100 - the
                     * search's 1 px tolerance, not the physics. */
                    land_at = t;
                    if (!(u.grounded && u.y == 100.0 && u.vy == 0.0))
                        badlaw = 1;
                } else if (u.y != yd) {
                    badlaw = 1;             /* the HOP TABLE: yd walks
                                             * 99 at t=6, 98 at t=8, and
                                             * then stands at 98 through
                                             * the passes at 10, 12, 13,
                                             * 15 - the register climbing
                                             * -70, -13, +42, +96 through
                                             * the idiv(v,100) = 0 band -
                                             * the whole hop is 2 px and
                                             * the truncation holds him
                                             * at the top of it */
                }
            }
            if (pass)
                hx = hx - 50 - hx * 3 / 100;          /* the x pass: the
                                                       * authored G_x add
                                                       * first, the drag
                                                       * off the OLD value */
            if (u.sti == 1 && ll_play_v_raw(u.vx) != hx)
                badlaw = 1;
            if (u.sti == 2) {                 /* this fixture's A_DRAP authors
                                               * NO T and no F at all, so the
                                               * register is neither taken nor
                                               * added to: the -646 the reaction
                                               * hands over (G_x=-50 over nine
                                               * passes - the pullback
                                               * ACCELERATES toward the -1667
                                               * fixed point) coasts through
                                               * the hub at idiv(-646,100) =
                                               * -6 px per hub-row admit, the
                                               * 200-unit hub rows closing on
                                               * 23, 25, 27, 29, 31 (PR-83's
                                               * dragless hub, at the row
                                               * cadence) */
                if (ll_play_v_raw(u.vx) != -646)
                    badlaw = 1;
            }
            if (u.sti == 1) {
                int nc = 0;
                for (int k = 0; k < 9; k++)
                    if (close_at[k] <= t)
                        nc++;
                if (u.fi != nc % 9)                   /* the 9th close wraps */
                    badlaw = 1;                       /* back to row 1    */
            }
            if (!rec_at && u.sti == 2) {
                rec_at = t;
                if (u.fi != 0)
                    badlaw = 1;                       /* A_DRAP(1): the entry
                                                       * row stays standing */
            }
        }
        CHECK(rec_at == 22 && land_at == 17 && !badlaw);  /* AC_BREAK_NEXT at
                                                           * the wrap, after an
                                                           * 17-iteration list
                                                           * (one step short of
                                                           * the clock-only
                                                           * reading) */
        /* THE PULLBACK, from the walk above: the x pass runs at each row
         * admit (G_x=-50 minus a 3% idiv drag), and the STEP is
         * idiv(register,100) whole px at each: -3, -4, -4, -4, -5, -5,
         * -5, -6, -6 through the reaction (the register walking -361 to
         * -646 toward its -1667 fixed point), then -6 five times through
         * the hub at the hub rows' own cadence. Total -72 px - and the
         * entry step runs no pass at all, so ux0 is taken before any
         * movement. The old model's -42 for the same words came from a
         * drag-only x register walking every iteration; the authored
         * G_x=-50 is what makes this a pullback that gathers speed
         * instead of one that fades. */
        CHECK(u.x == ux0 - 72.0);
        ll_plt_free(&hp);
        free(hm);
    }
    {   /* PR-523: A CLAUSE ROW THAT REFUSES PARKS THE ROW CURSOR, AND
         * NOT THE BODY. The row-step half of this is PR-333's, pinned
         * by the A_DOSTAL and A_DOWN legs above: a row whose `Wait`
         * clause answers NO leaves the whole step through 0x522ed - no
         * execute slot, no cursor step, nothing consumed. Read as the
         * PHYSICS gate too (PR-518's gate), that parks the body as
         * well, and ZARIA.01's real [A_JUMP] shows the original does
         * not do that: its row 10 is `8,0,WAIT AC_DUP`, the AC_DUP
         * being the surface link that state is falling TOWARD, so with
         * the body gated on the admit the registers freeze at y 586 /
         * vy -747 on tick 59 and the ground that would raise AC_DUP is
         * never reached - the lion hangs in the air for the rest of the
         * run (/tmp/opencode/mut_zjump.log, the arm turned off: frame
         * 8, pos 161,586, v 1780,-14940 unmoved from tick 59 to 96,
         * and no landing transition at all). This leg is the same law
         * with no game data in it, so the arm has a hand-derived pin
         * and not only a real-data one.
         * THE FIXTURE: one BY_TIME state, no transition lines, header
         * F=0,0 T=20,0 G=100,0 - no x intent at all, so the ONLY x
         * force is the authored G_x word (PR-521: G_x rides every
         * pass, key or no key) - and three rows: `1,100`,
         * `2,100,WAIT AC_EDGE`, `3,100`. AC_EDGE has no raiser in this
         * fixture (no ladder, no seat box), so the middle row is a
         * gated row that never answers, and the state parks on it.
         * EVERY NUMBER BELOW IS THE CLOSED FORM, not a recorded run.
         * The row clock is fed 100 units an iteration (the engine's
         * constant, 0x4c8b7) and nothing else, so row 0 admits at
         * iteration 1 (100 >= 100) and the cursor lands on row 1,
         * where it stays for the rest of the leg - the clock keeps
         * running (row_t = 100, 200, ... 900 after steps 2..10) while
         * the cursor does not move one step.
         * A pass runs at the row-0 admit AND at every parked step:
         * the x register walks v -> v - idiv(v*20,100) + 100, i.e.
         * 100, 180, 244, 296, 337, 370, 396, 417, 434, 448 - the
         * ramp toward the fixed point 500 of the authored pair - and
         * the body takes idiv(v,100) whole px: 1, 1, 2, 2, 3, 3, 3,
         * 4, 4, 4, so x walks 501, 502, 504, 506, 509, 512, 515,
         * 519, 523, 527. WITHOUT THE ARM, steps 2..10 run no pass at
         * all: x stays 501 and the register stays 100, which is the
         * ZARIA hang in miniature.
         * MUTATION, MEASURED: forcing the arm's condition off (the two
         * lines in ll_player_step's row-admit block) leaves every other
         * leg of this file green - no other fixture state parks on a
         * clause row - and turns exactly ONE check red here, line
         * 5347's register-and-position check (/tmp/opencode/
         * mut_ps2.log). On real data the same mutation turns
         * ZARIA.01's jump leg red the same way: no landing transition,
         * the lion frozen mid-air at the row that waits for it. */
        static const char PR_TXT[] =
            "A_PARK\n\n"
            "[A_PARK]\ncontrol=BY_TIME\nF=0,0\nT=20,0\nG=100,0\n"
            "seq=park\n1,100\n2,100,WAIT AC_EDGE\n3,100\n";
        static const char PR_PLT[] = "1\n1 0,100;2000,100;\n";
        ll_mov_t *pr = calloc(1, sizeof *pr);
        ll_plt ppl;
        ll_player w;
        ll_play_keys wk = { 0 };
        static const long long vwant[] = { 100, 180, 244, 296, 337,
                                           370, 396, 417, 434, 448 };
        static const double xwant[] = { 501, 502, 504, 506, 509,
                                        512, 515, 519, 523, 527 };
        CHECK(pr && ll_plt_parse(PR_PLT, &ppl) == 0);
        CHECK(ll_mov_parse(PR_TXT, pr) == 1);
        CHECK(pr->st[0].nfr == 3 && pr->st[0].fr[1].thresh == 100u &&
              ll_mov_row_gated(&pr->st[0].fr[1]) &&
              !ll_mov_row_gated(&pr->st[0].fr[0]));
        ll_player_start(&w, pr, 500, 100, 1);
        CHECK(w.sti == 0 && w.fi == 0);
        for (int t = 1; t <= 10; t++)
            ll_player_step(&w, &ppl, &wk, 0, 2000, LL_ITER_MS);
        /* THE PARK: the cursor is on the clause row and has not moved
         * since the first admit, the frame drawn is that row's OWN
         * authored frame number, and the state never left. */
        CHECK(w.sti == 0 && w.fi == 1);
        CHECK(pr->st[0].fr[w.fi].frame == 2);
        /* THE CLOCK, still running: 100 units at each of steps 2..10,
         * never consumed, exactly the shape PR-518's live trace shows
         * between admits. */
        CHECK(w.row_t == 900u && w.row_rem == 0u && w.facc == 0.0);
        /* THE BODY, still stepping: the register and the position are
         * the tenth values of the closed form above. */
        CHECK(ll_play_v_raw(w.vx) == vwant[9] && w.x == xwant[9]);
        {
            long long v = 0;
            double xx = 500.0;
            for (int t = 0; t < 10; t++) {
                v = v - v * 20 / 100 + 100;
                xx += (double)(v / 100);
                if (v != vwant[t] || xx != xwant[t])
                    CHECK(0);          /* the table above is the closed
                                        * form; a mismatch here means the
                                        * arithmetic moved, not the run */
            }
        }
        CHECK(w.y == 100.0 && w.grounded);  /* the y side is untouched:
                                             * grounded, no G_y, no y
                                             * intent */
        ll_plt_free(&ppl);
        free(pr);
    }
    {   /* PR-108's BY_TY climb leg, RE-PINNED by PR-148: the leg
         * now tests the engine's input/guard law, not the old
         * ungated time clock. A census of the shipped files (tools
         * note in NEXT_STEPS PR-108, script kept out of git at
         * /tmp/opencode/pr108_census.py) finds BY_TY in 12 state
         * blocks in 7 .MOV files and three state names only: A_WSP
         * (seq climb / climb_r), A_WSP_LIN (the rope variant of the
         * same block) and TUNGA2's A_DRAB - the DRABINKA
         * rung-ladder family (PR-109 corrected the earlier "vine or
         * rope": ENA2's rope state A_CLIMB is BY_Y mode 1, a
         * different mechanism).
         * They all read `F=0,100 T=160,50` (A_DRAB `T=1000,50`),
         * all `G=0,0`, all `control=BY_TY|CHECK_ALL|SEQ_REV_Y`
         * (KARA's adds ANTI_SOLID|FRAME_REV), they are entered by
         * AC_DRAB/AC_DRAB_CATCH/AC_ONAIR and left by
         * `!AC_DRAB`/`!AC_LINA`. PR-147 read the rest of the law at
         * byte level and PR-148 implements it: the mode-4 branch
         * 0x5441b feeds the row accumulator 100 ONLY while
         * [obj+0xbe], the player's vertical intent, is non-zero -
         * the frame loop hands the LION the keyboard block through
         * its input method every iteration (0x4aeca, PR-147) and
         * 0x549f0 stores its word1 (Up/Down, y+ down) into that
         * word. The port's p->vert_intent and section 7's feed
         * guard are exactly that fact; the kind-4 contact override
         * (0x54b16) is PR-149's route, and so is the word's use
         * as a v_y force (0x542cd) - see the open items in the
         * PR-148 record.
         * None of it is reachable from shipped data in the player
         * sim - no port code raises AC_DRAB/AC_DRAB_CATCH/AC_ONAIR/
         * AC_LINA/AC_EDGE (PR-50 showed those are engine-raised) -
         * so the signals are raised directly, the way PR-91's leg
         * does. THE ARITHMETIC, RE-DERIVED BY HAND FOR PR-148:
         * with the intent held every iteration the spans are
         * exactly the PR-108 ones (the guard passes, so the held
         * climb's clock is unchanged), and the NEW fact is the
         * no-intent freeze and the partial accumulator surviving
         * the gap. The authored frame list is 300,200x8,300 with
         * no time_scale line; one call = one iteration = one feed
         * of 100 units and one row admit; `A_WSP(3)=` enters at
         * list index 2 (entry-1 = row 3, PR-108).
         * LEG 1, no keys held: the guard fails every iteration, so
         * the feed is zero FOREVER and 25 iterations change
         * NOTHING beyond the entry itself: rows never advance, the
         * list never wraps, AC_BREAK_NEXT never arms, the
         * accumulator stays 0, and x, y, v_x, v_y never move (the
         * model has no vertical motion for this state and this PR
         * adds none: the word's use as a v_y FORCE at 0x542cd is
         * counted open in the record, and the earlier session's
         * 0x50252 "rise law" candidate was caught dead - a byte
         * census shows NO call site and no table entry points at
         * it anywhere in the image; what the frame loop's
         * [lion.vt+0x74] call at 0x4afea really runs is 0x50320).
         * Under the old ungated model this exact loop advanced and
         * wrapped at t=17; the mutations that delete or invert the
         * guard land here. The final step lets go of AC_DRAB:
         * `A_FALL_DOWN(1)=!AC_DRAB` wins from a state whose list
         * NEVER ticked - transitions are a separate pass (section
         * 1), exactly as the engine evaluates transitions and rows
         * separately.
         * LEG 2, a fresh climb on a fresh player, Up held from the
         * first step to t=15: every iteration qualifies the feed, so
         * the row spans are the PR-108 ones -
         *   t=1  enter at index 2, rem 100 < 200, nothing closes
         *   t=2  rem 200  -> index 3      t=4 -> 4   t=6 -> 5
         *   t=8  -> 6                    t=10 -> 7  t=12 -> 8
         *   t=14 -> 9 (the last 200-row closes, the 300-row opens)
         *   t=15 rem 100 against the 300-row, still index 9
         * - and the climb MOVES, one px per row ADMIT (PR-523): the
         * register add is `idiv([ebx+0x4] * [obj+0xbe], 20)`,
         * 0x542cd-0x542e3: the second word of the authored F pair
         * times the vertical intent - at the PR-523 scale that is
         * idiv(100 * -20, 20) = -100 hundredths A PASS, against the
         * authored y drag T_y = 50 percent of the OLD register
         * (0x54292-0x542a7, applied at 0x54311). A pass is an admit,
         * and the admits are exactly the even steps - hand-run on the
         * integer register with toward-zero idiv, in the exact order
         * of ll_play_v_step: t=2 -100, t=4 -150, t=6 -175, t=8 -188,
         * t=10 -194, t=12 -197, t=14 -199 - crawling the equilibrium
         * of a 100 add against a 50% drag, which is -200 = 2*(-100),
         * never quite reached. The odd steps run NO pass and the
         * register simply stands where it is. The position takes the
         * TRUNCATED register (PR-256), so every admit is exactly one
         * whole px: 499 at t=2, 498 at t=4, ... 493 at t=14, held at
         * 493 through t=15.
         * LEG 3, release mid-row: t=16,17 hold NO key. The row
         * clock freezes at index 9 - no feed - but the carried
         * accumulator (100 of the 300-row) survives untouched,
         * because the guard feeds nothing while the row compare
         * still runs, the engine's own order (feed 0x52317, THEN
         * compare 0x52320). No wrap at t=17 either: a mutation
         * that deletes the guard wrapped HERE (the PR-108 spans),
         * and one that zeroes the accumulator on a non-qualifying
         * iteration loses the carry and wraps at t=20, not t=19.
         * And with no admit there is no pass, so the register freezes
         * at -199 with the row - both the carry and the momentum
         * belong to the row state, not to any per-iteration physics.
         * (The real engine's registers keep stepping through held
         * rows - PR-523's capture note, OI-013 - and the OLD model
         * decayed them here, -100 -> -50 -> -25; the port's gate
         * says the row IS the clock, physics included. Named, not
         * fitted.)
         *   t=18 Down held: the clock restarts ON THE CARRIED 100
         *   (the engine's [obj+0xa1] does the same): 100 + 100 =
         *   200 < 300, the row does NOT close - and with no admit
         *   still no pass runs, so the register stays at -199; the
         *   turn-around waits for the row.
         *   t=19 still Down: 200 + 100 = 300 -> the list WRAPS to
         *   index 0 and arms AC_BREAK_NEXT (readable only on the
         *   step that armed it, `broke` clears at iteration head) -
         *   and the close IS a pass: (-199 + 100) - idiv(-199*50,
         *   100) = -99 - (-99) = 0, the flip lands exactly on zero,
         *   the sign-cross zero of 0x5430a never even firing. y
         *   stays at 493 (idiv(0,100) = 0).
         *   t=20 still Down: index 0's 300-row takes a fresh 100,
         *   no close, so no pass: the register stands at 0.
         * The clock is sign-blind, Up and Down qualifying alike -
         * the guard tests `!= 0`, exactly like 0x5441b /
         * `cmp [obj+0xbe],0; je` - and only the force's sign, not the
         * clock, cares which way. */
        static const char CE[] =
            "A_JUMP\nA_WSP\nA_FALL_DOWN\nA_PRE_JUMP\nA_TURN_BACK\n"
            "A_PODCIAG\n\n[A_JUMP]\ncontrol=BY_TIME|CHECK_ALL\nF=0,100\n"
            "T=160,50\nG=0,0\nA_WSP(3)=AC_DRAB_CATCH,AC_ONAIR\nseq=climb\n"
            "1,900\n2,900\n\n[A_WSP]\ncontrol=BY_TY|CHECK_ALL|SEQ_REV_Y\n"
            "F=0,100\nT=160,50\nG=0,0\nA_PODCIAG(1)=AC_EDGE,K_UP\n"
            "A_TURN_BACK(1)=AC_DUP,K_DOWN\nA_FALL_DOWN(1)=!AC_DRAB\n"
            "A_PRE_JUMP(1)=K_JUMP\nseq=climb\n1,300\n2,200\n3,200\n4,200\n"
            "5,200\n6,200\n7,200\n8,200\n9,200\n10,300\n\n[A_FALL_DOWN]\n"
            "control=BY_TIME|CHECK_ALL\nF=0,100\nT=160,50\nG=0,0\n"
            "seq=spad\n1,400\n2,400\n\n[A_PRE_JUMP]\ncontrol=BY_TIME\n"
            "F=0,100\nT=160,50\nG=0,0\nseq=uj30\n1,200\n\n[A_TURN_BACK]\n"
            "control=BY_TIME\nF=0,100\nT=160,50\nG=0,0\nseq=turn_bk\n"
            "1,300\n\n[A_PODCIAG]\ncontrol=BY_TIME\nF=0,100\nT=160,50\n"
            "G=0,0\nseq=podciag\n1,400\n";
        ll_mov_t *cm = calloc(1, sizeof *cm);
        ll_player r, u;
        ll_play_keys rk = { 0 };
        static const int at[] = { 1, 2, 4, 6, 8, 10, 12, 14 };
        static const int want[] = { 2, 3, 4, 5, 6, 7, 8, 9 };
        int nx = 0;
        int bad = 0;

        CHECK(cm && ll_mov_parse(CE, cm) == 6);
        CHECK(cm->st[1].nfr == 10 && cm->st[1].fr[2].thresh == 200 &&
              cm->st[1].fr[9].thresh == 300);
        /* PR-148: BY_TY has a model bit now (the 0x5441b feed
         * guard) and reaches ctrl through the parser; SEQ_REV_Y
         * alone is the name left counted in ctrl_other - the PR-
         * 108 pin of ctrl_other == 2 re-pins to 1 here, honestly,
         * and the one flag the model already knew (CHECK_ALL)
         * still reaches ctrl. */
        CHECK(cm->st[1].ctrl_other == 1 &&
              (cm->st[1].ctrl & LL_CTRL_CHECK_ALL) != 0 &&
              (cm->st[1].ctrl & LL_CTRL_BY_TY) != 0 &&
              (cm->st[1].ctrl & (LL_CTRL_BY_X | LL_CTRL_BY_TIME)) == 0);
        /* PR-231 (OI-113, read PR-230): the engine's control parser
         * (0x51c62-0x51c73, keyword 0x921c6) maps CHECK_ALL to the
         * flag word EXACTLY as CHECK_LN|CHECK_ANI|CHECK_ZW (0x1c0) -
         * it IMPLIES the CHECK_LN rule. ZARIA.01's lion [A_WALK] is
         * `control=BY_X|FRAME_REV|CHECK_ALL|CH_DIR` with no spelled
         * CHECK_LN, so every CHECK_ALL state must reach the port's
         * CHECK_LN paths (terrain following 3) below, the gravity
         * fallback in section 3, and obj_settle in ll_level). */
        CHECK((cm->st[0].ctrl & LL_CTRL_CHECK_LN) != 0 &&
              (cm->st[0].ctrl &
               (LL_CTRL_CHECK_ANI | LL_CTRL_CHECK_ZW))
                  == (LL_CTRL_CHECK_ANI | LL_CTRL_CHECK_ZW));
        /* the authored exit list, in order, with the negated signal */
        CHECK(cm->st[1].tr[2].entry == 1 && cm->st[1].tr[2].nc == 1 &&
              cm->st[1].tr[2].c[0].cond == LL_COND_OTHER &&
              cm->st[1].tr[2].c[0].sig == LL_SIG_AC_DRAB &&
              cm->st[1].tr[2].c[0].neg == 1);
        CHECK(cm->st[0].tr[0].entry == 3 &&
              cm->st[0].tr[0].c[0].sig == LL_SIG_AC_DRAB_CATCH);
        /* LEG 1: the guard. No key ever held. */
        ll_player_start(&r, cm, 500, 500, 1);
        CHECK(r.sti == 0);                            /* the declared name  */
        CHECK(r.vert_intent == 0);
        for (int t = 1; t <= 25; t++) {
            r.sig |= 1ull << LL_SIG_AC_DRAB;   /* the ladder stays held   */
            if (t == 1)
                r.sig |= (1ull << LL_SIG_AC_DRAB_CATCH) |
                         (1ull << LL_SIG_AC_ONAIR);
            ll_player_step(&r, NULL, &rk, 0, 2000, LL_ITER_MS);
            if (r.sti != 1 || r.fi != 2)
                bad = 1;        /* entered, then frozen: no feed at all   */
            if (r.row_t != 0 || r.facc != 0.0)
                bad = 1;        /* zero feeds; mode 4 is not distance-fed */
        }
        CHECK(!bad);
        CHECK(r.broke == 0);                  /* never wrapped, never armed */
        CHECK(r.y == 500 && r.x == 500 && r.vx == 0 && r.vy == 0);
        r.sig &= ~(1ull << LL_SIG_AC_DRAB);   /* the ladder lets go     */
        ll_player_step(&r, NULL, &rk, 0, 2000, LL_ITER_MS);
        CHECK(r.sti == 2 && r.fi == 0);       /* the exit works from a     */
                                              /* state whose list NEVER    */
                                              /* ticked (separate passes)  */
        /* LEG 2/3: a fresh climb, Up held t=1..15 (PR-108's spans
         * while the guard passes), no key t=16..17 (frozen, carry
         * kept, NO wrap at 17), Down t=18..20 (restart on the
         * carry, wrap at 19). */
        ll_player_start(&u, cm, 500, 500, 1);
        rk.k_up = 1;
        for (int t = 1; t <= 15; t++) {
            u.sig |= 1ull << LL_SIG_AC_DRAB;
            if (t == 1)
                u.sig |= (1ull << LL_SIG_AC_DRAB_CATCH) |
                         (1ull << LL_SIG_AC_ONAIR);
            ll_player_step(&u, NULL, &rk, 0, 2000, LL_ITER_MS);
            /* PR-259 + PR-523: the climb MOVES, once per row admit.
             * The first admit (t=2) already carries the -100 add, one
             * whole truncated px; the odd steps run no pass and the
             * body stands. */
            if (u.y != (t == 1 ? 500.0 : 500.0 - (double)(t / 2)))
                bad = 1;
            if (nx < (int)(sizeof at / sizeof at[0]) && t == at[nx]) {
                if (u.sti != 1 || u.fi != want[nx] ||
                    ll_player_frame(&u) != want[nx] + 1)
                    bad = 1;
                nx++;
            }
        }
        CHECK(!bad && nx == (int)(sizeof at / sizeof at[0]));
        CHECK(u.sti == 1 && u.fi == 9 && u.row_t == 100);
        CHECK(u.y == 493);                        /* 500 - 7 admits      */
        CHECK(u.x == 500 && u.vx == 0);           /* no x force: F_x = 0 */
        CHECK(ll_play_v_raw(u.vy) == -199);       /* the crawl toward the
                                                   * -200 drag equilibrium */
        rk.k_up = 0;
        for (int t = 16; t <= 17; t++) {
            u.sig |= 1ull << LL_SIG_AC_DRAB;
            ll_player_step(&u, NULL, &rk, 0, 2000, LL_ITER_MS);
            CHECK(u.sti == 1 && u.fi == 9 && u.row_t == 100);
            /* no feed, no admit, NO pass: the register freezes at -199
             * with the row, and y with it (PR-523's gate; the old
             * model's -100 -> -50 -> -25 decay lived here) */
            CHECK(u.y == 493);
            CHECK(ll_play_v_raw(u.vy) == -199);
            CHECK(u.broke == 0);        /* NO wrap at t=17: the guard     */
        }
        rk.k_down = 1;
        u.sig |= 1ull << LL_SIG_AC_DRAB;
        ll_player_step(&u, NULL, &rk, 0, 2000, LL_ITER_MS);       /* t=18 */
        CHECK(u.sti == 1 && u.fi == 9 && u.row_t == 200);
        CHECK(ll_play_v_raw(u.vy) == -199); /* the carried 100 + 100 does */
                                            /* not close the 300-row, so  */
                                            /* no pass: the turn-around   */
                                            /* waits for the row          */
        u.sig |= 1ull << LL_SIG_AC_DRAB;
        ll_player_step(&u, NULL, &rk, 0, 2000, LL_ITER_MS);       /* t=19 */
        CHECK(u.sti == 1 && u.fi == 0 && u.broke == 1);
        CHECK(u.row_t == 0);            /* the wrap: index 0, BREAK_NEXT  */
        CHECK(ll_play_v_raw(u.vy) == 0);   /* (-199+100) - idiv(-199*50, */
                                           /* 100): the flip lands on 0   */
        u.sig |= 1ull << LL_SIG_AC_DRAB;
        ll_player_step(&u, NULL, &rk, 0, 2000, LL_ITER_MS);       /* t=20 */
        CHECK(u.sti == 1 && u.fi == 0 && u.row_t == 100);
        CHECK(u.y == 493);              /* Down qualifies the clock like  */
                                        /* Up: the guard is sign-blind,   */
        CHECK(ll_play_v_raw(u.vy) == 0); /* no close, no pass: it stands */
        rk.k_down = 0;
        free(cm);
    }
    {   /* PR-110's own non-twin test: the row clock counts ITERATIONS,
         * not wall ms, and one authored list must keep the same
         * ITERATION spans on two different play grids. The fixture is
         * this test's own - deliberately, since what is under test is
         * the clock, not a shipped script.
         * Hand-derived, no game data. `A_ONE` is a BY_TIME list of
         * 300,200,200,200,300 = 1200 units = 12 iteration feeds
         * (a feed is 100 units, ll_play.h). Section 7 adds a feed,
         * then subtracts a row's threshold when the accumulator
         * reaches it, so a 300-unit row is current for 3 feeds and a
         * 200-unit row for 2: spans of 3,2,2,2,3 however the grid is
         * cut (MUT-A: hard-wiring "a row is 2 iterations" would break
         * the 300s).
         * Grid 1, dt = LL_ITER_MS: exactly one feed per step, so the
         * rows close at steps 3,5,7,9,12 and the 1200th unit WRAPS the
         * list in step 12 - 12 steps for the whole cycle, where the
         * wall-ms model took 1200 ms = 43 steps of 28 ms. Because 12 x
         * 100 is exactly the list, the accumulator and the ms carry are
         * both 0 after every step; that is the sharpest form of the
         * claim, and it fails at once on a mutated LL_ITER_MS, a
         * subtract-once rule, or a feed that does not carry.
         * Grid 2, dt = 20 ms (a sub-iteration grid, which is the
         * point of running it): the m-th feed lands at step
         * ceil(50m/20), i.e. 3,5,8,10,13,15,18,20,23,25,28,30 for
         * m = 1..12, so the SAME list closes its rows at steps
         * 8,13,18,23,30 - the same 12 feeds, 30 grid steps (600 ms,
         * this grid quantising the 600 ms the list is worth at 50 ms
         * a feed), and the row spans in feeds are still 3,2,2,2,3.
         * The arithmetic inside the loop is the MUT-C trip-wire:
         * after every step the accumulator must be exactly 100 x
         * floor(20t/50) minus the thresholds closed so far, and
         * row_rem exactly (20t) mod 50 - a feed that drifted would
         * show a residue, a feed in ms would show 20t. PR-135
         * re-derived this table for the new LL_ITER_MS; at the old
         * 28 ms iteration the closes were at steps 5,7,10,13,17.
          * Both loops also read `facc`, which section 4 only feeds in a
          * BY_X state with a non-zero state F= (0x543a2's mode-0 branch
          * REPLACES the 100-per-iteration feed, it does not add to it):
          * in this BY_TIME state it stays exactly 0 at every step.
         * time_scale= is the engine's own pre-scale of the authored
         * row values: `threshold = value * time_scale / 1000` inside
         * each row node's ctor (0x51679 `imul`, 0x5168d `idiv 0x3e8`),
         * so a block with time_scale=2000 runs its rows at DOUBLE the
         * iteration count - the same list is then 1100 units and 11
         * feeds, not 7. PR-104's ctor-order reading (only rows built
         * after the line see the scale) is asserted too; no shipped
         * .MOV has a row before its line (116 active lines in 38
         * files, none with a preceding row), and the block without the
         * line is the MUT-B control: 300,200,200 unchanged, the default
         * scale 1000. */
        static const char RC[] =
            "A_ONE\n\n[A_ONE]\ncontrol=BY_TIME\nF=0,0\nG=0,0\nseq=a\n"
            "1,300\n2,200\n3,200\n4,200\n5,300\n";
        static const unsigned RC_THR[] = { 300, 200, 200, 200, 300 };
        static const int RC_CLOSE50[] = { 3, 5, 7, 9, 12 };
        static const int RC_CLOSE20[] = { 8, 13, 18, 23, 30 };
        ll_mov_t *rcm = calloc(1, sizeof *rcm);
        ll_player v, w;
        ll_play_keys vk = { 0 };

        CHECK(rcm && ll_mov_parse(RC, rcm) == 1);
        CHECK(rcm->st[0].nfr == 5 && rcm->st[0].fr[0].thresh == 300u &&
              rcm->st[0].fr[1].thresh == 200u);
        CHECK(rcm->st[0].time_scale == 1000 && rcm->st[0].scale_at == 0);
        ll_player_start(&v, rcm, 500, 100, 1);
        for (int t = 1; t <= 12; t++) {         /* grid 1: one feed/step */
            unsigned closed = 0;
            int want = 0;
            for (int k = 0; k < 5; k++)
                if (RC_CLOSE50[k] <= t) {
                    closed += RC_THR[k];
                    want = k + 1;
                }
            if (want == 5)
                want = 0;                      /* the 12th feed wraps */
            ll_player_step(&v, NULL, &vk, 0, 2000, LL_ITER_MS);
            CHECK(v.fi == want &&
                  v.row_t == 100u * (unsigned)t - closed &&
                  v.row_rem == 0u && v.facc == 0.0);
            CHECK(v.x == 500 && v.y == 100);    /* rows with no M dx,dy */
        }
        CHECK(v.broke == 1);                    /* the wrap armed it */
        ll_player_start(&w, rcm, 500, 100, 1);
        for (int t = 1; t <= 30; t++) {         /* grid 2: 100 per 50 ms  */
            unsigned closed = 0;
            int want = 0;
            for (int k = 0; k < 5; k++)
                if (RC_CLOSE20[k] <= t) {
                    closed += RC_THR[k];
                    want = k + 1;
                }
            if (want == 5)
                want = 0;
            ll_player_step(&w, NULL, &vk, 0, 2000, 20);
            CHECK(w.fi == want &&
                  w.row_t == 100u * (unsigned)((20 * t) / 50) - closed &&
                  w.row_rem == (unsigned)((20 * t) % 50) &&
                  w.facc == 0.0);
        }
        static const char TS[] =
            "A_ONE\n\n[A_ONE]\ncontrol=BY_TIME\nF=0,0\nG=0,0\nseq=a\n"
            "1,300\ntime_scale=2000\n2,200\n3,200\n";
        static const char TS0[] =
            "A_ONE\n\n[A_ONE]\ncontrol=BY_TIME\nF=0,0\nG=0,0\nseq=a\n"
            "1,300\n2,200\n3,200\n";
        static const unsigned TS_THR[] = { 300, 400, 400 };
        static const int TS_CLOSE[] = { 3, 7, 11 };
        ll_mov_t *tsm = calloc(1, sizeof *tsm), *ts0 = calloc(1, sizeof *ts0);
        ll_player u;

        CHECK(tsm && ts0 && ll_mov_parse(TS, tsm) == 1 &&
              ll_mov_parse(TS0, ts0) == 1);
        CHECK(tsm->st[0].nfr == 3 && tsm->st[0].time_scale == 2000 &&
              tsm->st[0].scale_at == 1);
        CHECK(tsm->st[0].fr[0].thresh == 300u &&     /* before the line  */
              tsm->st[0].fr[1].thresh == 400u &&     /* after it: x2     */
              tsm->st[0].fr[2].thresh == 400u);
        CHECK(ts0->st[0].fr[1].thresh == 200u &&     /* MUT-B control    */
              ts0->st[0].time_scale == 1000 && ts0->st[0].scale_at == 0);
        ll_player_start(&u, tsm, 500, 100, 1);
        for (int t = 1; t <= 11; t++) {         /* 1100 units = 11 feeds */
            unsigned closed = 0;
            int want = 0;
            for (int k = 0; k < 3; k++)
                if (TS_CLOSE[k] <= t) {
                    closed += TS_THR[k];
                    want = k + 1;
                }
            if (want == 3)
                want = 0;
            ll_player_step(&u, NULL, &vk, 0, 2000, LL_ITER_MS);
            CHECK(u.fi == want &&
                  u.row_t == 100u * (unsigned)t - closed &&
                  u.row_rem == 0u);
        }
        free(rcm);
        free(tsm);
        free(ts0);
    }
    {   /* PR-114: the `fx != 0` half of the BY_X gate - the half no
         * leg was ever in. Section 4's feed and section 7's `walk`
         * flag both read `(st->ctrl & LL_CTRL_BY_X) && st->fx != 0`;
         * the second term is the port's own addition, since 0x543a0
         * keys mode 0 on the MODE alone (PR-100's owner reading). The
         * corpus count is now a tool output, not a claim:
         * tools/re/mov_byx_gate.py over the 777 .MOV measures 105
         * BY_X state blocks in 69 files, and 30 of them (290 rows,
         * 21 files) say F=0,0 - 29 of those 30 declare no motion
         * source of their own, so for them this is the whole law.
         * Three fixture states in this repo are BY_X with fx == 0
         * (here: GL's A_GO_TO_BALON_L; in ll_level.c: obj_motion's
         * `halt`, and a CHECK_LN ground probe with no F= line), but
         * none of them says anything about the row clock: A_GO_TO_
         * BALON_L's rows are 1 unit and its CHECKs read the drawn
         * frame; `halt`'s leg only checks that the object does not
         * move, and its comment marks the frame list "falls back to
         * time - estimate". So the choice was unpinned, not absent.
         * This leg pins it and changes nothing: what the engine does
         * with these 30 states is the open question, recorded as
         * NEXT_STEPS PR-114.
         * Hand-derived, no game data, every step at dt = LL_ITER_MS,
         * which is the grid where one step is exactly one feed.
         * A_PUSH is the control half: BY_X, F=1000,0, T=40,0. Under
         * the PR-523 law a held direction key is the full intent
         * s=20, and the add is idiv(F*s,100) = +200 u/s - but the
         * pass runs on the intent EDGE, at the pressing step, and
         * nowhere else: row 1 is 1000 units and the feed is the
         * whole-px step, so no row ever admits there.
         *   v1 = 1000 raw = 200 u/s, x += idiv(1000,100) = 10 px;
         *   v2: NO pass (no admit, no new edge) - the register, the
         *   position and the row all stand. The fixed point
         *   F*s/T = 1000 u/s is never approached: one edge, one
         *   pass, then the row gate holds everything.
         * The 10 px of that one step feed `facc` ONLY: one row of
         * 1000 units stays unclosed and row_t/row_rem stay exactly
         * 0 - the mode-0 branch replaces the wall clock.
         * The K_GEN1 pulse then moves him into A_COAST: BY_X with
         * F=0,0 and NO T= at all, the shape of 29 of the 30. Both
         * terms of the force law are now false, and two things
         * follow:
         * - the velocity is FROZEN at the entry value, 200 u/s: the
         *   law has neither term to change it - no F= word means no
         *   force and no T= word means no drag (the same reading
         *   PR-83 recorded for ZARIA's own A_TURN, and the reason
         *   the old "target velocity 0" vs "auto_walk" disagreement
         *   this leg kept on purpose no longer exists). It does not
         *   brake on release either, for the same reason.
         * - the rows run on the ITERATION clock instead: fx == 0
         *   disqualifies the distance feed, so section 7 feeds 100
         *   per step, six rows of 100 close one per step - and
         *   under PR-523 every one of those admits IS the physics
         *   pass: the body moves idiv(1000,100) = 10 px per step,
         *   integrated nowhere else. The sixth closing wraps
         *   (fi back to 0, `broke` armed by the wrap alone, which
         *   is why the loop below must end on that step). `facc`
         *   starts at 0 with the new state and stays 0.0 the whole
         *   way.
         * MUT-1 (delete `st->fx != 0`, i.e. key the feed on the mode
         * as the dispatcher does): A_COAST becomes a distance-fed
         * state - one 10-unit feed against 100-unit rows, no admit
         * and so no pass EVER again, the body frozen at the entry
         * px, and the leg fails at its first step - and no other
         * leg changes, which IS the coverage claim. MUT-2 (delete
         * the MODE half instead): this leg stands, PR-110's BY_TIME
         * legs fail. The mode half was covered; the fx half was
         * not. */
        static const char BX[] =
            "A_PUSH\nA_COAST\n\n[A_PUSH]\ncontrol=BY_X|CH_DIR\n"
            "F=1000,0\nT=40,0\nG=0,0\nA_COAST(1)=K_GEN1\nseq=a\n"
            "1,1000\n\n[A_COAST]\ncontrol=BY_X\nF=0,0\nT=0,0\nG=0,0\n"
            "seq=a\n-1,100\n-2,100\n-3,100\n-4,100\n-5,100\n-6,100\n";
        ll_mov_t *bxm = calloc(1, sizeof *bxm);
        ll_player bx;
        ll_play_keys bk = { 0 };
        int bad = 0;

        CHECK(bxm && ll_mov_parse(BX, bxm) == 2);
        CHECK((bxm->st[1].ctrl & LL_CTRL_BY_X) && bxm->st[1].fx == 0 &&
              bxm->st[1].tx == 0 && bxm->st[1].nfr == 6 &&
              bxm->st[1].fr[0].thresh == 100u);
        ll_player_start(&bx, bxm, 500, 100, 1);
        CHECK(bx.sti == 0 && bx.walkdir == 1 && bx.vx == 0.0);
        bk.k_fwd = 1;
        for (int t = 1; t <= 2; t++) {          /* the distance-fed half */
            double want = 200.0;                 /* one edge pass, then    */
                                                 /* the gate: no more      */
            ll_player_step(&bx, NULL, &bk, 0, 2000, LL_ITER_MS);
            if (bx.sti != 0 || bx.fi != 0 ||
                bx.vx < want - 1e-9 || bx.vx > want + 1e-9 ||
                bx.row_t != 0u || bx.row_rem != 0u)
                bad = 1;
        }
        CHECK(!bad);
        CHECK(bx.facc > 9.99 && bx.facc < 10.01);     /* fed, never enough  */
        CHECK(bx.x > 500 + 9.99 && bx.x < 500 + 10.01);  /* one 10 px pass  */
        double bxc = bx.x;                           /* where the walk got  */
        bx.sig |= 1ull << LL_SIG_K_GEN1;         /* the shipped exit shape:
                                                  * a pulse, not a key */
        bad = 0;
        for (int s = 1; s <= 6; s++) {
            ll_player_step(&bx, NULL, &bk, 0, 2000, LL_ITER_MS);
            if (bx.sti != 1)
                bad = 1;                            /* the pulse fired once */
            if (bx.fi != (s == 6 ? 0 : s))
                bad = 1;                            /* one row per step, and
                                                     * the 6th wraps it */
            if (bx.row_t != 0u || bx.row_rem != 0u)
                bad = 1;                            /* 100 in, 100 spent */
            if (bx.facc != 0.0)
                bad = 1;                            /* section 4's feed does
                                                     * not run at all here */
            if (bx.vx != 200.0)
                bad = 1;                            /* frozen: no F= term, and
                                                     * no T= term to change it */
            if (bx.broke != (s == 6))
                bad = 1;                           /* the wrap armed it */
            double want_x = bxc + s * 10.0;        /* idiv(1000,100) per */
            /* admit: the pass IS the row close here */
            if (bx.x < want_x - 1e-9 || bx.x > want_x + 1e-9)
                bad = 1;
        }
        CHECK(!bad);
        CHECK(bx.broke == 1);                      /* the wrap armed it */
        CHECK(bx.y == 100 && bx.vy == 0.0);        /* G=0,0: no fall of its
                                                    * own either */
        bad = 0;
        bk.k_fwd = 0;
        for (int s = 1; s <= 3; s++) {              /* and the same law is
                                                     * why it never brakes */
            ll_player_step(&bx, NULL, &bk, 0, 2000, LL_ITER_MS);
            if (bx.vx != 200.0 || bx.fi != s || bx.facc != 0.0)
                bad = 1;
        }
        CHECK(!bad);
        free(bxm);
    }
    {   /* PR-521: the x-side law in closed form, as PR-520 measured it live.
         * The register update at each row admit is
         *   v += idiv(F*s, 20) + G_x - idiv(v*T_x, 100)
         * with the authored ZARIA.01 A_WALK supplying F=100, T_x=20 and the
         * `G=30,350` word's first number G_x=30 - read exactly as PR-517 read
         * its second number as the flight's +55 y force. The x force is NOT
         * key-gated: G_x rides every admit, key down or not. HAND-DERIVED,
         * no game data: the 17 target values ARE the held-value ramp PR-520
         * records from a standstill (18 register states 1,131,...,638, each
         * held one admit), and the final key-release step is the value that
         * window names (541 with the authored G_x word, 511 if it is dropped).
         * This runs the arithmetic on the raw integer register through the
         * port's own ll_play_v_step and ll_play_x_force, so it pins BOTH the
         * new term and the primitive. MUTATION: with_gx=0 (the G_x line in
         * ll_play_x_force deleted) gives 511 at the last step and diverges
         * from the ramp earlier; the leg fails. This is why the x law is now
         * in the port (on the row-admit path only); OI-011's constant s is
         * deliberately NOT tested here - PR-520 passed s=20 in this arithmetic
         * because that is the live [obj+0xba] word, but the port's
         * LL_PLAY_INTENT_FULL stays 10 until the legs are re-derived. */
        static const char WX[] =
            "A_WALKX\n\n[A_WALKX]\ncontrol=BY_X\nF=100,0\nT=20,5\n"
            "G=30,350\nseq=a\n1,6\n";
        ll_mov_t wx;
        CHECK(ll_mov_parse(WX, &wx) == 1);
        const ll_mov_st *ws = &wx.st[0];
        CHECK(ws->fx == 100 && ws->tx == 20 && ws->gx == 30); /* the authored
                                                                 A_WALK words:
                                                                 F=100,0;
                                                                 T=20,5 ->
                                                                 T_x = 20;
                                                                 G=30,350 ->
                                                                 G_x = 30 */
        ll_player pw = { 0 };
        pw.dirword = 1;             /* the default direction word (0x50442);
                                     * PR-520's window faced right, so G_x is
                                     * the +30 force it measures */
        static const long long ramp[] = {
            131, 235, 318, 385, 438, 481, 515, 542, 564, 582, 596, 607, 616,
            623, 629, 634, 638 };  /* held value by held value, 18/18 */
        long long v = 1;            /* the capture starts at 1, not 0 */
        int bad = 0;
        for (size_t k = 0; k < sizeof ramp / sizeof ramp[0]; k++) {
            long long add = ll_play_x_force(&pw, ws, 20, 1); /* the held key:
                                                              * s = 20 is the
                                                              * live [obj+0xba] */
            if (add != 130)                                   /* idiv(100*20,20)
                                                               * = 100, + 30 */
                bad = 1;
            v = ll_play_v_step(v, ws->tx, add);
            if (v != ramp[k])
                bad = 1;
        }
        CHECK(!bad);
        CHECK(v == 638);            /* the top of the ramp, not an overshoot */
        /* The key-release step is the proof G_x is not key-gated: with the
         * key let go (s = 0) the register still gains the authored G_x. */
        long long rel_gx = ll_play_v_step(638, ws->tx,
                                          ll_play_x_force(&pw, ws, 0, 1));
        long long rel_no = ll_play_v_step(638, ws->tx,
                                          ll_play_x_force(&pw, ws, 0, 0));
        CHECK(rel_gx == 541);       /* the value that window shows */
        CHECK(rel_no == 511);       /* what dropping G_x predicts - wrong */
        CHECK(rel_gx != rel_no);
    }
    {   /* PR-115: PR-110 open items 1 and 8 - how many ROWS ONE STEP may
         * close. The engine admits exactly one. 0x52320-0x5232b, the
         * exact bytes read again for this record: `8b 43 10` (eax = the
         * row's threshold, [node+0x10]), `8b 9a a1 00 00 00` (ebx = the
         * accumulator [obj+0xa1]), `39 d8` (cmp eax,ebx), `7d 27`
         * (jge 0x52354: threshold >= accumulator -> the epilogue, no row
         * this step) - and `29 c3` at 0x52334 subtracts in REGISTERS
         * only, handing `acc - thr` to the row handler at
         * [[rec+0x44]+0x18]. Nothing in that path writes [obj+0xa1],
         * and the only writes to it in the dispatcher are feeds:
         * `add [obj+0xa1], eax` at 0x543d1 (mode 0: the truncated
         * v_x/100 of the tick), 0x543e7 (the constant 100) and 0x54412
         * / 0x5443b (the gated modes 3/4). So the accumulator is
         * monotonic and a covered threshold is admitted once per step.
         * PR-135 REMOVED that loop, its `4 * nfr` cap and its resync: the
         * state step now admits AT MOST ONE row per iteration and hands the
         * remainder over, which is the shape those bytes describe. The two
         * fixtures still separate the shapes, on arithmetic re-derived by
         * hand for the new clock (one call = one iteration per
         * LL_ITER_MS, a feed of 100, one admit, remainder carried in ms).
         * MULTI's rows parse to 100,30,30 (the port's clamp of the authored
         * `1,0` stays, ll_play.c:481) and it starts at x=100, fi=0:
         * - step 1, dt = 116 ms = 2 x LL_ITER_MS + 16: TWO iterations, so
         *   exactly two closures - row 0 (`M 100,0`, x 200) and then row 1
         *   (`M 30,0`, x 230) - row_t = 100 + 100 - 100 - 30 = 70,
         *   row_rem = 16, and no wrap happened, so `broke` is 0. Under the
         *   old loop this same step closed SIX rows (twice round the list,
         *   x 420, row_t 80) and the cap never fired; the two shapes are
         *   furthest apart exactly here.
         * - step 2, dt = LL_ITER_MS: one iteration, row_rem stays 16
         *   (16 + 50 - 50), row_t becomes 70 + 100 = 170, which covers
         *   row 2 - the last of the list, so the admit WRAPS it to index 0
         *   and arms `broke` - and its `M 30,0` puts x at 260, with 140 of
         *   the feed still owed. That leftover is the debt the engine keeps
         *   too: nothing in its path writes [obj+0xa1], so the remainder of
         *   one step is the head start of the next, and no feed of 100 can
         *   land the accumulator on 0 when a threshold is not a divisor of
         *   100.
         * BOUND is the same clock with a two-row list whose rows move by
         * different amounts, so the COUNT of closures is visible in x: rows
         * of 100 with `M 100,0` and `M 1,0`, nfr = 2, and dt = 1706 ms =
         * 34 x 50 + 6 feeds 34 iterations. Each admits exactly one row, so
         * 34 closures, alternating 17 of each row: x = 100 + 17*100 +
         * 17*1 = 1817, back at index 0 after an even number of steps with
         * the last one a wrap, so `broke` is 1 - it is cleared at the head
         * of every iteration (:992) and only a wrap arms it. row_t is 0
         * (each 100-feed drains exactly against a 100 threshold) and
         * row_rem is 6. Call 2, dt = LL_ITER_MS, feeds 100 more and closes
         * exactly one row: x +100 = 1917, index 1, and `broke` 0, because
         * this step did not wrap. The M deltas are what make the count
         * readable: a single-row list cannot tell "one closure per step"
         * from "many", because a feed of 100k drains exactly to 0 either
         * way - which is why the first version of this leg let M3 through.
         * The bounds clamp at :1228 is why the deltas here are positive and
         * the window is wide.
         * MUTATIONS THIS LEG NOW COVERS (PR-135's run: the harness is
         * /tmp/opencode/pr135_mut2.py and each mutant's full selftest
         * output is kept in /tmp/opencode/pr135/mut2/). Admitting two
         * rows per iteration - the old loop - breaks this leg at its
         * FIRST step: the two closures it asks for become four, the
         * three-row list wraps inside the call instead of on the next
         * one, and every later count of the MULTI half fails with it
         * (M1_play_two_admits.play.log, 8 checks; it also breaks two
         * frame-index CHECKs in the frame-lookup leg above). Zeroing the
         * accumulator instead of carrying it - PR-110's old resync -
         * fails the row_t/row_rem CHECKs of BOTH halves here and 13
         * checks in the walk-law and distance legs
         * (M2_play_zero_carry.play.log, 15 in all). Feeding the row
         * clock per millisecond instead of per iteration is the widest
         * failure of the set: 77 checks (M4_play_per_ms_feed.play.log).
         * Dropping the parser's clamp so an authored 0 threshold stays 0,
         * as the engine stores it, still fails this leg's first fixture
         * CHECK - the 30/30 rows the parser must raise to 100 first
         * (M14_parser_no_thresh_clamp.play.log). */
        static const char RRW[] =
            "MULTI\nBOUND\n\n[MULTI]\ncontrol=BY_TIME\n"
            "1,0, M 100,0\n2,30, M 30,0\n3,30, M 30,0\n"
            "\n[BOUND]\ncontrol=BY_TIME\n1,100, M 100,0\n"
            "2,100, M 1,0\n";
        ll_mov_t *rm = calloc(1, sizeof *rm);
        ll_player rp;
        ll_play_keys rk = { 0 };

        CHECK(rm && ll_mov_parse(RRW, rm) == 2);
        CHECK(rm->st[0].nfr == 3 && rm->st[0].fr[0].thresh == 100u &&
              rm->st[0].fr[1].thresh == 30u && rm->st[0].fr[2].thresh == 30u);
        CHECK(rm->st[0].fr[0].dx == 100 && rm->st[0].fr[1].dx == 30 &&
              rm->st[1].nfr == 2 && rm->st[1].fr[0].dx == 100 &&
              rm->st[1].fr[1].dx == 1);
        ll_player_start(&rp, rm, 100, 100, 1);
        ll_player_start(&rp, rm, 100, 100, 1);
        CHECK(rp.sti == 0 && rp.fi == 0 && rp.row_t == 0u);
        ll_player_step(&rp, NULL, &rk, 0, 200000, 116);
        CHECK(rp.sti == 0 && rp.fi == 2 && !rp.broke);  /* TWO iterations =
                                                         * two closures only */
        CHECK(rp.row_t == 70u && rp.row_rem == 16u);
        CHECK(rp.x == 230.0);        /* 100 + +100 (row 0) + +30 (row 1) */
        ll_player_step(&rp, NULL, &rk, 0, 200000, LL_ITER_MS);
        CHECK(rp.fi == 0 && rp.broke == 1);              /* the last row of
                                                         * the list: a wrap */
        CHECK(rp.row_t == 140u && rp.row_rem == 16u);   /* the debt is carried */
        CHECK(rp.x == 260.0);       /* +30 (row 2)                        */
        ll_player_start(&rp, rm, 100, 100, 1);
        rp.sti = 1;
        rp.fi = 0;
        ll_player_step(&rp, NULL, &rk, 0, 200000, 1706);
        CHECK(rp.fi == 0 && rp.broke == 1);              /* 34 closures, the
                                                         * last one a wrap */
        CHECK(rp.row_t == 0u && rp.row_rem == 6u);      /* each feed drains
                                                         * exactly: no cap,
                                                         * no resync */
        CHECK(rp.x == 1817.0);      /* 17 x +100 and 17 x +1            */
        ll_player_step(&rp, NULL, &rk, 0, 200000, LL_ITER_MS);
        CHECK(rp.fi == 1 && rp.broke == 0);              /* one closure, no
                                                         * wrap */
        CHECK(rp.row_t == 0u && rp.row_rem == 6u);
        CHECK(rp.x == 1917.0);
        free(rm);
    }
    if (!rc)
        printf("play selftest: .plt physics (+ the '!' separator variant,"
               " + an authored-empty '0' ground is data not an error),"
               " extended .MOV transitions walk/turn/stop, self-transition"
               " keeps its state (frames keep running), edge fall, K_GEN2"
               " landing pulse, PR-83: declared-name spawn, key-gated walk"
               " stops on release, GEN frame pulses, wrap/time= arm nothing,"
               " horizontal pull pair keeps the feet down, auto-walk stays,"
               " PR-85: facing-relative K_FWD/K_BKWD settle through the"
               " state pair / A_TURN+AC_BREAK, PR-86: (N) = target start"
               " row, authored AC_BREAK_NEXT ends a jump at its list wrap"
               " (landing no longer cuts it; fallback kept only without"
               " such a line), PR-87: comma groups AND (K_FIRE,K_F1"
               " attack entry - FIRE alone never fires it), PR-91: a raised"
               " AC_ENEMY_ATC drives the authored hit reaction (entry row,"
               " entry-row pull pair, its OWN AC_BREAK_NEXT wrap ends it),"
               " PR-108/148: the BY_TY climb runs the engine's input/"
               " guard law (mid-list entry row 3; with NO key held the"
               " 0x5441b mode-4 feed never runs - rows freeze, no"
               " wrap, no BREAK_NEXT; Up held gives the PR-108 spans"
               " 3,2,2,2,2,2,2,2,3; release mid-row keeps the carried"
               " 100 and wraps only when Down restarts it - the clock"
               " is sign-blind like the engine's cmp/je; x, y, v_x,"
               " the 0x542cd y force moves it: the register ramps"
               " -50,-75,-88,-94,-97,-99 to the -100 equilibrium"
               " and, because the position takes the TRUNCATED"
               " register, y is unmoved for six iterations and then"
               " the drag keeps"
               " pulling after the key is let go (-50, -25) and Down"
               " turns the register around (+37, +69, +85), every one"
               " of those steps sub-pixel, so y holds);"
               " !AC_DRAB drops him into A_FALL_DOWN from a state"
               " whose list never ticked), "
               "PR-110: the row clock is that iteration"
               " clock (a 300-unit row is 3 feeds, a 200-unit row 2, on"
               " the 28 ms AND the 20 ms grid, with the exact carry"
               " arithmetic checked), a mode-0 state's rows are fed by"
               " its distance only, and time_scale= pre-scales the rows"
               "PR-114: a BY_X state whose F= is 0,0 is"
               " NOT a walking one either - its velocity is frozen at the"
               " entry value (the law has no force term and, with no T="
               " authored, no drag term either; the same reading is why"
               " it never brakes on key release), its rows run on the"
               " ITERATION clock (one 100-unit row per feed, the sixth"
               " wraps the list and only that wrap arms BREAK_NEXT), and"
               " the distance accumulator stays exactly 0, PR-115: the"
               " state step admits ONE row per iteration, exactly as the"
               " engine's record step does (a 116 ms call is two"
               " iterations and closes exactly two rows of a"
               " 100/30/30 list, and the 70-unit remainder plus the 16"
               " ms carry are the debt the next call inherits; there is"
               " no cap and no resync any more, so a 1706 ms call is 34"
               " iterations and 34 rows, moved by their authored M"
               " deltas), and PR-135/PR-256: the clock IS the constant"
               " LL_ITER_MS (asserted above), the velocity is the"
               " engine's own INTEGER register - hundredths of a px per"
               " iteration - stepped by v += idiv(F*s,20) - idiv(v*T,100)"
               " with the sign-cross zero, and moved by idiv(v,100) WHOLE"
               " px, no carry of the remainder; the two gaits come out of"
               " the authored F/T and row pitch (the lion's register"
               " settles at 250 = F*s/T and his body walks 2 px an"
               " iteration, 3 iterations per 6-unit pose; the worm's"
               " settles at 125 and walks 1 px, 4 per 4-unit pose - the"
               " 20%% and 25%% difference against PR-134's video counts is"
               " this truncation, named at OI-015), a released key leaves"
               " a 4 px slide and a residue the arithmetic never zeroes,"
               " and a state that authors no T= takes nothing away at all,"
               " PR-395: the .blk record is x, TOP y and HEIGHT at"
               " rec+4/+8/+0x10 (the loader scans a comma pair and then"
               " one more number out of one token cursor, and its header"
               " count runs that scan that many times, so lines mean"
               " nothing),"
               " and the blockade query 0x45620 wants the wall's x inside"
               " the requester's TWO x and the wall's y RANGE to overlap"
               " its two y, then throws the body out of the wall on the"
               " side it overlapped it least - the target is the"
               " requester's own edge plus one, never a face of the"
               " record - and refuses a body already flush at 0 or 1 px"
               " without writing [obj+0xca]; the walk over the registry"
               " stops at the FIRST record that answers,"
               " PR-396: the .suf record is x, y and LENGTH ALONG x at"
               " rec+0/+4/+0xc (the same scanner, and the record's own"
               " constructor is what seeds its method table - this binary"
               " keeps a class record's fields at C+1/C+5/C+9/C+0xd),"
               " and the ceiling query 0x45500 is given the BODY, not a"
               " box: it wants the anchor's x on the line (both ends"
               " included), the body's own top at or below the line, and"
               " the anchor strictly above it, then moves the anchor down"
               " to the line through [obj+0xce] and never refuses a move"
               " of 1; its walk stops at the first answer too and raises"
               " a DIFFERENT flag bit, `or BYTE [result+3],0x2`, and it"
               " runs in the same physics step AFTER the blockades; and"
               " the registry it walks is named by the level's own"
               " `sufity=` line and lives in that level's container slot"
               " - three .SUF files, three SUFITY.DES sub-descriptors"
               " (SADES.06's carries six rows) and seven NONE.DES whose"
               " only token is `0` (ok)\n");

    /* PR-199: the `end_level <n>` row tail, end to end INSIDE the
     * player. The engine side is PR-198's read (the parse arm
     * 0x46beb-0x46c01; the apply 0x45750 of class 0xa1048 writing the
     * level object's +0x4; the loop's compare against zero at 0x4ac55
     * that returns the constant 1 for any nonzero). What is pinned here
     * is the port's half: the number is parsed as DATA, the row admit
     * copies it into the exit-word slot, the frame's own motion is
     * unchanged by carrying it, and NOTHING here reads the value as a
     * number or clears it again - the loop's test is the only reader,
     * and ll_level.c's play leg is what turns a nonzero into the loop
     * return 1 (the contract half, code 1 = run on with no reset, is
     * pinned by stage_selftest). */
    {
        static const char EL[] =
            "A_WALK\nA_EXIT\n\n[A_WALK]\ncontrol=BY_TIME\nF=0,0\n"
            "T=0,0\nG=0,0\nA_EXIT(1)=K_JUMP\nseq=a\n1,100\n"
            "2,100,end_level 2\n3,100\n\n[A_EXIT]\ncontrol=BY_TIME\n"
            "F=0,0\nT=0,0\nG=0,0\nseq=a\n0,200\n";
        static const char ELP[] = "1\n1 0,100;2000,100;\n";
        ll_mov_t *em = calloc(1, sizeof *em);
        ll_plt ep;
        ll_player eq;
        ll_play_keys ek = { 0 };
        int ekk, fired = -1;

        CHECK(em && ll_plt_parse(ELP, &ep) == 0);
        CHECK(ll_mov_parse(EL, em) == 2);
        CHECK(em->st[0].nfr == 3);
        CHECK(em->st[0].fr[0].endlv == 0 &&
              em->st[0].fr[1].endlv == 2 &&        /* the authored number, */
              em->st[0].fr[2].endlv == 0);         /* data, and ONLY there */
        ll_player_start(&eq, em, 500, 100, 1);
        CHECK(eq.exit_word == 0);                  /* the ctor's zero      */
        for (ekk = 0; ekk < 10 && !eq.exit_word; ekk++) {
            ll_player_step(&eq, &ep, &ek, 0, 2000, LL_ITER_MS);
            if (eq.exit_word && fired < 0)
                fired = ekk;
        }
        CHECK(eq.exit_word == 2);                  /* the admit set it     */
        CHECK(fired == 1);                         /* exactly when the     */
                                                  /* state's 2nd row became */
                                                  /* current, never before */
        for (ekk = 0; ekk < 60; ekk++)            /* and NOTHING clears   */
            ll_player_step(&eq, &ep, &ek, 0, 2000, LL_ITER_MS);
                                                  /* it: the engine's loop */
                                                  /* body has no store to */
                                                  /* the slot either      */
        CHECK(eq.exit_word == 2);
        CHECK(eq.sti == 0);                        /* the tail is not an  */
                                                  /* arm: the state never */
                                                  /* left A_WALK         */
        ll_plt_free(&ep);
        free(em);
    }

    /* PR-317: the `pos x,y` row tail, end to end inside the player.
     * The engine side is PR-316's read: the parse arm 0x466d6-0x46718
     * allocates a 0x10-byte node, writes the class word 0xa11b0 at
     * [node+0x4] and reads the pair through the format at 0x91944 into
     * [node+0x8] and [node+0xc]; the apply 0x45a30 takes the object out
     * of its context argument and calls ITS class +0x4 verb with
     * edx = x, ebx = y, which for the lion classes 0x96874/0x96960 is
     * 0x23850: `mov [eax],edx; mov [eax+0x4],ebx; call [class+0x54]` -
     * write [obj+0x0]/[obj+0x4] (the position the physics step and the
     * ask protocol both read) and then refresh the object through the
     * +0x54 verb, which recomputes the presented point at [obj+0xd6]
     * with the class +0xb0 provider. NO carry, NO clearance and NO
     * signal: it bypasses the physics step of PR-312 entirely. Both
     * shipped rows are A_WYJSCIE's `0,0,pos 1020,692` in ZAR_LEW and
     * ZAR_LEWPL (PR-309), and the port is what makes that level's
     * authored exit reachable, so the tail is modelled as the engine's
     * apply is: an absolute set at the moment its row is admitted.
     * The fixture's ground line is placed at 692 + the contact offset,
     * so the authored point is an exactly-grounded point: the ground
     * pass neither adds nor removes anything there and the assertion
     * isolates the set. */
    {
        static const char PP[] =
            "A_WPAD\n\n[A_WPAD]\ncontrol=BY_TIME\nF=0,0\n"
            "T=0,0\nG=0,0\nseq=a\n1,250,POS 220,140,GEN K_GEN1\n"
            "2,100,pos 1020,692\n3,100\n";
        static const char PPL[] = "1\n1 0,720;2000,720;\n";
        ll_mov_t *pm = calloc(1, sizeof *pm);
        ll_plt pp;
        ll_player pq;
        ll_play_keys qk = { 0 };
        int qk2, at = -1;
        double stay_x = 0, stay_y = 0, wrap_x = 0, wrap_y = 0;

        CHECK(pm && ll_plt_parse(PPL, &pp) == 0);
        CHECK(ll_mov_parse(PP, pm) == 1);
        CHECK(pm->st[0].nfr == 3);
        CHECK(pm->st[0].fr[2].has_pos == 0);
        CHECK(pm->st[0].fr[0].has_pos == 1 &&
              pm->st[0].fr[1].has_pos == 1 &&          /* the authored pair, */
              pm->st[0].fr[1].posx == 1020 &&          /* as DATA, and ONLY  */
              pm->st[0].fr[1].posy == 692 &&
              pm->st[0].fr[2].has_pos == 0);           /* on its own row     */
        /* the shipped RAN.01 [A_DEAD] shape: the UPPER-CASE spelling and
         * another tail right after the pair, so the tail scan must land on
         * `GEN` and not eat it. */
        CHECK(pm->st[0].fr[0].frame == 1 && pm->st[0].fr[0].thresh == 250 &&
              pm->st[0].fr[0].has_pos && pm->st[0].fr[0].posx == 220 &&
              pm->st[0].fr[0].posy == 140 &&
              (pm->st[0].fr[0].gsig & (1ull << LL_SIG_K_GEN1)));
        ll_player_start(&pq, pm, 300, 100, 1);
        for (qk2 = 0; qk2 < 8; qk2++) {
            ll_player_step(&pq, &pp, &qk, 0, 2000, LL_ITER_MS);
            if (at < 0 && pq.x == 1020 && pq.y == 692)
                at = qk2;
            if (qk2 == 4)                 /* a row with no `pos`: the  */
                stay_x = pq.x, stay_y = pq.y;   /* set must NOT drift or */
                                               /* accumulate             */
            if (qk2 == 6)                 /* the list wrapped and row 0  */
                wrap_x = pq.x, wrap_y = pq.y;   /* was admitted again: its */
                                               /* own pair re-applies    */
        }
        /* The derived moment. PR-110 pins the row clock as 100 units a
         * feed, so row 0 (250 units) is closed by the THIRD feed, which
         * is where ITS `POS 220,140` runs - iteration 2 - and row 1
         * (100 units) is closed one feed later: the authored pair must
         * appear on iteration 3 and nowhere earlier. */
        CHECK(at == 3);                    /* the set lands on the  */
                                          /* iteration its row is  */
                                          /* admitted, not before  */
        CHECK(stay_x == 1020 && stay_y == 692);   /* a row with no `pos`  */
                                                  /* moves nothing: it is  */
                                                  /* a set, not a delta   */
        CHECK(wrap_x == 220 && wrap_y == 140);    /* and the set runs  */
                                                 /* AGAIN on the row's  */
                                                 /* next admit, the list  */
                                                 /* wrap included: the  */
                                                 /* engine re-runs the  */
                                                 /* row's directive chain  */
                                                 /* every time it runs it */
        ll_plt_free(&pp);
        free(pm);
    }

    /* PR-321: the one-word `lock`/`unlock` tails, end to end inside the
     * player. The engine side is PR-320's read: the apply 0x45c50 takes
     * the owner through the shared binder 0x457e0 and calls 0x54f40 or
     * 0x54f90, which `or BYTE PTR [obj+0xb6],0x1` / `and ...,0xfe` -
     * set and clear VALUE 1; state entry 0x54e00 clears the value again
     * (`and ah,0xc0`). The physics step branches on it AT THE POSITION
     * STORE (0x54ee0-0x54f20): while it is set the store adds the
     * frozen anchor pair [obj+0x89]/[obj+0x8d] minus the presented
     * point, and 0x541f0 refreshes that pair only while the bit is
     * clear - so the drawn body stands still while a row sequence
     * holds the lock, whatever the velocity register does. ZARIA.01
     * brackets both of its exit states with the pair: A_WPAD rows 1
     * and 11, A_WYJSCIE rows 0 and 16, the `pos 1020,692` set of
     * PR-317 INSIDE the locked window. The port's counterpart of that
     * store branch is the integration gate in ll_player_step: while
     * p->lock is set the position words are not written; velocity,
     * the row clock feeds and the signal path run on, exactly as the
     * engine's store branch leaves them. */
    {
        static const char LKF[] =
            "[A_LK]\ncontrol=BY_TIME\nF=0,0\nT=0,0\nG=0,0\nseq=a\n"
            "1,100\n2,100,lock\n3,100,I 100,0\n4,100\n"
            "5,100,unlock\n6,100\n";
        static const char LKF2[] =      /* the same fixture with the
                                         * two words taken out: the
                                         * control case, the body must
                                         * walk while its row pulls */
            "[A_LK]\ncontrol=BY_TIME\nF=0,0\nT=0,0\nG=0,0\nseq=a\n"
            "1,100\n2,100\n3,100,I 100,0\n4,100\n"
            "5,100\n6,100\n";
        ll_mov_t *lk = calloc(1, sizeof *lk), *nl = calloc(1, sizeof *nl);
        ll_player lp, lq;
        ll_play_keys lkk = { 0 };
        double lx[6], nx[6];
        CHECK(lk && nl);
        CHECK(ll_mov_parse(LKF, lk) == 1);
        CHECK(lk->st[0].nfr == 6);
        CHECK(lk->st[0].fr[0].lockcmd == 0 &&
              lk->st[0].fr[1].lockcmd == 1 &&      /* the word, and ONLY  */
              lk->st[0].fr[4].lockcmd == -1 &&     /* on its own row      */
              lk->st[0].fr[2].lockcmd == 0 &&
              lk->st[0].fr[2].has_i && lk->st[0].fr[2].ix == 100);
        /* The derived timeline. PR-110's row clock feeds 100 a
         * iteration, so each 100-unit row closes in exactly one
         * iteration, and a row's directive runs when ITS row closes
         * (PR-317's timing, and 2044's admit block). Row 1 ends on
         * iteration 1: the bit is SET there, and the same boundary
         * admits row 2, whose `I 100,0` sets the register to 100 =
         * 1 px an iteration (PR-253's pull). Iterations 2 and 3 are
         * therefore the frozen window: register live, position not
         * written. Row 4 ends on iteration 4 and clears the bit, so
         * the first position write back is iteration 5: x = 301. */
        ll_player_start(&lp, lk, 300, 100, 0);
        for (int t = 0; t < 6; t++) {
            ll_player_step(&lp, NULL, &lkk, 0, 2000, LL_ITER_MS);
            lx[t] = lp.x;
        }
        CHECK(lp.lock == 0);                       /* the row cleared it   */
        CHECK(ll_play_v_raw(lp.vx) == 100);        /* the register never  */
                                                   /* stopped: only the    */
                                                   /* store was gated     */
        CHECK(lx[0] == 300 && lx[1] == 300);       /* before the lock: no */
                                                   /* velocity yet         */
        CHECK(lx[2] == 300 && lx[3] == 300 && lx[4] == 300);
                                                   /* the frozen window   */
        CHECK(lx[5] == 301);                       /* first write after   */
                                                   /* `unlock`            */
        /* The control half: the identical fixture minus the two words
         * walks the whole way, so the freeze above is the word and not
         * the row structure. Row 2 ends on iteration 2; the pull was
         * admitted at row 2's ADMIT (iteration 1's boundary), so the
         * first step is iteration 2: 301, 302, 303, 304. */
        CHECK(ll_mov_parse(LKF2, nl) == 1);
        CHECK(nl->st[0].fr[0].lockcmd == 0 && nl->st[0].fr[1].lockcmd == 0);
        ll_player_start(&lq, nl, 300, 100, 0);
        for (int t = 0; t < 6; t++) {
            ll_player_step(&lq, NULL, &lkk, 0, 2000, LL_ITER_MS);
            nx[t] = lq.x;
        }
        CHECK(nx[2] == 301 && nx[3] == 302 && nx[4] == 303 && nx[5] == 304);
        free(lk);
        free(nl);
    }

    /* PR-321, leg LK-C: the state ENTRY clears the lock - 0x54e00's
     * `and ah,0xc0` drops [obj+0xb6] to its top two bits, value 1
     * included. One authored state locks from its first closed row and
     * never unlocks; its list ends on the row that wraps, the wrap's
     * AC_BREAK_NEXT runs the transition, and the NEXT state - which
     * carries no words of its own - must have its position store back.
     * Derived: A's row 0 closes on iteration 0 and SETS the bit; the
     * row-1/row-2 `I 100,0` pulls are admitted at the boundaries but
     * iterations 1 and 2 write no position (300, 300); row 2 is last,
     * so its close on iteration 2 wraps and arms BREAK_NEXT, and
     * iteration 3 enters B, clears the bit, and applies B's own row-0
     * pull through the state-entry admit (PR-253) - the register is
     * live and the body walks: 301 on iteration 3, 302 on iteration 4
     * (the pulls re-admit on every row boundary). Without the
     * state-entry clear B would stand at 300 forever. */
    {
        static const char LKC[] =
            "A_L2\nA_L3\n\n[A_L2]\ncontrol=BY_TIME\nF=0,0\nT=0,0\n"
            "G=0,0\nA_L3(1)=AC_BREAK_NEXT\nseq=a\n"
            "1,100,lock\n2,100,I 100,0\n3,100,I 100,0\n"
            "\n[A_L3]\ncontrol=BY_TIME\nF=0,0\nT=0,0\nG=0,0\nseq=a\n"
            "1,100,I 100,0\n2,100,I 100,0\n3,100,I 100,0\n";
        ll_mov_t *kc = calloc(1, sizeof *kc);
        ll_player kp;
        ll_play_keys kkk = { 0 };
        double kx[5];
        CHECK(kc);
        CHECK(ll_mov_parse(LKC, kc) == 2);
        ll_player_start(&kp, kc, 300, 100, 0);
        for (int t = 0; t < 5; t++) {
            ll_player_step(&kp, NULL, &kkk, 0, 2000, LL_ITER_MS);
            kx[t] = kp.x;
        }
        CHECK(kp.sti == 1);                         /* the wrap ran the   */
                                                    /* transition         */
        CHECK(kx[1] == 300 && kx[2] == 300);        /* locked inside A    */
        CHECK(kx[3] == 301 && kx[4] == 302);        /* B starts UNlocked  */
        free(kc);
    }

    /* PR-321: the `ldir`/`rdir` tails - the engine's OWN writes to the
     * direction word [obj+0x6d] (applies 0x459f0/0x45a10, PR-320), and
     * 0x50442 rewrites the word to 1 at EVERY state start. Both legs
     * drive the word through the FC channel, the one consumer the
     * engine scales by the word EVERY iteration the row stays current
     * (0x54340, PR-250/PR-251), so a flip is visible in one iteration.
     * The fixtures also pin the shipped spellings' case twins (3 LOCK /
     * 9 RDIR / 1 LDIR in the corpus): like `pos`'s in PR-317, the name
     * match is case-insensitive. */
    {
        /* LEG D1 - the flip. Timeline (PR-110's 100-a-iteration row
         * clock, directives run when their own row closes): row 0 ends
         * on iteration 0 and writes -1. Rows 1-3 carry `FC 100,0`, so
         * iteration 1's x target is 100 * (-1): the register goes to
         * -100 and x walks 299, then 298. Row 2 carries `RDIR` too and
         * writes it when row 2 closes at the END of iteration 2, so
         * iteration 3's FC target is +100: it pulls the register from
         * -100 to 0 and x STANDS at 298 - the flip, one iteration
         * wide. An unmodelled word would walk to 297 there. */
        static const char D1[] =
            "[A_D1]\ncontrol=BY_TIME\nF=0,0\nT=0,0\nG=0,0\nseq=a\n"
            "1,100,ldir\n2,100,FC 100,0\n3,100,RDIR FC 100,0\n"
            "4,100,FC 100,0\n";
        ll_mov_t *d1 = calloc(1, sizeof *d1);
        ll_player p1;
        ll_play_keys kk1 = { 0 };
        double x1[4];
        CHECK(d1);
        CHECK(ll_mov_parse(D1, d1) == 1);
        CHECK(d1->st[0].fr[0].dircmd == -1 &&        /* the words, and    */
              d1->st[0].fr[2].dircmd == 1 &&         /* ONLY on their     */
              d1->st[0].fr[1].dircmd == 0 &&         /* own rows          */
              d1->st[0].fr[1].has_fc && d1->st[0].fr[1].fcx == 100 &&
              d1->st[0].fr[2].has_fc &&              /* the RDIR+FC row   */
              d1->st[0].fr[2].fcx == 100);           /* keeps BOTH tails  */
        ll_player_start(&p1, d1, 300, 100, 0);
        for (int t = 0; t < 4; t++) {
            ll_player_step(&p1, NULL, &kk1, 0, 2000, LL_ITER_MS);
            x1[t] = p1.x;
        }
        CHECK(x1[1] == 299 && x1[2] == 298);         /* `ldir` took effect */
        CHECK(x1[3] == 298);                         /* `RDIR` reversed it */
        free(d1);
    }
    {
        /* LEG D2 - the 0x50442 reset. Row 0 writes -1; rows 1-2 carry
         * `FC 100,0` and walk the body left (299, 298); row 2 is the
         * last one, so closing it on iteration 2 wraps the list and
         * the wrap's AC_BREAK_NEXT runs the authored transition on
         * iteration 3. The state entry rewrites the word to its
         * default, and the target's row 0 - which also CLOSES on
         * iteration 3, the transition zeroing the clock but the
         * feed still running that iteration (PR-115's one row per
         * iteration) - pulls the register from -100 to 0, so x
         * stands at 298; iteration 4's row-1 FC takes the register
         * to +100 and walks to 299. Had the word survived the state
         * change it would still pull left: 297 at iteration 3. */
        static const char D2A[] =
            "A_D2\nA_RST\n\n[A_D2]\ncontrol=BY_TIME\nF=0,0\nT=0,0\n"
            "G=0,0\nA_RST(1)=AC_BREAK_NEXT\nseq=a\n"
            "1,100,LDIR\n2,100,FC 100,0\n3,100,FC 100,0\n"
            "\n[A_RST]\ncontrol=BY_TIME\nF=0,0\nT=0,0\nG=0,0\nseq=a\n"
            "1,100,FC 100,0\n2,100,FC 100,0\n3,100,FC 100,0\n";
        ll_mov_t *d2 = calloc(1, sizeof *d2);
        ll_player p2;
        ll_play_keys kk2 = { 0 };
        double x2[5];
        CHECK(d2);
        CHECK(ll_mov_parse(D2A, d2) == 2);
        CHECK(d2->st[0].fr[0].dircmd == -1);         /* the LDIR spelling  */
        ll_player_start(&p2, d2, 300, 100, 0);
        for (int t = 0; t < 5; t++) {
            ll_player_step(&p2, NULL, &kk2, 0, 2000, LL_ITER_MS);
            x2[t] = p2.x;
        }
        CHECK(x2[1] == 299 && x2[2] == 298);         /* left, while the   */
                                                     /* word is authored   */
        CHECK(p2.sti == 1);                          /* the wrap ran the   */
                                                     /* transition         */
        CHECK(x2[3] == 298 && x2[4] == 299);         /* the state entry    */
                                                     /* rewrote the word  */
        free(d2);
    }

    /* PR-525, leg LK-S: THE ANCHOR SHIFT THE `lock`/`unlock` PAIR
     * CARRIES. 0x54f40 is not only the bit: behind its
     * `test BYTE PTR [ecx+0xb6],0x1; jne` guard it reads the current
     * element (the lion's getter [vt+0x48] is 0x23aa0, walking
     * [obj+0x5c] and the state array [obj+0x50] to the CURRENT ROW's
     * element record), takes the draw-flip sign ([vt+0x50] is
     * 0x53e40, -1 when the current frame record's flip word is set),
     * and calls that element's own land method 0x42980 with edx =
     * the object - the write target is [obj+0x0]/[obj+0x4], the live
     * position. 0x54f90 guards the OTHER way (`je`, LEAVE runs only
     * while the bit is set) and calls 0x429d0.
     * DERIVED on PR-110's 100-a-iteration row clock and PR-317's
     * chain timing (a row's directive chain runs when THAT row
     * CLOSES, and p->fi is still the closing row there, so the
     * shift takes the CLOSING row's element): rows 1..6 are T=100,
     * so row k closes at the end of iteration index k-1. Row 1
     * closes at index 0: LAND on element 0 - y -= 40, x -= 7 at
     * facing +1 - and the bit goes up. Row 3 closes at index 2 with
     * its own `lock`: the bit guard SKIPS the second LAND - this is
     * the leg's central pin, without that guard a chain re-run on a
     * locked row would walk the body off with it. Row 4 closes at
     * index 3: LEAVE on element 3, y += 33, x += 4, and the bit
     * clears. Row 5's `unlock` at index 4 is skipped the same way.
     * Net: 300,100 -> 293,60 -> 297,93, and the same fixture at
     * facing -1 takes x 307 then 303 (0x429bd/0x42a0d). */
    {
        static const char LKSF[] =
            "[A_LKS]\ncontrol=BY_TIME\nF=0,0\nT=0,0\nG=0,0\nseq=a\n"
            "1,100,lock\n2,100\n3,100,lock\n4,100,unlock\n"
            "5,100,unlock\n6,100\n";
        ll_mov_t *ls = calloc(1, sizeof *ls);
        ll_player sp, sq;
        ll_play_keys skk = { 0 };
        double sy[6], sx[6], mx[6];
        int sb[6];
        CHECK(ls);
        CHECK(ll_mov_parse(LKSF, ls) == 1);
        CHECK(ls->st[0].fr[0].lockcmd == 1 &&
              ls->st[0].fr[2].lockcmd == 1 &&
              ls->st[0].fr[3].lockcmd == -1 &&
              ls->st[0].fr[4].lockcmd == -1);
        ll_player_start(&sp, ls, 300, 100, 0);
        sp.anchor_shift = st_anchor_shift;
        sp.anchor_ctx = NULL;
        for (int t = 0; t < 6; t++) {
            ll_player_step(&sp, NULL, &skk, 0, 2000, LL_ITER_MS);
            sy[t] = sp.y; sx[t] = sp.x; sb[t] = sp.lock;
        }
        CHECK(sy[0] == 60 && sy[1] == 60 && sy[2] == 60);  /* LAND once,  */
                                                           /* guard holds */
        CHECK(sy[3] == 93 && sy[4] == 93 && sy[5] == 93);  /* LEAVE once  */
        CHECK(sx[0] == 293 && sx[2] == 293);               /* facing +:   */
        CHECK(sx[3] == 297 && sx[5] == 297);               /* 0x429a0 /   */
                                                           /* 0x429fb     */
        CHECK(sb[0] == 1 && sb[1] == 1 && sb[2] == 1);     /* the bit     */
        CHECK(sb[3] == 0 && sb[4] == 0 && sb[5] == 0);     /* stays put   */
        ll_player_start(&sq, ls, 300, 100, 0);             /* the mirror  */
        sq.face = -1;                                      /* half: the   */
        sq.anchor_shift = st_anchor_shift;                 /* flipped x   */
        for (int t = 0; t < 6; t++) {
            ll_player_step(&sq, NULL, &skk, 0, 2000, LL_ITER_MS);
            mx[t] = sq.x;
        }
        CHECK(mx[0] == 307 && mx[2] == 307);
        CHECK(mx[3] == 303 && mx[5] == 303);
        free(ls);
    }

    /* PR-526, leg EN-S: THE LEVEL'S LION CLASS ENDS THE STAGE. The
     * engine reads the flag it is about to act on at the top of the
     * NEXT level-loop iteration (0x4ac3b-0x4ac50 -> code 3), and it
     * raises that flag from the transition pass: 0x53520 matches the
     * transition list against its copy of the iteration's EventSet
     * (0x5356b), then calls [class+0x90] (0x5357d) - BEFORE the
     * applier 0x535a0 writes the new state into [obj+0x5c] (the
     * write is at 0x535ed). ZARIA's arm 0x1e250 sets [obj+0x116]=1
     * exactly when [obj+0x5c] is 9 - A_WPAD, the tenth name in
     * ZARIA's own declared table - and bit 2 of [obj+0x60], the
     * AC_BREAK_NEXT the row walk armed at 0x521c1 in THIS iteration,
     * is live. The transition the same wrap arms (`A_WYJSCIE(1)=
     * AC_BREAK_NEXT`) is NOT suppressed: the engine runs it and the
     * loop still ends.
     * DERIVED on PR-110's 100-a-iteration clock, the port's one-beat
     * convention included (transition pass reads what the row pass
     * armed the iteration before - PR-333): the fixture's two
     * T=100 rows close at the ends of iterations 0 and 1, the list
     * WRAPS at the end of iteration 1 (fi + 1 >= nfr arms p->broke),
     * and on iteration 2 the transition pass sees sti still A_WPAD
     * with the bit armed: end_req must be 1 from iteration 2 and
     * nowhere before, and the authored transition to A_DRAP still
     * runs at iteration 2 (sti == 1) - the flag changes no state,
     * no position, no signal. With the hook call commented out
     * exactly the one er[2] check fails and nothing else moves
     * (RED first). */
    {
        static const char ENSF[] =
            "A_WPAD\nA_DRAP\n\n"
            "[A_WPAD]\ncontrol=BY_TIME\nF=0,0\nT=0,0\nG=0,0\n"
            "A_DRAP(1)=AC_BREAK_NEXT\n1,100\n2,100\n"
            "[A_DRAP]\ncontrol=BY_TIME\nF=0,0\nT=0,0\nG=0,0\n1,100\n";
        ll_mov_t *es = calloc(1, sizeof *es);
        ll_player ep;
        ll_play_keys ek = { 0 };
        int er[3], es_sti[3];
        CHECK(es);
        CHECK(ll_mov_parse(ENSF, es) == 2);
        CHECK(es->nst == 2 && !strcmp(es->st[0].name, "A_WPAD") &&
              es->start_st == 0);              /* the declared first name,
                                               * exactly how ZARIA's own
                                               * table puts A_WPAD at
                                               * index 9 and the engine
                                               * compares 9 */
        ll_player_start(&ep, es, 300, 100, 0);
        ep.end_rule = st_end_rule;
        ep.end_rule_ctx = NULL;
        for (int t = 0; t < 3; t++) {
            ll_player_step(&ep, NULL, &ek, 0, 2000, LL_ITER_MS);
            er[t] = ep.end_req;
            es_sti[t] = ep.sti;
        }
        CHECK(er[0] == 0 && er[1] == 0);      /* nothing armed at the */
                                              /* passes of iters 0, 1  */
        CHECK(er[2] == 1);                    /* the wrap iteration's  */
                                              /* transition pass raises */
        CHECK(es_sti[0] == 0 && es_sti[1] == 0); /* the dive state runs */
        CHECK(es_sti[2] == 1);                /* and the authored      */
                                              /* transition still runs */
        free(es);
    }

    /* PR-323: the answer-6 protocol of PR-322 - `goto N` /
     * `GOTO_REL n` rewrite the ROW CURSOR at the row's close
     * (0x45d90 writes [rec+4] = the stored 0-based number,
     * 0x45dc0 adds the verbatim delta, and 0x52140 skips the
     * whole +1/wrap step when the chain walk answers 6). Both
     * legs are derived on the same 100-a-iteration row clock:
     * a row of T=100 closes on EVERY iteration. */
    {
        /* LEG G1 - the absolute jump. Row 0 carries `goto 4`:
         * the engine's parse stores 3 (0x46777's dec), so
         * closing row 0 at the END of iteration 0 puts the
         * cursor on the FOURTH row, and that row's `I 100,0`
         * - applied once when the row becomes CURRENT
         * (PR-253's admit law) - moves the body: iteration 1
         * walks to 301. Without the jump the I row only
         * becomes current at the end of iteration 2 and the
         * first step is iteration 3; the control fixture, the
         * same rows minus the word, must therefore still stand
         * at 300 through iteration 1. Iteration 1's close is
         * the normal last-row step (the cursor IS the last
         * row): wrap to 0 with the list bit - the jump does
         * not suppress THAT, only the step of the row that
         * carries the word. */
        static const char G1[] =
            "[A_G1]\ncontrol=BY_TIME\nF=0,0\nT=0,0\nG=0,0\nseq=a\n"
            "1,100,goto 4\n2,100\n3,100\n4,100,I 100,0\n";
        static const char G1C[] =
            "[A_G1]\ncontrol=BY_TIME\nF=0,0\nT=0,0\nG=0,0\nseq=a\n"
            "1,100\n2,100\n3,100\n4,100,I 100,0\n";
        ll_mov_t *g1 = calloc(1, sizeof *g1), *gc = calloc(1, sizeof *gc);
        ll_player gp, cp;
        ll_play_keys gk = { 0 };
        double gx[4], cx[4];
        CHECK(g1 && gc);
        CHECK(ll_mov_parse(G1, g1) == 1 && ll_mov_parse(G1C, gc) == 1);
        CHECK(g1->st[0].fr[0].gotocmd == 1 &&        /* the word, stored  */
              g1->st[0].fr[0].gotoval == 3 &&        /* 0-based, and ONLY */
              g1->st[0].fr[1].gotocmd == 0 &&        /* on its own row    */
              g1->st[0].fr[3].gotocmd == 0);
        ll_player_start(&gp, g1, 300, 100, 0);
        for (int t = 0; t < 4; t++) {
            ll_player_step(&gp, NULL, &gk, 0, 2000, LL_ITER_MS);
            gx[t] = gp.x;
        }
        CHECK(gp.fi == 0);                           /* 0 -> 3 -> wrap    */
        CHECK(ll_play_v_raw(gp.vx) == 100);          /* the jumped-to     */
                                                     /* row's pull ran    */
        CHECK(gx[1] == 301 && gx[2] == 302 && gx[3] == 303);
        ll_player_start(&cp, gc, 300, 100, 0);
        for (int t = 0; t < 4; t++) {
            ll_player_step(&cp, NULL, &gk, 0, 2000, LL_ITER_MS);
            cx[t] = cp.x;
        }
        CHECK(cx[0] == 300 && cx[1] == 300 && cx[2] == 300);
        free(g1);
        free(gc);
    }
    {
        /* LEG G2 - the relative loop, the lion's A_DEAD shape
         * (`27,400,GOTO_REL -3`): row 5 is the list's last one
         * and adds -3, so closing it on iteration 4 puts the
         * cursor back on row 2 and the NORMAL step - the wrap,
         * the AC_BREAK_NEXT raise, everything - is SKIPPED
         * (0x52140). The state therefore never reaches its
         * declared AC_BREAK_NEXT transition: an unmodelled
         * word wraps on iteration 4 and the transition runs on
         * iteration 5 (PR-83/PR-86's mechanism, D2's timing).
         * The x line confirms the loop: row 2's `I -100,0` is
         * re-applied at the re-entry (admit law), holding the
         * register at -100 - a steady one-pixel-a-step drift
         * with no gap anywhere. */
        static const char G2[] =
            "A_G2\nA_END\n\n[A_G2]\ncontrol=BY_TIME\nF=0,0\nT=0,0\n"
            "G=0,0\nA_END(1)=AC_BREAK_NEXT\nseq=a\n"
            "1,100\n2,100,I -100,0\n3,100\n4,100\n5,100,GOTO_REL -3\n"
            "\n[A_END]\ncontrol=BY_TIME\nF=0,0\nT=0,0\nG=0,0\nseq=a\n"
            "1,100\n";
        ll_mov_t *g2 = calloc(1, sizeof *g2);
        ll_player kp;
        ll_play_keys kk = { 0 };
        double kx[7];
        CHECK(g2);
        CHECK(ll_mov_parse(G2, g2) == 2);
        CHECK(g2->st[0].fr[4].gotocmd == 2 &&        /* the delta, kept   */
              g2->st[0].fr[4].gotoval == -3 &&       /* verbatim          */
              g2->st[0].fr[0].gotocmd == 0);
        ll_player_start(&kp, g2, 300, 100, 0);
        for (int t = 0; t < 7; t++) {
            ll_player_step(&kp, NULL, &kk, 0, 2000, LL_ITER_MS);
            kx[t] = kp.x;
        }
        CHECK(kp.sti == 0);                          /* never wrapped:    */
                                                     /* no BREAK_NEXT     */
        CHECK(kx[0] == 300 && kx[1] == 299 && kx[2] == 298 &&
              kx[3] == 297 && kx[4] == 296);         /* the first pass    */
        CHECK(kx[5] == 295 && kx[6] == 294);         /* and the looped    */
                                                     /* one, seamless     */
        free(g2);
    }
    {
        /* PR-330, LEG S - the row line that OPENS with the engine's
         * `s <seq>` field. ZARIA.01's own [A_FALL_DOWN] is the data:
         * THREE authored rows, of which this parser read two until
         * this PR - and the missing one is the FIRST, so every row
         * index and the `A_FALL_DOWN(3)` start-row token were one row
         * off. The engine appends that row: 0x51a6a-0x51af7, all of
         * it riding one chained character cursor per line (the name
         * with " %[^ ,]%*[ ,]" at 0x92148, the frame with "%d," at
         * 0x92156, the units with "%hi" at 0x92115 inside the row
         * ctor 0x515e0), and the record the name becomes is stored ON
         * THE ROW at [node+0x8]. The port models the row and COUNTS
         * the sequence switch (st->seq_switch, reported): drawing it
         * is OI-120. */
        static const char SD[] =
            "[A_FALL_DOWN]\ncontrol=BY_TIME|FRAME_REV|CHECK_ALL|CH_DIR\n"
            "F=120,0\nT=30,1\nG=0,55\n"
            "A_WPAD(1)=AC_ONAIR,AC_DRECT\nA_DRAP(1)=AC_BREAK_NEXT\n"
            "A_FALL_DOWN(3)=!AC_ONAIR\nseq=fall_dn\n"
            "s idle 1,300,GEN K_GEN1\n7,0,WAIT K_NEVER\n"
            "8,300,F 0,100\n";
        ll_mov_t *sd = calloc(1, sizeof *sd);
        CHECK(sd);
        CHECK(ll_mov_parse(SD, sd) == 1);
        CHECK(sd->st[0].nfr == 3);                 /* ALL THREE rows,     */
        CHECK(sd->st[0].seq_switch == 1);          /* one `s`-led among   */
        CHECK(sd->st[0].fr[0].frame == 1 &&        /* the frame and the   */
              sd->st[0].fr[0].thresh == 300 &&     /* units come AFTER    */
              sd->st[0].fr[0].gsig ==              /* the name, and its   */
              (1ull << LL_SIG_K_GEN1));            /* tails still parse   */
        CHECK(sd->st[0].fr[1].frame == 7 &&        /* `7,0` keeps the     */
              sd->st[0].fr[1].thresh == 100 &&     /* engine's minimum    */
              sd->st[0].fr[1].gsig_id == -2);      /* K_NEVER = no bit    */
        CHECK(sd->st[0].fr[2].frame == 8 &&        /* the landing row     */
              sd->st[0].fr[2].thresh == 300 &&
              sd->st[0].fr[2].has_f &&
              sd->st[0].fr[2].fx == 0 && sd->st[0].fr[2].fy == 100);
        CHECK(ll_mov_row_start(&sd->st[0], 3) == 2);   /* the authored    */
                                                       /* (3) lands on    */
                                                       /* `8,300,F 0,100` */
        /* The two ctor arguments differ and only this state shows it:
         * the `s` arm pushes 0x3e8 (0x51ab7), the ordinary row fill
         * pushes the state's [st+0xe] (0x51f37-0x51f3e). LONA.05's
         * T_TELEPORT trio is `time_scale=600` with 37 `s`-led rows -
         * the one place in the shipped set where the two differ, and
         * the shape below is that state's, 600 and 300 verbatim. */
        static const char SC[] =
            "[T_TELEPORT1]\ncontrol=BY_TIME\ntime_scale=600\n"
            "seq=uj22\n1,300\ns uj25a 2,300\n";
        ll_mov_t *sc = calloc(1, sizeof *sc);
        CHECK(sc);
        CHECK(ll_mov_parse(SC, sc) == 1);
        CHECK(sc->st[0].nfr == 2 && sc->st[0].seq_switch == 1);
        CHECK(sc->st[0].fr[0].thresh == 180 &&     /* 300 * 600 / 1000, */
                                              /* the PR-110 law   */
              !sc->st[0].fr[0].no_scale);
        CHECK(sc->st[0].fr[1].frame == 2 &&        /* the `s` row keeps */
                                              /* its authored     */
              sc->st[0].fr[1].thresh == 300 &&     /* units            */
              sc->st[0].fr[1].no_scale);
        free(sc);
        /* The field-name compare is the engine's plain byte compare
         * 0x76d20 (0x76a69, which folds A-Z, is only for state names),
         * and the name scan needs a token: all 206 shipped lines are a
         * lowercase `s`, a name, and the numbers. A nameless `s` line
         * WOULD still append a row in the original - the `%d,` scan
         * fails and the ctor takes the caller's UNINITIALIZED slot, an
         * undefined read with no shipped instance - and this parser
         * calls it not-a-row, which is the named difference. */
        static const char SN[] =
            "[A]\nS idle 1,300\ns\ns ,1,300\nsample=25\nseq=a\n1,100\n";
        ll_mov_t *sn = calloc(1, sizeof *sn);
        CHECK(sn);
        CHECK(ll_mov_parse(SN, sn) == 1);
        CHECK(sn->st[0].nfr == 1 && sn->st[0].seq_switch == 0);
        free(sd);
        free(sn);
    }

    /* PR-331, the SURFACE-LINK raises of the engine's physics step
     * (the tail 0x54760-0x54803, after the ground queries 0x4b430/
     * 0x4b7f0 of PR-312/313): when the query finds NO surface the
     * engine detaches whatever [obj+0xa9] held and raises AC_ONAIR
     * (0x54800 `or BYTE PTR [edi+0x2],0x4`, bit 18); attaching when
     * nothing was held raises AC_DUP (0x54785 `or BYTE PTR [edi],0x10`,
     * bit 4); SWITCHING surfaces raises NEITHER; and a surface whose
     * kind word the query leaves at [obj+0xe2] is not 0 posts
     * AC_DUP_CD every step (0x547d1, bit 13). That word is not
     * modelled here - no
     * shipped .plt surface carries one - and that gap is OI-121. The
     * port's counterpart of the query result is p->grounded. The
     * clauses this makes live are the authored ones:
     * `A_FALL_DOWN(1)=AC_ONAIR` is 109 lines in 11 of the 777
     * extracted texts (208 clause lines name AC_ONAIR, in 23 files),
     * and `A_JUMP(11)=AC_DUP` 13 lines in 7 - the legs below are the
     * first time any of them RUN in this port. Timing follows the
     * pulse model exactly: a raise at the end of iteration N is seen
     * by the transition block of iteration N+1 (the FIRST block of
     * player_iterate, before the clear). */
    {
        static const char OA_MOV[] =
            "A_STAY\nA_FALL\nA_BACK\n\n"
            "[A_STAY]\ncontrol=BY_TIME|CHECK_LN\nF=0,0\nT=0,0\nG=0,0\n"
            "A_FALL(1)=AC_ONAIR\nseq=a\n1,100\n\n"
            "[A_FALL]\ncontrol=BY_TIME|CHECK_LN\nF=0,0\nT=0,0\nG=0,0\n"
            "A_BACK(1)=AC_DUP\nseq=a\n1,100\n\n"
            "[A_BACK]\ncontrol=BY_TIME|CHECK_LN\nF=0,0\nT=0,0\nG=0,0\n"
            "seq=a\n1,100\n";
        static const char OA_PLT[] = "1\r\n1 0,100;700,100;\r\n";
        ll_mov_t *oa = calloc(1, sizeof *oa);
        ll_plt oplt;
        ll_player op;
        ll_play_keys ok_ = { 0 };
        CHECK(oa);
        CHECK(ll_mov_parse(OA_MOV, oa) == 3);
        CHECK(ll_plt_parse(OA_PLT, &oplt) == 0);

        /* T1: a player STANDING on the shelf. The first physics step
         * ATTACHES (nothing was held) - that one iteration posts
         * AC_DUP, exactly like the engine's first query after spawn -
         * and every later one posts NOTHING: a held surface does not
         * re-raise AC_DUP, and a found surface never raises AC_ONAIR
         * (the found path 0x5477b-0x54798 is the attach branch). The
         * start carries grounded = 0: the engine's ctor zeroes
         * [obj+0xa9] too, and the ATTACHING is the first physics
         * step's own act. */
        ll_player_start(&op, oa, 100, 100, 0);
        for (int t = 0; t < 3; t++) {
            ll_player_step(&op, &oplt, &ok_, 0, 2000, LL_ITER_MS);
            CHECK(!(op.sig & (1ull << LL_SIG_AC_ONAIR)));
            CHECK((op.sig & (1ull << LL_SIG_AC_DUP)) ==
                  (t == 0 ? (1ull << LL_SIG_AC_DUP) : 0ull));
            CHECK(op.grounded);
        }
        CHECK(op.sti == 0);                  /* no clause fired        */

        /* T2: a player DROPPED above the shelf, in the air. Every
         * airborne iteration posts AC_ONAIR (the query finds nothing,
         * 0x547d7-0x54803); the authored `A_FALL(1)=AC_ONAIR`
         * transition - inert in every earlier PR - runs on the
         * iteration AFTER the first raise; AC_DUP is never posted
         * while in the air. On the landing iteration the attach edge
         * posts AC_DUP exactly once and AC_ONAIR stops; the authored
         * `A_BACK(1)=AC_DUP` then runs, and the step after that
         * posts neither. */
        ll_player_start(&op, oa, 450, 60, 1);
        int first_onair = -1, dup_at = -1, n_onair = 0, n_dup = 0;
        int fell_at = -1, back_at = -1;
        for (int t = 0; t < 200 && back_at < 0; t++) {
            ll_player_step(&op, &oplt, &ok_, 0, 2000, LL_ITER_MS);
            int on = !!(op.sig & (1ull << LL_SIG_AC_ONAIR));
            int dp = !!(op.sig & (1ull << LL_SIG_AC_DUP));
            if (on) {
                n_onair++;
                if (first_onair < 0)
                    first_onair = t;
                CHECK(op.sti != 2);         /* never reached A_BACK
                                             * while still airborne   */
            }
            if (dp) {
                n_dup++;
                dup_at = t;
            }
            if (op.sti == 1 && fell_at < 0)
                fell_at = t;
            if (op.sti == 2)
                back_at = t;
        }
        CHECK(first_onair == 0);            /* the first airborne
                                             * iteration raises it    */
        CHECK(fell_at == 1);                /* the next one runs the
                                             * authored clause        */
        CHECK(n_onair == dup_at);           /* onair every airborne
                                             * iteration, and none
                                             * from the landing on    */
        CHECK(n_dup == 1);                  /* AC_DUP exactly once,
                                             * at the attach          */
        CHECK(back_at == dup_at + 1);       /* the AC_DUP clause runs
                                             * one iteration later    */
        CHECK(op.grounded);
        ll_player_step(&op, &oplt, &ok_, 0, 2000, LL_ITER_MS);
        CHECK(!(op.sig & ((1ull << LL_SIG_AC_ONAIR) |
                          (1ull << LL_SIG_AC_DUP))));
        free(oa);
    }

    {
        /* PR-333, the row step's TWO PATHS (the engine's 0x521f0, read end
         * to end by PR-332) on a hermetic one-state .MOV. A row with NO
         * `Wait` clause is the EMPTY answer (0x495b0 returns 12) and takes
         * the clock path: its threshold is compared against the row
         * accumulator at 0x52320 and the remainder carried - so a
         * 1000-unit row takes exactly ten iterations of the 100-per-step
         * feed. A row whose clause carries bits takes the match path at
         * 0x52251: no match and the step returns through 0x522ed having
         * done NOTHING - no frame, no cursor step, no wrap, no
         * AC_BREAK_NEXT - and a match runs with the ZERO budget of
         * 0x522f8: its threshold is never consumed and the accumulator is
         * left at zero, whatever it says. The clause is a bit mask with
         * the clause's OWN mode: plain is the subset arm 0x47e63, `!` is
         * shares-nothing 0x47edc, and K_NEVER is the resolver's own mask
         * K_UP|K_DOWN (0x4806f-0x4807d) - not a wildcard for "never",
         * which is why case 2 below ADMITS it with both arrows held.
         * Rows: `1,100` / `2,1000,<tail>` / `3,100`, twelve iterations.
         * `at` is the iteration that first reaches row 3, `fi`/`wr` where
         * the list stands after twelve and how many wraps it took. */
        static const struct {
            const char *tail;   /* the row-2 `Wait` tail; "" = no clause */
            int up, down;       /* the arrows held throughout             */
            int at, fi, wr;
        } U_CASES[] = {
            { "WAIT K_NEVER", 0, 0, 0, 1, 0 },   /* subset of {UP,DOWN}
                                                  * with neither up: parks
                                                  * for good at 1100 units,
                                                  * far past its own 1000,
                                                  * and NEVER wraps */
            { "WAIT K_NEVER", 1, 1, 2, 0, 4 },   /* both arrows: it admits
                                                  * in ONE iteration, the
                                                  * 1000 units unread */
            { "WAIT !K_DOWN", 0, 0, 2, 0, 4 },   /* shares nothing with
                                                  * DOWN: admits */
            { "WAIT !K_DOWN", 0, 1, 0, 1, 0 },   /* DOWN held: parks */
            { "WAIT K_DOWN",  0, 1, 2, 0, 4 },   /* one-bit subset: admits */
            { NULL,           0, 0, 11, 0, 1 },  /* no clause: the clock
                                                  * path, ten feed
                                                  * iterations to pass the
                                                  * 1000-unit row - the
                                                  * same row the four
                                                  * gated cases passed at
                                                  * once or never */
        };
        static const char UP[] = "1\n1 0,100;700,100;\r\n";
        for (size_t uc = 0; uc < sizeof U_CASES / sizeof U_CASES[0]; uc++) {
            char mov[512];
            ll_mov_t *um = calloc(1, sizeof *um);
            ll_plt upt;
            ll_player u;
            ll_play_keys uk = { 0 };
            int at = 0, wr = 0, prev_fi = 0;
            unsigned zeroed = 0;

            snprintf(mov, sizeof mov,
                     "A_STAY\n\n[A_STAY]\ncontrol=BY_TIME|CHECK_LN\n"
                     "F=0,0\nT=0,0\nG=0,0\nseq=a\n1,100\n2,1000%s%s\n3,100\n",
                     U_CASES[uc].tail ? "," : "",
                     U_CASES[uc].tail ? U_CASES[uc].tail : "");
            uk.k_up = U_CASES[uc].up;
            uk.k_down = U_CASES[uc].down;
            CHECK(um && ll_plt_parse(UP, &upt) == 0);
            CHECK(ll_mov_parse(mov, um) == 1);
            CHECK(um->st[0].nfr == 3);
            ll_player_start(&u, um, 100, 100, 1);
            for (int t = 1; t <= 12; t++) {
                ll_player_step(&u, &upt, &uk, 0, 2000, LL_ITER_MS);
                if (u.fi > prev_fi && u.fi == 2 && !at)
                    at = t;                     /* the iteration that
                                                 * reached row 3 */
                if (prev_fi == 2 && u.fi == 0)
                    wr++;                       /* a list wrap */
                if (u.fi == 2 && u.row_t != 0u && !zeroed)
                    zeroed = t;                 /* the gated admit left a
                                                 * non-zero clock: the zero
                                                 * budget of 0x522f8 is
                                                 * NOT being applied */
                prev_fi = u.fi;
            }
            CHECK(at == U_CASES[uc].at);
            CHECK(u.fi == U_CASES[uc].fi);
            CHECK(wr == U_CASES[uc].wr);
            if (U_CASES[uc].at) {
                /* a matched clause was handed the ZERO budget: the clock is
                 * left at zero, NOT one tick past the row's own threshold -
                 * the old code would have carried 200 - 1000 here. */
                CHECK(!zeroed);
            } else {
                CHECK(u.row_t == 1100u && !zeroed);
            }
            free(um);
        }
    }

    /* PR-441: the vertical-intent flags of PR-438's 0x54910. The
     * field the builder tests at 0x5499d-0x549b4 is [obj+0xbe], the
     * INTENT word (PR-148's y-intent, PR-326's y-force input), NOT
     * the velocity register at [obj+0xc6]. A first draft of this
     * change tested the register's sign and MOVED the authored
     * ZARIA.01 climb leg (verify-play): at the drop its A_DRAP
     * carries `A_LOOKDOWN(1)=K_DOWN`, the model's vy is positive
     * while the intent is zero, and the chain re-routed through
     * A_LOOKDOWN instead of the authored `A_FALL_DOWN(1)=AC_ONAIR`
     * exit - the video-verified chain (PR-192 and after) says the
     * drop takes the AC_ONAIR line with NO Up/Down involved. The
     * fixture below pins that negative half directly: a body whose
     * register RISES, with no key held, matches no K_UP clause.
     * The positive half - held Up answers K_UP - is the key-flag
     * path above, exercised by the authored ZARIA ladder and RAN
     * legs since PR-108; the intent word and the keys coincide in
     * this port's model, and the non-key intents (the contact
     * chain's word, PR-149's kind-4 arm at 0x54b16) stay named at
     * OI-016. */
    {
        static const char VS_MOV[] =
            "A\nC\n\n[A]\ncontrol=BY_TIME\nF=0,0\nT=0,0\nG=0,0\nseq=a\n"
            "C(1)=K_UP\n1,100\n2,20,I 0,-200\n3,30000\n\n"
            "[C]\ncontrol=BY_TIME\nF=0,0\nT=0,0\nG=0,0\nseq=a\n1,30000\n";
        static const char VS_PLT[] = "1\n1 0,100;2000,100;\n";
        ll_mov_t vm;
        ll_plt vp;
        ll_player v;
        ll_play_keys vk = { 0 };

        CHECK(ll_mov_parse(VS_MOV, &vm) == 2 && vm.st[0].ntr == 1 &&
              vm.st[0].fr[1].has_i && vm.st[0].fr[1].iy == -200);
        CHECK(ll_plt_parse(VS_PLT, &vp) == 0);
        ll_player_start(&v, &vm, 500, 100, 1);
        /* t=0: the list advances to row 2, whose `I 0,-200` lifts the
         * register (PR-253's one shot at row entry); no clause
         * matches, because the register was ZERO when the pass ran. */
        ll_player_step(&v, &vp, &vk, 0, 2000, LL_ITER_MS);
        CHECK(v.fi == 1 && v.sti == 0 && ll_play_v_raw(v.vy) < 0);
        /* t=1,2: the register is NEGATIVE at both transition passes
         * and no key is held - under the corrected reading nothing
         * matches, the list simply runs on. (The velocity-only
         * reading answered K_UP here and entered C; the ZARIA climb
         * leg rejects that half of its own accord.) */
        ll_player_step(&v, &vp, &vk, 0, 2000, LL_ITER_MS);
        CHECK(v.sti == 0 && v.fi == 2 && ll_play_v_raw(v.vy) < 0);
        ll_player_step(&v, &vp, &vk, 0, 2000, LL_ITER_MS);
        CHECK(v.sti == 0 && v.fi == 2);
    }

    return rc;
}

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
                if (sscanf(line, "%d,%d", &f, &ms) == 2) { /* frame line   */
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
                        mf->dx = mf->dy = 0;
                        mf->gsig = 0;
                        mf->atc = 0;
                        mf->has_i = 0;
                        mf->has_f = mf->fx = mf->fy = 0;   /* PR-253 tails */
                        mf->has_fc = mf->fcx = mf->fcy = 0;
                        mf->endlv = 0;
                        mf->gsig_id = -1;   /* PR-217: no `Wait` tail */
                        mf->gneg = 0;
                        mf->gop = LL_GOP_NAME;
                        /* A frame line may carry further fields. The ones the
                         * model acts on are the authored motion of THIS frame,
                         * "N,ms, M dx,dy" (285 of 18617 shipped frame lines
                         * carry it; there are no standalone "M" lines), the
                         * `GEN <signal>` tails (PR-88: the engine's own
                         * directive table is at 0x917c0, the names it can
                         * raise are the 0x919ec block) and ATC_ON/ATC_OFF
                         * (PR-89, the attack-connect window).
                         * Everything else the table names (ground_on/off,
                         * lock/unlock, chdir, ldir/rdir, pos, goto/gotorel,
                         * F/IC, MOVE, the smp and vol sound
                         * words, inline F=) is counted -
                         * measured 1568 such lines before this PR took
                         * the GEN and ATC ones out of that count, PLAN
                         * 8g - never dropped in silence. `end_level`
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
                         * counted but unmodeled.
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
                                     * own signal clause, KEPT as DATA
                                     * (ll_mov_fr.gsig_id/gneg), not acted
                                     * on yet. The arm mirrors the engine's
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
         * FOUR iterations per row, not two. */
        for (int r = s->scale_at; r < s->nfr; r++)
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
static unsigned long long live_bits(const ll_player *p,
                                   const ll_play_keys *k)
{
    int face = p->face >= 0 ? 1 : -1;
    int fwd  = face  > 0 ? k->k_fwd : k->k_bkwd;
    int bkwd = face  > 0 ? k->k_bkwd : k->k_fwd;
    unsigned long long m = p->sig;
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
    if (p->broke)     m |= 1ull << LL_SIG_AC_BREAK_NEXT;
    if (p->ac_time)   m |= 1ull << LL_SIG_AC_TIME;
    return m;
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
 * scales the row FC pair by the same unread word (0x54340); that word's
 * value is not read, so `add` carries no x gravity here - counted at
 * OI-015, not guessed. */
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
    /* The horizontal target keeps PR-74's facing multiply: the engine
     * instead scales the X pair by the unread terrain/facing word
     * [obj+0x6d] (0x54340), which stays an open read. */
    ll_play_pull(&vx100, tx * (p->walkdir ? (long long)p->walkdir : 1));
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
    /* PR-256: this body is 0x54250 + the row clock + the position step,
     * and every number in it is in that pass's units: the velocity
     * register's (a signed integer, hundredths of a px per ITERATION) and
     * whole px for the position. Nothing here scales by the caller's dt:
     * one call of this body is one iteration, which is what the caller's
     * accumulator guarantees. */

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
    p->sig = 0;                                      /* one-tick pulses */
    p->broke = 0;
    p->ac_time = 0;

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
                               ((long long)st->fx * (long long)s) / 20LL);
    }                                       /* add 0x542ab-0x542c7 makes */
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
    p->vert_intent = keys->k_up ? -LL_PLAY_INTENT_FULL
                                : (keys->k_down ? LL_PLAY_INTENT_FULL : 0);

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
         * gate). Its x side is the one the engine scales by [obj+0x6d]
         * (0x54340); the facing multiply stands in for that word. */
        ll_play_pull(&vx100, (long long)st->fr[p->fi].fcx *
                             (p->walkdir ? (long long)p->walkdir : 1));
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
    p->x += dxv;
    p->y += (double)(vy100 / 100);        /* [obj+0xce] */
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
    if ((st->ctrl & LL_CTRL_BY_X) && st->fx != 0)
        p->facc += dxv < 0 ? -dxv : dxv;

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
            p->row_t += 100;          /* one iteration = exactly one
                                       * feed of the engine's constant
                                       * (0x4c8b7): no millisecond
                                       * arithmetic is left here, the
                                       * clock is ll_player_step */
        if ((walk ? p->facc : (double)p->row_t) >=
                (double)st->fr[p->fi].thresh) {
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
            if (walk)
                p->facc -= (double)st->fr[p->fi].thresh;
            else
                p->row_t -= st->fr[p->fi].thresh;
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
            if (p->fi + 1 >= st->nfr) {
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
         * 20,20,20000, and the first feed of 100 lands in the SAME
         * iteration that entered the state (transitions run first), so
         * row 0 closes and row 1's pull fires right there; with one
         * row admitted per iteration the 20-unit rows are one
         * iteration each and the 20000-unit row never ends before the
         * state does (this fixture declares no AC_BREAK_NEXT exit, so
         * PR-86's fallback is the route).
         * THE ARC, per iteration (PR-256's exact arithmetic: the T_y=1
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
         * The 20000-unit third row keeps the state alive through the
         * whole flight, so the row list - not a fitted constant -
         * decides the air. The apex is arithmetic on the authored
         * words: no estimate remains in this leg, the PR-73/74 /4 +
         * 1650 fit is retired by this PR (see ll_play.h). */
        static const char JM[] =
            "A_STAY\nA_JUMP\n\n[A_STAY]\ncontrol=BY_TIME|CHECK_LN\n"
            "F=600,0\nT=90,90\nG=0,0\nA_JUMP(1)=K_JUMP\nseq=a\n1,100\n\n"
            "[A_JUMP]\ncontrol=BY_TIME\nF=180,0\nT=20,1\nG=0,55\nseq=a\n"
            "1,20\n2,20,I 120,-1200\n3,20000\n";
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
         * and M4-C raises that 0 to 100, so the list is 2225 units:
         *   1 enter A_JUMP(1) = row index 0; closes row 0 (75)
         *   2 closes row 1 (75) -> row 2 becomes CURRENT, and its
         *     `I 150,-1200` pull fires on that admit: vy = -240,
         *     vx = +30, grounded = 0 (PR-253: /LL_PLAY_RAW_VDIV)
         *   3 closes row 2 (75); 4 closes 3 (100); 5 closes 4 (150);
         *   7 closes 5 (150); 9 closes 6 (225) -> the 900-unit row
         *     (index 7) holds steps 9..18
         *   18 closes it (900); 20 closes 8 (225); 21 closes 9 (100 =
         *     the M4-C clamp of the authored 0); 23 closes 10 (150)
         *     and the leftover 75 is kept as the debt against the next
         *     pass - the engine's state step admits ONE row per step,
         *     so the wrap lands on the NEXT step, 24 - which arms
         *     AC_BREAK_NEXT and the match comes at the top of step 24.
         * Cumulative units closed: 75, 150, 225, 325, 475, 625, 850,
         * 1750, 1975, 2075, 2225 - and the 75 still owed.
         * The jump physics are that arithmetic (PR-256): the two
         * `F 0,350` heads fire on rows 0 and 1 while he is still
         * GROUNDED, the ground contact zeroes the register again before
         * the `I 150,-1200` admit SETS it to -1200 (with the x pair
         * +150), and from there the loop's own steps read +6, -7, -7,
         * -6, -6, -5, -4, -4, -3, -3, -2, -1, -1 and then 0, 0, 0, 0:
         * the APEX is y = 57 at loop step 12, 43 px up, where PR-253's
         * float model computed 48.13. The whole-px step truncates the
         * top of the arc away, and it truncates the fall's first steps
         * too - but the authored +55 per iteration wins and, unlike the
         * float model, HE LANDS. The wrap exit at step 24 (unchanged by
         * this PR: it is row arithmetic, not physics) moves him to
         * A_WALK row 3, the `A_WALK(3)` seek, mid-air at y = 73; that
         * target's row 0 `1,100,F 0,350` pulls the register to -350 UP,
         * and A_WALK authors no G of its own, so the project's
         * LL_PLAY_GRAVITY word (200 per iteration, see ll_play.h) is
         * the bigger add and the steps go +2, +2, ... down to the
         * surface at loop step 37. What this leg OWNS is the row
         * grammar - the (N) entry token, the wrap arming the state's
         * OWN exit at the seek's row, the carry across it. What it no
         * longer covers is "a landing does not cut a state that
         * declares its own AC_BREAK_NEXT exit": with this arithmetic the
         * flight does not outlast this row list, so the state is gone
         * before the surface arrives. Counted as a coverage gap in the
         * PR-256 record, not papered over. (The JM fixture above keeps
         * covering the FALLBACK branch: its A_JUMP declares no exit, so
         * its landing still goes idle.) */
        static const char JE[] =
            "A_WALK\nA_JUMP\n\n[A_WALK]\ncontrol=BY_TIME|CHECK_LN\n"
            "F=600,0\nT=90,90\nG=0,0\nA_JUMP(1)=K_JUMP\n"
            "A_WALK(3)=AC_BREAK_NEXT\nseq=a\n1,100\n2,100\n3,100\n4,100\n\n"
            "[A_JUMP]\ncontrol=BY_TIME\nF=180,0\nT=20,1\nG=0,55\nseq=a\n"
            "1,75,F 0,350\n2,75,F 0,350,smpauto\n2,75,I 150,-1200\n2,100\n"
            "3,150,FC 30,0\n4,150,GEN K_GEN1\n5,225,GEN K_GEN1\n"
            "6,900,GEN K_GEN1\n7,225,GEN K_GEN1\n8,0,WAIT AC_DUP\n"
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
        CHECK(je->st[1].nfr == 11);
        CHECK(je->st[1].fr[2].has_i && je->st[1].fr[2].ix == 150 &&
              je->st[1].fr[2].iy == -1200);
        CHECK(je->st[1].fr[7].thresh == 900u);      /* the 900-unit middle */
        CHECK(je->st[1].fr[9].thresh == 100u);      /* authored 0 -> 100   */
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
        for (int t = 0; t < 45; t++) {              /* t = leg step - 2    */
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
        CHECK(air2 == 37 && land == 37);    /* the flight ENDS: the whole
                                             * -px fall reaches the surface
                                             * at loop step 37 */
        CHECK(exit_t == 24 && exit_fi == 3);  /* wrap arms the OWN exit
                                                * at the seek's row     */
        CHECK(exit_t < land);               /* ... mid-air, 13 steps
                                             * before the surface: the
                                             * "landing does not cut the
                                             * state" half of this leg is
                                             * no longer reachable here */
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
         * PR-106 case the stand-in was counted for. Derived step by
         * step with LL_PLAY_GRAVITY = 200: v = 0 + 200 - 0 = 200
         * (y += idiv(200,100) = 2 -> 102), v = 200 + 200 - 100 =
         * 300 (y += 3 -> 105), v = 300 + 200 - 150 = 350 (y += 3
         * -> 108). The fixed point of a 200 add against a 50% drag
         * is idiv(50*v,100) = 200, v = 400, i.e. 4 px an iteration;
         * the leg checks the register is climbing toward it. */
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
        for (int t = 0; t < 3; t++)
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
            "[A_DRAP]\ncontrol=BY_TIME\nF=100,0\nG=0,0\n"
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
         * THE RECURSION, from the constants in this fixture, in the
         * engine's own arithmetic (PR-256): the force adds
         * idiv(100*10, 20) = 50 per iteration and the drag takes
         * idiv(20*v, 100) of the OLD register, so the register walks
         *   50, 90, 122, 148, 169, 186, 199, 210, 218, ... -> 250
         * and settles at exactly 250 = F*s/T = 50 u/s of register. The
         * STEP is idiv(v,100) whole px, so the body moves
         *   0, 0, 1, 1, 1, 1, 1, 2, 2, 2, ... = 2 px per iteration
         * once the register is at 250: 40 u/s of TRAVEL, where the
         * register alone says 50. PR-134's measured bracket [38,76] u/s
         * brackets the travel, and 40 sits inside it at the low edge.
         * THE GAIT, one row of 6 units admitted per iteration (the
         * one-admit rule of 0x521f0) over the distance above,
         * D = 0,0,1,2,3,4,5,7,9,11,13,15,...: the rows close at
         *   N = 8, 11, 14, 17, 20, 23, 26, 29
         * - exactly 3 iterations per pose at speed, 0.15 s per pose at
         * this clock. That is the point of this leg now: the authored
         * row distance and the authored F/T, through the engine's
         * arithmetic, give the gait. */
        sk.k_fwd = 1;
        ll_player_step(&s, &zp, &sk, 0, 2000, LL_ITER_MS);
        CHECK(s.sti == 1);
        zx = s.x;
        int badlaw = 0;
        long long vwant = 0, dwant = 0;    /* the register walk, derived
                                            * here from this fixture's own
                                            * words: the force adds
                                            * idiv(F*s,20) = idiv(100*10,20)
                                            * = 50 per iteration, the drag
                                            * takes idiv(T*v,100) = idiv(v,5)
                                            * of the OLD register, and the
                                            * step is idiv(v,100) whole px.
                                            * C's / truncates toward zero
                                            * as x86 idiv does. */
        {   long long d1 = vwant / 5, p1 = vwant + 50;
            vwant = p1 - d1; dwant += vwant / 100;    /* iteration 1: the
                                                        * press call above */
        }
        for (int t = 0; t < 17; t++) {      /* 18 iterations of the walk in
                                              * all: enough to show the gait
                                              * and to stop the release slide
                                              * BEFORE this gait reaches one
                                              * of ZARIA's real `GEN AC_TIME`
                                              * rows (see the release note
                                              * below) */
            long long vd = vwant / 5, vp = vwant + 50;
            vwant = vp - vd;
            dwant += vwant / 100;           /* [obj+0xca], the only thing
                                              * that moves the body */
            ll_player_step(&s, &zp, &sk, 0, 2000, LL_ITER_MS);
            if (ll_play_v_raw(s.vx) != vwant)
                badlaw = 1;                 /* the closed form above, not a
                                             * copy of the model's recursion */
            CHECK(vwant <= 250);            /* the register's steady state:
                                             * idiv(v,5) equals the 50 the
                                             * force adds from v = 250 on,
                                             * and the walk reaches exactly
                                             * 250 - while the BODY travels
                                             * idiv(250,100) = 2 px per
                                             * iteration, i.e. 40 u/s, not
                                             * the 50 u/s the register
                                             * alone says. PR-134's measured
                                             * bracket [38,76] u/s brackets
                                             * the travel, and 40 is inside
                                             * it at the low edge. */
            if (s.row_t != 0u || s.row_rem != 0u)
                badlaw = 1;             /* section 7 must NOT feed a walking
                                         * BY_X state: 0x543a2's mode-0
                                         * branch replaces the 100-per-
                                         * iteration feed */
            int nrow = (int)(dwant / 6);    /* the gait, from the distance
                                             * this pair of words travels:
                                             * 6 units per row, and once the
                                             * register is at 250 that is
                                             * exactly 3 iterations per pose
                                             * (0.15 s), the cadence of the
                                             * authored row distances at the
                                             * authored F/T */
            if (s.fi != nrow)
                badlaw = 1;                 /* the gait cadence, row by row */
        }
        CHECK(!badlaw);
        CHECK(s.sti == 1);                   /* the first two rows raised
                                              * AC_TIME, but this state's own
                                              * A_WALK(x) line is earlier in
                                              * the list and first match
                                              * wins (the engine's table
                                              * order, PR-86) */
        CHECK(dwant == 27 && s.x == zx + 27.0);
        CHECK(s.fi == 4);                    /* four of the twelve rows: the
                                              * rows of 6 units close at
                                              * iterations 8, 11, 14, 17 on
                                              * the distance above */
        /* RELEASE (this is where the SDL KEYUP fix enters the picture -
         * a stuck key would keep walking forever). NO !K_FWD exit
         * exists, so the walk state stays and the ONLY thing that stops
         * him is the authored drag with s = 0, which in this arithmetic
         * is v - idiv(v,5): 247, 198, 159, 128, 103, 83, 67, 54, 44,
         * ... The drag term itself truncates to 0 once the register is
         * below 5, so the register keeps a residue (4 here) forever -
         * nothing in 0x54250 zeroes a small magnitude, only a sign
         * cross. What stops is the STEP: idiv(v,100) is 0 from v = 83
         * on, so the whole slide is 1 + 1 + 1 + 1 = 4 px in four
         * iterations, and it adds up independently below.
         * Under the old model this leg read differently in kind, not in
         * decimals: a target velocity of T=20 braked by F=100 to a stop
         * in 10 steps and 4.4 px, which stopped the list mid-cycle.
         * What the data says instead is a 4 px slide and a register that
         * never reaches 0 - and ZARIA's own A_WALK has no authored brake
         * of its own, exactly as PR-83 reported. The game's real brake
         * is a STATE, not a term: this gait's fifth and later rows are
         * `GEN AC_TIME` rows (ZARIA.01 lines 136-143, copied into this
         * fixture), and the real A_WALK declares `A_DRAP(1)=AC_TIME`
         * (line 134), so a walk long enough to close one of those rows
         * while the key is NOT held is routed into the idle state, whose
         * header authors T_x=60 - a drag of 60% of the register per
         * iteration, which from 250 gives 100, 40, 16, 7, 3, 2, 1, 1:
         * one pixel of travel and a stop. This leg releases early, at
         * the fourth row, so the slide inside A_WALK stays measurable;
         * the A_DRAP route is measured (PR-256 record) but not pinned
         * here, and this fixture's own [A_DRAP] is a one-row stub that
         * authors no T at all, so it brakes nothing - it is NOT the real
         * idle block. */
        double xr = s.x;
        int rel_idle = 0;
        long long vbrk = vwant, dbrk = 0;
        sk.k_fwd = 0;
        for (int t = 1; t <= 30; t++) {     /* long enough to reach the
                                              * residue: from 247 the drag
                                              * alone needs 21 iterations to
                                              * walk down to 4 */
            vbrk -= vbrk / 5;               /* the same drag, with the */
            dbrk += vbrk / 100;             /* force's add at s = 0    */
            ll_player_step(&s, &zp, &sk, 0, 2000, LL_ITER_MS);
            if (s.sti == 2)
                rel_idle = t;
            if (ll_play_v_raw(s.vx) != vbrk)
                badlaw = 1;                   /* a non-gated BY_X state
                                               * would sit at the F*s/T
                                               * target through all of this */
        }
        CHECK(rel_idle == 0 && s.sti == 1);
        CHECK(!badlaw && vbrk == 4 && vbrk / 100 == 0);
                                              /* the residue stays in the
                                               * register; the step is zero */
        CHECK(dbrk == 4 && s.x == xr + 4.0);
        CHECK(s.fi == 5);                      /* five of twelve: the 27 px
                                               * of the walk plus the 4 px
                                               * of the slide, at 6 per row
                                               * - and the fifth row the
                                               * slide closes is `2,6`, no
                                               * AC_TIME, so the state is
                                               * still A_WALK */
        double xs = s.x;
        for (int t = 0; t < 15; t++)
            ll_player_step(&s, &zp, &sk, 0, 2000, LL_ITER_MS);
        CHECK(s.sti == 1 && vbrk != 0 && vbrk / 100 == 0);
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
        /* THE PULL LAW, on the register this leg's own release left
         * behind: the drag stopped at a residue of 4 (see the release
         * note), and 0x54170 from the opposite sign CLIMBS by |target|
         * and never past, so the -100 target lands at 4 - 100 = -96, not
         * at -100: 19.2 u/s, which idiv(.,100) truncates to a step of
         * ZERO px. The turn therefore does not slide here - a direct
         * consequence of the residue the drag keeps, and the mirror of
         * what PR-83 recorded as "froze the turn slide forever": A_TURN's
         * F=0,0 with no T= authored takes NOTHING away (no force term, no
         * drag term), so whatever the register holds runs through the
         * whole state, and A_DRAP's stub block is the same. From a body
         * that was actually still (register 0) the same pull gives
         * exactly -100, as PR-253 recorded. */
        CHECK(ll_play_v_raw(s.vx) == -96);
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
        /* The walk LEFT starts from the register the pull left, -96, with
         * the sign of s reversed (facing left). Derived here from the
         * fixture's own words, in the register's unit: the force adds
         * idiv(F*s,20) = idiv(100*-10,20) = -50 an iteration, the drag
         * takes idiv(T*v,100) = idiv(v,5) of the OLD register, and the
         * step is idiv(v,100) whole px:
         *   -127, -152, -172, -188, -201, -211, ... -> -250
         * the same F*s/T = -50 u/s of register, and the BODY steps
         *   -1, -1, -1, -1, -2, -2, ... = -2 px an iteration
         * The call that ENTERED A_WALK is iteration 1 (a transition runs
         * before the physics of the same iteration): the pull's -96 does
         * not move the body at all (idiv(-96,100) = 0, see the note on
         * the turn above), but that call's own pass runs with A_WALK's
         * words, gives -127, and steps 1 px. The 25 calls below are
         * n = 2..26 and add -(1+1+1+2*22) = -47 px from where xw is
         * taken; with the entry call's 1 px the state's own row distance
         * is 48 whole px - 40 u/s of travel, the same rate as the
         * right-hand walk, mirrored. */
        double xw = s.x;
        long long vl = -127, dl = 0;       /* the entry call's -1 px is
                                            * already in xw; its register
                                            * is what the loop below
                                            * continues from */
        if (ll_play_v_raw(s.vx) != vl)
            badlaw = 1;
        for (int t = 0; t < 25; t++) {
            long long vld = vl / 5, vlp = vl - 50;   /* idiv(v,5), the
                                                      * facing-reversed add */
            vl = vlp - vld;
            dl += vl / 100;
            ll_player_step(&s, &zp, &sk, 0, 2000, LL_ITER_MS);
            if (ll_play_v_raw(s.vx) != vl || s.x != xw + (double)dl)
                badlaw = 1;
        }
        CHECK(!badlaw);
        CHECK(dl == -47 && s.x == xw - 47.0);      /* the 25 calls after the
                                                    * entry, and the entry's
                                                    * own 1 px is what makes
                                                    * the row count 8 */
        CHECK(s.fi == 8);                            /* eight of the twelve
                                                     * rows: 48 whole px at
                                                     * 6 units per row, and
                                                     * the entry iteration's
                                                     * 1 px of it is part of
                                                     * that distance */
        ll_plt_free(&zp);
        free(zm);
    }
    {   /* PR-135's second measured-law leg, RE-DERIVED by PR-256 for the
         * worm pair. ZARIA.01's ZWIERZ1 worm is (F,T) = (100,40) with a
         * 9-row list of 4-unit pitch. In the engine's own arithmetic
         * (add = idiv(F*s,20) = 50 per iteration, drag = idiv(T*v,100) =
         * idiv(2v,5) of the OLD register, step = idiv(v,100) whole px)
         * the register walks
         *   50, 80, 98, 109, 116, 120, 122, 124, 125, 125, ...
         * and stops at 125: idiv(2*125,5) = 50 equals the add, so 125 =
         * 25 u/s of register = F*s/T, reached at iteration 9. The STEP is
         * idiv(v,100) = 1 px from iteration 4 on, so D(n) = n - 3 whole
         * px and the rows of 4 units close at
         *   7, 11, 15, 19, 23, 27, 31, 35, 39
         * - exactly 4 iterations per pose, 0.20 s at this clock.
         * THE DISAGREEMENT WITH THE VIDEO, counted not smoothed: PR-79
         * timed a 1.44 s cycle for this creature = 28.8 iterations, i.e.
         * 3.2 iterations per pose, and PR-134's table gives the (100,40)
         * pair 1.25 px per iteration - which is exactly what the REGISTER
         * says (125 hundredths) and exactly what the whole-px STEP throws
         * away. So this arithmetic is 25% slow on this gait. Either the
         * engine keeps the idiv remainder somewhere in the [obj+0xca]
         * chain (0x54358-0x54372 and 0x54827-0x54829 show no carry), or
         * the BY_X row feed is not the post-truncation step. Named as an
         * open item, on the same footing as the [obj+0x6d] multiplier:
         * recorded, not fitted. The lion's own pair (100,20) settles at
         * 250 = 2.5, truncates to 2 px, and its gait comes out 3
         * iterations per pose against PR-134's 2.4 - the same
         * truncation, 20% there.
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
            long long q6 = wv * 40 / 100;     /* idiv(T*v,100); C's / is */
            wv = wv + 50 - q6;                /* x86 idiv, toward zero   */
            wd += wv / 100;                   /* [obj+0xca], whole px     */
            ll_player_step(&wp, NULL, &wk, 0, 200000, LL_ITER_MS);
            if (ll_play_v_raw(wp.vx) != wv || wp.x != 500.0 + (double)wd)
                badw = 1;
            if (wp.sti != 0 || wp.grounded != 1 || wp.y != 100.0)
                badw = 1;                     /* G=0,0: the walk stays on
                                               * its line */
            if (wp.fi != (int)(wd / 4) % 9)
                badw = 1;                     /* the gait table, row by row,
                                               * from the distance above */
            if (wp.broke && !wrap_at)
                wrap_at = t;
        }
        CHECK(!badw);
        CHECK(wv == 125 && wd == 37);         /* the register's fixed point
                                               * and the distance travelled */
        CHECK(wrap_at == 39 && wp.fi == 0);   /* nine rows closed by 36 px at
                                               * iteration 39, and the list
                                               * wrapped back to row 0 there;
                                               * that row is now 1 px into
                                               * itself */
        CHECK(wp.x == 500.0 + 37.0);          /* 36 of it spent on nine
                                               * rows, 1 unit owed */
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
            "time=50\nA_RUN(1)=AC_TIME\nseq=a\n1,100\n";
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
         * under the engine law is v_n = 0.7 v_(n-1) + 10 ->
         * F*s/T = 1000/30 = 33.33 u/s, not the 30 of the old
         * "T = target velocity" reading. Its own row runs on distance,
         * 33.33 u/s * 0.05 = 1.67 units per iteration, so the 100-unit
         * row needs 60 iterations and stays open through the 40 of
         * this leg - no AC_TIME anywhere (and this is a BY_X state,
         * whose mode never runs the time feed at all). */
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
        long long pv = 50, pd = 0;          /* A_PULL's own arithmetic,
                                             * derived from its header:
                                             * BY_X and AUTO, so the intent
                                             * is held at full and the force
                                             * adds idiv(F*s,20) = 50 an
                                             * iteration while the drag
                                             * takes idiv(T*v,100) =
                                             * idiv(3v,10) of the OLD
                                             * register. The call that
                                             * entered A_PULL (pull_at
                                             * above) already ran one such
                                             * pass from rest, and its step
                                             * idiv(50,100) is 0 px, so the
                                             * register starts at 50 here
                                             * and the position has not
                                             * moved. */
        for (int t = 0; t < 40; t++) {
            pv = pv + 50 - pv * 30 / 100;
            pd += pv / 100;                 /* [obj+0xca], whole px */
            ll_player_step(&q, &pp, &qk, 0, 2000, LL_ITER_MS);
            CHECK(ll_play_v_raw(q.vx) == pv);
        }
        CHECK(pv == 167 && pd == 39);       /* the register's fixed point:
                                             * idiv(3*166,10) = 49 < 50 so it
                                             * still grows, and idiv(3*167,
                                             * 10) = 50 equals the add. That
                                             * is 33.4 u/s of register, a
                                             * hair over the F*s/T = 33.33
                                             * the float model settled at -
                                             * the truncation of the drag is
                                             * what leaves the extra unit.
                                             * The body moves idiv(167,100)
                                             * = 1 px an iteration from
                                             * iteration 3 on: 39 px in the
                                             * 40 calls of this loop, and
                                             * A_PULL's own 100-unit row is
                                             * 39 units into itself, so no
                                             * AC_TIME and no exit. */
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
         * is 18 of them.
         * - idle 4 iterations: A_WALK is BY_X with no key held, so
         *   NOTHING feeds its rows (the mode-0 branch feeds the distance
         *   travelled and there is none); its 3 rows of 6 need 6 units of
         *   travel, row_t and row_rem stay 0, no row closes.
         * - leg step 5: the raise makes the transition match at the top of
         *   the iteration; the entry row (index 0) STAYS current and its
         *   `I -320,-250` pair fires at the admit: 0x54170 SETS both axes
         *   from rest to the authored numbers, in the register's unit -
         *   the x in the facing direction. The feed of 100 that same
         *   iteration leaves row_rem = 100 against the 200-unit row, so
         *   nothing closes.
         * - THE X SIDE: the authored T_x=3 is a real drag of 3% of the
         *   register, truncated: -311, -302, -293, ... It is not the
         *   -64 x 0.97^n curve; the two first terms agree to a hundredth
         *   and then the truncation keeps more.
         * - THE Y SIDE: the pull lifts -250; each iteration adds the
         *   authored G_y=55 word FIRST and subtracts idiv(3*v,100) of the
         *   old register: -188, -128, -70, -13, +42, +96, +149. The
         *   STEP is idiv of that by 100, so the hop is 1 px + 1 px and
         *   then FIVE iterations of zero motion while the register climbs
         *   through the truncation band - the flinch is a 2 px hop with a
         *   long hang, and the landing search re-seats him at step 11,
         *   INSIDE the state, long before its list ends. With
         *   `grounded` back the y pass does not run, so the authored G_y
         *   is not a leash.
         * - rows close on the EVEN steps of the state: 6, 8, 10, ..., 22,
         *   the 9th close (step 22) wraps the list and arms
         *   AC_BREAK_NEXT, and the exit matches at the top of step 23 -
         *   18 iterations = 900 ms for the whole authored reaction.
         *   `A_DRAP(1)` = the hub's row 1 (index 0) and it stays: the
         *   state change zeroes row_t and that entry iteration's feed of
         *   100 is below its 200. This fixture's A_DRAP has no T=, so the
         *   hit's remaining -192 is taken by NOTHING and keeps coasting
         *   through it at 1 px an iteration (the dragless-hub behaviour
         *   PR-83 recorded, in the engine's own unit). */
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
        /* THE PULL AND THE FIRST PASS, in the register's own unit. The
         * entry row's `I -320,-250` SETS both axes from rest (0x54170),
         * and the state's own pass runs in that same iteration: x takes
         * the T_x=3 drag idiv(3*-320,100) = -9 of it, so the register is
         * -320 + 9 = -311; y takes the drag idiv(3*-250,100) = -7 AND the
         * authored G_y=55 word, giving -195 + 7 = -188. The step pair is
         * idiv of each by 100: x moves -3 px, y -1 px, and the -250 lift
         * is what cleared `grounded` (PR-83's rule, unchanged). */
        CHECK(ll_play_v_raw(u.vx) == -311 && ll_play_v_raw(u.vy) == -188);
        CHECK(u.x == 497.0 && u.y == 99.0 && !u.grounded);
        double ux0 = u.x;
        int rec_at = 0, badlaw = 0, land_at = 0;
        long long hx = -311;              /* the leg's own copy of the
                                           * authored T_x=3 drag: */
        long long hy = -188;              /* v - idiv(3*v,100)          */
        double yd = 99.0;                 /* and of the y pass below it,
                                           * with the authored G_y=55 word
                                           * added first and the position
                                           * stepped by idiv(vy,100) px */
        static const int close_at[] = { 6, 8, 10, 12, 14, 16, 18, 20, 22 };
        for (int t = 6; t <= 31; t++) {               /* rows are two feeds  */
            ll_player_step(&u, &hp, &uk, 0, 2000, LL_ITER_MS); /* each, so every even step */
            if (!land_at) {
                hy = hy + 55 - hy * 3 / 100;
                yd += (double)(hy / 100);
                if (hy >= 0 && yd >= 99.0) {
                    /* the landing test of 0x54…: the register is not rising
                     * and the contact line is within a pixel of the .plt
                     * line at y=100. It fires HERE at t=11, one iteration
                     * before the arithmetic would take him down to 100 -
                     * the search's 1 px tolerance, not the physics. */
                    land_at = t;
                    if (!(u.grounded && u.y == 100.0 && u.vy == 0.0))
                        badlaw = 1;
                } else if (u.y != yd) {
                    badlaw = 1;             /* the HOP TABLE: yd walks
                                             * 98 at t=6 and then stands at
                                             * 98 for four iterations, the
                                             * register climbing -128, -70,
                                             * -13, +42, +96 through the
                                             * idiv(v,100) = 0 band - the
                                             * whole hop is 2 px and the
                                             * truncation holds him at the
                                             * top of it */
                }
            }
            hx = hx - hx * 3 / 100;                   /* the x pass, which
                                                       * runs in both states */
            if (u.sti == 1 && ll_play_v_raw(u.vx) != hx)
                badlaw = 1;
            if (u.sti == 2) {                 /* this fixture's A_DRAP authors
                                               * NO T at all, so nothing is
                                               * taken from the register any
                                               * more: the hit's remaining
                                               * -192 coasts through the hub
                                               * at idiv(-192,100) = -1 px an
                                               * iteration (PR-83's
                                               * dragless-hub behaviour, now
                                               * in the engine's own unit) */
                if (ll_play_v_raw(u.vx) != -192)
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
        CHECK(rec_at == 23 && land_at == 11 && !badlaw);  /* AC_BREAK_NEXT at
                                                           * the wrap, after an
                                                           * 18-iteration list */
        /* THE PULLBACK, from the walk above: the x register decays by 3% of
         * itself an iteration (idiv, so the step of the decay is what it is:
         * -9, -9, -8, -8, ...) and the STEP is idiv(register,100) whole px:
         *   -3 (the entry iteration), then -2 for fourteen iterations while
         *   the register is past 200, then -1 for the eleven that follow,
         *   through the state change into the hub. Total -45 px, of which
         *   -42 after the entry step that ux0 is taken after. The old float
         *   model summed the geometric series to -57.21 for the same words:
         *   the difference is the whole-px truncation, the same effect that
         *   makes the worm gait 25% slow above. Named at OI-015. */
        CHECK(u.x == ux0 - 42.0);
        ll_plt_free(&hp);
        free(hm);
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
         * first step to t=15: every iteration qualifies, so the
         * row spans are the PR-108 ones -
         *   t=1  enter at index 2, rem 100 < 200, nothing closes
         *   t=2  rem 200  -> index 3      t=4 -> 4   t=6 -> 5
         *   t=8  -> 6                    t=10 -> 7  t=12 -> 8
         *   t=14 -> 9 (the last 200-row closes, the 300-row opens)
         *   t=15 rem 100 against the 300-row, still index 9
         * - and the climb MOVES: PR-259's half of this leg. The
         * register add is `idiv([ebx+0x4] * [obj+0xbe], 20)`,
         * 0x542cd-0x542e3: the second word of the authored F pair
         * times the vertical intent - the param field PR-148 left
         * unread. The x side of the same body (0x542ab) multiplies
         * [ebx+0x0] by the horizontal intent word [obj+0xba], the
         * law PR-132 shipped as `idiv(F*s,20)`, so [ebx+0x0] is F_x
         * and [ebx+0x4] is F_Y - the pair this fixture's header
         * writes, `F=0,100`. With the port's one validated intent
         * scale (LL_PLAY_INTENT_FULL = 10) the add is
         * idiv(100 * -10, 20) = -50 hundredths an iteration, against
         * the authored y drag T_y = 50 percent of the OLD register
         * (0x54292-0x542a7, applied at 0x54311). Hand-run on the
         * integer register with toward-zero idiv, the exact order of
         * ll_play_v_step: t=1 -50, t=2 -75, t=3 -88, t=4 -94,
         * t=5 -97, t=6 -99, t=7 onward -100 - the equilibrium of a
         * 50 add against a 50% drag (100 = 2*50). The position takes
         * the TRUNCATED register (PR-256), so those six sub-pixel
         * iterations move NOTHING (no carry, no rounding: the part of
         * a px is dropped) and from t=7 the body rises one px per
         * iteration: 499, 498, ... 491 at t=15.
         * LEG 3, release mid-row: t=16,17 hold NO key. The row
         * clock freezes at index 9 - no feed - but the carried
         * accumulator (100 of the 300-row) survives untouched,
         * because the guard feeds nothing while the row compare
         * still runs, the engine's own order (feed 0x52317, THEN
         * compare 0x52320). No wrap at t=17 either: a mutation
         * that deletes the guard wrapped HERE (the PR-108 spans),
         * and one that zeroes the accumulator on a non-qualifying
         * iteration loses the carry and wraps at t=20, not t=19.
         * The body keeps its momentum and the drag keeps working with
         * no key held: the guard this PR widens opens on the STATE's
         * authoring, not on the key, so the same 50% drag runs
         * undisturbed - -100 -> -50 -> -25 - and both are under a
         * whole px, so y stays at 491 while the register decays.
         * That decay is what stops a released climb; an
         * implementation that gates the pass on the intent instead of
         * the state would freeze the register at -100 here and the
         * body would rise forever.
         *   t=18 Down held: the clock restarts ON THE CARRIED 100
         *   (the engine's [obj+0xa1] does the same): 100 + 100 =
         *   200 < 300, the row does NOT close. The register turns
         *   around, drag first: idiv(-25*50,100) = -12, the add is
         *   +50, (-25 + 50) - (-12) = 37.
         *   t=19 still Down: 200 + 100 = 300 -> the list WRAPS to
         *   index 0 and arms AC_BREAK_NEXT (readable only on the
         *   step that armed it, `broke` clears at iteration head);
         *   the register is (37 + 50) - idiv(37*50,100) = 87 - 18 =
         *   69.
         *   t=20 still Down: index 0's 300-row takes a fresh 100,
         *   no close; the register is (69 + 50) - idiv(69*50,100) =
         *   119 - 34 = 85. A model without the drag would sit at
         *   25, 75, 125 through those three steps: the numbers that
         *   make this half of the leg pin 0x54292-0x542a7 and not
         *   only the add. All three steps are sub-pixel, so y holds
         *   at 491 through the whole turn-around.
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
            /* PR-259: the climb MOVES. The register is under one whole
             * pixel for the six ramp iterations - and the position takes
             * the TRUNCATED register (PR-256), so y is untouched until
             * t=7 and then falls exactly one px per iteration. */
            if (u.y != (t <= 6 ? 500.0 : 500.0 - (double)(t - 6)))
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
        CHECK(u.y == 491);                        /* 500 - (15 - 6)     */
        CHECK(u.x == 500 && u.vx == 0);           /* no x force: F_x = 0 */
        CHECK(ll_play_v_raw(u.vy) == -100);       /* the drag equilibrium */
        rk.k_up = 0;
        for (int t = 16; t <= 17; t++) {
            u.sig |= 1ull << LL_SIG_AC_DRAB;
            ll_player_step(&u, NULL, &rk, 0, 2000, LL_ITER_MS);
            CHECK(u.sti == 1 && u.fi == 9 && u.row_t == 100);
            /* the drag runs with no key: -100 -> -50 -> -25, and
             * neither is a whole px, so y holds at 491 */
            CHECK(u.y == 491);
            CHECK(ll_play_v_raw(u.vy) == (t == 16 ? -50 : -25));
            CHECK(u.broke == 0);        /* NO wrap at t=17: the guard     */
        }
        rk.k_down = 1;
        u.sig |= 1ull << LL_SIG_AC_DRAB;
        ll_player_step(&u, NULL, &rk, 0, 2000, LL_ITER_MS);       /* t=18 */
        CHECK(u.sti == 1 && u.fi == 9 && u.row_t == 200);
        CHECK(ll_play_v_raw(u.vy) == 37); /* (-25+50) - idiv(-25*50,100)*/
        u.sig |= 1ull << LL_SIG_AC_DRAB;
        ll_player_step(&u, NULL, &rk, 0, 2000, LL_ITER_MS);       /* t=19 */
        CHECK(u.sti == 1 && u.fi == 0 && u.broke == 1);
        CHECK(u.row_t == 0);            /* the wrap: index 0, BREAK_NEXT  */
        CHECK(ll_play_v_raw(u.vy) == 69);  /* (37+50) - 18              */
        u.sig |= 1ull << LL_SIG_AC_DRAB;
        ll_player_step(&u, NULL, &rk, 0, 2000, LL_ITER_MS);       /* t=20 */
        CHECK(u.sti == 1 && u.fi == 0 && u.row_t == 100);
        CHECK(u.y == 491);              /* Down qualifies the clock like  */
                                        /* Up: the guard is sign-blind,   */
        CHECK(ll_play_v_raw(u.vy) == 85); /* (69+50) - 34                */
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
         * A_PUSH is the control half: BY_X, F=1000,0, T=40,0. Under the
         * engine law a held direction key is the full intent s=10, so
         * each iteration drags T/100 = 40% off the speed and then adds
         * F*s/100 = +100 u/s:
         *   v1 = 100, v2 = 100*0.6 + 100 = 160, -> F*s/T = 10000/40
         *   = 250 u/s, i.e. v_n = 250*(1 - 0.6^n) exactly.
         * The distance those two iterations travel (100*0.05 + 160*0.05
         * = 13 units) feeds `facc` ONLY: one row of 1000 units stays
         * unclosed and row_t/row_rem stay exactly 0 - the mode-0 branch
         * replaces the wall clock.
         * The K_GEN1 pulse then moves him into A_COAST: BY_X with
         * F=0,0 and NO T= at all, the shape of 29 of the 30. Both
         * terms of the gate are now false, and two things follow:
         * - the velocity is FROZEN at the entry value, 160 u/s: the law
         *   has neither term to change it - no F= word means no force
         *   and no T= word means no drag (the same reading PR-83
         *   recorded for ZARIA's own A_TURN, and the reason the old
         *   "target velocity 0" vs "auto_walk" disagreement this leg
         *   kept on purpose no longer exists). It does not brake on
         *   release either, for the same reason.
         * - the rows run on the ITERATION clock instead: section 7
         *   feeds 100 per step, so six rows of 100 close in six steps,
         *   one per step, the sixth closing index 5 of 5 and wrapping
         *   (fi back to 0, `broke` armed by the wrap alone, which is
         *   why the loop below must end on that step). `facc` starts at
         *   0 with the new state and stays 0.0 the whole way.
         * MUT-1 (delete `st->fx != 0`, i.e. key the feed on the mode
         * as the dispatcher does): A_COAST becomes a walking state,
         * its wall clock never runs and 6 steps travel 48 against a
         * 100-unit row, so the leg fails at its first step - and no
         * other leg changes, which IS the coverage claim. MUT-2
         * (delete the MODE half instead): this leg stands, PR-110's
         * BY_TIME legs fail. The mode half was covered; the fx half
         * was not. */
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
            double want = t == 1 ? 100.0 : 160.0; /* v_n = 250*(1-0.6^n)  */
            ll_player_step(&bx, NULL, &bk, 0, 2000, LL_ITER_MS);
            if (bx.sti != 0 || bx.fi != 0 ||
                bx.vx < want - 1e-9 || bx.vx > want + 1e-9 ||
                bx.row_t != 0u || bx.row_rem != 0u)
                bad = 1;
        }
        CHECK(!bad);
        CHECK(bx.facc > 12.99 && bx.facc < 13.01);    /* fed, never enough  */
        CHECK(bx.x > 500 + 12.99 && bx.x < 500 + 13.01);  /* 100*.05 + 160*.05 */
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
            if (bx.vx != 160.0)
                bad = 1;                            /* frozen: no F= term, and
                                                     * no T= term to change it */
            if (bx.broke != (s == 6))
                bad = 1;                           /* the wrap armed it */
            double want_x = bxc + s * 8.0;         /* 160 u/s * 0.05 s  */
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
            if (bx.vx != 160.0 || bx.fi != s || bx.facc != 0.0)
                bad = 1;
        }
        CHECK(!bad);
        free(bxm);
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
               " and a state that authors no T= takes nothing away at all"
               " (ok)\n");

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

    return rc;
}

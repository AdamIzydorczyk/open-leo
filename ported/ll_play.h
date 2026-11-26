/* ll_play.h - playable-level core (M4-B, PR-43): .plt ground polylines,
 * the extended .MOV state-machine parser (physics + transitions), and the
 * player physics runtime driven by real LEW.MOV data.
 *
 * Pure CPU, no SDL, no .SEQ decoding, no container access: ll_level.c owns
 * loading and drawing; this module owns the platform grammar and the
 * simulation, so every leg of it is hermetic and CI-testable.
 *
 * Data provenance (verified 2026-11-16 on LL.iso + the HD-install tree):
 *   <level>.plt   plain text (HD ETAPY block): a polyline count line, then
 *                 "n x,y;x,y;..." world-coordinate ground lines (RAN.01:
 *                 RAN.PLT 153 lines, TUNGA.11: TUNGA.PLT 7 lines). Point
 *                 separators: ';' normally, '!' occurs as an EXACT
 *                 equivalent inside the same polyline (ENA.PLT
 *                 "1 1,560!7500,570;", ZARIA_A.PLT 10 times) - treated
 *                 identically, the leading count still counts SEGMENTS.
 *   LEW.MOV       plain text (HD ANIMACJE player leaf, 8666 B in RAN.01):
 *                 the lion's real state machine - 30 states; each block
 *                 adds F=/T=/G= physics (acceleration px/s^2, speed cap
 *                 px/s, gravity px/s^2 - walk states F=400 T=90, grounded
 *                 states G=0 + CHECK_LN, the real fall state A_START_LAD1
 *                 G=0,200), "TARGET(p)=COND" transitions (K_FWD/K_BKWD/
 *                 K_UP/K_DOWN/K_GEN1..3/K_ANY/K_JUMP/AC_TIME/AC_BREAK_NEXT,
 *                 "!" negates,
 *                 "|" ORs; the (..) token is the target's START ROW
 *                 1-based when numeric (PR-86, see ll_mov_tr.entry),
 *                 non-numeric - real KADMAR2 writes "A_MASZERUJ(x)=K_FWD" -
 *                 means the top; and a transition
 *                 targeting its OWN state means KEEP running, no frame
 *                 reset and no event) and per-state "seq=<stem>" art
 *                 selection. PR-85 MODEL CHOICE (recorded, see cond_true):
 *                 K_FWD/K_BKWD are FACING-RELATIVE (K_FWD = the arrow in
 *                 the facing direction) - the only reading under which
 *                 ZARIA-style data (one A_WALK, no backward-walk state)
 *                 can walk left at all. Condition names the data uses
 *                 but this runtime does not model (K_FIRE, K_ALT,
 *                 K_F1..F12, the engine-internal AC_* signals) evaluate
 *                 FALSE - nothing is guessed.
 * Physics constants therefore come from the data; the single project-owned
 * constant is LL_PLAY_GRAVITY (the authored G=0,200 of RAN.01's real
 * A_START_LAD1, in the engine's own unit for that add: the per-iteration
 * increment of the velocity register, see LL_PLAY_RAW_GMUL) used only when
 * a state marked CHECK_LN loses its ground without an authored fall state
 * to route into (documented simplification: RAN.01's authored chain
 * A_START_LAD1 -> A_LADOW is an 18.4 s scripted waterfall landing, not a
 * curb step; see NEXT_STEPS.md PR-43).
 */
#ifndef LL_PLAY_H
#define LL_PLAY_H

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ .plt
 * Ground polylines in world coordinates (x right, y DOWN, feet on top).
 * Parse allocates; free with ll_plt_free. */
#define LL_PLT_MAXPT 128                    /* points kept per polyline    */
typedef struct { int x, y; } ll_xy;
typedef struct { ll_xy *p; int n; } ll_poly;
typedef struct { ll_poly *line; int n; } ll_plt;

/* text: first non-empty line = polyline count, rest = "n x,y;x,y;..." with
 * ';' or '!' point separators (equivalents in the real files); the leading
 * per-line number counts SEGMENTS, so every coordinate pair is read.
 * 0 = parsed (garbage lines are skipped; a header that DECLARES 0
 * polylines is a complete file that simply has no ground - JUNCTION's
 * platformy=none.des is literally "0"), 1 = nothing usable. */
int  ll_plt_parse(const char *text, ll_plt *out);
void ll_plt_free(ll_plt *o);

/* Ground CONTINUATION while walking: nearest segment covering x whose y is
 * within tol of want_y (slope/step following). 0 = none (stepped off an
 * edge). hit.y is the interpolated surface y. */
int  ll_plt_surface_at(const ll_plt *o, int x, int want_y, int tol,
                       ll_xy *hit);
/* FALLING: first surface at or below from_y (highest candidate reached
 * first while falling). 0 = none (continue falling). */
int  ll_plt_first_below(const ll_plt *o, int x, int from_y, ll_xy *hit);

/* ------------------------------------------------------------------ .blk
 * The `blokady=` registry the physics step's blockade query walks
 * (0x4b430 over [world+0x70], count [world+0x7c], stride 0x14; PR-313
 * named the registry, PR-394 read the query, PR-395 read the record).
 * One record is FIVE dwords and the loader parses the file's three
 * numbers into +0x4 (the wall's x), +0x8 (its top y) and +0x10 (how
 * far down it goes): 0x455e0 runs the engine's scanner 0x49920 three
 * times over one token cursor, with the formats "%d,%d" (0x91d0f, into
 * +0x4 and +0x8, through 0x4a0b0), "%d" (0x9183d, into +0x10) and the
 * bare "," (0x91840, a separator probe). +0x0 and +0xc never come from
 * the file: the record's CONSTRUCTOR 0x4fb70 - the first field of the
 * class record 0xa14c0, which keeps its fields at C+1/C+5/C+9/C+0xd -
 * zeroes +0x4, +0x8 and +0x10 and stores the method table 0xa2178 at
 * +0x0 and the shared class table 0x98278 at +0xc (PR-396 read
 * this; it
 * corrects what PR-395 said about calloc). The allocation 0x76a64 is
 * the engine's plain malloc chain (0x5c320 -> 0x7521f -> 0x78ce0) and
 * 0x76bb7 -> 0x76b24 writes the count at [mem], runs that constructor
 * over count records, and returns mem+4 - which is what the registry
 * stores at [world+0x70]. A record is therefore a
 * VERTICAL wall at x, from y down to y+ly - and the ten `.BLK` members
 * of the HD ETAPY read exactly that way: headers 2, 2, 2, 3, 4, 6, 6,
 * 9, 11 and 49 records (94 in all), every file carrying exactly
 * 1+3*count integers, with the walls spanning each whole world (the
 * minimum authored x is -1..3, the maximum the level's right edge;
 * census: /tmp/opencode/pr395_census.txt). */
typedef struct { int x, y, ly; } ll_blk_rec;
typedef struct { ll_blk_rec *r; int n; } ll_blkreg;

#define LL_BLK_MAX 64          /* records one file may own; the measured
                                * maximum across the shipped set is 49 */

/* 0x4db50's read: ONE read of up to 0x400 bytes into the scanner object
 * (0x49820) and then a run of scans over that same object, so the file
 * is a stream of numbers and not a set of lines - the header count
 * first (0x4db8e, format "%d" at 0x91f24), then count triples. A
 * record whose numbers are missing keeps the zero its constructor
 * wrote: the loader reports nothing about a short file (0x4dbcf-0x4dbe4
 * runs count times whatever happens) and ignores anything past the
 * count. 0 = a count was read; 1 = the first token is not a number, or
 * a count the port will not hold. */
int ll_blkreg_parse(const char *text, ll_blkreg *out);
void ll_blkreg_free(ll_blkreg *o);

/* The blockade test 0x45620 for one record. bx0..by1 are the
 * requester's four request-block fields as 0x4b430 leaves them at
 * 0x4b462-0x4b490 - [0] and [4] are the body's own x and y through the
 * +0x68 getter, [8] and [0xc] its far corners, and all FOUR carry the
 * pending clamp delta ([obj+0xca] added to both x fields at
 * 0x4b479/0x4b47b, [obj+0xce] to both y fields at 0x4b47d/0x4b48a). So
 * the box here is where the body WOULD be after the move it has not
 * applied yet. Returns 1 with *dx set to what 0x45688-0x45690 ADD to
 * [obj+0xca], or 0 with *dx at 0 (the engine never writes the field on
 * a miss). A record answers when the wall's x lies between the body's
 * two x (0x45629/0x45631, both ends inclusive) AND the wall's y range
 * y..y+ly OVERLAPS the body's two y (0x4563d-0x45649 and
 * 0x4564b-0x4565a - it is an overlap test, not a point test). The body
 * is then moved to the side of the wall its NEARER edge is on, so that
 * that edge lands at wall.x -/+ 1; a distance of 0 or 1 means the body
 * is already flush and is not a hit (0x4567e-0x45686). A blockade
 * therefore TELESCOPES a body out of a wall through the [obj+0xca] the
 * position carry at 0x54825 applies - it never stops it. */
int ll_block_hit(const ll_blk_rec *rec, int bx0, int bx1, int by0, int by1,
                 int *dx);

/* The walk 0x4b4a1-0x4b4bc over the registry, and what it does with the
 * answer: it asks the records in FILE ORDER and 0x4b4b1 leaves at the
 * FIRST one that answers - it does not add several pushes. The caller
 * then raises AC_BLOK once (0x4b4ec `or BYTE [result],0x1`). Returns 1
 * with *dx set and *idx the record that answered, or 0 with *dx at 0
 * and *idx -1. */
int ll_block_walk(const ll_blkreg *reg, int bx0, int bx1, int by0, int by1,
                  int *dx, int *idx);

/* ------------------------------------------------------------------ .suf
 * The `sufity=` CEILING registry - the query the SAME physics step runs
 * after the blockades (0x546f0 and 0x54732 are the blockade walks,
 * 0x5474b is this one, with the same eax = the world [0xcef40] and the
 * same ebx = the body). 0x4da90 builds [world+0x78], count
 * [world+0x84], stride 0x14; the walk is 0x4b7f0 and the test is
 * 0x45500. Its record class is 0xa14d8, whose constructor 0x4fbd0
 * zeroes rec+0x0, rec+0x4 and rec+0xc, puts the shared class table
 * 0x98278 at
 * rec+0x8 and the method table 0xa218c at rec+0x10; the loader then
 * calls [[rec+0x10]] - 0x454d0 - once per record over the same token
 * cursor, and 0x454d0 parses the file's three numbers: "%d,%d , "
 * (0x91d0f, through 0x4a0b0) into rec+0x0 and rec+0x4, then "%d"
 * (0x91836) into rec+0xc, then the probe " , " (0x91839). A ceiling
 * record is therefore a HORIZONTAL line: it starts at x, runs lx px
 * along +x, and sits at y. The shipped registries read exactly that
 * way: the three `.SUF` members (KADMAR.07 `0,0,3000`, RAN.01
 * `20,100,28800`, KARA.07 `0,0,1600`, `125,290,10`, `1535,40,10`) and
 * the three `SUFITY.DES` sub-descriptors (ENA.03 `0,20,6720`,
 * TUNGA2.12 `0,613,358` and three more, SADES.06 six rows) - which is
 * what the `sufity=` lines of the level descriptors name. The other
 * seven name a `NONE.DES` sub-descriptor holding the single token `0`:
 * an authored EMPTY registry, not a missing file (census:
 * /tmp/opencode/pr396_suf_des.txt). */
#define LL_SUF_MAX 8           /* records one file may own; the measured
                                * maximum across the shipped set is 6 */
typedef struct { int x, y, lx; } ll_suf_rec;
typedef struct { ll_suf_rec *r; int n; } ll_sufreg;

/* 0x4da90's read, one 0x400-byte read into the scanner object
 * (0x4dabb, the same scanset 0x961fc as `.BLK`) and a run of scans over
 * it: the header count first (0x4dac0, format "%d" at 0x91f21) into
 * [world+0x84], then the record scan EXACTLY count times
 * (0x4db12-0x4db2e). A count of 0 allocates nothing and leaves
 * [world+0x78] at 0 (0x4dadf); a short file leaves the rest of the
 * records at their constructed zero. 0 = a count was read; 1 = the
 * first token is not a number, or a count the port will not hold. */
int ll_sufreg_parse(const char *text, ll_sufreg *out);
void ll_sufreg_free(ll_sufreg *o);

/* The ceiling test 0x45500 for one record. Unlike 0x45620 it is given
 * the BODY, not a box: it asks the body's class slot +0x68 and keeps
 * only [4] of what comes back (the `top` argument, to which the engine
 * adds NO delta, 0x45566), and asks slot +0xb0 for two numbers and then
 * adds [obj+0xca] and [obj+0xce] to them (0x4553c-0x4554f) - the
 * `ax`,`ay` here are those two sums, i.e. where the body's ANCHOR would
 * be once the pending delta is applied. Both getters are the ones
 * PR-305..PR-308 read: +0x68 is 0x23900 = `add eax,0xd6`, so the box is
 * the point object at [obj+0xd6]/[obj+0xda] - the STORED presented
 * point, refreshed by the class's +0x1c verb 0x54e00, not every step -
 * and +0xb0 is 0x544f0, which copies the LIVE [obj+0]/[obj+4] and then
 * an attachment path for a body flagged at [obj+0xb6]. Those are two
 * different positions, which is why this function takes all three values
 * as parameters instead of choosing one. A record answers when all four
 * hold: ax in [rec.x, rec.x + rec.lx] inclusive at both ends (0x45555,
 * 0x45562); the body's top is
 * at or BELOW the line, top >= rec.y (0x4556c `cmp [esi+4], rec.y /
 * jl`); and the anchor is strictly ABOVE it, rec.y - ay >= 1 (0x45576
 * `sub`, 0x45578 `test / jle`). Then 0x45582-0x45584 ADDs rec.y - ay
 * to [obj+0xce], which lands the anchor exactly ON the line; *dy is
 * that amount. A ceiling therefore pulls a body down to the line it is
 * sticking above - it never pushes it up and never stops it. Returns 1
 * with *dy set, or 0 with *dy at 0 (the engine never writes the field
 * on a miss). */
int ll_ceiling_hit(const ll_suf_rec *rec, int ax, int ay, int top, int *dy);

/* The walk 0x4b806-0x4b818: it asks the records in FILE ORDER, the
 * count is [world+0x84] (re-read every step, 0x4b80d), and 0x4b82d
 * `test eax,eax / je` moves on while a record refuses - the FIRST one
 * that answers ends the walk (0x4b82f). The caller's result block is
 * then cleared (0x4b83b-0x4b83f, 0x749a0 with the 0xc size, exactly as
 * the blockade walk's miss path) and 0x4b844 raises the ceiling flag,
 * `or BYTE [result+3],0x2` - a different byte and bit from AC_BLOK's
 * `or BYTE [result],0x1`. Returns 1 with *dy and *idx set, or 0 with
 * *dy at 0 and *idx -1. */
int ll_ceiling_walk(const ll_sufreg *reg, int ax, int ay, int top,
                    int *dy, int *idx);

/* ------------------------------------------------- .MOV state machine ---
 * EXTENDED version of the viewer's mov_t (ll_level.c PR-38): same frame
 * semantics (a frame number is 1-based, its sign the mirror flag, see
 * ll_mov_frame_ref below;
 * non-positive ms clamped to 100 - the M4-C rule), plus the physics /
 * transition grammar above. Caps are sized from the shipped data, not
 * guessed (measured over all 777 .MOV, PLAN 8g): 40 states in one file,
 * 97 frames in one state, 30 transitions in one state, 4 condition parts
 * in one expression. ll_mov_parse counts anything it had to drop. */
#define LMOV_MAXST 64
#define LMOV_MAXFR 128
#define LMOV_MAXTR 32
#define LMOV_MAXCON 4                       /* parts per condition expr    */

/* control= flags the model acts on. The real vocabulary is a | separated
 * (rarely , separated) list; measured use counts (PLAN 8g):
 *   BY_TIME 2101  FRAME_REV 798  CH_DIR 552  CHECK_LEW 535  DANGER 385
 *   CHECK_ALL 356  CHECK_LN 340  CHECK_ANI 168  SOLID2 152  SOLID 141
 *   ANTI_POCISK 121  BY_X 105  SEMI_SOLID 92  ANTI_SOLID 87
 *   CHECK_ALL_PLANE 62  CHECK_ZW 47  SEQ_REV_Y 22  BY_Y 13  BY_TY 12
 *   CHECK_BASE_LN 8  SEQ_REV_X 7  INV_V 3
 * `BY_TY` and `BY_Y` are NOT typos/aliases in the engine, whatever this
 * comment used to say (PR-102): the mode field at bits 0..2 of the
 * state's control word has five values - BY_X 0 (sets nothing), BY_Y 1,
 * BY_TIME 2, BY_TX 3, BY_TY 4 - and the dispatcher's jump table
 * (0x54380) gives mode 1 its own feed of |v_y| (0x543dc: `mov
 * eax,[obj+0xce]` -> abs -> `add [obj+0xa1],eax`), while mode 4 is a
 * separate body (0x5441b) guarded by `[obj+0xbe] != 0` that feeds the
 * same level constant 100 as mode 3 (0x543f2, guarded by `[obj+0xba] !=
 * 0`). Mode 2, BY_TIME, is 0x543fb and carries NO guard at all: PR-102
 * wrote that modes 2/3 are both gated by `[obj+0xba]`, and PR-108
 * corrects that here and in HANDOFF 7.13 / 8. BY_TX appears in 0 shipped
 * files; BY_TY is 12 blocks in 7 files and only three state names -
 * A_WSP, A_WSP_LIN, A_DRAB - all of them the lion on a RUNG LADDER (the
 * DRABINKA object, `seq=climb`/`climb_r`), which fits the vertical
 * reading. ENA2's rope state A_CLIMB is mode 1 instead: a |v_y| feed
 * with thresholds of 4, i.e. velocity-coupled animation, a different
 * mechanism (PR-109). WHAT THE 100 IS PER IS NOW MEASURED, not
 * guessed: one frame-loop iteration, whose period the pacer (0x4b0e0 ->
 * 0x4a120 -> 0x1dba0) holds to TWO display frames - so an authored row
 * threshold of 200 is two iterations, measured 66-100 ms of screen time
 * on the owner's ladder clip, NOT 200 ms. That makes the `.ms` field of
 * ll_mov_fr a misnomer (renamed only with PR-110, the change) and the
 * row clocks of ll_play.c/ll_level.c 3.0-3.5x too slow in wall time.
 * MODE 4 (BY_TY) IS MODELLED FROM PR-148: ll_play.c section 7 guards
 * its feed on the player's vertical intent, exactly as 0x5441b guards
 * on [obj+0xbe] (PR-147's byte-exact reading; its 12/0 BY_TY/BY_TX
 * byte census over the authored archive confirms the counts above).
 * Mode 1 (BY_Y) has no LL_CTRL bit acting on its |v_y| feed, and
 * PR-108's play selftest leg pinned the ungated ms model this port
 * ran until PR-148 re-pinned it. Names without a bit below are
 * parsed, named here for the
 * record, and counted in ctrl_other - never silently dropped, never
 * invented into a behaviour. */
enum {
    LL_CTRL_BY_TIME         = 1u,
    LL_CTRL_BY_X            = 2u,           /* walk states: move at speed  */
    LL_CTRL_CHECK_LN        = 4u,           /* keep-ground / fall-off rule */
    LL_CTRL_CH_DIR          = 8u,
    LL_CTRL_CHECK_ALL       = 0x10u,
    LL_CTRL_CHECK_ALL_PLANE = 0x20u,
    LL_CTRL_FRAME_REV       = 0x40u,
    LL_CTRL_BY_Y            = 0x80u,
    LL_CTRL_SOLID           = 0x100u,
    /* PR-59: the game's own flag word (token parser 0x51b30..0x51dc3, flag
     * word at +0x12 of the state): SOLID = 0x400, SOLID2 = 0x20400,
     * SEMI_SOLID = 0x40400 (both include SOLID's bit), ANTI_SOLID = 0x10000,
     * ANTI_POCISK = 0x80000. FUN_00053ae0 tests exactly these. */
    LL_CTRL_SOLID2          = 0x200u,       /* also sets LL_CTRL_SOLID    */
    LL_CTRL_SEMI_SOLID      = 0x400u,       /* also sets LL_CTRL_SOLID    */
    LL_CTRL_ANTI_SOLID      = 0x800u,
    LL_CTRL_ANTI_POCISK     = 0x1000u,
    /* PR-60: the mover's own list selectors (FUN_00052780 0x527b3..0x5282f):
     * CHECK_ANI (0x40) = test against the animacje= objects, CHECK_ZW (0x80)
     * = the zwierzaki= objects, CHECK_LEW (0x200) = against Lew; with
     * CHECK_ALL_PLANE the plane gate is dropped. CHECK_ALL = ANI|ZW|LN in the game (0x1c0);
     * PR-231 makes the port match it exactly - CHECK_ALL also sets
     * LL_CTRL_CHECK_LN (PR-60's "LN physics is unchanged" exemption
     * predates the LN rules being modelled; ZARIA.01's [A_WALK]
     * relies on the implication). */
    LL_CTRL_CHECK_ANI       = 0x2000u,
    LL_CTRL_CHECK_ZW        = 0x4000u,
    LL_CTRL_CHECK_LEW       = 0x8000u,
    /* PR-90 (T3): DANGER. The game's own flag word carries it as 0x2000:
     * the touch path tests exactly that bit of the state word at
     * 0x5528b/0x51019 (`test ah,0x20`) and that branch then reads the
     * level's danger_power at [level+0x54]. The authored data carries it
     * on every enemy walk state, and the port uses it as the attack
     * arming PRECONDITION - a model: the engine's per-class AI is
     * compiled and reads no control flag (NEXT_STEPS PR-90 record,
     * HANDOFF §7 item 10). Here it is an internal bit again (the
     * enum's values never are the wire values). */
    LL_CTRL_DANGER          = 0x10000u,
    /* PR-148: BY_TY, mode 4 of the feed dispatcher. Its branch at
     * 0x5441b (byte-exact, PR-147) feeds the level constant 100 to
     * the row accumulator ONLY while [obj+0xbe], the player's own
     * vertical intent word, is non-zero; that word is the keyboard's
     * y-axis (Up/Down at +/-20, reader 0x582f0; a kind-4 contact
     * node overrides it to +/-10 toward the partner's y, 0x54b16,
     * PR-149's route). ll_play.c section 7 implements the guard;
     * the enum's values never are the wire values (wire: BY_TY 0x5
     * in bits 0..2, PR-147's byte-exact keyword encoding). */
    LL_CTRL_BY_TY           = 0x20000u
};
#define LL_CTRL_NAMES \
    X(BY_TIME) X(BY_X) X(BY_Y) X(BY_TY) X(CHECK_LN) X(CHECK_LEW) \
    X(CHECK_ALL) X(CHECK_ALL_PLANE) X(CHECK_ANI) X(CHECK_ZW) X(CHECK_BASE_LN) \
    X(CH_DIR) X(DANGER) X(SOLID) X(SOLID2) X(SEMI_SOLID) X(ANTI_SOLID) \
    X(ANTI_POCISK) X(FRAME_REV) X(SEQ_REV_X) X(SEQ_REV_Y) X(INV_V)

/* transition condition ids (K_ANY = any held key; OTHER = recognized
 * token we do not simulate - never true, logged once by the caller).
 * The authored expressions separate ALTERNATIVES with '|' (49 lines
 * start with one) and CONJUNCTIONS with ',' (466 of 2911 lines carry
 * comma groups); a ';' inside a group (3 lines) stays one OTHER token.
 * PR-87 MODEL CHOICE, data-motivated: ',' = AND. Proof it cannot be a
 * no-op or OR: `A_WPAD(1)=AC_ONAIR,AC_DRECT` and `A_WSP(3)=
 * AC_DRAB_CATCH,AC_ONAIR` pair a catch trigger with the state of being
 * airborne (an OR there makes the pair meaningless), and every player
 * attack line is `K_FIRE,K_Fn` - fire WHILE weapon n is selected (the
 * game's F1..F4 weapon keys + Ctrl attack, per the walkthrough). */
enum {
    LL_COND_NONE = 0,
    LL_COND_K_FWD, LL_COND_K_BKWD, LL_COND_K_UP, LL_COND_K_DOWN,
    LL_COND_K_GEN1, LL_COND_K_GEN2, LL_COND_K_GEN3,
    LL_COND_K_ANY, LL_COND_AC_BREAK_NEXT,
    LL_COND_K_GEN4, LL_COND_AC_TIME,   /* PR-69: scene objects (not the lion) */
    LL_COND_K_JUMP,                    /* PR-74: the jump key               */
    /* PR-87: the fire family. K_FIRE = the attack button (the walkthrough
     * names Ctrl); K_F1..K_F4 are the WEAPON SELECTOR keys the comma
     * groups gate attacks with (K_F1 81, K_F2 38, K_F3 11, K_F4 6 uses
     * in the player .MOV files; K_F6..F12 also exist, stay OTHER);
     * K_ALT = the alternate-modifier condition (28 uses: A_FIGHT_END /
     * throw routes). */
    LL_COND_K_FIRE, LL_COND_K_F1, LL_COND_K_F2, LL_COND_K_F3,
    LL_COND_K_F4, LL_COND_K_ALT, LL_COND_K_GEN5,
    LL_COND_OTHER
};

/* PR-88: the engine's signal bit table. The object's signal bitset at
 * [obj+0x60] is a TEventSet (error string "TEventSet::match" at 0x919cd);
 * the name->bit function at 0x48000 compares a condition/GEN token against
 * the name block at 0x919ec..0x91c68 (PR-173: the block is 73
 * NUL names - AC_K_ANY/K_ANY/AC_K_NEVER/K_NEVER FIRST (the
 * node-op specials, NOT bits), then EXACTLY the 68 signals of
 * THIS enum, in this order, then the 'AC_EV_MAX' sentinel;
 * bit N = block index N+4, and the whole table plus this
 * port's token->enum table are verified 68/68 complete) and
 * sets an EXPLICIT bit constant per
 * name (verified constants: AC_BLOK mov al,0x0; AC_BLOK_Y 0x1;
 * AC_BREAK_NEXT 0x2; AC_ENEMY mov dl,0x7; AC_ONAIR mov dl,0x12;
 * K_GEN1 mov al,0x36 = 54; K_GEN2 0x37 - PR-70's "bit 54 = K_GEN1"
 * assumption CONFIRMED). K_ANY (and the AC_K_ANY alias) and K_NEVER /
 * AC_K_NEVER are NOT single bits: the resolver 0x48000 answers them with
 * WHOLE masks - 0x2001f8 in dword 1 and 0x60 in byte 4 (PR-263) - and
 * the clause parser at 0x47d60 compiles ONE mask plus ONE operation
 * byte per condition ('!' = disjoint, '*' = equal, '|' = intersects,
 * plain = subset; a ',' folds its name into the same mask). PR-87's
 * OR-of-AND-groups reading was superseded by that one-mask reading
 * (PR-261, ported by PR-264). AC_EV_MAX =
 * 68 bounds the table, so the node/set masks are 3 dwords. */
enum {
    LL_SIG_AC_BLOK = 0,     LL_SIG_AC_BLOK_Y,     LL_SIG_AC_BREAK_NEXT,
    LL_SIG_AC_BREAK_PREV,   LL_SIG_AC_DUP,        LL_SIG_AC_OFF,
    LL_SIG_AC_DEADDUP,      LL_SIG_AC_ENEMY,      LL_SIG_AC_EDGE,
    LL_SIG_AC_TIME,         LL_SIG_AC_DRAB,       LL_SIG_AC_DRABK,
    LL_SIG_AC_DRAB_CATCH,   LL_SIG_AC_DUP_CD,     LL_SIG_AC_STONE,
    LL_SIG_AC_PUSH,         LL_SIG_AC_ENEMY_ATC,  LL_SIG_AC_RECT,
    LL_SIG_AC_ONAIR,        LL_SIG_AC_DRECT,      LL_SIG_AC_DRECT2,
    LL_SIG_AC_LINA,         LL_SIG_AC_LINA_CATCH, LL_SIG_AC_LEAVE,
    LL_SIG_AC_ATAK,         LL_SIG_AC_UP_DUP,     LL_SIG_AC_SPOKUJ,
    LL_SIG_AC_BUTLA,        LL_SIG_AC_BECZKA,     LL_SIG_AC_START,
    LL_SIG_AC_MRECT,        LL_SIG_AC_TABLICZKA,  LL_SIG_AC_KILOF,
    LL_SIG_AC_KOREK,        LL_SIG_AC_KOLO,
    LL_SIG_K_BKWD = 35,     LL_SIG_K_FWD,         LL_SIG_K_UP,
    LL_SIG_K_DOWN,          LL_SIG_K_FIRE,        LL_SIG_K_ALT,
    LL_SIG_K_F1,            LL_SIG_K_F2,          LL_SIG_K_F3,
    LL_SIG_K_F4,            LL_SIG_K_F5,          LL_SIG_K_F6,
    LL_SIG_K_F7,            LL_SIG_K_F8,          LL_SIG_K_F9,
    LL_SIG_K_F10,           LL_SIG_K_F11,         LL_SIG_K_F12,
    LL_SIG_K_JUMP = 53,     LL_SIG_K_GEN1,        LL_SIG_K_GEN2,
    LL_SIG_K_GEN3,          LL_SIG_K_GEN4,        LL_SIG_K_GEN5,
    LL_SIG_AC_PLAT_OFF = 59, LL_SIG_AC_PLAT_OUT,  LL_SIG_AC_XCHG_LEVEL,
    LL_SIG_AC_SHOW_CROWN,   LL_SIG_AC_MSG_PUSH,   LL_SIG_AC_WALK_UP,
    LL_SIG_AC_WALK_DOWN,    LL_SIG_AC_APPEAR,     LL_SIG_AC_DISAPPEAR,
    LL_SIG_AC_EV_MAX = 68
};

/* Table index of a signal name (case-insensitive), or -1 for unknown
 * names and for the non-bit specials K_ANY / K_NEVER (+ AC_ aliases).
 * PR-217: ll_mov_parse stores that -1 as LL_SIG_NOBIT in a row's
 * gsig_id, so a reader can tell "no bit" from "no clause" (-1). */
#define LL_SIG_NOBIT (-2)
/* Which of the engine's non-bit clause operands a captured name is
 * (PR-218, measured against its name comparator 0x48000). LL_GOP_NAME is
 * the common case and is 0, so a zeroed frame means "a real signal name,
 * or no clause at all". */
#define LL_GOP_NAME  0
#define LL_GOP_NEVER 1
#define LL_GOP_ANY   2
#define LL_GOP_NONE  3
/* PR-263: the MASK the engine answers the name K_ANY with - its name
 * comparator 0x48000 clears the set and OR-s the constant 0x2001f8 into
 * dword 1 (0x48029-0x48048), which in this table is signals 35..40 and
 * 53: K_BKWD, K_FWD, K_UP, K_DOWN, K_FIRE, K_ALT, K_JUMP, the seven bits
 * the input bridge 0x54910 raises out of [obj+0xa5] bits 0/4/5,
 * [obj+0xb6] bit 4 and the intent words [obj+0xba]/[obj+0xbe]. The
 * weapon selectors K_F1..K_F4 (41..44) are NOT in it. Its sibling
 * K_NEVER is answered with 0x60 in the same dword = K_UP,K_DOWN
 * (0x4806f-0x4807d). Both are ordinary masks: what combines them with
 * the live set is the clause's operation byte, PR-261. */
#define LL_SIG_K_ANY_MASK ((1ull << LL_SIG_K_BKWD) | (1ull << LL_SIG_K_FWD) | \
                           (1ull << LL_SIG_K_UP) | (1ull << LL_SIG_K_DOWN) | \
                           (1ull << LL_SIG_K_FIRE) | (1ull << LL_SIG_K_ALT) | \
                           (1ull << LL_SIG_K_JUMP))
/* PR-263 (c): the same resolver's K_NEVER branch OR-s 0x60 into mask
 * BYTE 4 (0x4806f-0x4807d) = signals 37,38 = K_UP,K_DOWN. */
#define LL_SIG_K_NEVER_MASK ((1ull << LL_SIG_K_UP) | (1ull << LL_SIG_K_DOWN))
/* PR-264: the four operations a compiled condition is matched with. The
 * clause parser writes the byte (0x47d73 clears it; '!' at 0x47dc8, '*'
 * at 0x47dc2, '|' at 0x47dbc), and TEventSet::match 0x47e40 switches on
 * it through the table at 0x47e30: 0 is true when the clause mask is a
 * SUBSET of the set handed to it, 1 when the AND is EMPTY, 2 when the
 * two sets are byte-identical, 3 when the AND is NOT empty (PR-260,
 * PR-261). */
#define LL_TOP_SUBSET 0
#define LL_TOP_NOT    1
#define LL_TOP_EXACT  2
#define LL_TOP_ANY    3
int ll_sig_id(const char *name);

typedef struct { int frame; unsigned thresh; int dx, dy;
                 int no_scale;        /* PR-330: a row opened by the
                                       * engine's `s <seq>` field is
                                       * built with the CONSTANT 1000
                                       * divisor (0x51ab7 pushes 0x3e8),
                                       * while the ordinary row site
                                       * passes the state's [st+0xe]
                                       * time_scale (0x51f37-0x51f3e).
                                       * So those rows keep their
                                       * authored units: measured in
                                       * LONA.05's T_TELEPORT trio,
                                       * time_scale=600 and 37 such
                                       * rows. 1 = do not scale. */
                 unsigned long long gsig; unsigned char atc;
                 int has_i, ix, iy, endlv;
                 int has_f, fx, fy;           /* PR-253 row `F x,y` tail  */
                 int has_fc, fcx, fcy;       /* PR-253 row `FC x,y` tail */
                 int has_pos, posx, posy;   /* PR-317 row `pos x,y` tail   */
                 signed char lockcmd, dircmd; /* PR-321 row `lock`/`unlock`
                                               * and `ldir`/`rdir` words */
                 signed char revcmd;         /* PR-527 row `rev` word: the
                                              * one-word tail whose node
                                              * apply 0x459b0 NEGATES the
                                              * row-walk direction word
                                              * [obj+0x99] (see
                                              * ll_player.rowdir) */
                 signed char gotocmd;  /* PR-323: the `goto` family tail.
                                        * 0 none, 1 absolute (gotoval is the
                                        * 0-based row index: the engine's
                                        * parse arm 0x46777-0x46780 stores
                                        * the authored number minus one),
                                        * 2 relative (gotoval is the verbatim
                                        * delta - 0x467dd/0x467ee dec-inc). */
                 short gotoval;
                 short gsig_id; short gneg; short gop; } ll_mov_fr;
                 /* has_i: `I ix,iy` row pull pair (PR-98/PR-250: with the
                  * `F` pair it forms the one-shot 0x54170 target applied
                  * when this row becomes CURRENT; the old "impulse" name
                  * is retired by PR-253).
                 * has_f/has_fc: the `F` (channel id 0, gated with I,
                  * one-shot at row entry) and `FC` (id 1, UNGATED - the
                  * pull re-applies every iteration the row stays current)
                  * tails of PR-250/PR-251. The `IC` tail (id 3, force
                  * pair) has no shipped instance's consumer modeled here. */
                  /* has_pos/posx/posy: the `pos x,y` row tail, modelled in
                  * PR-317. The engine's parse arm 0x466d6-0x46718 makes a
                  * 0x10-byte node (class word 0xa11b0 at [node+0x4]) and reads
                  * the pair through the format at 0x91944 into [node+0x8] and
                  * [node+0xc]; its apply is that node class's +0x4 verb
                  * 0x45a30 - every directive class differs from its siblings at
                  * +0x4 and +0x1c, and each +0x4 body opens with the shared
                  * helper 0x457e0, which walks [node+0x0] and calls the OWNING
                  * object's class +0x4 verb: a directive is bound to an object,
                  * not to the level. For the lion classes 0x96874/TLewZaria
                  * 0x96960 that verb is 0x23850, `mov [eax],edx;
                  * mov [eax+0x4],ebx; call [class+0x54]` - write the position
                  * the physics step and the ask protocol both read, then
                  * refresh: 0x55070 recomputes the presented point [obj+0xd6]
                  * through the class +0xb0 provider (the ask path of PR-313),
                  * then calls 0x541f0 with the [obj+0x89] field and the class
                  * +0x98 verb. NO carry, NO clearance and NO signal: `pos`
                  * bypasses the physics step 0x546a0 of PR-312 completely. The
                  * port applies the SET at the row admit, which is where the
                  * engine runs the row's directive chain, and touches no
                  * velocity. Measured over the 777 extracted texts: 4 rows in 3
                  * files, BOTH spellings shipped - `pos 1020,692` in ZAR_LEW and
                  * ZAR_LEWPL [A_WYJSCIE] (the level's own exit, PR-309) and
                  * `POS 220,140` / `POS 200,140` in RAN.01 [A_DEAD] /
                  * [A_DEAD1]. The 4 rows carry no `M` tail, so the chain's
                  * authored order is not observable in the data. */
                  /* lockcmd / dircmd: the one-word row tails PR-321
                  * models. The engine's names sit in its own directive
                  * table block - "lock" at 0x91901, "unlock" at 0x91906,
                  * the ldir/rdir pair beside them - and the lock/unlock
                  * apply is the node class 0xa10d8's +0x4 verb 0x45c50
                  * calling 0x54f40 (lock: `or BYTE PTR [obj+0xb6],0x1`)
                  * or 0x54f90 (unlock: `and ...,0xfe`); ldir/rdir are
                  * node classes 0xa0f58/0xa0f70 with applies
                  * 0x459f0/0x45a10 writing [obj+0x6d] = -1/+1 outright
                  * (PR-320). Value 0 here = the row carries no such
                  * word; a non-zero value is the engine's OWN write,
                  * not a port invention. Measured over the 777
                  * extracted texts: lock 126 (3 spelled LOCK), unlock
                  * 73, ldir 18 (1 LDIR), rdir 58 (9 RDIR) - ZARIA.01
                  * brackets both exit states with them (A_WPAD rows
                  * 1/11, A_WYJSCIE rows 0/16). The `lock2`/`unlock2`
                  * pair (11 each; node class 0xa10c0, applies
                  * 0x54fe0/0x55020 on values 1|2) is NOT modelled here
                  * and stays counted. */
                 /* gsig_id / gneg: this row's `Wait <signal>` tail
                 * (PR-217), ACTED ON by the object walker since
                 * PR-287 (ll_mov_row_gate); the player's row admit
                 * does not consult it yet, named at OI-111.
                 * -1 = the row carries
                 * no Wait tail; -2 = a token the engine's signal table has
                 * no BIT for (the 255 tails name 161 of them: K_NEVER 159,
                 * K_ANY 1 and `rdir` 1); otherwise its LL_SIG_* index.
                 * MEASURED census of the 777 extracted .MOV
                 * texts: 255 Wait tails in 133 files, 254 of them exactly
                 * ONE token and no text after it (one has none), 22 tokens
                 * negated with `!`; operands K_NEVER 159, K_GEN1 25,
                 * AC_DUP 20, AC_TIME 18, !AC_ENEMY_ATC 8, !K_UP 7,
                 * !K_DOWN 7, the rest <=3. The word is in the engine's own
                 * directive table and its parse arm is 0x46b62-0x46ba2: it
                 * builds a 0x15-byte node (class word 0xa11c8) and parses
                 * the operand with the clause parser 0x47d60 into the
                 * node's 12-byte TEventSet at [node+0x8] - the SAME
                 * shape, node ctor and clause parser 0x47d60 the modelled
                 * `GEN` arm builds (0x46af4-0x46b34, also 0x15 bytes,
                 * class word 0xa11e0); the two differ ONLY in that class
                 * word. Nodes are a CHAIN: the ctor 0x45780 writes the
                 * base class word 0xa1000 at [node+0x4] and puts its
                 * operand at [node+0x0], and the two chain walkers
                 * 0x45800 (test) and 0x45870 (execute) hand back what the
                 * operand answers - 0x45800 an EMPTY set when there is no
                 * operand. The `Wait` class 0xa11c8 answers test with
                 * 0x45970 = the chain's answer OR the node's own
                 * [node+0x8].
                 * HOW THE ROW STEP READS IT (PR-218, end to end): 0x52229
                 * takes the row object from [rec+0x2e][rec+0x4] and
                 * 0x52235 calls ITS test slot +0x8 (class 0xa2638 is the
                 * delegate 0x45800, so the walk lands on the row's
                 * directive chain); 0x5223c calls 0x495b0, which answers 1
                 * only when all three data dwords are zero, and the
                 * test/sete/and/je at 0x52241-0x5224b branches ON that
                 * answer: an EMPTY one takes the plain path 0x522fc, where
                 * 0x52303 calls the execute slot +0xc and 0x5230c OR-s
                 * what it returns into the CALLER's set, and the row then
                 * runs only if its authored frame [row+0x10] is BELOW the
                 * object's clock [obj+0xa1] (0x52320-0x5232b), with the
                 * difference as its budget. Only a NON-empty answer - a
                 * `Wait` node's own clause - reaches the second test:
                 * 0x52251-0x522b3 copy the caller's key and the object's
                 * own EventSet at [obj+0x60] and OR them (0x49580), and
                 * 0x522e6 calls TEventSet::match (0x47e40, its own name in
                 * the error string 0x919ca), which skips the row when it
                 * answers 0 (0x522ed) and otherwise runs it with a ZERO
                 * budget. match switches on the clause's MODE byte at
                 * [set+0xc]: 0 (0x47e63) true when the AND of the two
                 * sets equals the clause byte for byte, i.e. the clause is
                 * a SUBSET of the set it is handed; 1 (0x47edc) true when
                 * that AND is EMPTY (shares nothing); 2 (0x47f42) when it
                 * is NOT empty (shares something); 3 (0x47fb2) when the two
                 * sets are byte-identical. That byte comes from the clause
                 * parser: 0x47d73 clears it to 0 and 0x47db2-0x47dd0 write
                 * 1 for a leading `!`, 2 for `*`, 3 for `|`; a comma does
                 * NOT change it, it only adds a second signal to the same
                 * clause. MEASURED over the shipped texts: 230 clauses are
                 * mode 0 and 21 mode 1, no text uses `*` or `|`, and every
                 * one of them leaves a NON-empty set (K_NEVER writes 0x60
                 * into data byte 4, 0x4806f-0x4807d) - so it is the
                 * clause-carrying rows that reach match, and a refusal
                 * ENDS the step: 0x522ed's `je 0x52354` is the epilogue,
                 * past the execute, the clock compare and the class verb.
                 * PR-287 RE-READS THE FLAGS here: PR-218 had this branch
                 * the other way round, and that reading would leave the
                 * rows carrying NO clause - all but the 255 measured tails
                 * - running with a zero budget and no authored duration
                 * honoured at all, while PR-109's and PR-110's measured
                 * row durations sit on the other side of the branch. So a
                 * `Wait` row is gated on the live set (a plain name must
                 * be up, a `!` name must be down), a `Wait K_NEVER` row
                 * never runs at all - THAT is how a shipped state holds
                 * its last frame for good - and the token-less `Wait ;`
                 * compiles to an empty set and gates nothing. Two halves
                 * stay unmodelled and stay at OI-111: the gated path
                 * skips the threshold compare at 0x52329 and runs with a
                 * ZERO budget, where this port still requires its clock
                 * (the two agree on every row whose authored threshold is
                 * 0, which is every row of the ladder chain PR-287 nets),
                 * and the player's own row admit does not consult the
                 * clause yet. ZARIA.01's own DRAB.MOV is
                 * one of these rows (`[A_UP]` row 1 is `WAIT K_GEN1`,
                 * `[A_DOWN]` row 11 is `WAIT K_NEVER`), which is why the
                 * field lives beside
                 * gsig and not in the transition table.
                 * gop: which of the three no-bit operands a captured
                 * clause names (LL_GOP_NAME / _NEVER / _ANY / _NONE), so
                 * a reader can tell K_NEVER's 0x60 mask from K_ANY's
                 * 0x2001f8 mask from a name the engine's comparator 0x48000
                 * does not know at all. Measured 2026-11-20 over the 777
                 * texts: 161 / 0 / 0 of the 251 captured rows. */
                 /* endlv: this row's `end_level <n>` tail: the AUTHORED
                 * NUMBER kept as DATA, 0 = the row carries no such tail
                 * (PR-199). The word is in the engine's own .MOV
                 * directive vocabulary (the 0x91940 block: pos, goto,
                 * gotorel, goto_rel, I, F, IC, FC, M, MOVE, GEN, Wait,
                 * end_level) and its parse arm is 0x46beb-0x46c01: the
                 * number goes through the `' %i '` format at 0x91998
                 * into [node+0x8] of a node whose class word is
                 * 0xa1048 (PR-194 F(a)). What EXECUTES such a node is
                 * PR-198's read: class 0xa1048's slot +0x04 is 0x45750,
                 * `mov ebx,ds:0xcef40; mov ecx,[node+0x8];
                 * mov [ebx+0x4],ecx` and then the rest of the row's
                 * chain - so the number lands in the CURRENT LEVEL
                 * OBJECT's +0x4 slot, and the level loop 0x4abd0 tests
                 * that slot against ZERO at the top of every iteration
                 * (0x4ac55): a nonzero makes the loop RETURN THE
                 * CONSTANT 1 (0x4ac5b; `jmp 0x4ae53` at 0x4ac60 into
                 * the single epilogue 0x4ae81) for EVERY value, which
                 * is why the shipped 25x1 and 1x2 rows behave alike.
                 * The same pair 0x5d25e/0x5d2dc also publishes the
                 * number beside the code, and PR-198 enumerated all 14
                 * call sites of the level function 0x5d170: NONE reads
                 * it. So the value is carried, never interpreted.
                 * One documented collapse: the engine's apply is
                 * unconditional, so a literal `end_level 0` row would
                 * CLEAR the slot; the parser cannot tell that row from
                 * a row without the tail once the value is 0. No shipped
                 * script does it (26 rows, values 1 and 2 only), and the
                 * census leg (verify-endlevel) re-measures that. */
                 /* gsig: the frame's `GEN <signal>` tail(s), one bit per name
                 at its LL_SIG_* index (PR-88: any table name, not just
                 K_GENn/AC_TIME - the engine's own GEN survey raises
                 AC_PLAT_OFF 181x, AC_TIME 60x ...). Names at index >= 64
                 (AC_WALK_UP/.., AC_EV_MAX) never appear in a GEN tail in
                 any shipped script, so one u64 covers all raised signals;
                 such a name would be counted unmodeled, not guessed.
                 The old K_GENn bit n-1 / AC_TIME bit 4 encodings are
                 gone: the same bits now live at their table indices.
                 atc: this frame's `ATC_ON`/`ATC_OFF` tail (the engine's
                 frame-directive table 0x917c0 has `atc_on`/`atc_off`):
                 bit 0 = the frame OPENS the attack-connect window,
                 bit 1 = the frame CLOSES it. The window decides when the
                 state's shape can land a hit (ZARIA's A_FIGHT opens it
                 at frame 8, right before its `FC 100,0`; the LICHA's
                 A_ATAK opens at -4 and closes at -8).
                 thresh: the row's THRESHOLD, in 1/100 units of the
                 engine's row accumulator [obj+0xa1], which is fed
                 exactly 100 per FRAME-LOOP ITERATION (the level
                 constant of 0x4c8b7, read at 0x543f2/0x54406/0x5442f).
                 The unit was settled by PR-109's measurement of the
                 owner's ladder clip (the authored 200/300 of [A_WSP]
                 are on screen 66-100 ms, i.e. 2-3 iterations); before
                 PR-110 this field was called `ms` and the row clock ran
                 on wall milliseconds, 3.0-3.5x too slowly. The engine
                 pre-scales every row value by the block's `time_scale`
                 at parse time (`value * time_scale / 1000`,
                 0x51679-0x5168f); ll_mov_parse applies that scale the
                 same way (PR-110), so this field holds the SCALED
                 threshold. A row of 200 is therefore current for
                 exactly 2 iterations; the port's iteration period
                 LL_ITER_MS decides the wall time. */
#define LMOV_ATC_ON  1u
#define LMOV_ATC_OFF 2u
/* One parsed condition/signal token: `cond` is the modeled LL_COND_* id
 * (LL_COND_OTHER for anything else), `neg` the `!` prefix, `grp` the
 * AND-group number (PR-87), `sig` the LL_SIG_* index when the token is a
 * signal NAME rather than a modeled condition. PR-217: this element type
 * is shared by the transition table (ll_mov_tr.c) and by a row's `Wait`
 * clause, which the clause parser reads the same way. */
typedef struct { int cond; int neg; int grp; short sig; short gop; }
    ll_mov_gate;
typedef struct {
    char target[24];                        /* resolved at parse time      */
    int tidx;                               /* state index, -1 unresolved  */
    int entry;               /* PR-86: the numeric (N) = START ROW (1-based)
                              * in the TARGET state's frame list; 0 = from
                              * the top. Evidence: A_PRE_JUMP's impulse row
                              * precedes `A_JUMP(4)=AC_BREAK_NEXT` and
                              * A_JUMP row 4 is its FIRST row after the
                              * `I 150,-1200` row - entering at (4) skips
                              * re-applying the impulse A_PRE_JUMP already
                              * gave; A_JUMP's `A_WALK(7)=AC_BREAK_NEXT`
                              * re-joins ZARIA's 12-row walk cycle at row
                              * 7 (mid-phase, seamless landing); A_KILOF's
                              * `A_JUMP(9)` re-joins the jump mid-air. The
                              * token can be non-numeric (KADMAR2 "(x)"):
                              * then it starts from the top. PR-107 read
                              * the engine's own handling of this token -
                              * `%ld` then `dec` (0x57fd0/0x51e3b), -1 when
                              * the token is absent or not a number
                              * (0x51e3e) - and made the scene objects
                              * honour it as well as the lion: both sides
                              * now map it through ll_mov_row_start. */
    ll_mov_gate c[LMOV_MAXCON];
    /* PR-87: tokens sharing a grp number are ANDed; the line is true
     * when ANY group is fully true ('|' starts a new group, ',' splits
     * one; see the enum comment). A single-token line is one group -
     * identical behavior to the old OR-everything model. PR-88: sig is
     * the LL_SIG_* table index of the token's NAME - every modelled
     * condition token is also a bit in the engine's signal table, so
     * K_DOWN carries 38 as well as LL_COND_K_DOWN; LL_SIG_NOBIT (-2) is
     * the engine's no-bit group (K_ANY/K_NEVER and their aliases, and
     * names in no table at all). The transition switch answers modelled
     * tokens first and only reads sig in its OTHER arm; the `Wait`-clause
     * capture (PR-217) reads sig directly.
     * PR-264 REPLACES that grammar in the evaluators with the engine's
     * own: `op` is the operation byte the clause parser writes for the
     * WHOLE mask - 0 when it names no operator, 1 for '!', 2 for '*',
     * 3 for '|' (0x47d73 clears it and 0x47dc8/0x47dc2/0x47dbc write
     * those three; the last one seen wins, and the parser puts it on the
     * compiled entry, PR-261). The group numbers stay in the terms
     * because the row-clause capture still reads them; they no longer
     * mean anything to a match. */
    unsigned char op;
    int nc;
} ll_mov_tr;
/* The mask a compiled condition carries: the union of its tokens' bits,
 * K_ANY's and K_NEVER's whole masks included, and nothing at all for a
 * name in no table - the resolver's return value is DISCARDED at its
 * call site 0x47de5, so an unresolved name simply contributes no bit
 * (PR-264). */
unsigned long long ll_mov_tr_mask(const ll_mov_tr *tr);
typedef struct {
    char name[24];
    char seqkey[16];                        /* per-state seq= art select   */
    int fx, fy, tx, ty, gx, gy;             /* F= T= G= (px/s^2, px/s)    *
                                             * ENGINE SEMANTICS, PR-98 RE'd
                                             * (NEXT_STEPS PR-98; the port's
                                             * parameterization below is NOT
                                             * the engine's): per tick the
                                             * engine drags first
                                             * (`v -= T= * v/100`, a PERCENT,
                                             * sign flip stops it, x drag
                                             * floored at 20 at 0x54266) and
                                             * then increments
                                             * `v += F_eff*s/20 + G=*facing`,
                                             * F_eff = the state F= plus the
                                             * current row's `FC` pairs (+
                                             * `IC` when obj+0x95 is set),
                                             * s = an unread per-object
                                             * terrain scalar. A row's `F`/
                                             * `I` pairs are instead a
                                             * velocity TARGET (0x54170 sets
                                             * v to it, never past it, x by
                                             * facing, y down-positive, after
                                             * the drag). Its steady state
                                             * v_eq = 5*F_eff*s/T. PR-98 called
                                             * that refuted by the measured
                                             * walks; PR-99 corrects the
                                             * wording. The two OBJECT samples
                                             * (worm, rhino) share T=40, where
                                             * this law and the port's F/4 law
                                             * differ only by a constant, and
                                             * the T=20 sample is the player,
                                             * whose path through 0x54250 is
                                             * unverified (NEXT_STEPS PR-99).
                                             * The mode dispatch PR-99 did read
                                             * is 0x543a0 - PR-100 corrects
                                             * that record's 0x543a1; jump
                                             * table 0x54380 - and PR-100
                                             * found its caller: the state
                                             * record's own step at 0x521f0,
                                             * which compares the current
                                             * row's authored threshold with
                                             * [obj+0xa1] and takes at most
                                             * one row per step. It
                                             * shows a BY_X walk DOES go
                                             * through that loop: mode 0 is
                                             * BY_X, and it feeds the row
                                             * distance accumulator [obj+0xa1]
                                             * from this loop's v/100 (PR-98
                                             * item 2 CLOSED). So the two laws
                                             * differ only in the T dependence,
                                             * and what decides between them is
                                             * a BY_X object at T other than
                                             * 40: the data has F=100 against
                                             * 11 different T pairs, but only
                                             * ZARIA is on video. The struct
                                             * mapping is not the open part:
                                             * 0x52590 parses F= to +0x50/54,
                                             * T= to +0x58/5c (capped at 130)
                                             * and G= to +0x48/4c, and the
                                             * header F= reaches only the
                                             * increment channel. The port
                                             * keeps the px/s reading until
                                             * that T test exists. */
    int time_lo, time_hi;                   /* PR-83 parsed, PR-163 MODELLED:
                                             * `time=<min>[,<max>]`. The
                                             * engine law (PR-159 F2 / PR-
                                             * 160 F2-F3): the pair lands
                                             * at [rec+0x8]/+0xa as WORDs
                                             * and the ENTER verb (0x52072)
                                             * rolls [rec+0xc] = min +
                                             * (max-min)*rand()/32767 (its
                                             * 0x76bcc = the classic Borland
                                             * LCG); same-state transition
                                             * MATCHES counted in [obj+0x9d]
                                             * raise live-set bit 9 once the
                                             * count reaches the roll; NO
                                             * time= line = gate 0 = never.
                                             * The scanf pair ('%hi' /',%hi')
                                             * leaves max=min when the comma
                                             * is absent (the single-value
                                             * `time=350` form) - same shape
                                             * as parse_pair. */
    int time_scale;                         /* PR-110: the block's
                                             * `time_scale=`, stored as
                                             * read; 1000 = the engine's
                                             * no-op default. Applied to
                                             * every row threshold of
                                             * this block at parse time
                                             * exactly where the engine
                                             * applies it - at row-node
                                             * construction, 0x51679
                                             * `imul` / 0x5168d `idiv
                                             * 0x3e8`, i.e. only rows
                                             * that FOLLOW the line in
                                             * the block (PR-104 read
                                             * the ctor order; in the
                                             * shipped scripts it is
                                             * always ahead of them and
                                             * none of the 777 writes
                                             * rows first - verified,
                                             * not assumed). */
    int scale_at;                           /* index of the first row the
                                             * scale applies to: nfr at the
                                             * moment the line was read
                                             * (0 when the line precedes
                                             * every row) */
    unsigned ctrl;
    ll_mov_tr tr[LMOV_MAXTR]; int ntr;
    ll_mov_fr fr[LMOV_MAXFR]; int nfr;
    /* authored content the model reads but does not act on (reported): */
    int ctrl_other;                         /* control= names without a bit */
    int directive;                          /* frame-line GEN/ATC/WAIT/...  */
    int wait_cap;                           /* PR-217: rows whose `Wait`
                                             * clause was CAPTURED into
                                             * ll_mov_fr.gsig_id/gneg; no
                                             * model acts on it, so it is
                                             * reported as captured, not
                                             * as an unread directive    */
    int field_other;                        /* sample= time= (time_scale=
                                             * is modelled since PR-110)   */
    int seq_switch;                         /* PR-330: row lines that OPEN
                                             * with the engine's `s <seq>`
                                             * field. The ROW is modelled
                                             * (frame, units, tails, exactly
                                             * like any other line); the
                                             * sequence record the field
                                             * names - [node+0x8], the art
                                             * this row draws - is captured
                                             * as a count and not acted on:
                                             * the draw half is OI-120    */
    /* PR-216: the engine's state index for THIS state, the value of
     * [obj+0x5c] while the object runs it. The engine numbers states
     * by their position in the list a script declares BEFORE its
     * blocks, ONE-BASED: the loader 0x53905 reads that list into the
     * vector [obj+0x3c] and its length at [obj+0x58]; 0x5397a allocs
     * count+1 record slots; the per-block handler 0x53480 stores its
     * record at [[obj+0x50] + 0x53440(name)*4] and 0x53440 returns
     * the 1-based position in that vector, 0 when the name is not
     * declared; the record-ctor loop 0x533e3 starts at byte offset 4
     * = slot 1; and the object ctor writes [obj+0x5c] = 1 (0x538ab),
     * the first declared state. Block order is NOT the index space:
     * 68 of the 777 shipped scripts (PR-51's count) put an [A_DEAD]
     * block ahead of the declared start, and ZARIA.01's DRAB.MOV
     * declares A_UP,A_DOWN,A_DROP but writes the blocks
     * A_UP,A_DROP,A_DOWN. 0 = not in the list, the engine's own
     * not-found value (0x53471). */
    int decl_idx;
    /* PR-235: the AUTHORED block position (0-based index of the
     * `[NAME]` line in the file). The engine numbers every record
     * with a counter at ds:0xceff8: the block parser zeroes it at
     * 0x5150b and every record create call receives it
     * (`push ds:0xceff8`, e.g. 0x51a2a/0x51a8c), so the record ID
     * word - record +0x4, stored at object offset +0x8d by the
     * object-block copy 0x5386c - is that position. The state-ID
     * branches read it: 0x287de-0x287ed compares the LION's
     * current state's ID with 6 and 0x2880b-0x28826 with 0x12;
     * cross-checking ZARIA.01's LEW.MOV block order (a census,
     * not a fit) puts A_DEAD at 6 and A_DRAP at 18 = 0x12. */
    int block_pos;
} ll_mov_st;
typedef struct {
    ll_mov_st st[LMOV_MAXST];
    int nst;
    char seqkey[16];                        /* last seq= seen (viewer uses
                                             * this, matching old mov_t)   */
    int drop_st, drop_fr, drop_tr;          /* cap overflows: reported     */
    /* the state LIST every script declares before its first [STATE] (777 of
     * 777 do): its first name is the state the object starts in. 68 scripts
     * write a terminal [A_DEAD] block before the declared start (PR-51:
     * the port used to start those in the dead state). start_st = index of
     * that name among the blocks, 0 when absent/unmatched. */
    char first_decl[24];
    int start_st;
    /* PR-216: the whole declared list, in the order the script writes
     * it, kept because that order IS the engine's state index space
     * (see ll_mov_st.decl_idx). Census of the 777 shipped .MOV texts:
     * all 777 declare a list, the longest is 40 names (so
     * LMOV_MAXST covers it; extras would be dropped like the blocks),
     * the longest name 17 chars, and in 172 of them the engine's
     * index differs from the block position for at least one state.
     * A block whose name is NOT declared (9 files have one) gets
     * decl_idx 0, the engine's own not-found value. */
    char decl[LMOV_MAXST][24];
    int nddecl;
} ll_mov_t;

int ll_mov_parse(const char *text, ll_mov_t *m);   /* returns m->nst      */
/* PR-330: ONE copy of the engine's row-line test, so anything that
 * counts rows counts what ll_mov_parse appends. Returns 1 and sets
 * *body to the first byte of the frame field and *frame and *units
 * to its two numbers; else 0. A line whose first token is the field
 * name `s` (0x92146) is a row line too: the engine reads one .MOV
 * line through one character cursor, so its `s` arm reads the
 * sequence NAME (" %[^ ,]%*[ ,]", 0x92148) at 0x51a6a, registers
 * that record at 0x51a7b-0x51a95, then reads the frame with "%d,"
 * (0x92156) at 0x51a9a and the units with "%hi" (0x92115) inside the
 * row ctor 0x515e0 (0x51660), and appends the node to the state's
 * row vector [st+0x2e] (0x51ac9-0x51af7). *seqsw is 1 when the line
 * opened that way; the caller then keeps the units as authored,
 * because that arm builds the row with the constant 0x3e8 divisor
 * (0x51ab7) instead of the state's time_scale. */
int ll_mov_row_line(const char *line, const char **body, int *seqsw,
                    int *frame, int *units);
int ll_mov_find(const ll_mov_t *m, const char *name); /* state idx or -1  */
/* PR-107: the row of ll_mov_tr.entry as a 0-based index into s's frame
 * list: entry-1, and 0 for an absent/non-numeric token or an over-long
 * row. The engine evidence and both model choices are in the definition. */
int ll_mov_row_start(const ll_mov_st *s, int entry);
/* PR-287: this row's `Wait` clause, matched against the live signal set.
 * PR-333 re-read it as the engine compiles it: the operand is a BIT MASK
 * (K_ANY and K_NEVER are the resolver's whole masks at 0x48037/0x48075,
 * not wildcards) and the clause's OWN mode byte decides the test - the
 * plain prefix is the subset arm 0x47e63, `!` is shares-nothing 0x47edc.
 * 1 admits the row, 0 holds it (no frame, no list wrap, no signal). */
int ll_mov_row_gate(const ll_mov_fr *f, unsigned long long live);
/* PR-333: the row step's TWO admission paths, 0x521f0 read end to end
 * (PR-332's record). 1 = this iteration admits the row, 0 = nothing at
 * all happens; the row clock is updated by the path's own rule - the
 * clause-less row subtracts its threshold and carries the remainder,
 * the clause-matched row is zeroed (the ZERO budget of 0x522f8) and its
 * threshold compare never runs. */
int ll_mov_row_admit(const ll_mov_fr *f, unsigned long long live,
                     int walk, unsigned *row_t, double *facc);

/* ------------------------------------------------------- player sim ---- */
/* Held-key view of the input (platform.h translates the arrows; the CI
 * drives the same struct from LEO_PLAY_SCRIPT). */
/* PR-87: the fire family. k_fire = the attack key (viewer mapping:
 * Ctrl, per the walkthrough); k_f1..k_f4 = the weapon-selector keys the
 * data's comma groups gate with (viewer mapping: F1..F4 held); k_alt =
 * the ALT modifier condition (throw/secondary routes). */
typedef struct { int k_fwd, k_bkwd, k_up, k_down, k_jump,
                 k_fire, k_f1, k_f2, k_f3, k_f4, k_alt; } ll_play_keys;

/* PR-110/135: the port's model of ONE FRAME-LOOP ITERATION, in ms.
 * The engine advances its row accumulator by exactly 100 once per
 * iteration of the level frame loop (0x4c8b7's constant, read at
 * 0x543f2/0x54406/0x5442f), and PR-133 closed the rate MECHANISM:
 * one state step per iteration, one physics pass per state step for
 * the BY_X and time-mode families. PR-109 first bracketed the period at
 * 22-50 ms (its [A_WSP] rows 200/300 ran 66-100 ms of screen time),
 * and PR-134 measured the rate itself on two independent law objects
 * inside one four-ways-verified camera lock: the ZARIA.01 worm
 * (F=100,T=40 -> 1.25 px/iteration) ran <= 30.3 units/s, i.e.
 * <= 24.2 iterations/s, and the lion's ground run (A_WALK F=100,T=20
 * -> 2.5 px/iteration) measured [38,76] units/s, i.e. >= 15.2
 * iterations/s. The two families overlap at 15-24 iterations/s and
 * 50 ms (20/s, the far edge of PR-109's bracket) sits inside both;
 * 28 ms (35.7/s) predicts 44.6 u/s for the worm - above its measured
 * bound - and 89 u/s for the lion - above the whole bracket. So 28
 * is REFUTED and 50 ms is the port's clock. The author's own startup
 * fallback (0x4a190, 2 retrace intervals, 27 ms at his assumed 14 ms
 * retrace) measured on real hardware is exactly what PR-134's band
 * refutes at that retrace rate - the fallback only says the engine
 * paces on retraces, not how long one was.
 * THE SINGLE GRID (PR-135, closing PR-131's probe ambiguity): every
 * fixed grid in the port - the PPM probe, the play selftest legs, the
 * live loop's substeps - runs at exactly this constant, one
 * ll_player_step/play_advance/obj_advance call per iteration when the
 * caller passes dt == LL_ITER_MS; other dts are consumed as whole
 * iterations with a Bresenham remainder (grid-independent by design,
 * exercised by the 2x50+16 and 10x50 legs). The real value on a
 * given machine was display-rate dependent (the setpoint was measured
 * at runtime) - open item PR-109.3 stays; this constant says which
 * rate the port reproduces. --play-selftest asserts this number. */
#define LL_ITER_MS 50u

/* PR-135: the two constants the engine law of PR-133/134 is written
 * with, in port units. This port keeps velocities in units/s; the
 * engine's own velocity register is hundredths of a px per iteration.
 *   drag   v -= v*T/100 per iteration  (0x5427c..0x542a7; the divisor
 *                                      is LL_PLAY_DRAG_DEN and T is the
 *                                      authored drag percent word)
 *   force  v += F*s/100 per iteration  (0x5434f adds F*s/20 in the
 *                                      engine's units, which at the
 *                                      50 ms iteration is F*s/100 in
 *                                      units/s)
 *   steady v = F*s/T units/s was PR-134's PER-ITERATION reading.
 *                                      PR-518 REPLACES the cadence
 *                                      half of it: the live trace
 *                                      (PR-517, named states) shows
 *                                      the physics pass runs only at
 *                                      ROW ADMITS, so px/s is
 *                                      (register//100) x admit rate -
 *                                      PR-134's video bands still
 *                                      close, now on the live
 *                                      cadence (see LL_PLAY_INTENT_
 *                                      FULL for the s decision that
 *                                      goes with it)
 * s is the engine's intent register ([obj+0xba]/[obj+0xbe]). The
 * scale question (PR-147's 10-vs-20 census, OI-011) is CLOSED by
 * PR-515/516/517 reading the playable lion itself: the keyboard path
 * writes the RAW +/-20 (0x582f0; live [obj+0xba] = 20 under a held
 * arrow, and the register settles at the s=20 equilibrium - EXACTLY
 * 500 at the authored A_WALK pair F=100,T=20, 622-646 in another
 * state). The 10 of 0x54a57/0x54b16 is the CONTACT-node path writing
 * the same word toward a partner - a path this port does not model,
 * so no port consumer needs it. The video band [38,76] px/s then
 * picks the s that FITS the corrected cadence: at s=20 the register
 * reaches the authored equilibrium 500, the walking state's row feed
 * is v//100 = 5 per frame against its authored threshold 6, which
 * admits 16.7 times a second and moves the body 5 px each time -
 * 83 px/s, inside the band and the same shape the live A_WALK
 * windows measured (feed 5/frame, admits ~14/s, 50-64 px/s); at s=10
 * the register reaches 250, the feed runs 2 a frame and the same
 * arithmetic gives ~16 px/s, matching nothing. So the constant is the keyboard's word, 20 - the decision
 * OI-011 asked for, made on three independent measurements (live
 * word, live register equilibrium, live px/s) with the video band as
 * the bound. PR-523 FLIPS THE CONSTANT to that recorded answer, 20,
 * together with the row-admit default and the entry register-seed:
 * PR-522 proved this is not merely a speed question - a BY_X walk
 * cannot cold-start at all below s>=14, so 10 and the corrected
 * physics cannot coexist as the default. The 69 --play-selftest
 * checks 20 turns red (the gait, walk-distance and BY_TY legs), and
 * the ~90 the row-admit order turns red, are re-derived here on the
 * closed-form F/T/G arithmetic, not scaled. PR-99's 10 and PR-134's
 * fitted band stay cited where they were measured; partial intents
 * remain an open item, not modeled. */
#define LL_PLAY_DRAG_DEN 100
#define LL_PLAY_INTENT_FULL 20
/* PR-135: one unit of the engine's velocity register. That register is
 * an INTEGER in hundredths of a px per iteration (PR-98 read the /100
 * at 0x5435d and the sub-pixel scale of `I 150,-1200` = 1.5 px per
 * iteration), so its smallest non-zero magnitude is 0.01 px per
 * iteration = 0.01 * 1000/LL_ITER_MS = 0.2 units/s at this clock. The
 * port carries velocities in a double, so a drag tail like
 * 44.4 -> 4.4 -> 0.44 -> 0.04 never reaches the 0 an integer register
 * would hold. What IS read at byte level is only the sign-flip rule
 * (0x5426x zeroes the component); the sub-unit case is MODEL CHOICE,
 * counted as an open item: the port zeroes what is below one register
 * unit, because nothing below it can exist there. */
#define LL_PLAY_V_EPS 0.2

/* Project constant, see header note; used only when airborne with a
 * CHECK_LN state that carries no authored fall route. PR-256 fixes its
 * UNIT: the value is the word RAN.01's real A_START_LAD1 authors (its
 * header says `G=0,200`), and 0x542ee shows what such a word does - it
 * is added to the velocity register once per iteration, in the register's
 * own unit. Reading it as 200 px/s^2 made this add four times smaller
 * than the authored word is (and with a state T_y of 90, as RAN's
 * A_WALK carries, the resulting terminal speed of 0.56 px per iteration
 * truncated to a step of ZERO px: the body reported AIR and then did not
 * move). Counted as an open item, not smoothed over: this is a project
 * route, not an authored one, and its magnitude has no video check.
 * PR-285 NARROWS ITS SCOPE, on data: it is never used in a state whose
 * authored y-drag is 100 or more, because that word IS the author's hold
 * - idiv(T_y*v,100) with T_y >= 100 consumes the register whole, so with
 * no add the body stops where it is. The census of the 777 .MOV texts
 * counts 2318 state blocks with no default gravity anywhere in the game
 * (683 author a non-zero G_y, 697 author G=0,0, 938 author no G line),
 * every falling state authors its own word, and the drag-100 +
 * zero-force idiom appears 39 times in the player's own scripts (26
 * blocks at exactly 100, 13 more at 150/1000) under the hold names
 * A_DEAD, A_SCIAG, A_PODCIAG, A_WPAD, A_JUMP_DEAD, A_FINISH_LEVEL. */
#define LL_PLAY_GRAVITY 200
/* PR-253 replaces the PR-73/74 ESTIMATES (LL_PLAY_JUMP_DIV=4,
 * LL_PLAY_JUMP_G=1650) with the measured unit law. The engine keeps its
 * velocities in hundredths of a pixel per iteration (the /100 at
 * 0x5435d/0x54370) and this port runs one iteration per LL_ITER_MS
 * (PR-135's 50 ms clock), so one authored row-pull pair value is raw/5
 * px/s (LL_PLAY_RAW_VDIV) and one authored G= word is raw*4 px/s^2
 * (LL_PLAY_RAW_GMUL). These are unit conversions of measured words, not
 * fits: the pull itself is 0x54170, byte-read in PR-98 and re-read
 * end-to-end in PR-253. */
#define LL_PLAY_RAW_VDIV 5
#define LL_PLAY_RAW_GMUL 4
/* Vertical snap tolerance while walking, BOTH directions (slope
 * following; the real .plt slopes need far less, this bounds curbs and
 * short same-height line gaps - RAN.PLT's 302->323 shelf step is 1 px
 * up across a 20 px gap - without teleporting across real drops). */
#define LL_PLAY_STEP 16

typedef struct ll_player {           /* tagged for the anchor_shift hook */
    const ll_mov_t *mov;
    /* PR-97: x,y is the POSITION POINT the frame art is anchored from (the
     * element's own anchor f0/f2 is measured from it), NOT the spot that
     * touches the terrain. What touches the ground is `y + contact` below.
     * Proof in ll_play.c's ground-logic note: every animation's frame
     * anchors are authored so its SHAPE bottom sits at one fixed offset
     * under the position point (ZARIA LEW.ZAR WALK.SEQ: f2=31 constant for
     * all 12 frames, shape bottom at pos+30..+32; its END.SEQ moves f2
     * from 5 to 54 while the shape bottom stays at exactly pos+58; the
     * ZARIA worm WAZ.SEQ swings f0 from -2 to -22 and f2 from -9 to -16
     * while its bottom stays at pos+46..+49). */
    double x, y;
    double vx, vy;              /* px/s                                   */
    int sti;                    /* current state in mov                   */
    int fi; unsigned row_t, row_rem;
                                /* PR-110/135: the row clock. row_t
                                 * counts 1/100 units of the engine's row
                                 * accumulator (fed 100 per iteration,
                                 * ll_mov_fr.thresh is compared against
                                 * it); row_rem is the ITERATION
                                 * remainder: ll_player_step feeds dt_ms
                                 * here and runs exactly one state step
                                 * and one physics pass per completed
                                 * iteration of LL_ITER_MS (PR-133's
                                 * chain; Bresenham carry, so the port
                                 * is grid-independent). Replaces the
                                 * old `ft` wall-ms clock, PR-108's leg
                                 * pinned that one. Only the TIME modes
                                 * feed row_t: the dispatcher 0x543a0's
                                 * mode-0 (BY_X) branch feeds the same
                                 * engine accumulator from the distance
                                 * travelled, which is `facc` below. */
    double facc;                /* PR-110: the OTHER feed of the same
                                 * accumulator. The engine's dispatcher
                                 * 0x543a0 feeds [obj+0xa1] BY MODE:
                                 * mode 0 (BY_X) adds |[obj+0xca]|, the
                                 * distance this step travelled (PR-98
                                 * item 2, PR-100's owner reading), and
                                 * only the time modes add 100 per
                                 * iteration. ll_level.c's obj_advance
                                 * has modelled that split since PR-98
                                 * (`walk ? facc : t`); the player side
                                 * never had the distance half, and
                                 * under the iteration feed a BY_X row
                                 * list of 6-unit rows (RAN's and
                                 * ZARIA's real walk lists) would cycle
                                 * inside one iteration. Same law, same
                                 * field name as lvl_obj.facc. PR-518:
                                 * for a BY_X state this feed is added EVERY
                                 * iteration from the PERSISTED step word
                                 * row_dx below - the engine's mode-0 arm
                                 * adds [obj+0xca], a stored word, while
                                 * physics runs only at admits, so between
                                 * admits the accumulator still walks
                                 * toward the threshold. */
    long long row_dx;           /* PR-518: [obj+0xca], the whole-px step
                                 * the last physics pass produced. The
                                 * mode-0 row feed reads this word; it
                                 * survives between admits because the
                                 * object's words do. */
    int intent_on;              /* PR-523: last iteration's answer to
                                 * "is this state's x-intent word
                                 * non-zero". The rising edge of that
                                 * word ([obj+0xba], the live trigger
                                 * PR-522 measured at every cold walk
                                 * start) arms the one register-seed
                                 * pass; the port tracks its own s so
                                 * both the key path and the auto-walk
                                 * path produce the same edge. */
    int grounded;
    int walkdir;                /* -1/1, set by K_BKWD/K_FWD transitions
                                 * (the real CH_DIR flag's effect)        */
    signed char dirword;        /* PR-321: the authored value of the
                                 * engine's direction word [obj+0x6d],
                                 * written by the row words `ldir`/`rdir`
                                 * at 0x459f0/0x45a10 and rewritten 1 at
                                 * every state start (0x50442). 0 here
                                 * means no row has written it and the
                                 * walkdir stand-in keeps its role at
                                 * the two x-scale sites (0x54340,
                                 * 0x54bb2); +/-1 overrides it there.
                                 * Whether the key-walk path also writes
                                 * that word in the engine is unread -
                                 * the stand-in stays named at OI-015. */
     signed char rowdir;         /* PR-527: the engine's ROW-WALK
                                 * DIRECTION word [obj+0x99]. The row advance
                                 * PR-99/PR-107 read at 0x520c0-0x521e1 steps
                                 * the cursor -1 and wraps at the UNDERflow
                                 * while this word is -1, and +1 with the
                                 * wrap at the top end otherwise; the reverse
                                 * wrap raises mask bit 3 = AC_BREAK_PREV
                                 * (0x521a1 `orb $0x8`) and leaves the cursor
                                 * at count-1 (0x521a5-0x521a9), the twin of
                                 * the forward wrap's bit 2 (0x521c1). The
                                 * `rev` row word NEGATES it (0x459b0: read
                                 * [obj+0x99], `neg`, write it back), and the
                                 * word is written +1 by the object ctor
                                 * 0x5388f and again at every state start
                                 * 0x52377 - the same instruction block that
                                 * zeroes the row accumulator [obj+0xa1].
                                 * Two more terms join that parity: SEQ_REV_X
                                 * with the owner class's +0x70 getter (all
                                 * three lion classes answer 0x239f0) and
                                 * SEQ_REV_Y with [obj+0x71]. PR-99/100/101
                                 * read the flags; neither word has a reader
                                 * in this port and that half stays at OI-113.
                                 * 0 here stands for the engine's +1: a
                                 * zero-initialised model walks forward like a
                                 * fresh object, and `rev` writes -1. */
    int lock;                   /* PR-321: mirror of VALUE 1 of the
                                 * engine's [obj+0xb6]: set by the `lock`
                                 * row word through 0x54f40, cleared by
                                 * `unlock` through 0x54f90, and cleared
                                 * again at every state entry by 0x54e00
                                 * (`and ah,0xc0`). While it is set the
                                 * engine's position store 0x54ee0-
                                 * 0x54f20 keeps the drawn body at the
                                 * frozen anchor; the port's counterpart
                                 * is ll_player_step's integration gate. */
    /* PR-525: the ELEMENT ANCHOR SHIFT pair that the `lock`/`unlock`
     * words carry. 0x54f40/0x54f90 are not only bit-writers: behind
     * the bit guard they read the CURRENT .SEQ element (the lion's
     * getter [vt+0x48] is 0x23aa0 - state word [obj+0x5c], state array
     * [obj+0x50], the current row's element record), take the draw-flip
     * sign ([vt+0x50] is 0x53e40: -1 while the current frame record's
     * flip word [rec+0x14] is set, +1 otherwise) and call that
     * element's own land/leave method 0x42980/0x429d0, whose write
     * target is [edx]/[edx+4] with edx = the object - the LIVE
     * position pair, not a presented point. ZARIA.01's A_PODCIAG
     * `seq=lift` measures f2-f6 = +40 on element 0 (its `lock` row is
     * `1,300,lock`) and -22 on element 7 (its `unlock` row is
     * `8,300,unlock`), so the top of the authored climb is a 40 px
     * jump of the position pair followed by the T=100 hold. The port
     * resolves the art side through a driver hook because the loaded
     * .SEQ blobs are the driver's (play_art in ll_level.c): the hook
     * answers the facing-signed delta pair for the CURRENT row's
     * element, 0 = no element for that row (blank frame / the 0x7fff
     * sentinel, the engine's guard at 0x4298d/0x429dd). */
    int (*anchor_shift)(void *ctx, const struct ll_player *p, int land,
                        int *dx, int *dy);
    void *anchor_ctx;
    /* PR-526: the level's OWN lion class carries its end-of-stage rule.
     * The level .DES names it (`lew=TLewZaria,...` in ZARIA1.DES) and
     * the level-session open 0x4dde0 (the ctor-alloc method; its
     * factory call sites run 0x4de24-0x4dece) tries that name down an
     * ordered factory chain - 0x1fef0 'TLewZaria'/'TLewZariaB' (alloc
     * 0x12e, ctor 0x1e0a0, class 0x96960 stored at [obj+0x8]), 0x25510
     * 'TLewEna', 0x269a0 'TLewEna2', 0x29b00 'TLewKadmar1', 0x2bfa0
     * 'TLewKadmar2', 0x2f0f0 'TLewSades', 0x33500 'TLewRan', 0x35200
     * 'TLewKara', 0x3a430 'TLewTunga1'/'TLewJunction', 0x3b150
     * 'TLewTunga2', 0x38a30 'TLewLona' (the name strings are at
     * 0x9078b/0x90795, 0x908b3, 0x908f6, 0x90aae, 0x90b36, 0x90d01,
     * 0x90fe6, 0x9112a, 0x9132c/0x91337, 0x91391, 0x912cf) - and
     * stores the winner at [session+0x394]. THE LEVEL LOOP TESTS THAT
     * OBJECT FIRST: 0x4ac3b `mov 0x394(%ecx),%eax`, 0x4ac41
     * `mov 0x116(%eax),%eax`, 0x4ac47 test, 0x4ac49 je - and only on
     * zero does it reach 0x4ac55's end_level slot `cmpl $0,0x4(%ecx)`;
     * a nonzero +0x116 takes ebx=3 and jumps to the loop's exit tail.
     * Every object class carries one verb on the transition path:
     * 0x53520 copies the iteration's EventSet [obj+0x60] (0x53536-
     * 0x53557), matches the state's transition list against that copy
     * (0x5356b), then calls [class+0x90] (0x53574 loads the class,
     * 0x5357d the call) - and only then zeroes [obj+0x60] through
     * 0x749a0(eax=[obj+0x60], ebx=0xc, edx=0) at 0x5358e, while the
     * state change 0x535a0 (called next by the object step 0x536f0 at
     * 0x5374b) has not run yet. For ZARIA that slot is 0x1e250 (the
     * dword at 0x969f0 = class 0x96960 +0x90): unless the one-shot
     * [obj+0x12a] is armed (0x1e27f), it compares [obj+0x5c] with 9 -
     * A_WPAD, the 10th name in ZARIA's OWN declared state table -
     * (0x1e2a0), tests bit 2 of [obj+0x60] (0x1e2a6 `testb $4`, the
     * AC_BREAK_NEXT the row walk itself arms at 0x521c1
     * `orb [ebp],4`), and writes `movl $1,0x116(%ecx)` (0x1e2b6). The
     * base lion's arm (0x96874+0x90 = 0x3d940) raises nothing and
     * TLewZariaB's (0x97a98+0x90 = 0x1e350) has no such arm, so the
     * rule is per-class data: the driver supplies it exactly like
     * anchor_shift, and end_req is this port's copy of [obj+0x116] -
     * raised once, cleared only by the next spawn's ctor (the base
     * lion's 0x500b0 zeroes it at 0x5014c), never mid-run. */
    int (*end_rule)(void *ctx, const struct ll_player *p);
    void *end_rule_ctx;
    int end_req;                    /* [obj+0x116] in this model      */
    signed char face;           /* PR-82: instance facing, -1/1, like
                                 * lvl_obj.face (PR-81). Updated from walkdir
                                 * only while the state carries CH_DIR;
                                 * states that bake their direction into the
                                 * frame sign (A_GO_TO_BALON_L) never touch
                                 * it. Starts +1 = what the frames ask for. */
    int vert_intent;            /* PR-148: the engine's y-intent word,
                                 * [obj+0xbe], mirrored per iteration from
                                 * the held Up/Down keys at the port's
                                 * intent scale (Up = negative, y+ is down:
                                 * scancode 0x248 writes -20 into the input
                                 * block, 0x250 writes +20, reader 0x582f0;
                                 * the scale question is named at
                                 * LL_PLAY_INTENT_FULL). It guards the
                                 * mode-4 (BY_TY) row feed exactly where
                                 * 0x5441b guards on the same word. The
                                 * kind-4 contact-chain override to
                                 * +/-10 toward the partner's y and the
                                 * word's use as a v_y force (0x542cd)
                                 * are PR-149's route and not modelled
                                 * here. */
    unsigned long long sig;   /* PR-88: the lion's signal set, a mirror of
                               * the engine's TEventSet at [obj+0x60] with
                               * the LL_SIG_* bit table. One-tick pulses
                               * (cleared every step): raised by a frame's
                               * `GEN <signal>` tail, by gameplay logic
                               * (K_GEN2 = bit 54+1 on landing), and read
                               * by name conditions. The old gen[3] array
                               * is the same K_GENn pulses at bits 54..56. */
    unsigned long long gsig_last;
                                /* PR-257: the GEN bits raised by the rows
                                 * admitted since the last level tick - the
                                 * level-bus copy of p->sig, consumed and
                                 * cleared by the level loop (ll_level.c's
                                 * gen_bus_raise), never read here. */
    int broke;                  /* AC_BREAK_NEXT pending (frame list just
                                 * completed)                             */
    int broke_prev;             /* PR-527: AC_BREAK_PREV pending - the
                                  * row walk ran BACKWARD (rowdir < 0)
                                  * and stepped off the BOTTOM of the
                                  * list: 0x521a1/0x521a1 `orb $0x8` on
                                  * the mask byte, then the cursor goes
                                  * to count-1 (0x521a5-0x521a9). The
                                  * forward wrap's twin is `broke`
                                  * (bit 2, 0x521c1/0x521c1 `orb $0x4`).
                                  * ZARIA's A_LOOKUP exits only through
                                  * this one: `A_DRAP(1)=AC_BREAK_PREV`. */
    int ac_time;                /* PR-83: AC_TIME pending - raised by a
                                 * frame's `GEN AC_TIME` tail (measured:
                                 * ZARIA's player .MOV names it on 11
                                 * frame lines); consumed by transitions
                                 * next tick. The state time= timer is
                                 * NOT a source yet: unit undetermined.  */
    int atc;                    /* PR-89: the attack-connect WINDOW is    *
                                 * open. The engine's frame-directive     *
                                 * table (0x917c0) carries `atc_on`/      *
                                 * `atc_off`, and the scripts pair them:  *
                                 * measured over the extracted .MOV texts, *
                                 * 267 state blocks open AND close the    *
                                 * window (ZARIA's A_FIGHT: ATC_ON frame  *
                                 * 8 - just before its `FC 100,0` lunge,  *
                                 * ATC_OFF at 13, re-open at 17; the      *
                                 * LICHA's A_ATAK opens at -4, closes at  *
                                 * -8), 18 open without closing. So the   *
                                 * window is a frame flag that STAYS open *
                                 * until a frame closes it; for those 18  *
                                 * it also ends when the state does: this *
                                 * model clears it on every state change  *
                                 * (the engine's own clearing rule is not *
                                 * pinned; an attack that hit forever     *
                                 * after one swipe is not what the video  *
                                 * shows). The window is what lets the    *
                                 * lion's shape land a hit (0x552e0: a    *
                                 * touch runs the push-out and only then  *
                                 * the hit effects, FUN_00055230). */
    int atc_hit;                /* this window has already landed a hit   */
    int auto_walk;              /* PR-83: the current state was entered
                                 * WITHOUT a K_FWD/K_BKWD condition, so its
                                 * BY_X target velocity is not key-gated
                                 * (cutscene walkers: RAN A_GO_TO_BALON_*
                                 * walks on K_GEN1 with no key held)     */
    double air_from;            /* position-point y when ground was lost  */
    int prev_line;              /* PR-272: the contact line (y + contact)
                                 * this body's LAST iteration ended on,
                                 * rounded - the top of the landing sweep.
                                 * The engine's terrain side is a shape-
                                 * box CONTACT (PR-97: hit test 0x42a70,
                                 * push-out 0x42de0): a body cannot pass
                                 * a line that lies between where its
                                 * contact line was and where it is now.
                                 * A point test can model that only by
                                 * sweeping the step: PR-271 measured
                                 * ~25-33 px/tick ZARIA.01 falls crossing
                                 * the main ground (y 682) unseen through
                                 * a 16 px window. */
    int jumping;                /* a row `F`/`I` pair put the lion in the
                                 * air (PR-253's name for that verb) */
    int support;                /* a solid object holds the feet up (set by
                                 * the collision pass, PR-59)              */
    int contact;                /* PR-97: the y BELOW this position point at
                                  * which the animation being shown is solid -
                                  * the deepest shape-box bottom over its
                                  * frame list: ll_seq_shape_bbox's y + h - 2
                                  * (the game's obj+0x0e + obj+0x12 - 2) per
                                  * element, maxed over the state's frames by
                                  * the viewer (ll_level.c mov_state_contact:
                                  * the per-frame value wobbles a few px over
                                  * a cycle, which is the walk cycle's own
                                  * art, not a place to stand).
                                  * The terrain tests are made there: a
                                  * grounded lion stands with his paws on the
                                  * .plt line, not with his position point on
                                  * it. 0 = the animation carries no shape box
                                  * (the game's hit test and push-out skip
                                  * those elements too), which is the old
                                  * position-is-feet behavior. The viewer sets
                                  * it from the state it is showing
                                  * (ll_player_set_contact); ll_player_start
                                  * leaves it 0. */

    /* PR-199: THE EXIT WORD, the port's copy of the engine's field at
     * [level object + 0x4]. The whole path is PR-198's read: an authored
     * `.MOV` row tail `end_level <n>` reaches a node of class 0xa1048,
     * that class's apply method 0x45750 stores n into this slot of the
     * object the global ds:0xcef40 points at (the level-session object),
     * and the level loop 0x4abd0 compares the slot against ZERO at the
     * top of EVERY iteration (0x4ac55) - nonzero sets ebx=1 (0x4ac5b)
     * and jumps to the loop's single epilogue, so the level function
     * returns 1 whatever n was. Two measured facts about the slot's
     * life: the loop body contains NO store to it (zero [reg]/[reg+4]
     * dword stores, zero ds:0xcef40 mentions), and the level ctor
     * 0x4a4c0 zeroes it (0x4a4c3/0x4a4cc) on a freshly allocated 0x3ea
     * block (0x5d1f5). So the flag is never cleared mid-run and cannot
     * leak into the next level: the end lands on the first iteration
     * boundary after the row ran.
     * THE MAPPING, stated rather than hidden: the engine's slot is ONE
     * PER LEVEL OBJECT; the port's owner here is the lion's own state,
     * because the lion's row walk is the only one the shipped data puts
     * the directive in - all 26 rows are in `LEW.MOV` members, in three
     * of them only (JUNCTION A_TUNGA1/A_TUNGA2, TUNGA.11 A_EXIT,
     * TUNGA2.12 A_EXIT; measured by verify-endlevel over the 777 .MOV
     * members of the HD ANIMACJE). A SCENE OBJECT's row cannot reach
     * this field and stays counted (no shipped script does it today).
     * ll_level.c's play leg reads the field at the top of its iteration
     * - the 0x4ac55 analogue - and hands code 1 to ll_stage_outcome. */
    int exit_word;              /* 0 = no authored end_level has fired  */
} ll_player;

enum {                                              /* ll_player_step ev */
    LL_PLAY_EV_STATE  = 1,                          /* state transition   */
    LL_PLAY_EV_GROUND = 2,                          /* landed             */
    LL_PLAY_EV_AIR    = 4                           /* stepped/swept off  */
};

/* Start (or respawn): position point at (x, y); caller resolves the initial
 * ground (ll_plt_first_below) and passes grounded accordingly. Initial
 * state: A_STAY when the .MOV defines it (pinned standing spawn), else
 * the .MOV's declared first state (PR-83: the declared list, not the
 * block order, names the starting state - ZARIA.01), else state 0.
 * p->contact starts at 0 (ll_player_start zeroes the struct); a viewer that
 * drives a real .SEQ must set it every tick - see ll_player_set_contact. */
void ll_player_start(ll_player *p, const ll_mov_t *mov, double x, double y,
                     int grounded);

/* PR-97: tell the sim where the animation it is showing is solid BELOW the
 * position point (the max over its frames of each element's shape box
 * bottom, ll_seq_shape_bbox's y + h - 2; see ll_level.c mov_state_contact).
 * Every terrain test in ll_player_step is made at y + contact;
 * with 0 the position point itself is the feet, as before PR-97. */
void ll_player_set_contact(ll_player *p, int below_pos);

/* One fixed dt_ms tick (CI grid: 20 ms). xmin/xmax clamp the position x
 * (lew_rect). Returns the LL_PLAY_EV_* bitmask of what happened. */
int  ll_player_step(ll_player *p, const ll_plt *ground,
                    const ll_play_keys *keys, double xmin, double xmax,
                    unsigned dt_ms);

/* PR-82: the DRAW mirror of the current frame - the authored frame sign
 * (ll_mov_frame_ref .mirror) XORed with the facing, exactly as
 * ll_level's obj_mirror_of does for scene objects. */
int  ll_player_mirror_of(const ll_player *p, int authored);

/* Current .MOV frame NUMBER of the player (raw, as authored - NOT an element
 * index: feed it through ll_mov_frame_ref). */
int ll_player_frame(const ll_player *p);

/* What a .MOV frame number means (PR-49; the game's own frame-line parser,
 * FUN_00051660, machine code 0x51692..0x516d5): 1-based, the sign is the
 * mirror flag. n > 0: element n-1; n < 0: element -n-1 drawn MIRRORED
 * (frame record +0x14 = 1); n == 0: no element index - the game pulls a
 * placeholder from a global resource (FUN_00044110), which this port does
 * not model, so it draws nothing (blank = 1). Before PR-49 the number was
 * used as a 0-based index and out-of-range numbers were clamped to the last
 * element - 4266 of 18432 authored frame numbers are out of range under
 * that reading, 0 are (but the real blanks) under the game's. */
typedef struct { int idx; int mirror; int blank; } ll_frame_ref;
ll_frame_ref ll_mov_frame_ref(int n);

/* Data-free selftest for CI (ll_level --play-selftest): synthetic .plt +
 * a mini .MOV using the REAL walk constants (F=400, T=90, G=200): walks,
 * hits the T cap, falls off an edge, lands, turns around, stops.
 * 0 = ok, 1 = failed (reasons to stderr). */
int ll_play_selftest(void);

#ifdef __cplusplus
}
#endif
#endif /* LL_PLAY_H */

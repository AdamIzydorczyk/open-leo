/* ll_seq.h - .SEQ animation-script reader for Lew Leon (Longsoft 1997).
 * Format recovered 2026-11-14 from LEO.EXE raw disassembly: the game loads
 * a whole .SEQ record payload from the llrs container into a buffered
 * stream (FUN_00043e70 + FUN_0005c6b0), reads u16 element count, then
 * parses `count` elements (FUN_00041ef0 -> FUN_00041f60, leaf readers
 * FUN_00041d40 / FUN_00041da0 / FUN_00041e00; every stream read goes
 * through vtable slot +0x1c = read(dest, count-BYTES)):
 *
 *   blob    := u16 count; count x element
 *   element := u16 w0                        flag word (low nibble used)
 *              u16 b19, b15, b17             the element's SHAPE frame:
 *                                            total_w = int16(b19)*0x50 +
 *                                            int16(b15), rows = int16(b17)
 *                                            (obj+0x19/0x15/0x17)
 *              u16 f0, f2, f4, f6            obj+0x00..0x06 (f0 = id,
 *                                            0x7fff = ctor sentinel,
 *                                            f0/f2 = the anchor the shape
 *                                            is positioned from)
 *              [u16 f8, u16 fA]              obj+0x08/0x0a, when w0&4
 *              [n B left-margin][n B right-margin]  when w0&3, n =
 *                                            int16(b17), skipped n<=0
 *              (SHAPE geometry, NOT pixels: per-row margins of the
 *               element's solid spans - see the geometry section below.
 *               RE of 2026-11-16: w0&1 = carries patch art, w0&2 =
 *               carries shape; an element never carries neither)
 *              [for i=0..int16(b19): u16 L; L bytes]  unless w0&2 && !(w0&1)
 *
 * This grammar consumes every .SEQ payload the containers carry exactly
 * (consumed == payload size) - measured on all 1656 extractable ones
 * across ANIMACJE, ETAPY and LEVELDES; 4 of them are malformed in the
 * 1997 original itself (tools/verify_seq.py names them and accepts them).
 * Reference: tools/seq_parse.py; make verify-seq checks byte-exact
 * agreement, with the frame-patch rows and the shape geometry of every
 * element (ll_seq_shape_*) cross-checked the same way. The derived fields
 * the game computes at load (obj+0x0c..0x12 bbox, obj+0x14 = w0&8) are
 * NOT part of the blob; FUN_000420b0 recomputes them from parsed data.
 */
#ifndef LL_SEQ_H
#define LL_SEQ_H

#include <stddef.h>
#include <stdint.h>

typedef struct {                 /* one variable-length byte array entry  */
    uint32_t off, len;           /* byte offset into the blob             */
} ll_seq_ent;

typedef struct {
    uint32_t off;                /* element start (the w0 word) in blob   */
    uint16_t w0;
    uint16_t b19, b15, b17;      /* sub-block words                       */
    uint16_t f0, f2, f4, f6;     /* object header words                   */
    uint16_t f8, fA;             /* valid only when (w0 & 4)              */
    int32_t  n;                  /* shape row count, 0 = no shape         */
    uint32_t ml, mr;             /* left-/right-margin row lists, byte
                                  * offsets into the blob, valid n > 0
                                  * (the game's runtime fields +0x27/+0x2f)*/
    uint32_t arr_first, arr_n;   /* window into ll_seq.ents; arr_n = 0 +
                                  * arr_present for empty arrays          */
    uint8_t  arr_present;
} ll_seq_elem;

typedef struct {
    const uint8_t *blob;         /* not owned                             */
    uint32_t len, count;
    uint32_t consumed;           /* == len for all known files            */
    ll_seq_elem *els;            /* malloc'd [count]                      */
    ll_seq_ent  *ents;           /* malloc'd array entries (flat)         */
    uint32_t n_ents, cap_ents;
} ll_seq;

/* Parse a .SEQ blob (payload bytes straight from an llrs id=1 record).
 * 0 = ok, all bytes consistent; -1 = malformed/truncated. Free with
 * ll_seq_free. No side tables: element data stays in the blob. */
int  ll_seq_parse(const uint8_t *blob, uint32_t len, ll_seq *out);
void ll_seq_free(ll_seq *s);

/* --- frame-patch rendering (M4-A, from blitter FUN_0001dc80 raw disasm) ---
 * Entry blobs (ll_seq_ent .off/.len) are length-prefixed sprite rows:
 *   row := (0x80|len) content[len];  content := gap run*
 *   op pos: C < 0x80 copy C bytes | F >= 0x80 fill F&0x7f bytes, 1 value B
 *   marker pos (after any run): B < 0x40 dst gap advance (kind 2, paints
 *   nothing, no dump token) | 0x40..0x7f after COPY run = fill (B&0x3f)
 *   bytes + value byte, after FILL run = copy B&0x3f bytes
 * The game blits rows onto a 400-byte pitch; chunks: entries 0..arr_n-2 are
 * 80 px wide, the last entry is b15 px wide, x cursor += 80. Element screen
 * position = (-f0, -f2) (bbox derived from negated f0/f2, FUN_000420b0).
 *
 * ll_seq_patch_walk decodes one entry blob, invoking cb per row (gap -1 =
 * empty row; ops array valid only during the call). Returns rows decoded,
 * -1 malformed (cb may have seen earlier rows already). Marker bytes seen
 * between ops (FUN_0001dc80 @0x1dd59/0x1dd83): <0x40 = dst gap advance
 * (kind 2; the painter MUST advance px — missing that garbled AUTORZY.SEQ
 * word rendering), 0x40..0x7f alternates fill/copy on the previous run.
 * ll_seq_patch_draw paints the decoded palette indices into an 8bpp canvas
 * (clip to cw/ch). ll_seq_draw_elem paints all chunks of an element with
 * origin (x,y). */
typedef struct {
    int kind;                /* 0 = copy (b), 1 = fill (val), 2 = gap skip */
    const uint8_t *b;        /* copy payload (points into blob) */
    int val;                 /* fill value */
    int cnt;                 /* run length in pixels */
} ll_seq_patch_op;

typedef void (*ll_seq_patch_row_cb)(void *ctx, int row, int gap,
                                    const ll_seq_patch_op *ops, int nops);

/* TWO row codecs exist (PR-48; the game picks one per ELEMENT from w0&8,
 * loader FUN_00041f60 stores obj+0x14 = w0&8):
 *   LL_SEQ_CODEC_DC80  w0&8 set   FUN_0001dc80 - the grammar above
 *   LL_SEQ_CODEC_D9B8  w0&8 clear FUN_0001d9b8 - row content is
 *     gap (cnt bytes[cnt] gap)*: any byte < 0x80 in a gap position is a
 *     plain advance (0x40..0x7f are LONG gaps, not markers), counts are
 *     plain copy lengths, NO fill runs. Only 106 elements use it (all
 *     w0 = 1: the player's LEW*.ZAR/I.SEQ and the BERLO map cursor), but
 *     decoding them with the other codec shifts pixels (48 of those 106
 *     blobs differ). ll_seq_elem_codec says which one an element uses;
 *     ll_seq_draw_elem* apply it. ll_seq_patch_walk (no element known) is
 *     the DC80 default - safe for counting rows, which both codecs frame
 *     identically through the length byte. */
#define LL_SEQ_CODEC_DC80 0
#define LL_SEQ_CODEC_D9B8 1
int ll_seq_elem_codec(const ll_seq_elem *e);
int ll_seq_patch_walk_codec(const uint8_t *blob, uint32_t len, int codec,
                            ll_seq_patch_row_cb cb, void *ctx);
int ll_seq_patch_draw_codec(const uint8_t *blob, uint32_t len, int codec,
                            uint8_t *canvas, int cw, int ch, int x, int y,
                            const uint8_t *map);
int ll_seq_patch_walk(const uint8_t *blob, uint32_t len,
                      ll_seq_patch_row_cb cb, void *ctx);
int ll_seq_patch_draw(const uint8_t *blob, uint32_t len, uint8_t *canvas,
                      int cw, int ch, int x, int y);
/* PR-49: the game's own placement convention (see ll_seq.c): the position
 * is the OBJECT's, the element's anchor f0/f2 is subtracted (0x7fff = none),
 * mirror != 0 draws the image flipped across its width tw. ll_seq_draw_elem
 * below stays the raw "origin = (x,y)" draw (menu/help callers compute their
 * own position). */
void ll_seq_anchor_origin(const ll_seq_elem *e, int x, int y, int mirror,
                          int *ox, int *oy);
/* PR-357: the element's OWN land and leave methods, 0x42980 and 0x429d0,
 * slots +0x04 and +0x08 of the block 0xa0c08 that every element record
 * carries at its last field (+0x33: the record is 0x37 bytes). Both work
 * on a POSITION PAIR - the engine passes edx = the object, whose
 * [obj+0x00]/[obj+0x04] is its live position, or a copy of that pair.
 * With f0 = 0x7fff (the sentinel the element initializer stamps at 0x41ef3)
 * neither method writes anything. Otherwise the pair is moved by
 * (f4-f0, f6-f2), the vector from the element's FIRST anchor to its
 * SECOND, the X leg taken the other way when facing <= 0, and
 * ll_seq_anchor_leave undoes exactly what ll_seq_anchor_land did. facing
 * is the sign the caller supplies: [obj+0x6d] (the authored `ldir`/`rdir`
 * word) in the class +0xB4/+0xB8 verbs, the +0x50 verb's answer in the
 * other six call sites.
 * NOT WIRED INTO THE RUNTIME PATH YET. PR-360 measured the shipped data:
 * 98.7% of elements carry an anchor, half of them spell (f4,f6) ==
 * (f0,f2) so this pair is a deliberate no-op, and the rest carry a real
 * step - through ZARIA.01's own scripts the lion's A_WALK frames all
 * give (0,-31), i.e. landing would step his position down 31 px and
 * leaving back. Leg L16 pins the law; what stops the wiring is OI-129
 * (the port is green on the fixture legs WITHOUT this offset, so which
 * of the three statements is wrong has to be measured at a landing
 * instant) and the fact that the engine's argument is the current
 * frame's element through [view+0x48], which the port does not track
 * per object. */
void ll_seq_anchor_land(const ll_seq_elem *e, int facing, int *x, int *y);
void ll_seq_anchor_leave(const ll_seq_elem *e, int facing, int *x, int *y);
int ll_seq_draw_elem_anchor(const ll_seq *s, const ll_seq_elem *e,
                            uint8_t *canvas, int cw, int ch, int x, int y,
                            int mirror, const uint8_t *map);
int ll_seq_draw_elem(const ll_seq *s, const ll_seq_elem *e,
                     uint8_t *canvas, int cw, int ch, int x, int y);

/* *_map: identical blit, but every palette index the sprite paints goes
 * through map[256] before it reaches the canvas (map == NULL = identity,
 * which is what the plain entry points pass). Used to re-tint one sprite
 * along a palette ramp (the menu's selected answer word) without touching
 * the background indices under it. */
int ll_seq_patch_draw_map(const uint8_t *blob, uint32_t len, uint8_t *canvas,
                          int cw, int ch, int x, int y, const uint8_t *map);
int ll_seq_draw_elem_map(const ll_seq *s, const ll_seq_elem *e,
                         uint8_t *canvas, int cw, int ch, int x, int y,
                         const uint8_t *map);

/* --- element SHAPE geometry (M4-C; RE of 2026-11-16 from the full
 * decompilation in build/decompiled/all: FUN_000420b0 (its box math),
 * FUN_00042a70 (hit test), FUN_00042de0 + FUN_00042cf0 (push-out),
 * FUN_00041f60 (the loader's branch), FUN_000437a0 (the mirror blitter)).
 * An element that carries w0&3 lists its shape as per-row margins: row i
 * (i = 0..rows-1) is solid from x = left[i] to x = total_w - right[i]
 * inside the element's own frame, where total_w = int16(b19)*0x50 +
 * int16(b15) and rows = int16(b17). The byte 0xFF means "empty here":
 * the span tests skip a row when either LEFT margin is 0xFF, and the box
 * math skips only the run of 0xFF at the end it scans from - an interior
 * 0xFF takes part in its minimum as the value 255, exactly as the game
 * computes it. An element WITHOUT lists is the implicit full rectangle:
 * the loader takes that branch when w0&3 == 0 and derives the rect
 * straight from b15/b19/b17.
 * What consumes the lists: the element hit test (FUN_00042a70, per-row
 * span overlap between two positioned elements), the push-out
 * (FUN_00042de0 + FUN_00042cf0), the box math (FUN_000420b0) and the
 * mirror blitter (FUN_000437a0, which per row copies half of
 * total_w - left - right). None of them decodes pixels - the art comes
 * from the element's patch array - so a shape-only element (w0&2 without
 * w0&1) was NEVER drawn by the original game either: it is an invisible
 * solid. The "inline palette+sprite" wording PR-45 shipped for those 169
 * tables was wrong on both counts, and the reports say so now.
 * Positioning, identical at both call sites: the caller's position is
 * shifted by the element's anchor - x -= f0, y -= f2, BOTH skipped when f0
 * holds the 0x7fff ctor sentinel - and with facing < 0 the x
 * shift is (total_w - f0) instead (none for the sentinel) and the two margin
 * lists swap (the
 * element is mirrored). Row i of the shape then sits at that y + i. The hit
 * test and the push-out both begin by testing the element's margin-list
 * pointer, so an element WITHOUT lists (the loader's implicit full-rectangle
 * case) takes part in NEITHER of them: no overlap, no push - it has a rect,
 * no shape. Same when its row count is not positive.
 * The functions below take element words plus caller-frame positions and
 * reproduce the game's arithmetic statement for statement; no read ever
 * leaves the declared rows, and the loops' bounds are reproduced exactly -
 * including the right-margin walk, which runs off the top of the rows
 * when every row there is empty.
 * One measured degenerate case, ported as the game computes it (over all
 * 14306 shape elements on these containers, 2026-11-16): 34 declare rows
 * with BOTH margin lists entirely 0xFF, so both scans come out of the rows
 * having found nothing and the box math stores w = 1 - total_w,
 * h = 1 - rows - plus the game's own h off-by-one over the row range it
 * scanned, which applies everywhere (h = (last_row - first_row) + 2).
 * ll_seq_shape_empty() names the all-empty case so the C-vs-python
 * cross-check is known to cover it. */

/* int16(b19)*0x50 + int16(b15) and int16(b17): the implicit shape of an
 * element WITHOUT lists, and the frame of the lists of one WITH them.
 * *n is the row count (<= 0 = none). Always defined. */
void ll_seq_shape_wh(const ll_seq_elem *e, int *w, int *h, int *n);

/* FUN_000420b0 verbatim: element box in element-local coordinates for a
 * shape-bearing element (f0/f2 sentinel handling included; w is the
 * inclusive span its scans cover and h is one row MORE than those rows -
 * the game's own arithmetic, reproduced). With no shape lists the loader
 * uses the implicit rect instead (FUN_00041f60): x = f0==0x7fff ? 0 : -f0,
 * y = f2==0x7fff ? 0 : -f2, w/h from b19/b15/b17; do not call this then. */
void ll_seq_shape_bbox(const ll_seq *s, const ll_seq_elem *e,
                       int *x, int *y, int *w, int *h);

/* 1 when the element declares margin rows but EVERY margin byte of BOTH
 * lists is the empty marker: no solid span at all (34 of the 14306 shape
 * elements here, and no element with only ONE list empty). Not a data
 * gap; it is the input of the second degenerate case above, and the flag
 * lets the cross-check show that those lines agree on both sides. */
int ll_seq_shape_empty(const ll_seq *s, const ll_seq_elem *e);

/* PR-97: the LAST solid row of the shape box, measured below the position
 * point the element is drawn from = ll_seq_shape_bbox's y + h - 2 (the
 * game's obj+0x0e + obj+0x12 - 2). This is the row where the sprite is
 * solid on the ground side, and the frame anchors of an animation are
 * authored to keep it at ONE fixed offset through the whole cycle: ZARIA
 * LEW.ZAR WALK.SEQ holds 30..32 across its 12 frames while f2 stays 31,
 * LEW.ZAR END.SEQ holds exactly 58 across 30 frames while f2 climbs from 5
 * to 54, ZARIA's worm WAZ.SEQ holds 46..49 while f0 swings -2..-22 and f2
 * -9..-16. It is NOT the frame anchor: LEW.RAN/UJ09.SEQ (the walk cycle the
 * RAN.01 lion shows) keeps f2 = 45 on every frame while this value is only
 * 10..14 - an anchor-based contact would leave that lion 33 px in the air.
 * Callers that place an ACTOR use the value of its whole animation (the max
 * over the frames its current state can show: ll_level.c mov_state_contact),
 * because this per-element value wobbles a few px over a cycle (planted paw
 * vs lifted paw) and the frame cursor is not a property of the ground.
 * 0 when the element carries no margin lists (no shape: the game's hit test
 * and push-out skip those too) or when the box math comes out negative (the
 * all-empty-rows degenerate case above). */
int ll_seq_shape_contact(const ll_seq *s, const ll_seq_elem *e);

/* FUN_00042a70: do the positioned shapes overlap? 1/0. Positions are the
 * caller-frame x/y the game passes to its wrappers (the f0/f2 anchor shift
 * and the facing mirror happen inside, exactly as in the game). The row
 * window is the game's: the element lower ON SCREEN (the numerically larger
 * adjusted y, y grows downward) is read from row 0, the other one from the distance between them, over the shorter
 * remaining span (0x42b85..0x42be6; see shape_window in ll_seq.c, where a
 * transposed version of exactly this was fixed on 2026-11-16). A row takes
 * part only when neither element's left margin there is the empty marker;
 * the first row whose spans overlap in x answers yes. */
int ll_seq_shape_hit(const ll_seq *s,
                     const ll_seq_elem *a, int ax, int ay, int afacing,
                     const ll_seq_elem *b, int bx, int by, int bfacing);

/* FUN_00042de0 + FUN_00042cf0: the push-out between two positioned shapes
 * (PR-58; operands resolved from the caller FUN_00053030 and the element
 * vtable slots, see the comment on the definition in ll_seq.c). Returns the
 * penetration (negative when B is to the left of A) and the (dx, dy) push in
 * out1 and out2; 0/0/0 when the shapes do not overlap or an element carries
 * no shape. A = the element that is moved. */
int ll_seq_shape_push(const ll_seq *s,
                      const ll_seq_elem *a, int ax, int ay, int afacing,
                      const ll_seq_elem *b, int bx, int by, int bfacing,
                      int *out1, int *out2);

/* the paired form: a's margin lists are read from sa, b's from sb. Use it
 * whenever the two elements come from different .SEQ records (the lion's
 * art against an object's art): in the single-payload form the element
 * offsets of the other record read the WRONG bytes. A same-record pairing
 * behaves exactly the same through it. See the note at the definition. */
int ll_seq_shape_push2(const ll_seq *sa,
                       const ll_seq_elem *a, int ax, int ay, int afacing,
                       const ll_seq *sb,
                       const ll_seq_elem *b, int bx, int by, int bfacing,
                       int *out1, int *out2);

#endif /* LL_SEQ_H */

/* ll_level.c - shared level-scene composer (see ll_level.h).
 *
 * Resolves a level's plain-text wiring, then composites its gameplay scene
 * exactly the way the data says the game does it:
 *   background = first tlo= layer that exists in the level's ETAPY block
 *                (a .PCX: 8bpp indexed + embedded VGA palette);
 *   objects    = ANIMACJE/<level>/<LEAF> leaf records: each id=1 .SEQ is one
 *                object's frame-patch sprite script (ll_seq.c blitter);
 *   motion     = the matching HD-install .MOV state machine (same record
 *                key, e.g. ED000002.SEQ art <-> ED000002.MOV script):
 *                per state a frame list "<frame>,<period ms>[, M dx,dy]" -
 *                the game walks states in transition order, each frame line
 *                moves the object by its delta. The CD containers carry the
 *                .SEQ art, the SETUP-installed HD containers the .MOV text
 *                (found via LEO_HD_DIR=<dir with ANIMACJE/ETAPY>), never the
 *                other way round: keys join them 1:1.
 *   placement  = sub-.des files ("TYPE,name,mov,X,Y" lines) exist only for
 *                some levels; object X,Y join needs SETUP's mov->ED key
 *                mapping, still open (PLAN.md). Until then sprites anchor by
 *                their element header (x=|f0|, tall sprites bottom-aligned).
 * PCM payloads are TRANSPARENT overlay image layers (PR-53: column strips
 * of compiled-sprite tiles, ll_pcm.h) - NOT audio, and NOT the byte-RLE plane
 * earlier PRs decoded. They join the scene as the tlo= background stack: the
 * first .PCX is the backdrop, every .PCM is painted over it in tlo order;
 * a level without a backdrop .PCX (KADMAR2) shows them over an empty canvas
 * with the block's BACK.PCX palette.
 *   player     = with LEO_PLAY (M4-B, PR-43) the scene additionally runs
 *                the real lion: `lew=` leaf .SEQ art + that leaf's HD
 *                .MOV (physics/transitions), ground from `platformy=`
 *                (a .PLT, or a sub-.des whose text carries the same
 *                polyline grammar), spawn `pozycja=`, bounds `lew_rect=`.
 *                Parsing and physics live in ll_play.c; this file only
 *                loads the data and draws the lion at the .SEQ anchor
 *                (-f0,-f2).
 */
#include "ll_level.h"
#include "fs_ci.h"
#include "ll_pcx.h"
#include "ll_pcm.h"
#include "ll_play.h"
#include "ll_seq.h"
#include "ll_stage.h"
#include "llrs.h"
#include "ll_mod.h"
#include "platform.h"

#include <stdatomic.h>  /* pump-thread stop flag (portable C11; the
                         * thread itself is plat_thread_start, T-01).
                         * This file contains no SDL: the queue-only
                         * audio contract and its samples=0 rationale
                         * live with the SDL backend (ported/ll_audio.h,
                         * ported/platform_sdl.c). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* strncasecmp for container record names */

/* Music audio (E-09): the device is plat_audio_open (queue-only, s16
 * interleaved - same call the cutscene player uses); the queue is kept
 * >= ~250 ms by a dedicated pump thread (plat_thread_start) so window
 * drags (Windows modal loop), slow loads and screen delays never
 * underrun it - the old in-loop pump did. When the platform has no
 * threads (headless dummy backend) plat_thread_start fails cleanly and
 * the UI loops keep topping the queue via ll_music_pump (the pre-E-09
 * path); both paths produce the same bytes. */

/* ---- standalone music playback (menu/map screens), singleton ---- *
 * Engine anchor: FUN_0005dec0 opens+plays "mapka.mod" for the MAPKA
 * (level-select) screen; FUN_0005df20 stops it on every exit path.
 * Same SOUND container + ll_mod player as the level path. */
static llrs s_mus_snd;
static unsigned char *s_mus_img;
static ll_mod s_mus;
static ll_mod_state s_mst;
static int s_mus_aopen;                /* platform audio device is ours */
static int s_mus_snd_open;
static FILE *s_mus_wf;                 /* LEO_MUSIC_WAV diagnostic dump */
static char s_mus_name[32];
static int s_mus_thr;                  /* plat_thread running (E-09)   */
static atomic_int s_mus_quit;

/* E-09: while it lives this thread owns s_mst/s_mus_wf/the audio
 * queue (ll_music_stop joins it before touching any of them,
 * ll_music_play stops first), so no lock is needed. Queue kept
 * >= ~250 ms with 125 ms chunks (old in-loop low-water was 12000 B
 * ~= 68 ms, review E-09). */
static int music_pump_thread(void *unused)
{
    (void)unused;
    static int16_t buf[(LL_MOD_RATE / 8) * 2];      /* 125 ms stereo s16 */
    const size_t low_water = (size_t)LL_MOD_RATE;   /* 250 ms bytes */
    while (!atomic_load(&s_mus_quit)) {
        if (plat_audio_queued_bytes() < low_water) {
            size_t frames = LL_MOD_RATE / 8;
            ll_mod_mix(&s_mst, buf, frames);
            if (s_mus_wf) {
                fwrite(buf, 2 * sizeof(int16_t), frames, s_mus_wf);
                fflush(s_mus_wf);       /* CI hook: LEO_MUSIC_WAV stays
                                         * tail-able while the game runs */
            }
            plat_audio_queue(buf, frames * 2 * sizeof(int16_t));
        }
        plat_sleep_until(plat_now_ns() + 25000000ull); /* first pass runs
                                         * immediately, so even very short
                                         * plays get their first chunk */
    }
    return 0;
}

/* first 125 ms chunk mixed+queued synchronously (play context): the
 * thread start races ll_music_stop in very short plays, and the old
 * in-loop pump always produced its first chunk before anything else
 * could touch the state - LEO_MUSIC_WAV consumers rely on >=1 chunk
 * per play. */
static void music_first_chunk(void)
{
    static int16_t buf[(LL_MOD_RATE / 8) * 2];
    size_t frames = LL_MOD_RATE / 8;
    ll_mod_mix(&s_mst, buf, frames);
    if (s_mus_wf) {
        fwrite(buf, 2 * sizeof(int16_t), frames, s_mus_wf);
        fflush(s_mus_wf);
    }
    if (s_mus_aopen)
        plat_audio_queue(buf, frames * 2 * sizeof(int16_t));
}

/* the LEO_MUSIC_WAV diagnostic: one raw file per play */
static void music_wav_open(void)
{
    static unsigned seq;
    char path[512];
    snprintf(path, sizeof path, "%s.%02u.%s.raw",
             getenv("LEO_MUSIC_WAV"), seq++, s_mus_name);
    s_mus_wf = ll_fopen_ci(path, "wb");
}

int ll_music_play(const char *data_dir, const char *modname)
{
    if (getenv("LEO_NO_MUSIC"))
        return 1;
    ll_music_stop();
    snprintf(s_mus_name, sizeof s_mus_name, "%s", modname);
    char p[4096];
    snprintf(p, sizeof p, "%s/SOUND", data_dir);
    if (llrs_open(&s_mus_snd, p) != 0)
        return 1;
    s_mus_snd_open = 1;
    const llrs_slot *sl = llrs_find(&s_mus_snd, modname);
    long n = sl ? llrs_read(&s_mus_snd, sl, &s_mus_img) : -1;
    char err[96] = "";
    if (n <= 0 || ll_mod_open(&s_mus, s_mus_img, (size_t)n, err,
                              sizeof err) != 0) {
        printf("ll_music: %s: %s\n", modname, err[0] ? err : "missing");
        ll_music_stop();
        return 1;
    }
    ll_mod_start(&s_mst, &s_mus);
    /* Queue-only device (samples=0 contract + accepted-spec log P-13
     * enforced inside the backend; the samples A/B probe results are
     * documented at ported/ll_audio.h): -1 = audio subsystem
     * unavailable -> play silent, exactly like the old silent
     * ll_audio_session_init failure path; -2 = subsystem up, device
     * failed -> keep the old diagnostic. */
    int arc = plat_audio_open(LL_MOD_RATE, 2);
    if (arc == -2) {
        fprintf(stderr, "ll_music: device: %s\n", plat_last_error());
        ll_music_stop();
        return 1;
    }
    if (arc != 0) {
        ll_music_stop();
        return 1;
    }
    s_mus_aopen = 1;
    /* E-09: dedicated pump thread; if the platform cannot give us one
     * we fall back to ll_music_pump() being called from the UI loops
     * (the pre-E-09 behaviour). */
    if (getenv("LEO_MUSIC_WAV"))
        music_wav_open();
    music_first_chunk();
    atomic_store(&s_mus_quit, 0);
    if (plat_thread_start(music_pump_thread, NULL, "ll_music_pump") == 0)
        s_mus_thr = 1;
    else
        printf("ll_music: pump thread failed (%s), using UI-loop pump\n",
               plat_last_error());
    printf("ll_music: playing %s: '%s' %upos %uuniqpat %uch\n", modname,
           s_mus.title, s_mus.nseq, s_mus.npat_stored, s_mus.nch);
    return 0;
}

void ll_music_stop(void)
{
    if (s_mus_thr) {                    /* join first: the thread owns
                                            s_mst/s_mus_wf/audio queue */
        atomic_store(&s_mus_quit, 1);
        plat_thread_join();
        s_mus_thr = 0;
    }
    if (s_mus_wf) {
        fclose(s_mus_wf);
        s_mus_wf = NULL;
    }
    if (s_mus_aopen)
        plat_audio_close();
    s_mus_aopen = 0;
    ll_mod_close(&s_mus);
    free(s_mus_img);
    s_mus_img = NULL;
    if (s_mus_snd_open)
        llrs_close(&s_mus_snd);
    s_mus_snd_open = 0;
}

/* Fallback queue top-up (pre-E-09 path): only used when the pump
 * thread could not be created; the UI loops keep calling it blindly.
 * No-op when no music device is open or the thread is running. */
void ll_music_pump(void)
{
    if (!s_mus_aopen || s_mus_thr)
        return;
    if (plat_audio_queued_bytes() > 12000)
        return;
    static int16_t buf[(LL_MOD_RATE / 8) * 2];      /* 125 ms stereo s16 */
    size_t frames = LL_MOD_RATE / 8;
    ll_mod_mix(&s_mst, buf, frames);
    if (s_mus_wf)
        fwrite(buf, 2 * sizeof(int16_t), frames, s_mus_wf);
    plat_audio_queue(buf, frames * 2 * sizeof(int16_t));
}

int ll_level_names(const char *data_dir, char names[][32], int max)
{
    char p[4096];
    snprintf(p, sizeof p, "%s/ETAPY", data_dir);
    llrs et;
    memset(&et, 0, sizeof et);
    if (llrs_open(&et, p) != 0)
        return 0;
    int n = 0;
    for (unsigned k = 0; k < et.cap && n < max; k++) {
        if (et.slots[k].id != 2)
            continue;
        char key[15] = "";
        memcpy(key, et.slots[k].key, 14);
        snprintf(names[n], 32, "%s", key);
        n++;
    }
    llrs_close(&et);
    return n;
}

typedef struct { char key[32], val[256]; } des_kv;

/* case-insensitive compare */
static int ci_eq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y)
            return 0;
    }
    return *a == *b;
}

/* chars before the first '.' (level dir 'RAN.01' -> stem 'RAN') */
static void stem_of(const char *s, char *out, size_t n)
{
    size_t k = 0;
    while (s[k] && s[k] != '.' && k + 1 < n) {
        out[k] = s[k];
        k++;
    }
    out[k] = '\0';
}

/* find printable-name slot, then fall back to stem match ('RAN.01'->'RAN') */
static const llrs_slot *find_or_stem(const llrs *c, const char *lvl)
{
    const llrs_slot *s = llrs_find(c, lvl);
    if (s)
        return s;
    char stem[16];
    stem_of(lvl, stem, sizeof stem);
    for (unsigned k = 0; k < c->cap; k++) {
        char s2[16];
        if (!c->slots[k].id || !c->slots[k].name[0])
            continue;
        stem_of(c->slots[k].name, s2, sizeof s2);
        if (ci_eq(s2, stem))
            return &c->slots[k];
    }
    return NULL;
}

/* value of key `k` in a parsed .DES table ("" if absent) */
static const char *des_get(const des_kv *kvs, int n, const char *k)
{
    for (int i = 0; i < n; i++)
        if (ci_eq(kvs[i].key, k))
            return kvs[i].val;
    return "";
}

/* Parse the "key=value" .DES text (CRLF); returns lines filled.
 * `len` bounds the scan: container payloads are not NUL-terminated. */
static int des_parse(const char *txt, size_t len, des_kv *kvs, int max)
{
    int n = 0;
    const char *p = txt, *end = txt + len;
    while (p < end && *p && n < max) {
        const char *eol = p;
        while (eol < end && *eol && *eol != '\n' && *eol != '\r')
            eol++;
        const char *eq = memchr(p, '=', (size_t)(eol - p));
        if (eq) {
            size_t kl = (size_t)(eq - p), vl = (size_t)(eol - eq - 1);
            if (kl < sizeof kvs[n].key && vl < sizeof kvs[n].val) {
                memcpy(kvs[n].key, p, kl);
                kvs[n].key[kl] = '\0';
                memcpy(kvs[n].val, eq + 1, vl);
                kvs[n].val[vl] = '\0';
                n++;
            }
        }
        p = eol;
        while (*p == '\n' || *p == '\r')
            p++;
    }
    return n;
}

/* Lists the level's ANIMACJE animations: the level's id=2 block holds
 * id=1 scripts (SEQ cutscene scripts) and id=2 leaf tables keyed by
 * object name (SCHODY, CAMEL...), each leaf a mini index whose id=1
 * records are that object's animation-frame .SEQ scripts. */
int ll_level_anims(const char *dir, const char *level)
{
    char p[4096];
    llrs an;
    snprintf(p, sizeof p, "%s/ANIMACJE", dir);
    if (llrs_open(&an, p)) {
        printf("ll_level: no ANIMACJE container\n");
        return 1;
    }
    const llrs_slot *blk = find_or_stem(&an, level);
    if (!blk) {
        printf("ll_level: no ANIMACJE block for '%s'\n", level);
        llrs_close(&an);
        return 1;
    }
    llrs_slot *recs = NULL;
    unsigned nrec = 0;
    if (llrs_block(&an, blk, &recs, &nrec)) {
        llrs_close(&an);
        return 1;
    }
    printf("ll_level: ANIMACJE '%s' animations:\n", blk->name);
    for (unsigned k = 0; k < nrec; k++) {
        if (!recs[k].id)
            continue;
        const char *nm = recs[k].name[0] ? recs[k].name : "<binary-key>";
        if (recs[k].id == 1) {
            printf("  script  %-14s %5u B\n", nm, recs[k].size);
        } else if (recs[k].id == 2) {
            llrs_slot *fr = NULL;
            unsigned nfr = 0;
            if (llrs_block(&an, &recs[k], &fr, &nfr) == 0) {
                unsigned nf = 0;
                char first[96] = "";
                for (unsigned j = 0; j < nfr; j++)
                    if (fr[j].id == 1) {
                        if (nf == 0)
                            snprintf(first, sizeof first, "%s",
                                     fr[j].name[0] ? fr[j].name : "?");
                        nf++;
                    }
                printf("  object  %-14s %2u frame scripts (first: %s)\n",
                       nm, nf, nf ? first : "-");
                llrs_block_free(fr);
            }
        }
    }
    llrs_block_free(recs);
    llrs_close(&an);
    return 0;
}

/* ---------------- .MOV object state machine -------------------------------
 * The parser and structs moved to ll_play.c (PR-43, ll_mov_*): IDENTICAL
 * frame semantics for scene objects (a frame number is 1-based and its sign
 * is the mirror flag - ll_mov_frame_ref, PR-49; non-positive durations clamped to 100 ms = the M4-C rule),
 * plus the player-facing extension documented in ll_play.h (F=/T=/G=
 * physics, TARGET(n)=COND transitions, control= flags, per-state seq=).
 * Scene objects read that too: control=BY_X states WALK at F/4 px/s and
 * their frame lists' second field counts movement units - that slot is
 * authored BOTH ways across the shipped scripts, and only BY_X states
 * read it as units (PR-79; basis re-labelled 2026-11-16, the model
 * stands: see obj_walk below). Everything else keeps the frame-list
 * time semantics through obj_advance/obj_elem. */

/* ---------------- scene objects (PR-45: the authored scene) ------------
 * A level's own placement data (the sub-.des files its main .des names
 * through zwierzaki= / animacje= / ani_plat=) says which object scripts
 * stand where: "<LEAF>,<class>,<script>.mov,<x>,<y>". The first field
 * does double duty: the engine reads it with `' %[^,\r\n] , '` and
 * hands it to its object-class factory 0x4df10, a chain of
 * case-insensitive name tests (0x76a69) that constructs one of 163
 * class-table pairs by that name - ANIBACK, ZWIERZ, POCISK, the
 * per-level groups, and a TLew<level> lion class for each stage
 * (NEXT_STEPS PR-103). The script is the
 * HD ANIMACJE record of that exact name (ANIMACJE/<level>/<LEAF>/
 * <SCRIPT>.MOV), and the ART is whatever that script selects for the
 * state it is in: every [STATE] block carries "seq=<name>", naming
 * ANIMACJE/<level>/<LEAF>/<NAME>.SEQ on the CD (measured over the 708
 * distinct (level, leaf, script) triples the 2188 placement entries use,
 * PLAN 8g - NOT the .mov stem, which matches one of its own seq= names in
 * only 439 of the 681 scripts that exist).
 * So the model is: one obj_script per distinct (<LEAF>,<SCRIPT>.MOV) the
 * level references (art + parsed script loaded ONCE), and one lvl_obj per
 * placement (RAN.01 places the same 2 cloud scripts 184 times). Nothing
 * is instantiated that the level's data does not name. */

#define OBJ_MAXPTS 8       /* authored contact points one placement names.
                            * Measured: every one of the 21 shipped lists
                            * (PR-243 census of hd_files/ETAPY) authors a
                            * count of 1; the array is wider so an
                            * over-run is counted, never dropped silently. */
#define OBJ_MAXART 16      /* distinct seq= names one .MOV selects; 10 is
                            * the measured maximum (PLAN 8g) */
#define OBJ_MAXSEAT 24     /* PR-335: surface records one entry's OWN
                            * surface table authors (0x45120's record
                            * array). Measured over the 103 .des members
                            * dumped from the shipped ETAPY: the widest
                            * line of this shape is LONA.05 ANI3's `23
                            * 999,354;...`: its count asks 24 records,
                            * the line holds 23 pairs, and it sits
                            * after a `SOUNDSOURCE,` line, so no record
                            * verb owns it. The widest table this reader
                            * DOES reach is TUNGA.11 ANIPL1's `3
                            * 0,0;30,0;63,-1;135,10;`, 4 records. So
                            * nothing the reader reaches is cut; an
                            * over-run is still counted. */

typedef struct {           /* one .SEQ art table, owned by its script    */
    char name[24];                    /* the seq= name, .SEQ form        */
    unsigned char *blob;
    ll_seq seq;                       /* parsed (owns the element table) */
    int found;                        /* 0 = the record is not on these
                                       * containers (a data hole)        */
    int eidx0;                        /* first element with patch art; -1
                                       * with found=1 = the table carries
                                       * NO patch art at all, only shape
                                       * geometry (w0&2 without w0&1). The
                                       * game's own draw paths all gate on
                                       * its patch pointer array, so such a
                                       * table was never drawn THERE either
                                       * - invisible, and drawn as nothing
                                       * here too (RE 2026-11-16, PLAN 8h) */
    int sw, sh;                       /* sprite bbox (element chunks)    */
    int rx, ry;                       /* worst-case draw reach around the
                                       * object position over ALL elements
                                       * (anchor + width, both facings)   */
    /* the table's SHAPE facts (measured from its own elements, never
     * invented): the first element that carries the per-row margin lists
     * (the game's hit/push geometry, ll_seq_shape_*). */
    int sidx0;                        /* first shape-bearing element, -1  */
    int nshape;                       /* elements with margin lists       */
    int stw, srows;                   /* that element's shape frame      */
    int sbx, sby, sbw, sbh;           /* ... its FUN_000420b0 bbox      */
    int nomargins;                    /* 1 = that element declares rows
                                       * whose margin lists are BOTH wholly
                                       * the empty marker: the game's own
                                       * box math stores w = 1 - total_w,
                                       * h = 1 - rows there (arithmetic,
                                       * not a data gap)                */
} obj_art;

typedef struct {                     /* one authored object script       */
    char leaf[16], movname[24];      /* exactly as the placement names it */
    unsigned char *movtxt;           /* owned, or NULL                    */
    ll_mov_t *mov;                   /* owned, NULL = no script on these
                                      * containers (a real data hole)     */
    obj_art art[OBJ_MAXART];         /* distinct seq= art, deduped       */
    int nart;
    int ast[LMOV_MAXST];             /* state -> art index, -1 = missing */
    int artmiss[OBJ_MAXART][24];     /* seq= names not on the CD, once   */
    int nartmiss;                    /* ... counted                      */
    int nartshape;                   /* art tables carrying shape only    */
    int nartcarry;                   /* ... art AND shape (w0&1|w0&2)    */
    int nartover;                    /* seq= names beyond OBJ_MAXART     */
    int ninst;                       /* placements using this script     */
    /* what the script authors but the scene model does not act on yet
     * (counts only: reported, never invented; see PLAN 8g) */
    int ntrans, nflag_other, ndirect, nfield_other;
} obj_script;

typedef struct {                     /* one placement of a script        */
    obj_script *sc;
    int bx, by;                      /* authored anchor                  */
    int x, y;                        /* current (anchor + authored M)    */
    int sti, fi;                     /* state / frame cursor             */
    unsigned row_t, row_rem;     /* PR-110: the row clock, 1/100-iteration
                                  * units + the iteration carry (mirrors
                                  * [obj+0xa1] and ll_player.row_t; PR-
                                  * 172: the engine's one register is
                                  * zeroed at EVERY state change by the
                                  * record ENTER at [[rec+0x44]+0x1c] -
                                  * see obj_transition and obj_hit) */
    unsigned brk_arm;            /* PR-135: the "row list ended" bit of the
                                  * engine walk: at the wrap it ORs 4 at
                                  * 0x521c1-0x521c5 (PR-162 F1b; reach
                                  * into the key is open). Raised by
                                  * the step that wraps the list, consumed by
                                  * the NEXT step's key composition - never
                                  * matched by the step that armed it.
                                  * ll_player.broke is the lion's copy. */
    unsigned char src;               /* which .des key placed it: 0 zwierzaki,
                                      * 1 animacje, 2 ani_plat (PR-60)    */
    unsigned char layer;             /* the tlo/parallax layer it rides =
                                      * [obj+0x24]: the plane plany= names
                                      * for its animacje= cell (PR-208), or
                                      * the entry index in the key's list for
                                      * the two keys plany= never names      */
    /* PR-68: the key=value lines that follow an entry in the placement
     * file (zwierzaki .des: damage= contact damage, bounds= the patrol x
     * range, scope= detection range, energy= hit points). -1 = absent. */
    int damage, b0, b1, scope, energy;
    /* PR-89: the object's own combat state, mirroring the engine's level
     * object fields (NEXT_STEPS "T2 RE lead" for the whole pipeline):
     * hp = the energy counter at [obj+0xad], initialised from the
     * placement's `energy=`; hp < 0 = not hit-able (the engine writes
     * [obj+0xad] = -1 for its never-dying types at 0x340f5/0x54478 and
     * tests death as `energy < 0` at 0x544a5). hit_cd = the hit
     * countdown at [obj+0x102]: the engine's gate FUN_000502f0 tests
     * it non-zero to drop a hit, and 0x50f67/0x50300 arms exactly 30
     * on EVERY landed hit (PR-139; its own decrement by 1 is 0x50720,
     * run once per state step from 0x5371c).
     * atc = the object's own attack-connect window (PR-90: the ENEMY
     * hits the LION through it, the mirror of the lion's own; PR-139:
     * this is the ONLY path an authored damage= takes - the engine has
     * no unconditional contact damage, 0x50f80's gate at 0x50ff1). */
    int hp, hit_cd;
    /* PR-243: the AUTHORED CONTACT-POINT LIST that follows an entry line
     * in the placement file - the engine's [obj+0xa5] vector. Its
     * kind-table +0x18 verb (0x3f0f0 for the placed DRABINKA) parses
     * `N dx,dy ...` with 0x44f10 into N 12-byte elements (x, y, a
     * class word - PR-305 measured that 0x98274 is NOT that word:
     * the factory installs it at the authored rect's +0x10 =
     * [obj+0xc1], while the list header's own +0x08 holds 0x97f50,
     * the list class whose slots are 0x44e40/0x44ec0/0x44f10), and
     * 0x44e90 immediately ADDS the
     * placement to every point: the authored numbers are offsets and the
     * live values are ABSOLUTE from load and never follow the object
     * (PR-242). The only consumer measured so far is the 0x44ec0
     * ANY-test in front of the ladder's K_GEN1 self-raise (PR-240/241),
     * whose element verb 0x23e00 is a point-in-rectangle test against
     * the ASKER's live rectangle. npts = 0 is the engine's empty vector,
     * which answers FALSE (0x44ec0: the count at [hdr+4] is not > 0). */
    unsigned char npts;
    short cpx[OBJ_MAXPTS], cpy[OBJ_MAXPTS];
    /* PR-246/PR-247: the AUTHORED RECTANGLE the same entry's
     * next line authors (`x0,y0;x1,y1`, offsets from the
     * placement). The engine reads it into [obj+0xb1..0xbd] and
     * makes it ABSOLUTE in the same record verb that reads the
     * points (0x3f0f0's 0x49f90 call, then 0x3f124-0x3f14b adds
     * the placement x,y), with the layout x0,y0,x1,y1 - the port
     * stores the same four in the same order. Its measured uses:
     * the class verb +0x64 (0x23140) intersects it with a
     * LAYER'S ACTIVE WINDOW in the broad phase 0x4b970 and
     * writes that answer into the contact-node field [obj+0x2c];
     * the ladder's state-2 answer copies it into a window for
     * its element test (0x3f256-0x3f29f, PR-244). NOT the ask's
     * candidate filter (that is +0x60, PR-247). nrect = 0 when
     * the entry authors none; the third authored line (`30,50`)
     * is the raw pair, stored ABSOLUTE as apx = x + a, apy = y + b
     * the way 0x3f187-0x3f1a7 makes the engine's [obj+0xc5] block
     * absolute; npair = 1 when the entry authors one. The engine's
     * edge block for such an object is the band x apx +/- trunc(n/2)
     * (n = (arx1-arx0)+1, the widening of 0x4a050), y apy-0x50..apy
     * (PR-270); the port builds it in ladder_answer_for. */
    unsigned char nrect, npair;
    short arx0, arx1, ary0, ary1;
    short apx, apy;
    /* PR-335: the entry's OWN SURFACE TABLE, the continuation line the
     * animated-platform record verbs read through the element method
     * 0x45120 (called at 0x1e51c/0x1e640 with eax = [obj+0xa5], the
     * object's own embedded point object, class word 0x98278). Its
     * grammar is `%d` then, for every record, one ` %c` separator and a
     * `%d,%d` pair (the formats at 0x9182d/0x91830/0x91836): a count of
     * N allocates N+1 13-byte records (0x45149-0x4515a) and the loop
     * runs N times whatever the separator says - the character after
     * record i is record i's FLAG byte (0x451a1-0x451e0: ';' 1, '#' 2,
     * any other character 0), and the LAST record's flag byte is never
     * written (0x451cb ends the loop before storing one). A zero count
     * allocates NO table at all: 0x45141-0x45143 returns after the
     * count. After the loop 0x451e2 expects one literal ';' (0x91834),
     * which is why the shipped `N x,y;..;` lines end clean and ZARIA.01's
     * KATAPULT `1 7,45;40,45 -6300,-1350` leaves its trailing pair with
     * no reader (the only such line in the shipped set). The record verb
     * follows the parse with 0x45200(point, dx = [obj+0], dy =
     * [obj+4]) (0x1e521-0x1e52a), which adds that pair to EVERY record
     * (0x45217-0x4521a), so the table is stored ABSOLUTE like the
     * contact points; the same verb is also the runtime shift - the
     * class +0xa4 verb 0x1e5c0 runs it once per admitted row (0x52341)
     * and once per state change (0x535db), never per tick, with (0,+1)
     * or (0,-1) taken from the state record's ROW CURSOR at +4
     * (0x1e5dd-0x1e5f6, PR-341; PR-335 wrote "per tick" and PR-336
     * "field +4", and PR-341 settled which). PR-342 applies that shift
     * to these tables for the two shipped types whose class carries the
     * verb (PALMA 0x1e5c0, KATAPULT 0x1e690); nothing else acts on the
     * table yet - OI-123 still gates the ask chain. nseat = 0 is the
     * engine's no table. Measured through this reader over the shipped
     * install:
     * 147 lines in 15 .des files of 9 levels, 309 records stored
     * absolute, one line with text after its records (ZARIA.01 ANIPL2's
     * KATAPULT), none over the cap, none with a stale count, none with a
     * zero count. What is NOT read, counted over the 338 lines of this
     * shape in the 103 dumped .des members by the line that stands in
     * front of them: 154 follow an entry line - this reader takes 147
     * and the other 7 hang off entries authored at 0,0, which PR-93
     * does not instantiate (ENA2 PLAT.DES's six EBECZKA barrels,
     * TUNGA2.12 ZWIERZ.DES's GORYL); 150 continue another table line
     * and 3 follow a bare count line, so no record verb owns them
     * (KADMAR2 ANI1's `76` and its 76 `0 x,y;` rows, TUNGA2.12
     * PLATF.DES's `66`, SADES.06 PLATF.DES's `8`); 23 follow a
     * per-entry property line - `bounds=`, `koniec=`, `R=`, `frame=` -
     * and this walk stops at a property line instead of stepping over
     * it; 8 follow a group line (`TRASA`, `SOUNDSOURCE,`). Separators
     * in that text: ';' 467 times, '!' 11 -
     * all of them LONA.05 ANIPL3's CEBRZYK `1 -5,0!67,0` - and '#'
     * nowhere. 6 lines hold one pair FEWER than their count (LONA.05
     * ANI3's `23 ...` and `20 ...`, TUNGA2.12 ZWIERZ.DES's four GORYL
     * patrol paths); none of the 147 read here is one of them. */
    unsigned char nseat;
    short seat_x[OBJ_MAXSEAT], seat_y[OBJ_MAXSEAT];
    unsigned char seat_flag[OBJ_MAXSEAT];
    unsigned char atc;
    unsigned char atc_hit;          /* PR-90: this object's window has    *
                                     * already landed its one hit (mirror  *
                                     * of ll_player.atc_hit, reset exactly *
                                     * like atc on every state change).    */
    unsigned char dead;             /* the engine-forced death (A_DEAD) */
    unsigned long long sig;         /* PR-88: pending signal pulses at the
                                     * LL_SIG_* bit indexes of the engine's
                                     * signal table (was: K_GEN1..4 in bits
                                     * 0..3). The engine's own set at
                                     * [obj+0x60] is a latching TEventSet;
                                     * the raise-a-pulse-consume-it-same-
                                     * tick model (raised as the frame ends,
                                     * tested by the transition checks of
                                     * the same advance) is from PR-69 and
                                     * stays - the engine's clearing rule
                                     * is unresearched and no data depends
                                     * on it yet. */
    /* PR-79: BY_X walking (see obj_walk): fractional-pixel carry, units
     * accumulated toward the next frame, and the patrol direction
     * (0 = never flipped: walks right - PR-81 shows that is what the
     * authored frame sign draws). PR-81: face is the instance FACING set
     * by walking in control=CH_DIR states (the census pairs bounds= with
     * CH_DIR, and matching the art in both polarities the winner flips
     * with the travel direction). frame numbers already carry the
     * authored base facing in their sign (LICHA's art faces left, its
     * walk list is all-negative and the game draws it facing RIGHT);
     * face=-1 adds a second mirror on top. Initialized +1 at placement =
     * no extra mirror, so non-walkers are unaffected. */
    double xa, facc;
    signed char dir, face;
    /* PR-93: 1 = a static image placement (the line named its .SEQ
     * directly; no .MOV, no states, no interaction - and drawn BEFORE the
     * animated objects, which is what the video shows: actors in front of
     * the stone shelves). Every runtime path already treats a placement
     * whose script has no .mov as inert (obj_hit/arm/advance/settle/
     * shape/touch all gate on sc->mov), so the flag drives only the
     * counts, the draw order and the report labels. */
    unsigned char static_img;
    /* PR-163: the engine's AC_TIME self-raiser (PR-159 F2/PR-160 F3 -
     * ENTER 0x52072 + the match arm 0x53626-0x5365c). act_gate = the
     * rolled gate of the CURRENT state ([rec+0xc]: 0 = never; ENTER
     * rolls it from the authored time= pair); act_n = the same-state
     * transition-match counter ([obj+0x9d]: zeroed at ENTER and at the
     * applier's state-change arm 0x535c2); act_r = the rand() LCG state
     * (the engine keeps ONE at 0x76bc6; the port keeps it per-object -
     * deterministic selftests; no shipped content authors time= so no
     * leg observes the difference). */
    unsigned act_gate, act_n, act_r;
    unsigned char act_init;   /* lazy ENTER at first tick = the placement
                               * ctor's ENTER on the initial record
                               * (0x539e6-0x539fb, PR-162 F4) */
} lvl_obj;

typedef struct {                     /* one level's authored scene: the
                                      * scripts its placement data names
                                      * (shared art) + its placements    */
    obj_script **set;                /* heap array of owned scripts     */
    int nsc, capsc;
    lvl_obj *objs;                   /* heap array, grows as needed     */
    int nobj, capobj;
} scene_owner;                       /* no fixed cap: the measured maxima in
                                      * the shipped data are 164 scripts and
                                      * 246 art tables (SADES.06) and 827
                                      * placements (RAN.01) */

typedef struct { int maxrow; } row_count_ctx;

static void row_count_cb(void *ctx, int row, int gap,
                         const ll_seq_patch_op *ops, int nops)
{
    (void)gap; (void)ops; (void)nops;
    row_count_ctx *rc = ctx;
    if (row + 1 > rc->maxrow)
        rc->maxrow = row + 1;
}

/* sprite size of an element: chunks 0..arr_n-2 are 80 px wide, last is b15;
 * height = rows in the patch stream (ll_seq_patch_walk row counter). */
static void elem_size(const ll_seq *s, const ll_seq_elem *e, int *w, int *h)
{
    *w = 0;
    *h = 0;
    for (unsigned k = 0; k < e->arr_n; k++) {
        const ll_seq_ent *en = &s->ents[e->arr_first + k];
        *w += (k + 1 < e->arr_n) ? 80 : (int)(short)e->b15;
        row_count_ctx rc = { 0 };
        ll_seq_patch_walk_codec(s->blob + en->off, en->len,
                                ll_seq_elem_codec(e), row_count_cb, &rc);
        if (rc.maxrow > *h)
            *h = rc.maxrow;
    }
}

/* The old bottom-anchor heuristic (x = |f0|, tall sprites bottom-aligned in
 * the view) is GONE with PR-45: the .des placement coordinates ARE the
 * authored position of the object, so nothing has to be guessed any more.
 * (ll_menu.c keeps its own rule for MENU scripts, which really are drawn at
 * (-f0,-f2).) */

/* The art of the state an object is in: exactly the .SEQ that state names
 * through seq=. If it names none, or names one this install does not carry,
 * the object draws NOTHING - taking another state's table instead would be a
 * guess, and the missing name is already reported once per script. An art
 * table whose elements carry only shape geometry (no patch art at all) IS
 * returned: the draw path skips it through its element index, and the report
 * says why - the original game's draw paths gate on the patch array too, so
 * it drew nothing from such a table either. */
static const obj_art *obj_art_of(const lvl_obj *o)
{
    const obj_script *sc = o->sc;
    if (!sc || o->sti < 0 || o->sti >= LMOV_MAXST)
        return NULL;
    int idx = sc->ast[o->sti];
    if (idx < 0 || idx >= sc->nart)
        return NULL;
    const obj_art *a = &sc->art[idx];
    return a->found ? a : NULL;
}

/* current drawable element of an object (frame number from .MOV through
 * ll_mov_frame_ref: 1-based, negative = mirrored, 0 or out of range = none;
 * the element table is the art the CURRENT state selected, PR-45) */
static const ll_seq_elem *obj_elem(const lvl_obj *o, int *mirror)
{
    const obj_art *a = obj_art_of(o);
    *mirror = 0;
    if (!a || a->seq.count == 0 || a->eidx0 < 0)
        return NULL;
    int n = a->eidx0 + 1;                      /* no script: the first
                                                * drawable element, as its
                                                * 1-based frame number */
    if (o->sc->mov && o->sti >= 0 && o->sti < o->sc->mov->nst) {
        const ll_mov_st *s = &o->sc->mov->st[o->sti];
        if (s->nfr > 0) {                      /* the cycle guard of
                                                * obj_advance can stop with
                                                * the cursor at the end of a
                                                * state: hold the last frame */
            int fi = o->fi < s->nfr ? o->fi : s->nfr - 1;
            n = s->fr[fi].frame;
        }
    }
    /* the game's frame-line grammar (ll_mov_frame_ref): 1-based, the sign is
     * the mirror flag, 0 has no element. A number that names an element
     * this table does not have draws NOTHING - the old clamp to the last
     * element invented art the data never asked for */
    ll_frame_ref r = ll_mov_frame_ref(n);
    if (r.blank || r.idx >= (int)a->seq.count)
        return NULL;
    *mirror = r.mirror;
    return &a->seq.els[r.idx];
}

/* Is this placement worth a draw pass in the view window? With RAN.01's 827
 * cloud placements (a 28800 px world, most of them off screen) the pass must
 * skip them, and skipping must match what ll_seq_draw_elem would do: patches
 * are 80 px column strips drawn right and down from (x,y), so the real
 * extent is the element bbox (elem_size), not a guess. */
static int obj_visible(const lvl_obj *o, int cw, int ch, int cam, int camy)
{
    const obj_art *a = obj_art_of(o);      /* NULL: no art for this state */
    if (!a || a->eidx0 < 0)             /* < 0: shape-only, nothing to draw */
        return 0;
    int x = o->x - cam, y = o->y - camy;
    /* the drawn rectangle is the OBJECT position moved by the element's own
     * anchor and possibly mirrored (ll_seq_draw_elem_anchor): a->rx / a->ry
     * bound that over every element of the table, so the cull cannot drop
     * a frame that would have reached the view */
    if (x - a->rx >= cw || y - a->ry >= ch || x + a->rx < 0 || y + a->ry < 0)
        return 0;
    return 1;
}

/* The draw mirror of a placement: the authored frame sign (obj_elem) plus
 * the facing a CH_DIR walker turned into (PR-81). face=+1 or 0 (never
 * turned, or a non-walker: the placement builder sets +1) draws exactly
 * what the frame line asks for; face=-1 adds one mirror, which is what
 * the video shows - matching the worm art in BOTH polarities, the winner
 * flips with the travel direction (head-right at t=204.4/211.2/212.4,
 * head-left at t=206.9/208.0/213.2) and both polarities appear at
 * x~330 one second apart (t=209.2 vs 210.0): one instance, turned. */
static int obj_mirror_of(const lvl_obj *o, int authored)
{
    return authored ^ (o->face < 0 ? 1 : 0);
}

/* Draw one placement. The authored .des x,y is the OBJECT's position in
 * world coordinates (the PR-36/PR-44 scroll legs pin that: seen at x - cam);
 * the element subtracts its own anchor f0/f2 and a negative frame number
 * mirrors it - the game's element draw methods do exactly that and nothing
 * in the actor adds the anchor back (PR-49, machine code of the four draw
 * methods, ll_seq_draw_elem_anchor). The art is the table the object's
 * CURRENT state selected (obj_art_of). */
static void obj_draw(const lvl_obj *o, unsigned char *canvas, int cw,
                     int ch, int cam, int camy)
{
    if (!obj_visible(o, cw, ch, cam, camy))
        return;
    const obj_art *a = obj_art_of(o);
    int mirror;
    const ll_seq_elem *e = obj_elem(o, &mirror);
    if (!a || !e)
        return;
    if (getenv("LEO_OBJ_POS")) {
        /* read-only probe (PR-207): where this placement's art is about to
         * be drawn AND the camera it is being drawn through. objs_draw passes
         * the LAYER camera per the layer law (PR-61's math, PR-208's plane
         * from plany=), so cam/camy here is that layer camera, not the main
         * one; the screen origin is what the canvas actually gets. Nothing in
         * this block changes any pixel. */
        int ox_, oy_;
        ll_seq_anchor_origin(e, o->x - cam, o->y - camy,
                             obj_mirror_of(o, mirror), &ox_, &oy_);
        fprintf(stderr, "OBJPOS leaf=%s x=%d y=%d layer=%u draw-cam=(%d,%d) "
                "sti=%d fi=%d art=%s e=%u f0=%d f2=%d b15=%d arr_n=%u "
                "codec=%d mirror=%d draw-origin screen=(%d,%d)\n",
                o->sc->leaf, o->x, o->y, (unsigned)o->layer, cam, camy,
                o->sti, o->fi,
                a && a->name[0] ? a->name : "(none)",
                (unsigned)(e - a->seq.els), (int)(short)e->f0,
                (int)(short)e->f2, (int)(short)e->b15, e->arr_n,
                ll_seq_elem_codec(e), obj_mirror_of(o, mirror),
                ox_, oy_);
    }
    ll_seq_draw_elem_anchor(&a->seq, e, canvas, cw, ch, o->x - cam,
                            o->y - camy, obj_mirror_of(o, mirror), NULL);
    /* PR-81: the facing a CH_DIR walker turned into is drawn on top of the
     * authored frame sign; every other object keeps face=+1 and the plain
     * authored mirror. Evidence in obj_walk's comment. */
}

/* PR-69: the object scripts are state machines, not a ring of states: a
 * state's frame list loops until one of its transitions fires. Conditions
 * the scene model can decide: AC_BREAK_NEXT (the frame list just ended),
 * K_GEN1..5 and any other signal NAME of the LL_SIG_* table (PR-88: tested
 * against this object's own pulse set - raised by frame `GEN <signal>`
 * tails or by gameplay code like the PR-70 balloon pop); AC_TIME is
 * PR-163 MODELLED, not approximated: the engine's own self-
 * raiser (PR-159 F2 / PR-160 F2-F3 / PR-162 F1c) - ENTER rolls
 * a gate [rec+0xc] uniformly from the authored `time=<min>[,max]`
 * pair (rand = the Borland LCG at 0x76bcc), same-state transi-
 * tion MATCHES counted in [obj+0x9d] raise live-set bit 9 once
 * the count reaches the gate (0x53626-0x5365c), and NO time=
 * line = gate 0 = never. obj_transition/obj_enter now implement
 * exactly that law, so the bit reaching THIS test is real. The
 * one approximation that stays is `brk ||`: the engine's record
 * walk arms the list-end bit in the composed key itself, while
 * the port passes it as an argument beside its own pulse set -
 * additive, not conflicting; whether the engine's in-walk raise
 * is visible to LATER lines of the same list is the PR-164
 * mechanism-boundary question (counted open). What still stays
 * false (keys, AC_ONAIR, AC_ENEMY_ATC ...)
 * needs the lion/world signals feeding this set (open). First-match order
 * as the player's transitions. PR-88 aligned the grammar with the engine;
 * PR-264 completes that alignment: the clause parser at 0x47d60 compiles
 * ONE bit mask and ONE operation byte per condition (it clears the byte
 * at 0x47d73, '!'/'*'/'|' write 1/2/3 for the WHOLE clause, a ',' folds
 * its name into the same mask), and TEventSet::match 0x47e40 tests
 * mask-against-live with that operation - subset / disjoint / equal /
 * intersects. The OR-of-AND-groups reading is gone on this side too;
 * the match lives in one place, ll_mov_tr_mask plus this switch. */
static int obj_cond_true(const lvl_obj *o, int brk, const ll_mov_tr *tr)
{
    /* The live set is exactly what the engine composes into [obj+0x60]
     * before the pass (0x53705-0x53725, PR-262): this object's own
     * raised bits, plus the list-end bit the walk arms (the port's
     * `brk` argument stands in for the bit the engine ORs at
     * 0x521c1-0x521c5). */
    unsigned long long mask = ll_mov_tr_mask(tr);
    unsigned long long live = o->sig;

    if (brk)
        live |= 1ull << LL_SIG_AC_BREAK_NEXT;
    if (!mask)
        return 0;               /* PR-88 convention kept (see the player
                                 * evaluator): a name in NO table compiles
                                 * to an empty mask; the port never takes
                                 * it, the engine's subset arm arguably
                                 * would. No shipped clause does it. */
    switch (tr->op) {
    case LL_TOP_NOT:                        /* '!' : share NOTHING */
        return (mask & live) == 0ull;
    case LL_TOP_EXACT:                      /* '*' : byte-identical
                                            * (no shipped clause) */
        return mask == live;
    case LL_TOP_ANY:                        /* '|' : share ANYTHING. For
                                            * a scene object even the
                                            * input-key mask can only be
                                            * live the way the engine
                                            * puts bits in [obj+0x60] -
                                            * a row's `GEN <name>` tail
                                            * or the bus - so K_ANY
                                            * needs no case here (all 41
                                            * shipped any-rows are lion
                                            * texts). */
        return (mask & live) != 0ull;
    default:                                /* 0: SUBSET - every name it
                                            * names is up (the AND of a
                                            * ',' list, PR-87) */
        return (mask & ~live) == 0ull;
    }
}

/* A state change. PR-101 read the engine's own: it is a TABLE, not a
 * search. 0x52000 (the state record's sub-table slot +0x04, called from
 * 0x53520 inside the key compositor 0x536f0) walks 21-byte entries at
 * [rec+0x1a] - a 13-byte key, a record index at +0xd, a row index at
 * +0x11 - calling 0x47e40 with the object's composed key for each, and
 * 0x535a0 (the entry; 0x535a2 is listed one byte in) applies
 * the first match as "go to record A, row B" (its
 * same-index path is just `cmp esi,[rec+4]; mov [rec+4],edx`). So the
 * engine's transition is a keyed lookup that also picks the starting
 * row, and the key it looks up is the OR of the attribute-node chains
 * of PR-101, not an authored name. What the port matches is the
 * CONDITION half (a key/flag test gating a state change) and the
 * per-state reset of the motion accumulators. PR-102 corrects what
 * this comment first said about the row-index half: the engine's row
 * number IS authored and the engine APPLIES it. The TARGET name is not
 * looked up at run time either: at PARSE time 0x51e0b calls 0x53440,
 * a case-insensitive walk of the owner's [obj+0x3c] state-name table
 * (stride 8, 1-based result, 0 = unresolved), and the index is baked
 * into the entry at +0xd. The transition line in
 * a .MOV state section is `TARGET(N)=<key expression>` (the base
 * record parse method 0x51910 matches `" ( %[^) ] %*[)] = "` at
 * 0x9225c - the same function that parses `control=`; the derived
 * record class's parser 0x52590 handles `F=`/`T=`/`G=` and delegates
 * everything else to it), N is stored at entry+0x11 MINUS ONE: the token
 * is tried against `"%ld"` (0x57db0) and read with strtol then `dec`
 * (0x57fd0, 0x51e3b), while an absent or non-numeric token stores -1
 * (0x51e3e). That value really is a row: the transition apply step
 * 0x535a0 (what PR-101 called "the switch") writes the int it was
 * handed into the state record's live row counter at rec+0x4 on its same-index
 * path (0x53616), and rec+0x4 IS that counter - the
 * row stepper helper 0x520c0 advances it (lea+store 0x521d4 - PR-162
 * F1b, one byte off PR-107's 0x521d7) and wraps last->0 at
 * 0x521c1-0x521c5. Its cross-state path reaches the same field through the
 * state record's own method table: every record stores the fixed table
 * 0xa25d4 at rec+0x44 (its constructors, 0x524cf and 0x52508) and that
 * path calls slot +0x1c of it on the NEW record (0x535e7-0x535ea) - and
 * +0x1c is the method 0x52360. Its row-set half refuses a negative value
 * (`test edx,edx` + `jl`, 0x52381) and otherwise writes it there raw,
 * mirrored to `[rec+0x36] - n - 1` when the SEQ_REV_X/Y reversal of PR-95
 * says so (0x523bc-0x523c5) - with NO upper bound. PR-160/PR-162 read
 * the OTHER half of the same method: its tail 0x52070/0x52072 zeroes
 * the [obj+0x9d] match counter and rolls the AC_TIME gate - one
 * ENTER. PR-107 applies the row here, through ll_mov_row_start, the
 * rule the lion's side has used since PR-86. The shipped scope,
 * measured through the port's own parser
 * over the 777 .MOV: of 2572 numeric transition lines only 273 ask for a
 * row other than 0 (2299 say `(1)`, which is row 0); 172 of the 273
 * cross a state boundary, 101 are self-targets, and 25 of the 172 are in
 * a SCENE-object script - KADMAR.07 22, KADMAR2 1, ENA2 1, SADES.06 1.
 * 14 lines ask for a row past their target's list and the port clamps
 * them to the top (the PR-86 rule; the engine does not bound the value);
 * 12 of those 14 are self-targets. The port also has one frame counter
 * per object, where the engine keeps one per state record at rec+0x4, and
 * it does not model the mirror (NEXT_STEPS PR-107, open items). PR-105
 * adds the shape around it: the whole sequence - compose the key, step
 * the rows, look up this table, apply the match - is ONE virtual method,
 * the object's slot +0x14 (0x536f0), called once per step from the
 * level's message pump. */

/* PR-163: the engine's rand() - 0x76bcc verbatim (Borland LCG: state
 * x*0x41c64e6d+0x3039, output (x>>16)&0x7fff; 0x76bc6 holds ONE state
 * image-wide). */
static unsigned obj_rand(unsigned *st)
{
    *st = *st * 0x41c64e6du + 0x3039u;
    return (*st >> 16) & 0x7fffu;
}

/* PR-163: ENTER - the record-class +0x1c verb body (base 0x52360 ->
 * 0x52070/0x52072): zero the [obj+0x9d] match counter and ROLL the
 * gate from the authored time= pair, [rec+0xc] = min + (max-min)*r/
 * 32767 (r = obj_rand); NO time= pair -> gate 0 -> NEVER raises (the
 * ctor leaves it; PR-160 F3). The single-value form pairs to min=max
 * (parse_pair = the engine scanf), so the roll degenerates to gate =
 * min - the deterministic N-match wait; the engine's lerp with max=min
 * is its open item (PR-160), the port's reading is pinned by the
 * PR-163 selftest. */
static void obj_enter(lvl_obj *o, ll_mov_t *mov, int sti)
{
    o->act_n = 0;
    if (sti < 0 || sti >= mov->nst || mov->st[sti].time_lo <= 0) {
        o->act_gate = 0;
        return;
    }
    unsigned r = obj_rand(&o->act_r);
    unsigned lo = (unsigned)mov->st[sti].time_lo,
             hi = (unsigned)mov->st[sti].time_hi;
    o->act_gate = (hi > lo)
        ? (unsigned)(lo + (hi - lo) * r / 32767u)
        : lo;
}

static int obj_transition(lvl_obj *o, ll_mov_t *mov, int brk)
{
    const ll_mov_st *s = &mov->st[o->sti];

    for (int t = 0; t < s->ntr; t++) {
        if (!obj_cond_true(o, brk, &s->tr[t]))
            continue;
        if (s->tr[t].tidx < 0)
            continue;                         /* unresolved */
        if (s->tr[t].tidx == o->sti) {
            /* PR-174: the arm opens with the ROW write, exactly as the
             * engine's does: 0x5360f-0x53616 read the current record's
             * row counter, skip the store only when it already holds the
             * value (0x53612 - which today only gates the +0xac verb's
             * call, an unread body, item (1)), else `mov [eax+0x4],edx`
             * stores the authored start row into it. PR-107 settled the
             * value's shape for THIS path: stored raw, and an absent
             * token (-1) lands on row 0 through the stepper's wrap
             * (0x521c5) - "the same visible result" as starting at the
             * top - so the shared ll_mov_row_start mapping is the
             * engine's observable behaviour here (its over-long clamp
             * stays the recorded PR-107 model choice). */
            o->fi = ll_mov_row_start(&mov->st[o->sti],
                                     s->tr[t].entry);
            /* PR-163: the engine's same-state match arm (0x53626-0x5365c,
             * the else beside the applier's state-change arm): a MATCH
             * that keeps this state is the AC_TIME counting event - it
             * bumps [obj+0x9d] and the gate check raises live-set bit 9
             * once the count reaches the rolled gate. Unresolved targets
             * (tidx < 0) never reach this arm. */
            o->act_n++;
            if (o->act_gate && o->act_n >= o->act_gate)
                o->sig |= 1ull << LL_SIG_AC_TIME;
            continue;                         /* keep this state */
        }
        int byx = (s->ctrl & LL_CTRL_BY_X) ||
                  (mov->st[s->tr[t].tidx].ctrl & LL_CTRL_BY_X);
        o->sti = s->tr[t].tidx;
        o->fi = ll_mov_row_start(&mov->st[o->sti], s->tr[t].entry);
                                              /* PR-107: the authored start */
                                              /* row of `( N ) `, the same   */
                                              /* rule the lion's side uses  */
        if (!byx) {
            o->x = o->bx;
            o->y = o->by;       /* motion deltas are per state; a BY_X
                                 * position is instance state: walking
                                 * creatures carry it across states */
        }
        /* face is instance state too (PR-81): a creature that turned
         * around stays turned around when it changes state */
        o->facc = 0.0;
        o->xa = 0.0;
        o->row_t = 0;            /* PR-172: the engine's state-change
                                  * applier (entry 0x535a0) calls the NEW
                                  * record's ENTER at [[rec+0x44]+0x1c]
                                  * = 0x4fef0, which tail-calls base
                                  * ENTER 0x52360 - and that zeroes the
                                  * feed accumulator [obj+0xa1] (PR-167
                                  * F4). One register, fresh per state;
                                  * a match that KEEPS the state (the
                                  * 0x53626 arm) calls no ENTER and
                                  * carries it - see obj_rowzero_selftest
                                  * (obj_state_selftest's stale-mutant
                                  * leg already pins the facc half). */
        o->sig = 0;
        o->atc = 0;                  /* PR-89: an attack window belongs to
                                      * the state that opened it           */
        o->atc_hit = 0;              /* PR-90: and so does its one-hit flag */
        obj_enter(o, mov, o->sti);   /* PR-163: the applier's state-change
                                     * arm (0x535c2) runs ENTER on the new
                                     * record: zero the counter and roll
                                     * the new state's gate               */
        return 1;
    }
    return 0;
}

/* PR-89: the lion's attack lands on a scene object. The engine's damage
 * pipeline (fully mapped in NEXT_STEPS "T2 RE lead"): a touch runs the
 * push-out first (FUN_000552e0 -> 0x53ae0/0x55130) and only then the hit
 * effects (0x55230); the victim's energy counter at [obj+0xad] is
 * decremented by FUN_00054550 `sub [eax+0xad], ebx`, the deliverer's
 * damage is its own [obj+0xb1] (the placement `damage=`, already parsed),
 * and the death gate is `energy < 0` (0x544a5 `cmp [eax+0xad],0; jge`).
 * The ATTACK dispatcher (0x34020) answers on the ARGUMENT object's .des
 * template TYPE (it compares [arg.vt+0xc], the class id, against the
 * factory-instance id array ds:0xcecf0..0xced04 - one startup routine calls a
 * factory, 0x44350, and stores each returned id there; the id->TYPE map is
 * still an open question). Two entries (0xced00, 0xcecf0) make the receiver
 * take `energy*30/100+10` damage (0x34037/0x3406b: shl 4, sub, `lea [eax+
 * eax*1]` = *30, idiv 100, +10 - NOT *45; the earlier *45 note was a lea
 * scale misread, see PR-96), one (0xcecf8) sets the receiver's `[obj+0xad] =
 * -1`, and 0xced04 is the lion-stomp path PR-89 ported (0x340b5 is 0x34020's
 * fall-through second half, NOT a separate method). The fall-through default
 * is 30 - `mov ebx,0x1e` at 0x34103, `call 0x50ed0` at 0x34108. No shipped
 * .des authors any lion damage number, so 30 is the only value the shipped
 * data points to; the proportional types stay unreachable here.
 *
 * Rejection: the receiver method 0x50ed0 - installed at [obj.vt+0xd0] by
 * the 15 tables that hold the answer-A dword 0x50f80 - is gated by
 * FUN_000502f0 "invulnerable?", which tests the countdown at
 * [obj+0x102]: 0x50f54 calls it, 0x50f59 drops the hit. On "not
 * invulnerable" 0x50f67 runs [obj.vt+0xe4](1) - 0x50300 for those
 * tables, which is `edx ? mov [obj+0x102],0x1e : mov [obj+0x102],edx`
 * (0x50300..0x50315) - and only then 0x54550 subtracts the hp. THE LION
 * IS ONE OF THOSE TABLES: the factory 0x1fef2 matches the class-name
 * literal "TLewZaria" (0x9078b) and writes 0x96960 at [obj+8], and
 * 0x96960 carries +0xd0 = 0x50ed0 and +0xe4 = 0x50300 - so the lion's
 * own countdown (the port's pl->lock, PR-89) is exactly 30, and 0x50720
 * decrements it once per state step (0x536f0 -> 0x523f0 -> [.vt+0xa8]).
 * Objects get the same law here, as a NAMED stand-in: an animal's own
 * class table (SEP's is 0x97bc0, written at 0x62e59) installs 0x1ecd0 at
 * +0xd0 and 0x4ffb0 at +0xe4, the ZWIERZ base 0x97f64 installs
 * 0x41c30/0x41460, and neither writes 0x1e. The 14 at
 * 0x27264 belongs to a different helper, `edx ? [obj+0x102]=14 :
 * [obj+0x102]=0` at 0x27260, referenced by exactly one +0xe4 slot, table
 * 0x9a54c (its factory name is not pinned) - its role is an OPEN item,
 * counted in the PR-139 record.
 *
 * The hit reaction is an INFERENCE, flagged as such: every animal script
 * and every lion state carries "A_DOSTAL(1)=AC_ENEMY_ATC" ("got hit" on
 * "enemy attacked"), and no shipped .MOV line ever GENs AC_ENEMY_ATC (0
 * of 279 uses) - the combat code must be its raiser, and the object that
 * was hit is the one that knows. So the hit raises the bit ON THE HIT
 * OBJECT's own set and lets its own transition table pick A_DOSTAL: no
 * new mechanism, exactly the PR-88 one. PR-91 mirrors the raise onto the
 * LION for the enemy's ACTIVE attack (the ATC-window hit): every shipped
 * lion .MOV authors the same lines - measured on ZARIA.01 LEW.ZAR/LEWPL,
 * A_WALK/A_DRAP/A_FIGHT... carry "A_DOSTAL(1)=AC_ENEMY_ATC", and ZARIA's
 * lion A_DOSTAL is a full authored reaction (BY_TIME 9x200 ms, seq=hit,
 * frame 1 `I -320,-250` pullback, exit A_DRAP(1)=AC_BREAK_NEXT at the
 * list end). The raise itself is now read verbatim, not inferred:
 * answer-C 0x55130 saves the toucher's [toucher+0xad] at 0x55140 and
 * its tail at 0x5516a compares it back: hp DROPPED -> the ATC variant
 * of the AC_ENEMY token (0x5518c `or bl,0x1`), unchanged -> plain
 * AC_ENEMY (0x551ad `or dl,0x80`); and 0x50f80's own tail at
 * 0x51036..0x51059 merges plain AC_ENEMY for any shape meet (PR-92,
 * already ported). So a landed window hit raises AC_ENEMY_ATC and a
 * dropped one still raises AC_ENEMY - which is exactly what the port
 * does. (PR-139 removed the port's unconditional contact damage, so
 * the "contact hit" case whose raise was an open question no longer
 * exists: the engine has no damage path without the window - see the
 * play_collide comment and the PR-139 record.)
 *
 * Death enters the script's own state BY NAME because the engine forces
 * it: 340 of the 413 scripts that have an A_DEAD state have NO
 * transition line into it (measured over the extracted .MOV texts), and
 * the engine hardcodes the string "A_DEAD" four times (0x9068c/0x907ab/
 * 0x90e79/0x91008, inside per-level lion classes next to their
 * `energy = %d` parsers). A script with no A_DEAD state keeps its state
 * here - what the engine then does is open, so nothing is invented.
 * The dead object stops hurting the lion (model choice: its A_DEAD art
 * stays in place; whether the engine's corpse still deals contact
 * damage is not pinned). */
#define LL_LION_HIT_DAMAGE 30   /* the fixed 0x1e of the stomp branch:
                                 * the receiver 0x34020 (table 0x9e134)
                                 * takes 30 when the attacker's class id
                                 * is ds:0xced04 (0x340c8/0x340db,
                                 * PR-89); what the engine really passes
                                 * is [lion+0xb1] (its only setter is
                                 * 0x1e540), unread here - open item,
                                 * PR-139 */
#define LL_OBJ_HIT_CD      30   /* [obj+0x102] = 0x1e, armed by the
                                 * answer-A family's [obj.vt+0xe4] =
                                 * 0x50300 at 0x50f67 (the lion's own
                                 * class 0x96960 among them); the
                                 * animals' +0xe4 methods (0x4ffb0,
                                 * 0x41460) are different, so the
                                 * object-side use is a named stand-in,
                                 * counted as an open item */

static int obj_hit(lvl_obj *o, int damage)   /* 0 = no hit, 1 = hurt,
                                              * 2 = hurt AND killed      */
{
    ll_mov_t *mov = (o->sc && o->sc->mov) ? o->sc->mov : NULL;

    if (!mov || o->hp < 0 || o->hit_cd > 0)
        return 0;              /* no script / energy = -1 is not hit-able
                                * (the engine's own never-dying marker,
                                * 0x340f5) / inside the hit countdown    */
    o->hp -= damage;
    o->hit_cd = LL_OBJ_HIT_CD;
    if (o->hp < 0) {                        /* the engine's death gate:
                                             * energy < 0 (0x544a5)      */
        int d = ll_mov_find(mov, "A_DEAD");

        o->dead = 1;
        if (d >= 0 && d != o->sti) {
            int byx = (mov->st[o->sti].ctrl & LL_CTRL_BY_X) ||
                      (mov->st[d].ctrl & LL_CTRL_BY_X);
            o->sti = d;
            o->fi = 0;
            if (!byx) {
                o->x = o->bx;
                o->y = o->by;
            }
            o->facc = 0.0;
            o->xa = 0.0;
            o->row_t = 0;        /* PR-172: the forced change runs the
                                  * same applier ENTER (obj_enter below),
                                  * so the feed register is zeroed here
                                  * too - the engine zeroes [obj+0xa1]
                                  * on every state change */
            o->sig = 0;
            o->atc = 0;             /* PR-90: same window rule as the     */
            o->atc_hit = 0;         /* transition table's own             */
            obj_enter(o, mov, d);   /* PR-163: every forced state change
                                     * runs the applier's ENTER arm too */
        }
        return 2;
    }
    o->sig |= 1ull << LL_SIG_AC_ENEMY_ATC;
    obj_transition(o, mov, 0);
    o->sig = 0;
    return 1;
}

/* PR-90 (T3): what ARMS an enemy's attack state. The .MOV data never
 * transitions into A_ATAK - ZARIA's LICHA3 has no line into it - because
 * the engine's enemy AI is COMPILED PER CLASS, and the class comes from
 * the placement's OWN class token (measured in the image, NEXT_STEPS
 * PR-90 record): 0x616d0 string-compares the token against "SEP",
 * "PARROT", "SNAKE", "LICHA" (0x9320b..0x93234) and builds that class's
 * AI object (LICHA: 0x120 bytes, constructor 0x61890); the AI attach
 * (0x618b0) creates four behavior records named "MAIN", "WALK", "ATACK",
 * "TURN" (0x93238..0x93248, list [x+0x106]) with method tables 0xa30a8
 * / 0xa308c / 0xa3070 / 0xa3054, and those classes' state-NAME tables
 * hardcode "A_RUN" and "A_ATAK" (0x93262/0x9326d) - the exact state
 * names of this script. So the attack state is ENTERED BY CODE. The gate
 * for the class LICHA is pinned: its MAIN behavior's check (table
 * 0xa30a8 slot +0x18 -> 0x61b50, jump table 0x61b40 case 0 -> 0x61b6e)
 * tests |obj.x - lion.x| < 0x28 (40) AND |obj.y - lion.y| < 0x32 (50)
 * game px, both against CURRENT positions (position objects: the
 * object's own [x+0x26], the lion's through getter 0x3ff60 = [x+0x394];
 * x at +4, y at +0 of those). The same class's ATACK::check (0x61cd2)
 * then tests the lion's side against a facing getter (vtable +0x70);
 * that second stage, the other AI classes' boxes (they are per class)
 * and the engine's consumer of the placement's scope= pair
 * ([obj+0xfa]/+0xfe: a pair argument to a vtable+0x4 method at
 * 0x3bc1d/0x3bcb9, compared against a vtable+0x70 result at 0x3d70c)
 * are open question 10. scope= is NOT the box: measured with this
 * port's own shape margins the centers hold >= 21 px apart in ZARIA.01,
 * so an authored scope=20 box could never open at all. MODEL CHOICE
 * shipped (flagged everywhere it is used): an object whose CURRENT state
 * carries the authored DANGER control flag arms the script's OWN A_ATAK
 * state by name (the PR-89 A_DEAD pattern) when the lion enters the
 * pinned LICHA MAIN box; only that pinned stage, no invented extras.
 * The engine's per-class AI stays open; this gate is the port's
 * stand-in, off unless LEO_PLAY_DANGER=1, exactly like the PR-59 solid
 * gate. What the armed attack drives (charge motion, TURN behaviors) is
 * T3 stage 2.
 * PR-139 names where the engine reads that same authored flag:
 * [obj.vt+0x44] is 0x23920, which returns the flag word of the object's
 * CURRENT animation row record, [[[obj+0x50]+[obj+0x5c]*4]+0x12], and
 * the animal answer 0x5528b tests its 0x2000 bit before taking the
 * danger_power branch (0x55290..0x552a2). The row flag's other bit this
 * PR read is 0x80000 (0x55245), which calls the hit with damage 0. */
static void obj_arm_atak(lvl_obj *o, int lx, int ly, int verbose, int tick)
{
    ll_mov_t *mov = (o->sc && o->sc->mov) ? o->sc->mov : NULL;
    if (!mov || o->dead || o->sti < 0 || o->sti >= mov->nst)
        return;
    ll_mov_st *s = &mov->st[o->sti];
    if (!(s->ctrl & LL_CTRL_DANGER))     /* the authored DANGER flag is  */
        return;                          /* the port's own precondition  */
    int dx = o->x - lx, dy = o->y - ly; /* the pinned LICHA MAIN box    */
    if (dx <= -40 || dx >= 40 || dy <= -50 || dy >= 50)
        return;                          /* |dx| < 40, |dy| < 50 (0x61b6e) */
    int a = ll_mov_find(mov, "A_ATAK");
    if (a < 0 || a == o->sti)            /* nothing to arm               */
        return;
    int byx = (s->ctrl & LL_CTRL_BY_X) ||
              (mov->st[a].ctrl & LL_CTRL_BY_X);
    if (verbose)
        printf("ll_level: play tick %d: %s/%s DANGER: lion within the class"
               " MAIN box (dx %d dy %d): %s arms A_ATAK (model)\n",
               tick, o->sc->leaf, o->sc->movname, dx, dy, s->name);
    o->sti = a;                          /* the PR-89 force-by-name      */
    o->fi = 0;                           /* pattern (A_DEAD): same       */
    if (!byx) {                          /* per-state rules the transi-  */
        o->x = o->bx;                    /* tion table itself applies    */
        o->y = o->by;
    }
    o->facc = 0.0;
    o->xa = 0.0;
    o->sig = 0;
    o->atc = 0;
    o->atc_hit = 0;
    obj_enter(o, mov, a);                /* PR-163: forced change = ENTER */
}

/* PR-79: BY_X states WALK. The frame-list slot semantics are data-
 * internal: the same .MOV authors the second frame-list field as ms for
 * BY_TIME states (400/200) and as 4 for its BY_X state - ZARIA.01's
 * LICHA3 A_RUN - so a BY_X frame list counts MOVEMENT UNITS per
 * animation frame, not ms (no video needed; the PR-79 "measured
 * 3.0-3.75 px/frame" per-frame claim was withdrawn in the 2026-11-16
 * re-check: the blob centroid tracked pose deformation, the worm's walk
 * cycle itself rears vertical S-coil poses, and the camera-band shift
 * method tracked parallax layers, not the ground). The scale claim
 * stands as CORROBORATED: on the video (geometry 2.0x of a 320x240
 * screen, pinned) the ZARIA.01 worm's walk bursts average ~20-30 px/s
 * with its authored F=100 -> |fx|/4 = 25 px/s, and its 9-frame x 4-unit
 * stride cycle = 1.44 s, matching the observed stride rhythm; T's
 * semantics stay unresolved and nothing is built on it. NOTE (PR-99,
 * and it limits the /4 above): every object measured on video has
 * T=40 (worm F=100,T=40; rhino F=180,T=40), so /4 is only measured at
 * one T, and at a fixed T it is indistinguishable from the engine's
 * own law, whose steady state is proportional to F/T (the velocity
 * step at 0x54250: `v -= T*v/100` then `v += F*s/20`; s is the
 * terrain scalar at obj+0xba). PR-98 called that law refuted by these
 * samples; PR-99 corrects that - the samples never vary T (the one
 * T=20 walk is the lion, whose path through that code is unverified).
 * So this walk law is unvalidated for objects at T != 40, and the
 * measurement that would settle it is a BY_X object there (the data
 * has F=100 against 11 different T pairs; only ZARIA is on video).
 * bounds= is the authored patrol key; the flip is modelled at its
 * edges. CH_DIR (PR-81):
 * the census pairs bounds= with CH_DIR (8/8 sampled placements; 0/14
 * without; 93 of the 105 BY_X states carry it). The frame-number sign is
 * the authored BASE facing - ZARIA.01's worm art faces left (the eye
 * sits 11-20% from the left edge in all 9 poses) and its walk list is
 * authored all-NEGATIVE, so the game draws it head-RIGHT - and matching
 * the art in BOTH polarities on video, the polarity that wins flips
 * with the travel direction: head-right while it travels right
 * (t=204.4, 211.2, 212.4), head-left while it travels left (t=206.9,
 * 208.0, 213.2), and both polarities at x~330 one second apart (209.2
 * vs 210.0) = one instance turning around. So a CH_DIR state that turns
 * at the bounds turns its facing too, mirrored on top of the frame
 * sign. It also corroborates the initial direction +1 (right): that is
 * what the authored sign draws. The video shows pause-in-place-then-
 * walk bursts: state-gating (A_ATAK/DANGER vs A_RUN), not modelled.
 * With F=0 nothing accumulates, so the state keeps the old time-driven
 * frame advance without moving: kept as the safe reading (a frozen list
 * could deadlock; no shipped game shows a stationary BY_X walk).
 * PR-100 read the engine's own gate for this and it is the SAME shape
 * as below: the state record's step (0x521f0) calls the mode
 * dispatcher (0x543a0, slot +0xa0 of the owner object's method table),
 * whose BY_X branch adds the truncated |v_x|/100 to [obj+0xa1] once per
 * record step, and the step then compares that accumulator against the
 * current row's authored threshold field, admits at most one row per
 * step, and hands the remainder to the row handler - slot +0x18 of a
 * second method table inside the record object ([rec+0x44], 0x520c0),
 * which writes that remainder straight back into [obj+0xa1] at 0x52118:
 * the carry is kept in the object, exactly as `facc` is here. PR-101
 * corrects one half of that reading and it is worth knowing here: that
 * gate is the EMPTY-KEY branch of the step (0x52241 `sete`/`je` after
 * 0x495b0's all-zero test), and a row whose node DOES carry a key
 * takes the other branch - OR the object's key, look it up with
 * 0x47e40, and if it is absent the step does nothing at all, no feed,
 * no threshold. The port has no per-row key, so every row here is the
 * engine's un-keyed kind; that is the assumption to revisit if a
 * per-row condition is ever parsed. So the
 * pair obj_walk's `facc` + obj_advance's `facc < f->thresh` gate is
 * structurally what the engine does; what differs is the per-step feed
 * (the engine truncates to whole px - NEXT_STEPS PR-99 item 9 - while
 * the port carries a fraction) and the unit of the comparison value,
 * which in the engine is that feed's unit: counted px for BY_X/BY_Y,
 * not ms. PR-105 closed what drives that gate: the step 0x521f0 has
 * exactly ONE call site (0x5240c, in the record-step driver 0x523f0),
 * the driver has exactly one (0x5371c, inside 0x536f0 - one virtual
 * slot, +0x14), and the feed dispatcher 0x543a0 itself has exactly
 * one call site (0x52317, in 0x521f0's own keyed branch). So one
 * call to slot +0x14 = one key composition + ONE feed update + at
 * most one row, with no other timer or scheduler anywhere. PR-106
 * found the caller: the level frame loop 0x4abd0 runs a think pass
 * 0x4ba90 over the level's four object lists once per iteration, and
 * an object is stepped (0x52b80 -> +0x14) only where its slot +0x60
 * answers non-zero - in 132 of 162 measured class tables that slot
 * is `return [obj+0x2c]`, a flag written during the draw pass
 * 0x4b970 through slot +0x64, a rect-in-view test. So the engine
 * steps on-screen objects once per frame and freezes off-screen ones
 * completely; the loop's own period is a retrace sync (0x4b0e0 ->
 * 0x4a120, 0x1dba0) against a startup-measured two-frame setpoint
 * (0x4a190). PR-109 measured that setpoint on the owner's footage: two
 * display frames, 28-33 ms per iteration, and the row thresholds are
 * counted in those iterations (see obj_advance). The length of the
 * iteration this port runs those steps at is LL_ITER_MS = 50 ms: PR-135
 * made that choice, because PR-133/134's measured velocity law is a law
 * per iteration and has no meaning per millisecond (NEXT_STEPS PR-106,
 * PR-109, PR-135). 50 ms is above the 28-33 ms PR-109 measured; how far
 * that gap moves an object's rows is counted open in HANDOFF 7. */
/* PR-135: the rate is the engine's own velocity law (PR-133/134, the
 * same one ll_play.c now runs for the lion), read in px per ITERATION
 * instead of units/s: each iteration the drag takes T/100 of the speed
 * and the force adds F*s/100 with s the intent, so the steady state is
 * F*s/T velocity units = F*s*LL_ITER_MS/(1000*T) px per iteration =
 * F/(2T) at LL_ITER_MS = 50 and the object scale 10 spelled out in
 * obj_walk below. An
 * object's walk is at the full intent while its state runs (a scene
 * object has no keys; PR-98's obj_walk always walked), which is why s
 * is not gated here. This reproduces PR-134's table exactly: the worm
 * (F=100,T=40) is 1.25 px per iteration, the pitch PR-79 measured,
 * and for the T=40 objects in this port the number is what the old
 * `|F| * dt / 4000` estimate already gave on that grid - which is why
 * the totals of those legs do not move.
 * NOT modelled, and counted as an open item (HANDOFF 7): the object's
 * own velocity REGISTER. This steps straight to the steady rate, with
 * none of the ~5-iteration start-up ramp the lion's side now runs,
 * because every object speed in the measured record is a mid-walk
 * measurement (PR-79's worm, PR-81's flips at the bounds) and a
 * register that keeps momentum would carry a creature through its
 * `bounds=` edge, which the video does not show. A state with no T=
 * word has no drag term, so the law has no steady state to solve for;
 * those keep PR-98's /4000 estimate, unchanged and still counted. */
static void obj_walk(lvl_obj *o, const ll_mov_st *s)
{
    double dist = (double)(s->fx >= 0 ? s->fx : -s->fx);

    if (s->tx > 0) {
        int td = s->tx > 130 ? 130 : s->tx;   /* 0x52590 caps T at parse */
        /* PR-523 moved the LION's intent word to 20 - the register
         * seed evidence is all on the lion's side. This object model
         * does NOT follow it: its rate is pinned to PR-79/134's
         * measured object pitches (the worm F=100, T=40 at 1.25 px
         * per iteration), every one of which is a mid-walk video
         * measurement at this scale, and this side models no register
         * at all (HANDOFF 7). 10.0 is that measured scale, spelled
         * out here so the lion's constant moving cannot move it. */
        dist = dist * 10.0 * (double)LL_ITER_MS /
               (1000.0 * (double)td);
    } else {
        dist = dist * (double)LL_ITER_MS / 4000.0;
    }
    o->facc += dist;                          /* units toward the next frame */
    o->xa += dist;
    int px = (int)o->xa;
    o->xa -= (double)px;                      /* carry the fraction to dt+1  */
    if (px <= 0)
        return;
    int dir = o->dir < 0 ? -1 : 1;
    o->x += dir * px;
    if (o->b1 > o->b0) {                      /* bounds= patrol range: flip  */
        if (dir > 0 && o->x >= o->b1)
            o->dir = -1;
        else if (dir < 0 && o->x <= o->b0)
            o->dir = 1;
        dir = o->dir < 0 ? -1 : 1;
    }
    /* PR-81/82: CH_DIR states FACE the direction they travel (PR-81
     * record: the art-vs-authored-sign census and the video, where the
     * worm's winning draw polarity flips with its travel direction at the
     * bounds). PR-82 sets it from the travel direction on every step
     * instead of only at the bounds flip: identical result today (this
     * walk is the port's only object movement source, and dir only
     * changes at a flip), but it stays right the moment another source
     * exists (the DANGER charge-then-walk of T3, `I` lunges). States
     * without CH_DIR never touch facing - it is instance state and sticks
     * (same model as ll_player_step). Applied by obj_mirror_of. */
    if (s->ctrl & LL_CTRL_CH_DIR)
        o->face = dir;                        /* PR-81: CH_DIR = facing follows
                                               * the movement direction */
}

/* PR-110: the object's row clock, the same iteration law as
 * ll_player_step's section 7 (the engine feeds ONE accumulator per
 * object, [obj+0xa1], once per frame-loop iteration - the constant
 * 100 of 0x4c8b7, read by the time-mode branches 0x543f2/0x54406/
 * 0x5442f of the dispatcher 0x543a0). The tick below turns the play
 * grid into iterations of LL_ITER_MS. obj_advance calls it ONLY for a
 * state whose mode is a time mode: the dispatcher's mode-0 (BY_X)
 * branch adds the travelled distance to that same accumulator instead,
 * and there obj_walk's `facc` is the port's copy of it (PR-98's
 * distance law). The scriptless-container branch below keeps ticking
 * it: those objects have no walk feed at all, and the engine's
 * accumulator runs for them too. PR-172 completes that model: the
 * engine's ONE register is ZEROED at every state change (the applier
 * 0x535a0 calls the new record's ENTER at [[rec+0x44]+0x1c] = the
 * wrapper 0x4fef0, whose tail call 0x52360 writes [obj+0xa1] := 0),
 * and a same-state match (0x53626) does not. A register is therefore
 * fresh at every state entry and mode-fixed within one - which is
 * exactly what row_t/facc zeroed at the applier arms give: the split
 * is observationally identical to the single register. */
static unsigned obj_row_tick(lvl_obj *o, unsigned dt_ms)
{
    unsigned n = 0;

    o->row_rem += dt_ms;
    while (o->row_rem >= LL_ITER_MS) {
        o->row_rem -= LL_ITER_MS;
        n++;                                  /* each of these is one   */
    }                                         /* record step, fed 100   */
    return n;
}

/* PR-342: the SURFACE-TABLE MOVE of the object class's +0xa4 verb, which
 * the row admit runs at 0x5233b-0x52341 - after the row handler at
 * [[rec+0x44]+0x18], on the admitted branch of the row law, with eax =
 * [rec+0], the record's owner. PR-339 read what each shipped seat class
 * carries at that slot, at the class view its installer stores: PALMA's
 * 0x96c38 holds 0x1e5c0 and KATAPULT's 0x96d1c holds 0x1e690, while
 * KAMIEN's 0x96a88 holds 0x239c0 and KILOF_PL's 0x96b88 the query
 * 0x45320 - so only the first two move a table at all. PR-341 read what
 * those two switch on: the state record's word at +0x4, the same word
 * the applier writes with the transition's start row (0x53616) and the
 * walk zeroes and increments (0x521c5, 0x521d7) - the ROW CURSOR, which
 * this port keeps as o->fi. 0x1e5c0 compares it with 7 (0x1e5dd) and
 * hands 1 or -1 to 0x45200 as its y argument with x = 0
 * (0x1e5e2-0x1e5f8); 0x1e690 subtracts 3, compares 4, and jumps through
 * the table at 0x1e670 to five arms that call 0x45200 with the fixed
 * pairs below. 0x45200 adds edx to [rec+0] and ebx to [rec+4] of every
 * record of the table at [obj+0xa5] (PR-338), which is what
 * o->seat_x/o->seat_y mirror (PR-335). What is NOT modelled here: the
 * element half of either verb - 0x1e5fd-0x1e624 behind [obj+0xb1], and
 * the [[0xcef40+0x394]+8]+0x10 calls the five arms make behind
 * [obj+0xbd]/[obj+0xc1] - and the slot's SECOND runner, 0x535db in the
 * state applier, which runs with the OLD state's cursor before the new
 * record's ENTER (no shipped seat script has a second state to change
 * to). Both are OI-125.
 * MODEL CHOICE, counted open (PR-341 paragraph (c)): the engine's cursor
 * step lives in the walk body 0x520c0, which PR-324 reads as running
 * BEFORE the step, so its +0xa4 call may see the NEXT row's index. This
 * port keys the move to the row it has just admitted. A cycle carries the
 * cursor through 0..n-1 either way, so what the seat leg measures - the
 * balanced 14-row PALM.MOV cycle, the closed KAT.MOV one - is the same
 * under both readings; only the phase within the cycle differs. */
static void obj_seat_shift(lvl_obj *o, unsigned row)
{
    /* 0x1e690's five arms, index = cursor - 3: the jump table 0x1e670
     * answers 0x1e6be, 0x1e723, 0x1e788, 0x1e7f3 and 0x1e886, whose
     * edx/ebx pairs are these. They sum to exactly (0,0). */
    static const short jdx[5] = { 13, 13, 13, 3, -42 };
    static const short jdy[5] = { 5, 6, 10, 10, -31 };
    int clock, jump, dx = 0, dy = 0;

    if (!o->nseat || !o->sc || !o->sc->leaf[0])
        return;
    clock = !strcasecmp(o->sc->leaf, "PALMA");      /* 0x96c38 +0xa4 */
    jump = !strcasecmp(o->sc->leaf, "KATAPULT");    /* 0x96d1c +0xa4 */
    if (!clock && !jump)
        return;
    if (clock) {
        dy = row < 7 ? 1 : -1;
    } else {
        if (row < 3 || row > 7)       /* `sub eax,3; cmp eax,4; ja 0x1e95d` */
            return;
        dx = jdx[row - 3];
        dy = jdy[row - 3];
    }
    for (unsigned i = 0; i < o->nseat; i++) {
        o->seat_x[i] = (short)(o->seat_x[i] + dx);
        o->seat_y[i] = (short)(o->seat_y[i] + dy);
    }
}

static void obj_advance(lvl_obj *o, unsigned dt_ms)
{
    ll_mov_t *mov = (o->sc && o->sc->mov) ? o->sc->mov : NULL;

    if (!mov || !mov->nst)          /* no script on these containers: stop */
    {
        obj_row_tick(o, dt_ms);
        return;
    }
    if (!o->act_init) {
        /* PR-163: the placement ctor runs ENTER on the INITIAL record
         * (0x539e6-0x539fb, PR-162 F4) - the gate for the declared
         * start state is rolled AT SPAWN, before any step. The port
         * defers that to the object's first scripted call. */
        o->act_init = 1;
        obj_enter(o, mov, o->sti);
    }
    unsigned its = obj_row_tick(o, dt_ms);
    for (unsigned step = 0; step < its; step++) {
    /* PR-135: ONE engine record step per iteration - PR-105's reading
     * of 0x523f0 (one key composition + ONE feed + at most one row),
     * the same shape player_iterate now runs on the lion's side. The
     * loop this replaces admitted as many rows as one feed covered,
     * and the player's copy capped that at `4 * nfr` and resynced the
     * rest to zero; the record step at 0x521f0/0x5232b does neither,
     * and PR-115's own reading of those bytes is what ends it here.
     * The millisecond carry stays in row_rem, so a 116 ms call is two
     * steps and a 20 ms substep is none until the 50 ms add up. */
    if (o->hit_cd > 0)          /* PR-89: the [obj+0x102] countdown runs on
                                 * the engine's iteration, the same fixed
                                 * step as the lion's pl->lock */
        o->hit_cd--;
    /* The key composition comes FIRST, as it does in the driver: the
     * bits the PREVIOUS step armed - the row list ending (brk_arm, the
     * port's model of the wrap flag the engine ORs at 0x521c1-0x521c5,
     * PR-162 F1b) and
     * whatever a frame's `GEN` tail raised - are what this step's
     * transitions test, and they are cleared here rather than by the
     * step that raised them. */
    int brk1 = (int)o->brk_arm;         /* the bit the PREVIOUS step armed */
    /* PR-265: the pass is asked on EVERY step, empty set or not. The
     * engine's step 0x536f0 calls 0x53520 at 0x5373c unconditionally -
     * its only call site in the image - and the walk 0x52000 matches
     * EVERY entry (0x52024-0x5203a), so an `=!X` row (op 1, "the AND
     * IS empty", 0x47edc/0x47f35) is TRUE on a clean step and a
     * subset row (op 0) is false unless its names are live. The old
     * `(o->sig || brk1)` guard was the port's own; it had no engine
     * counterpart and hid exactly the rows KADMAR.07's stairs text
     * (SCHODY_SCH01..03 line 10 `=!K_GEN3`-shape) authors. */
    int hit1 = obj_transition(o, mov, brk1);

    o->sig = 0;
    o->brk_arm = 0;
    if (hit1)
        continue;   /* a step that CHANGES state does not also feed the
                     * new state's rows - MODEL CHOICE, counted open:
                     * 0x523f0 composes keys before the record step, and
                     * whether a state change lands in the composition or
                     * in the step's own tail is not settled by the bytes
                     * read so far. obj_state_selftest pins this shape. */
    ll_mov_st *start = &mov->st[o->sti];
    int walk0 = (start->ctrl & LL_CTRL_BY_X) && start->fx != 0;
    if (walk0)                  /* PR-110: one accumulator, fed BY MODE
                                 * (0x543a0): the time modes add 100 per
                                 * iteration, a walking state's rows are
                                 * fed by obj_walk's distance above and
                                 * its wall clock never runs */
        obj_walk(o, start);
    else {
        o->row_t += 100;        /* one iteration = the engine's constant
                                 * feed of 100 (0x4c8b7); PR-172: the
                                 * engine feeds its ONE register [obj+
                                 * 0xa1] and zeroes it at EVERY state
                                 * change (the record ENTER at [[rec+
                                 * 0x44]+0x1c] = 0x4fef0 -> 0x52360),
                                 * so a fresh state starts at 0 there -
                                 * see the applier arms above */
        o->facc = 0.0;
        o->xa = 0.0;
    }
    {
        ll_mov_st *s = &mov->st[o->sti];
        if (s->nfr == 0)                        /* an empty list: nothing */
            continue;                           /* to admit at all        */
        if (o->fi >= s->nfr) {                  /* past the last row     */
            /* PR-101 read the engine's end of the row list; PR-162
             * F1b re-read it byte-accurate: 0x520c0's sequential
             * path wraps it - at the last row (`n = [[rec+0x2e]+8]`,
             * the record's row count at rec+0x36) it writes 0 and
             * ORs 4 into the owner's vtable-slot-4 word (0x521c1-
             * 0x521c5); the reversed arm at 0x521a1-0x521b1 is gated
             * by the SEQ_REV_X/Y bits 0x10/0x20 XOR-ed against two
             * owner-side position sentinels (PR-95 item 1 CONFIRMED
             * by PR-162: rows reverse by flags, not frame reversal).
             * Whether that OR reaches the transition key directly or
             * through a return value is open (walk return encoding,
             * NEXT_STEPS PR-162); the port models the list end as
             * brk_arm, tested by the NEXT step's key composition.
             * Reaching this branch without a matching transition is
             * the same shape as the engine wrapping and re-running
             * the row list. */
            o->fi = 0;                          /* no way out: loop the state */
            if (!(s->ctrl & LL_CTRL_BY_X)) {    /* walking keeps its position */
                o->x = o->bx;
                o->y = o->by;
            }
            o->brk_arm = 1;                     /* the bit, tested by the NEXT
                                                 * step's key composition */
        }
        ll_mov_fr *f = &s->fr[o->fi];
        int walk = (s->ctrl & LL_CTRL_BY_X) && s->fx != 0;
        /* PR-110: the row gate. f->thresh is a threshold in 1/100-
         * iteration units (PR-109 measured the unit on the owner's
         * ladder clip: the authored 200/300 of [A_WSP] are on screen
         * 66-100 ms, two and three iterations, not 200/300 ms), and
         * o->row_t is fed exactly 100 per iteration of LL_ITER_MS -
         * the engine's accumulator rule, remainder carried by the
         * subtract below. A walking state instead compares against
         * `facc`, the distance its own walk fed this step (PR-98's
         * distance law, 0x543a0 mode 0), and the branch above fed it
         * no wall clock at all: one accumulator, one source. */
        /* PR-287, completed by PR-333: the row's own `Wait` clause comes
         * FIRST, as the engine's does - 0x521f0 asks the node's test
         * slot +0x8 at 0x52235 and tests the answer for EMPTY with
         * 0x495b0 at 0x5223c - and the two paths differ in more than
         * the gate: the EMPTY answer is the one that reaches the
         * threshold compare at 0x52320, while a clause that MATCHES runs
         * with the zero budget of 0x522f8, so its threshold is never
         * consumed, and one that does not match leaves the step through
         * 0x522ed without admitting anything. Both rules, including the
         * clock, are in ll_mov_row_admit. The live set here is what this
         * port keeps in o->sig - the bits raised since the transition
         * pass cleared the mask, PR-262. */
        if (!ll_mov_row_admit(f, o->sig, walk, &o->row_t, &o->facc))
            continue;                           /* this step admits no row;
                                                 * the NEXT iteration's step
                                                 * carries the accumulator  */
        o->x += f->dx;             /* the authored "N,ms, M dx,dy" motion  */
        o->y += f->dy;             /* directive of the frame being held    */
        o->sig |= f->gsig;          /* `GEN <signal>`: raised as the frame ends */
        if (f->atc & LMOV_ATC_ON)     /* PR-89: the object's own attack- */
            o->atc = 1;               /* connect window (an A_ATAK opens  */
        if (f->atc & LMOV_ATC_OFF)    /* it, an ATC_OFF frame closes it). */
            o->atc = 0;               /* The ENEMY hitting the LION with  */
                                      /* it is T3, not this PR.           */
        /* PR-342: the surface-table move the object class's +0xa4 verb
         * makes in THIS step - 0x52341 follows the row handler 0x52338 on
         * the admitted branch, and the word both verbs switch on is the
         * state record's row cursor, which is o->fi at this point. It
         * moves only the seat tables (0x1e5c0 / 0x1e690), and only for
         * the two shipped types whose class stores one of those two. */
        obj_seat_shift(o, (unsigned)o->fi);
        /* PR-324: the answer-6 cursor jump, the object half of F2b
         * (the player half is PR-323). PR-322 read the law: 0x520c0
         * walks the closing row's +0x10 verbs BEFORE stepping
         * (ebx = &[rec+4]), 0x45d90 writes the cursor outright (the
         * parse stored authored-minus-one) and 0x45dc0 adds the
         * verbatim delta, and the answer of 6 at 0x52140 skips the
         * normal step COMPLETELY - this loop's head test at the
         * top of the next iteration included, so a jump off the
         * LAST row neither wraps nor arms the list-end bit. The
         * clamp is the same named port safety net as PR-323's (the
         * engine reads the heap around the row table; KADMAR2_
         * SCHODY's `1,0,GOTOREL -1` at row 1 is exactly that
         * out-of-range case). */
        if (f->gotocmd) {
            int nf = f->gotocmd == 1 ? (int)f->gotoval
                                     : o->fi + (int)f->gotoval;
            if (nf < 0)
                nf = 0;                          /* port safety net  */
            if (nf >= s->nfr)
                nf = s->nfr - 1;                 /* ... and here too */
            o->fi = nf;
        } else {
        o->fi++;
        if (o->fi >= s->nfr) {      /* the admit just ran past the last row:
                                     * that WAS this step's one row, the list
                                     * wraps, and the bit is tested by the
                                     * next step, not by this one */
            o->fi = 0;
            o->brk_arm = 1;
            if (!(s->ctrl & LL_CTRL_BY_X)) {
                o->x = o->bx;
                o->y = o->by;
            }
        }
        }
    }
    }
}

/* PR-49: the .MOV frame-number grammar and the anchored/mirrored element
 * draw, proven without game data. One element: shape+art (w0=9: dc80 codec,
 * margin lists), b19=0, b15=4 (tw 4), b17=1, anchor f0=3 f2=2, a single row
 * {gap 0, copy 1 2 3 4}. Expected (hand-derived from the game's draw
 * methods, ll_seq.c comment):
 *   plain    at (10,10): origin (7,8)  -> row y=8,  x=7..10  = 1 2 3 4
 *   mirrored at (10,10): origin (9,8)  -> row y=8,  x=9..12  = 4 3 2 1
 *   sentinel f0=0x7fff:  plain (10,10) -> row y=10, x=10..13 = 1 2 3 4
 *                        mirrored      -> row y=10, x=6..9   = 4 3 2 1 */
static int seq_anchor_selftest(void)
{
    static const unsigned char blob[] = {
        0x01, 0x00,                                 /* count 1             */
        0x09, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00,   /* w0 b19 b15 b17 */
        0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,   /* f0 f2 f4 f6    */
        0x00, 0x00,                                 /* ml {0}, mr {0}      */
        0x08, 0x00,                                 /* array 0: 8 bytes    */
        0x86, 0x00, 0x04, 0x01, 0x02, 0x03, 0x04, 0x80,
    };
    unsigned char sent[sizeof blob];
    ll_seq sq;
    unsigned char cv[40 * 20];
    int mir, x, ok = 1;
    static const struct { int n, idx, mir, blank; } refs[] = {
        { 1, 0, 0, 0 }, { 5, 4, 0, 0 }, { -1, 0, 1, 0 }, { -7, 6, 1, 0 },
        { 0, 0, 0, 1 },
    };

    for (unsigned i = 0; i < sizeof refs / sizeof refs[0]; i++) {
        ll_frame_ref r = ll_mov_frame_ref(refs[i].n);
        if (r.idx != refs[i].idx || r.mirror != refs[i].mir ||
            r.blank != refs[i].blank) {
            fprintf(stderr, "anchor selftest: FAILED frame number %d means"
                            " idx %d mirror %d blank %d (want %d %d %d)\n",
                    refs[i].n, r.idx, r.mirror, r.blank, refs[i].idx,
                    refs[i].mir, refs[i].blank);
            return 1;
        }
    }
    memcpy(sent, blob, sizeof blob);
    sent[10] = 0xff;                                /* f0 = 0x7fff         */
    sent[11] = 0x7f;
    for (int variant = 0; variant < 2 && ok; variant++) {
        if (ll_seq_parse(variant ? sent : blob, (uint32_t)sizeof blob,
                         &sq) != 0) {
            fprintf(stderr, "anchor selftest: FAILED synthetic .SEQ did not"
                            " parse\n");
            return 1;
        }
        for (mir = 0; mir < 2 && ok; mir++) {
            int row = variant ? 10 : 8;
            int x0 = variant ? (mir ? 6 : 10) : (mir ? 9 : 7);
            static const unsigned char fwd[4] = { 1, 2, 3, 4 };
            memset(cv, 0, sizeof cv);
            ll_seq_draw_elem_anchor(&sq, &sq.els[0], cv, 40, 20, 10, 10, mir,
                                    NULL);
            for (x = 0; x < 40; x++) {
                int k = x - x0;
                unsigned char want = (k >= 0 && k < 4)
                                         ? fwd[mir ? 3 - k : k] : 0;
                if (cv[row * 40 + x] != want) {
                    fprintf(stderr, "anchor selftest: FAILED %s %s pixel"
                                    " (%d,%d) = %u (want %u)\n",
                            variant ? "sentinel" : "anchored",
                            mir ? "mirrored" : "plain", x, row,
                            cv[row * 40 + x], want);
                    ok = 0;
                    break;
                }
            }
            for (x = 0; x < 40 * 20 && ok; x++)     /* nothing else drawn */
                if (cv[x] && (x / 40 != row)) {
                    fprintf(stderr, "anchor selftest: FAILED stray pixel"
                                    " at row %d\n", x / 40);
                    ok = 0;
                }
        }
        ll_seq_free(&sq);
    }
    return ok ? 0 : 1;
}

/* PR-48: the TWO patch row codecs (ll_seq.h), proven without game data.
 * One row, written for FUN_0001d9b8 (w0&8 clear):
 *     gap 2, copy 1 byte {7}, gap 0x45, copy 1 byte {9}
 * The 0x45 is a plain 69-pixel advance there. FUN_0001dc80 (w0&8 set)
 * reads the same byte in the marker position as a SHORT FILL (0x40..0x7f
 * after a copy run), so the two codecs paint different pixels: this is
 * exactly what moved 48 shipped blobs (the lion's LEW*.ZAR/I.SEQ, the BERLO
 * map cursor) when PR-45/46 decoded every element with the dc80 rule. */
static int seq_codec_selftest(void)
{
    static const unsigned char row[] = {
        0x86, 0x02, 0x01, 0x07, 0x45, 0x01, 0x09,   /* the row            */
        0x80,                                       /* end marker row     */
    };
    unsigned char a[100], b[100];
    ll_seq_elem e1 = { 0 }, e15 = { 0 }, e12 = { 0 };
    int i;

    e1.w0 = 1;
    e15.w0 = 15;
    e12.w0 = 12;
    if (ll_seq_elem_codec(&e1) != LL_SEQ_CODEC_D9B8 ||
        ll_seq_elem_codec(&e15) != LL_SEQ_CODEC_DC80 ||
        ll_seq_elem_codec(&e12) != LL_SEQ_CODEC_DC80) {
        fprintf(stderr, "codec selftest: FAILED w0&8 does not pick the"
                        " game's codec\n");
        return 1;
    }
    memset(a, 0, sizeof a);
    memset(b, 0, sizeof b);
    if (ll_seq_patch_draw_codec(row, sizeof row, LL_SEQ_CODEC_D9B8, a, 100, 1,
                                0, 0, NULL) < 0) {
        fprintf(stderr, "codec selftest: FAILED d9b8 row rejected\n");
        return 1;
    }
    for (i = 0; i < 100; i++) {
        unsigned char want = i == 2 ? 7 : i == 72 ? 9 : 0;
        if (a[i] != want) {
            fprintf(stderr, "codec selftest: FAILED d9b8 pixel %d = %u"
                            " (want %u)\n", i, a[i], want);
            return 1;
        }
    }
    /* the SAME bytes under the other codec must NOT give that picture
     * (it may even be malformed there: both outcomes prove the split) */
    if (ll_seq_patch_draw_codec(row, sizeof row, LL_SEQ_CODEC_DC80, b, 100, 1,
                                0, 0, NULL) >= 0 && memcmp(a, b, sizeof a) == 0) {
        fprintf(stderr, "codec selftest: FAILED dc80 decoded the d9b8 row"
                        " identically (the codecs are not distinct)\n");
        return 1;
    }
    return 0;
}

/* PR-46: the element SHAPE geometry (ll_seq_shape_*), proven without any
 * game data or window, from a 9-element .SEQ payload built by hand below.
 * The bytes and every expected number were derived by hand from the game's
 * own functions (FUN_000420b0, FUN_00042a70, and the loader's grammar) and
 * independently reproduced by tools/shape_geom.py over the same payload -
 * the port is checked against BOTH, never against itself. Pinned:
 *   - the shape frame int16(b19)*0x50 + int16(b15) (E7: 1*0x50+8 = 88) and
 *     the participation gate: an element without margin lists (E8, w0=1,
 *     b17=0) has NO shape - it takes part in no hit test (H13);
 *   - the box math: the edge-marker runs (E1: rows of 0xFF skipped at the
 *     ends, so t=2/b=0 and h = b - t + 2 comes out 0), the all-empty
 *     degenerate case (E2: BOTH lists wholly 0xFF -> the game stores
 *     w = 1 - tw = -19, h = 1 - rows = -2, ll_seq_shape_empty = 1), the
 *     anchor shift x -= f0, y -= f2 and its 0x7fff ctor sentinel (E3 vs
 *     E4: the same box, shifted only for the non-sentinel element), and a
 *     per-row RIGHT margin (E5: w = (tw - r) - l + 1 = 6);
 *   - the hit test: the x and y adjacency bounds of a full-rectangle
 *     shape (H1-H4: x=19 hits, x=20 does not; y=3 hits, y=4 does not -
 *     the bounds are strict, exactly as the game's), the row window
 *     (H5/H6: E6 sits one row ABOVE E7, so the game's window pairs E6's
 *     rows 1..2 - left margin 19, right margin 5, i.e. x edges 19 and 15
 *     - with E7's rows 0..1, and E7 overlaps at x=14 but only touches
 *     those edges at x=15 (the game's compares are strict). With the
 *     transposed window PR-46 fixes, the same positions paired E6's wide
 *     row 0 with E7's row 1 instead, and answered hit at x=15 - wrong),
 *     the anchor adjustments
 *     (H7-H9b: same geometry, hit only when the anchor shift was applied,
 *     and the sentinel element behaves as unshifted, H8), the facing
 *     mirror (H11/H12: facing < 0 swaps the two margin lists and shifts
 *     x by -(tw - f0'), so the mirrored narrow side faces the other way
 *     and the hit disappears), and the empty-row skip (H14: E2's rows are
 *     all the marker - no row takes part, no hit at any position). */
static int seq_shape_selftest(void)
{
    /* built by tools/pr46_synth twin (kept in the PR record): 9 elements,
     * 186 bytes, exact consumption asserted below through parse success
     * plus the element count; shape-only elements (w0=2) carry no patch
     * sub-blocks, E8 carries one empty array (its single sub-block). */
    static const unsigned char blob[] = {
        0x09, 0x00, 0x02, 0x00, 0x00, 0x00, 0x14, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x14, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x02, 0x01, 0xff, 0xff,
        0x02, 0x00, 0x00, 0x00, 0x14, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x02, 0x00,
        0x00, 0x00, 0x0a, 0x00, 0x01, 0x00, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x01, 0x00,
        0xff, 0x7f, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
        0x00, 0x00, 0x0a, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x05, 0x02, 0x00, 0x00, 0x00, 0x14, 0x00, 0x04, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x13, 0x00,
        0x00, 0x05, 0x05, 0x00, 0x02, 0x00, 0x01, 0x00, 0x08, 0x00, 0x02, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    /* per element: shape frame, participation count, FUN_000420b0 box,
     * shape_empty flag. E8 has no lists: box values are not applicable
     * (ll_seq_shape_bbox is only called for elements WITH lists). */
    static const struct {
        int tw, rows, part;
        int bx, by, bw, bh;
        int empty;
    } exp[] = {
        {  20, 4, 4,   0,  0,  21,  5, 0 },   /* E0 full-rectangle     */
        {  20, 3, 3,   2,  2,  18,  0, 0 },   /* E1 edge 0xFF runs     */
        {  20, 3, 3,  20,  3, -19, -2, 1 },   /* E2 both lists empty   */
        {  10, 1, 1,  -3, -2,  11,  2, 0 },   /* E3 anchor 3,2         */
        {  10, 1, 1,   0,  0,  11,  2, 0 },   /* E4 anchor 0x7fff      */
        {  10, 1, 1,   0,  0,   6,  2, 0 },   /* E5 right margin 5     */
        {  20, 4, 4,   0,  0,  21,  5, 0 },   /* E6 narrow middle rows */
        {  88, 2, 2,   0,  0,  89,  3, 0 },   /* E7 tw = 1*0x50 + 8    */
        {   8, 0, 0,   0,  0,   0,  0, 0 },   /* E8 no lists: no shape */
    };
    /* a, ax, ay, fa, b, bx, by, fb, expected hit */
    static const struct {
        int a, ax, ay, fa, b, bx, by, fb, want;
    } hits[] = {
        { 0, 0, 0,  1, 0, 19, 0, 1, 1 },   /* H1  x edge, adjacent    */
        { 0, 0, 0,  1, 0, 20, 0, 1, 0 },   /* H2  x edge, touching    */
        { 0, 0, 0,  1, 0,  0, 3, 1, 1 },   /* H3  y window, 1 row     */
        { 0, 0, 0,  1, 0,  0, 4, 1, 0 },   /* H4  y window, none left */
        { 6, 0, -1, 1, 7, 15, 0, 1, 0 },   /* H5  window pairing      */
        { 6, 0, -1, 1, 7, 14, 0, 1, 1 },   /* H6  window pairing      */
        { 3, 0, 2,  1, 0,  8, 0, 1, 0 },   /* H7  anchor x shift      */
        { 4, 0, 2,  1, 0,  8, 0, 1, 1 },   /* H8  sentinel: no shift  */
        { 3, 0, 0,  1, 7,  0, 2, 1, 0 },   /* H9  anchor y: no rows   */
        { 3, 0, 2,  1, 7,  0, 2, 1, 0 },   /* H9b anchor y puts A     */
        { 3, 0, 4,  1, 7,  0, 2, 1, 1 },   /*     above, then below B */
        { 5, 0, 0,  1, 0,  1, 0, 1, 1 },   /* H11 facing +: [0,5)     */
        { 5, 0, 0, -1, 0,  1, 0, 1, 0 },   /* H12 facing -: [-10,0)   */
        { 0, 0, 0,  1, 8,  0, 0, 1, 0 },   /* H13 no lists: no shape  */
        { 2, 0, 0,  1, 0,  0, 0, 1, 0 },   /* H14 all rows empty      */
    };
    ll_seq sq;
    unsigned i;

    if (ll_seq_parse(blob, (uint32_t)sizeof blob, &sq) != 0 ||
        sq.count != sizeof exp / sizeof exp[0]) {
        fprintf(stderr, "shape selftest: FAILED the synthetic .SEQ did not"
                        " parse as built\n");
        return 1;
    }
    for (i = 0; i < sq.count; i++) {
        const ll_seq_elem *e = &sq.els[i];
        int tw, rows, part, bx = 0, by = 0, bw = 0, bh = 0, empty;

        ll_seq_shape_wh(e, &tw, &rows, &part);
        if (tw != exp[i].tw || rows != exp[i].rows || part != exp[i].part) {
            fprintf(stderr, "shape selftest: FAILED E%u shape frame "
                            "tw %d rows %d part %d (want %d %d %d)\n",
                    i, tw, rows, part, exp[i].tw, exp[i].rows, exp[i].part);
            ll_seq_free(&sq);
            return 1;
        }
        if ((e->w0 & 3) && rows > 0)
            ll_seq_shape_bbox(&sq, e, &bx, &by, &bw, &bh);
        empty = ll_seq_shape_empty(&sq, e);
        if (bx != exp[i].bx || by != exp[i].by || bw != exp[i].bw ||
            bh != exp[i].bh || empty != exp[i].empty) {
            fprintf(stderr, "shape selftest: FAILED E%u box %d %d %d %d"
                            " empty %d (want %d %d %d %d empty %d)\n",
                    i, bx, by, bw, bh, empty, exp[i].bx, exp[i].by,
                    exp[i].bw, exp[i].bh, exp[i].empty);
            ll_seq_free(&sq);
            return 1;
        }
    }
    for (i = 0; i < sizeof hits / sizeof hits[0]; i++) {
        int got = ll_seq_shape_hit(&sq, &sq.els[hits[i].a],
                                   hits[i].ax, hits[i].ay, hits[i].fa,
                                   &sq.els[hits[i].b],
                                   hits[i].bx, hits[i].by, hits[i].fb);
        if (got != hits[i].want) {
            fprintf(stderr, "shape selftest: FAILED E%d@%d,%d facing %d vs"
                            " E%d@%d,%d facing %d: hit %d (want %d)\n",
                    hits[i].a, hits[i].ax, hits[i].ay, hits[i].fa,
                    hits[i].b, hits[i].bx, hits[i].by, hits[i].fb,
                    got, hits[i].want);
            ll_seq_free(&sq);
            return 1;
        }
    }
    ll_seq_free(&sq);
    return 0;
}

/* PR-58: the push-out (ll_seq_shape_push, FUN_00042de0 + FUN_00042cf0)
 * pinned from a 2-element hand-built .SEQ: E0 = a 20x4 full rectangle (all
 * margins 0), E1 = a slanted shape (left margins 0,2,4,6). Every expected
 * number was derived by hand from the machine code and reproduced by the
 * independent python twin (tools/shape_geom.py push), never from this port:
 *   P1 E0@0 vs E0@10  - depth 10 on every row, but the vertical candidate
 *      (4) is smaller than the horizontal push, so (dx,dy) = (0,4);
 *   P2/P3 E0@0 vs E0@18 and the reverse - depth 2 (horizontal wins), the
 *      reverse branch negates the push AND the result;
 *   P4/P5 E0 vs the slanted E1 at x=14 - rows 0..2 overlap by 6,4,2, so
 *      best 6, first row 0, last row 2, d = 0-4 = -4, dr = 2:
 *      dx = 6*2/6 = 2, dy = 2*(-4)/6 = -1 (C truncation); P5 is the mirror;
 *   P6/P7 the mirrored facing: E0 facing -1 sits at x - 20; touching gives
 *      nothing (P6), one pixel of overlap gives (1,0) (P7). */
static int seq_push_selftest(void)
{
    static const unsigned char blob[] = {
        0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x14, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x14, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x04, 0x06, 0x00, 0x00,
        0x00, 0x00,
    };
    static const struct {
        int a, ax, fa, b, bx, fb;
        int ret, dx, dy;
    } c[] = {
        { 0,  0,  1, 0, 10,  1,  10,  0,  4 },   /* P1 */
        { 0,  0,  1, 0, 18,  1,   2,  2,  0 },   /* P2 */
        { 0, 18,  1, 0,  0,  1,  -2, -2,  0 },   /* P3 */
        { 0,  0,  1, 1, 14,  1,   6,  2, -1 },   /* P4 */
        { 1, 14,  1, 0,  0,  1,  -6, -2,  1 },   /* P5 */
        { 0,  0, -1, 0,  0,  1,   0,  0,  0 },   /* P6 */
        { 0,  0, -1, 0, -1,  1,   1,  1,  0 },   /* P7 */
    };
    ll_seq sq;
    unsigned i;

    if (ll_seq_parse(blob, (uint32_t)sizeof blob, &sq) != 0 || sq.count != 2) {
        fprintf(stderr, "push selftest: FAILED the synthetic .SEQ did not"
                        " parse as built\n");
        return 1;
    }
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        int o1, o2;
        int r = ll_seq_shape_push(&sq, &sq.els[c[i].a], c[i].ax, 0, c[i].fa,
                                  &sq.els[c[i].b], c[i].bx, 0, c[i].fb,
                                  &o1, &o2);
        if (r != c[i].ret || o1 != c[i].dx || o2 != c[i].dy) {
            fprintf(stderr, "push selftest: FAILED P%u: ret %d dx %d dy %d"
                            " (want %d %d %d)\n", i + 1, r, o1, o2,
                    c[i].ret, c[i].dx, c[i].dy);
            ll_seq_free(&sq);
            return 1;
        }
    }
    /* PR-89 follow-up: the PAIRED payload form. The port parses one blob
     * per .SEQ record, so a lion-art vs object-art pairing must read each
     * element's margin lists from its OWN blob; the engine-shaped single
     * form reads the OTHER record's bytes at those offsets (the element
     * offsets are payload-relative). Fixture: a shape element of the SAME
     * frame (w0=2, total_w 20, rows 4) in a second blob whose lists are
     * [0,2,4,6] left and [4,6,1,3] right (the element's own lists are
     * left [0,2,4,6], right all 0; a 4-row element's v-candidate is at
     * most 4, so the horizontal results are not masked). Values measured
     * with the python oracle (tools/shape_geom.py) on the two payloads:
     *   A@8  vs B@0 read correctly -> hit, ret -8, push (-2, 2)
     *   A@12 vs B@0 read correctly -> hit, ret -4, push (-1, 1)
     *   the SAME calls single-form -> ret -12, push (-4, 2)
     *                                    (it read A's own right list)
     * and a same-payload pairing must be byte-identical through both. */
    {
        static const unsigned char blob_b[] = {
            0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x14, 0x00, 0x04, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x02, 0x04, 0x06, 0x01, 0x03, 0x05, 0x07, 0x00, 0x00,
        };
        ll_seq sb;
        int r1, r2, s1, s2, t1, t2, bad = 0;
        if (ll_seq_parse(blob_b, (uint32_t)sizeof blob_b, &sb) != 0 ||
            sb.count != 1 || sb.els[0].n != 4) {
            fprintf(stderr, "push selftest: FAILED the paired synthetic"
                            " .SEQ did not parse as built\n");
            ll_seq_free(&sq);
            return 1;
        }
        if (!ll_seq_shape_hit(&sq, &sq.els[1], 8, 0, 1,
                              &sb.els[0], 0, 0, 1))
            bad = 1;
        if (ll_seq_shape_push2(&sq, &sq.els[1], 8, 0, 1,
                               &sb, &sb.els[0], 0, 0, 1, &r1, &r2) != -8 ||
            r1 != -2 || r2 != 2)
            bad = 2;
        if (!ll_seq_shape_hit(&sq, &sq.els[1], 12, 0, 1,
                              &sb.els[0], 0, 0, 1))
            bad = 3;
        if (ll_seq_shape_push2(&sq, &sq.els[1], 12, 0, 1,
                               &sb, &sb.els[0], 0, 0, 1, &r1, &r2) != -4 ||
            r1 != -1 || r2 != 1)
            bad = 4;
        r1 = ll_seq_shape_push(&sq, &sq.els[1], 8, 0, 1,
                               &sb.els[0], 0, 0, 1, &s1, &s2);
        if (r1 != -12 || s1 != -4 || s2 != 2)
            bad = 5;    /* the wrong-bytes single form: different answer */
        r1 = ll_seq_shape_push(&sq, &sq.els[1], 8, 0, 1,
                               &sq.els[1], 0, 0, 1, &s1, &s2);
        r2 = ll_seq_shape_push2(&sq, &sq.els[1], 8, 0, 1,
                                &sq, &sq.els[1], 0, 0, 1, &t1, &t2);
        if (r1 != r2 || s1 != t1 || s2 != t2)
            bad = 6;
        if (bad) {
            fprintf(stderr, "push selftest: FAILED the paired-payload leg"
                            " (case %d)\n", bad);
            ll_seq_free(&sb);
            ll_seq_free(&sq);
            return 1;
        }
        ll_seq_free(&sb);
    }
    ll_seq_free(&sq);
    return 0;
}

/* PR-69: object scripts are state machines (hand-derived, data-free).
 * Script text: A_RUN loops frames 1,2 (100 ms each) with A_DEAD(1)=K_GEN1;
 * A_DEAD holds frame 3 and goes back with A_RUN(1)=AC_BREAK_NEXT.
 *   no signal: 250 ms later the state is still A_RUN (the old ring would
 *   have walked into A_DEAD at 200 ms);
 *   frame 2 carrying `GEN K_GEN1`: at 200 ms A_RUN -> A_DEAD (fi 0), and
 *   100 ms later the frame list of A_DEAD ends -> A_RUN again. */
static int balloon_hit(int bx, int by, int rx, int ry, int rw, int rh);
static void obj_touch_enemy(lvl_obj *o, int verbose, int tick);
/* PR-93: the two placement grammars proved on synthetic lines (the two
 * real shapes from ZARIA.01 ZWIERZ1.DES + ANI1.DES: one SEP entry with its
 * carried jajo child at 0,0, and PICT/plan4.seq + plan6.seq static images
 * whose art is NOT on these synthetic containers - only the classification,
 * counts and placement are exercised). */
static int place_grammar_selftest(void);
static int place_rectclass_selftest(void);   /* PR-328 */
static int rect_class_ask_selftest(void);         /* PR-329 */

static int obj_contact(const lvl_obj *o);   /* defined below, with the shape
                                             * helpers it reads */

/* PR-78: a placement whose START state has CHECK_LN is a ground-bound actor
 * (the lion's own spawn works the same way: authored pozycja 50,640 falls to
 * the 680 line). Its authored y is where it is DROPPED from, so it settles on
 * the first ground line at or below it. Evidence: licha3.mov A_RUN is
 * `BY_X|CHECK_LN`, `G=0,100`, and ZARIA.01 places the worms at y=650 over a
 * 680 floor (they floated 30 units above the lion's ground line). The fall is
 * instant here (the G= fall itself is not simulated for scene objects).
 * PR-97: the line it comes to rest on is its CONTACT line - the deepest
 * shape-box bottom of its START state's animation (obj_contact ->
 * mov_state_contact, ll_seq_shape_contact), so the actor stands with that row
 * on the floor instead of with its position point on the floor. Same rule and
 * same proof as ll_player.contact. Measured on ZARIA.01: the LICHA worms' own
 * art (MOVE.SEQ, the state their start frame list shows) carries 12 there -
 * they settle at (120,669) on the 681 line instead of (120,681) - and
 * ANIBACK/WAZ.SEQ, the big background worm, carries 46..49.
 * Returns 1 when the object moved. */
static int obj_settle(lvl_obj *o, const ll_plt *plt)
{
    if (!o->sc || !o->sc->mov || o->sti >= o->sc->mov->nst)
        return 0;
    if (!(o->sc->mov->st[o->sti].ctrl & LL_CTRL_CHECK_LN))
        return 0;
    ll_xy hit;
    if (!ll_plt_first_below(plt, o->bx, o->by, &hit))
        return 0;
    int c = obj_contact(o);
    if (hit.y - c == o->by)
        return 0;                       /* already resting on this line */
    o->by = o->y = hit.y - c;
    return 1;
}

/* PR-69: object scripts are state machines (hand-derived, data-free).
 * Script text: A_RUN loops frames 1,2 (100 units each) with
 * A_DEAD(1)=K_GEN1; A_DEAD holds frame 3 and goes back with
 * A_RUN(1)=AC_BREAK_NEXT.
 *   no signal: 250 ms later the state is still A_RUN (the old ring would
 *   have walked into A_DEAD at 200 ms);
 *   frame 2 carrying `GEN K_GEN1`: the state change is the frame's OWN
 *   signal, so the count is in ITERATIONS, not ms (PR-110): a row of 100
 *   is one iteration, and at the port's LL_ITER_MS one call feeds exactly
 *   100. PR-135 RE-DERIVED THE CALL COUNT for the record-step shape: a
 *   step raises a signal or wraps its list, and the NEXT step's key
 *   composition is what matches a transition (0x523f0 composes keys, then
 *   calls the step - PR-105). So:
 *     call 1 closes row 1 (100 units);
 *     call 2 closes the K_GEN1 row, which runs past the end of the list,
 *       so it wraps to row 1 and arms the bit: the signal and brk_arm are
 *       both set, and NOTHING has matched yet;
 *     call 3 matches A_DEAD(1) in its key composition and enters A_DEAD at
 *       its authored row 1; a step that changes state does NOT also feed
 *       the new state's rows (MODEL CHOICE, counted open - see the note
 *       in obj_advance), so A_DEAD's row starts this step at 0 units;
 *     call 4 closes A_DEAD's single row, wrapping ITS list and arming the
 *       bit again;
 *     call 5 matches A_RUN(1)=AC_BREAK_NEXT and is back in A_RUN.
 *   Five iterations = 250 ms, where the same script took 3 calls before
 *   PR-135 and 200/300 ms on the old ms clock. Nothing here is tuned:
 *   the numbers follow from one feed + one admit + match-at-the-head. */
static int obj_state_selftest(void)
{
    static const char plain[] =
        "A_RUN\nA_DEAD\n\n[A_RUN]\ncontrol=BY_TIME\nA_DEAD(1)=K_GEN1\n"
        "1,100\n2,100\n\n[A_DEAD]\ncontrol=BY_TIME\n"
        "A_RUN(1)=AC_BREAK_NEXT\n3,100\n";
    static const char gen[] =
        "A_RUN\nA_DEAD\n\n[A_RUN]\ncontrol=BY_TIME\nA_DEAD(1)=K_GEN1\n"
        "1,100\n2,100,GEN K_GEN1\n\n[A_DEAD]\ncontrol=BY_TIME\n"
        "A_RUN(1)=AC_BREAK_NEXT\n3,100\n";
    int rc = 1;
    ll_mov_t *m1 = calloc(1, sizeof *m1), *m2 = calloc(1, sizeof *m2);
    obj_script s1, s2;
    lvl_obj a, b;

    if (!m1 || !m2)
        goto out;
    memset(&s1, 0, sizeof s1);
    memset(&s2, 0, sizeof s2);
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    ll_mov_parse(plain, m1);
    ll_mov_parse(gen, m2);
    s1.mov = m1;
    s2.mov = m2;
    a.sc = &s1;
    b.sc = &s2;
    obj_advance(&a, 250);
    if (a.sti != 0) {
        fprintf(stderr, "object state selftest: FAILED plain script left A_RUN"
                        " (state %d)\n", a.sti);
        goto out;
    }
    obj_advance(&b, LL_ITER_MS);          /* iteration 1: row 1 of 100 ends */
    if (b.sti != 0 || b.fi != 1 || b.row_t != 0u) {
        fprintf(stderr, "object state selftest: FAILED the first iteration"
                        " did not close exactly one row (state %d frame %d"
                        " t %u)\n", b.sti, b.fi, b.row_t);
        goto out;
    }
    obj_advance(&b, LL_ITER_MS);          /* step 2: the K_GEN1 row ends and */
    if (b.sti != 0 || b.fi != 0 || b.brk_arm != 1u ||                    /* the list wraps with it */
        !((b.sig >> LL_SIG_K_GEN1) & 1u)) {
        fprintf(stderr, "object state selftest: FAILED the K_GEN1 step did"
                        " not arm signal+bit (state %d frame %d brk %u"
                        " sig %llu)\n", b.sti, b.fi, b.brk_arm,
                (unsigned long long)b.sig);
        goto out;
    }
    obj_advance(&b, LL_ITER_MS);          /* step 3: THIS step's key */
    if (b.sti != 1 || b.fi != 0 || b.row_t != 0u) {                       /* composition matches it */
        fprintf(stderr, "object state selftest: FAILED K_GEN1 did not enter"
                        " A_DEAD on the next step (state %d frame %d"
                        " t %u)\n", b.sti, b.fi, b.row_t);
        goto out;
    }
    obj_advance(&b, LL_ITER_MS);          /* step 4: A_DEAD's list ends */
    if (b.sti != 1 || b.fi != 0 || b.brk_arm != 1u) {
        fprintf(stderr, "object state selftest: FAILED A_DEAD did not arm"
                        " the list-end bit (state %d frame %d brk %u)\n",
                b.sti, b.fi, b.brk_arm);
        goto out;
    }
    obj_advance(&b, LL_ITER_MS);          /* step 5: AC_BREAK_NEXT, and the */
    if (b.sti != 0) {                                                        /* bus is empty again */
        fprintf(stderr, "object state selftest: FAILED AC_BREAK_NEXT did not"
                        " return to A_RUN (state %d)\n", b.sti);
        goto out;
    }
    if (b.sig != 0ull || b.brk_arm != 0u) {
        fprintf(stderr, "object state selftest: FAILED the transition left a"
                        " signal or the bit armed (sig %llu brk %u)\n",
                (unsigned long long)b.sig, b.brk_arm);
        goto out;
    }
    /* PR-88: the named-signal bus on objects. A frame raising a table
     * signal by NAME (`GEN AC_PLAT_OFF` - the real platform scripts do
     * exactly this, 181 lines shipped) satisfies a condition naming that
     * signal; a condition naming a DIFFERENT raised-or-not name stays
     * false (the wiring is per name, not "any GEN"); and ',' really
     * ANDs here too (the engine folds a comma run into ONE node mask -
     * clause parser 0x47d60): both raised fires, one raised does not. */
    {
        static const char named[] =
            "A_RUN\nA_DEAD\n\n[A_RUN]\ncontrol=BY_TIME\n"
            "A_DEAD(1)=K_GEN1,AC_PLAT_OFF\n"
            "1,100\n2,100,GEN K_GEN1,GEN AC_PLAT_OFF\n\n[A_DEAD]\n"
            "control=BY_TIME\n3,100\n";
        static const char named_neg[] =
            "A_RUN\nA_DEAD\n\n[A_RUN]\ncontrol=BY_TIME\n"
            "A_DEAD(1)=K_GEN1,AC_PLAT_OUT\n"
            "1,100\n2,100,GEN K_GEN1,GEN AC_PLAT_OFF\n\n[A_DEAD]\n"
            "control=BY_TIME\n3,100\n";
        ll_mov_t *mn = calloc(1, sizeof *mn), *mp = calloc(1, sizeof *mp);
        obj_script sn, sp;
        lvl_obj c, d;
        int ok = mn && mp;

        memset(&sn, 0, sizeof sn);
        memset(&sp, 0, sizeof sp);
        memset(&c, 0, sizeof c);
        memset(&d, 0, sizeof d);
        if (ok) {
            ll_mov_parse(named, mn);
            ll_mov_parse(named_neg, mp);
            sn.mov = mn;
            sp.mov = mp;
            c.sc = &sn;
            d.sc = &sp;
            obj_advance(&c, 200);
            obj_advance(&d, 250);
            if (c.sti != 1)
                fprintf(stderr, "object state selftest: FAILED named-signal"
                                " AND did not enter A_DEAD (state %d)\n",
                        c.sti);
            else if (d.sti != 0)
                fprintf(stderr, "object state selftest: FAILED unraised name"
                                " in an AND group let it leave A_RUN\n");
            else
                ok = 0;                            /* both cases passed */
        }
        free(mn);
        free(mp);
        if (ok)
            goto out;
    }
    /* PR-89: the hit pipeline on objects. A non-lethal hit raises
     * AC_ENEMY_ATC on the HIT object and its own transition table picks
     * A_DOSTAL (exactly the LICHA3 wiring); the engine's death gate is
     * energy < 0, and since no shipped script has a transition INTO
     * A_DEAD (340 of 413), the dead object enters it BY NAME, forced;
     * the [obj+0x102] countdown (30 armed by the landed hit itself -
     * 0x50f67/0x50300, PR-139) rejects a second hit inside it and
     * expires exactly one per state step (0x50720, PR-135);
     * `energy = -1` is the never-hit-able marker (0x340f5). */
    {
        static const char hitt[] =
            "A_RUN\nA_DOSTAL\nA_DEAD\n\n[A_RUN]\ncontrol=BY_TIME\n"
            "A_DOSTAL(1)=AC_ENEMY_ATC\n1,100\n2,100\n\n[A_DOSTAL]\n"
            "control=BY_TIME\nA_RUN(1)=AC_BREAK_NEXT\n3,100\n\n[A_DEAD]\n"
            "control=BY_TIME\n4,100\n";
        ll_mov_t *mh = calloc(1, sizeof *mh);
        obj_script sh;
        lvl_obj h, u;
        int ok = mh != NULL, r1 = 0, r2 = 0;

        memset(&sh, 0, sizeof sh);
        memset(&h, 0, sizeof h);
        if (ok) {
            ll_mov_parse(hitt, mh);
            sh.mov = mh;
            h.sc = &sh;
            h.bx = h.x = 200;
            h.by = h.y = 100;
            h.hp = 20;                       /* .des energy=20, like ZARIA */
            r1 = obj_hit(&h, 15);
            if (r1 != 1 || h.hp != 5 || h.hit_cd != LL_OBJ_HIT_CD ||
                h.sti != 1)                    /* A_DOSTAL: 2nd state BLOCK */
                fprintf(stderr, "object state selftest: FAILED the first"
                                " hit did not hurt and enter A_DOSTAL"
                                " (r %d hp %d cd %d state %d)\n",
                        r1, h.hp, h.hit_cd, h.sti);
            else if (obj_hit(&h, 5) != 0 || h.hp != 5)
                fprintf(stderr, "object state selftest: FAILED a hit inside"
                                " the [obj+0x102] countdown was taken\n");
            else {
                /* PR-135 pins the RATE of the countdown, not only its
                 * end: half of it (15 of the 30 steps armed by the hit,
                 * 0x50300) must still be
                 * running. Before this the leg only asked that the
                 * counter be gone after LL_OBJ_HIT_CD steps, which a
                 * countdown running TWICE as fast also satisfies -
                 * PR-135's mutation run proved it (hit_cd -= 2 passed
                 * every leg in the repo). 30 steps at the exact 50 ms
                 * grid = 1500 ms; A_DOSTAL's one 100-unit row closes
                 * and wraps on step 1 and its own AC_BREAK_NEXT exit
                 * takes step 2, so the state is back in A_RUN long
                 * before the countdown is over, and the half-way hit
                 * below is refused by the countdown, not by the state. */
                int half = LL_OBJ_HIT_CD / 2, bad = 0;
                for (int q = 0; q < half; q++)
                    obj_advance(&h, LL_ITER_MS);
                if (h.hit_cd <= 0) {
                    fprintf(stderr, "object state selftest: FAILED the hit"
                                    " countdown ran out faster than one per"
                                    " engine iteration (%d steps, cd %d)\n",
                            half, h.hit_cd);
                    bad = 1;
                } else if (obj_hit(&h, 5) != 0 || h.hp != 5) {
                    fprintf(stderr, "object state selftest: FAILED a hit"
                                    " inside the half-way countdown was"
                                    " taken\n");
                    bad = 1;
                }
                for (int q = 0; q < LL_OBJ_HIT_CD - half; q++)
                    obj_advance(&h, LL_ITER_MS);
                if (bad) { }                      /* already reported     */
                else if (h.hit_cd != 0 || h.sti != 0)
                    fprintf(stderr, "object state selftest: FAILED the hit"
                                    " countdown did not run out (cd %d"
                                    " state %d)\n", h.hit_cd, h.sti);
                else {
                    r2 = obj_hit(&h, 10);
                    if (r2 != 2 || h.hp != -5 || !h.dead || h.sti != 2)
                        fprintf(stderr, "object state selftest: FAILED a"
                                        " lethal hit did not force A_DEAD"
                                        " (r %d hp %d state %d)\n",
                                r2, h.hp, h.sti);
                    else {
                        obj_advance(&h, 500);        /* A_DEAD has no way
                                                      * out: it loops */
                        if (h.sti != 2)
                            fprintf(stderr, "object state selftest: FAILED"
                                            " the A_DEAD state did not"
                                            " hold (state %d)\n", h.sti);
                        else {
                            u = h;
                            u.hp = -1;       /* the never-dying marker */
                            u.dead = 0;
                            u.sti = 0;
                            if (obj_hit(&u, 30) != 0 || u.hp != -1)
                                fprintf(stderr, "object state selftest:"
                                                " FAILED an energy=-1"
                                                " object was hit\n");
                            else
                                ok = 0;
                        }
                    }
                }
            }
        }
        free(mh);
        if (ok)
            goto out;
    }
    /* PR-107: the `( N ) ` START ROW of a transition, on a scene object
     * (the lion's side has honoured it since PR-86; the object side
     * parsed it and threw it away). Hand-derived, no game data. A_RUN has
     * two 100-unit rows and its second raises K_GEN1; PR-110's clock
     * makes each of them exactly ONE iteration (100 units fed per
     * LL_ITER_MS), and PR-135's record-step shape puts the transition
     * match at the HEAD of the step AFTER the one that raised the
     * signal, so the leg drives three exact-grid calls: call 1 closes
     * row 1, call 2 closes the K_GEN1 row - which runs past the end of
     * A_RUN's two-row list, so it wraps and arms the list-end bit as
     * well - and call 3's key composition is what enters A_DEAD at its
     * row N. N is 1-based, so token `3` means index 2, the last row of
     * the three, and not the top: fi 2 is what no earlier version of
     * this code could produce.
     * The rows left after the start then end that list, and PR-135
     * re-derived the call count for that too: entering costs a full
     * step (the composition step feeds nothing), each further
     * 100-unit row takes one feed, the admit that passes the last row
     * wraps the list and arms the bit, and ONE more call - the next
     * key composition - is where `A_RUN(1)=AC_BREAK_NEXT` matches and
     * goes back to row 1 = index 0. So a start at row 3 (adv 2) and a
     * start at the top (adv 4): the wrap is reached from a mid-list
     * start and from the top alike, one step later than the row that
     * armed it in both. The three boundary cases are the same script with a
     * different token: `(9)` is past A_DEAD's 3 rows and the port clamps
     * it to the top (the engine bounds nothing at 0x523cb); the
     * non-numeric `(x)` also starts at the top, and that token form is
     * real - 44 of the 777 .MOV files author it, KADMAR2's own lion
     * script among them (`A_MASZERUJ(x)=K_FWD`); and `(1)` - the form of
     * 2299 of the 2572 shipped numeric lines, the engine's row 0 - must
     * not move at all, which is what catches an off-by-one the other
     * way. */
    {
        static const char row_tmpl[] =
            "A_RUN\nA_DEAD\n\n[A_RUN]\ncontrol=BY_TIME\nA_DEAD(%s)=K_GEN1\n"
            "1,100\n2,100,GEN K_GEN1\n\n[A_DEAD]\ncontrol=BY_TIME\n"
            "A_RUN(1)=AC_BREAK_NEXT\n5,100\n6,100\n7,100\n";
        static const struct { const char *tok; int fi; int adv;
                             const char *what; } cases[] = {
            { "3", 2, 2, "the third row of a 3-row target: one feed" },
            { "1", 0, 4, "row 1 = the top, the shipped common case:" },
            { "9", 0, 4, "a row past the target's list clamps to the top" },
            { "x", 0, 4, "a non-numeric token starts from the top" },  /* three rows, then the
                                                                          * wrap step, then the
                                                                          * match step        */
        };
        int bad = 0;

        for (size_t q = 0; q < sizeof cases / sizeof cases[0]; q++) {
            char txt[512];
            snprintf(txt, sizeof txt, row_tmpl, cases[q].tok);
            ll_mov_t *mr = calloc(1, sizeof *mr);
            obj_script sr;
            lvl_obj r;

            if (!mr) {
                bad = 1;
                break;
            }
            memset(&sr, 0, sizeof sr);
            memset(&r, 0, sizeof r);
            ll_mov_parse(txt, mr);
            sr.mov = mr;
            r.sc = &sr;
            obj_advance(&r, LL_ITER_MS);        /* step 1: row 1 of A_RUN ends */
            obj_advance(&r, LL_ITER_MS);        /* step 2: the K_GEN1 row ends,
                                                 * past the end of the list    */
            obj_advance(&r, LL_ITER_MS);        /* step 3: its key composition
                                                 * is where the match happens  */
            if (r.sti != 1 || r.fi != cases[q].fi) {
                fprintf(stderr, "object state selftest: FAILED A_DEAD(%s)"
                                " entered at state %d frame %d, expected"
                                " state 1 frame %d (%s)\n", cases[q].tok,
                        r.sti, r.fi, cases[q].fi, cases[q].what);
                bad = 1;
            } else {
                /* the rows that are left (3 - start) iterations: the list
                 * must end and the AC_BREAK_NEXT line must publish it */
                obj_advance(&r, (unsigned)cases[q].adv * LL_ITER_MS);
                if (r.sti != 0 || r.fi != 0) {
                    fprintf(stderr, "object state selftest: FAILED the"
                                    " AC_BREAK_NEXT wrap from a start at"
                                    " row %s did not return to A_RUN row 1"
                                    " (state %d frame %d)\n", cases[q].tok,
                            r.sti, r.fi);
                    bad = 1;
                }
            }
            free(mr);
            if (bad)
                break;
        }
        if (bad)
            goto out;
    }
    {   /* PR-90: the DANGER + class MAIN box arming stand-in. A
         * DANGER-flagged state arms the script's OWN A_ATAK by name when
         * the lion enters the pinned LICHA MAIN box (|dx| < 40,
         * |dy| < 50 - strict, so dx = -40 and dy = -50 are OUT) - and
         * only then: without the DANGER flag, out of range, with no
         * A_ATAK state in the script, or dead, nothing moves. The
         * walking (BY_X) position carries across the forced entry
         * (same rule as the transition table's), the armed state loops
         * (its own frames open/close the attack window), and the one-
         * hit flag resets only on the next state change. */
        static const char dg[] =
            "A_RUN\nA_ATAK\n\n[A_RUN]\ncontrol=BY_X|DANGER\nF=100,0\n"
            "T=40,0\n-1,4\n-2,4\n\n[A_ATAK]\ncontrol=BY_TIME\n"
            "-1,200,ATC_ON\n-2,200\n-3,200,ATC_OFF\n";
        static const char ndg[] =
            "A_RUN\nA_ATAK\n\n[A_RUN]\ncontrol=BY_X\nF=100,0\n"
            "T=40,0\n-1,4\n-2,4\n\n[A_ATAK]\ncontrol=BY_TIME\n"
            "-1,200,ATC_ON\n-2,200\n-3,200,ATC_OFF\n";
        static const char na[] =
            "A_RUN\n\n[A_RUN]\ncontrol=BY_X|DANGER\nF=100,0\n"
            "T=40,0\n-1,4\n-2,4\n";
        ll_mov_t *md = calloc(1, sizeof *md), *mn = calloc(1, sizeof *mn),
                 *ma = calloc(1, sizeof *ma);
        obj_script sd, sn, sa;
        lvl_obj d, n, a;
        int ok = md && mn && ma;

        if (ok) {
            ll_mov_parse(dg, md);
            ll_mov_parse(ndg, mn);
            ll_mov_parse(na, ma);
            if (md->nst < 2 || mn->nst < 2 || ma->nst < 1)
                ok = 0;                           /* parse guard           */
            memset(&sd, 0, sizeof sd);
            memset(&sn, 0, sizeof sn);
            memset(&sa, 0, sizeof sa);
            sd.mov = md;
            sn.mov = mn;
            sa.mov = ma;
            memset(&d, 0, sizeof d);
            d.sc = &sd; d.x = d.bx = 100; d.y = d.by = 50;
            d.hp = 20;
            n = d; n.sc = &sn;
            a = d; a.sc = &sa;                   /* script has no A_ATAK */
            obj_arm_atak(&d, 140, 50, 0, 0);     /* dx -40: out (< test) */
            obj_arm_atak(&d, 100, 100, 0, 0);    /* dy -50: out (< test) */
            obj_arm_atak(&n, 115, 50, 0, 0);     /* no DANGER flag       */
            obj_arm_atak(&a, 115, 50, 0, 0);     /* no A_ATAK to arm     */
            if (d.sti != 0 || n.sti != 0 || a.sti != 0) {
                fprintf(stderr, "object state selftest: FAILED a negative"
                                " DANGER case armed (%d %d %d)\n",
                        d.sti, n.sti, a.sti);
                ok = 0;
            } else {
                d.dead = 1;                       /* dead: never arms */
                obj_arm_atak(&d, 135, 50, 0, 0);
                d.dead = 0;
                obj_arm_atak(&d, 135, 50, 0, 0);  /* dx -35, dy 0: arms  */
                if (d.sti != 1 || d.fi != 0 || d.x != 100 || d.atc ||
                    d.atc_hit) {
                    fprintf(stderr, "object state selftest: FAILED the"
                                    " DANGER arming did not force A_ATAK"
                                    " (state %d frame %d x %d)\n",
                            d.sti, d.fi, d.x);
                    ok = 0;
                } else {
                    /* PR-110: A_ATAK's rows are 200 units = TWO
                     * iterations each, so the exact grid gives the whole
                     * window timeline hand-derived: 2 calls open it
                     * (ATC_ON is on the row that ENDS), 2 more hold it
                     * (the plain middle row), 2 more close it
                     * (ATC_OFF). On the old ms clock those were 400 and
                     * 200 ms. */
                    obj_advance(&d, 2 * LL_ITER_MS);  /* ATC_ON row ends   */
                    if (!d.atc || d.fi != 1) {    /* attack window open */
                        fprintf(stderr, "object state selftest: FAILED"
                                        " A_ATAK did not open the attack"
                                        " window (state %d frame %d)\n",
                                d.sti, d.fi);
                        ok = 0;
                    } else {
                        obj_advance(&d, 2 * LL_ITER_MS);  /* the middle row */
                        if (!d.atc || d.fi != 2) {
                            fprintf(stderr, "object state selftest: FAILED"
                                            " A_ATAK's middle row closed"
                                            " the window (frame %d)\n",
                                    d.fi);
                            ok = 0;
                        } else {
                            obj_advance(&d, 2 * LL_ITER_MS); /* ATC_OFF */
                            if (d.atc || d.fi != 0) {
                                fprintf(stderr, "object state selftest:"
                                                " FAILED A_ATAK did not"
                                                " close the window"
                                                " (frame %d)\n", d.fi);
                                ok = 0;
                            }
                        }
                    }
                }
            }
        }
        free(md);
        free(mn);
        free(ma);
        if (!ok) {
            fprintf(stderr, "object state selftest: FAILED DANGER/MAIN-box"
                            " arming leg\n");
            goto out;
        }
    }
    {   /* PR-92: the touch raise AC_ENEMY, consumed by the object's OWN
         * lines. The raiser is the shipped obj_touch_enemy (raise bit 7,
         * run the transition table, leave the bus empty); the chains are
         * the two shipped .MOV texts verbatim: RBLOK (ENA.03) carries
         * A_ATAK(1)=AC_ENEMY in A_RUN (a touch ARMS the attack), GPTAK
         * (RAN.01) carries A_DEAD(1)=AC_ENEMY in A_ATAK. Negatives: the
         * [obj+0x102] countdown gate (hit_cd) and death block the raise,
         * a script without any AC_ENEMY line (LICHA) stays put, and the
         * signal bus is empty after every call (one-beat pulse). */
        static const char tch[] =
            "A_RUN\nA_ATAK\nA_DEAD\n\n[A_RUN]\ncontrol=BY_TIME\n"
            "A_ATAK(1)=AC_ENEMY\n1,100\n\n[A_ATAK]\ncontrol=BY_TIME\n"
            "A_DEAD(1)=AC_ENEMY\n2,100\n\n[A_DEAD]\ncontrol=BY_TIME\n"
            "3,100\n";
        static const char tno[] =
            "A_RUN\nA_ATAK\n\n[A_RUN]\ncontrol=BY_TIME\n"
            "A_ATAK(1)=K_GEN1\n1,100\n\n[A_ATAK]\ncontrol=BY_TIME\n"
            "2,100\n";
        ll_mov_t *mt = calloc(1, sizeof *mt), *mo = calloc(1, sizeof *mo);
        obj_script st, so;
        lvl_obj t, u;
        int ok = mt && mo;

        if (ok) {
            ll_mov_parse(tch, mt);
            ll_mov_parse(tno, mo);
            if (mt->nst < 3 || mo->nst < 2)
                ok = 0;                           /* parse guard          */
            memset(&st, 0, sizeof st);
            memset(&so, 0, sizeof so);
            st.mov = mt;
            so.mov = mo;
            memset(&t, 0, sizeof t);
            t.sc = &st; t.x = t.bx = 300; t.y = t.by = 100; t.hp = 20;
            memset(&u, 0, sizeof u);
            u.sc = &so; u.x = u.bx = 120; u.y = u.by = 50; u.hp = 20;
            u.hit_cd = LL_OBJ_HIT_CD;              /* countdown gate     */
            obj_touch_enemy(&u, 0, 0);
            if (u.sti != 0 || u.sig) {
                fprintf(stderr, "object state selftest: FAILED a raised"
                                " AC_ENEMY passed the hit_cd gate\n");
                ok = 0;
            }
            u.hit_cd = 0;
            u.dead = 1;                           /* dead: never answers */
            obj_touch_enemy(&u, 0, 0);
            u.dead = 0;
            if (u.sti != 0 || u.sig) {
                fprintf(stderr, "object state selftest: FAILED a dead"
                                " object answered the touch\n");
                ok = 0;
            }
            obj_touch_enemy(&u, 0, 0);            /* no AC_ENEMY line:   */
            if (u.sti != 0 || u.sig) {            /* bit 7 is not K_GEN1 */
                fprintf(stderr, "object state selftest: FAILED a script"
                                " without an AC_ENEMY line answered"
                                " (%d)\n", u.sti);
                ok = 0;
            } else {
                obj_touch_enemy(&t, 0, 0);        /* RBLOK: the touch    */
                if (t.sti != 1 || t.fi != 0 || t.sig) {   /* arms A_ATAK */
                    fprintf(stderr, "object state selftest: FAILED the"
                                    " touch did not arm A_ATAK (%d)\n",
                            t.sti);
                    ok = 0;
                } else {
                    obj_touch_enemy(&t, 0, 0);    /* GPTAK: A_ATAK meets */
                    if (t.sti != 2 || t.fi != 0 || t.sig) { /* A_DEAD    */
                        fprintf(stderr, "object state selftest: FAILED"
                                        " AC_ENEMY in A_ATAK did not go"
                                        " to A_DEAD (%d)\n", t.sti);
                        ok = 0;
                    }
                }
            }
        }
        free(mt);
        free(mo);
        if (!ok) {
            fprintf(stderr, "object state selftest: FAILED AC_ENEMY touch"
                            " leg\n");
            goto out;
        }
    }
    /* balloon_hit: rect 100,50 20x30; the point is (x+10, y+10), inclusive */
    if (!balloon_hit(90, 40, 100, 50, 20, 30) ||     /* corner 100,50      */
        !balloon_hit(110, 70, 100, 50, 20, 30) ||    /* far corner 120,80  */
        balloon_hit(89, 40, 100, 50, 20, 30) ||      /* 99: left of it     */
        balloon_hit(90, 71, 100, 50, 20, 30)) {      /* 81: below it       */
        fprintf(stderr, "object state selftest: FAILED balloon rect\n");
        goto out;
    }
    {   /* PR-78: a CHECK_LN start state drops onto the first line at or
         * below it (150 -> 180 here); a BY_TIME-only one stays; a line
         * ABOVE the object never pulls it up. */
        static const char gnd[] = "A\n\n[A]\ncontrol=BY_X|CHECK_LN\n1,100\n";
        static const char air[] = "A\n\n[A]\ncontrol=BY_TIME\n1,100\n";
        static const char PLTT[] = "2\n1 0,100;500,100;\n1 0,180;500,180;\n";
        ll_mov_t *mg = calloc(1, sizeof *mg), *ma = calloc(1, sizeof *ma);
        obj_script sg, sa;
        lvl_obj g, f, u;
        ll_plt pt;
        int ok = mg && ma && ll_plt_parse(PLTT, &pt) == 0;

        if (ok) {
            ll_mov_parse(gnd, mg);
            ll_mov_parse(air, ma);
            memset(&sg, 0, sizeof sg);
            memset(&sa, 0, sizeof sa);
            sg.mov = mg;
            sa.mov = ma;
            memset(&g, 0, sizeof g);
            g.sc = &sg; g.bx = g.x = 200; g.by = g.y = 150;
            f = g; f.sc = &sa;
            u = g; u.by = u.y = 190;                  /* below every line */
            ok = obj_settle(&g, &pt) == 1 && g.by == 180 && g.y == 180 &&
                 obj_settle(&f, &pt) == 0 && f.by == 150 &&
                 obj_settle(&u, &pt) == 0 && u.by == 190;
            ll_plt_free(&pt);
        }
        free(mg);
        free(ma);
        if (!ok) {
            fprintf(stderr, "object state selftest: FAILED CHECK_LN settle\n");
            goto out;
        }
    }
    {   /* PR-263: the same K_ANY mask on the object side. obj_cond_true
         * had no K_ANY case at all, so an any-row was dead for a scene
         * object. The mask is the one the engine's resolver 0x48000 OR-s
         * in for the name (0x2001f8 in dword 1 = signals 35..40 and 53),
         * and for an object those bits can only arrive the way the engine
         * raises them - merged into [obj+0x60] by the step (0x53705-
         * 0x53725), which in this port is a row's `GEN <name>` tail. So
         * the pair is one script with a different GEN name, and the
         * cadence is obj_state_selftest's own derived one: close row 1,
         * close the GEN row (the list wraps and arms), and the THIRD
         * call's composition is where a match lands. `GEN K_FWD` (36) is
         * in the mask and enters A_DEAD; `GEN K_GEN1` (54) is not and
         * never does. No shipped .MOV puts K_ANY in an OBJECT's text -
         * all 41 shipped any-rows are lion texts - so this leg pins the
         * law, not a level. */
        static const char any_fwd[] =
            "A_RUN\nA_DEAD\n\n[A_RUN]\ncontrol=BY_TIME\n"
            "A_DEAD(1)=|K_ANY\n1,100\n2,100,GEN K_FWD\n\n[A_DEAD]\n"
            "control=BY_TIME\n3,100\n";
        static const char any_gen[] =
            "A_RUN\nA_DEAD\n\n[A_RUN]\ncontrol=BY_TIME\n"
            "A_DEAD(1)=|K_ANY\n1,100\n2,100,GEN K_GEN1\n\n[A_DEAD]\n"
            "control=BY_TIME\n3,100\n";
        ll_mov_t *af = calloc(1, sizeof *af), *ag = calloc(1, sizeof *ag);
        obj_script sf, sg2;
        lvl_obj of, og;
        int pass = 0;
        memset(&sf, 0, sizeof sf);
        memset(&sg2, 0, sizeof sg2);
        memset(&of, 0, sizeof of);
        memset(&og, 0, sizeof og);
        if (af && ag) {
            ll_mov_parse(any_fwd, af);
            ll_mov_parse(any_gen, ag);
            sf.mov = af;
            sg2.mov = ag;
            of.sc = &sf;
            og.sc = &sg2;
            for (int i = 0; i < 2; i++) {
                obj_advance(&of, LL_ITER_MS);
                obj_advance(&og, LL_ITER_MS);
            }
            if (!(of.sti == 0 && (of.sig & (1ull << LL_SIG_K_FWD))))
                fprintf(stderr, "object state selftest: FAILED the"
                                " `GEN K_FWD` row did not raise signal 36"
                                " in the object's own set (state %d sig"
                                " %llu)\n", of.sti,
                        (unsigned long long)of.sig);
            else if (!(og.sti == 0 && (og.sig & (1ull << LL_SIG_K_GEN1))))
                fprintf(stderr, "object state selftest: FAILED the"
                                " `GEN K_GEN1` row did not raise its bit"
                                " (state %d sig %llu)\n", og.sti,
                        (unsigned long long)og.sig);
            else {
                obj_advance(&of, LL_ITER_MS);
                obj_advance(&og, LL_ITER_MS);
                if (of.sti != 1)
                    fprintf(stderr, "object state selftest: FAILED a"
                                    " `|K_ANY` row ignored the K_FWD that"
                                    " was in the set (state %d)\n", of.sti);
                else if (og.sti != 0)
                    fprintf(stderr, "object state selftest: FAILED a"
                                    " `|K_ANY` row took the K_GEN1 that is"
                                    " outside the mask (state %d)\n",
                            og.sti);
                else
                    pass = 1;
            }
        }
        free(af);
        free(ag);
        if (!pass)
            goto out;
    }
    {   /* PR-264 red: the same one-mask, one-operation law on the
         * object side. The clause parser compiles `|AC_TIME,AC_EDGE`
         * (the shape of ZARIA.02 line 119, `|K_GEN1,AC_BREAK_NEXT`)
         * into ONE mask holding both names plus operation 3 - "the AND
         * of the two sets is NOT empty" (TEventSet::match 0x47e40,
         * PR-260) - so a raised AC_TIME ALONE is enough; the port's
         * OR-of-AND-groups reading demanded both. `=!AC_TIME,AC_ONAIR`
         * is operation 1 on its two-name mask - "the AND IS empty":
         * with only an UNRELATED bit (AC_EDGE) in the set the clause
         * is TRUE and must be taken; the port's per-token negation
         * needed AC_ONAIR itself up and never took it. (What this leg
         * does NOT settle: a NOT clause when the set is EMPTY - the
         * port's `(o->sig || brk1)` gate skips the whole pass then,
         * and whether the engine's 0x53520 has the same guard - the
         * empty-test 0x495b0 - is unread.) Expectations are read from
         * the operation table, not from this port's answer. */
        static const char any_t[] =
            "A_RUN\nA_DEAD\n\n[A_RUN]\ncontrol=BY_TIME\n"
            "A_DEAD(1)=|AC_TIME,AC_EDGE\n1,100\n2,100,GEN AC_TIME\n"
            "\n[A_DEAD]\ncontrol=BY_TIME\n3,100\n";
        static const char bang_t[] =
            "A_RUN\nA_DEAD\n\n[A_RUN]\ncontrol=BY_TIME\n"
            "A_DEAD(1)=!AC_TIME,AC_ONAIR\n1,100\n2,100,GEN AC_EDGE\n"
            "\n[A_DEAD]\ncontrol=BY_TIME\n3,100\n";
        ll_mov_t *at = calloc(1, sizeof *at), *bt = calloc(1, sizeof *bt);
        obj_script sta, stb;
        lvl_obj oa, ob;
        int pass = 0;
        memset(&sta, 0, sizeof sta);
        memset(&stb, 0, sizeof stb);
        memset(&oa, 0, sizeof oa);
        memset(&ob, 0, sizeof ob);
        if (at && bt) {
            ll_mov_parse(any_t, at);
            ll_mov_parse(bang_t, bt);
            sta.mov = at;
            stb.mov = bt;
            oa.sc = &sta;
            ob.sc = &stb;
            for (int i = 0; i < 2; i++) {
                obj_advance(&oa, LL_ITER_MS);
                obj_advance(&ob, LL_ITER_MS);
            }
            obj_advance(&oa, LL_ITER_MS);
            obj_advance(&ob, LL_ITER_MS);
            if (oa.sti != 1)
                fprintf(stderr, "object state selftest: FAILED the"
                                " `|AC_TIME,AC_EDGE` row refused the raised"
                                " AC_TIME alone (state %d)\n", oa.sti);
            else if (ob.sti != 1)
                fprintf(stderr, "object state selftest: FAILED the"
                                " `=!AC_TIME,AC_ONAIR` row refused while"
                                " only AC_EDGE - in NEITHER name - was"
                                " up (state %d)\n", ob.sti);
            else
                pass = 1;
        }
        free(at);
        free(bt);
        if (!pass)
            goto out;
    }
    {   /* PR-265 red: the transition pass is UNCONDITIONAL. The step
         * 0x536f0 composes [obj+0x60] and calls the pass 0x53520 at
         * 0x5373c with no test of the set anywhere on the path
         * (0x536ff-0x5374b, and 0x5373c is the pass's ONLY call site
         * in the image), and 0x52000 then asks EVERY entry (0x52024-
         * 0x5203a). So on a step whose set is EMPTY the match still
         * runs, and TEventSet::match's disjoint arm (op 1: the AND of
         * the two sets IS empty, 0x47edc/0x47f35, PR-264) answers TRUE
         * for `=!X` rows. The port's object step skipped the whole
         * pass when nothing was armed (`o->sig || brk1`) - no engine
         * counterpart. KADMAR.07's stairs text (SCHODY_SCH01..03 line
         * 10, `A_...(...)=!K_GEN3`) is exactly that row shape; a
         * clean object takes it at its FIRST step. The control: a
         * plain subset row on an empty set (op 0) stays false. */
        static const char ung_eq[] =
            "A_RUN\nA_DEAD\n\n[A_RUN]\ncontrol=BY_TIME\n"
            "A_DEAD(1)=!K_GEN3\n1,100\n2,100,GEN K_GEN3\n\n[A_DEAD]\n"
            "control=BY_TIME\n3,100\n";
        static const char ung_ne[] =
            "A_RUN\nA_DEAD\n\n[A_RUN]\ncontrol=BY_TIME\n"
            "A_DEAD(1)=AC_ENEMY\n1,100\n2,100\n\n[A_DEAD]\n"
            "control=BY_TIME\n3,100\n";
        ll_mov_t *ue = calloc(1, sizeof *ue), *un = calloc(1, sizeof *un);
        obj_script sue, sun;
        lvl_obj ea, en;
        int pass = 0;
        memset(&sue, 0, sizeof sue);
        memset(&sun, 0, sizeof sun);
        memset(&ea, 0, sizeof ea);
        memset(&en, 0, sizeof en);
        if (ue && un) {
            ll_mov_parse(ung_eq, ue);
            ll_mov_parse(ung_ne, un);
            sue.mov = ue;
            sun.mov = un;
            ea.sc = &sue;
            en.sc = &sun;
            obj_advance(&ea, LL_ITER_MS);
            obj_advance(&en, LL_ITER_MS);
            if (ea.sti != 1)
                fprintf(stderr, "object state selftest: FAILED the"
                                " `=!K_GEN3` row was NOT taken on the"
                                " first, empty-set step (state %d)\n",
                        ea.sti);
            else if (en.sti != 0)
                fprintf(stderr, "object state selftest: FAILED an"
                                " unraised `AC_ENEMY` subset row was taken"
                                " on an empty set (state %d)\n", en.sti);
            else
                pass = 1;
        }
        free(ue);
        free(un);
        if (!pass)
            goto out;
    }
    {   /* PR-324: the answer-6 cursor jump on the OBJECT row clock,
         * the other half of F2b. PR-322 read that 0x520c0's chain
         * walk rewrites [rec+4] (0x45d90/0x45dc0) and 0x52140 then
         * skips the WHOLE normal step, head test included; the
         * scene objects share the law - RYBA, KAMIEN, SEP, SPLYW,
         * KADMAR2_SCHODY and the rest of the 102 states. G3-O is
         * the absolute shape: row 0 carries `goto 3` (the parse
         * stores 2, 0x46777), and its `M 100,0` moves the body, so
         * after the first step the JUMPED object sits at x=100 with
         * fi=2 and no list-end bit. Closing that jumped-to LAST
         * row at the end of step 1 is an ordinary last-row close -
         * the bit arms then; the word-less control arms its own
         * one step later, at the end of its step 2, and its
         * declared AC_BREAK_NEXT transition is taken at its step
         * 4 - the bit is tested by the composition of a LATER
         * step than the one that armed it (PR-135) - so it
         * LEAVES the state - while the jumped
         * object, which declares none, re-runs row 1 and its
         * word: the state cycles jump, wrap, jump, wrap - the
         * held-loop pattern KADMAR2's `gotorel 0` rows make
         * explicit.
         * G4-O is the loop shape KADMAR2_SCHODY actually ships:
         * `4,100,GOTO_REL -3` from the last row back to row 1,
         * where every row moves -30 - no wrap and no bit ever, so
         * the object walks through the seam (0, -30, -60, -90,
         * then the jumped close gives -120 where the plain wrap
         * lands back at the base 0). */
        static const char go_abs[] =
            "A_GO\nA_END\n\n[A_GO]\ncontrol=BY_TIME\n"
            "1,100,goto 3,M 100,0\n2,100\n"
            "3,100\n\n[A_END]\ncontrol=BY_TIME\n1,100\n";
        static const char go_abs_plain[] =
            "A_GO\nA_END\n\n[A_GO]\ncontrol=BY_TIME\n"
            "A_END(1)=AC_BREAK_NEXT\n1,100,M 100,0\n2,100\n"
            "3,100\n\n[A_END]\ncontrol=BY_TIME\n1,100\n";
        static const char go_rel[] =
            "A_HO\nA_END\n\n[A_HO]\ncontrol=BY_TIME\n"
            "1,100,M -30,0\n2,100,M -30,0\n"
            "3,100,M -30,0\n4,100,M -30,0,GOTO_REL -2\n\n[A_END]\n"
            "control=BY_TIME\n1,100\n";
        static const char go_rel_plain[] =
            "A_HO\nA_END\n\n[A_HO]\ncontrol=BY_TIME\n"
            "A_END(1)=AC_BREAK_NEXT\n1,100,M -30,0\n2,100,M -30,0\n"
            "3,100,M -30,0\n4,100,M -30,0\n\n[A_END]\n"
            "control=BY_TIME\n1,100\n";
        ll_mov_t *ga = calloc(1, sizeof *ga), *gp2 = calloc(1, sizeof *gp2);
        ll_mov_t *gr = calloc(1, sizeof *gr), *gp3 = calloc(1, sizeof *gp3);
        obj_script sga, sgp, sgr, sgp3;
        lvl_obj ja, jp, lr, lp;
        long long ax[5], cx[5], rx[6], px[6];
        int afi[5], cfi[5], rfi[6];
        int ast[5], lst[5], cst[5];
        int lp_st = -1, lp_fi = -1, lr_st = -1;
        unsigned abr[5];
        unsigned lp_brk = 9u;
        int pass = 0;
        memset(&sga, 0, sizeof sga);
        memset(&sgp, 0, sizeof sgp);
        memset(&sgr, 0, sizeof sgr);
        memset(&sgp3, 0, sizeof sgp3);
        memset(&ja, 0, sizeof ja);
        memset(&jp, 0, sizeof jp);
        memset(&lr, 0, sizeof lr);
        memset(&lp, 0, sizeof lp);
        int pa = 0, pb = 0, pc = 0, pd = 0;
        if (ga && gp2 && gr && gp3) {
            pa = ll_mov_parse(go_abs, ga);
            pb = ll_mov_parse(go_abs_plain, gp2);
            pc = ll_mov_parse(go_rel, gr);
            pd = ll_mov_parse(go_rel_plain, gp3);
        }
        if (pa == 2 && pb == 2 && pc == 2 && pd == 2) {
            sga.mov = ga;
            sgp.mov = gp2;
            sgr.mov = gr;
            sgp3.mov = gp3;
            ja.sc = &sga;
            jp.sc = &sgp;
            lr.sc = &sgr;
            lp.sc = &sgp3;
            for (int it = 0; it < 4; it++) {
                obj_advance(&ja, LL_ITER_MS);
                obj_advance(&jp, LL_ITER_MS);
                ax[it] = ja.x;
                cx[it] = jp.x;
                afi[it] = ja.fi;
                ast[it] = ja.sti;
                cfi[it] = jp.fi;
                cst[it] = jp.sti;
                abr[it] = ja.brk_arm;
            }
            for (int it = 0; it < 5; it++) {
                obj_advance(&lr, LL_ITER_MS);
                obj_advance(&lp, LL_ITER_MS);
                rx[it] = lr.x;
                px[it] = lp.x;
                rfi[it] = lr.fi;
                lst[it] = lr.sti;
                if (it == 4) {      /* the control's state change: its */
                    obj_advance(&lp, LL_ITER_MS);   /* armed bit is TESTED */
                    lp_st = lp.sti;                 /* by this extra step, */
                    lp_fi = lp.fi;                  /* which then admits   */
                    lp_brk = lp.brk_arm;            /* A_END's one row.    */
                }
            }
            lr_st = lr.sti;
            if (ga->st[0].fr[0].gotocmd != 1 ||
                ga->st[0].fr[0].gotoval != 2)
                fprintf(stderr, "object state selftest: FAILED the goto "
                                "word parsed as cmd %d val %d (want 1, 2)\n",
                        ga->st[0].fr[0].gotocmd,
                        ga->st[0].fr[0].gotoval);
            else if (ast[0] != 0 || afi[0] != 2 || ax[0] != 100 ||
                     abr[0] != 0u)
                fprintf(stderr, "object state selftest: FAILED the "
                                "absolute jump (state %d fi %d x %lld "
                                "brk %u; want 0, 2, 100, 0)\n",
                        ast[0], afi[0], ax[0], abr[0]);
            else if (afi[1] != 0 || abr[1] != 1u || ax[1] != 0 ||
                     rfi[3] != 1 || rfi[4] != 2)
                fprintf(stderr, "object state selftest: FAILED the step "
                                "after the jump (fi %d brk %u x %lld, rel "
                                "cursor %d %d; want 0, 1, 0, 1, 2)\n",
                        afi[1], abr[1], ax[1], rfi[3], rfi[4]);
            else if (ast[2] != 0 || afi[2] != 2 || abr[2] != 0u ||
                     ax[2] != 100 || cx[2] != 0)
                fprintf(stderr, "object state selftest: FAILED the cycle "
                                "after the jump (fi %d brk %u x %lld, "
                                "control x %lld; want 2, 0, 100, 0)\n",
                        afi[2], abr[2], ax[2], cx[2]);
            else if (cst[3] != 1 || jp.fi != 0 || cfi[0] != 1)
                fprintf(stderr, "object state selftest: FAILED the control's "
                                "transition (state %d fi %d at its step 4)\n",
                        cst[3], jp.fi);
            else if (lst[0] != 0 || lst[4] != 0 ||
                     lr_st != 0 || lp_brk != 1u || lp_st != 1 ||
                     lp_fi != 0 || px[4] != 0)
                fprintf(stderr, "object state selftest: FAILED the loop "
                                "controls (rel state %d, plain state %d "
                                "fi %d x %lld)\n", lr_st, lp_st,
                        lp_fi, px[4]);
            else if (afi[3] != 0 ||
                     rfi[0] != 1 || rfi[1] != 2 || rfi[2] != 3 ||
                     rfi[3] != 1 || rfi[4] != 2)
                fprintf(stderr, "object state selftest: FAILED the cursor "
                                "cycle (abs %d, rel %d %d %d %d %d)\n",
                        afi[3], rfi[0], rfi[1], rfi[2], rfi[3],
                        rfi[4]);
            else if (ax[3] != 0 ||
                     rx[0] != -30 || rx[1] != -60 || rx[2] != -90 ||
                     rx[3] != -120 || rx[4] != -150)
                fprintf(stderr, "object state selftest: FAILED the motion "
                                "across the jumps (abs %lld %lld, rel "
                                "%lld %lld %lld %lld %lld)\n",
                        ax[2], ax[3], rx[0], rx[1], rx[2], rx[3], rx[4]);
            else
                pass = 1;
        }
        free(ga);
        free(gp2);
        free(gr);
        free(gp3);
        if (!pass)
            goto out;
    }
    rc = 0;
out:
    free(m1);
    free(m2);
    return rc;
}

/* PR-79 selftest, data-free: BY_X states walk at the port's object
 * scale F/4 px/s toward their T target, advance their frames by
 * MOVEMENT UNITS (the frame list's second field, not ms - PR-110 made
 * that feed real, see the x/row split below), keep their position
 * across state-list loops, and flip at the bounds= patrol range. A
 * time-driven or wrong-scale mutation fails here: time-driven never
 * moves x and feeds the row clock LL_ITER_MS at a time instead; F px/s
 * would move 4x too far; a position reset at the state loop lands on
 * the anchor. */

/* PR-163: the AC_TIME self-raise law, unit level (engine ENTER
 * 0x52070/0x52072 + the same-state match arm 0x53626-0x5365c):
 * time=<lo>[,<hi>] rolls a gate at ENTER (rand = the Borland LCG at
 * 0x76bcc), same-state transition MATCHES bump the counter, and when
 * the count reaches the gate the object's own live set gets bit 9,
 * which its own AC_TIME line can then take. NO time= line = gate 0 =
 * never. Every leg is data-free and hand-derived: a fresh object's
 * LCG starts at 0, whose first output is 0 (so gate = lo for ANY
 * pair) and whose second is 21468. */
static int obj_actime_selftest(void)
{
    /* [A_ONE] counts its own K_GEN1 matches behind a gated AC_TIME
     * exit; [A_TWO] authors NO time= line. */
    static const char gated[] =
        "A_ONE\nA_TWO\n\n[A_ONE]\ncontrol=BY_TIME\ntime=2,50\n"
        "A_ONE(1)=K_GEN1\nA_TWO(1)=AC_TIME\n1,0\n\n"
        "[A_TWO]\ncontrol=BY_TIME\nA_TWO(1)=K_GEN1\n1,0\n";
    /* no time= anywhere: the same shape must never raise */
    static const char plain[] =
        "A_ONE\n\n[A_ONE]\ncontrol=BY_TIME\nA_ONE(1)=K_GEN1\n1,0\n";
    /* single-value time=2 pairs to (2,2) - the engine's scanf form
     * (the authored time=350 shape); [A_TWO]'s time=1,10 then rolls
     * the SECOND rand output (21468): gate = 1 + 9*21468/32767 = 6,
     * hand-derived. */
    static const char single[] =
        "A_ONE\nA_TWO\n\n[A_ONE]\ncontrol=BY_TIME\ntime=2\n"
        "A_ONE(1)=K_GEN1\nA_TWO(1)=AC_TIME\n1,0\n\n"
        "[A_TWO]\ncontrol=BY_TIME\ntime=1,10\nA_TWO(1)=K_GEN1\n1,0\n";
    /* the death force: [A_ONE] holds a live gate and counter when a
     * lethal hit forces A_DEAD - ENTER must clear both there too. */
    static const char deadf[] =
        "A_ONE\nA_DOSTAL\nA_DEAD\n\n[A_ONE]\ncontrol=BY_TIME\ntime=2\n"
        "A_ONE(1)=K_GEN1\n1,0\n\n[A_DOSTAL]\ncontrol=BY_TIME\n1,0\n\n"
        "[A_DEAD]\ncontrol=BY_TIME\n1,0\n";
    int rc = 1, i = 0, r = 0;
    ll_mov_t *mg = calloc(1, sizeof *mg), *mp = calloc(1, sizeof *mp),
             *ms = calloc(1, sizeof *ms), *md = calloc(1, sizeof *md);
    obj_script sg, sp, ss, sd;
    lvl_obj a, c, b;

    if (!mg || !mp || !ms || !md)
        goto out;
    memset(&sg, 0, sizeof sg);
    memset(&sp, 0, sizeof sp);
    memset(&ss, 0, sizeof ss);
    sg.mov = mg;
    sp.mov = mp;
    ss.mov = ms;
    if (ll_mov_parse(gated, mg) != 2 || ll_mov_parse(plain, mp) != 1 ||
        ll_mov_parse(single, ms) != 2) {
        fprintf(stderr, "object actime selftest: FAILED parse\n");
        goto out;
    }
    if (mg->st[0].time_lo != 2 || mg->st[0].time_hi != 50 ||
        mg->st[1].time_lo != 0 || ms->st[0].time_lo != 2 ||
        ms->st[0].time_hi != 2 || ms->st[1].time_lo != 1 ||
        ms->st[1].time_hi != 10) {
        fprintf(stderr, "object actime selftest: FAILED a time= pair did"
                        " not land in the state record\n");
        goto out;
    }
    /* LEG 1 (range pair, gated wait): the placement ENTER is the
     * deferred one at the object's first scripted call; the fresh
     * LCG's first output is 0, so a gate rolled from (2,50) is 2. */
    memset(&a, 0, sizeof a);
    a.sc = &sg;
    a.sti = 0;
    obj_advance(&a, 0);                     /* ENTER only: zero steps */
    if (!a.act_init || a.act_gate != 2 || a.act_n != 0) {
        fprintf(stderr, "object actime selftest: FAILED the placement"
                        " ENTER did not roll the gate (init %u gate"
                        " %u n %u)\n", a.act_init, a.act_gate, a.act_n);
        goto out;
    }
    for (i = 1; i <= 2; i++) {
        a.sig = 1ull << LL_SIG_K_GEN1;      /* what a row's GEN tail */
        r = obj_transition(&a, mg, 0);      /* arms for a step; brk 0:  */
        if (i == 1) {                       /* ONLY the count can  */
            if (r != 0 || a.sti != 0 || a.act_n != 1 ||
                ((a.sig >> LL_SIG_AC_TIME) & 1)) {
                fprintf(stderr, "object actime selftest: FAILED match 1"
                                " of a gate-2 wait counted or raised"
                                " wrong (r %d n %u state %d)\n",
                        r, a.act_n, a.sti);
                goto out;
            }
        } else if (r != 1 || a.sti != 1) {  /* the state change IS the */
            fprintf(stderr, "object actime selftest: FAILED the count"
                            " reaching its gate did not raise AC_TIME"
                            " through the object's own line (r %d n %u"
                            " state %d)\n", r, a.act_n, a.sti);
            goto out;
        }
    }
    /* The change itself proves the raise: the AC_TIME line was tested
     * with brk = 0, so only bit 9 raised by the count could take it.
     * And the change ran ENTER on the NEW record: A_TWO authors no
     * time=, so its gate must be 0 and its counter must start clean
     * (this also kills a stale-gate mutant: A_ONE's leftover 2 would
     * fire at the second match below). */
    if (a.act_n != 0 || a.act_gate != 0) {
        fprintf(stderr, "object actime selftest: FAILED the state change"
                        " left a counter or gate (n %u gate %u)\n",
                a.act_n, a.act_gate);
        goto out;
    }
    /* A_TWO has no time= AND the change entered it: one match there
     * must leave the gate at 0, not A_ONE's leftover. This is the
     * leg that kills a gate-zero mutant at the ENTER's no-time= arm. */
    a.sig = 1ull << LL_SIG_K_GEN1;
    if (obj_transition(&a, mg, 0) != 0 || a.sti != 1 ||
        a.act_gate != 0 || a.act_n != 1 ||
        ((a.sig >> LL_SIG_AC_TIME) & 1)) {
        fprintf(stderr, "object actime selftest: FAILED a match in the"
                        " just-entered no-time= state (gate %u n %u"
                        " state %d)\n", a.act_gate, a.act_n, a.sti);
        goto out;
    }
    /* LEG 2: A_TWO has no time= - ten more matches, never a raise. */
    for (i = 1; i <= 10; i++) {
        a.sig = 1ull << LL_SIG_K_GEN1;
        r = obj_transition(&a, mg, 0);
        if (r != 0 || a.sti != 1 || ((a.sig >> LL_SIG_AC_TIME) & 1))
            break;
    }
    if (i <= 10 || a.act_n != 11) {
        fprintf(stderr, "object actime selftest: FAILED a state without"
                        " any time= line raised AC_TIME (n %u state"
                        " %d)\n", a.act_n, a.sti);
        goto out;
    }
    /* LEG 3: the whole-script never case. */
    memset(&c, 0, sizeof c);
    c.sc = &sp;
    obj_advance(&c, 0);
    if (c.act_gate != 0) {
        fprintf(stderr, "object actime selftest: FAILED ENTER rolled a"
                        " gate for a state with no time= line (%u)\n",
                c.act_gate);
        goto out;
    }
    for (i = 1; i <= 6; i++) {
        c.sig = 1ull << LL_SIG_K_GEN1;
        if (obj_transition(&c, mp, 0) != 0 ||
            ((c.sig >> LL_SIG_AC_TIME) & 1))
            break;
    }
    if (i <= 6 || c.act_n != 6) {
        fprintf(stderr, "object actime selftest: FAILED a gate-0 state"
                        " raised AC_TIME (n %u)\n", c.act_n);
        goto out;
    }
    /* LEG 4 (the single-value form + the re-roll): time=2 pairs to
     * (2,2) and its roll degenerates to gate = 2 deterministically;
     * the state change ENTER rolls again - the fresh LCG's SECOND
     * output is 21468, and the port's reading of the engine's lerp
     * on (1,10) is 1 + 9*21468/32767 = 6. */
    memset(&b, 0, sizeof b);
    b.sc = &ss;
    obj_advance(&b, 0);
    if (b.act_gate != 2) {
        fprintf(stderr, "object actime selftest: FAILED the single-value"
                        " form did not roll a deterministic gate (%u)\n",
                b.act_gate);
        goto out;
    }
    b.sig = 1ull << LL_SIG_K_GEN1;
    if (obj_transition(&b, ms, 0) != 0 || b.sti != 0 || b.act_n != 1) {
        fprintf(stderr, "object actime selftest: FAILED match 1 of the"
                        " single-value wait (state %d n %u)\n",
                b.sti, b.act_n);
        goto out;
    }
    b.sig = 1ull << LL_SIG_K_GEN1;
    if (obj_transition(&b, ms, 0) != 1 || b.sti != 1 || b.act_n != 0 ||
        b.act_gate != 6) {
        fprintf(stderr, "object actime selftest: FAILED the state-change"
                        " ENTER (gate %d n %u state %d): expected gate 6"
                        " from the second LCG output 21468\n",
                b.act_gate, b.act_n, b.sti);
        goto out;
    }
    for (i = 1; i <= 5; i++) {
        b.sig = 1ull << LL_SIG_K_GEN1;
        obj_transition(&b, ms, 0);
    }
    if (b.act_n != 5 || b.sti != 1 || ((b.sig >> LL_SIG_AC_TIME) & 1)) {
        fprintf(stderr, "object actime selftest: FAILED the gate-6 wait"
                        " raised early (n %u state %d)\n",
                b.act_n, b.sti);
        goto out;
    }
    b.sig = 1ull << LL_SIG_K_GEN1;
    if (obj_transition(&b, ms, 0) != 0 || b.sti != 1 ||
        !((b.sig >> LL_SIG_AC_TIME) & 1)) {
        fprintf(stderr, "object actime selftest: FAILED the 6th match of"
                        " a gate-6 wait did not raise (n %u state %d)\n",
                b.act_n, b.sti);
        goto out;
    }
    /* LEG 5: the death force is a forced state change and runs ENTER
     * too (the port mirrors the applier's arm at its two forced
     * sites). The observable is a leftover live gate or counter in
     * A_DEAD: this fixture carries gate 2 and count 1 into the hit. */
    if (ll_mov_parse(deadf, md) != 3) {
        fprintf(stderr, "object actime selftest: FAILED the death"
                        " fixture did not parse\n");
        goto out;
    }
    memset(&sd, 0, sizeof sd);
    sd.mov = md;
    memset(&a, 0, sizeof a);              /* fresh: reuse a, mg done */
    a.sc = &sd;
    obj_advance(&a, 0);                   /* placement ENTER: (2,2) */
    if (a.act_gate != 2) {
        fprintf(stderr, "object actime selftest: FAILED the death"
                        " fixture did not arm gate 2 (%u)\n", a.act_gate);
        goto out;
    }
    a.sig = 1ull << LL_SIG_K_GEN1;
    obj_transition(&a, md, 0);            /* count 1 of the gate of 2 */
    a.hp = 1;
    if (obj_hit(&a, 15) != 2 || a.sti != 2 || a.act_gate != 0 ||
        a.act_n != 0) {
        fprintf(stderr, "object actime selftest: FAILED the forced"
                        " A_DEAD change did not run ENTER (sti %d gate"
                        " %u n %u)\n", a.sti, a.act_gate, a.act_n);
        goto out;
    }
    rc = 0;
out:
    free(mg);
    free(mp);
    free(ms);
    free(md);
    return rc;
}

/* PR-172: the row-clock zeroing law (engine applier entry 0x535a0:
 * the state-change arm calls the NEW record's ENTER at
 * [[rec+0x44]+0x1c] - the real class tables carry the wrapper 0x4fef0
 * there, whose tail call 0x52360 zeroes the ONE feed accumulator
 * [obj+0xa1], PR-167 F4; the same-state match arm 0x53626 calls NO
 * ENTER, so the accumulator CARRIES). The feed is the constant 100
 * per iteration (0x4c8b7), and the authored ZARIA.01 BY_TIME thresh-
 * olds include NON-multiples of 100 - 1, 40, 75, 80, 120, 150, 225
 * across 114 states / 1128 rows - so a leftover value is OBSERVABLE
 * at the new state's first row: a fresh register and a carried one
 * open rows at different steps. This fixture (gates 70/240 in
 * [A_ONE], 120 in [A_TWO], 150 in [A_DEAD] - no gate is a multiple
 * of 100, every number below is hand-derived) pins all three arms:
 * the subtract-remainder carry within a state, carry across a SAME-
 * STATE match, zeroing at a keyed transition, and zeroing at the
 * forced A_DEAD hit. */
static int obj_rowzero_selftest(void)
{
    static const char rz[] =
        "A_ONE\nA_TWO\nA_DEAD\n\n[A_ONE]\ncontrol=BY_TIME\n"
        "A_TWO(1)=K_GEN1\nA_ONE(1)=K_GEN2\n1,70\n2,240\n\n"
        "[A_TWO]\ncontrol=BY_TIME\n1,120\n\n[A_DEAD]\n"
        "control=BY_TIME\n1,150\n";
    int rc = 1, r = 0;
    ll_mov_t *m = calloc(1, sizeof *m);
    obj_script sc;
    lvl_obj a;

    if (!m)
        goto out;
    memset(&sc, 0, sizeof sc);
    sc.mov = m;
    if (ll_mov_parse(rz, m) != 3) {
        fprintf(stderr, "object rowzero selftest: FAILED parse\n");
        goto out;
    }
    memset(&a, 0, sizeof a);
    a.sc = &sc;
    obj_advance(&a, 0);                   /* placement ENTER only:    */
    if (a.row_t != 0u || a.act_gate != 0u) {   /* zero steps, no gate  */
        fprintf(stderr, "object rowzero selftest: FAILED a fresh"
                        " object (gate %u t %u)\n", a.act_gate,
                a.row_t);
        goto out;
    }
    /* LEG 1 (the carry WITHIN a state): row 1 of [A_ONE] gates at
     * 70. One iteration feeds 100, the row opens, the remainder 30
     * carries - exactly the effect the non-multiple authored gates
     * make in the engine. Rows stay held at 130, 230 under gate
     * 240, then 330 opens row 2 with remainder 90; the admit of the
     * LAST row wraps fi to 0 in that same step (the PR-135 clamp). */
    obj_advance(&a, LL_ITER_MS);
    if (a.sti != 0 || a.fi != 1 || a.row_t != 30u) {
        fprintf(stderr, "object rowzero selftest: FAILED the first"
                        " row of gate 70 (state %d fi %d t %u want"
                        " fi 1 t 30)\n", a.sti, a.fi, a.row_t);
        goto out;
    }
    obj_advance(&a, LL_ITER_MS);
    obj_advance(&a, LL_ITER_MS);
    if (a.fi != 1 || a.row_t != 230u) {
        fprintf(stderr, "object rowzero selftest: FAILED the carry"
                        " under gate 240 (fi %d t %u want fi 1 t"
                        " 230)\n", a.fi, a.row_t);
        goto out;
    }
    obj_advance(&a, LL_ITER_MS);
    if (a.fi != 0 || a.row_t != 90u) {
        fprintf(stderr, "object rowzero selftest: FAILED row 2 of"
                        " gate 240 (fi %d t %u want fi 0 t 90)\n",
                a.fi, a.row_t);
        goto out;
    }
    /* LEG 2 (a SAME-STATE match CARRIES): the engine's 0x53626 arm
     * bumps the AC_TIME counter and calls no ENTER. gate 0 (no
     * time= line) can never raise, and the register keeps its 90 -
     * the half a zero-at-match mutant loses. K_GEN2 is the same-
     * state line's token so this call matches ONLY that one. */
    a.sig = 1ull << LL_SIG_K_GEN2;
    r = obj_transition(&a, m, 0);
    a.sig = 0;                 /* the real step clears the set at its  */
    if (r != 0 || a.sti != 0 || a.act_n != 1u || a.row_t != 90u) {
                               /* top; a direct call must not leak it */
        fprintf(stderr, "object rowzero selftest: FAILED the same-"
                        "state match (r %d state %d n %u t %u want"
                        " r 0 n 1 t 90)\n", r, a.sti, a.act_n,
                a.row_t);
        goto out;
    }
    /* And the carry is USED: the next iteration feeds on from 90 -
     * 190 opens row 1 (gate 70) again, remainder 120. A zero-at-
     * match mutant would land 30. (The step's own key pre-pass
     * finds nothing: the K_GEN1/K_GEN2 bits are clear, and the
     * wrap bit c4 armed tests only numeric conditions.) */
    obj_advance(&a, LL_ITER_MS);
    if (a.sti != 0 || a.fi != 1 || a.row_t != 120u) {
        fprintf(stderr, "object rowzero selftest: FAILED the carry"
                        " across a same-state match (fi %d t %u want"
                        " fi 1 t 120)\n", a.fi, a.row_t);
        goto out;
    }
    /* LEG 3 (the TRANSITION zeroes): the applier's state-change arm
     * runs ENTER on the new record - row_t must be 0 there (the
     * carried 120 is the M1 mutant, caught here and again below). */
    a.sig = 1ull << LL_SIG_K_GEN1;
    r = obj_transition(&a, m, 0);
    a.sig = 0;
    if (r != 1 || a.sti != 1 || a.fi != 0 || a.row_t != 0u ||
        a.act_n != 0u || a.act_gate != 0u) {
        fprintf(stderr, "object rowzero selftest: FAILED the keyed"
                        " transition (r %d state %d fi %d t %u n %u"
                        " want r 1 state 1 fi 0 t 0 n 0)\n", r,
                a.sti, a.fi, a.row_t, a.act_n);
        goto out;
    }
    /* Observable at the new state's FIRST row: [A_TWO] gates at
     * 120. A carried 120 would open it at once (220 >= 120); the
     * fresh register holds at 100 and opens one step later (the
     * last row, so fi wraps back to 0 with remainder 80). */
    obj_advance(&a, LL_ITER_MS);
    if (a.sti != 1 || a.fi != 0 || a.row_t != 100u) {
        fprintf(stderr, "object rowzero selftest: FAILED the first"
                        " row of the NEW state (state %d fi %d t %u"
                        " want fi 0 t 100)\n", a.sti, a.fi,
                a.row_t);
        goto out;
    }
    obj_advance(&a, LL_ITER_MS);
    if (a.fi != 0 || a.row_t != 80u) {
        fprintf(stderr, "object rowzero selftest: FAILED row 1 of"
                        " gate 120 (fi %d t %u want fi 0 t 80)\n",
                a.fi, a.row_t);
        goto out;
    }
    /* LEG 4 (the forced A_DEAD change runs the same ENTER): the
     * lethal hit lands with 80 still in the register and must
     * zero it; [A_DEAD]'s gate of 150 makes that observable - a
     * carried 80 would open the corpse's first row at once (180
     * >= 150). Fresh, it holds at 100 and opens next step (50). */
    if (obj_hit(&a, 30) != 2 || a.sti != 2 || a.fi != 0 ||
        a.row_t != 0u) {
        fprintf(stderr, "object rowzero selftest: FAILED the forced"
                        " A_DEAD change (state %d fi %d t %u want"
                        " state 2 fi 0 t 0)\n", a.sti, a.fi,
                a.row_t);
        goto out;
    }
    obj_advance(&a, LL_ITER_MS);
    if (a.fi != 0 || a.row_t != 100u) {
        fprintf(stderr, "object rowzero selftest: FAILED the first"
                        " row after death (fi %d t %u want fi 0 t"
                        " 100)\n", a.fi, a.row_t);
        goto out;
    }
    obj_advance(&a, LL_ITER_MS);
    if (a.fi != 0 || a.row_t != 50u) {
        fprintf(stderr, "object rowzero selftest: FAILED row 1 of"
                        " gate 150 (fi %d t %u want fi 0 t 50)\n",
                a.fi, a.row_t);
        goto out;
    }
    rc = 0;
out:
    free(m);
    return rc;
}

/* PR-174: the same-state match arm writes the authored start row into
 * the CURRENT state's row counter - the engine's first act at
 * 0x5360f-0x53616 (`mov esi,[eax+0x4]; cmp esi,edx; je skip; mov
 * [eax+0x4],edx`), which sits BEFORE the 0x53626 count arm and calls
 * no ENTER. PR-107's settlement applies to this path: the value is
 * stored raw, an absent token lands on row 0 at the next tick through
 * the stepper's wrap (0x521c5) - "the same visible result" as the top
 * - and the port's mapping is the shared ll_mov_row_start (entry ->
 * entry-1; empty/non-numeric/over-long -> the top, the recorded
 * model choices). The row the seek writes is the LIVE row: its
 * threshold gates the next advance (legs B and the advances after
 * C/E). ZARIA.01 grounding for the record: ZERO self-target lines in
 * any ZARIA.01 scene-object script (the port-parser census
 * reproduces PR-107's 101 exactly: LION 77 / SCENE 24; all 20
 * ZARIA.01 ones are the lion's own scripts) - so this behaviour is
 * law-completion for the object path, observable on KADMAR/LONA-
 * class data, and no shipped ZARIA.01 object output moves. */
static int obj_rowseek_selftest(void)
{
    static const char rs[] =
        "A_ONE\nA_TWO\n\n[A_ONE]\ncontrol=BY_TIME\n"
        "A_TWO(1)=K_GEN1\nA_ONE(3)=K_GEN2\nA_ONE(1)=K_GEN5\n"
        "A_ONE()=K_GEN3\nA_ONE(9)=K_GEN4\n"
        "1,70\n2,240\n3,70\n4,70\n5,70\n6,70\n\n[A_TWO]\n"
        "control=BY_TIME\nA_ONE(4)=K_GEN2\n1,100\n";
    int rc = 1;
    ll_mov_t *m = calloc(1, sizeof *m);
    obj_script sc;
    lvl_obj a;

    if (!m)
        goto out;
    memset(&sc, 0, sizeof sc);
    sc.mov = m;
    if (ll_mov_parse(rs, m) != 2 || m->st[0].ntr != 5 ||
        m->st[0].nfr != 6) {
        fprintf(stderr, "object rowseek selftest: FAILED parse"
                        " (nst %d ntr %d nfr %d)\n", m->nst,
                        m->st[0].ntr, m->st[0].nfr);
        goto out;
    }
    memset(&a, 0, sizeof a);
    a.sc = &sc;
    obj_advance(&a, LL_ITER_MS);          /* row 0 (gate 70) opens:   */
    if (a.fi != 1 || a.row_t != 30u) {   /* fi 1, remainder 30        */
        fprintf(stderr, "object rowseek selftest: FAILED the opening"
                        " step (fi %d t %u want fi 1 t 30)\n", a.fi,
                a.row_t);
        goto out;
    }
    /* LEG A: the seek to (3) lands on row 2 and the object KEEPS the
     * state (the M1 no-write and M2 raw-store mutants land 1 and 3;
     * M6, which gates the write on a rolled AC_TIME gate, lands 1:
     * this state has no time= line and the seek is unconditional). */
    a.sig = 1ull << LL_SIG_K_GEN2;
    obj_transition(&a, m, 0);
    a.sig = 0;
    if (a.sti != 0 || a.fi != 2) {
        fprintf(stderr, "object rowseek selftest: FAILED the seek to"
                        " (3) (state %d fi %d want state 0 fi 2)\n",
                a.sti, a.fi);
        goto out;
    }
    /* LEG B: the seeked row is LIVE - its gate 70 opens this very
     * step (30+100=130, remainder 60). A seek that landed anywhere
     * else would not meet gate 70 from the carried 30. */
    obj_advance(&a, LL_ITER_MS);
    if (a.fi != 3 || a.row_t != 60u) {
        fprintf(stderr, "object rowseek selftest: FAILED advancing"
                        " from the seeked row (fi %d t %u want fi 3"
                        " t 60)\n", a.fi, a.row_t);
        goto out;
    }
    /* LEG C: the empty token (the "(x)" class the parser maps to
     * entry 0) ALSO re-seeks - to the top, from row 3. The engine
     * stores its -1 raw and the wrap lands 0; same visible row
     * (PR-107); the M3 skip-empty mutant lands fi 3 here. */
    a.sig = 1ull << LL_SIG_K_GEN3;
    obj_transition(&a, m, 0);
    a.sig = 0;
    if (a.sti != 0 || a.fi != 0) {
        fprintf(stderr, "object rowseek selftest: FAILED the empty-"
                        "token seek (state %d fi %d want state 0 fi"
                        " 0)\n", a.sti, a.fi);
        goto out;
    }
    obj_advance(&a, LL_ITER_MS);          /* the top is live: 160     */
    if (a.fi != 1 || a.row_t != 90u) {   /* opens row 0, fi 1, 90    */
        fprintf(stderr, "object rowseek selftest: FAILED advancing"
                        " from the empty-token top (fi %d t %u want"
                        " fi 1 t 90)\n", a.fi, a.row_t);
        goto out;
    }
    /* LEG D: (1) = row 0 = the top - the engine's "(1) is the
     * engine's row 0" (PR-107) - from row 1; a raw store (M2)
     * lands 1 and a skip-small-token mutant lands 1 here too. */
    a.sig = 1ull << LL_SIG_K_GEN5;
    obj_transition(&a, m, 0);
    a.sig = 0;
    if (a.sti != 0 || a.fi != 0) {
        fprintf(stderr, "object rowseek selftest: FAILED the (1)"
                        " seek (state %d fi %d want state 0 fi 0)\n",
                a.sti, a.fi);
        goto out;
    }
    obj_advance(&a, LL_ITER_MS);          /* 190 opens row 0 again:   */
    if (a.fi != 1 || a.row_t != 120u) {  /* fi 1, carry 120           */
        fprintf(stderr, "object rowseek selftest: FAILED the step"
                        " after the (1) seek (fi %d t %u want fi 1 t"
                        " 120)\n", a.fi, a.row_t);
        goto out;
    }
    /* LEG E: an over-long token (9 > 6 rows) clamps to the top - the
     * recorded PR-107 model choice; an unclamped store (M4) lands 8
     * and fails here. */
    a.sig = 1ull << LL_SIG_K_GEN4;
    obj_transition(&a, m, 0);
    a.sig = 0;
    if (a.sti != 0 || a.fi != 0) {
        fprintf(stderr, "object rowseek selftest: FAILED the over-"
                        "long clamp (state %d fi %d want state 0 fi"
                        " 0)\n", a.sti, a.fi);
        goto out;
    }
    obj_advance(&a, LL_ITER_MS);          /* the clamped top is live: */
    if (a.fi != 1 || a.row_t != 150u) {  /* 220 opens row 0: fi 1    */
        fprintf(stderr, "object rowseek selftest: FAILED advancing"
                        " from the clamped top (fi %d t %u want fi 1"
                        " t 150)\n", a.fi, a.row_t);
        goto out;
    }
    /* LEG F: the seek writes the ROW only - the register keeps the
     * carried 150 (an arm that also zeroed it - M7 - lands 30 and
     * fails here; PR-172's obj_rowzero_selftest pins the same law
     * from its own side). Seek to (3) from fi 1, step: row 2's gate
     * 70 opens (150+100=250, remainder 180) and row 2 is admitted
     * (fi 3). */
    a.sig = 1ull << LL_SIG_K_GEN2;
    obj_transition(&a, m, 0);
    a.sig = 0;
    obj_advance(&a, LL_ITER_MS);
    if (a.sti != 0 || a.fi != 3 || a.row_t != 180u) {
        fprintf(stderr, "object rowseek selftest: FAILED the carried"
                        " register across the seek (state %d fi %d t"
                        " %u want state 0 fi 3 t 180)\n", a.sti,
                a.fi, a.row_t);
        goto out;
    }
    /* LEG G (regression, PR-107): the state-CHANGE arm's authored
     * seek is untouched by this PR - first into A_TWO, then the
     * (4) line there re-joins A_ONE at row 3, mid-list. */
    a.fi = 0;
    a.row_t = 0;
    a.sig = 1ull << LL_SIG_K_GEN1;
    obj_transition(&a, m, 0);
    a.sig = 0;
    if (a.sti != 1 || a.fi != 0) {
        fprintf(stderr, "object rowseek selftest: FAILED the"
                        " state-change control (state %d fi %d want"
                        " state 1 fi 0)\n", a.sti, a.fi);
        goto out;
    }
    a.sig = 1ull << LL_SIG_K_GEN2;
    obj_transition(&a, m, 0);
    a.sig = 0;
    if (a.sti != 0 || a.fi != 3) {
        fprintf(stderr, "object rowseek selftest: FAILED the"
                        " cross-state seek to (4) (state %d fi %d"
                        " want state 0 fi 3)\n", a.sti, a.fi);
        goto out;
    }
    rc = 0;
out:
    free(m);
    return rc;
}

/* PR-217: a row's `Wait <signal>` tail is the engine's OWN row-directive
 * keyword (the chain 0x45df0, whose arms run 0x45e03 zero_v .. 0x46af4 GEN ..
 * 0x46b62 Wait .. 0x46bd0 end_level), and its parse arm 0x46b62-0x46ba2
 * allocates 0x15 bytes (class word 0xa11c8) whose operand the clause
 * parser 0x47d60 reads into the node's 12-byte TEventSet at [node+0x8] -
 * the same shape, node ctor 0x45770 and parser the modelled `GEN` arm
 * (0x46af4-0x46b34, class word 0xa11e0) uses. So the tail is a signal
 * clause, one token in 254 of the MEASURED 255 tails (133 of the
 * 777 shipped files; operands K_NEVER 159, K_GEN1 25, AC_DUP 20, AC_TIME
 * 18, 22 `!`-negated), not a magic delay. This leg pins what the parser
 * CAPTURES into ll_mov_fr.gsig_id/gneg. What the engine DOES with the
 * clause was a separate question when this leg was written, and the
 * reading here was WRONG about the branch: it said the row step "admits
 * the row only when the returned EventSet is EMPTY (0x5223c calls
 * 0x495b2, 0x5224b branches), then tests the union of that with the
 * caller's key and the object's [obj+0x60] at 0x522e6". PR-287 re-reads
 * those six instructions (test/sete/and/je at 0x52241-0x5224b): 0x495b0
 * answers 1 for an EMPTY set, and the `je` takes the plain path 0x522fc
 * - the execute slot +0xc at 0x52303 and the threshold compare at
 * 0x52329 - for exactly that case; only a NON-empty set, a `Wait` node's
 * own clause, reaches the match at 0x522e6, and its refusal ends the
 * step at 0x52354. The `Wait` vtable 0xa11c8 answers slot +0x8 with
 * 0x45970, which copies the node's own [node+0x8] and never calls its
 * operand, so a `Wait` row is gated on the live signal set and a `Wait
 * K_NEVER` row never runs. That is what the model of PR-287's
 * ll_mov_row_gate now acts on; the two halves it leaves open (the gated
 * path skips the threshold compare, and the player's row admit does not
 * consult the clause yet) stay at OI-111. ZARIA.01's own DRAB.MOV (the
 * fixture's shape, rows trimmed) is a file whose A_UP state is one
 * `WAIT K_GEN1` row and whose A_DOWN is one `WAIT K_NEVER` row. */
static int obj_rowgate_selftest(void)
{
    static const char drab[] =
        "A_UP\nA_DOWN\nA_DROP\n\n[A_UP]\ncontrol=BY_TIME\n"
        "A_DROP(1)=K_GEN1\nseq=drab\n1,0,WAIT K_GEN1\n\n[A_DROP]\n"
        "control=BY_TIME\nA_DOWN(1)=AC_BREAK_NEXT\nseq=drab\n"
        "2,500\n3,400\n\n[A_DOWN]\ncontrol=BY_TIME\nseq=drab\n"
        "11,0,WAIT K_NEVER\n";
    static const char tails[] =
        "A_ONE\n\n[A_ONE]\ncontrol=BY_TIME\n"
        "1,100,WAIT !K_DOWN\n2,100,WAIT !AC_ENEMY_ATC\n"
        "3,100,WAIT AC_DUP\n4,100,WAIT K_ANY\n5,100,WAIT AC_BOGUS\n"
        "6,100,WAIT ;\n7,100,lock\n8,100\n"
        "11,12000,WAIT K_NEVER ;,smpauto\n"
        "12,1200,WAIT !K_UP ;\n";
    static const struct { const char *mov; int st, fr;
                         int sig; int neg; int op; const char *what; }
        cs[] = {
        { drab, 0, 0, LL_SIG_K_GEN1, 0, LL_GOP_NAME,
          "ZARIA.01's ladder A_UP row 1: `WAIT K_GEN1` = the K_GEN1"
          " clause, un-negated" },
        { drab, 1, 0, -1, 0, LL_GOP_NAME,
          "the A_DROP rows carry no tail at all" },
        { drab, 1, 1, -1, 0, LL_GOP_NAME, "including its second row" },
        { drab, 2, 0, -2, 0, LL_GOP_NEVER,
          "A_DOWN row 11 `WAIT K_NEVER`: K_NEVER is one of the four"
          " operands the engine's comparator gives NO signal bit" },
        { tails, 0, 0, LL_SIG_K_DOWN, 1, LL_GOP_NAME,
          "`!K_DOWN` keeps the negation and the real bit" },
        { tails, 0, 1, LL_SIG_AC_ENEMY_ATC, 1, LL_GOP_NAME,
          "`!AC_ENEMY_ATC` resolves through the signal table, not as"
          " OTHER" },
        { tails, 0, 2, LL_SIG_AC_DUP, 0, LL_GOP_NAME,
          "`AC_DUP` is a plain signal bit" },
        { tails, 0, 3, -2, 0, LL_GOP_ANY, "`K_ANY` is the mask operand"
          " (the engine OR-s 0x2001f8 into dword 1, 0x48037-0x48048)" },
        { tails, 0, 4, -2, 0, LL_GOP_NONE, "a name in NO table stays the"
          " unresolvable special, never a guess" },
        { tails, 0, 5, -1, 0, LL_GOP_NAME, "`WAIT ;` (1 of the 255)"
          " carries no token: counted, not captured" },
        { tails, 0, 6, -1, 0, LL_GOP_NAME,
          "the other directives (`lock`) stay counted" },
        { tails, 0, 7, -1, 0, LL_GOP_NAME, "and a plain row keeps no"
          " clause" },
        { tails, 0, 8, -2, 0, LL_GOP_NEVER,
          "SADES.06's `[A_RUN]` row `WAIT K_NEVER ;,smpauto`: the"
          " clause ENDS at the `;` the engine's row text puts there,"
          " so the operand is K_NEVER and the directive after the"
          " `;` is not part of it" },
        { tails, 0, 9, LL_SIG_K_UP, 1, LL_GOP_NAME,
          "`WAIT !K_UP ;` (the ZARIA.01 `[A_LOOKUP]` shape): the `;`"
          " ends the clause without becoming part of the name" },
    };
    int rc = 1;

    for (size_t i = 0; i < sizeof cs / sizeof cs[0]; i++) {
        ll_mov_t *m = calloc(1, sizeof *m);
        if (!m)
            return 1;
        if (ll_mov_parse(cs[i].mov, m) < 1 ||
            cs[i].st >= m->nst || cs[i].fr >= m->st[cs[i].st].nfr) {
            fprintf(stderr, "row gate selftest: FAILED parse for %s"
                            " (nst %d)\n", cs[i].what, m->nst);
            free(m);
            return 1;
        }
        const ll_mov_fr *f = &m->st[cs[i].st].fr[cs[i].fr];
        if ((int)f->gsig_id != cs[i].sig || (int)f->gneg != cs[i].neg ||
            (int)f->gop != cs[i].op) {
            fprintf(stderr, "row gate selftest: FAILED %s (clause %d"
                            " neg %d op %d want clause %d neg %d"
                            " op %d)\n",
                    cs[i].what, (int)f->gsig_id, (int)f->gneg,
                    (int)f->gop, cs[i].sig, cs[i].neg, cs[i].op);
            free(m);
            return 1;
        }
        free(m);
    }
    rc = 0;
    return rc;
}

/* PR-287: and the clause ACTS. The row step 0x521f0 asks the row node's
 * test slot +0x8 at 0x52235 and 0x495b0 at 0x5223c - that call is the
 * EMPTY test (three dwords, any one non-zero answers 0, 0x495bd-
 * 0x495d5) - and takes the plain admit path 0x522fc ONLY when the set
 * it was handed is empty. A node whose +0x8 returns a NON-empty set -
 * the `Wait` class 0xa11c8 answers +0x8 with 0x45970, which copies the
 * node's own 12-byte clause at [node+0x8] and never calls its operand -
 * goes to 0x52251 instead, where the caller's key set (copied at
 * 0x5226f) and the object's [obj+0x60] (copied at 0x5229d) are merged
 * by 0x49580 at 0x522bc and matched against the clause by 0x47e40 at
 * 0x522e6; the `je 0x52354` at 0x522ed leaves the WHOLE step - no row
 * admitted, no list advance, no signal raised. So a `Wait` row is gated
 * on the live set, and a `Wait` whose operand carries no signal bit at
 * all (K_NEVER -> LL_GOP_NEVER, one of the comparator's own no-bit
 * operands, PR-217) never admits: THAT is how a shipped state holds its
 * last frame for good, and it is why ZARIA.01's ladder sits at the top
 * of its own A_DOWN instead of looping back through it. This leg runs
 * that ladder's own script end to end. Its A_UP is one
 * `1,0,WAIT K_GEN1` row, its A_DROP is the nine rows 500+400+400+300+
 * 300+200+200+200+200 = 2700 units, and its A_DOWN is one
 * `11,0,WAIT K_NEVER` row. Every tick below is the fixture's own
 * arithmetic on THIS port's row clock (PR-110: 100 units per
 * LL_ITER_MS, a row admits when the clock reaches its threshold and the
 * threshold is subtracted; PR-135: the step that CHANGES state feeds no
 * clock at all; PR-172: a state change zeroes the clock and lands on
 * the authored entry row). */
static int obj_rowgate_act_selftest(void)
{
    /* ZARIA.01/DRABINKA/drab.MOV, lines 1-30, verbatim but for the
     * `seq=`/`sample=` art keys, which this port does not model - the
     * same trim PR-217's capture fixture makes. */
    static const char drab[] =
        "A_UP\nA_DOWN\nA_DROP\n\n[A_UP]\ncontrol=BY_TIME\n"
        "A_DROP(1)=K_GEN1\n1,0,WAIT K_GEN1\n\n[A_DROP]\n"
        "control=BY_TIME\nA_DOWN(1)=AC_BREAK_NEXT\n2,500\n3,400\n"
        "4,400\n5,300\n6,300\n7,200\n8,200\n9,200\n10,200\n\n"
        "[A_DOWN]\ncontrol=BY_TIME\n11,0,WAIT K_NEVER\n";
    static const int drop_rows[] = { 500, 400, 400, 300, 300,
                                     200, 200, 200, 200 };
    ll_mov_t *m = calloc(1, sizeof *m);
    obj_script sc;
    lvl_obj o;
    int i, up = -1, drop = -1, down = -1;

    if (!m)
        return 1;
    if (ll_mov_parse(drab, m) < 3) {
        fprintf(stderr, "row gate act selftest: FAILED the ladder script"
                        " parsed to %d state(s)\n", m->nst);
        free(m);
        return 1;
    }
    up = ll_mov_find(m, "A_UP");
    drop = ll_mov_find(m, "A_DROP");
    down = ll_mov_find(m, "A_DOWN");

    if (up < 0 || drop < 0 || down < 0 ||
        m->st[drop].nfr != (int)(sizeof drop_rows / sizeof drop_rows[0])) {
        fprintf(stderr, "row gate act selftest: FAILED the script's states"
                        " (up %d drop %d down %d rows %d)\n", up, drop,
                down, m->st[drop].nfr);
        free(m);
        return 1;
    }
    memset(&sc, 0, sizeof sc);
    memset(&o, 0, sizeof o);
    sc.mov = m;
    o.sc = &sc;

    /* A_UP, iterations 1..6: the row is a 0-unit row, so an ungated
     * walker admits it every single step and wraps its one-row list,
     * arming the list-end bit; the gate holds it instead. No clock
     * reaches a threshold, so `at` stays 0 and the expected frame here
     * is the one the state opens on - and NO list-end bit ever. */
    for (i = 0; i < 6; i++)
        obj_advance(&o, LL_ITER_MS);
    if (o.sti != up || o.fi != 0 || o.brk_arm != 0u || o.sig != 0ull) {
        fprintf(stderr, "row gate act selftest: FAILED `WAIT K_GEN1` let"
                        " A_UP admit or wrap (state %d frame %d brk %u"
                        " sig %llu)\n", o.sti, o.fi, o.brk_arm,
                (unsigned long long)o.sig);
        goto out;
    }
    /* The bus names K_GEN1: the transition at 0x523f0's composition
     * matches A_UP's only row, and the state change zeroes the clock
     * and lands on the authored entry row (1) of A_DROP. */
    o.sig |= 1ull << LL_SIG_K_GEN1;
    obj_advance(&o, LL_ITER_MS);
    if (o.sti != drop || o.fi != 0 || o.row_t != 0u) {
        fprintf(stderr, "row gate act selftest: FAILED K_GEN1 did not enter"
                        " A_DROP at its authored row (state %d frame %d"
                        " t %u)\n", o.sti, o.fi, o.row_t);
        goto out;
    }
    /* A_DROP's nine rows, one admit per authored threshold: the row at
     * index k closes after drop_rows[k]/100 of this port's iterations,
     * so the walk below steps exactly that many times per row and
     * checks where the row counter stands after each - 5, 4, 4, 3, 3,
     * 2, 2, 2, 2 = 27 iterations in all. Nothing here is gated, so it
     * also checks the gate stays out of the way of an ungated list.
     * The LAST row is different by the engine's own rule: 0x521c5
     * writes 0 to the row counter and ORs the list-end bit in the SAME
     * call that admits the last row (0x521c1-0x521c5), so the counter
     * is back at the top with the bit armed, not one past the end. */
    for (size_t k = 0; k < sizeof drop_rows / sizeof drop_rows[0]; k++) {
        int last = k + 1 == sizeof drop_rows / sizeof drop_rows[0];
        for (i = 0; i < drop_rows[k] / 100; i++)
            obj_advance(&o, LL_ITER_MS);
        if (o.sti != drop || o.fi != (last ? 0 : (int)k + 1) ||
            o.brk_arm != (unsigned)last) {
            fprintf(stderr, "row gate act selftest: FAILED A_DROP row %u"
                            " (%d units) did not close after %d of them"
                            " (state %d frame %d brk %u t %u)\n",
                    (unsigned)k, drop_rows[k], drop_rows[k] / 100,
                    o.sti, o.fi, o.brk_arm, o.row_t);
            goto out;
        }
    }
    /* The list has run out and the bit is armed: the NEXT step's
     * composition is where A_DROP's `A_DOWN(1)=AC_BREAK_NEXT` lives. */
    if (o.fi != 0 || o.brk_arm != 1u) {
        fprintf(stderr, "row gate act selftest: FAILED A_DROP's 2700-unit"
                        " list did not arm the list-end bit (frame %d"
                        " brk %u)\n", o.fi, o.brk_arm);
        goto out;
    }
    obj_advance(&o, LL_ITER_MS);
    if (o.sti != down || o.fi != 0 || o.row_t != 0u) {
        fprintf(stderr, "row gate act selftest: FAILED the list-end bit did"
                        " not enter A_DOWN at its authored row (state %d"
                        " frame %d t %u)\n", o.sti, o.fi, o.row_t);
        goto out;
    }
    /* A_DOWN's one row is `11,0,WAIT K_NEVER`: the operand has no
     * signal bit, so the match at 0x522e6 can never hold and the step
     * returns at 0x522ed - the state holds frame 11 forever, and above
     * all it never wraps, so it never hands a list-end bit to a
     * transition. An ungated walker admits a 0-unit row every step and
     * arms that bit forever; anything downstream of it then reads a
     * transition this script does not author. */
    for (i = 0; i < 12; i++)
        obj_advance(&o, LL_ITER_MS);
    if (o.sti != down || o.fi != 0 || o.brk_arm != 0u || o.sig != 0ull) {
        fprintf(stderr, "row gate act selftest: FAILED `WAIT K_NEVER` let"
                        " A_DOWN admit or wrap (state %d frame %d brk %u"
                        " sig %llu)\n", o.sti, o.fi, o.brk_arm,
                (unsigned long long)o.sig);
        goto out;
    }
    free(m);
    return 0;
out:
    free(m);
    return 1;
}

static int obj_motion_selftest(void)
{
    static const char run[] =
        "A_RUN\n\n[A_RUN]\ncontrol=BY_X\nF=100,0\nT=40,0\n1,4\n2,4\n";
    static const char halt[] =
        "A_RUN\n\n[A_RUN]\ncontrol=BY_X\nF=0,0\nT=30,0\n1,4\n2,4\n";
    static const char hit[] =
        "A_RUN\nA_HIT\n\n[A_RUN]\ncontrol=BY_X\nF=100,0\nT=40,0\n"
        "A_HIT(1)=K_GEN1\n1,4,GEN K_GEN1\n2,4\n\n[A_HIT]\ncontrol=BY_TIME\n"
        "A_RUN(1)=AC_BREAK_NEXT\n1,100\n";
    static const char chd[] =
        "A_RUN\n\n[A_RUN]\ncontrol=BY_X|CH_DIR\nF=100,0\nT=40,0\n1,4\n2,4\n";
    int rc = 1;
    ll_mov_t *m1 = calloc(1, sizeof *m1), *m2 = calloc(1, sizeof *m2);
    obj_script s1, s2;
    lvl_obj a, b;

    if (!m1 || !m2)
        goto out;
    memset(&s1, 0, sizeof s1);
    memset(&s2, 0, sizeof s2);
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    ll_mov_parse(run, m1);
    ll_mov_parse(halt, m2);
    s1.mov = m1;
    s2.mov = m2;
    a.sc = &s1;
    b.sc = &s2;
    if (m1->nst != 1 || m1->st[0].fx != 100 || m1->st[0].fr[0].thresh != 4) {
        fprintf(stderr, "motion selftest: FAILED F=/units did not parse "
                        "(nst %d fx %d thresh %u)\n", m1->nst, m1->st[0].fx,
                        m1->st[0].fr[0].thresh);
        goto out;
    }
    /* 1. speed: 4 calls x 100 ms = EIGHT engine iterations at the
     * PR-133/134 law's steady walk rate F/(2T) = 100/80 = 1.25 units
     * per iteration = exactly 10 units (an F-as-px/s mutation moves
     * 40). PR-135 re-derived the count, not the total: the old
     * |F| * dt / 4000 estimate summed to the same 10 over the same
     * 400 ms, because at T=40 it hit the law's steady rate on that
     * grid; what changes is the granularity - the distance is paid
     * per 50 ms iteration, never per call, and a call that is not a
     * whole number of iterations leaves the remainder in row_rem.
     * PR-110 pinned the SECOND HALF
     * of this expectation down rather than changing it: the old model
     * left the time accumulator untouched for walking states ("time is
     * never consumed"), and PR-100's reading of the engine's dispatcher
     * 0x543a0 says why - the BY_X branch feeds that one accumulator
     * with the travelled distance INSTEAD of the 100-per-iteration
     * feed. obj_row_tick is therefore called only for time modes, and
     * through these 400 ms of walking row_t and row_rem stay exactly
     * 0 (a port that fed both clocks would read 1400 here, 14
     * iterations). What the BY_X law really claims
     * - and what stays pinned here - is that the ROWS advance on
     * distance only: 10.0 units of distance completes both
     * 4-unit rows with 2.0 carried, ONE row per iteration admitted
     * (the first closes on iteration 4, the second on iteration 8,
     * whose admit passes the end of the list and wraps it back to
     * index 0): x 10. A gate that read the clock instead of the
     * distance would be somewhere else entirely. */
    a.bx = a.x = 0;
    a.b0 = a.b1 = -1;
    for (int i = 0; i < 4; i++)
        obj_advance(&a, 100);
    if (a.x != 10 || a.fi != 0 || a.row_t != 0 || a.row_rem != 0) {
        fprintf(stderr, "motion selftest: FAILED BY_X walk: x %d fi %d"
                        " row_t %u (want 10 px at F/4 px/s, back to"
                        " index 0 after the 4-unit pair, and no wall"
                        " clock at all in a mode-0 state)\n",
                a.x, a.fi, a.row_t);
        goto out;
    }
    /* Four iterations of the row clock = 5 units at 1.25 units per
     * iteration (an F-as-px/s mutation moves 20). Frames advance on
     * UNITS: hand-derived, the fixture's rows are `1,4` and `2,4` -
     * four iterations accumulate exactly 5.0 distance units, so the
     * FIRST row (4 units) closes on the fourth, and the row gate then
     * holds the second row at facc 1.0 < 4: x 105, fi 1. (Under the
     * old model this pin read fi=1 for a different reason - the time
     * accumulator was frozen at 0 and an early break skipped the walk
     * gate; PR-110's re-derivation says the same numbers come from
     * the distance law alone, which is the claim this leg was always
     * meant to test.) Then 4 more iterations: facc passes 4.0 on the
     * eighth, the second row's admit goes past the end of the list
     * and wraps it, arming the list-end bit no line here reads - a
     * state loop that snapped to the anchor lands back on 100
     * instead of 110 */
    memset(&a, 0, sizeof a);
    a.sc = &s1;
    a.bx = 100;
    a.x = 100;
    a.b0 = a.b1 = -1;
    for (int i = 0; i < 4; i++)
        obj_advance(&a, 50);
    if (a.x != 105 || a.fi != 1) {
        fprintf(stderr, "motion selftest: FAILED unit-driven frames: x %d"
                        " fi %d (want 105 px and frame index 1: one 4-"
                        "unit row completed, the next waits at facc 1;"
                        " a walking state's rows are fed by distance"
                        " only)\n", a.x, a.fi);
        goto out;
    }
    for (int i = 0; i < 4; i++)
        obj_advance(&a, 50);
    if (a.x != 110 || a.sti != 0) {
        fprintf(stderr, "motion selftest: FAILED state loop: x %d sti %d"
                        " (want 110: a walker keeps its position)\n",
                a.x, a.sti);
        goto out;
    }
    /* 2. bounds= flip: patrol 10..26 starting at x=20 walking right
     * (dir 0 = the initial estimate). PR-135 RE-DERIVED THE GRID: 50
     * calls x 40 ms are not 50 steps of anything - row_rem accrues
     * 40 per call and completes an iteration only when the 50 add up,
     * so this is floor(40*50/50) = 40 walk iterations of 1.25 units
     * = 50 units of travel, the same total the old 1 px/tick model
     * walked. The stride comes out through the xa carry as 1 or 2 px
     * per iteration (1,1,1,2 on the 1.25 phase), the flip lands on
     * iterations 5 (x 26), 18 (x 10) and 31 (x 26), and it stops at
     * x=14 walking left. The anchor (20) is never re-occupied: a
     * state loop must not snap a walker. */
    memset(&a, 0, sizeof a);
    a.sc = &s1;
    a.bx = 20;
    a.x = 20;
    a.b0 = 10;
    a.b1 = 26;
    for (int i = 0; i < 50; i++)
        obj_advance(&a, 40);
    if (a.x != 14 || a.dir != -1 || a.face != 0) {
        fprintf(stderr, "motion selftest: FAILED bounds flip: x %d dir %d"
                        " face %d (want 14 walking left inside 10..26,"
                        " facing untouched without CH_DIR)\n",
                a.x, a.dir, a.face);
        goto out;
    }
    /* 3. F=0: no movement at all (the walk model has nothing to
     * accumulate; the frame list falls back to time - estimate) */
    b.bx = b.x = 5;
    b.b0 = b.b1 = -1;
    for (int i = 0; i < 100; i++)
        obj_advance(&b, 40);
    if (b.x != 5) {
        fprintf(stderr, "motion selftest: FAILED F=0 walk state moved to"
                        " x %d (want: stays at 5)\n", b.x);
        goto out;
    }
    /* 4. a walker keeps its position ACROSS states: it GENs into a
     * BY_TIME state and AC_BREAK_NEXTs back. PR-135 re-derived the
     * whole trace for the one-record-step shape: the pulse and the
     * list-end bit are tested by the NEXT step's key composition, so
     * every state change costs one step more. Walking 5 units (four
     * iterations at 1.25) closes A_RUN's 4-unit first row on
     * iteration 4, and that admit raises `GEN K_GEN1` - the
     * transition does NOT fire in that step, which is what the first
     * check below reads. Call 5 is the step whose key composition
     * matches, and A_HIT is entered at x=105 - no snap, because the
     * CURRENT state is a walking one - holding its row 1. A_HIT is a
     * time mode and the walk state fed its accumulator nothing, so
     * its row of 100 units = one iteration: call 6 closes it, passes
     * the end of the one-row list and wraps it, and THAT wrap snaps
     * the position - the CURRENT state A_HIT is not BY_X, so its
     * re-entry owns its anchor, x 100; the old code matched
     * AC_BREAK_NEXT inside the wrap before the snap could run, and
     * PR-135's ordering lets the snap run first and hands the bit to
     * the next composition, which is the same rule leg 6's BY_TIME
     * control pins on its own anchor. Call 7 matches
     * `A_RUN(1)=AC_BREAK_NEXT` and returns to A_RUN without a snap
     * there (the TARGET is the walking state), x stays 100. Calls
     * 8-10 walk on: three iterations, x=103, frame index 0 again
     * (facc was reset by the transition and 3.75 units is below 4).
     * Mutations: an always-snap one also snaps call 5 and fails the
     * x 105 entry; a never-snap one fails call 6, which wants A_HIT
     * back on its 100 anchor; a wall-clock-fed walk comes back
     * inside the 4 walking calls. */
    ll_mov_parse(hit, m2);
    memset(&b, 0, sizeof b);
    b.sc = &s2;
    b.bx = 100;
    b.x = 100;
    b.b0 = b.b1 = -1;
    for (int i = 0; i < 4; i++)
        obj_advance(&b, LL_ITER_MS);      /* four walk iterations      */
    if (b.x != 105 || b.sti != 0 || b.fi != 1 ||
        !((b.sig >> LL_SIG_K_GEN1) & 1u)) {
        fprintf(stderr, "motion selftest: FAILED cross-state pulse: x %d"
                        " state %d frame %d sig %llu (want 105 still in"
                        " A_RUN row 2 with K_GEN1 just raised: the admit"
                        " arms the pulse, the NEXT step's key composition"
                        " is the one that matches it)\n",
                b.x, b.sti, b.fi, (unsigned long long)b.sig);
        goto out;
    }
    obj_advance(&b, LL_ITER_MS);          /* call 5: the step that matches */
    if (b.x != 105 || b.sti != 1 || b.fi != 0) {
        fprintf(stderr, "motion selftest: FAILED cross-state entry: x %d"
                        " state %d frame %d (want 105 in A_HIT row 1: the"
                        " pulse's own transition, no position snap while"
                        " the CURRENT state is a walking one)\n",
                b.x, b.sti, b.fi);
        goto out;
    }
    obj_advance(&b, LL_ITER_MS);          /* call 6: A_HIT's row = one feed */
    if (b.x != 100 || b.sti != 1 || b.fi != 0 || b.brk_arm != 1u) {
        fprintf(stderr, "motion selftest: FAILED cross-state wrap: x %d"
                        " state %d frame %d brk %u (want 100: A_HIT's"
                        " 100-unit row is exactly one iteration, its admit"
                        " wraps the one-row list, the wrap snaps the"
                        " non-walking state back to its 100 anchor, and"
                        " the bit is left for the next composition)\n",
                b.x, b.sti, b.fi, b.brk_arm);
        goto out;
    }
    obj_advance(&b, LL_ITER_MS);          /* call 7: AC_BREAK_NEXT, no snap */
    if (b.x != 100 || b.sti != 0 || b.fi != 0 || b.brk_arm != 0u) {
        fprintf(stderr, "motion selftest: FAILED cross-state return: x %d"
                        " state %d frame %d brk %u (want 100 back in"
                        " A_RUN row 1: the target is a walking state, so"
                        " the transition keeps the instance position as"
                        " the wrap left it, and the bit it matched is"
                        " consumed here)\n",
                b.x, b.sti, b.fi, b.brk_arm);
        goto out;
    }
    for (int i = 0; i < 3; i++)
        obj_advance(&b, LL_ITER_MS);      /* calls 8-10: walk on at 1.25  */
    if (b.x != 103 || b.sti != 0 || b.fi != 0) {
        fprintf(stderr, "motion selftest: FAILED cross-state carry: x %d"
                        " state %d frame %d (want 103 back in A_RUN: a"
                        " transition keeps a walker's position and resets"
                        " its frame accumulator)\n",
                b.x, b.sti, b.fi);
        goto out;
    }
    /* 5. PR-81 facing: the frame-number sign is the authored BASE facing
     * (ZARIA.01's worm art faces left - the eye sits 11-20% from the
     * left edge of all 9 poses - and its walk list is authored all
     * NEGATIVE, so the game draws it head-RIGHT), and control=CH_DIR is
     * what turns that facing when the walk turns at bounds=: the census
     * pairs bounds= with CH_DIR (8/8 sampled placements, 0/14 without),
     * and matching the art in both polarities on the video the winning
     * polarity flips with the travel direction (head-right at
     * t=204.4/211.2/212.4 while it moves right, head-left at
     * t=206.9/208.0/213.2 while it moves left, both at x~330 one second
     * apart = one instance turned around).
     * PR-135 RE-DERIVED THE GRID: 40 calls x 40 ms = 32 walk
     * iterations of 1.25 units = 40 units of travel (the same total
     * as 40 ticks of 1 px). From x=20 in 10..26 the flips land on
     * iterations 5 (x=26), 18 (x=10) and 31 (x=26), ending at x=24
     * facing left - the same endpoint the old model had.
     * 20 more calls = 16 more iterations = 20 units, and here the
     * new stride shows: the position is paid out 1 or 2 whole px per
     * iteration through the xa carry, so the walk CROSSES the left
     * bound between pixels - x goes 11 to 9, past 10, and the flip
     * (the same `x <= b0` test) fires there - ending at x=14 facing
     * right, where the 1 px/tick walk ended at 16. That difference
     * is granularity, not law: the same 20 units of travel, reflected
     * one pixel inside the bound.
     * Mutations: no facing update on the flip (face stays 1) or one that
     * ignores the CH_DIR gate both fail here. */
    ll_mov_parse(chd, m2);
    if (!(m2->nst == 1 && (m2->st[0].ctrl & LL_CTRL_CH_DIR))) {
        fprintf(stderr, "motion selftest: FAILED control=BY_X|CH_DIR did not"
                        " parse (nst %d ctrl 0x%x)\n", m2->nst,
                        m2->st[0].ctrl);
        goto out;
    }
    memset(&b, 0, sizeof b);
    b.sc = &s2;
    b.face = 1;                          /* what the placement builder sets */
    b.bx = 20;
    b.x = 20;
    b.b0 = 10;
    b.b1 = 26;
    for (int i = 0; i < 40; i++)
        obj_advance(&b, 40);
    if (b.x != 24 || b.face != -1) {
        fprintf(stderr, "motion selftest: FAILED CH_DIR facing: x %d face %d"
                        " (want 24 facing left after the bounds flip)\n",
                b.x, b.face);
        goto out;
    }
    for (int i = 0; i < 20; i++)
        obj_advance(&b, 40);
    if (b.x != 14 || b.face != 1) {
        fprintf(stderr, "motion selftest: FAILED CH_DIR facing back: x %d"
                        " face %d (want 14 facing right: the 1.25-unit"
                        " stride crossed the b0=10 bound at x=9, one px"
                        " inside it)\n", b.x, b.face);
        goto out;
    }
    /* the draw mirror: authored sign preserved while facing right, XORed
     * once turned (obj_mirror_of is what obj_draw passes to the draw) */
    if (obj_mirror_of(&b, 0) != 0 || obj_mirror_of(&b, 1) != 1 ||
        obj_mirror_of(&a, 1) != 1 || obj_mirror_of(&a, 0) != 0) {
        fprintf(stderr, "motion selftest: FAILED draw mirror: facing right"
                        " must keep the authored frame mirror, facing left"
                        " must add one\n");
        goto out;
    }
    b.face = -1;
    if (obj_mirror_of(&b, 1) != 0 || obj_mirror_of(&b, 0) != 1) {
        fprintf(stderr, "motion selftest: FAILED draw mirror: a turned"
                        " CH_DIR walker must draw the other polarity\n");
        goto out;
    }
    /* 6. PR-114: an F=0,0 BY_X block - the shape of 30 of the 105
     * shipped BY_X state blocks. The count is a tool output now:
     * tools/re/mov_byx_gate.py over the 777-file corpus measures 105
     * BY_X blocks in 69 files, 30 of them F=0,0 (290 rows, 21 files),
     * and 29 of those 30 name no motion source of their own, so for
     * them the gate below is the whole law. Leg 3 above enters such a
     * state but only asks that the object not move, and its own
     * comment marks the rest as an estimate - "the frame list falls
     * back to time - estimate". This leg turns that estimate into a
     * pin, on the exact grid, and pairs it with the control that
     * shows WHICH half of the gate decides the state loop's anchor
     * snap. Hand-derived, no game data.
     * obj_advance's walk0 (line 1169) reads `(ctrl & BY_X) && fx != 0`
     * - the second term is the port's own, 0x543a0 keys mode 0 on the
     * MODE alone (PR-100's owner reading). With fx == 0 the state is
     * NOT a walking state, so obj_row_tick feeds 100 units per
     * iteration and the row gate compares row_t (line 1230): rows of
     * 100 units close exactly one per call at dt = LL_ITER_MS - fi 1,
     * 2, then the third call closes index 2 of 3, the loop reaches
     * the end of the list, finds no transition and wraps to 0. The
     * walk branch is never reached, so x/y are untouched and facc/xa
     * stay exactly 0.0 (lines 1179-1181 zero them for a non-walking
     * state; obj_walk would add 0 with fx == 0 anyway).
     * And the anchor is NOT re-occupied at the wrap: line 1212 asks
     * for the MODE BIT, not for a non-zero F=, so an F=0 walker keeps
     * the position it was entered at (x 40, y 30, anchor 5/9). The
     * BY_TIME control after it shows that branch really does snap, so
     * the difference is the mode bit and nothing else.
     * MUT-1 (delete `start->fx != 0` from walk0, i.e. the mode-only
     * reading): this state becomes a walking one - no wall clock,
     * obj_walk adds 0, the rows never advance, so fi stays 0 at the
     * first call. Leg 3's "F=0 does not move" check still passes:
     * that is the gap. MUT-2 (make line 1212's snap also require
     * fx != 0): the F=0 walker snaps to 5/9, caught here, and leg 1's
     * walker-keeps-position check is unaffected. */
    static const char glide[] =
        "A_RUN\n\n[A_RUN]\ncontrol=BY_X\nF=0,0\nT=40,0\n"
        "1,100\n2,100\n3,100\n";
    static const char snaps[] =
        "A_RUN\n\n[A_RUN]\ncontrol=BY_TIME\nF=0,0\n1,100\n";
    ll_mov_parse(glide, m2);
    if (!(m2->nst == 1 && (m2->st[0].ctrl & LL_CTRL_BY_X) &&
          m2->st[0].fx == 0 && m2->st[0].nfr == 3 &&
          m2->st[0].fr[0].thresh == 100u)) {
        fprintf(stderr, "motion selftest: FAILED the BY_X F=0,0 fixture did"
                        " not parse as authored (nst %d ctrl 0x%x fx %d"
                        " nfr %d t0 %u)\n", m2->nst, m2->st[0].ctrl,
                m2->st[0].fx, m2->st[0].nfr, m2->st[0].fr[0].thresh);
        goto out;
    }
    memset(&b, 0, sizeof b);
    b.sc = &s2;
    b.bx = 5;                            /* anchor deliberately far from */
    b.by = 9;                            /* the entry position: a snap   */
    b.x = 40;                            /* would be impossible to miss  */
    b.y = 30;
    b.b0 = b.b1 = -1;
    for (int i = 1; i <= 3; i++) {
        obj_advance(&b, LL_ITER_MS);
        if (b.fi != i % 3 || b.sti != 0 || b.row_t != 0 || b.row_rem != 0 ||
            b.facc != 0.0 || b.xa != 0.0 || b.x != 40 || b.y != 30) {
            fprintf(stderr, "motion selftest: FAILED F=0,0 BY_X rows: call"
                            " %d fi %d row_t %u facc %g xa %g at (%d,%d)"
                            " (want one 100-unit row per iteration, no"
                            " distance feed at all, and no anchor snap for"
                            " an F=0 walker)\n", i, b.fi, b.row_t, b.facc,
                    b.xa, b.x, b.y);
            goto out;
        }
    }
    ll_mov_parse(snaps, m1);                       /* the BY_TIME control  */
    if (!(m1->nst == 1 && !(m1->st[0].ctrl & LL_CTRL_BY_X))) {
        fprintf(stderr, "motion selftest: FAILED the BY_TIME snap control"
                        " did not parse (nst %d ctrl 0x%x)\n", m1->nst,
                m1->st[0].ctrl);
        goto out;
    }
    memset(&a, 0, sizeof a);
    a.sc = &s1;
    a.bx = 5;
    a.by = 9;
    a.x = 40;
    a.y = 30;
    a.b0 = a.b1 = -1;
    obj_advance(&a, LL_ITER_MS);
    if (a.fi != 0 || a.x != 5 || a.y != 9) {
        fprintf(stderr, "motion selftest: FAILED BY_TIME wrap snap: fi %d at"
                        " (%d,%d) (want the one-row list wrapped and the"
                        " object back on its 5,9 anchor: line 1212 keys on"
                        " the mode bit, which is why the F=0 walker above is"
                        " NOT snapped)\n", a.fi, a.x, a.y);
        goto out;
    }
    /* 7. PR-115 (PR-110 open items 1 and 8), RE-DERIVED BY PR-135 for
     * the engine's own step: how many ROWS ONE step may close. Exactly
     * one. 0x52329 `39 d8` / 0x5232b `7d 27` (jge -> the epilogue)
     * admits at most one row per record step and 0x52334 `29 c3` hands
     * `acc - thr` to the row handler in registers only, so the
     * accumulator at [obj+0xa1] is monotonic (the only writes to it
     * are the feeds at 0x543d1 / 0x543e7 / 0x54412 / 0x5443b) and the
     * remainder of one step is the head start of the next. The port's
     * object loop used to admit as many rows as ONE call's feed
     * covered, and its player twin capped that at `4 * nfr` and
     * resynced the rest to 0; PR-135 replaced both with the record
     * step - one iteration, one feed of 100, at most one row, carry
     * kept in row_rem - so this leg now pins THAT, and the number of
     * record steps a call runs is made visible by the authored M
     * deltas. The parser's clamp of an authored `1,0` to 100
     * (ll_play.c, "the M4-C rule") stays and this fixture's first row
     * IS that authored shape: the engine's row-node reader
     * (0x51667/0x51679, `%hi` into [node+0x10], scaled only by the
     * block's `time_scale` at 0x51680-0x5168f) really does store a 0
     * there, so PR-110 item 1's "170 of 18617 authored rows have
     * threshold 0 (73 files)" measures the clamp difference, not a
     * rows-per-step difference (the rows-per-step set, per the tool's
     * PR-110 items 1 and 8 block, is 108 time-fed states in 77 files
     * with a stored minimum threshold <= 50). Hand-derived, no game
     * data.
     * OBJM (100/30/30 - the clamp applied) at dt = 116 = 2 x 50 + 16:
     * TWO record steps, so exactly TWO admits - index 0 exactly, then
     * index 1 leaving 70 owed - with the 16 ms carried; no wrap, so no
     * snap and the object stands at 40,30 (the old shape closed SIX
     * rows and wrapped twice in that one call, ending snapped on its
     * 5,9 anchor: the two shapes are furthest apart right here).
     * Call 2, one iteration: 70 + 100 covers the last row, its admit
     * WRAPS the list, snaps the BY_TIME anchor, and arms the list-end
     * bit; 140 and the 16 stay as the debt. Call 3: the head key
     * composition consumes the bit (no line reads it) and the step
     * pays the debt forward - 140 + 100 closes index 0 and owes 140
     * again. A feed of 100 can never land the accumulator on 0 once a
     * threshold is not a divisor of 100.
     * ONE is the CAP made visible: BY_X so no wrap can snap it away,
     * F=0 so the mode branch feeds the wall clock (leg 6's finding),
     * and its single row carries `M 10,0`, so every admit moves x by
     * a readable 10. dt = 500 runs TEN record steps in one call: ten
     * one-row admits, ten wraps, x 40 -> 140, and nothing capped at
     * 4 * nfr or resynced the way ll_play.c used to. The M delta is
     * what makes the count readable: rows whose thresholds divide the
     * feed drain to 0 either way, and a list without position deltas
     * hides wraps under the anchor snap - which is why the previous
     * version of this half used a 500 ms / 17-row count that the
     * snap made invisible.
     * MUTATIONS, RUN IN PR-135 (the harness is
     * /tmp/opencode/pr135_mut2.py; each mutant's full selftest output is
     * in /tmp/opencode/pr135/mut2/). Three of these four are caught
     * HERE; the fourth is caught earlier, because obj_state_selftest runs
     * before this selftest and the chain stops at the first failure.
     * Admitting TWO rows per record step fails call 1: the three-row
     * list wraps inside two steps - fi 0, row_t 40, brk 1, x/y 5,9
     * against the 40,30 that two admits and no wrap should leave -
     * M5_obj_two_admits.mov.log. Zeroing the accumulator instead of
     * carrying it fails that same call 1, "fi 2 row_t 0" against the
     * 70-unit remainder it owes - M6_obj_zero_carry.mov.log. The old
     * `4 * nfr` cap, copied into this loop, stops ONE at four admits,
     * x 80 against 140 - M8_obj_4nfr_cap.mov.log. Feeding the row clock
     * per millisecond instead of per iteration never gets here: the
     * object-state leg fails first, "the first iteration did not close
     * exactly one row (state 0 frame 0 t 50)" -
     * M7_obj_per_ms_feed.mov.log. */
    {
        static const char objm[] =
            "A_RUN\n\n[A_RUN]\ncontrol=BY_TIME\n1,0\n2,30\n3,30\n";
        static const char one[] =
            "A_RUN\n\n[A_RUN]\ncontrol=BY_X\nF=0,0\nT=40,0\n"
            "1,100, M 10,0\n";
        ll_mov_t *mo = calloc(1, sizeof *mo), *on = calloc(1, sizeof *on);
        obj_script so;
        lvl_obj c;
        memset(&so, 0, sizeof so);
        so.mov = mo;
        if (!mo || !on || ll_mov_parse(objm, mo) != 1 || mo->st[0].nfr != 3 ||
            mo->st[0].fr[0].thresh != 100u || mo->st[0].fr[1].thresh != 30u ||
            mo->st[0].fr[2].thresh != 30u) {
            fprintf(stderr, "motion selftest: FAILED the multi-row fixture"
                            " did not parse as authored (an authored 0 must"
                            " land on 100 here, the M4-C rule)\n");
            free(mo);
            free(on);
            goto out;
        }
        memset(&c, 0, sizeof c);
        c.sc = &so;
        c.bx = 5;
        c.by = 9;
        c.x = 40;
        c.y = 30;
        c.b0 = c.b1 = -1;
        obj_advance(&c, 116);
        if (c.fi != 2 || c.sti != 0 || c.row_t != 70u || c.row_rem != 16u ||
            c.brk_arm != 0u || c.facc != 0.0 || c.xa != 0.0 || c.x != 40 ||
            c.y != 30) {
            fprintf(stderr, "motion selftest: FAILED rows per step: call 1"
                            " fi %d row_t %u rem %u brk %u at (%d,%d)"
                            " (want TWO record steps and TWO admits only:"
                            " 116 = 2 x 50 + 16 keeps the 16; the first"
                            " feed of 100 closes index 0 exactly, the"
                            " second closes index 1 and owes 70; no wrap"
                            " so no snap - still at 40,30)\n", c.fi,
                    c.row_t, c.row_rem, c.brk_arm, c.x, c.y);
            free(mo);
            free(on);
            goto out;
        }
        obj_advance(&c, LL_ITER_MS);
        if (c.fi != 0 || c.row_t != 140u || c.row_rem != 16u ||
            c.brk_arm != 1u || c.x != 5 || c.y != 9) {
            fprintf(stderr, "motion selftest: FAILED rows per step: call 2"
                            " fi %d row_t %u rem %u brk %u at (%d,%d)"
                            " (want 70 + 100 covering the last 30-unit row,"
                            " the admit WRAPPING the list, snapping the"
                            " BY_TIME anchor to 5,9 and arming the bit,"
                            " and 140 + the 16 ms carry kept as the debt"
                            " the next step inherits)\n", c.fi, c.row_t,
                    c.row_rem, c.brk_arm, c.x, c.y);
            free(mo);
            free(on);
            goto out;
        }
        obj_advance(&c, LL_ITER_MS);
        if (c.fi != 1 || c.row_t != 140u || c.row_rem != 16u ||
            c.brk_arm != 0u) {
            fprintf(stderr, "motion selftest: FAILED rows per step: call 3"
                            " fi %d row_t %u rem %u brk %u (the head key"
                            " composition consumes the bit - no line here"
                            " reads it - and the step pays the debt"
                            " forward: 140 + 100 closes index 0 and owes"
                            " 140 again)\n", c.fi, c.row_t, c.row_rem,
                    c.brk_arm);
            free(mo);
            free(on);
            goto out;
        }
        memset(&so, 0, sizeof so);
        so.mov = on;
        if (ll_mov_parse(one, on) != 1 || on->st[0].nfr != 1 ||
            on->st[0].fr[0].dx != 10) {
            fprintf(stderr, "motion selftest: FAILED the one-row fixture"
                            " did not parse\n");
            free(mo);
            free(on);
            goto out;
        }
        memset(&c, 0, sizeof c);
        c.sc = &so;
        c.bx = 5;
        c.by = 9;
        c.x = 40;
        c.y = 30;
        c.b0 = c.b1 = -1;
        obj_advance(&c, 500);              /* TEN record steps in one call */
        if (c.fi != 0 || c.row_t != 0u || c.row_rem != 0u || c.x != 140 ||
            c.y != 30 || c.brk_arm != 1u) {
            fprintf(stderr, "motion selftest: FAILED no cap here: row_t %u"
                            " rem %u fi %d brk %u at (%d,%d) (want all TEN"
                            " record steps of a 500 ms call run, each with"
                            " its one row and its authored M delta: +10 ten"
                            " times, x 40 -> 140, the one-row list wrapped"
                            " ten times, nothing capped at 4 * nfr and"
                            " nothing resynced)\n", c.row_t, c.row_rem,
                    c.fi, c.brk_arm, c.x, c.y);
            free(mo);
            free(on);
            goto out;
        }
        free(mo);
        free(on);
    }
    /* 8. PR-117 (PR-116 open item 2): a DISTANCE-fed accumulator sitting
     * EXACTLY on a threshold. Leg 7 above closes rows on a TIME feed,
     * and PR-116's mutation run (/tmp/opencode/mut/run116.sh) found
     * that flipping ONLY the walking half of the object's row gate -
     * `walk ? o->facc < (double)f->thresh` at :1230, to `<=` - broke
     * nothing anywhere: no leg of this repo put a distance-fed
     * accumulator exactly on a threshold, so the rule at equality was
     * unpinned on that half. The accumulator is `facc`, fed by obj_walk
     * per ITERATION; this fixture names no T= word, so the law has no
     * steady state to solve and PR-98's estimate stands in its
     * per-iteration form - |F| * LL_ITER_MS / 4000 = 1.25 units per
     * iteration (the open item HANDOFF 7 carries; the engine's
     * mode-0 feed of |obj+0xca| into [obj+0xa1], 0x543d1, is still
     * the model). Hand-derived, no game data: one row of 100 units is
     * the whole threshold and dt = 2000 ms completes exactly 40 of
     * those iterations, half of it.
     *   call 1, dt = 2000 = 40 iterations of 1.25: facc 50.0 < 100,
     *     so NO row closes: fi 0, and the walk moved the object 50
     *     px, x 20 -> 70 (the xa carry pays 1 or 2 whole px per
     *     iteration; 40 * 1.25 is 50.0 exactly, so xa ends at 0.0).
     *   call 2, 40 more iterations: the 40th feed lands facc EXACTLY
     *     on 100.0, the threshold. That is the boundary. The port's
     *     rule (the >= side of the same compare, PR-110) closes the
     *     row: facc 100.0 - 100.0 = 0.0, the admit runs past the end
     *     of the one-row list, wraps fi back to 0 and arms the
     *     list-end bit - without snapping the anchor, because the
     *     wrap branch skips the snap for a walking state - x 70 + 50
     *     = 120. 1.25 is exact in binary64 (5/4) and 40 of them sum
     *     exactly, so this is a real equality and not a near miss.
     * The engine at that instant does NOT fire: 0x52329 `39 d8` /
     * 0x5232b `7d 27` (PR-116) returns through 0x52354 with the
     * accumulator kept, and the row runs on the NEXT feed with a
     * remainder of a whole feed. That half stays the owner's decision
     * (PR-116 item 1); this leg's job is to pin which side the port is
     * on, and to make the equality a tested case instead of a gap. The
     * fixture also pins the "one accumulator, one source" rule at
     * a walking state is never fed the 100 at all, so row_t and
     * row_rem are still 0 after 4000 ms of wall time.
     * MUTATION (PR-116's survivor, RE-RUN IN PR-135): flipping that half
     * to `<=` (/tmp/opencode/mut/prog/g2b.py) breaks exactly the rule
     * this leg exists for - at call 2 the compare reads 100.0 <= 100.0,
     * the loop stops instead of closing the row, facc is left at 100.0
     * and no row ever closes, while x still reaches 120 because obj_walk
     * moves the object independently of this gate. PR-135's run adds the
     * honest detail: the flip is now caught BEFORE this leg -
     * obj_state_selftest fails its first iteration, "the first iteration
     * did not close exactly one row (state 0 frame 0 t 100)"
     * (/tmp/opencode/pr135/mut2/M9_obj_gate_equality.mov.log) - and the
     * motion selftest is never reached, so this leg's own call-2 break
     * is DERIVED from the fixture rather than observed in that run. The
     * play selftest stays clean under the same patch (that run's play
     * rc=0), and what that asymmetry rests on is the next paragraph:
     * the player's distance-fed `facc` (ll_play.c:1119) is fed the same
     * way but its gate is ONE compare for both feeds (:1286), which
     * PR-116's G1 already breaks 41 ways - and none of those 41 comes
     * from a distance-fed leg: the only play leg whose distance feed
     * runs at all is the A_PUSH one (ll_play.c, PR-114), against a
     * 1000-unit row, where facc reaches 13.0 and never a threshold,
     * so the player side has no equality case to pin. PR-117 open
     * item 1 records why it stays that way. */
    {
        static const char walkrow[] =
            "A_RUN\n\n[A_RUN]\ncontrol=BY_X\nF=100,0\n1,100\n";
        ll_mov_t *mw = calloc(1, sizeof *mw);
        obj_script sw;
        lvl_obj d;
        memset(&sw, 0, sizeof sw);
        sw.mov = mw;
        if (!mw || ll_mov_parse(walkrow, mw) != 1 ||
            mw->st[0].nfr != 1 || mw->st[0].fx != 100 ||
            mw->st[0].fr[0].thresh != 100u) {
            fprintf(stderr, "motion selftest: FAILED the walking-row"
                            " fixture did not parse as authored (F=100 and"
                            " one 100-unit row)\n");
            free(mw);
            goto out;
        }
        memset(&d, 0, sizeof d);
        d.sc = &sw;
        d.x = 20;
        obj_advance(&d, 2000);
        if (d.fi != 0 || d.facc != 50.0 || d.xa != 0.0 || d.x != 70 ||
            d.row_t != 0u || d.row_rem != 0u) {
            fprintf(stderr, "motion selftest: FAILED half feed: fi %d facc"
                            " %g x %d row_t %u rem %u (want NO row from"
                            " half a row's distance: facc 50.0 < 100, the"
                            " walk moving x 20 -> 70, and no wall clock on"
                            " a walking state at all)\n", d.fi, d.facc,
                    d.x, d.row_t, d.row_rem);
            free(mw);
            goto out;
        }
        obj_advance(&d, 2000);
        if (d.fi != 0 || d.facc != 0.0 || d.xa != 0.0 || d.x != 120 ||
            d.row_t != 0u || d.row_rem != 0u) {
            fprintf(stderr, "motion selftest: FAILED at exact equality:"
                            " fi %d facc %g x %d row_t %u (want facc"
                            " EXACTLY on the 100-unit threshold to close"
                            " that one row and land on 0.0, the list"
                            " wrapping without an anchor snap for a"
                            " walking state, x 70 -> 120)\n", d.fi,
                    d.facc, d.x, d.row_t);
            free(mw);
            goto out;
        }
        free(mw);
    }
    /* 9. PR-119 (PR-118 open item 1), RE-DERIVED BY PR-135: the
     * port's distance accumulator DRIFTS off a landing its own law
     * says is exact, and this leg pins where - and where it no longer
     * does. PR-118's census (section 6 of tools/re/mov_byx_gate.py,
     * output /tmp/opencode/pr118_census.txt) found two corpus states
     * where exact arithmetic and repeated double addition disagreed:
     * both LONA.05, F=30, on the 20 ms grid. A state with no T= word
     * keeps PR-98's estimate under the new law, in per-iteration
     * form, and 30 * LL_ITER_MS / 4000 = 0.375 = exactly 3/8 - that
     * pair no longer drifts at all, and its old half-feed artifact
     * (0.15 per 20 ms call, a sum 20 calls could not land exactly)
     * was a property of a feed proportional to the CALL, not of the
     * accumulator. The drift cases moved to the states the law
     * divides: F/(2T) is rarely a binary fraction. Rebuilt here with
     * no game data: `control=BY_X`, `F=20,0`, `T=30,0` ->
     * 20/(2*30) = 1/3 units per iteration, one row `1,2`, two units.
     *     The law's exact arithmetic closes that row at k = 6
     * iterations; the port adds the rounded double 0.3333333333333333
     * six times and lands on 1.9999999999999998, ONE ULP SHORT, so
     * the gate holds the row back and it closes a full iteration
     * LATE, on the seventh. Meanwhile the position side has already
     * paid: the same feed runs into `xa` too and its whole part is
     * taken away at every pixel boundary, and at the sixth iteration
     * x has moved the full 2 px (20 -> 22, xa exactly 0.0) in the
     * very step whose facc is still short - one feed, two variables,
     * opposite sides of a whole unit. The remainder the late close
     * leaves, 1.9999999999999998 + 0.3333333333333333 - 2 =
     * 0.33333333333333304, is strictly under one feed and is NOT
     * the feed: the drift carries into the next row. Verified in
     * compiled C too (the sums above were reproduced with the exact
     * obj_walk expressions; /tmp/opencode/pr118/acc.c is the
     * original dt-proportional mirror).
     *     Not the engine's bug to fix: its accumulator is an integer
     * dword (0x543d1 `add [obj+0xa1], eax`) fed from [obj+0xca],
     * still unread (PR-118 open item 2), so whether the engine has
     * any drift here is unknown. What is known is that the port is
     * one iteration late against ITS OWN documented law, and nothing
     * tested it.
     *     The contrast half is the old F=30 shape: no T= word,
     * 0.375 per iteration, and a 3-unit row. Eight feeds land
     * EXACTLY on 3.0, the row closes ON TIME, and the subtract
     * leaves exactly 0.0, 3 px paid, xa 0.0. Exact feed -> exact
     * landing; inexact feed -> one iteration late. That pair, on one
     * fixture each, is the whole claim of this leg.
     *     MUTATIONS, AS RE-RUN IN PR-135 (the harness is
     * /tmp/opencode/pr135_mut2.py, and the full selftest output of every
     * mutant is kept in /tmp/opencode/pr135/mut2/): carrying no remainder
     * at the subtract (`o->facc = 0.0`, run119.sh's prog/norem.py again)
     * IS caught - but the run stops at its first failing leg, the
     * object-state one, whose call 1 then reads "fi 2 row_t 0" against
     * the 70-unit remainder it expects (M6_obj_zero_carry.mov.log). This
     * leg's own second check, the one that requires 0.33333333333333304,
     * is downstream of that and was not reached in the run. PR-119's
     * `exactf.py` control (an exact integer accumulator in place of this
     * double) is NOT a one-line patch, so PR-135 did not re-run it: it
     * stays PR-119's. g2b - adding equality to the walk/row gate, the
     * PR-116/117 flip - is caught at that same first leg: "the first
     * iteration did not close exactly one row (state 0 frame 0 t 100)"
     * (M9_obj_gate_equality.mov.log). This leg and leg 8 are what that
     * flip breaks NEXT, both by construction - each puts an accumulator
     * exactly on a threshold - but the motion selftest never reaches them
     * once the object-state leg has failed, so the OBSERVED g2b failure
     * is that earlier one. What stays unique to this leg is its drift
     * half: the g2b flip leaves those checks clean, because the
     * accumulator is UNDER the threshold at the sixth feed either way. */
    {
        static const char driftrow[] =
            "A_RUN\n\n[A_RUN]\ncontrol=BY_X\nF=20,0\nT=30,0\n1,2\n";
        static const char exactrow[] =
            "A_RUN\n\n[A_RUN]\ncontrol=BY_X\nF=30,0\n1,3\n";
        ll_mov_t *md = calloc(1, sizeof *md), *mx = calloc(1, sizeof *mx);
        obj_script sd, sx;
        lvl_obj e;
        memset(&sd, 0, sizeof sd);
        memset(&sx, 0, sizeof sx);
        sd.mov = md;
        sx.mov = mx;
        if (!md || !mx || ll_mov_parse(driftrow, md) != 1 ||
            md->st[0].nfr != 1 || md->st[0].fx != 20 ||
            md->st[0].tx != 30 || md->st[0].fr[0].thresh != 2u) {
            fprintf(stderr, "motion selftest: FAILED the drift fixture did"
                            " not parse as authored (F=20,0 T=30,0 and a"
                            " 2-unit row)\n");
            free(md);
            free(mx);
            goto out;
        }
        memset(&e, 0, sizeof e);
        e.sc = &sd;
        e.x = 20;
        obj_advance(&e, 300);               /* exactly six iterations     */
        if (e.fi != 0 || !(e.facc > 1.99 && e.facc < 2.0) || e.x != 22 ||
            e.xa != 0.0 || e.row_t != 0u || e.row_rem != 0u) {
            fprintf(stderr, "motion selftest: FAILED at six iterations of"
                            " F=20,T=30: fi %d facc %.17g x %d xa %.17g"
                            " row_t %u (want NO row yet: the law says 2"
                            " units at k = 6, six feeds of the rounded"
                            " double land on 1.9999999999999998, one ulp"
                            " short, while the pixel path has already"
                            " paid its 2 px and xa is exactly 0.0)\n",
                    e.fi, e.facc, e.x, e.xa, e.row_t);
            free(md);
            free(mx);
            goto out;
        }
        obj_advance(&e, LL_ITER_MS);        /* the 7th: the row closes LATE */
        if (e.fi != 0 || !(e.facc > 0.33 && e.facc < 0.3333333333333333) ||
            e.x != 22 || e.row_t != 0u || e.brk_arm != 1u) {
            fprintf(stderr, "motion selftest: FAILED on the 7th iteration:"
                            " fi %d facc %.17g x %d row_t %u brk %u (want"
                            " that row to close LATE, one iteration behind"
                            " the law, carrying its drift into the next"
                            " row as a remainder of 0.33333333333333304 -"
                            " strictly under one feed, never the feed and"
                            " never 0 - and the admit wrapping the one-row"
                            " list and arming the bit)\n",
                    e.fi, e.facc, e.x, e.row_t, e.brk_arm);
            free(md);
            free(mx);
            goto out;
        }
        free(md);
        if (ll_mov_parse(exactrow, mx) != 1 || mx->st[0].nfr != 1 ||
            mx->st[0].fx != 30 || mx->st[0].tx != 0 ||
            mx->st[0].fr[0].thresh != 3u) {
            fprintf(stderr, "motion selftest: FAILED the exact-feed"
                            " contrast fixture did not parse as authored"
                            " (F=30,0, no T=, a 3-unit row)\n");
            free(mx);
            goto out;
        }
        memset(&e, 0, sizeof e);
        e.sc = &sx;
        e.x = 20;
        obj_advance(&e, 400);               /* exactly eight iterations   */
        if (e.fi != 0 || e.facc != 0.0 || e.x != 23 || e.xa != 0.0 ||
            e.row_t != 0u || e.row_rem != 0u) {
            fprintf(stderr, "motion selftest: FAILED the exact-feed"
                            " contrast: fi %d facc %.17g x %d xa %.17g"
                            " (want the 3/8 feed to land EXACTLY where"
                            " the law says: eight feeds = 3.0, row on"
                            " time, subtract leaves exactly 0.0, 3 px"
                            " paid, xa 0.0)\n", e.fi, e.facc, e.x, e.xa);
            free(mx);
            goto out;
        }
        free(mx);
    }

    rc = 0;
out:
    free(m1);
    free(m2);
    return rc;
}

static int obj_layer_selftest(void);
static int plany_selftest(void);
static int contact_model_selftest(void);
static int ladder_answer_selftest(void);
static int place_points_selftest(void);  /* L9 (PR-243): defined with the
                                          readers it belongs to */
static int place_seat_selftest(void);    /* L13 (PR-335): the entry's own
                                            surface table, 0x45120 */
static int seat_motion_selftest(void);   /* L14 (PR-342): the +0xa4 verb's
                                            move of that table at the row
                                            admit - 0x1e5c0's row-cursor
                                            sign and 0x1e690's five jumps */
static int surface_ask_selftest(void);   /* L15 (PR-352): the engine's OWN
                                            standing query - 0x45380's
                                            bracket, 0x455a0's slope and
                                            0x45400's two gates - run over
                                            the ZARIA.01 seat tables under
                                            the motion L14 pins */
static int seq_landshift_selftest(void); /* L16 (PR-359): the element's own
                                            land and leave methods, 0x42980
                                            and 0x429d0, slots +0x04/+0x08
                                            of the block 0xa0c08 every .SEQ
                                            element record carries at
                                            +0x33 (PR-357) - and what the
                                            step does to the ask point L15
                                            asks with */
static int seat_sample_selftest(void);   /* L17 (PR-363): the two writers of
                                            [obj+0x89], the pair the class
                                            verb +0x6c hands to the standing
                                            query as its first sample point
                                            (PR-349) - the class verb +0x54
                                            in 31 tables (0x53df0, no gate)
                                            and in 124 (0x55070, gated by
                                            0x54200-0x5421f) */

/* FIX-13 (X-11) selftest, test-only entry point (ll_level.h): proves the
 * two .MOV duration invariants without any window or game data:
 *   1. ll_mov_parse clamps non-positive durations (the M4-C rule,
 *      pinned);
 *   2. obj_advance terminates on an all-zero-ms cycle (the guard). The
 *      proof is that this RETURNS - the CI legs run it under a timeout,
 *      so a regression HANGS (caught) instead of passing silently.
 * The chain this entry point also runs (PR-49..79) covers the frame
 * grammar, anchored draw, shape geometry, push-out, object layers, the
 * object state machine and the walking model - PR-79's law re-derived
 * by PR-133/134/135 as F/(2T) units per engine iteration of
 * LL_ITER_MS (obj_motion_selftest: rate, unit-driven frame advance,
 * bounds= flip, one record step per iteration with the list-end bit
 * tested one step later, anchor snap only for non-walking states and
 * only at a wrap). */
int ll_mov_selftest(void)
{
    ll_mov_t *m = calloc(1, sizeof *m);
    if (!m) {
        fprintf(stderr, "mov selftest: FAILED out of memory\n");
        return 1;
    }
    ll_mov_parse("[A]\n0,0\n1,0\n2,-5\n[B]\n0,5\n", m);
    if (m->nst != 2 || m->st[0].nfr != 3 || m->st[1].nfr != 1) {
        fprintf(stderr, "mov selftest: FAILED parse shape "
                        "(nst=%d nfr=%d/%d)\n",
                m->nst, m->st[0].nfr, m->st[1].nfr);
        free(m);
        return 1;
    }
    /* PR-51: the declared state list picks the start state (box40.mov
     * shape: header A_RUN, A_DEAD but the [A_DEAD] block written first) */
    ll_mov_parse("A_RUN\nA_DEAD\n\n[A_DEAD]\n0,100\n\n[A_RUN]\n1,50\n", m);
    if (m->nst != 2 || m->start_st != 1 ||
        strcmp(m->first_decl, "A_RUN") != 0) {
        fprintf(stderr, "mov selftest: FAILED declared start state"
                        " (start_st=%d decl='%s')\n", m->start_st,
                m->first_decl);
        free(m);
        return 1;
    }
    ll_mov_parse("[A]\n0,0\n1,0\n2,-5\n[B]\n0,5\n", m);
    if (m->start_st != 0 || m->first_decl[0]) {
        fprintf(stderr, "mov selftest: FAILED no declaration must mean"
                        " state 0\n");
        free(m);
        return 1;
    }
    for (int k = 0; k < 3; k++)
        if (m->st[0].fr[k].thresh != 100u) {
            fprintf(stderr, "mov selftest: FAILED zero/negative duration "
                            "not clamped (frame %d thresh=%u)\n",
                    k, m->st[0].fr[k].thresh);
            free(m);
            return 1;
        }
    if (m->st[1].fr[0].thresh != 5u) {
        fprintf(stderr, "mov selftest: FAILED positive duration changed "
                        "(thresh=%u)\n", m->st[1].fr[0].thresh);
        free(m);
        return 1;
    }
    /* The corrupt shape X-11 describes, BUILT DIRECTLY to bypass the
     * parser clamp (a future patch path or caller must not be able to
     * hang the scene loop either): */
    obj_script sc;
    memset(&sc, 0, sizeof sc);
    for (int k = 0; k < LMOV_MAXST; k++)
        sc.ast[k] = -1;
    sc.mov = calloc(1, sizeof *sc.mov);
    if (!sc.mov) {
        fprintf(stderr, "mov selftest: FAILED out of memory\n");
        free(m);
        return 1;
    }
    sc.mov->nst = 1;
    sc.mov->st[0].nfr = LMOV_MAXFR;
    for (int k = 0; k < LMOV_MAXFR; k++)
        sc.mov->st[0].fr[k].thresh = 0;
    lvl_obj o;
    memset(&o, 0, sizeof o);
    o.sc = &sc;
    obj_advance(&o, 60000u);       /* without the guard: spins forever */
    if (o.sti != 0) {              /* single state: must not wander     */
        fprintf(stderr, "mov selftest: FAILED guard left state %d\n",
                o.sti);
        free(sc.mov);
        free(m);
        return 1;
    }

    /* PR-45: the art an object draws is exactly the table its CURRENT state
     * names through seq=. Built directly here (not through the parser):
     * state 1 must get slot 1; a state that names no art, or names one this
     * install does not carry (a reported data hole), must draw NOTHING -
     * never another state's table. And an art table that IS there but whose
     * selected element carries only the SHAPE margins, no patch art (an
     * invisible solid, PLAN 8h), must be reported as that table and still
     * never drawn. */
    sc.mov->nst = 2;
    sc.nart = 2;
    sc.ast[0] = 0;
    sc.ast[1] = -1;                       /* state names no seq= art */
    sc.art[0].found = sc.art[1].found = 1;
    sc.art[0].eidx0 = sc.art[1].eidx0 = 0;
    sc.art[1].sw = sc.art[1].sh = 8;
    o.sti = 1;
    if (obj_art_of(&o) != NULL) {
        fprintf(stderr, "mov selftest: FAILED a state without seq= drew"
                        " another state's art (%p)\n", (void *)obj_art_of(&o));
        free(sc.mov);
        free(m);
        return 1;
    }
    sc.ast[1] = 1;
    if (obj_art_of(&o) != &sc.art[1]) {
        fprintf(stderr, "mov selftest: FAILED a state did not select its own"
                        " seq= art\n");
        free(sc.mov);
        free(m);
        return 1;
    }
    if (!obj_visible(&o, 640, 400, 0, 0)) {
        fprintf(stderr, "mov selftest: FAILED a selected patch-art table was"
                        " not drawable\n");
        free(sc.mov);
        free(m);
        return 1;
    }
    sc.art[1].found = 0;                  /* authored name, no record here */
    if (obj_art_of(&o) != NULL) {
        fprintf(stderr, "mov selftest: FAILED a data-hole art name was"
                        " drawn anyway\n");
        free(sc.mov);
        free(m);
        return 1;
    }
    sc.art[1].found = 1;                  /* record is there ...            */
    sc.art[1].eidx0 = -1;                 /* ... but shape-only, no patch   */
    if (obj_art_of(&o) != &sc.art[1]) {
        fprintf(stderr, "mov selftest: FAILED a shape-only art table was"
                        " not reported as the state's own\n");
        free(sc.mov);
        free(m);
        return 1;
    }
    if (obj_visible(&o, 640, 400, 0, 0)) {
        fprintf(stderr, "mov selftest: FAILED a shape-only art table was"
                        " drawn anyway\n");
        free(sc.mov);
        free(m);
        return 1;
    }

    /* PR-46: the element SHAPE geometry leg (no window, no game data). */
    if (seq_codec_selftest() != 0 || seq_anchor_selftest() != 0 ||
        seq_shape_selftest() != 0 || seq_push_selftest() != 0 ||
        obj_layer_selftest() != 0 || obj_state_selftest() != 0 ||
        obj_actime_selftest() != 0 ||
        obj_rowzero_selftest() != 0 || obj_rowseek_selftest() != 0 ||
        obj_rowgate_selftest() != 0 ||
        obj_rowgate_act_selftest() != 0 ||
        obj_motion_selftest() != 0 || contact_model_selftest() != 0 ||
        plany_selftest() != 0 ||
        ladder_answer_selftest() != 0 || place_grammar_selftest() != 0 ||
        place_rectclass_selftest() != 0 ||
        rect_class_ask_selftest() != 0 ||
        seat_motion_selftest() != 0 || surface_ask_selftest() != 0 ||
        seq_landshift_selftest() != 0 || seat_sample_selftest() != 0) {
        free(sc.mov);
        free(m);
        return 1;
    }

    free(sc.mov);
    free(m);
    printf("mov selftest: duration clamp + cycle guard + per-state seq= art"
           " selection + patch codecs + anchored draw + shape geometry + push-out + object layers + object state machine + object walking motion"
           " + PR-92 AC_ENEMY touch raise (touch arms A_ATAK, A_ATAK meets"
           " A_DEAD, hit_cd/dead gate it, bus empty) + PR-93 placement"
           " grammars (0,0 child joined not instantiated, static .SEQ lines"
           " placed at their authored x,y, x-outside-world and non-.SEQ"
           " 3-comma lines stay unplaced) + PR-95 attached static image"
           " (a .SEQ line inside a bounded actor's property block is drawn"
           " at parent + offset, not the world corner; one after a"
           " non-bounded entry keeps its absolute x,y) + PR-328 the"
           " rectangle-class placement line `TOKEN,x0,y0;x1,y1` of the"
           " eight class tables whose +0x18 verb is the one record verb"
           " 0x40c90 (the token names the class and the four numbers ARE"
           " its box at [obj+0x3c]: absolute, because that verb adds no"
           " placement the way the ladder's 0x3f0f0 does, and unsorted as"
           " authored; one class record per token, one instance per line;"
           " ZONE, KRY and a token whose numbers are not a box place"
           " nothing) + PR-329 the rectangle-class answer of the class"
           " tables' +0x78 entry, run by the requester's ask walk"
           " (DRECT = askable + falling + point-in-box raises AC_DRECT,"
           " DRECT2 drops only the fall, RECT drops the probe too,"
           " KILOF_PL adds facing > 0, BICZRECT facing == -1 and the"
           " anchor point, BRECT the AC_RECT bit; the channel bit of the"
           " requester's state and the plane gate 0x4b5a1 decide who is"
           " asked at all)"
           " + PR-107 authored"
           " start row ((3) enters a 3-row state at its THIRD row, (1),"
           " (9) and (x) at the top, and that row list's wrap still"
           " returns through A_RUN(1)=AC_BREAK_NEXT) + PR-110 the object's"
           " row clock is the ITERATION clock (a 100-unit row is one feed"
           " of LL_ITER_MS, A_ATAK's 200-unit rows two each, and a"
           " walking state's rows are fed by its distance only, never by"
           " wall time) + PR-114 a BY_X state whose F= is 0,0 is NOT a"
           " walking one (its rows still close one per iteration, the"
           " distance feed does not run at all so facc/xa stay 0 and the"
           " object does not move, and its state-loop wrap does NOT snap"
           " the anchor - line 1212 asks for the mode bit, not for a"
           " non-zero F=; the BY_TIME control does snap) + PR-135 the"
           " object loop now runs the engine's OWN record step: one key"
           " composition, one feed of 100, at most ONE row per"
           " iteration of LL_ITER_MS - a 116 ms call is TWO steps and"
           " closes exactly TWO rows of a 100/30/30 list, the wrap of"
           " the third comes one step later because the list-end bit"
           " is tested by the NEXT step's key composition, the 140-"
           "unit debt and the 16 ms carry are kept as its inheritance,"
           " and a 500 ms call on a one-row list runs TEN record"
           " steps whose ten admits read as ten authored M deltas -"
           " no cap, no resync, no multi-row feed; the walk rate is"
           " the measured law F/(2T) px per iteration, with PR-98's"
           " estimate standing only where no T= is authored, and a"
           " BY_TIME state that wraps its list re-occupies its anchor"
           " one step BEFORE the exit matches it) + PR-117 a"
           " distance-fed accumulator landing EXACTLY on a row's"
           " threshold closes it (two dt=2000 calls at F=100 put facc"
           " on 100.0 against a 100-unit row: the row fires, facc"
           " lands on 0.0, the list wraps without an anchor snap"
           " because the state is a walking one, and the wall clock"
           " never ran; the engine at that instant would keep the"
           " accumulator and fire one feed later, which stays PR-116"
           " open item 1, not this test's claim) + PR-119 that same"
           " accumulator DRIFTS off an exact landing when the law"
           " gives an inexact feed, and does not when the feed is a"
           " binary fraction: F=20 T=30 feeds 1/3 of a unit per"
           " iteration, six of them sum to 1.9999999999999998 - one"
           " ulp under a 2-unit row the law says is DUE - so the row"
           " closes a full iteration late and carries"
           " 0.33333333333333304, under one feed and never the feed,"
           " while the position path had already paid its 2 px on the"
           " sixth iteration; the old F=30 shape, 0.375 = 3/8 exact,"
           " lands exactly on 3.0 on time and leaves exactly 0.0"
           " + PR-150/216 the ladder's contact answer: its state gate"
           " (0x3f1dc reads [obj+0x5c], which numbers states by their"
           " position in the list the script declares before its"
           " blocks, ONE-BASED, so the first declared state and every"
           " index but 2 answer nothing; block order is NOT that"
           " order - ZARIA.01's own DRAB.MOV differs - and the"
           " numbers the parser computes are legged on their own),"
           " the full-extent then 70%%-probe"
           " predicate with the merged CATCH+DRAB and lone-DRAB"
           " tails and the inclusive bounds, the probe's truncating"
           " symmetric law at every w I pinned by hand, the edge"
           " block's priority, the DRABINKA-only class scope, PR-269"
           " the AUTHORED COLUMN as the P2/P3 rectangle for a placed"
           " object - the `x0,y0;x1,y1` line its own placement text"
           " authors, made absolute by the record verb 0x3f0f0 at"
           " 0x3f124-0x3f14b, with the 70%% probe inside it and the"
           " inclusive corners legged - PR-270 the EDGE BAND from"
           " the same entry's `a,b` line - 0x3f16a-0x3f175, the"
           " 0x4a050 widening at trunc(n/2) and the 0x3f187-"
           "0x3f1a7 absolute store, ZARIA.01's (195,480)-(225,560)"
           " legged with its inclusive corners and P1's priority"
           " inside the column - and the model-default 1x1"
           " blocks only where NOTHING is authored, and PR-217 a"
           " row's `Wait <signal>`"
           " tail captured as the engine's own signal clause - one"
           " token in 254 of the 255 shipped tails, `!` in 22,"
           " K_NEVER/K_ANY kept as the no-bit specials, `WAIT ;`"
           " left counted; and PR-287 that SAME clause ACTING"
           " on the object's row admit - the row step 0x521f0"
           " asks the row node's test slot +0x8 at 0x52235 and"
           " 0x495b0 (the empty test) at 0x5223c, and only an"
           " EMPTY set reaches the threshold compare at 0x52329,"
           " while a `Wait` node's own set is matched against the"
           " caller's keys and [obj+0x60] at 0x522e6 and"
           " `je 0x52354` ENDS the step: no frame, no list wrap,"
           " no signal - so ZARIA.01's own ladder holds its A_UP"
           " row until K_GEN1 is live, then walks the authored"
           " 2700 units of A_DROP (27 of this port's iterations,"
           " its last row arming the list-end bit at"
           " 0x521c1-0x521c5), and holds frame 11 for good on"
           " `WAIT K_NEVER`; and"
           " PR-218 WHICH no-bit operand a captured clause names"
           " (K_NEVER's 0x60 mask, K_ANY's 0x2001f8 mask, or a"
           " name the comparator 0x48000 does not know), with"
           " the clause ENDED by the `;` the engine's row text"
           " puts there, so `WAIT K_NEVER ;,smpauto` is the"
           " K_NEVER clause and not an unknown name; and PR-335"
           " the entry's OWN SURFACE TABLE - 0x45120's record"
           " grammar (a count, then one separator and a pair per"
           " record: the separator after record i is record i's"
           " flag byte, ';' 1 '#' 2 any other character 0, and"
           " the last record carries none; a zero count allocates"
           " nothing; the literal ';' of 0x451e2 ends the line"
           " and ZARIA.01's KATAPULT trailing pair has no"
           " reader), 0x45200's placement step making the table"
           " ABSOLUTE like the contact points, and the order of"
           " the four continuation grammars, ladder list first"
           " + PR-342 the SURFACE TABLE MOVE of the class +0xa4 verb at"
           " the row admit - the row cursor PR-341 read: PALMA's 0x1e5c0"
           " steps the table 1 px down while the cursor is under 7 and up"
           " from 7, so ZARIA.01's own 14-row PALM.MOV cycle ends where"
           " it began, and KATAPULT's 0x1e690 jumps it by (+13,+5),"
           " (+13,+6), (+13,+10), (+3,+10), (-42,-31) at cursors 3..7,"
           " a path whose farthest point is (+42,+31) and which closes;"
           " a KAMIEN, whose class holds 0x239c0 at that slot and no"
           " table verb, keeps its table exactly where it was authored)"
           " + PR-352 the ENGINE'S OWN STANDING QUERY on those tables -"
           " 0x45380's bracket (left end inside, right end NOT, a hit is"
           " the first record past x), the flag gate of 0x4540c reading"
           " record 0 ONLY, and 0x45400's two 0x455a0 calls, which"
           " PR-392 reads as ONE line value taken at TWO points: both"
           " take the pair in the bracket's order (A = record i-1, B ="
           " record i), the first at the +0x6c sample pair [obj+0x89]"
           " and the second at the aimed block, and `idiv` truncates"
           " toward zero, so the value sits one pixel above the real"
           " line on a falling span; 0x4547d rejects a surface above the"
           " SNAPSHOT unless it is the one at [obj+0xa9], and"
           " 0x45496/0x4549a require the AIM value to be <= 0 and keep"
           " the MOST NEGATIVE, so the highest answerable surface"
           " wins. Measured over full cycles, ZARIA.01's four ANIPL2 seat"
           " tables cover x 838..900, 885..947, 930..992 and 307..382 with"
           " tops y 475..482, 380..387, 320..327 and 633..664, all of them"
           " level, so 0x45400 carries no step of the authored chain"
           " (53..306 px between every pair of top bands)"
           " (ok)\n");
    return 0;
}


/* find "<stem>.<ext>" (case-insensitive) in a block record list */
static const llrs_slot *rec_find(const llrs_slot *recs, unsigned n,
                                 const char *stem, const char *ext)
{
    for (unsigned k = 0; k < n; k++) {
        if (recs[k].id != 1 || !recs[k].name[0])
            continue;
        char st[16];
        stem_of(recs[k].name, st, sizeof st);
        const char *dot = strrchr(recs[k].name, '.');
        if (ci_eq(st, stem) && dot && ci_eq(dot, ext))
            return &recs[k];
    }
    return NULL;
}

/* ---------------- playable-level runtime (M4-B, PR-43) -------------------
 * LEO_PLAY wires the real player: art = every .SEQ in the ANIMACJE leaf
 * named by the .des `lew=<class>,<art leaf>,<mov>,<player des>` token 2,
 * script = the HD leaf's <stem>.MOV (ll_play.c), ground = the ground
 * polylines named by `platformy=` in the HD ETAPY level block - either
 * the block's <stem>.PLT record or, for the levels that say so, a
 * sub-.des record of the same block whose text IS the same polyline
 * grammar (TUNGA2.12/SADES.06 platf.des; JUNCTION none.des = "0" = no
 * ground), spawn = `pozycja=`, bounds
 * (and the fall-out-of-world respawn) = `lew_rect=`. The simulation and
 * the .MOV physics live in ll_play.c; this file only loads the data and
 * draws. If any piece is missing the level stays a preview and the gap
 * is logged (data-hole), never guessed.
 * The CI drives the same runtime headless through LEO_PLAY_SCRIPT
 * ("<tick>:<FWD|BKWD|UP|DOWN|JUMP|FIRE|F1..F4|ALT|NONE> entries, comma-
 * combinable per entry e.g. "100:FIRE,F1" (PR-87, mirrors the .MOV's
 * AND-groups); whitespace separated; the
 * last entry at or before the current tick is held until the next one),
 * on the fixed grid of the LEO_LEVEL_PPM leg - one engine iteration per
 * tick, LL_ITER_MS (PR-135) - deterministic. */
/* PR-531: the shipped start value of the life counter is 5, NOT the
 * video's 6: `mov DWORD ds:0xa94f0,0x5` at 0x5d2f9 (inside the queue
 * runner 0x5d2f0, which the level stubs also reach on loop code 2) and
 * again at 0x5e3f0/0x5e3fb (PR-530, OI-083). One named constant so the
 * run start and the code-2 reset cannot drift apart. */
#define LL_LIVES_START 5

typedef struct {
    int have;                               /* fully wired = play mode on */
    ll_plt plt;
    ll_mov_t *mov;                          /* heap: ~160 KB              */
    ll_player p;
    struct {
        char stem[16];
        unsigned char *blob;
        ll_seq seq;
    } art[32];
    int nart;
    char leaf[24];                          /* art leaf being claimed     */
    char lion_class[24];                    /* PR-526: the `lew=` first   *
                                             * field - the level's own    *
                                             * lion CLASS name, exactly   *
                                             * what the level-session     *
                                             * open 0x4dde0 compares in   *
                                             * its factory arms (0x1fef0  *
                                             * answers 'TLewZaria'/       *
                                             * 'TLewZariaB'), and the    *
                                             * class carries the         *
                                             * end-of-stage rule read at *
                                             * 0x1e250                    */
    double xmin, xmax, ydead;               /* lew_rect bounds. PR-532    *
                                             * READ the engine's real      *
                                             * death LINE: 0x54458-0x5446e *
                                             * is sx16(WORD [desc+0x2])    *
                                             * + 0x1f4, desc =              *
                                             * [[tlo ctrl 0xcef40+0x390]   *
                                             * +0x44] + main_back*4 - the  *
                                             * main_back .PCM descriptor,  *
                                             * +0x2 = its height (0x57460  *
                                             * uses the same word - 200).  *
                                             * The lew_rect bottom stays   *
                                             * the stand-in until PR-533   *
                                             * ports it. */
    double sx, sy;                          /* pozycja spawn              */
    ll_play_keys keys;                      /* held arrows (live path)    */
    int layer;                              /* the lion's layer = main_back */
    int energy, lives, lock;                /* PR-68: the counter pair;    *
                                             * PR-531: the engine's run     *
                                             * start is 5 (0x5d2f9), the   *
                                             * life is spent by the verb   *
                                             * below and a zero reaches    *
                                             * loop code 2 (loop_exit)     */
    int loop_exit;                          /* PR-531: pending level-loop   *
                                             * exit CODE 2 - 0x4ae82 tests  *
                                             * the counter every iteration  *
                                             * and 0x4ae8b writes ebx=2     */
    int solid;                              /* PR-71: contact pass on/off      */
    int danger;                             /* PR-90: the DANGER + class   *
                                             * MAIN box arming stand-in    *
                                             * (off by default)            */
    int cam_ox, camy_v, camy_set;           /* PR-72: camera follow state      */
} play_ctx;

/* PR-526: LEO_PLAY_STATE="<tick>:<STATE>[,...]" - a TEST INJECTION, not
 * an engine law: it puts the lion into a named state of its own script at
 * that tick, exactly as a matched transition's applier does (the port's
 * state-change block: authored start row through ll_mov_row_start, both
 * feeds zeroed, the PR-321 word and the attack window cleared). It exists
 * because some authored states have no entry a leg can drive from the
 * keys yet: A_WPAD's five entry lines (ZAR LEW.MOV lines 53/78/105/
 * 131/219) all ask AC_ONAIR,AC_DRECT. The port DOES raise AC_DRECT -
 * ZARIA.01 authors the one DRECT box in the shipped placement text,
 * 988,645;1340,750, and rect_class_answer answers it for a falling
 * player inside it (OI-018) - and PR-529's ly_pool leg now walks the
 * level far enough to reach it under keys: the answer at 506 and the
 * authored `A_FALL_DOWN -> A_WPAD` at 507 are that run's own lines.
 * The injection stays for the states a leg still cannot drive from the
 * keys (the PR-526 stage-complete leg enters A_WPAD at tick 5), and
 * for the ones whose entry the port has no path into at all.
 * Nothing is printed and no byte changes unless the variable names a
 * state. */
static void play_force_state(play_ctx *pl, const char *spec, int tk)
{
    if (!spec)
        return;
    const char *p = spec;
    while (*p) {
        char *end = NULL;
        long t = strtol(p, &end, 10);
        if (end == p || *end != ':') {
            p++;
            continue;
        }
        const char *v = end + 1;
        char tok[24];
        size_t n = 0;
        while (v[n] && v[n] != ',' && n + 1 < sizeof tok) {
            tok[n] = v[n];
            n++;
        }
        tok[n] = '\0';
        if (t == tk && pl->have && pl->mov) {
            for (int i = 0; i < pl->mov->nst; i++) {
                if (!strcmp(pl->mov->st[i].name, tok)) {
                    ll_player *q = &pl->p;
                    q->sti = i;
                    q->fi = ll_mov_row_start(&pl->mov->st[i], 1);
                    q->row_t = 0;
                    q->facc = 0.0;
                    q->atc = 0;
                    q->atc_hit = 0;
                    q->lock = 0;
                    q->dirword = 0;
                    printf("ll_level: play state injection: tick %d: %s "
                           "(test hook; its authored entries ask AC_ONAIR,"
                           "AC_DRECT and the leg cannot reach the pool "
                           "box at 988,645;1340,750 yet)\n", tk, tok);
                    break;
                }
            }
        }
        p = v + n;
        while (*p == ',')
            p++;
    }
}

static ll_play_keys script_keys(const char *s, int tick)
{
    ll_play_keys k = { 0 };
    if (!s)
        return k;
    int best = -1;
    const char *p = s;
    while (*p) {
        char *end = NULL;
        long t = strtol(p, &end, 10);
        if (end == p || *end != ':') {
            p++;
            continue;
        }
        const char *v = end + 1;
        char tok[12];
        size_t n = 0;
        while (v[n] && v[n] != ' ' && v[n] != '\t' && n + 1 < sizeof tok) {
            tok[n] = v[n];
            n++;
        }
        tok[n] = '\0';
        if (n && (int)t <= tick && (int)t > best) {
            best = (int)t;
            /* PR-87: comma-combinable parts, mirroring the .MOV's own
             * AND-groups: "100:FIRE,F1" = fire while weapon 1 selected
             * (the attack entry). Legacy combined token kept. */
            memset(&k, 0, sizeof k);
            for (char *sub = tok; sub && *sub; ) {
                char *cm = strchr(sub, ',');
                if (cm)
                    *cm++ = '\0';
                k.k_fwd  |= strcasecmp(sub, "FWD")  == 0;
                k.k_bkwd |= strcasecmp(sub, "BKWD") == 0;
                k.k_up   |= strcasecmp(sub, "UP")   == 0;
                k.k_down |= strcasecmp(sub, "DOWN") == 0;
                k.k_jump |= strcasecmp(sub, "JUMP") == 0 ||
                            strcasecmp(sub, "FWDJUMP") == 0;
                k.k_fire |= strcasecmp(sub, "FIRE") == 0;
                k.k_f1   |= strcasecmp(sub, "F1")   == 0;
                k.k_f2   |= strcasecmp(sub, "F2")   == 0;
                k.k_f3   |= strcasecmp(sub, "F3")   == 0;
                k.k_f4   |= strcasecmp(sub, "F4")   == 0;
                k.k_alt  |= strcasecmp(sub, "ALT")  == 0;
                if (strcasecmp(sub, "FWDJUMP") == 0)
                    k.k_fwd = 1;
                sub = cm;
            }
        }
        p = v + n;
    }
    return k;
}

static const ll_seq *play_art(const play_ctx *pl, const char *stem)
{
    if (!stem || !stem[0])
        return NULL;
    for (int k = 0; k < pl->nart; k++)
        if (ci_eq(pl->art[k].stem, stem))
            return &pl->art[k].seq;
    return NULL;
}

/* PR-97: the contact offset of a STATE's animation - the deepest shape-box
 * bottom (ll_seq_shape_contact) among the elements its frame list can show.
 * The per-frame value wobbles by a few px over a cycle (a walk cycle's
 * lowest solid row is the planted paw, not the lifted one: ZARIA LEW.ZAR
 * WALK 30..32, RAN LEW.RAN UJ09 10..14), and the frame the actor is on at
 * one instant is not a property of where it stands: two runs that differ
 * only in which state the actor came from would park its position point 2
 * px apart with the SAME state and frame on screen. So the offset is taken
 * once per animation: the position point then never depends on which frame
 * happens to be showing, and no frame's art can dip below the line.
 * MODEL CHOICE, not read from the machine code: the shape bottom as the
 * contact geometry is the data-derived part (see the ll_seq.h note), the
 * "max over the animation" part is this port's decision - the alternative
 * (per-frame) makes the sprite bob with the walk cycle. */
static int mov_state_contact(const ll_mov_t *mov, int sti, const ll_seq *s)
{
    int best = 0;

    if (!mov || sti < 0 || sti >= mov->nst || !s || !s->count)
        return 0;
    for (int i = 0; i < mov->st[sti].nfr; i++) {
        ll_frame_ref r = ll_mov_frame_ref(mov->st[sti].fr[i].frame);
        int d;

        if (r.blank || r.idx >= (int)s->count)
            continue;
        d = ll_seq_shape_contact(s, &s->els[r.idx]);
        if (d > best)
            best = d;
    }
    return best;
}

/* PR-97: the contact offset of the state the player is in (his current
 * animation's art, resolved exactly as play_draw resolves it). */
static int play_contact(const play_ctx *pl)
{
    if (!pl->have || pl->p.sti >= pl->mov->nst)
        return 0;
    const ll_mov_st *st = &pl->mov->st[pl->p.sti];
    const ll_seq *s = play_art(pl, st->seqkey[0] ? st->seqkey
                                                 : pl->mov->seqkey);

    return mov_state_contact(pl->mov, pl->p.sti, s);
}

/* PR-525: the anchor-shift resolver the `lock`/`unlock` row words call
 * through ll_player.anchor_shift. The engine side is 0x54f40/0x54f90:
 * the current-element getter [vt+0x48] (the lion's is 0x23aa0 - it
 * walks [obj+0x5c] and the state array [obj+0x50] to the CURRENT ROW's
 * element record), the flip getter [vt+0x50] (0x53e40: -1 while the
 * current frame record's flip word is set, +1 otherwise), and the
 * element record's own land/leave methods 0x42980/0x429d0, whose write
 * target is the object pair passed in edx. This resolves the same
 * element from the state's seqkey exactly as play_draw and play_contact
 * do, and hands back the delta pair - answering 0 when the row has no
 * element (frame 0 / out of range, the port's counterpart of the 0x7fff
 * sentinel guard at 0x4298d/0x429dd). */
static int play_anchor_shift(void *ctx, const ll_player *p, int land,
                             int *dx, int *dy)
{
    const play_ctx *pl = ctx;
    if (!pl->have || p->sti >= pl->mov->nst)
        return 0;
    const ll_mov_st *st = &pl->mov->st[p->sti];
    const ll_seq *s = play_art(pl, st->seqkey[0] ? st->seqkey
                                                 : pl->mov->seqkey);
    if (!s)
        return 0;
    int frame = ll_player_frame(p);
    ll_frame_ref r = ll_mov_frame_ref(frame);
    if (frame == 0 || r.blank || r.idx >= (int)s->count)
        return 0;
    int x = 0, y = 0;
    int facing = ll_player_mirror_of(p, r.mirror) ? -1 : 1;
    if (land)
        ll_seq_anchor_land(&s->els[r.idx], facing, &x, &y);
    else
        ll_seq_anchor_leave(&s->els[r.idx], facing, &x, &y);
    *dx = x;
    *dy = y;
    return 1;
}

/* PR-526: the +0x90 arm of the lion class THIS level's own .DES names.
 * ZARIA1.DES: `lew=TLewZaria,Lew.zar,lew.mov,lew.des` - the engine's
 * level-session open 0x4dde0 compares that first field at its 0x1fef0
 * arm and installs the matching lion; its class 0x96960 carries
 * the end-of-stage verb at +0x90 = 0x1e250 (dword at 0x969f0), which
 * raises [obj+0x116] when the running state is index 9 = A_WPAD (the
 * tenth name of ZARIA's own declared table) and this pass's live set
 * carries AC_BREAK_NEXT - the list wrap of A_WPAD's `1,200 ...
 * 11,200,unlock` wet rows. TLewZariaB (class 0x97a98, +0x90 =
 * 0x1e350, ZARIA2.DES) has NO such arm, and neither does the base
 * lion (0x96874+0x90 = 0x3d940), so the rule is keyed to the authored
 * class name and nothing else. The engine's comparison is the numeric
 * 9; 9 is A_WPAD's position in ZARIA's authored table, so the name is
 * the same fact, honestly spelled. */
static int play_end_rule(void *ctx, const ll_player *p)
{
    const play_ctx *pl = ctx;
    if (strncmp(pl->lion_class, "TLewZaria", 9) != 0)
        return 0;
    return p->broke &&
           p->sti < p->mov->nst &&
           !strcmp(p->mov->st[p->sti].name, "A_WPAD");
}

/* the surface under the spawn point is the CONTACT line: the position point
 * is placed contact px above it (PR-97; play_contact needs the state the
 * player starts in, which ll_player_start picks, so this runs after it) */
static void play_spawn(play_ctx *pl)
{
    ll_xy hit;
    int g = ll_plt_first_below(&pl->plt, (int)pl->sx, (int)pl->sy, &hit);
    ll_player_start(&pl->p, pl->mov, pl->sx, g ? (double)hit.y : pl->sy, g);
    pl->p.anchor_shift = play_anchor_shift;   /* PR-525: ll_player_start
                                              * memsets the player, so the
                                              * hook rides on every spawn */
    pl->p.anchor_ctx = pl;
    pl->p.end_rule = play_end_rule;           /* PR-526: the class arm,  */
    pl->p.end_rule_ctx = pl;                  /* same one-per-spawn rule */
    ll_player_set_contact(&pl->p, play_contact(pl));
    if (g && pl->p.grounded)
        pl->p.y = (double)hit.y - (double)pl->p.contact;
}

/* PR-59: the element an object's CURRENT frame shows, shape-only tables
 * included (obj_elem returns NULL for those because it gates on drawable
 * art; the collision test does not need art). */
static const ll_seq_elem *obj_shape_elem(const lvl_obj *o, int *mirror,
                                         const ll_seq **seq)
{
    const obj_art *a = obj_art_of(o);
    *mirror = 0;
    if (!a || a->seq.count == 0 || !o->sc->mov || o->sti < 0 ||
        o->sti >= o->sc->mov->nst)
        return NULL;
    const ll_mov_st *s = &o->sc->mov->st[o->sti];
    if (s->nfr <= 0)
        return NULL;
    int fi = o->fi < s->nfr ? o->fi : s->nfr - 1;
    ll_frame_ref r = ll_mov_frame_ref(s->fr[fi].frame);
    if (r.blank || r.idx >= (int)a->seq.count)
        return NULL;
    /* PR-81: the shape belongs to the sprite as it is DRAWN, so a turned
     * CH_DIR walker (obj_walk keeps face) uses the same polarity as
     * obj_draw - otherwise the push box would sit mirrored relative to
     * the art (ll_seq_shape_push takes the object polarity directly). */
    *mirror = obj_mirror_of(o, r.mirror);
    *seq = &a->seq;
    return &a->seq.els[r.idx];
}

/* PR-97: where this object's animation is solid below its position point:
 * the same ll_seq_shape_contact geometry and the same per-animation rule as
 * the player (mov_state_contact). The art is the table the object's CURRENT
 * state selected (obj_art_of), the same one obj_draw draws and play_collide
 * tests, so the ground line, the picture and the hit box agree. 0 when the
 * script has no frames or its art carries no shape. */
static int obj_contact(const lvl_obj *o)
{
    const obj_art *a;

    if (!o->sc || !o->sc->mov)
        return 0;
    a = obj_art_of(o);
    if (!a)
        return 0;
    return mov_state_contact(o->sc->mov, o->sti, &a->seq);
}

/* PR-70: the balloon pickup (class BAL, touch method FUN_0003d370): when
 * the mover is Lew and the balloon is in its first state, the point
 * (balloon.x + 10, balloon.y + 10) is tested against Lew's bounding rect
 * (inclusive on all four sides: jl/jg at 0x3d3d7..0x3d3e9); a hit sends Lew
 * the kind-9 message (FUN_00050df4): energy < 100 -> +32 (clamped to 100)
 * and a signal bit is set on the balloon (its K_GEN1 transition: A_RUN ->
 * A_DEAD, the pop). At full energy the message is ignored - the balloon
 * stays. */
static int balloon_hit(int bx, int by, int rx, int ry, int rw, int rh)
{
    int px = bx + 10, py = by + 10;

    return px >= rx && px <= rx + rw && py >= ry && py <= ry + rh;
}

/* PR-92: the lion's touch raises AC_ENEMY (bit 7 of the PR-88 table) on
 * the touched object. The engine site is the tail of the generic touch
 * handler (0x50ed0, 0x50fd4..0x51083): it builds a 12-byte named-set
 * answer packet, sets BIT 7 into it at 0x51050 (`or dh,0x80` = bit 7 of
 * byte 0), merges it through the TEventSet OR (0x49580) together with
 * whatever 0x55130 returned, and writes the packet into the head of the
 * OTHER object it touched ([obj+0x0..0xb], `rep movsd` 0x51074..0x51081).
 * The whole handler is gated by the invulnerability countdown [obj+0x102]
 * (0x502f0; setter default 14, 0x27264) - the port mirrors that gate
 * through o->hit_cd, which PR-89 already runs on the same fixed step.
 * The bit means "an enemy touched me" exactly as the data reads: all 14
 * active AC_ENEMY consumers in the shipped .MOV texts are object-side
 * lines - "A_DEAD(1)=AC_ENEMY" x6 (the vultures of RAN.01, the barrels
 * of KADMAR.07), "A_AWAY(1)=AC_ENEMY" x4, "A_SHOW(1)=AC_ENEMY" x2 (the
 * seal of LONA.05), "A_ATAK(1)=AC_ENEMY" x1 (the RBLOK of ENA.03) - so
 * the port raises it on the touched object's OWN set (the PR-88
 * mechanism, exactly what PR-89/91 shipped) and the object's own
 * transition table decides the reaction: only the raise is new.
 * MODEL CHOICES (recorded NEXT_STEPS PR-92 + HANDOFF item 10): the
 * packet is a mailbox at the object's HEAD, the port feeds the object's
 * own set - nothing else could read the bit (0x51062 wrote [obj+0x0]
 * fields no shipped model consumes); and the countdown gates but a pure
 * touch does not SET it - the engine's handler does set 14 there, and
 * shipping that would re-pin the PR-89 kill fixtures (an object touched
 * would reject the lion's next hit for 14 ticks); open question.
 * Balloons are excluded: they have their own touch method (the block
 * above). Raise and consume happen at once, exactly like the balloon
 * path: the object's table reacts on this beat or the bus goes empty
 * again (one-beat pulses are the PR-88 bus semantics). */
static void obj_touch_enemy(lvl_obj *o, int verbose, int tick)
{
    if (!o->sc || !o->sc->mov || o->dead || o->hit_cd > 0 || o->sti < 0 ||
        o->sti >= o->sc->mov->nst)
        return;
    o->sig |= 1ull << LL_SIG_AC_ENEMY;
    static int told;
    if (verbose && !told++)
        printf("ll_level: play tick %d: first touch raises AC_ENEMY on"
               " %s/%s: the object's own A_* lines decide the reaction"
               " (PR-92)\n", tick, o->sc->leaf, o->sc->movname);
    if (obj_transition(o, o->sc->mov, 0)) {
        if (verbose)
            printf("ll_level: play tick %d: %s/%s ANSWERS the touch:"
                   " AC_ENEMY -> state %s\n", tick, o->sc->leaf,
                   o->sc->movname, o->sc->mov->st[o->sti].name);
    }
    o->sig = 0;
}

/* PR-59: FUN_00053ae0 - what a solid object does to the lion. The lion is
 * B (pushed); an object is A when its CURRENT state carries SOLID (flag word
 * 0x400) and the lion's state is not ANTI_SOLID (0x10000). The push is
 * ll_seq_shape_push; SOLID alone displaces B by the penetration r
 * horizontally (message {1, r, 0}, 0x53c4d), SOLID2 by the full vector
 * (dx, dy) (0x53c0e). SEMI_SOLID: kind-1 r/5 applied, the kind-13 velocity
 * impulse counted (below).
 * A push UP (dy < 0) is standing on the object: grounded, support set.
 * The shape test uses the PAIRED form (ll_seq_shape_push2): the lion's
 * element and the object's element live in different .SEQ payloads, and
 * reading one from the other's bytes silently broke lion-vs-object
 * contacts (measured 2026-11-17, ZARIA.01: no lion/worm shape contact
 * at any distance; with the paired read the worms answer at their real
 * margin ranges). The earlier PR-66/PR-68 pins of this path were
 * therefore computed through wrong bytes and are re-pinned.
 * OFF unless LEO_PLAY_SOLID=1: WHICH pairs the game tests (the per-class
 * collision loops behind FUN_00053c90 and the CHECK_LEW/CHECK_ALL gating)
 * is not resolved yet, and with it on RAN.01's invisible EDGE0046 stops the
 * lion at x~135 - unverified against the original, so not the default. */
/* PR-139 (G-5): FUN_000550c0, verbatim. Answer-C (0x55130) asks it
 * before calling the hit: 0x550cf tests the TOUCHER's own window
 * predicate ([toucher.vt+0x40] = bit 5 of [toucher+0xb6]); when it is
 * NOT set the answer is "no suppression" (0x5511b). When it is,
 * 0x550db reads the toucher's facing getter ([toucher.vt+0x70] =
 * [toucher+0x6d] in 95 of the 111 class tables; the other 12 return a
 * constant 1), 0x550e6/0x550f1 take both positions through the level
 * provider's position getter 0x52d30 (its camera offsets are the same
 * function of the same layer, so they cancel between the pair),
 * 0x550f6..0x55108 build sign(toucher.x - self.x), 0x5510d multiplies
 * by the facing, and 0x55112..0x55119 return 1 - "skip this hit" -
 * only for a strictly negative product. So a window hit that the
 * object answers to the lion is cancelled exactly while the lion's
 * OWN window is open and the lion is turned toward the object (the
 * lion's attack wins the clash); turned away, at the same x (product
 * 0), or with the lion's window closed, the object's hit lands. */
static int clash_suppressed(const play_ctx *pl, int lx, int ox)
{
    int sd;

    if (!pl->p.atc)
        return 0;
    sd = lx > ox ? 1 : lx < ox ? -1 : 0;
    return sd * (int)pl->p.face < 0;
}

/* PR-150: THE LADDER'S ANSWER BECOMES BEHAVIOUR. The DELIVERY route
 * is PR-147 Finding 4's, read end to end: the mover's motion pipeline
 * (0x52750 -> its merger 0x52790 -> the candidate walk 0x4bd30, whose
 * lists are gated by the MOVER STATE's token bits - PR-131) puts the
 * pair before the ASKED object's +0x78 answer entry; the answer writes
 * a 13-byte set, the answerer appends it as a contact NODE (kind 2 =
 * the builder 0x53f50 that COPIES the set in, PR-149 F1), and the
 * asker's contact method ORs the node's stored set into the asker's
 * live set (0x54b65 -> 0x49580, PR-145) under a ROW guard (0x54b6a:
 * `[rec+8] == 0 || [rec+8] == [obj+0x5c]` - PR-157 F3 re-terms it a
 * STATE guard: [obj+0x5c] is the state index and the node carries
 * the state it was planted in; 0 = any). The port merges straight
 * away: the node list, its pop and the merge-delay are counted open
 * (item 4) - the port's pulse set is consumed at the same step
 * boundary the engine's key provider sits at (PR-88's order; the
 * raise lands after ll_player_step, the transitions of the NEXT
 * iteration read it). The answer itself is the ladder class's entry
 * 0x3f1c0, PR-149 F2's predicate tree, ported byte-for-byte:
 *   THE GATE FIELD [obj+0x5c] - PR-150 read it as a 1-BASED
 *   ROW POSITION and ported the gate onto o->fi (engine =
 *   fi+1); PR-157's row-state law plus the bytes re-read at
 *   0x3f1c2..0x3f24d (PR-158) CORRECT the attribution. 0x3f1c6
 *   `mov ecx,eax` makes ecx = SELF and the gate reads
 *   [ecx+0x5c] DIRECTLY - a field PR-157 named for what it
 *   is: the rows-array INDEX, the current STATE/record slot.
 *   [[obj+0x50] is built at placement by the +0x7C class verb
 *   0x537f0 (PR-157 F1); the base-ctor initializes the index
 *   to 1 (0x5321e) and the ENTER verb runs on rows[1]
 *   (0x539e6-0x539fb); the transition applier stores the
 *   TARGET RECORD INDEX at 0x535ed - the only functional
 *   writer the PR-157 census found in transition/action code
 *   (the 0x300d7-0x300e6 window is NOT a "state restart"
 *   idiom after all: PR-159 read its full context 0x30083-
 *   0x300f8 - alloc + 0x54030 ctor + field copies + class
 *   pair store - and its tail is CLONE/Ctor INITIALIZATION
 *   of a NEW object: index 1, rows[1]'s row cursor 0). The
 *   +0x44 getter 0x23920 indexes records with the field -
 *   PR-147/150 read that part right; what PR-150 got wrong is
 *   the field's UNITS: it does NOT count rows, and the
 *   authored `( N )` lands in the current state record's ROW
 *   CURSOR [[rec]+4] (0x53616, PR-157 F3), not here. So the
 *   gate at 0x3f1dc/0x3f214 is a STATE test, and PR-216 READS
 *   what its numbers mean. The loader parses the state list a
 *   script declares BEFORE its blocks into a name vector at
 *   [obj+0x3c] (the 0x53905 loop, its count at [obj+0x58]),
 *   allocates count+1 record slots (0x53974..0x53994), and the
 *   per-block handler 0x53480 stores its record at
 *   [[obj+0x50] + idx*4] where idx = 0x53440(name) = the
 *   ONE-BASED position of that name in the vector, 0 when the
 *   name is not declared (0x53471). Corroborating: the
 *   record-ctor loop starts at byte 4 = slot 1 (0x533de), and
 *   the object ctor writes [obj+0x5c] = 1 (0x538ab) before the
 *   ENTER verb runs records[1] (0x539e6..0x539fb). So the
 *   number is the DECLARED position, not the block order:
 *   1 = the first declared state -> the FLAG branch (the child
 *   ANY-test 0x44ec0 over [self+0xa5], sets [self+0x66] bit 6,
 *   answers NOTHING); 2 = the SECOND declared state -> the ONLY
 *   answering state, the P1/P2/P3 tree below; anything else,
 *   the not-declared 0 included, takes 0x3f217 -> the inert
 *   0x3f364. ZARIA.01's own DRAB.MOV is one of the 172 of 777
 *   shipped scripts where the two orders differ (census of the
 *   extracted texts): it DECLARES A_UP, A_DOWN, A_DROP but
 *   writes the blocks A_UP, A_DROP, A_DOWN - so the answering
 *   state is A_DOWN, NOT A_DROP. That CORRECTS PR-158/159's
 *   value mapping; their structure stands (a state test, first
 *   state inert, second state answers) - what was unknown then
 *   was which NAMES sit at those numbers (PR-157 items 2/3,
 *   PR-158 item 1's "binding order", the encrypted pool), and
 *   the loader answers it without the pool. PR-216 ports the
 *   state gate and DROPS the fi-gate: the row cursor is a
 *   different field ([[rec]+4]) and nothing in 0x3f1c0 reads
 *   it.
 *   THE PREDICATE, 0x23e00 verbatim (PR-106's point-in-rect:
 *   `p[0] in [q0..q2]` and `p[4] in [q1..q3]`, four signed
 *   jl/jg, INCLUSIVE): the asker side is its OWN first two dwords
 *   - PR-149 F2 pinned the operand chain: the +0x68 getter (the
 *   ret-self base getter 0x23b60 in 23/24 family tables, 0x23900
 *   = `add eax,0xd6` = the POSITION BLOCK in the lion's) hands the
 *   method the asker object, and its slot +0x8 is called with the
 *   ladder's block as edx. The port's asker point is the
 *   position (the first two dwords of the lion's position block).
 *   P1 (0x3f21d..0x3f24d): asker point vs the ladder's STORED
 *   block [self+0xc5] -> TRUE: the answer is AC_EDGE (bit 8) and
 *   NOTHING else.
 *   P2 (0x3f256..0x3f2a4): asker point vs the ladder's block
 *   [self+0xb1] at FULL extent: the stack copy at esp is inset
 *   first, but the FIRST test runs against ecx = self+0xb1 (the
 *   `add ecx,0xb1` at 0x3f283 feeds edx at 0x3f29b - read off
 *   the bytes THIS pass) - FALSE -> the inert exit 0x3f364.
 *   P3 (0x3f2aa..0x3f355): asker point vs the 70% PROBE (edx =
 *   esp, the inset stack copy, 0x3f2ad): TRUE -> AC_DRAB |
 *   AC_DRAB_CATCH (bits 10|12, `or dh,0x10` + `or bh,0x4`
 *   merged through 0x49580, 0x3f2dc..0x3f313); FALSE -> AC_DRAB
 *   ALONE (0x3f355). (CORRECTION, flagged against this session's
 *   OWN shipped PR-149 wording, which had P2 = inset / P3 =
 *   full: the operands are the other way round, and the
 *   correction makes all four PR-141 outcomes reachable with
 *   ONE rectangle format - CATCH inside the probe's band, DRAB
 *   alone in the outer band, inert outside, EDGE via the other
 *   block.)
 * THE 0x49fb0 ARITHMETIC, RE-READ BYTE BY BYTE THIS PASS (n =
 * 0x46 = 70, set at 0x3f278) - and a SECOND correction to the
 * PR-149 wording, recorded not hidden: the shipped text says
 * "[p] += inset, [p+8] -= half - asymmetric halving". The bytes
 * say otherwise: `0x49fde add esi,eax` and `0x49fe0 sub edi,eax`
 * use the SAME eax, the halved remainder (the inset value is
 * consumed at 0x49fce only, to form t = w - inset; eax is
 * rewritten at 0x49fd0/0x49fd5/0x49fd7). The true law:
 * w = [p+8]-[p]; inset = trunc(w*n/100) (idiv - C truncation
 * matches); t = w - inset; half = trunc(t/2) (the
 * `t - (t >>s 31)` then `sar 1` idiom IS division by two
 * truncated toward zero); then BOTH edges move by that SAME
 * half: [p] += half, [p+8] -= half. A SYMMETRIC inset keeping
 * the CENTRAL n percent of the x-span - for n=70 the middle 70%
 * (odd leftovers stay inside the probe; w <= 3 leaves it
 * unchanged, w <= 1 likewise). That is also exactly the
 * predicate shape needs: the probe sits INSIDE the full extent,
 * so "P2 true / P3 false", AC_DRAB alone, means precisely
 * "inside the rung, outside its middle 70%". The x-words are the
 * pair [p]/[p+8] - the SAME pair 0x23e00 bounds-tests at
 * 0x23e03/0x23e07 (`p[0] in [q[0], q[8]]`, `p[4] in [q[4],
 * q[0xc]]`, signed jl/jg, INCLUSIVE) - which pins which words
 * carry the x-span but not yet the layout of the other pair (a
 * DESCENDING-x block would make even P2 unsatisfiable under
 * 0x23e00's bounds test, a fact pinned in a leg; and the ctor
 * never stores [obj+0xc5] itself, PR-149, so even the engine's
 * default EDGE block value is un-read - the port's 1x1 is a
 * MODEL CHOICE, pinned by legs, never a read fact).
 * WHERE THE ENGINE GETS BOTH RECTANGLES (found by PR-268; this
 * paragraph replaces PR-149's open item 3 and PR-150's "no
 * authored source" wording): the class table's slot +0x18 is the
 * RECORD VERB, the only slot in any of the 197 named tables that
 * calls the scanf helper 0x49920, and for this class it is
 * 0x3f0f0. It reads the object's own placement text: 0x49f90
 * with the format at 0x91d00 (`%d,%d;%d,%d , `) fills
 * [obj+0xb1..0xbd] as x0,y0,x1,y1 and 0x3f124-0x3f14b adds the
 * placement x,y, so the column is ABSOLUTE from load and never
 * follows the object; the next line, a bare `a,b`, goes through
 * 0x4a0b0 into [obj+0xc5..0xd1] as x0 = x1 = a, y0 = b-0x50,
 * y1 = b, and 0x4a050 widens it to the authored column's own
 * width centred on a, again made absolute (0x3f187-0x3f1a7).
 * ZARIA.01's own text authors `23,30;53,160` and `30,50` after
 * `DRABINKA,drab.ani,drab.mov,180,510`, i.e. the column
 * (203,540)-(233,670) and the band (195,480)-(225,560) - 80 px
 * at the TOP of the ladder, which is what its lion script's
 * `A_PODCIAG(1)=AC_EDGE,K_UP` asks for.
 * THE PORT'S SOURCES: the rung block (the [self+0xb1] role,
 * P2/P3's q) is that AUTHORED column when the object's own
 * placement entry authors one - place_rect (PR-246) parses the
 * same four numbers and makes them absolute the same way, and
 * the loader prints them ("authors an authored rectangle:
 * absolute ..."); leg L11 pins the answer with them. An object
 * with NO authored rectangle - a synthetic object, or a class
 * whose record verb fills those fields from another parameter
 * (the ` bounds = %d, %d` family, 0x3b500) - keeps the old
 * fallback: the object's current art bbox, or the ctor-shaped
 * 1x1 at the object with no art. That fallback is a COUNTED
 * boundary, not the model. The edge block (the [self+0xc5]
 * role, P1's q) is the band from the pair line - built by
 * 0x3f16a-0x3f175 and widened by 0x4a050 as above (PR-270,
 * legs L12) - and an object with NO authored pair keeps the
 * 1x1 default, also counted. P1's priority over the rung
 * answer is a separate leg.
 * CLASS SCOPE (PR-528 EXTENDS IT): two chains are anchored end to
 * end. DRABINKA's is PR-141's (factory 0x1f8ac -> class table
 * 0x97cdc, +0x78 = 0x3f1c0). LINA's factory arm is 0x4e81d: the
 * name test 0x76a69 against "LINA" at 0x92005 allocates 0xd9,
 * calls ctor 0x531f0, and the pairs at 0x4e8c4/0x4e8d2 leave the
 * object's [obj+8] view = 0xa1dfc, whose +0x78 IS 0x3f390 and
 * whose +0x18 is the SAME record verb 0x3f0f0 - the rope reads
 * its column and band with the same grammar (OI-117 cited the
 * 0xa1dfc+0x78 row). The K-prefixed variants (KLINA, KDRABINA)
 * stay unanchored and unanswered, item 2.
 * 0x3f390 IS 0x3f1c0'S SHAPE, with three measured differences
 * (PR-528, read from the bytes this pass): the state gate is the
 * same (1 = the K_GEN1 flag raise at 0x3f3d3-0x3f3df, exactly
 * 0x3f203's form; 2 = the AC packet; anything else the inert
 * tail 0x3f534); P1 is the same edge band and answers the same
 * AC_EDGE (`or BYTE PTR [ecx+1],1`, 0x3f41d); P2/P3 differ: the
 * probe inset word is `mov edx,0x32` at 0x3f448 = 50, not 0x46 =
 * 70 (0x3f278), so the rope keeps the CENTRAL 50% of its column,
 * and the merged pair is `or dh,0x40` at 0x3f4ac (byte 6 bit 6 =
 * bit 22 = AC_LINA_CATCH) merged with `or bh,0x20` at 0x3f4d4
 * (byte 2 bit 5 = bit 21 = AC_LINA) through 0x49580/0x749a0,
 * while P3-outside-the-probe answers AC_LINA ALONE (`or BYTE PTR
 * [ecx+2],0x20`, 0x3f525). The bit pairs sit in different bytes
 * because AC_LINA/AC_LINA_CATCH are indexes 21/22 in the name
 * block at 0x91ad9/0x91ae1, where AC_DRAB/AC_DRAB_CATCH are
 * 10/12 - the port's mask uses those indexes verbatim.
 * WHAT DOES NOT SHIP (counted, not hidden): the kind-4 contact
 * node and its +/-10 y-override (PR-147 - PR-149 F1 built the
 * node shape it needs, the natural next PR); object-asked answers
 * (the engine's candidate walk runs for every stepped mover,
 * 0x52790 is in 15 class units; the port has no object-to-object
 * contact pass, so only the LION asks - item 3); the node-list
 * lifetime and merge delay (item 4); the [obj+0xa5] child ANY-
 * test of the row-1 branch (the port models no child list - the
 * feature was never read - so the engine's row-1 answer here is
 * the inert exit and [obj+0x66] bit 6 is never set, counted).
 * AT ZARIA.01 THIS CHANGED NOTHING OBSERVABLE UNTIL PR-528 BOUND
 * LINA: the ly_rope leg below shows the bound rope asking with
 * 0x3f390, taking its own K_GEN1 unroll, answering AC_EDGE at its
 * band and the CATCH pair in its column, and running the lion's
 * authored descent/hang/climb chain. The DRABINKA reading stayed
 * as PR-149/PR-216 left it, and the legs say why instead of
 * asserting it away: (i) the authored A_WALK token
 * carries no CHECK bit for the ladder's list (PR-131's gate, the
 * same gate the port's play_collide applies - zero answer
 * calls); (ii) even with the gate open the placed ladder answers
 * nothing, because a level-placed ladder sits in the FIRST DECLARED
 * state A_UP = index 1 = the FLAG branch (the child ANY-test, which
 * the port does not model, counted), and its ONLY authored exit is
 * `A_DROP(1)=K_GEN1`: K_GEN1 is never raised on it (PR-149 F5's
 * census with its two negatives), so state 2, A_DOWN, the one that
 * DOES answer, is unreachable - that and not the row cursor is the
 * named blocker, and PR-216 narrows it to one question: what raises
 * K_GEN1 on a DRABINKA. The transition into it is authored and
 * readable (`A_UP -> A_DROP` on row 1, then `A_DOWN(1)=
 * AC_BREAK_NEXT`), so once K_GEN1 is understood the chain needs no
 * new law. What the row-cursor law of PR-157 F3 says still stands
 * for the `WAIT K_GEN1`/`WAIT K_NEVER` row gate (a different field,
 * [[rec]+4], which the port's o->fi tracks); what PR-216 corrects
 * is only which field 0x3f1dc reads. Both legs are below, not
 * asserted away. */
static int ladder_class(const lvl_obj *o)
{
    if (!o->sc)
        return 0;
    if (ci_eq(o->sc->leaf, "DRABINKA"))
        return 1;                      /* 0x97cdc +0x78 = 0x3f1c0  */
    if (ci_eq(o->sc->leaf, "LINA"))
        return 2;                      /* 0xa1dfc +0x78 = 0x3f390  */
    return 0;
}

/* PR-257: the 0x3f1c0 branch gate for the AC answer packet. The verb
 * reaches that branch only at [obj+0x5c] == 2, the DOWNED state an
 * unrolled rope settles in; a ladder in its waiting state (1) or
 * mid-unroll (3) takes the verb's tail - no answer at all. */
static int ladder_answer_branch(const lvl_obj *o)
{
    return o->sc->mov->st[o->sti].decl_idx == 2;
}

typedef struct { int x0, y0, x1, y1; } ll_blk;   /* the engine's block
                                                  * order: [p]/[p+4] vs
                                                  * [q0..q3] at 0x23e00 */
typedef struct { int x, y; } ll_pt;

static int pt_in_blk(const ll_pt *p, const ll_blk *q)
{
    /* 0x23e00 verbatim: [p] in [[q0],[q2]], [p+4] in [[q1],[q3]],
     * signed, jl/jg - inclusive on all four sides. */
    return p->x >= q->x0 && p->x <= q->x1 &&
           p->y >= q->y0 && p->y <= q->y1;
}

static void blk_probe_pct(ll_blk *b, int n)
{
    /* 0x49fb0 with the caller's n (0x3f278 hands 0x46 = 70 for the
     * ladder, 0x3f448 hands 0x32 = 50 for the rope, PR-528),
     * verbatim 0x49fb6..0x49fe4 and CORRECTED against the PR-149
     * wording, see the model
     * note: inset the x-words SYMMETRICALLY by half =
     * trunc((w - trunc(w*n/100)) / 2) - keep the central n%.
     * Both C divisions truncate toward zero exactly where the
     * engine's idiv and the `sar 31 / sub / sar 1` idiom do, so
     * the plain code IS the byte law for both signs. The x-words
     * only; y untouched. w*n must not overflow: engine imul is
     * 32-bit too, and level spans are far below 2^31/n. */
    int w = b->x1 - b->x0;
    int ins = w * n / 100;            /* C99 division truncates
                                       * toward zero = idiv exactly */
    int t = w - ins;
    int half = t / 2;                 /* = the sar idiom, both signs */

    b->x0 += half;
    b->x1 -= half;
}

static unsigned long long ladder_answer_core(int kind, int state,
                                             const ll_blk *edge,
                                             const ll_blk *rung,
                                             const ll_pt *ask)
{
    ll_blk probe;
    int n = (kind == 2) ? 50 : 70;    /* 0x3f448 vs 0x3f278 */
    unsigned long long m_one =
        (kind == 2) ? (1ull << LL_SIG_AC_LINA)
                    : (1ull << LL_SIG_AC_DRAB);
    unsigned long long m_catch = m_one |
        (1ull << ((kind == 2) ? LL_SIG_AC_LINA_CATCH
                              : LL_SIG_AC_DRAB_CATCH));

    if (state != 2)
        return 0ull;                   /* the ONLY answering state is
                                        * the SECOND DECLARED one
                                        * (0x3f1dc reads [self+0x5c],
                                        * `cmp 2` at 0x3f214 -> the
                                        * P1/P2/P3 path; the rope's
                                        * verb is the same shape at
                                        * 0x3f3ac/0x3f3e4): 0x53440
                                        * numbers states from the
                                        * declared list, 1-based, and
                                        * the ctor starts the object
                                        * at 1, 0x538ab. `cmp 1` =
                                        * the FIRST declared state:
                                        * the child ANY-test 0x44ec0
                                        * over [self+0xa5] (header
                                        * count at +0x04, entries at
                                        * stride 0xc, each entry
                                        * asked through its own
                                        * class +0x08 verb,
                                        * 0x44ee3-0x44eec). Read in
                                        * full by PR-304: on a hit
                                        * it sets [self+0x66] bit 6
                                        * = signal bit 54 = K_GEN1
                                        * on the object itself and
                                        * leaves the caller's answer
                                        * set untouched, both paths
                                        * ending at 0x3f364, so 0
                                        * here IS the engine's
                                        * answer and nothing is
                                        * assumed away. Any other
                                        * index, including 0 = the
                                        * name is not declared,
                                        * falls to 0x3f217 ->
                                        * 0x3f364, inert. */
    if (pt_in_blk(ask, edge))         /* P1: [self+0xc5] */
        return 1ull << LL_SIG_AC_EDGE;
    if (!pt_in_blk(ask, rung))        /* P2 FALSE: inert exit 0x3f364 */
        return 0ull;
    probe = *rung;
    blk_probe_pct(&probe, n);
    if (pt_in_blk(ask, &probe))       /* P3 TRUE: merged, 0x3f2dc.. */
        return m_catch;               /* /0x3f493.. */
    return m_one;                     /* P3 FALSE: 0x3f355 / 0x3f525 */
}

/* the object's current block, port-source version (see the model
 * note). The SAME shape path as play_collide's two contact passes:
 * obj_shape_elem (the current row's element, its fi clamp, and the
 * drawn polarity - PR-81 keeps shape and sprite one polarity),
 * then the element box positioned at the object exactly as the
 * balloon pass above positions the LION's element box. An element
 * with no margin lists is skipped by the game's hit tests (the
 * rule ll_seq_shape_contact's contract names), so the port returns
 * 0 there and the caller falls back to the ctor-shaped 1x1. */
static int blk_of_obj(const lvl_obj *o, ll_blk *b)
{
    int mirror;
    const ll_seq *oseq = NULL;
    const ll_seq_elem *e = obj_shape_elem(o, &mirror, &oseq);

    b->x0 = b->x1 = o->x;
    b->y0 = b->y1 = o->y;
    if (!e)
        return 0;
    {
        int tw, th, rows, x, y, w, h;

        ll_seq_shape_wh(e, &tw, &th, &rows);      /* *n = row count */
        if (rows <= 0)
            return 0;
        ll_seq_shape_bbox(oseq, e, &x, &y, &w, &h);
        if (mirror)                /* mirrored shape: mirror the box
                                    * about the position point, the
                                    * polarity the shape pass uses */
            x = -(x + w);
        b->x0 = o->x + x;
        b->y0 = o->y + y;
        b->x1 = o->x + x + w;
        b->y1 = o->y + y + h;
    }
    return 1;
}

static ll_pt pt_of(const ll_player *p)
{
    ll_pt q;

    q.x = (int)(p->x >= 0 ? p->x + 0.5 : p->x - 0.5);
    q.y = (int)(p->y >= 0 ? p->y + 0.5 : p->y - 0.5);
    return q;
}

/* PR-235: the AC_DRAB contact raise law (0x287a0, reached ONLY from
 * the collision loop 0x3eac6 over the [obj+0x30] neighbour list).
 * eax = the LION, ebx = the other object. Gates measured:
 *  - the LION's CURRENT state's record ID word == 6 (0x287de-
 *    0x287ed) or == 0x12 (0x2880b-0x28826). The ID is the
 *    authored block position (the ds:0xceff8 counter handed to
 *    every record create, 0x5150b/0x51a2a; copied to object
 *    +0x8d with the record at 0x5386c, read back through the
 *    +0x6c accessor 0x23a10 = `add eax,0x89`.. the +0x89/0x8d
 *    record head inside the object). ZARIA.01's LEW.MOV block
 *    order puts A_DEAD at 6 and A_DRAP at 18 = 0x12 - the hang
 *    and dead states, exactly the states whose authored rows
 *    read AC_DRAB (`A_FALL_DOWN(1)=!AC_DRAB` from A_DRAP).
 *  - the OTHER object's same ID word tested `> 0x19 -> skip`
 *    (0x28823) - the model applies the bounded half (id <= 25)
 *    and NOT the `[[other]+0x3c]` probe or the second clause
 *    (0x28820 reads another word of the other's record), named
 *    model gaps.
 * On a pass it builds the synthetic signal message (type 0x10,
 * tag 0x8138764 at 0x2882d, payload 0xa = AC_DRAB at 0x28835)
 * and posts it through the target's class +0x10 verb - 0x2885a is
 * that CALL SITE, not a table entry (PR-319 measured the LION's own
 * +0x10 cells: 0x3d8f0 for the class 0x96874 and 0x1e130 for
 * 0x96960; 0x3d8f0 handles opcode 5 itself and chains every other
 * opcode to the base handler 0x54560, where opcode 0x10 falls
 * through to that handler's default 0x53770) - the chain that ends
 * with the bit OR-s into [lion+0x60] (PR-225): a raise on the
 * LION, class-scoped here to the ladder family like the answer
 * path (PR-304 read that scope: the answer verb 0x3f1c0 opens
 * with `call [[asker+8]+0x3c]`, so it answers only an ASKABLE
 * asker, which PR-303 measured as a lion). */
static int acdrab_law(int lion_id, int other_id)
{
    return (lion_id == 6 || lion_id == 0x12) && other_id >= 0 &&
           other_id <= 0x19;
}

static unsigned long long ladder_answer_for(const lvl_obj *o,
                                           const ll_player *asker)
{
    ll_blk rung, edge;
    ll_pt p = pt_of(asker);
    const ll_mov_st *s = NULL;
    int state;

    if (o->sc && o->sc->mov && o->sti >= 0 && o->sti < o->sc->mov->nst)
        s = &o->sc->mov->st[o->sti];
    state = s ? s->decl_idx : 0;       /* the engine's [obj+0x5c] for
                                        * the state this object runs;
                                        * no state at all gives 0,
                                        * which is also the engine's
                                        * not-declared value (0x53440
                                        * falls through to 0x53471) */
    if (o->nrect) {
        /* PR-269, the OI-117 law PR-268 measured: for a PLACED object
         * the engine's rung rectangle is the column the placement text
         * itself authors. 0x3f0f0 (the record verb, table slot +0x18)
         * reads `x0,y0;x1,y1` through 0x49f90 - the format at 0x91d00
         * is `%d,%d;%d,%d , ` - into [obj+0xb1..0xbd], and
         * 0x3f124-0x3f14b adds the placement x,y, so the block is
         * ABSOLUTE from load and never follows the object. The port's
         * place_rect already parses and makes absolute exactly those
         * four numbers (PR-246), which is what its loader prints as
         * "authors an authored rectangle". This is the rectangle P2
         * and the 70% probe P3 are tested against - not the art box. */
        rung.x0 = o->arx0;
        rung.y0 = o->ary0;
        rung.x1 = o->arx1;
        rung.y1 = o->ary1;
    } else if (!blk_of_obj(o, &rung)) {
        rung.x0 = rung.x1 = o->x;      /* the ctor-shaped 1x1 default */
        rung.y0 = rung.y1 = o->y;
    }
    if (o->npair && o->nrect) {
        /* PR-270, the edge band, from the record verb's own
         * instructions: 0x3f16a-0x3f175 builds [obj+0xc5] as
         * x0 = x1 = a, y0 = b-0x50, y1 = b; 0x3f178-0x3f17d
         * computes n = ([rect].x1 - [rect].x0) + 1 and calls
         * 0x4a050, which at 0x4a058-0x4a074 sets d = ([+8] -
         * [+0]) - n, half = d/2 truncated toward zero, [+0] +=
         * half, [+8] -= half - with x0 = x1 the span is 0, so
         * the band is x = a +/- trunc(n/2); 0x3f187-0x3f1a7
         * makes all four absolute. The port stores the absolute
         * pair point (apx, apy, place_pair), so the band is
         * x apx +/- trunc(n/2), y apy-0x50..apy. ZARIA.01's
         * `30,50` with n = 31: x 195..225, y 480..560 - the
         * top 80 px of the authored ladder, where its lion
         * script's `A_PODCIAG(1)=AC_EDGE,K_UP` asks. A pair
         * with NO authored rectangle keeps the 1x1 default:
         * the engine's n there comes from the unwritten
         * [obj+0xb1] fields - neither the ctor 0x531f0 nor
         * its base 0x52860, as read, stores +0xb1..0xbd
         * before the scanf - UNMEASURED, so the port models
         * nothing there. */
        int half = (o->arx1 - o->arx0 + 1) / 2;
        edge.x0 = o->apx - half;
        edge.x1 = o->apx + half;
        edge.y0 = o->apy - 0x50;
        edge.y1 = o->apy;
    } else {
        edge.x0 = edge.x1 = o->x;      /* MODEL CHOICE, see the note:
                                        * no authored pair, no band -
                                        * a 1x1 at the object, the
                                        * old default, kept as the
                                        * counted fallback (a synthetic
                                        * object, or a placement that
                                        * authors no pair line) */
        edge.y0 = edge.y1 = o->y;
    }
    return ladder_answer_core(ladder_class(o), state, &edge, &rung, &p);
}

/* PR-241: the OTHER half of the same verb - a raise, not an answer.
 * 0x3f1dc-0x3f20c: when the ASKED ladder is at its FIRST declared
 * state ([obj+0x5c] == 1, the branch the port has until now treated
 * as inert) and the ask passes the 0x44ec0 ANY-test - the ladder's
 * [obj+0xa5] contact vector, each element's class verb +0x8 against
 * the requester's [[kind]+0x4c] object (PR-240) - the verb writes
 * `[asked+0x66] |= 0x40`: bit 54 of the asked's own live set, the
 * signal the engine's name block calls K_GEN1. It answers NOTHING
 * (0x3f20f jumps to the empty tail 0x3f364). The bit is then the
 * feed of that state's own authored exit - ZARIA.01's `[A_UP]` line
 * `A_DROP(1)=K_GEN1` - because the next record step's key
 * composition OR-s the live set into the transition key (0x53725 ->
 * 0x53520 -> 0x52000, PR-239). MODEL CHOICES, counted: the port has
 * no [obj+0xa5] vector and no [obj+0x30] parent field, and its ask
 * site IS a pair contact (PR-150's counted model of the engine's
 * candidate walk), so the pair contact stands for the ANY-test here
 * exactly as it does for the answer branch, and the requester's
 * parent gate is not modelled at all. */
static void ladder_flag_raise(lvl_obj *o)
{
    const ll_mov_st *s;

    if (!o->sc || !o->sc->mov || o->sti < 0 ||
        o->sti >= o->sc->mov->nst)
        return;                       /* the engine's not-declared 0 */
    s = &o->sc->mov->st[o->sti];
    if (s->decl_idx != 1)
        return;                       /* 0x3f1e2 / the 0x3f214 tree */
    o->sig |= 1ull << LL_SIG_K_GEN1;  /* [asked+0x66] |= 0x40       */
}

/* PR-241: and the consumption half. The engine leaves that bit in
 * [obj+0x60] and the LADDER's own next record step feeds it to the
 * transition match (0x53725 -> 0x53520 -> 0x52000, PR-239) - the
 * level pump calls every object's slot +0x14 once per frame. The
 * port's probe runs no per-object record machine, so the raise is
 * applied the way the port already applies its other object-side
 * raises: the balloon's K_GEN1 pop and the AC_ENEMY touch raise
 * (obj_touch_enemy, PR-92) both arm, test the object's own lines at
 * that beat, and clear. The one-record-step deferral is NOT modelled
 * here and is counted as a model choice; the selftest leg (L7) pins
 * the deferred shape through obj_advance, which does model it. */
/* PR-243: the asker's live rectangle, the thing the engine's element
 * verb 0x23e00 tests its points against. The engine keeps it at
 * [obj+0x75..0x84] - every kind table answers the +0x4c getter with
 * 0x23a20 = `add eax,0x75` - and refreshes it through the +0x98 verb
 * 0x23a50 -> 0x534e0: the position through the +0x68 getter plus the
 * current state's extents through its +0x48/+0x50 verbs. The port's
 * equivalent of those extents is the frame's SEQ shape bbox, the same
 * box its balloon hit test uses (balloon_hit, PR-88), with the same
 * x..x+w / y..y+h inclusive convention. MODEL CHOICE, counted: the drawn
 * mirror is not applied to that box (a mirror shifts a shape box
 * sideways, and every shipped contact point sits on its own object's
 * column), and the port's box comes from the art, not from the
 * `x0,y0;x1,y1` rectangle the same placement line carries (that rect is
 * an open item, OI-078).
 * PR-255 READS THAT CHAIN TO THE INSTRUCTION and it says two things.
 * (1) THE EXTENTS ARE THE CURRENT FRAME'S, not the state's: the +0x48
 * verb 0x23aa0 walks [obj+0x5c] (the state index) into the state
 * instance at [obj+0x50], takes the frame vector at [inst+0x2e] and
 * indexes it by that instance's own cursor [inst+0x4]; 0x534e0 then
 * calls the function pointer at [frame+0x33], and every frame record
 * carries the SAME pointer there - the only writer of that field in the
 * image is 0x41f4d `mov DWORD PTR [ebx+0x33],0xa0c08`, and 0xa0c08 is
 * the frame records' shared method block (0x42930, 0x42980, 0x429d0,
 * 0x42a20, 0x421f0, ...). Its first entry, 0x42930, builds the
 * rectangle from [frame+0x0c/0x0e/0x10/0x12] - the same four values
 * FUN_000420b0 computes from the parsed art, which is exactly the box
 * this port already models (ll_seq.h names that field run: "the game
 * computes at load (obj+0x0c..0x12 bbox, obj+0x14 = w0&8)"). So the
 * box model stands and PR-254's "the CURRENT STATE's extents" lead is
 * withdrawn here. (2) THE RECTANGLE IS CORNER-BASED AND MIRRORED:
 * 0x42930 takes a sign from the +0x50 verb 0x53e40 - the frame
 * record's [0x14], which ll_seq.h records as `w0 & 8`, the authored
 * mirror bit, negated only when the state instance's [+0x12] bit 3 is
 * set and [obj+0x6d] == -1 - and then
 *   sign > 0:   x0 = x + m0c, x1 = x0 + m10
 *   sign <= 0:  x1 = x - m0c, x0 = x1 - m10
 * with y never mirrored: y0 = y + m0e, y1 = y0 + m12. The port ships
 * that branch below (live_box, leg L10) with the sign it already uses
 * for the lion's drawn shape in this same pass - PR-82's model of that
 * bit, which the pinned PPM legs confirm. The state-instance bit 3 and
 * the [obj+0x6d] == -1 condition are NOT modelled: [obj+0x6d] is the
 * same signed word the integrator scales the X legs by (PR-252), and
 * the port's stand-in for it is the facing (PR-253), counted there. */
static void live_box(int x, int y, int bx0, int by0, int bw0, int bh0,
                     int sign, int ab[4])
{
    if (sign > 0) {
        ab[0] = x + bx0;
        ab[2] = ab[0] + bw0;
    } else {                    /* 0x42930's mirrored branch */
        ab[2] = x - bx0;
        ab[0] = ab[2] - bw0;
    }
    ab[1] = y + by0;
    ab[3] = ab[1] + bh0;
}

static void player_shape_box(play_ctx *pl, const ll_seq *lseq,
                             const ll_seq_elem *le, int mirror, int ab[4])
{
    int bx0, by0, bw0, bh0;
    int lx = (int)(pl->p.x >= 0 ? pl->p.x + 0.5 : pl->p.x - 0.5);
    int ly = (int)(pl->p.y >= 0 ? pl->p.y + 0.5 : pl->p.y - 0.5);

    ll_seq_shape_bbox(lseq, le, &bx0, &by0, &bw0, &bh0);
    live_box(lx, ly, bx0, by0, bw0, bh0, mirror ? -1 : 1, ab);
}

/* PR-254: the ask-channel probe switch (LEO_PLAY_ASK=1), read once. */
static int ask_probe(void)
{
    static int cached = -1;

    if (cached < 0)
        cached = getenv("LEO_PLAY_ASK") ? 1 : 0;
    return cached;
}

static void ladder_ask_raise(lvl_obj *o, const int ab[4], int verbose,
                             int tick)
{
    int k, hit = 0;

    if (o->sig & (1ull << LL_SIG_K_GEN1))
        return;                       /* the port's set is one beat   */
    /* PR-243: the gate the engine puts IN FRONT of that raise. 0x44ec0
     * walks the asked object's authored contact points (PR-242: the
     * placement's `N dx,dy` list, made absolute at load by 0x44e90) and
     * calls the element verb 0x23e00, a point-in-rectangle test against
     * the asker's live rectangle. An empty vector answers FALSE
     * (0x44ec0 returns 0 unless the count at [hdr+4] is > 0), so a
     * ladder or rope that authors no points never raises this signal:
     * PR-241's unconditional raise was too broad, and the L8 leg below
     * pins the difference. */
    /* PR-247/PR-248: the walk's first gate (0x4bd81) is the
     * candidate's class verb +0x60, and for this family that is
     * the SAME getter 156 of the 186 classes use - 0x23b80 =
     * `mov eax,[obj+0x2c]`, "a contact node was planted in me"
     * - so the port's ask site, which IS a shape meet, stands
     * for it. The authored rectangle belongs to a DIFFERENT
     * verb: class slot +0x64 (0x23140), called by the broad
     * phase 0x4b970 against a LAYER'S ACTIVE WINDOW, and its
     * answer is one of the things that write that contact node.
     * It does not gate the ask itself, so the rectangle test
     * PR-246 put here is removed; the rectangle stays parsed on
     * the record (OI-078) for the culling model, which waits on
     * the layer transform. */
    for (k = 0; k < o->npts; k++)
        if (ab[0] <= o->cpx[k] && o->cpx[k] <= ab[2] &&
            ab[1] <= o->cpy[k] && o->cpy[k] <= ab[3]) {
            hit = 1;
            break;
        }
    /* PR-254: the ask-channel probe, opt-in (LEO_PLAY_ASK=1). This is the
     * only place where "the pair was asked" and "the raise happened" are
     * different facts: the engine's walk 0x4b500 asks, and 0x44ec0's point
     * test is what decides. Before this PR the question never got asked
     * (the ask sites were shape-meet tests, PR-249), so the CI leg that
     * pins the ladder chain needs to see the ask itself, not only its
     * answer. stderr, so a --play log stays what it was. */
    if (ask_probe())
        fprintf(stderr, "ll_level: play tick %d: asked ladder %s/%s at"
                " state %d: %s %d,%d tested against the asker's live box"
                " %d,%d..%d,%d -> %s (0x4b500 -> %s -> 0x44ec0)\n",
                tick, o->sc->leaf, o->sc->movname,
                o->sc->mov->st[o->sti].decl_idx,
                o->npts ? "its authored contact point" : "NO authored point",
                o->npts ? o->cpx[0] : 0, o->npts ? o->cpy[0] : 0,
                ab[0], ab[1], ab[2], ab[3], hit ? "PASS" : "no",
                ladder_class(o) == 2 ? "0x3f390" : "0x3f1c0");
    if (!hit)
        return;                       /* 0x3f1fb: je 0x3f364, the tail */
    /* PR-257: the 0x3f1c0 branch gate, read at last. The answer verb
     * branches on the ASKED object's own current table index, [obj+0x5c]
     * - the same index 0x54250's physics pass and 0x53672's mirror arm
     * read ([obj+0x50] is the .mov class table, [obj+0x5c] the record
     * index; the table is 1-based, decl_idx 0 is the engine's not-found
     * value). Index 1 = the flag branch modelled here; index 2 = the AC
     * answer packet (ladder_answer_for, PR-150, gated there too); any
     * other index - a ladder mid-unroll in A_DROP, e.g. - takes the
     * verb's tail: `param_2[3] = 0`, no answer at all. The ask itself
     * ran either way, which is why the probe line above prints first. */
    if (o->sc->mov->st[o->sti].decl_idx != 1)
        return;
    ladder_flag_raise(o);
    if (!(o->sig & (1ull << LL_SIG_K_GEN1)))
        return;
    if (verbose)
        printf("ll_level: play tick %d: asked ladder %s/%s at state %d"
               " raises K_GEN1 on itself (%s)\n", tick, o->sc->leaf,
               o->sc->movname, o->sc->mov->st[o->sti].decl_idx,
               ladder_class(o) == 2 ? "0x3f3d3-0x3f3dc" : "0x3f203");
    if (obj_transition(o, o->sc->mov, 0) && verbose)
        printf("ll_level: play tick %d: %s/%s takes its own K_GEN1"
               " line: state %s\n", tick, o->sc->leaf, o->sc->movname,
               o->sc->mov->st[o->sti].name);
    o->sig = 0;
}

/* PR-257: the K_GENn family is the LEVEL's bus, not a private wire.
 * The shipped .MOV texts use those names across objects: ZARIA.01's
 * DRAB.MOV and LINA.MOV name `A_DROP(1)=K_GEN1` in A_UP with the row
 * `1,0,WAIT K_GEN1`, and NOTHING in either file raises K_GEN1 - the
 * only K_GEN1 raisers in that level's words are the lion's own rows,
 * A_JUMP rows 4..7 (`4,150,GEN K_GEN1` ...), the A_DRAP idle loop's
 * rows 8/9, A_FALL_DOWN's `s idle 1,300,GEN K_GEN1`, A_DEAD, the
 * fight states. A row's `GEN` admit therefore lands the bit on every
 * scripted object's vector, and an object whose current state names
 * that signal takes the transition on the spot (0x5360f's match arm,
 * the port's obj_transition). One beat, cleared after the attempt -
 * the persistent WAIT-row meter is PR-217's, still open. The AC_*
 * names are NOT broadcast: their raisers are modelled per pair
 * (PR-92/94/235), and a row's `GEN AC_TIME`/`GEN AC_BREAK_NEXT`
 * stays player-local exactly as PR-83/88 had it. */
static void gen_bus_raise(play_ctx *pl, lvl_obj *objs, int nobj,
                          int verbose, int tick)
{
    unsigned long long kb = pl->p.gsig_last &
        ((1ull << (LL_SIG_K_GEN5 + 1)) - (1ull << LL_SIG_K_GEN1));

    pl->p.gsig_last = 0;
    if (!kb)
        return;
    for (int k = 0; k < nobj; k++) {
        lvl_obj *o = &objs[k];

        if (!o->sc || !o->sc->mov || o->sti < 0 ||
            o->sti >= o->sc->mov->nst)
            continue;
        o->sig |= kb;
        if (obj_transition(o, o->sc->mov, 0) && verbose)
            printf("ll_level: play tick %d: %s/%s takes its own K_GEN1"
                   " line: state %s\n", tick, o->sc->leaf,
                   o->sc->movname, o->sc->mov->st[o->sti].name);
        o->sig = 0;                   /* one beat, as above */
    }
}

/* PR-329: the rectangle-class ANSWER - the +0x78 entry of the eight class
 * tables whose record verb 0x40c90 read the box line (PR-327), called by
 * the requester's ask walk 0x4b500 (0x4b5be: `call DWORD PTR [edi+0x78]`
 * with eax = the candidate, ebx = the requester, edx = the answer record),
 * merged into the requester's live set at 0x4b5c5 (0x49580). The answer
 * record's ors are the signal bits themselves: 0x40d23/0x40d99/0x40dff write
 * BYTE [ans+0x2] |= 0x2 / 0x8 / 0x10 and 0x1e422-0x1e42b writes BYTE
 * [ans+0x4] |= 0x1 - bits 17, 19, 20 and 32, exactly the LL_SIG_* indexes
 * of AC_RECT, AC_DRECT, AC_DRECT2 and AC_KILOF (the signal table of
 * PR-69 confirms the numbering; this is also why PR-327's register-form
 * raise at 0x1e42b is AC_KILOF). Read verb by verb:
 *   RECT 0x40cf0: the only test is the requester's PRESENTED point (class
 *     verb +0x68, for the lion 0x23900 = `add eax,0xd6`, the position block
 *     mirrored at [obj+0xd6] by the +0xb0 provider 0x544f0, which starts it
 *     as a copy of [obj+0]/[obj+4]) against the box at [rect+0x3c]: no
 *     probe, no fall test. Raises AC_RECT.
 *   DRECT2 0x40dc0: the asker's askable probe first (class verb +0x3c;
 *     the lion class 0x97a98 answers 0x230a0 = `mov eax,1`, so for this
 *     asker it is always open - the ZARIA.01 lion IS askable, unlike the
 *     base pair of PR-327) and then the same point test. Raises AC_DRECT2.
 *   DRECT 0x40d50: that probe, then [asker+0xc6] > 0 - the velocity
 *     register of PR-99/PR-119 (OI-119), positive = falling - then the
 *     same point test. Raises AC_DRECT.
 *   KILOF_PL 0x1e3a0: the probe, then the facing verb (class +0x70, for
 *     the lion 0x239f0 = `mov eax,[obj+0x6d]`) must be POSITIVE, facing
 *     right, then the same point test against a copy of the box 0x4a050
 *     builds into a temporary object of class 0x98274 - the same numbers.
 *     Raises AC_KILOF.
 *   BICZRECT 0x286a0: the probe, the facing verb == -1, and the ANCHOR
 *     point (class +0x6c, for the lion 0x23a10 = `add eax,0x89`). Raises
 *     the AC_DRECT bit (0x286ec), the same bit the DRECT verb raises.
 *   BRECT 0x28710: the probe; then if [asker+0x5c], the current state
 *     index, sits in 9..13 the anchor point is tested, otherwise the
 *     presented one - both are the position block in this port (PR-321:
 *     no separate anchor/presented pair), so the two arms coincide.
 *     Raises AC_RECT (0x2772).
 * WSPRECT 0x287a0 and KRA1 0x37ea0 answer differently - the first an
 * anchor test PLUS a state lookup through the box's own [rect+0x30] object
 * id (0x55760), raising bit 12 (AC_DRAB_CATCH - not AC_DRAB, correcting
 * PR-327's naming), the second two signals not read at all - and no
 * shipped level places either of them as a box line, so they raise
 * nothing here; named again in OPENS.
 * The point test itself is the point class's +0x8 verb called at
 * 0x40d7d/0x40de3/0x1e405 on the box at [rect+0x3c] - the same
 * point-in-rectangle shape as 0x23e00 (PR-243: inclusive edges), and with
 * the authored edges NOT sorted, so a box written backwards matches
 * nothing (PR-328). */
static int rect_class_id(const char *tok);          /* the PR-328 table */

static unsigned long long rect_class_answer(const lvl_obj *o,
                                           const ll_player *p)
{
    int id = rect_class_id(o->sc->leaf);
    if (id < 0 || !o->nrect || id >= 6)     /* WSPRECT and KRA1: unread */
        return 0;
    int px = (int)(p->x >= 0 ? p->x + 0.5 : p->x - 0.5);
    int py = (int)(p->y >= 0 ? p->y + 0.5 : p->y - 0.5);
    if (px < o->arx0 || px > o->arx1 || py < o->ary0 || py > o->ary1)
        return 0;
    int face = p->face >= 0 ? 1 : -1;       /* the live_bits convention */
    switch (id) {
        case 0: return 1ull << LL_SIG_AC_RECT;      /* RECT: geometry only */
        case 1: return p->vy > 0.0                  /* DRECT: + the fall   */
                          ? 1ull << LL_SIG_AC_DRECT : 0ull;
        case 2: return 1ull << LL_SIG_AC_DRECT2;
        case 3: return face > 0                     /* KILOF_PL: + facing  */
                          ? 1ull << LL_SIG_AC_KILOF : 0ull;
        case 4: return face < 0                     /* BICZRECT: facing -1,*/
                          ? 1ull << LL_SIG_AC_DRECT : 0ull;  /* anchor    */
        default: return 1ull << LL_SIG_AC_RECT;     /* BRECT: same bit    */
    }
}

/* The walk's own gates, run exactly as 0x4b500 runs them for this asker:
 * the channel bit of the REQUESTER's current state picks the list the
 * candidate sits on (0x523f0 asks per step from the state's control words,
 * PR-245; the port's src index of des_place_keys = the same lists, the
 * model PR-208 read at FUN_00052780 and play_collide already applies), and
 * 0x4b5a1 skips a candidate of another parallax plane unless the state
 * carries CHECK_ALL_PLANE. What the port does NOT run is the walk's first
 * filter, the candidate's +0x60 verb: for these classes that is the
 * generic 0x23b80 getter of [obj+0x2c], and the contact node for a box
 * object is written by the broad phase 0x4b970 through class +0x64
 * (0x23b70 -> the base +0x8 verb 0x237f0: the authored box tested against
 * the LAYER'S ACTIVE WINDOW, 0x4b940 expanding the layer origin by
 * -0xa0,-0x78,+0x1e0,+0x140). That window needs the layer transform, which
 * this port has none of - so every placed box is asked, and an OPENS keeps
 * the culling model. It cannot change the answer of any leg below: a box
 * outside the active window cannot contain the player, whom the camera
 * keeps on it. */
static void rect_class_asks(play_ctx *pl, const lvl_obj *objs, int nobj,
                            int verbose, int tick)
{
    if (!pl->have || !pl->mov || pl->p.sti < 0 ||
        pl->p.sti >= pl->mov->nst)
        return;
    const ll_mov_st *ls = &pl->mov->st[pl->p.sti];
    for (int k = 0; k < nobj; k++) {
        const lvl_obj *o = &objs[k];
        if (!o->sc || !o->sc->leaf[0] || !o->nrect ||
            rect_class_id(o->sc->leaf) < 0)
            continue;
        if (o->src == 0 ? !(ls->ctrl & LL_CTRL_CHECK_ZW)
                        : o->src == 1 ? !(ls->ctrl & LL_CTRL_CHECK_ANI) : 1)
            continue;
        if (o->layer != pl->layer &&
            !(ls->ctrl & LL_CTRL_CHECK_ALL_PLANE))
            continue;
        unsigned long long ans = rect_class_answer(o, &pl->p);
        if (ans) {
            pl->p.sig |= ans;
            if (verbose)
                printf("ll_level: play tick %d: %s box %d,%d;%d,%d answered"
                       " the player at (%d,%d): 0x%llx\n",
                       tick, o->sc->leaf, o->arx0, o->ary0, o->arx1,
                       o->ary1, (int)(pl->p.x + 0.5),
                       (int)(pl->p.y + 0.5), ans);
        } else if (ask_probe()) {
            fprintf(stderr, "ll_level: play tick %d: asked rectangle class"
                    " %s box %d,%d;%d,%d: the player at (%d,%d) vy=%g face"
                    "=%d did not answer it (0x4b500 -> [class+0x78])\n",
                    tick, o->sc->leaf, o->arx0, o->ary0, o->arx1, o->ary1,
                    (int)(pl->p.x + 0.5), (int)(pl->p.y + 0.5),
                    pl->p.vy, (int)pl->p.face);
        }
    }
}

static void play_collide(play_ctx *pl, const lvl_obj *objs, int nobj,
                         int verbose, int tick)
{

    pl->p.support = 0;
    if (pl->lock > 0)
        pl->lock--;
    if (!pl->solid || !pl->have || pl->p.sti >= pl->mov->nst)
        return;
    const ll_mov_st *ls = &pl->mov->st[pl->p.sti];
    if (ls->ctrl & LL_CTRL_ANTI_SOLID)
        return;
    const ll_seq *lseq = play_art(pl, ls->seqkey[0] ? ls->seqkey
                                                    : pl->mov->seqkey);
    if (!lseq || !lseq->count)
        return;
    ll_frame_ref lr = ll_mov_frame_ref(ll_player_frame(&pl->p));
    if (lr.blank || lr.idx >= (int)lseq->count)
        return;
    const ll_seq_elem *le = &lseq->els[lr.idx];
    {   /* balloons: not solids, their own touch method */
        int bx0, by0, bw0, bh0;
        int lx0 = (int)(pl->p.x >= 0 ? pl->p.x + 0.5 : pl->p.x - 0.5);
        int ly0 = (int)(pl->p.y >= 0 ? pl->p.y + 0.5 : pl->p.y - 0.5);

        ll_seq_shape_bbox(lseq, le, &bx0, &by0, &bw0, &bh0);
        for (int k = 0; k < nobj; k++) {
            lvl_obj *o = (lvl_obj *)&objs[k];
            if (!o->sc || !ci_eq(o->sc->leaf, "BAL") || !o->sc->mov ||
                o->sti != o->sc->mov->start_st || o->layer != pl->layer)
                continue;
            if (!balloon_hit(o->x, o->y, lx0 + bx0, ly0 + by0, bw0, bh0))
                continue;
            if (pl->energy >= 100)
                continue;                       /* the message is ignored */
            pl->energy += 32;
            if (pl->energy > 100)
                pl->energy = 100;
            o->sig |= 1ull << LL_SIG_K_GEN1;        /* K_GEN1: the pop  */
            (void)obj_transition(o, o->sc->mov, 0);
            o->sig = 0;
            if (verbose)
                printf("ll_level: play tick %d: balloon at %d,%d picked:"
                       " energy %d\n", tick, o->x, o->y, pl->energy);
        }
    }
    /* PR-82: the lion's facing (ll_player_step keeps it from the CH_DIR
     * states, same flag and same model as obj_walk) decides which way his
     * collision shape and his drawn sprite point, mirrored on top of the
     * authored frame sign. The collision shape uses the DRAWN mirror,
     * exactly like obj_shape_elem does for scene objects (PR-81 rule:
     * the shape keeps the drawn polarity). */
    int lm = ll_player_mirror_of(&pl->p, lr.mirror);
    for (int k = 0; k < nobj; k++) {
        lvl_obj *o = (lvl_obj *)&objs[k];
        if (!o->sc || !o->sc->mov || o->sti < 0 || o->sti >= o->sc->mov->nst)
            continue;
        unsigned c = o->sc->mov->st[o->sti].ctrl;
        if (!(c & LL_CTRL_SOLID))
            continue;
        /* FUN_00052780: the mover's own CHECK_ZW / CHECK_ANI pick the list
         * it is tested against (ani_plat objects are reached through
         * another path, not ported) */
        if (o->src == 0 ? !(ls->ctrl & LL_CTRL_CHECK_ZW)
                        : o->src == 1 ? !(ls->ctrl & LL_CTRL_CHECK_ANI) : 1)
            continue;
        /* the plane gate (FUN_0004b500): obj.plane == mover.plane, plane =
         * the parallax layer [+0x24] (the plane plany= names for the cell
         * that placed it, PR-208); CHECK_ALL_PLANE lifts it */
        if (o->layer != pl->layer && !(ls->ctrl & LL_CTRL_CHECK_ALL_PLANE))
            continue;
        int om;
        const ll_seq *oseq = NULL;
        const ll_seq_elem *oe = obj_shape_elem(o, &om, &oseq);
        if (!oe)
            continue;
        int lx = (int)(pl->p.x >= 0 ? pl->p.x + 0.5 : pl->p.x - 0.5);
        int ly = (int)(pl->p.y >= 0 ? pl->p.y + 0.5 : pl->p.y - 0.5);
        int d1, d2, r;
        r = ll_seq_shape_push2(oseq, oe, o->x, o->y, om ? -1 : 1,
                               lseq, le, lx, ly, lm ? -1 : 1, &d1, &d2);
        if (d1 == 0 && d2 == 0)
            continue;
        int dx = (c & LL_CTRL_SOLID2) ? d1 : r;
        int dy = (c & LL_CTRL_SOLID2) ? d2 : 0;
        if (c & LL_CTRL_SEMI_SOLID) {
            /* 0x53b47..0x53ba9: SEMI_SOLID answers with the horizontal
             * penetration r only: |r| > 5 moves B by r/5 (kind 1), and a
             * kind-13 message adds 30*clamp(r, -4..4) to B's velocity x
             * (FUN_00054560: [+0xc2] += a). The velocity's unit scale is
             * not pinned (integrated at 0x5427c as dt*v/100), so only the
             * position part is applied and the impulse is counted. */
            static int imp;
            dx = (r > 5 || r < -5) ? r / 5 : 0;
            dy = 0;
            if (!imp++)
                printf("ll_level: play: SEMI_SOLID velocity impulse"
                       " (kind 13) counted, not applied (unit scale"
                       " unresolved)\n");
        }
        pl->p.x += dx;
        pl->p.y += dy;
        if (dy < 0 && !(c & LL_CTRL_SEMI_SOLID)) {
            pl->p.support = 1;
            pl->p.vy = 0;
            if (!pl->p.grounded) {
                pl->p.grounded = 1;
                pl->p.sig |= 1ull << LL_SIG_K_GEN2;
            }
        }
        if (pl->p.atc && !pl->p.atc_hit && o->hp >= 0 && !o->dead) {
            /* PR-89: the lion's attack window is open and the shapes
             * just met (same contact as the push-out above; the engine's
             * touch pipeline runs the push-out and then the hit effects
             * - 0x552e0 -> 0x55230). One hit per opened window.
             * PR-139 read the receiver too and one of its gates is a
             * no-op HERE: 0x50f04..0x50f1a drop the hit when the
             * RECEIVER's window is open and the TOUCHER's is not
             * (`o->atc && !pl->p.atc`), and pl->p.atc is this block's
             * own precondition. That gate does bite on the answer
             * entries the port does not model (0x55245's 0x80000-row
             * damage-0 hit, the [receiver+0x11a]==6/7 branch) - counted
             * as an open item. The port's obj_hit keeps the 0x50ed0
             * law: hp < 0 is not hit-able, hit_cd > 0 is the countdown
             * drop at 0x50f59, and a landed hit arms 0x1e at
             * 0x50f67/0x50300. NAMED APPROXIMATION, PR-139: that chain
             * is verified for the answer-A family (the 15 tables that
             * hold 0x50f80, the lion class 0x96960 among them - the
             * lion's own +0xe4 IS 0x50300, so the 30 the port keeps on
             * pl->lock is exact), but an animal's receiver is a
             * different method: 0x97bc0 (SEP) installs 0x1ecd0 at +0xd0
             * and 0x4ffb0 at +0xe4, the ZWIERZ base 0x97f64 installs
             * 0x41c30 (which ends in 0x41180 and a float from
             * [obj+0x107]) and 0x41460, and neither writes 0x1e. The
             * object-side hp/countdown law for animals is therefore a
             * stand-in; it is counted as an open item, not hidden. */
            int hh = obj_hit(o, LL_LION_HIT_DAMAGE);
            if (hh) {
                pl->p.atc_hit = 1;
                if (verbose)
                    printf("ll_level: play tick %d: lion %s %s/%s:"
                           " damage %d -> hp %d%s\n", tick,
                           hh == 2 ? "KILLS" : "hits", o->sc->leaf,
                           o->sc->movname, LL_LION_HIT_DAMAGE, o->hp,
                           hh == 2 ? " (dead)" : "");
            }
        }
        if (o->atc && !o->atc_hit && o->damage > 0 && pl->lock == 0 &&
            !clash_suppressed(pl, lx, o->x)) {
            /* PR-90 + PR-139: this is the ONLY path an authored
             * damage= reaches the LION. Engine chain, verbatim, for
             * the family ZARIA's animals belong to: the authored 'SEP'
             * token builds its object at 0x62e59 with the class table
             * 0x97bc0, whose answer entry [obj.vt+0x78] is 0x552e0;
             * that needs a real shape meet (0x53ae0) plus the
             * [obj.vt+0x9c] filter and then calls 0x55230, whose body
             * IS the law: a damage-0 hit when the current row's flag
             * word (0x55242 = [obj.vt+0x44] = 0x23920, which reads
             * [[[obj+0x50]+[obj+0x5c]*4]+0x12] - the row record
             * itself) carries 0x80000 (0x55245..0x55257, and the
             * receiver drops a 0-damage hit), answer-C 0x55130 ONLY
             * while the object's window bit is set (0x5525d..0x55267,
             * [obj.vt+0x40] = 0x23860 = bit 5 of [obj+0xb6] - set
             * ONLY by the authored atc_on row directive: the parser
             * 0x4636e matches the literal 0x918f2 and 0x46398 writes
             * the action record's vtable 0xa10f0 with its +8 flag,
             * and the apply method at slot +4, 0x45c20, is exactly
             * `and [obj+0xb6],~0x20` + `or [obj+0xb6],(flag&1)<<5`),
             * and else the LEVEL's global danger_power as the damage
             * when that row word carries 0x2000 (0x55283..0x552a2).
             * Answer-C saves the receiver's [obj+0xad] at 0x55140,
             * asks 0x550c0 (see clash_suppressed) and on "no
             * suppression" calls the receiver's hit method with the
             * toucher's [toucher+0xb1] as the damage at
             * 0x5514f..0x55164. For the lion that hit lands at
             * 0x54550 (`sub [eax+0xad],ebx` - the energy counter)
             * behind ITS OWN receiver: the factory 0x1fef2 matches the
             * class-name literal "TLewZaria" (0x9078b) and writes the
             * lion's class table 0x96960 at [obj+8], whose +0xd0 IS
             * 0x50ed0 and whose +0xe4 is 0x50300; 0x50ed0's gates are
             * damage==0 -> return (0x50ef1), the pairing rule
             * (0x50f04..0x50f1a), the countdown drop (0x50f59) - then
             * 0x50f67 arms the next 0x1e = 30 steps; the port's
             * pl->lock is that same countdown, PR-89. The other answer
             * family, answer-A 0x50f80, is installed by the 15 tables
             * that hold that dword (the lion class 0x96960 among them, NOT
             * the ZWIERZ-derived animals); its window gate 0x50ff1 and
             * its DANGER branch 0x51011..0x51033 are the same two
             * laws, so the port models ONE window rule. (Corrects
             * PR-94/96's reading of [obj.vt+0x40] as an is-enemy flag
             * and any earlier wording that every answering family
             * installs 0x50f80.) One landed hit per armed episode: the
             * engine's repeat-drop is the lion's own 30-countdown, and
             * the atc_hit flag below is consumed ONLY when the hit
             * lands - a window cancelled by 0x550c0 stays armed and
             * can land after the lion's own window closes. The window
             * still has to be ARMED: LICHA's A_RUN rows carry no
             * atc_on; only A_ATAK's do (opened by its row -4, closed
             * by -8), and the port arms A_ATAK only through the PR-90
             * DANGER stand-in (LEO_PLAY_DANGER), never by default. */
            pl->energy -= o->damage;
            pl->lock = 30;
            o->atc_hit = 1;
            /* PR-91: and the landed hit raises AC_ENEMY_ATC on the
             * LION's own set - 0x5516a compares the saved hp and
             * picks the ATC variant exactly when it dropped (0x5518c
             * `or bl,0x1`). The reaction is then pure authored data:
             * the .MOV's "A_DOSTAL(1)=AC_ENEMY_ATC" lines, the
             * seq=hit art and the frame-1 pullback impulse all landed
             * with PR-74/PR-86/PR-88; nothing here is new mechanism. */
            pl->p.sig |= 1ull << LL_SIG_AC_ENEMY_ATC;
            if (verbose)
                printf("ll_level: play tick %d: %s/%s ATTACKS through"
                       " its window: damage %d -> lion energy %d\n",
                       tick, o->sc->leaf, o->sc->movname, o->damage,
                       pl->energy);
        }
        /* PR-139 (G-5): the unconditional contact-damage block that
         * used to stand here is DELETED. The engine has no rule that a
         * touched enemy hurts the lion: an animal's answer 0x55230 has
         * exactly two damage paths besides the window - the row-flag
         * 0x80000 hit, which it calls with ebx = 0 (0x55255), and the
         * DANGER branch at 0x55283..0x552a2, which delivers the
         * LEVEL's global danger_power ([ds:0xcef40+0x54], read at
         * 0x55290/0x5529f) as the damage; answer-A 0x50f80 has the
         * same pair at 0x51011..0x51033. No shipped level authors
         * danger_power above 0 (PR-90), and the lion's receiver returns
         * the moment the damage is 0 (0x50ed0: test ebx,ebx at
         * 0x50eef). A bump against a resting animal therefore answers
         * with the push-out and the AC_ENEMY raise only. (Corrects
         * PR-94/96's reading of [obj.vt+0x40] as an "is-enemy"
         * predicate: it is the window bit.) */
        /* PR-92: the same touch answers with the AC_ENEMY raise too (the
         * engine's 0x50ed0 tail builds its packet after the damage and
         * DANGER branches, and this loop's shapes have just met). */
        obj_touch_enemy(o, verbose, tick);
        if (ladder_class(o)) {
            /* PR-150: the pair has just MET and the object was MOVED
             * by the push - the engine answers on the mover's CURRENT
             * position block (0x3f222 re-reads the getter at answer
             * time), so the answer is taken from the post-push shape
             * and state (PR-92's order note: the engine answers after
             * the push-out). */
            unsigned long long ans =
                ladder_answer_branch(o) ? ladder_answer_for(o, &pl->p) : 0;

            if (ans) {
                pl->p.sig |= ans;
                if (verbose)
                    printf("ll_level: play tick %d: ladder %s/%s state"
                           " %d ANSWERS the lion: set %llx\n", tick,
                           o->sc->leaf, o->sc->movname,
                           o->sc->mov->st[o->sti].decl_idx, ans);
            }
            /* PR-235: the 0x287a0 half - a raise on the LION itself,
             * separate from the answer packet, keyed on the state
             * record IDs (authored block positions). */
            if (o->sti >= 0 && o->sti < o->sc->mov->nst &&
                pl->p.sti >= 0 && pl->p.sti < pl->mov->nst &&
                acdrab_law(pl->mov->st[pl->p.sti].block_pos,
                           o->sc->mov->st[o->sti].block_pos)) {
                pl->p.sig |= 1ull << LL_SIG_AC_DRAB;
                if (verbose)
                    printf("ll_level: play tick %d: ladder %s/%s"
                           " contact"
                           " raises AC_DRAB on the lion (state IDs"
                           " %d/%d)\n", tick,
                           o->sc->leaf, o->sc->movname,
                           pl->mov->st[pl->p.sti].block_pos,
                           o->sc->mov->st[o->sti].block_pos);
            }
            /* PR-241: the FLAG branch of the answer verb itself - at
             * its first declared state the ladder raises K_GEN1 on
             * ITSELF (0x3f203) and answers nothing; that bit is the
             * feed of its own authored `A_DROP(1)=K_GEN1` exit. */
            {   int ab[4];            /* PR-243: the asker's rectangle,
                     * per the gate above */

                player_shape_box(pl, lseq, le, lm, ab);
                ladder_ask_raise(o, ab, verbose, tick);
            }
        }
        if (verbose)
            printf("ll_level: play tick %d: solid %s/%s %s at %d,%d push"
                   " %d,%d (ret %d) lion %d,%d\n", tick, o->sc->leaf,
                   o->sc->movname, (c & LL_CTRL_SOLID2) ? "SOLID2"
                   : (c & LL_CTRL_SEMI_SOLID) ? "SEMI" : "SOLID",
                   o->x, o->y, dx, dy, r, lx, ly);
        lx = (int)pl->p.x;            /* later objects see the moved lion */
    }
    /* PR-92 (2/2): the touch raise for objects that are NOT solids. The
     * engine's touch handler is not the solid-push pipeline - 0x50ed0
     * runs for any pair that meets (the push-out above is only its
     * FUN_00053ae0 part) - and EVERY shipped AC_ENEMY consumer lives in
     * a state without a SOLID flag (GPTAK: BY_TIME|FRAME_REV|CHECK_LEW|
     * CH_DIR|DANGER, FOKA: BY_TIME|CHECK_LEW|CHECK_LN, RBLOK: BY_TIME|
     * FRAME_REV|CHECK_LN|CHECK_LEW|CHECK_ANI|CH_DIR|DANGER): a raise
     * only inside the solid loop would be data-dead. Same paired-shape
     * test as the loop, evaluated at the lion's POST-push position (the
     * engine answers after the push-out: 0x552e0 -> 0x55230), same
     * list/plane gates, but nobody is moved by it. Objects whose CURRENT
     * state IS solid were already raised inside the loop. */
    {
        int lx2 = (int)(pl->p.x >= 0 ? pl->p.x + 0.5 : pl->p.x - 0.5);
        int ly2 = (int)(pl->p.y >= 0 ? pl->p.y + 0.5 : pl->p.y - 0.5);
        for (int k = 0; k < nobj; k++) {
            lvl_obj *o = (lvl_obj *)&objs[k];
            if (!o->sc || !o->sc->mov || o->sti < 0 ||
                o->sti >= o->sc->mov->nst)
                continue;
            if (ci_eq(o->sc->leaf, "BAL"))      /* their own touch method */
                continue;
            unsigned c = o->sc->mov->st[o->sti].ctrl;
            if (c & LL_CTRL_SOLID)              /* raised inside the loop  */
                continue;
            if (o->src == 0 ? !(ls->ctrl & LL_CTRL_CHECK_ZW)
                            : o->src == 1 ? !(ls->ctrl & LL_CTRL_CHECK_ANI) : 1)
                continue;
            if (o->layer != pl->layer && !(ls->ctrl & LL_CTRL_CHECK_ALL_PLANE))
                continue;
            /* PR-254 SPLITS THIS TEST IN TWO, the way the engine does.
             * The paired-shape meet is NOT how a pair gets asked: the
             * requester's walk 0x4b500 goes from ITS CHECK_ZW/CHECK_ANI
             * bits (the gates above) through the instance arrays into the
             * candidate's answer entry +0x78 with NO shape test at all
             * (PR-249). The meet test 0x53ae0 the port used to stand in
             * for the ask lives INSIDE the animals' own answer entry
             * 0x552e0 - so it stays, but as what the animal does when it
             * is asked, never as the ask. That is why the ladder was
             * unreachable: no shipped ladder state is a solid, and its
             * frames do not meet the lion. */
            int om;
            const ll_seq *oseq = NULL;
            const ll_seq_elem *oe = obj_shape_elem(o, &om, &oseq);
            int d1 = 0, d2 = 0;
            if (oe)
                ll_seq_shape_push2(oseq, oe, o->x, o->y, om ? -1 : 1,
                                   lseq, le, lx2, ly2, lm ? -1 : 1,
                                   &d1, &d2);
            if (d1 != 0 || d2 != 0)             /* the animal's own test  */
                obj_touch_enemy(o, verbose, tick);
            if (ladder_class(o)) {
                /* PR-150: same answer path for a NON-solid ladder
                 * state - the engine's candidate walk is not gated on
                 * solidity (PR-147 Finding 4: the pair goes to the
                 * asked object's +0x78 entry whatever the state's
                 * SOLID flag), and every shipped AC_* consumer lives
                 * in a non-solid state (PR-92's census). Evaluated at
                 * the lion's post-push position, like the touch. */
                unsigned long long ans =
                ladder_answer_branch(o) ? ladder_answer_for(o, &pl->p) : 0;

                if (ans) {
                    pl->p.sig |= ans;
                    if (verbose)
                        printf("ll_level: play tick %d: ladder %s/%s"
                               " state %d ANSWERS the lion"
                               " (non-solid): set %llx\n", tick,
                               o->sc->leaf, o->sc->movname,
                               o->sc->mov->st[o->sti].decl_idx, ans);
                }
                if (pl->p.sti >= 0 && pl->p.sti < pl->mov->nst &&
                    acdrab_law(pl->mov->st[pl->p.sti].block_pos,
                               o->sc->mov->st[o->sti].block_pos)) {
                    pl->p.sig |= 1ull << LL_SIG_AC_DRAB;
                    if (verbose)
                        printf("ll_level: play tick %d: ladder %s/%s"
                               " contact raises AC_DRAB on the lion"
                               " (state"
                               " IDs %d/%d, non-solid)\n", tick,
                               o->sc->leaf, o->sc->movname,
                               pl->mov->st[pl->p.sti].block_pos,
                               o->sc->mov->st[o->sti].block_pos);
                }
                /* PR-241: the FLAG branch, same as the solid site: the
                 * asked ladder at its first declared state raises
                 * K_GEN1 on itself (0x3f203), the feed of its own
                 * `A_DROP(1)=K_GEN1` exit. */
                {   int ab[4];        /* PR-243: same gate, same box */

                    player_shape_box(pl, lseq, le, lm, ab);
                    ladder_ask_raise(o, ab, verbose, tick);
                }
            }
        }
    }
}

/* PR-150 selftest: the ladder's contact answer, hand-derived from the
 * machine code quoted in the ladder_answer_* model note (0x3f1c0, the
 * 0x23e00 bounds test, the 0x49fb0 probe) and from the state index
 * its gate reads - the synthetic scripts mirror that shape, no game
 * data, no video. L1 pins the probe law at the truncation edges its
 * bytes decide (odd widths, w<=3, mirrored words); L2 the decision
 * table with the inclusive bounds, the edge block's priority and the
 * state gate; L3 the class scope; L4 the gate end to end with the
 * no-art default, including the declared-list order vs the block
 * order and the not-declared 0; L5 the index the parser itself
 * computes. */
static int ladder_answer_selftest(void)
{
    static const struct {
        int n, x0, x1, wx0, wx1;
        const char *what;
    } probe_cases[] = {
        { 70, 0, 40, 6, 34, "w=40: ins 28, half 6 keeps the middle 70%" },
        { 70, 0, 41, 6, 35, "w=41: ins trunc(28.7)=28, half trunc(6.5)=6" },
        { 70, 0, 43, 6, 37, "w=43: ins trunc(30.1)=30" },
        { 70, 0, 10, 1, 9, "w=10: half trunc(1.5)=1, the odd px stays in" },
        { 70, 0, 3, 0, 3, "w=3: half 0, the probe IS the rung" },
        { 70, 0, 1, 0, 1, "w=1: unchanged" },
        { 70, 40, 0, 34, 6, "descending words mirror the law (34 above 6)" },
        { 70, -41, 0, -35, -6, "negative coordinates: the law on values" },
        { 50, 0, 8, 2, 6, "rope n=50 (0x3f448): w=8 ins 4 half 2" },
        { 50, 546, 554, 548, 552, "ZARIA.01's rope column 546..554:"
          " w=8 keeps the central 4 px" },
        { 50, 0, 9, 2, 7, "rope w=9: ins 4, t 5, half trunc(2.5)=2" },
        { 50, 0, 3, 1, 2, "rope w=3: ins 1, t 2, half 1 - the middle"
          " 2 px (unlike n=70, whose half is 0 here)" },
    };
    static const unsigned long long m_drab = 1ull << LL_SIG_AC_DRAB;
    static const unsigned long long m_cat = m_drab |
                                            (1ull << LL_SIG_AC_DRAB_CATCH);
    static const unsigned long long m_lina = 1ull << LL_SIG_AC_LINA;
    static const unsigned long long m_lcat = m_lina |
                                             (1ull << LL_SIG_AC_LINA_CATCH);
    static const struct {
        int kind, state; ll_pt ask; unsigned long long want;
        const char *what;
    } core_cases[] = {
        { 1, 0, { 120, 124 }, 0ull, "state 0 (a block whose name is not"
          " declared, 0x53471): no branch, nothing even inside the probe" },
        { 1, 0, { 100, 100 }, 0ull, "state 0 even ON the edge pixel" },
        { 1, 1, { 120, 124 }, 0ull, "state 1 (ZARIA.01's A_UP, the first"
          " declared state) is the child-ANY branch (0x3f1e4/0x44ec0):"
          " no child list modelled, so nothing answers" },
        { 1, 3, { 120, 124 }, 0ull, "state 3 (A_DROP): inert 0x3f217->0x3f364" },
        { 1, 9, { 100, 100 }, 0ull, "state 9 even on the edge pixel" },
        { 1, 2, { 100, 100 }, 1ull << LL_SIG_AC_EDGE, "state 2 (A_DOWN, the"
          " second declared state): P1 FIRST, on the stored edge block,"
          " AC_EDGE only" },
        { 1, 2, { 106, 100 }, m_cat, "probe near corner inclusive: CATCH+"
          "DRAB merged" },
        { 1, 2, { 120, 124 }, m_cat, "probe band middle: merged" },
        { 1, 2, { 136, 148 }, m_cat, "probe far corner inclusive (136,148):"
          " merged (no y inset)" },
        { 1, 2, { 101, 124 }, m_drab, "inside the rung, left of the band:"
          " DRAB alone (0x3f355)" },
        { 1, 2, { 142, 148 }, m_drab, "rung far corner inclusive (142,148),"
          " outside the band: alone" },
        { 1, 2, { 99, 124 }, 0ull, "one px left of the rung: P2 FALSE ->"
          " inert 0x3f364" },
        { 1, 2, { 100, 149 }, 0ull, "below the rung (y not inset): inert" },
        { 1, 2, { 143, 124 }, 0ull, "one px right of the rung: inert" },
        /* PR-528, the ROPE verb 0x3f390 on the same edge/rung pair.
         * The probe keeps the CENTRAL 50%: rung 100..142, w=42,
         * ins 21, t 21, half 10 -> probe 110..132. x=104 is inside
         * the rung but outside that probe: AC_LINA ALONE (0x3f525).
         * AC_EDGE is the SAME bit as the ladder's (0x3f41d). */
        { 2, 1, { 120, 124 }, 0ull, "rope state 1: the flag branch raises"
          " K_GEN1 (0x3f3d9), answers nothing" },
        { 2, 3, { 120, 124 }, 0ull, "rope state 3 (A_DROP): inert"
          " 0x3f3e7 -> 0x3f534" },
        { 2, 2, { 100, 100 }, 1ull << LL_SIG_AC_EDGE, "rope state 2:"
          " P1 FIRST, AC_EDGE, the same bit as the ladder's" },
        { 2, 2, { 110, 124 }, m_lcat, "rope probe near corner inclusive"
          " (110 = 100+half 10): CATCH+LINA merged (0x3f4ac/0x3f4d4)" },
        { 2, 2, { 120, 124 }, m_lcat, "rope probe middle: merged" },
        { 2, 2, { 132, 148 }, m_lcat, "rope probe far corner inclusive"
          " (132 = 142-10): merged" },
        { 2, 2, { 104, 124 }, m_lina, "inside the column, left of the"
          " 50% probe: AC_LINA ALONE (0x3f525)" },
        { 2, 2, { 142, 148 }, m_lina, "column far corner inclusive, outside"
          " the probe: LINA alone" },
        { 2, 2, { 99, 124 }, 0ull, "one px left of the column: inert"
          " 0x3f534" },
        { 2, 2, { 143, 124 }, 0ull, "one px right of the column: inert" },
    };
    const ll_blk rung = { 100, 100, 142, 148 };
    const ll_blk edge = { 100, 100, 100, 100 };
    int k;

    for (k = 0; k < (int)(sizeof probe_cases / sizeof probe_cases[0]);
         k++) {
        ll_blk b = { probe_cases[k].x0, 7, probe_cases[k].x1, 9 };

        blk_probe_pct(&b, probe_cases[k].n);
        if (b.x0 != probe_cases[k].wx0 || b.x1 != probe_cases[k].wx1 ||
            b.y0 != 7 || b.y1 != 9) {
            fprintf(stderr, "ladder answer selftest: FAILED probe law"
                            " %s (got %d..%d y %d,%d)\n",
                    probe_cases[k].what, b.x0, b.x1, b.y0, b.y1);
            return 1;
        }
    }
    for (k = 0; k < (int)(sizeof core_cases / sizeof core_cases[0]);
         k++) {
        unsigned long long got = ladder_answer_core(
            core_cases[k].kind, core_cases[k].state, &edge, &rung,
            &core_cases[k].ask);

        if (got != core_cases[k].want) {
            fprintf(stderr, "ladder answer selftest: FAILED decision"
                            " table: %s (ask %d,%d state %d got %llx"
                            " want %llx)\n", core_cases[k].what,
                    core_cases[k].ask.x, core_cases[k].ask.y,
                    core_cases[k].state, got, core_cases[k].want);
            return 1;
        }
    }
    {   /* w <= 3: the probe IS the rung, so every rung touch is a
         * CATCH - the byte law's own consequence, not a smoothing. */
        const ll_blk narrow = { 100, 100, 102, 148 };
        const ll_blk far_edge = { 90, 90, 90, 90 };
        static const ll_pt nask[] = { { 100, 120 }, { 102, 120 } };

        for (k = 0; k < 2; k++)
            if (ladder_answer_core(1, 2, &far_edge, &narrow, &nask[k]) !=
                m_cat) {
                fprintf(stderr, "ladder answer selftest: FAILED narrow"
                                " rung (probe == rung must merge)\n");
                return 1;
            }
        /* PR-528: the same consequence for the rope's narrower
         * probe: w = 2 (< 50%'s floor) is a CATCH too. */
        for (k = 0; k < 2; k++)
            if (ladder_answer_core(2, 2, &far_edge, &narrow, &nask[k]) !=
                m_lcat) {
                fprintf(stderr, "ladder answer selftest: FAILED narrow"
                                " column (rope probe == column must"
                                " merge)\n");
                return 1;
            }
    }
    {   /* Descending x-words: 0x23e00's bounds test (p >= q[0] and
         * p <= q[8] with q[0] > q[8]) is never satisfiable, so such
         * a block answers inert at every asker. PR-149's open item 3
         * stays open; this leg pins the consequence, not the format. */
        const ll_blk down = { 140, 100, 100, 148 };
        const ll_blk off_edge = { 200, 200, 200, 200 };
        static const ll_pt dask[] = { { 120, 124 }, { 140, 100 },
                                      { 100, 148 } };

        for (k = 0; k < 3; k++)
            if (ladder_answer_core(1, 2, &off_edge, &down, &dask[k]) !=
                0ull) {
                fprintf(stderr, "ladder answer selftest: FAILED"
                                " descending block must answer inert"
                                " (ask %d,%d)\n", dask[k].x, dask[k].y);
                return 1;
            }
    }
    {   /* L3, class scope: two chains are anchored - DRABINKA's
         * (PR-141's factory 0x1f8ac -> table 0x97cdc -> +0x78 =
         * 0x3f1c0) and LINA's (PR-528's factory arm 0x4e81d, name
         * test against 0x92005, view pair 0x4e8d2 -> 0xa1dfc ->
         * +0x78 = 0x3f390). The K-prefixed variants stay
         * unanswered (item 2). */
        static const struct { const char *leaf; int want; } scope[] = {
            { "DRABINKA", 1 }, { "drabinka", 1 }, { "LINA", 2 },
            { "lina", 2 }, { "KLINA", 0 }, { "KDRABINA", 0 },
            { "BAL", 0 },
        };

        for (k = 0; k < (int)(sizeof scope / sizeof scope[0]); k++) {
            obj_script s2;
            lvl_obj o2;

            memset(&s2, 0, sizeof s2);
            strcpy(s2.leaf, scope[k].leaf);
            memset(&o2, 0, sizeof o2);
            o2.sc = &s2;
            if (ladder_class(&o2) != scope[k].want) {
                fprintf(stderr, "ladder answer selftest: FAILED class"
                                " scope for '%s'\n", scope[k].leaf);
                return 1;
            }
        }
    }
    {   /* L4, end to end with the model default: no art (obj_art_of
         * NULL through an empty table list), so both blocks are the
         * 1x1 at the object - the edge block wins AT the pixel (P1),
         * one px away NOTHING answers, and the STATE gate holds. The
         * row texts mirror the shape the record quotes from drab.MOV
         * (`1,0` idle, `2,500`/`3,400` drop) as a synthetic script;
         * the authored file itself is NOT in this test. */
        /* PR-216: the gate is the object's STATE INDEX, not its row
         * cursor, and that index is NOT the block position. 0x3f1dc
         * reads [self+0x5c] and branches `cmp 1` (0x3f1e4: the child
         * ANY-test over [self+0xa5], which only sets [self+0x66] bit
         * 6 and ends at the inert exit) / `cmp 2` (0x3f214: the
         * P1/P2/P3 path this model implements); anything else jumps
         * to the inert 0x3f364. What numbers the states: the loader
         * 0x53905 reads the list the script declares BEFORE its
         * blocks into the vector [obj+0x3c], 0x53440 resolves every
         * state name to that list's ONE-BASED position (0 when the
         * name is not declared, 0x53471), 0x534b6 stores the block's
         * record at that slot of an array of count+1 slots (0x5397a),
         * the record-ctor loop 0x533e3 starts at slot 1, and the
         * object ctor writes [obj+0x5c] = 1, the first declared state
         * (0x538ab). ZARIA.01's own DRAB.MOV is one of the 172 of 777
         * scripts where the two orders differ: it declares
         * A_UP,A_DOWN,A_DROP but writes the blocks A_UP,A_DROP,
         * A_DOWN - so A_UP = 1 = the child branch, A_DOWN = 2 = the
         * answer path, A_DROP = 3 = inert. The fixture is that file's
         * shape: the declared list, the block order and the
         * `WAIT K_GEN1`/`A_DOWN(1)=AC_BREAK_NEXT` arms are the shipped
         * ones; only the row numbers are trimmed. The `fi` column is
         * the row cursor and no answer here depends on it: that is the
         * discriminator against the old model, which keyed the branch
         * on fi. */
        static const char DRAB_MOV[] =
            "A_UP\nA_DOWN\nA_DROP\n\n[A_UP]\ncontrol=BY_TIME\n"
            "A_DROP(1)=K_GEN1\nseq=drab\n1,0,WAIT K_GEN1\n\n[A_DROP]\n"
            "control=BY_TIME\nA_DOWN(1)=AC_BREAK_NEXT\nseq=drab\n"
            "2,500\n3,400\n\n[A_DOWN]\ncontrol=BY_TIME\nseq=drab\n"
            "11,0,WAIT K_NEVER\n";
        static const struct { const char *mov; int sti, fi;
                             double ax, ay;
                             unsigned long long want;
                             const char *what; } e_cases[] = {
            { DRAB_MOV, 0, 0, 180, 510, 0ull,
              "the FIRST DECLARED state (A_UP, block 0), ON the object"
              " pixel: the child-ANY branch, so nothing answers - the"
              " ladder a level places never answers" },
            { DRAB_MOV, 1, 0, 180, 510, 0ull,
              "A_DROP (block 1, declared index 3): no branch, inert"
              " 0x3f217 -> 0x3f364 even at the pixel" },
            { DRAB_MOV, 2, 0, 180, 510, 1ull << LL_SIG_AC_EDGE,
              "A_DOWN (block 2 = declared index 2, NOT block index 2)"
              " answers at the pixel: on the default 1x1s the edge"
              " block wins, and only edge" },
            { DRAB_MOV, 2, 1, 180, 510, 1ull << LL_SIG_AC_EDGE,
              "the same at row 1: the row cursor changes nothing" },
            { DRAB_MOV, 2, 9, 180, 510, 1ull << LL_SIG_AC_EDGE,
              "fi 9 past the 2-row list: the shape still clamps"
              " (obj_shape_elem's clamp) and the state gate still holds" },
            { DRAB_MOV, 2, 0, 181, 510, 0ull,
              "A_DOWN but 1 px away: the default 1x1s miss and P2's"
              " FALSE path is the inert exit 0x3f364" },
            { "[A_UP]\ncontrol=BY_TIME\n1,0\n", 0, 0, 180, 510, 0ull,
              "no declared list at all: the block is the engine's"
              " not-declared index 0, so nothing answers" },
            { "A_UP\n\n[A_UP]\ncontrol=BY_TIME\n1,0\n2,500\n", 0, 1,
              180, 510, 0ull,
              "the FIRST declared state answers nothing even at its"
              " SECOND row, where the old model answered AC_EDGE: the row"
              " cursor and the row count are not the gate" },
            { "A_UP\nA_DOWN\n\n[A_DOWN]\ncontrol=BY_TIME\n2,0\n\n"
              "[A_UP]\ncontrol=BY_TIME\n1,0\n", 0, 0, 180, 510,
              1ull << LL_SIG_AC_EDGE,
              "the SECOND DECLARED state written FIRST: block 0 is"
              " index 2 and answers, so the index is the declared order"
              " and not the block order (the old model read the row"
              " cursor here)" },
            { "A_DOWN\n\n[A_UP]\ncontrol=BY_TIME\n1,0\n", 0, 0, 180, 510,
              0ull,
              "a block named in no list (0x53471's 0): inert" },
            { "A_GONE\nA_UP\n\n[A_UP]\ncontrol=BY_TIME\n", 0, 3, 180,
              510, 1ull << LL_SIG_AC_EDGE,
              "no rows at all, row cursor 3: the gate is the index, not"
              " the row list, so the second declared state with no art"
              " still answers AT the pixel" },
            { "A_GONE\nA_UP\n\n[A_UP]\ncontrol=BY_TIME\n", 0, 3, 181,
              510, 0ull,
              "the same state 1 px away: only the state gate is open,"
              " the geometry is not" },
        };

        for (k = 0; k < (int)(sizeof e_cases / sizeof e_cases[0]);
             k++) {
            ll_mov_t *m = calloc(1, sizeof *m);
            obj_script sc;
            lvl_obj o;
            ll_player asker;

            if (!m) {
                fprintf(stderr, "ladder answer selftest: FAILED out of"
                                " memory\n");
                return 1;
            }
            ll_mov_parse(e_cases[k].mov, m);
            if (m->nst < 1 || e_cases[k].sti >= m->nst) {
                fprintf(stderr, "ladder answer selftest: FAILED the"
                                " synthetic script did not parse (%d)\n",
                        m->nst);
                free(m);
                return 1;
            }
            memset(&sc, 0, sizeof sc);
            strcpy(sc.leaf, "DRABINKA");
            sc.mov = m;
            memset(&o, 0, sizeof o);
            o.sc = &sc;
            o.x = 180;
            o.y = 510;
            o.sti = e_cases[k].sti;
            o.fi = e_cases[k].fi;
            memset(&asker, 0, sizeof asker);
            asker.x = e_cases[k].ax;
            asker.y = e_cases[k].ay;
            if (ladder_answer_for(&o, &asker) != e_cases[k].want) {
                fprintf(stderr, "ladder answer selftest: FAILED end to"
                                " end: %s (state %d fi %d ask %d,%d)\n",
                        e_cases[k].what, e_cases[k].sti, e_cases[k].fi,
                        (int)e_cases[k].ax, (int)e_cases[k].ay);
                free(m);
                return 1;
            }
            free(m);
        }
        /* L5, the index space ITSELF (PR-216): ll_mov_parse must give
         * every block the engine's number for it - 1 + its position in
         * the declared list, 0 when its name is not declared - because
         * that number, [obj+0x5c], is what every state-based branch in
         * the game reads. The compare is case-insensitive because the
         * engine's comparator 0x76a69 folds A-Z at 0x76a75; one
         * shipped file (KADMAR.07's BRODA1.MOV, census of the 777)
         * declares A_Go and writes the block [A_GO]. */
        {
            static const struct { const char *mov; int blk, want; }
                idx_cases[] = {
                { DRAB_MOV, 0, 1 }, { DRAB_MOV, 1, 3 }, { DRAB_MOV, 2, 2 },
                { "A_Go\n\n[A_GO]\ncontrol=BY_TIME\n1,0\n", 0, 1 },
                { "A_ONE\nA_TWO\n\n[A_ONE]\ncontrol=BY_TIME\n1,0\n"
                  "[A_TWO]\ncontrol=BY_TIME\n2,0\n", 1, 2 },
                { "[A_GONE]\ncontrol=BY_TIME\n1,0\n", 0, 0 },
            };

            for (k = 0; k < (int)(sizeof idx_cases /
                                  sizeof idx_cases[0]); k++) {
                ll_mov_t *m = calloc(1, sizeof *m);

                if (!m) {
                    fprintf(stderr, "ladder answer selftest: FAILED out"
                                    " of memory\n");
                    return 1;
                }
                ll_mov_parse(idx_cases[k].mov, m);
                if (idx_cases[k].blk >= m->nst ||
                    m->st[idx_cases[k].blk].decl_idx !=
                                                    idx_cases[k].want) {
                    fprintf(stderr, "ladder answer selftest: FAILED state"
                                    " index of block %d (got %d want %d"
                                    " nst %d)\n", idx_cases[k].blk,
                            idx_cases[k].blk < m->nst ?
                            m->st[idx_cases[k].blk].decl_idx : -1,
                            idx_cases[k].want, m->nst);
                    free(m);
                    return 1;
                }
                free(m);
            }
        }
        /* L6 (PR-235): the record-ID space and the AC_DRAB contact
         * law. The engine numbers records with the ds:0xceff8
         * counter (zeroed by the block parser at 0x5150b, pushed to
         * every record create 0x51a2a) - the record's +0x4 ID,
         * copied to the object at +0x8d (0x5386c) and read back by
         * the +0x6c accessor (0x23a10). So ll_mov_parse must give
         * every block its authored position, and 0x287a0 raises
         * AC_DRAB on the LION at a ladder contact exactly when the
         * LION's current-state ID is 6 or 0x12 (A_DEAD/A_DRAP in
         * ZARIA.01's block order - a census cross-check, not a
         * fit) and the other's ID passes 0x28823 (`jg 0x19`
         * skips). */
        {
            static const char ID_MOV[] =
                "A_ONE\nA_TWO\n\n[A_ONE]\ncontrol=BY_TIME\n1,0\n"
                "[A_TWO]\ncontrol=BY_TIME\n2,0\n";
            ll_mov_t *m = calloc(1, sizeof *m);

            if (!m) {
                fprintf(stderr, "ladder answer selftest: FAILED out"
                                " of memory\n");
                return 1;
            }
            ll_mov_parse(ID_MOV, m);
            if (m->nst != 2 || m->st[0].block_pos != 0 ||
                m->st[1].block_pos != 1) {
                fprintf(stderr, "ladder answer selftest: FAILED"
                                " block_pos (nst %d got %d/%d)\n", m->nst,
                        m->nst > 0 ? m->st[0].block_pos : -1,
                        m->nst > 1 ? m->st[1].block_pos : -1);
                free(m);
                return 1;
            }
            free(m);
            static const struct { int lid, oid, want;
                                 const char *what; } law_cases[] = {
                { 18, 0, 1, "A_DRAP (block 18 = 0x12) at the ladder:"
                  " the raise runs (0x2882d)" },
                { 6, 2, 1, "A_DEAD (block 6): the second gate path"
                  " (0x287de) raises too" },
                { 17, 0, 0, "block 17: neither cmp matches" },
                { 19, 0, 0, "block 19: past 0x12 the jg leaves" },
                { 18, 26, 0, "other's ID past 0x19: 0x28823 `jg`"
                  " skips the message" },
                { 18, -1, 0, "no parsed state (the engine's absent"
                  " word): model choice, no raise" },
            };
            for (k = 0; k < (int)(sizeof law_cases /
                                  sizeof law_cases[0]); k++)
                if (acdrab_law(law_cases[k].lid, law_cases[k].oid) !=
                    law_cases[k].want) {
                    fprintf(stderr, "ladder answer selftest: FAILED"
                                    " AC_DRAB law: %s (lion %d other"
                                    " %d)\n", law_cases[k].what,
                            law_cases[k].lid, law_cases[k].oid);
                    return 1;
                }
        }
        {   /* L7 (PR-241): the FLAG branch's RAISE, which is not an
             * answer. 0x3f1dc-0x3f20c (PR-240): at the ASKED
             * ladder's FIRST declared state the answer verb writes
             * `[asked+0x66] |= 0x40` on ITSELF - byte 6 mask 0x40 of
             * the live set at [asked+0x60] = bit 54, which the
             * engine's own name block calls K_GEN1 - and answers
             * NOTHING (the tail 0x3f364). The feed is then the .MOV's
             * own line: ZARIA.01's `[A_UP]` `A_DROP(1)=K_GEN1`
             * matches that bit in the next record step (the key
             * composition 0x53725 hands it to 0x53520 -> 0x52000,
             * PR-239). That chain is the ladder climb OI-078 has been
             * after. Everything here is hand-derived from the bytes
             * quoted in the PR-240 record; the two conditions the
             * port does not model (the requester's [obj+0x30] parent,
             * and the [obj+0xa5] contact vector the 0x44ec0 ANY-test
             * walks) are counted in the model note below the helper. */
            static const char UP_MOV[] =
                "A_UP\nA_DOWN\nA_DROP\n\n[A_UP]\ncontrol=BY_TIME\n"
                "A_DROP(1)=K_GEN1\n1,0,WAIT K_GEN1\n\n[A_DROP]\n"
                "control=BY_TIME\n2,500\n\n[A_DOWN]\ncontrol=BY_TIME\n"
                "11,0,WAIT K_NEVER\n";
            ll_mov_t *m = calloc(1, sizeof *m);
            obj_script sc;
            lvl_obj o, ctl;
            ll_player asker;
            int st;

            if (!m) {
                fprintf(stderr, "ladder answer selftest: FAILED out of"
                                " memory\n");
                return 1;
            }
            ll_mov_parse(UP_MOV, m);
            if (m->nst != 3 || m->st[0].decl_idx != 1) {
                fprintf(stderr, "ladder answer selftest: FAILED the A_UP"
                                " script (nst %d)\n", m->nst);
                free(m);
                return 1;
            }
            memset(&sc, 0, sizeof sc);
            strcpy(sc.leaf, "DRABINKA");
            sc.mov = m;
            memset(&asker, 0, sizeof asker);
            asker.x = 180;                 /* on the rung: the ask that */
            asker.y = 510;                 /* reaches the flag branch   */
            memset(&o, 0, sizeof o);
            o.sc = &sc;
            o.x = 180;
            o.y = 510;
            o.sti = 0;                     /* A_UP = declared index 1  */
            if (ladder_answer_for(&o, &asker) != 0ull) {
                fprintf(stderr, "ladder answer selftest: FAILED: the"
                                " state-1 branch answered something"
                                " (0x3f20f goes to the inert tail)\n");
                free(m);
                return 1;
            }
            ladder_flag_raise(&o);
            if (!(o.sig & (1ull << LL_SIG_K_GEN1))) {
                fprintf(stderr, "ladder answer selftest: FAILED: the"
                                " state-1 branch raised no K_GEN1 on the"
                                " ladder itself (0x3f203-0x3f20c)\n");
                free(m);
                return 1;
            }
            for (st = 0; st < 4 && o.sti == 0; st++)
                obj_advance(&o, 100);
            if (o.sti != 1) {              /* A_DROP is block 1 of that */
                fprintf(stderr, "ladder answer selftest: FAILED: the"
                                " authored A_DROP(1)=K_GEN1 line did not"
                                " take the raised bit (state %d after"
                                " %d steps)\n", o.sti, st);
                free(m);
                return 1;
            }
            /* The control: nothing asked this one. The same script,
             * the same placement, four record steps: PR-149 F5's inert
             * A_UP still stands when no ask arms the bit. */
            memset(&ctl, 0, sizeof ctl);
            ctl.sc = &sc;
            ctl.x = 180;
            ctl.y = 510;
            ctl.sti = 0;
            for (st = 0; st < 4; st++)
                obj_advance(&ctl, 100);
            if (ctl.sti != 0 || ctl.sig) {
                fprintf(stderr, "ladder answer selftest: FAILED: an"
                                " unasked A_UP moved or armed a key"
                                " (state %d set %llx)\n", ctl.sti,
                        ctl.sig);
                free(m);
                return 1;
            }
            free(m);
        }
        {   /* L8 (PR-243): the GATE in front of that raise. 0x44ec0
             * (PR-240) walks the ASKED object's own contact vector and
             * its element verb 0x23e00 is a point-in-rectangle test
             * against the asker's live rectangle ([asker+0x75],
             * PR-242); an empty vector answers FALSE (the count at
             * [hdr+4] is not > 0). The numbers are ZARIA.01's: the
             * placed DRABINKA sits at (180,510) and authors the list
             * `1 33,-3`, which 0x44e90 makes absolute at load as
             * (213,507) - 3 px above its own placement. */
            static const char UP2_MOV[] =
                "A_UP\nA_DOWN\nA_DROP\n\n[A_UP]\ncontrol=BY_TIME\n"
                "A_DROP(1)=K_GEN1\n1,0,WAIT K_GEN1\n\n[A_DROP]\n"
                "control=BY_TIME\n2,500\n\n[A_DOWN]\ncontrol=BY_TIME\n"
                "11,0,WAIT K_NEVER\n";
            static const struct { int npts, px, py; int nrect;
                                 int rect[4]; int box[4]; int want;
                                 const char *what; } gate[] = {
                { 0, 0, 0, 0, { 0 }, { 200, 500, 260, 560 }, 0,
                  "no authored points: the empty vector answers false"
                  " (0x44ec0), so no raise" },
                { 1, 213, 507, 1, { 203, 540, 233, 670 },
                  { 100, 650, 300, 690 }, 0,
                  "a lion walking the ground: his box never covers the"
                  " point 3 px above the placement" },
                { 1, 213, 507, 1, { 203, 540, 233, 670 },
                  { 214, 508, 260, 560 }, 0,
                  "the box 1 px off on both axes: the point is outside" },
                { 1, 213, 507, 1, { 203, 540, 233, 670 },
                  { 213, 507, 260, 560 }, 1,
                  "the box touching the point on its inclusive edge:" },
                { 1, 213, 507, 1, { 203, 540, 233, 670 },
                  { 100, 400, 300, 600 }, 1,
                  "a box that covers it: the raise fires" },
                { 1, 213, 507, 1, { 203, 540, 233, 670 },
                  { 150, 600, 250, 690 }, 0,
                  "PR-246: a walking lion INSIDE the authored rect -"
                  " the candidate filter 0x23140 passes, but the point"
                  " gate (0x44ec0) still does not" },
                { 1, 213, 507, 0, { 0 }, { 100, 400, 300, 600 }, 1,
                  "no authored rect: the generic candidate filter"
                  " ([obj+0x2c], the port's shape meet) stands, the"
                  " raise still fires" },
                { 1, 213, 507, 1, { 400, 540, 460, 670 },
                  { 100, 300, 300, 539 }, 1,
                  "PR-248: a box that covers the point but"
                  " MISSES the authored rectangle: the ask still"
                  " runs. 0x23140 is class slot +0x64, the"
                  " broad phase's ACTIVE-WINDOW test that writes"
                  " the contact node [obj+0x2c]; the walk's"
                  " filter is +0x60, which only reads that node"
                  " (0x23b80) - the ask is NOT gated against the"
                  " authored rectangle" },
            };
            for (unsigned g = 0; g < sizeof gate / sizeof gate[0]; g++) {
                ll_mov_t *m2 = calloc(1, sizeof *m2);
                obj_script sc2;
                lvl_obj o2;

                if (!m2) {
                    fprintf(stderr, "ladder answer selftest: FAILED out of"
                                    " memory\n");
                    return 1;
                }
                ll_mov_parse(UP2_MOV, m2);
                memset(&sc2, 0, sizeof sc2);
                strcpy(sc2.leaf, "DRABINKA");
                sc2.mov = m2;
                memset(&o2, 0, sizeof o2);
                o2.sc = &sc2;
                o2.x = o2.bx = 180;
                o2.y = o2.by = 510;
                o2.sti = 0;                     /* A_UP = declared 1 */
                o2.npts = (unsigned char)gate[g].npts;
                for (int q = 0; q < gate[g].npts; q++) {
                    o2.cpx[q] = (short)gate[g].px;
                    o2.cpy[q] = (short)gate[g].py;
                }
                o2.nrect = (unsigned char)gate[g].nrect;
                if (gate[g].nrect) {
                    o2.arx0 = (short)gate[g].rect[0];
                    o2.ary0 = (short)gate[g].rect[1];
                    o2.arx1 = (short)gate[g].rect[2];
                    o2.ary1 = (short)gate[g].rect[3];
                }
                ladder_ask_raise(&o2, gate[g].box, 0, 0);
                if ((gate[g].want != 0) !=
                    ((o2.sig & (1ull << LL_SIG_K_GEN1)) != 0 ||
                     o2.sti != 0)) {
                    fprintf(stderr, "ladder answer selftest: FAILED the"
                                    " contact-point gate (%s): state %d set"
                                    " %llx, wanted %s\n", gate[g].what,
                            o2.sti, o2.sig,
                            gate[g].want ? "the raise" : "nothing");
                    free(m2);
                    return 1;
                }
                if (gate[g].want && o2.sti != 1) {
                    fprintf(stderr, "ladder answer selftest: FAILED:"
                                    " the raise did not take the authored"
                                    " A_DROP line (state %d)\n", o2.sti);
                    free(m2);
                    return 1;
                }
                free(m2);
            }
        }
        {   /* L10 (PR-255): live_box, the two branches of 0x42930,
             * quoted from it: y0 = y + m0e, y1 = y0 + m12, and the x
             * pair comes from the sign the +0x50 verb 0x53e40 returns -
             * sign > 0: x0 = x + m0c, x1 = x0 + m10; sign <= 0:
             * x1 = x - m0c, x0 = x1 - m10. The first pair is the frame
             * ZARIA.01's A_JUMP is on at the tick the ly_zask leg pins:
             * the ask probe prints that frame's margins as
             * -21,-33,47,66 at pos 209,542, which is the box
             * 188,509..235,575 in that log line. The mirrored case is
             * the same frame with the sign flipped - the same width on
             * the other side of the position point, which is what makes
             * the port's old "mirror not applied" model choice a real
             * gap and not a rounding question. The third pair takes a
             * POSITIVE x margin, where the branches differ by more than
             * a shift, and the last is the 1x1 no-shape default with
             * sign 0 = the `<= 0` branch. */
            static const struct {
                int x, y, mx, my, mw, mh, sign, want[4];
            } bx[] = {
                { 209, 542, -21, -33, 47, 66,  1,
                                        { 188, 509, 235, 575 } },
                { 209, 542, -21, -33, 47, 66, -1,
                                        { 183, 509, 230, 575 } },
                { 100,  50,   3,  -2, 10,  6,  1,
                                        { 103,  48, 113,  54 } },
                { 100,  50,   3,  -2, 10,  6, -1,
                                        {  87,  48,  97,  54 } },
                { 100,  50,   0,   0,  0,  0,  0,
                                        { 100,  50, 100,  50 } },
            };

            for (unsigned g = 0; g < sizeof bx / sizeof bx[0]; g++) {
                int ab[4];

                live_box(bx[g].x, bx[g].y, bx[g].mx, bx[g].my, bx[g].mw,
                         bx[g].mh, bx[g].sign, ab);
                for (int q = 0; q < 4; q++)
                    if (ab[q] != bx[g].want[q]) {
                        fprintf(stderr, "ladder answer selftest: FAILED"
                                        " live_box case %d word %d: %d,"
                                        " want %d (0x42930, sign %d)\n",
                                g, q, ab[q], bx[g].want[q], bx[g].sign);
                        return 1;
                    }
            }
        }
        {   /* L11 (PR-269, the OI-117 law PR-268 measured): the rung
             * rectangle is the AUTHORED COLUMN, not the art box.
             * ZARIA.01's own placement text (hd_files/ETAPY, the
             * ANI2.DES block, read as bytes) authors
             *   DRABINKA,drab.ani,drab.mov,180,510
             *   1 33,-3
             *   23,30;53,160
             *   30,50
             * and the record verb 0x3f0f0 puts that THIRD line into
             * [obj+0xb1..0xbd] through 0x49f90 (the format at 0x91d00
             * is `%d,%d;%d,%d , `) and adds the placement x,y at
             * 0x3f124-0x3f14b, so the column is x 203..233, y
             * 540..670: the authored ladder from its top to its foot.
             * P2 (0x3f256, 0x3f29f) is 0x23e00 against that block,
             * inclusive on both axes; P3 (0x3f283-0x3f2b1) is the
             * same verb against the copy 0x49fb0 narrows to 70 per
             * cent - w = 233-203 = 30, inset = 30*70/100 = 21, half
             * = (30-21)/2 = 4, so x 207..229, y unchanged. The EDGE
             * block is the FOURTH line and its own step, so these
             * legs keep the port's default there: that is what makes
             * the ask at the object pixel answer AC_EDGE and nothing
             * else, and it is counted, not hidden. */
            static const char DRAB_L11[] =
                "A_UP\nA_DOWN\nA_DROP\n\n[A_UP]\ncontrol=BY_TIME\n"
                "A_DROP(1)=K_GEN1\nseq=drab\n1,0,WAIT K_GEN1\n\n[A_DROP]\n"
                "control=BY_TIME\nA_DOWN(1)=AC_BREAK_NEXT\nseq=drab\n"
                "2,500\n3,400\n\n[A_DOWN]\ncontrol=BY_TIME\nseq=drab\n"
                "11,0,WAIT K_NEVER\n";
            static const struct { int nrect, r[4], ax, ay;
                                 unsigned long long want;
                                 const char *what; } au[] = {
                { 1, { 203, 540, 233, 670 }, 210, 561,
                  (1ull << LL_SIG_AC_DRAB) |
                  (1ull << LL_SIG_AC_DRAB_CATCH),
                  "the authored column, inside its middle 70%: the"
                  " merged pair (this is the ask point the ly_zask leg"
                  " prints, 9 px below the authored top)" },
                { 1, { 203, 540, 233, 670 }, 204, 600,
                  1ull << LL_SIG_AC_DRAB,
                  "inside the column, outside its middle 70%: AC_DRAB"
                  " alone (0x3f355), 3 px left of the inset" },
                { 1, { 203, 540, 233, 670 }, 233, 670,
                  1ull << LL_SIG_AC_DRAB,
                  "the far corner of the column, exactly ON it:"
                  " 0x23e00 bounds with jl/jg, so inclusive" },
                { 1, { 203, 540, 233, 670 }, 234, 600, 0ull,
                  "1 px beyond the authored right edge: P2 false is"
                  " the inert exit 0x3f364, no answer at all" },
                { 1, { 203, 540, 233, 670 }, 180, 510,
                  1ull << LL_SIG_AC_EDGE,
                  "the object pixel, OUTSIDE the column: P1 runs"
                  " first and the default edge block answers, so the"
                  " order is edge-then-column" },
                { 0, { 0, 0, 0, 0 }, 210, 561, 0ull,
                  "NO authored column (a synthetic object): the port's"
                  " art-box fallback, counted as a model boundary" },
            };

            for (unsigned g = 0; g < sizeof au / sizeof au[0]; g++) {
                ll_mov_t *m2 = calloc(1, sizeof *m2);
                obj_script sc2;
                lvl_obj o5;
                ll_player asker2;
                unsigned long long got5;

                if (!m2) {
                    fprintf(stderr, "ladder answer selftest: FAILED"
                                    " out of memory (L11)\n");
                    return 1;
                }
                ll_mov_parse(DRAB_L11, m2);
                memset(&sc2, 0, sizeof sc2);
                strcpy(sc2.leaf, "DRABINKA");
                sc2.mov = m2;
                memset(&o5, 0, sizeof o5);
                o5.sc = &sc2;
                o5.x = 180;
                o5.y = 510;
                o5.nrect = (unsigned char)au[g].nrect;
                o5.arx0 = (short)au[g].r[0];
                o5.ary0 = (short)au[g].r[1];
                o5.arx1 = (short)au[g].r[2];
                o5.ary1 = (short)au[g].r[3];
                for (int q = 0; q < m2->nst; q++)
                    if (m2->st[q].decl_idx == 2)
                        o5.sti = q;
                if (m2->st[o5.sti].decl_idx != 2) {
                    fprintf(stderr, "ladder answer selftest: FAILED"
                                    " L11: the fixture has no state with"
                                    " the engine's index 2\n");
                    free(m2);
                    return 1;
                }
                memset(&asker2, 0, sizeof asker2);
                asker2.x = au[g].ax;
                asker2.y = au[g].ay;
                got5 = ladder_answer_for(&o5, &asker2);
                if (got5 != au[g].want) {
                    fprintf(stderr, "ladder answer selftest: FAILED"
                                    " L11 case %u (%s): state %d ask"
                                    " %d,%d authored %d (%d,%d)-(%d,%d)"
                                    " got %llx want %llx\n", g,
                            au[g].what, m2->st[o5.sti].decl_idx,
                            au[g].ax, au[g].ay, au[g].nrect,
                            au[g].r[0], au[g].r[1], au[g].r[2],
                            au[g].r[3], got5, au[g].want);
                    free(m2);
                    return 1;
                }
                free(m2);
            }
        }
        if (place_points_selftest())      /* L9 (PR-243), with the reader */
            return 1;
        if (place_seat_selftest())        /* L13 (PR-335), 0x45120 */
            return 1;
        {   /* L12 (PR-270, the other half of the OI-117 law): the
             * EDGE block is the FOURTH authored line, read through
             * 0x4a0b0 (the format at 0x91d0f is `%d,%d`, the tail at
             * 0x91d15 is ` , `), and the record verb builds it at
             * 0x3f16a-0x3f175 as x0 = x1 = a, y0 = b-0x50, y1 = b,
             * WIDENS the x pair at 0x4a050 - called at 0x3f182 with
             * n = ([rect].x1 - [rect].x0) + 1 (0x3f178-0x3f17d) -
             * and makes all four fields absolute at
             * 0x3f187-0x3f1a7. 0x4a050 itself (0x4a058-0x4a074):
             * d = ([+8] - [+0]) - n, half = d/2 with C truncation
             * toward zero, [+0] += half, [+8] -= half. For the pair
             * x0 = x1 the span is 0, so half = -trunc(n/2) and the
             * band is x a_abs +/- trunc(n/2), y b_abs-0x50..b_abs.
             * ZARIA.01 authors `30,50` with n = 31: the band is
             * x 195..225, y 480..560 - 80 px at the TOP of the
             * ladder, where its lion script's
             * `A_PODCIAG(1)=AC_EDGE,K_UP` asks. The fields here
             * are exactly what place_pair (PR-270) stores: the raw
             * authored pair made absolute with the placement. */
            static const char DRAB_L12[] =
                "A_UP\nA_DOWN\nA_DROP\n\n[A_UP]\ncontrol=BY_TIME\n"
                "A_DROP(1)=K_GEN1\nseq=drab\n1,0,WAIT K_GEN1\n\n[A_DROP]\n"
                "control=BY_TIME\nA_DOWN(1)=AC_BREAK_NEXT\nseq=drab\n"
                "2,500\n3,400\n\n[A_DOWN]\ncontrol=BY_TIME\nseq=drab\n"
                "11,0,WAIT K_NEVER\n";
            static const struct { int r[4], a, b, ax, ay;
                                 unsigned long long want;
                                 const char *what; } pa[] = {
                { { 203, 540, 233, 670 }, 30, 50, 195, 480,
                  1ull << LL_SIG_AC_EDGE,
                  "the near corner of the band, exactly ON it: the\n"
                  "0x23e00 bounds are jl/jg, so inclusive" },
                { { 203, 540, 233, 670 }, 30, 50, 225, 560,
                  1ull << LL_SIG_AC_EDGE,
                  "the far corner AND inside the column and its 70%\n"
                  "middle at once: P1 answers first, AC_EDGE alone" },
                { { 203, 540, 233, 670 }, 30, 50, 226, 560,
                  (1ull << LL_SIG_AC_DRAB) |
                  (1ull << LL_SIG_AC_DRAB_CATCH),
                  "1 px past the band's x: P1 false, and the point\n"
                  "is inside the column AND its middle 207..229" },
                { { 203, 540, 233, 670 }, 30, 50, 194, 560, 0ull,
                  "1 px left of the band AND left of the column:\n"
                  "the inert exit, no answer at all" },
                { { 203, 540, 233, 670 }, 30, 50, 210, 479, 0ull,
                  "1 px above the band's top (b-0x50 = 480): P1\n"
                  "false, and the column starts at y 540" },
                { { 203, 540, 233, 670 }, 30, 50, 180, 510, 0ull,
                  "the object pixel itself: its x 180 is OUTSIDE\n"
                  "the band 195..225, so not even the object's own\n"
                  "anchor answers - the authored band replaced the\n"
                  "1x1 default there" },
                { { 203, 540, 232, 670 }, 30, 50, 225, 480,
                  1ull << LL_SIG_AC_EDGE,
                  "an EVEN element count n = 30: 0x4a050's half is\n"
                  "trunc(-30/2) = -15, the SAME band x 195..225" },
                { { 203, 540, 232, 670 }, 30, 50, 226, 545,
                  (1ull << LL_SIG_AC_DRAB) |
                  (1ull << LL_SIG_AC_DRAB_CATCH),
                  "same even rect, 1 px past the band (y 545 IS in\n"
                  "the column 540..670): and 0x49fb0 narrows\n"
                  "w = 29 to inset 29*70/100 = 20, half (29-20)/2\n"
                  "= 4, so the middle is x 207..228 and 226 is\n"
                  "inside it" },
            };

            for (unsigned g = 0; g < sizeof pa / sizeof pa[0]; g++) {
                ll_mov_t *m2 = calloc(1, sizeof *m2);
                obj_script sc2;
                lvl_obj o6;
                ll_player asker3;
                unsigned long long got6;

                if (!m2) {
                    fprintf(stderr, "ladder answer selftest: FAILED"
                                    " out of memory (L12)\n");
                    return 1;
                }
                ll_mov_parse(DRAB_L12, m2);
                memset(&sc2, 0, sizeof sc2);
                strcpy(sc2.leaf, "DRABINKA");
                sc2.mov = m2;
                memset(&o6, 0, sizeof o6);
                o6.sc = &sc2;
                o6.x = 180;
                o6.y = 510;
                o6.nrect = 1;
                o6.npair = 1;
                o6.arx0 = (short)pa[g].r[0];
                o6.ary0 = (short)pa[g].r[1];
                o6.arx1 = (short)pa[g].r[2];
                o6.ary1 = (short)pa[g].r[3];
                o6.apx = (short)(o6.x + pa[g].a);
                o6.apy = (short)(o6.y + pa[g].b);
                for (int q = 0; q < m2->nst; q++)
                    if (m2->st[q].decl_idx == 2)
                        o6.sti = q;
                if (m2->st[o6.sti].decl_idx != 2) {
                    fprintf(stderr, "ladder answer selftest: FAILED"
                                    " L12: the fixture has no state with"
                                    " the engine's index 2\n");
                    free(m2);
                    return 1;
                }
                memset(&asker3, 0, sizeof asker3);
                asker3.x = pa[g].ax;
                asker3.y = pa[g].ay;
                got6 = ladder_answer_for(&o6, &asker3);
                if (got6 != pa[g].want) {
                    fprintf(stderr, "ladder answer selftest: FAILED"
                                    " L12 case %u (%s): ask %d,%d band a"
                                    " %d b %d rect (%d,%d)-(%d,%d)"
                                    " got %llx want %llx\n", g,
                            pa[g].what, pa[g].ax, pa[g].ay, pa[g].a,
                            pa[g].b, pa[g].r[0], pa[g].r[1],
                            pa[g].r[2], pa[g].r[3], got6, pa[g].want);
                    free(m2);
                    return 1;
                }
                free(m2);
            }
        }
        {   /* L15 (PR-528): ZARIA.01's AUTHORED ROPE through the
             * same grammar. The .des text (ETAPY, read as bytes this
             * pass) authors
             *   LINA,lina.ani,lina.mov,525,525
             *   1 0,0
             *   21,30;29,120
             *   25,49
             * so the column is x 546..554, y 555..645 (the pit's
             * mouth) and the edge band is x apx 550 +/- trunc(n/2),
             * n = 9 -> 546..554 (the column's own 9 px), y
             * 574-0x50..574 = 494..574 - the UPPER part of the
             * rope, exactly where a lion walking the kind-5 canopy
             * above it stands (his centre at x 550 is y ~543).
             * The probe keeps
             * the CENTRAL 50%: w = 8, ins 4, half 2 -> x 548..552.
             * The answer pair is the AC_LINA twins (0x3f4ac/0x3f4d4
             * merge CATCH|LINA, 0x3f525 answers LINA alone). */
            static const char LINA_L15[] =
                "A_UP\nA_DOWN\nA_DROP\n\n[A_UP]\ncontrol=BY_TIME\n"
                "A_DROP(1)=K_GEN1\nseq=lin\n1,0,WAIT K_GEN1\n\n[A_DROP]\n"
                "control=BY_TIME\nA_DOWN(1)=AC_BREAK_NEXT\nseq=lin\n"
                "2,500\n3,400\n\n[A_DOWN]\ncontrol=BY_TIME\nseq=lin\n"
                "10,0,WAIT K_NEVER\n";
            static const struct { int r[4], a, b, ax, ay;
                                 unsigned long long want;
                                 const char *what; } ro[] = {
                { { 546, 555, 554, 645 }, 25, 49, 550, 560,
                  1ull << LL_SIG_AC_EDGE,
                  "the band 494..574 first, P1: AC_EDGE, the same\n"
                  "bit the ladder's band answers - this is what the\n"
                  "canopy walk at x 550 sees, centre y ~543" },
                { { 546, 555, 554, 645 }, 25, 49, 550, 609,
                  (1ull << LL_SIG_AC_LINA) |
                  (1ull << LL_SIG_AC_LINA_CATCH),
                  "1 px below the band, inside the column AND its\n"
                  "50% probe 548..552: the merged twins" },
                { { 546, 555, 554, 645 }, 25, 49, 552, 645,
                  (1ull << LL_SIG_AC_LINA) |
                  (1ull << LL_SIG_AC_LINA_CATCH),
                  "the probe's far corner at the rope's foot,\n"
                  "exactly ON it: inclusive" },
                { { 546, 555, 554, 645 }, 25, 49, 547, 620,
                  1ull << LL_SIG_AC_LINA,
                  "inside the column, left of the 50% probe:\n"
                  "AC_LINA ALONE (0x3f525) - the narrow probe is\n"
                  "the rope's one measured difference from a ladder" },
                { { 546, 555, 554, 645 }, 25, 49, 545, 620, 0ull,
                  "1 px left of the column: the inert exit 0x3f534" },
                { { 546, 555, 554, 645 }, 25, 49, 525, 525, 0ull,
                  "the object pixel itself: its x 525 is OUTSIDE\n"
                  "the band 546..554 (the pair point 550,574 sits\n"
                  "21 px east of the object and the band keeps the\n"
                  "column's width), so not even the anchor answers" },
                { { 546, 555, 554, 645 }, 25, 49, 555, 620, 0ull,
                  "1 px right of the column: inert" },
            };

            for (unsigned g = 0; g < sizeof ro / sizeof ro[0]; g++) {
                ll_mov_t *m2 = calloc(1, sizeof *m2);
                obj_script sc2;
                lvl_obj o7;
                ll_player asker4;
                unsigned long long got7;

                if (!m2) {
                    fprintf(stderr, "ladder answer selftest: FAILED"
                                    " out of memory (L15)\n");
                    return 1;
                }
                ll_mov_parse(LINA_L15, m2);
                memset(&sc2, 0, sizeof sc2);
                strcpy(sc2.leaf, "LINA");
                sc2.mov = m2;
                memset(&o7, 0, sizeof o7);
                o7.sc = &sc2;
                o7.x = 525;
                o7.y = 525;
                o7.nrect = 1;
                o7.npair = 1;
                o7.arx0 = (short)ro[g].r[0];
                o7.ary0 = (short)ro[g].r[1];
                o7.arx1 = (short)ro[g].r[2];
                o7.ary1 = (short)ro[g].r[3];
                o7.apx = (short)(o7.x + ro[g].a);
                o7.apy = (short)(o7.y + ro[g].b);
                for (int q = 0; q < m2->nst; q++)
                    if (m2->st[q].decl_idx == 2)
                        o7.sti = q;
                if (m2->st[o7.sti].decl_idx != 2) {
                    fprintf(stderr, "ladder answer selftest: FAILED"
                                    " L15: the fixture has no state with"
                                    " the engine's index 2\n");
                    free(m2);
                    return 1;
                }
                memset(&asker4, 0, sizeof asker4);
                asker4.x = ro[g].ax;
                asker4.y = ro[g].ay;
                got7 = ladder_answer_for(&o7, &asker4);
                if (got7 != ro[g].want) {
                    fprintf(stderr, "ladder answer selftest: FAILED"
                                    " L15 case %u (%s): ask %d,%d band a"
                                    " %d b %d rect (%d,%d)-(%d,%d)"
                                    " got %llx want %llx\n", g,
                            ro[g].what, ro[g].ax, ro[g].ay, ro[g].a,
                            ro[g].b, ro[g].r[0], ro[g].r[1],
                            ro[g].r[2], ro[g].r[3], got7, ro[g].want);
                    free(m2);
                    return 1;
                }
                free(m2);
            }
        }
    }
    return 0;
}

/* PR-139 (G-5) selftest: the contact/damage reality, hand-derived from
 * the machine code quoted in play_collide - no game data, no video.
 * Case 1: the 0x550c0 truth table. 0x550cf tests the toucher's window
 * (closed -> answer 0 at 0x5511b, whatever the geometry), 0x55105/
 * 0x55108 build sign(toucher.x - self.x) - EQUAL positions give 0, not
 * +/-1 - 0x5510d multiplies by the toucher's facing and 0x55112..0x55119
 * return 1 only for a STRICTLY negative product. The lion is the
 * toucher; ll_player.face +1 = right, -1 = left (PR-82).
 * Case 2: the receiver's arm. 0x50f54 calls 0x502f0 and 0x50f59 drops
 * the hit inside the countdown; on "not invulnerable" 0x50f67 runs
 * [obj.vt+0xe4](1) - 0x50300, which with a non-zero argument is
 * `mov [obj+0x102],0x1e` = 30 - and only then 0x54550 subtracts. The
 * next 0x50ed0 lands inside the countdown and is dropped without
 * touching hp. That arm is what the LION carries: its own class table
 * 0x96960 (the factory 0x1fef2 names it "TLewZaria") has +0xd0 =
 * 0x50ed0 and +0xe4 = 0x50300; the animals install other +0xd0/+0xe4
 * pairs, which is why the port's object-side use of the same constant
 * is named a stand-in in obj_hit's comment and counted as an open item.
 * The countdown's rate (one per state step, 0x50720) is PR-135's leg in
 * obj_state_selftest and is not re-pinned here.
 * Case 3: the engine drops a zero-damage hit before anything else
 * (0x50eef `test ebx,ebx je 0x50f76`) - that is why the port needs no
 * code for the DANGER branch: the level's danger_power ([ds:0xcef40
 * +0x54], read at 0x5102d for answer-A and at 0x55290/0x5529f for the
 * animals' 0x55230) is never authored above 0 by any shipped level
 * (PR-90 census), so that hit answers as "nothing" by the engine's own
 * first gate. */
static int contact_model_selftest(void)
{
    play_ctx pl;
    int bad = 0;

    memset(&pl, 0, sizeof pl);
    pl.p.atc = 0;
    pl.p.face = 1;
    if (clash_suppressed(&pl, 120, 100) || clash_suppressed(&pl, 80, 100)) {
        fprintf(stderr, "contact selftest: FAILED case 0/1 (a closed"
                        " lion window must never suppress)\n");
        bad = 1;
    }
    pl.p.atc = 1;
    if (clash_suppressed(&pl, 120, 100)) {          /* lion right, facing */
        fprintf(stderr, "contact selftest: FAILED case 2 (lion right of"
                        " the object, facing right: product +1, the hit"
                        " must land)\n");
        bad = 1;
    }
    pl.p.face = -1;
    if (!clash_suppressed(&pl, 120, 100)) {         /* lion right, toward */
        fprintf(stderr, "contact selftest: FAILED case 3 (lion right of"
                        " the object, facing left: product -1, the hit"
                        " must be suppressed)\n");
        bad = 1;
    }
    if (clash_suppressed(&pl, 80, 100)) {           /* lion left, away    */
        fprintf(stderr, "contact selftest: FAILED case 4 (lion left of"
                        " the object, facing left: product +1, the hit"
                        " must land)\n");
        bad = 1;
    }
    pl.p.face = 1;
    if (!clash_suppressed(&pl, 80, 100)) {          /* lion left, toward  */
        fprintf(stderr, "contact selftest: FAILED case 5 (lion left of"
                        " the object, facing right: product -1, the hit"
                        " must be suppressed)\n");
        bad = 1;
    }
    if (clash_suppressed(&pl, 100, 100)) {          /* same x: product 0  */
        fprintf(stderr, "contact selftest: FAILED case 6 (equal x: the"
                        " 0x55105 setg answers 0, NOT -1: the hit must"
                        " land)\n");
        bad = 1;
    }
    {   /* the receiver's arm (0x50300) and its drop (0x502f0/0x50f59) */
        static const char cmv[] =
            "A_RUN\n\n[A_RUN]\ncontrol=BY_TIME\n1,100\n";
        ll_mov_t *mm = calloc(1, sizeof *mm);
        obj_script sm;
        lvl_obj o;
        int r;

        if (!mm) {
            fprintf(stderr, "contact selftest: FAILED allocation\n");
            return 1;
        }
        memset(&sm, 0, sizeof sm);
        memset(&o, 0, sizeof o);
        ll_mov_parse(cmv, mm);
        sm.mov = mm;
        o.sc = &sm;
        o.x = o.bx = 300;
        o.y = o.by = 100;
        o.hp = 20;
        r = obj_hit(&o, 15);
        if (r != 1 || o.hp != 5 || o.hit_cd != 0x1e) {
            fprintf(stderr, "contact selftest: FAILED the landed hit did"
                            " not arm exactly 30 (r %d hp %d cd %d, want"
                            " 0x1e at 0x50304)\n", r, o.hp, o.hit_cd);
            bad = 1;
        } else if (obj_hit(&o, 15) != 0 || o.hp != 5) {
            fprintf(stderr, "contact selftest: FAILED a hit inside the"
                            " armed countdown was taken (hp %d)\n", o.hp);
            bad = 1;
        } else {
            o.hit_cd = 0;
            r = obj_hit(&o, 10);
            if (r != 2 || o.hp != -5 || !o.dead) {
                fprintf(stderr, "contact selftest: FAILED the hit after"
                                " the countdown was not taken (r %d hp %d"
                                " dead %d)\n", r, o.hp, o.dead);
                bad = 1;
            } else if (obj_hit(&o, 15) != 0) {
                fprintf(stderr, "contact selftest: FAILED an hp < 0"
                                " object was hit again\n");
                bad = 1;
            }
        }
        free(mm);
    }
    if (!bad)
        printf("contact selftest: the 0x550c0 table (closed window never"
               " suppresses, toward-facing strictly-negative products do,"
               " equal x does not) + the landed hit arms exactly 0x1e and"
               " the next hit is dropped untouched + hp < 0 is not"
               " hit-able (ok)\n");
    return bad;
}

/* one fixed step + the shared logging (transition lines always; per-tick
 * lines only on the headless CI grid where they are grep-pinned) */
static void play_advance(play_ctx *pl, const ll_play_keys *keys,
                         unsigned dt_ms, int verbose, int tick,
                         const lvl_obj *objs, int nobj)
{
    int was = pl->p.sti;
    /* The terrain tests of THIS step use the contact of the state the player
     * was in when it started (that is the animation being shown at this
     * instant). */
    int ev = ll_player_step(&pl->p, &pl->plt, keys, pl->xmin, pl->xmax,
                            dt_ms);
    /* PR-97: re-read the contact AFTER the step, because ll_player_step is
     * where the state transitions happen, and keep the actor's contact LINE
     * invariant across a state change: a grounded actor stands on the same
     * surface, so when the new animation's shape bottom sits a different
     * distance below the position point, the position point moves by exactly
     * that difference. Without this the standing height would depend on
     * which state the actor came from (two runs differing only by an
     * A_TURN detour parked the same state and frame 2 px apart). MODEL
     * CHOICE (the engine surely re-tests the ground every tick, the exact
     * instruction is unread, HANDOFF 7 open item): the shape-bottom geometry
     * itself is the data-derived part. */
    int nc = play_contact(pl);
    if (pl->p.grounded && nc != pl->p.contact)
        pl->p.y += (double)(pl->p.contact - nc);
    ll_player_set_contact(&pl->p, nc);
    gen_bus_raise(pl, (lvl_obj *)objs, nobj, verbose, tick);
    play_collide(pl, objs, nobj, verbose, tick);
    /* PR-329: the rectangle-class ask walk, part of the same per-step
     * question (0x523f0 asks from the state's control words, and 0x4b500
     * reaches the class answer entry 0x4b5be - no shape meet anywhere).
     * Raised as a pulse, consumed by the next step's transitions and row
     * gates before the clear, exactly like every other raise here. */
    rect_class_asks(pl, objs, nobj, verbose, tick);
    if (pl->danger && pl->have) {   /* PR-90 stand-in for the engine's
                                     * per-class attack AI: see
                                     * obj_arm_atak's model note. */
        int ax = (int)(pl->p.x >= 0 ? pl->p.x + 0.5 : pl->p.x - 0.5);
        int ay = (int)(pl->p.y >= 0 ? pl->p.y + 0.5 : pl->p.y - 0.5);
        for (int k = 0; k < nobj; k++)
            obj_arm_atak((lvl_obj *)&objs[k], ax, ay, verbose, tick);
    }
    if (pl->energy <= 0) {                  /* PR-530: the engine's
                                             * life-spending verb is the
                                             * class's +0xd4 (0x503d0):
                                             * `dec ds:0xa94f0` at
                                             * 0x503fb/0x50406 and
                                             * `[obj+0xad] = 0x64` at
                                             * 0x503fc, reached from the
                                             * +0x90 verb's state-2 test
                                             * at 0x54482-0x5449c. This
                                             * energy test is the port's
                                             * own trigger, not that
                                             * chain. */
        pl->lives--;
        if (pl->lives <= 0)       /* PR-531: the engine's `dec` at
                                   * 0x503fb/0x50406 has NO floor, and
                                   * the level loop tests the counter at
                                   * 0x4ae82, writing exit CODE 2 at
                                   * zero (0x4ae8b). The verb still
                                   * completes (energy, the [obj.vt+0x4]
                                   * respawn) and the loop exits on its
                                   * own test - so the port flags, then
                                   * respawns, then the leg routes it. */
            pl->loop_exit = 2;
        pl->energy = 100;
        pl->lock = 0;
        printf("ll_level: play: energy gone -> respawn, %d live(s) left\n",
               pl->lives);
        play_spawn(pl);
    }
    int xi = (int)(pl->p.x >= 0 ? pl->p.x + 0.5 : pl->p.x - 0.5);
    int yi = (int)(pl->p.y >= 0 ? pl->p.y + 0.5 : pl->p.y - 0.5);
    if (ev & LL_PLAY_EV_STATE)
        printf("ll_level: play: %s -> %s\n", pl->mov->st[was].name,
               pl->mov->st[pl->p.sti].name);
    if (pl->p.y > pl->ydead) {              /* PR-531: the BELOW-LINE leg */
        printf("ll_level: play: below lew_rect at (%d,%d) -> energy -1\n",
               xi, yi);
        pl->energy = -1;                    /* 0x54478 writes ONLY the   */
                                           /* energy word; the life is   */
                                           /* spent by the consumer      */
                                           /* above, on a LATER step -   */
                                           /* exactly like the +0x90     */
                                           /* verb being called again.   */
                                           /* The LINE is still the      */
                                           /* lew_rect bottom standing   */
                                           /* in for the camera record's */
                                           /* word + 0x1f4 (0x54467-     */
                                           /* 0x54474) - OI-132 - and so */
                                           /* is the trigger: the verb's */
                                           /* real gate (state index 2   */
                                           /* + the row-wrap pulse,      */
                                           /* 0x54482-0x5449c) cannot    */
                                           /* fire in this port yet:     */
                                           /* ZARIA's A_DRAP leaves on   */
                                           /* K_FWD at the next admit    */
                                           /* and while airborne on      */
                                           /* AC_ONAIR to A_FALL_DOWN.   */
    }
    if (verbose)
        printf("ll_level: play tick %d: state %s seq %s frame %d pos %d,%d"
               " grounded %d v %lld,%lld\n", tick,
               pl->mov->st[pl->p.sti].name,
               pl->mov->st[pl->p.sti].seqkey, ll_player_frame(&pl->p),
               xi, yi, pl->p.grounded,
               (long long)(pl->p.vx * 100.0), (long long)(pl->p.vy * 100.0));
}

/* the lion LAST, over bg + scene objects: the .SEQ anchor convention is
 * bbox origin = (-f0,-f2) relative to the feet point (ll_seq.h; verified
 * on UJ09.SEQ f0=35 f2=68, the walk cycle whose feet ARE the ground
 * line), so the draw origin is (x - cam - (short)f0, y - (short)f2). */
/* Camera follow. PR-136 MEASURED this law from the level's OWN static
 * placements (tools/re/level_landmarks.py + video_camera_read.py give an
 * absolute camera (x, y) per video frame, and camera_law.py turns it into
 * the numbers below; PR-136 record). The lead is the lion's x on the
 * 320-unit view - exactly this file's cam_ox - and the values that HELD
 * frame for frame while he walked are 95..108 walking right (57 of 76
 * held-window frames; the rest sit at the world's left edge, where no
 * lead can hold) and 199..251 walking left (19 frames, a weak band that
 * CAM_LEAD_L sits inside). So CAM_LEAD_R=100, the PR-72 guess, is
 * confirmed, and CAM_LEAD_L=220 stays unremarked-by-better-evidence.
 * What was NOT measured is the catch-up RATE: with the lion standing, the
 * camera's own rate over a 13-frame window is p50 0.25, p95 1.2, max
 * 11.5 units per engine iteration - the eases below (6 x, 2 y per
 * iteration) sit above p95 and below max, and the "ease" shape itself is
 * a model: the game may track at the lion's speed and correct slowly by
 * some other law. Counted open in the PR-136 record; the eases are kept
 * as PR-135 left them. Tracking (lion walking, lead held) is the camera
 * moving AT the lion's speed, which both models do identically. */
#define CAM_LEAD_R 100
#define CAM_LEAD_L 220

static int follow_camx(play_ctx *pl, int cw, int cam_max)
{
    int target = pl->p.walkdir < 0 ? CAM_LEAD_L : CAM_LEAD_R;
    int d = target - pl->cam_ox;

    (void)cw;
    pl->cam_ox += d > 6 ? 6 : d < -6 ? -6 : d;
    int c = (int)pl->p.x - pl->cam_ox;
    if (c < 0)
        c = 0;
    if (c > cam_max)
        c = cam_max;
    return c;
}

/* The vertical anchor. PR-84 read one original frame (gp.mp4 t=205.0,
 * colour split: paws' yellow at y 133..180) and took the 175..180 band
 * as "a grounded lion's feet sit at ~180 of 200". PR-136's absolute
 * read, on 3990 frames where the lion's world y is CONSTANT (standing or
 * walking on a surface), says the paws row is 125..134 with mode 128 -
 * and 128 again on the subset where the camera is NOT at the world
 * clamp. The 175..180 band appears ONLY in the clamp case: ZARIA's
 * ground sits at world y ~681, its cam_ymax is 500, so any anchor at or
 * below 181 lands on the clamp and the row comes out of the clamp, not
 * out of the constant. One anchor of 128 plus the clamp that has always
 * been there reproduces BOTH observed populations (row 128 mid-level,
 * row ~181 at the ground line). PR-73's single mid-level "feet at ~128"
 * observation - which PR-84 set aside as a catch-up artifact - is what
 * the full clip now measures.
 * Model: a GROUNDED lion eases the view to feet-at-CAM_FEET_Y; while
 * AIRBORNE the view HOLDS. PR-136 checked that too, weakly: across 139
 * jump arcs the camera's y change is p50 0 but p05 -26 / p95 +12, so
 * "holds rigidly" is the model's simplification, not the game's law -
 * counted open (the paws colour series fails in 40% of those windows).
 * The ease rate stays invented (2 u/tick) for the same reason: see the
 * follow_camx comment for what is and is not measured. */
#define CAM_FEET_Y 128

static int follow_camy(play_ctx *pl, int ch, int cam_ymax)
{
    (void)ch;
    /* PR-97: p->y is the position point, and the anchor is defined on
     * the PAWS line - the contact point, y + contact (PR-136 measured
     * that feature directly: the paws colour class, the same one PR-84
     * split). While grounded the contact line IS the surface the lion
     * stands on, whatever frame he is showing, so the view does not step
     * when the animation changes. */
    double fy = pl->p.y + (double)pl->p.contact;
    int feet = (int)(fy >= 0 ? fy + 0.5 : fy - 0.5);
    if (!pl->camy_set) {                   /* spawn: snap, no crawl-in */
        pl->camy_set = 1;
        pl->camy_v = feet - CAM_FEET_Y;
    }
    if (pl->p.grounded) {
        int target = feet - CAM_FEET_Y;
        int d = target - pl->camy_v;
        pl->camy_v += d > 2 ? 2 : d < -2 ? -2 : d;
    }
    if (pl->camy_v < 0)
        pl->camy_v = 0;
    if (pl->camy_v > cam_ymax)
        pl->camy_v = cam_ymax;
    return pl->camy_v;
}

/* PR-137: the viewer's OWN camera law, pinned hermetically - no game
 * data; every expected number is derived by hand from the constants
 * named in the two functions above and from the PR-136 measurements:
 *   anchor CAM_FEET_Y = 128 (the paws row measured on 3990
 *     constant-world-y frames: mode 128, and 128 again where the clamp
 *     does not bind);
 *   the world clamp cam_ymax reproduces the OTHER observed row
 *     population by itself: with ZARIA's ground at world y ~681 and
 *     cam_ymax 500 the paws row comes out 681-500 = 181, inside the
 *     175..180 band PR-84 read from one frame - the clamp sets that
 *     row, not the constant, so ONE constant gives both populations;
 *   the leads CAM_LEAD_R/L, the invented eases (6 x, 2 y per call =
 *     one engine iteration), the spawn snap (no crawl-in);
 *   AIRBORNE the view holds.
 * A ZARIA-shaped world: view 320x200, world 2240x700, cam_max 1920,
 * cam_ymax 500. */
int ll_follow_selftest(void)
{
#define FEET_FAIL(...) \
    do { fprintf(stderr, "follow selftest: FAILED " __VA_ARGS__); \
         return 1; } while (0)
    play_ctx pl;
    memset(&pl, 0, sizeof pl);
    pl.p.contact = 28;                      /* an art set's shape bottom  */

    /* 1. ground line, spawn snap, clamp case: feet 653+28 = 681 ->
     *    681-128 = 553 > 500 -> the clamp; the drawn paws row is then
     *    681-500 = 181, inside the band PR-84 measured. That band is
     *    the clamp, NOT the anchor. */
    pl.p.y = 653; pl.p.grounded = 1; pl.camy_set = 0;
    if (follow_camy(&pl, 200, 500) != 500 || pl.camy_v != 500)
        FEET_FAIL("ground spawn did not land on the world clamp "
                  "(camy_v=%d)\n", pl.camy_v);

    /* 2. mid-level surface, clamp NOT binding: feet 442+28 = 470 ->
     *    camy 470-128 = 342, paws row 470-342 = 128 = the measured
     *    anchor. (With the old 178 constant this returned 292 and put
     *    the paws at row 178 - the "40+ px too high" PR-84 itself
     *    noted for unclamped frames.) */
    memset(&pl, 0, sizeof pl);
    pl.p.contact = 28;
    pl.p.y = 442; pl.p.grounded = 1;
    if (follow_camy(&pl, 200, 500) != 342)
        FEET_FAIL("unclamped anchor is not CAM_FEET_Y (got %d)\n",
                  pl.camy_v);

    /* 3. the y ease is 2 units per call and lands EXACTLY on target:
     *    342-300 = 42 -> +2 per call; from 340 the difference is 2,
     *    the exact branch, not a step. PR-136 measured the game's own
     *    catch-up at p50 0.25 / p95 1.2 / max 11.5 units per
     *    iteration, so THIS rate is a model - the test pins that model
     *    so it cannot drift unmentioned. */
    pl.camy_v = 300;
    if (follow_camy(&pl, 200, 500) != 302 ||
        follow_camy(&pl, 200, 500) != 304)
        FEET_FAIL("the y ease is not 2/call (camy_v=%d)\n", pl.camy_v);
    pl.camy_v = 340;
    if (follow_camy(&pl, 200, 500) != 342)
        FEET_FAIL("the y ease does not land exactly on target\n");

    /* 4. the floor clamp: feet 100 -> target -28 -> view pinned at 0,
     *    never a negative camy. */
    memset(&pl, 0, sizeof pl);
    pl.p.y = 100; pl.p.grounded = 1;
    if (follow_camy(&pl, 200, 500) != 0)
        FEET_FAIL("camy went below the world's top edge\n");

    /* 5. AIRBORNE: the view HOLDS - the model PR-73 argued from the
     *    lion rising within the view through the arc (PR-136 counted
     *    that the game itself does not hold rigidly; open item). */
    memset(&pl, 0, sizeof pl);
    pl.camy_set = 1; pl.camy_v = 342; pl.p.y = 300; pl.p.grounded = 0;
    if (follow_camy(&pl, 200, 500) != 342 || pl.camy_v != 342)
        FEET_FAIL("the view moved while the lion was airborne\n");

    /* 6. the x lead at rest: walkdir 1 -> CAM_LEAD_R; a held lead
     *    means the camera is exactly p.x - 100 and the offset does not
     *    move. PR-136 confirmed the held right lead at 95..108. */
    memset(&pl, 0, sizeof pl);
    pl.cam_ox = CAM_LEAD_R;
    pl.p.x = 1000; pl.p.walkdir = 1;
    if (follow_camx(&pl, 320, 1920) != 900 || pl.cam_ox != CAM_LEAD_R)
        FEET_FAIL("held right lead is not CAM_LEAD_R\n");

    /* 7. a turn: the lead eases CAM_LEAD_R -> CAM_LEAD_L at 6 per
     *    call; the 120-unit difference takes exactly 20 calls, and the
     *    21st call still sits 220 behind, at 1000-220 = 780. (The
     *    held left-lead band on video is 199..251, n=19: CAM_LEAD_L
     *    is a weak middle. This test pins which constant the port
     *    commits to, and the 6/call ease model.) */
    pl.p.walkdir = -1;
    int c = 0;
    for (int i = 0; i < 10; i++)
        c = follow_camx(&pl, 320, 1920);
    if (pl.cam_ox != CAM_LEAD_R + 60 || c != 1000 - (CAM_LEAD_R + 60))
        FEET_FAIL("the x ease is not 6/call (ox=%d c=%d)\n",
                  pl.cam_ox, c);
    for (int i = 0; i < 11; i++)
        c = follow_camx(&pl, 320, 1920);
    if (pl.cam_ox != CAM_LEAD_L || c != 780)
        FEET_FAIL("the lead did not settle to CAM_LEAD_L in 20 calls "
                  "(ox=%d c=%d)\n", pl.cam_ox, c);

    /* 8. the world-edge clamps: the camera never leaves 0..world-view,
     *    whatever the lead would ask for. */
    memset(&pl, 0, sizeof pl);
    pl.cam_ox = CAM_LEAD_R; pl.p.x = 50; pl.p.walkdir = 1;
    if (follow_camx(&pl, 320, 1920) != 0)
        FEET_FAIL("camera went left of the world edge\n");
    pl.p.x = 2100;
    if (follow_camx(&pl, 320, 1920) != 1920)
        FEET_FAIL("camera went past the world's right edge\n");

    printf("follow selftest: the viewer's camera law as the model "
           "documents it - grounded anchor CAM_FEET_Y=%d on the contact "
           "line, the WORLD CLAMP (not a second constant) producing the "
           "175..180 ground-line row band PR-84 measured, spawn snap, "
           "y-ease 2/call with an exact final step, floor and ceiling "
           "clamps, view held while airborne; leads CAM_LEAD_R=%d/"
           "CAM_LEAD_L=%d with a 6/call ease settling in exactly 20 "
           "calls, and 0..world-view clamps on camx (ok)\n",
           CAM_FEET_Y, CAM_LEAD_R, CAM_LEAD_L);
    return 0;
#undef FEET_FAIL
}

static void play_draw(const play_ctx *pl, unsigned char *canvas, int cw,
                      int ch, int cam, int camy)
{
    if (!pl->have || pl->p.sti >= pl->mov->nst)
        return;
    const ll_mov_st *st = &pl->mov->st[pl->p.sti];
    const ll_seq *s = play_art(pl, st->seqkey[0] ? st->seqkey
                                                 : pl->mov->seqkey);
    if (!s || !s->count)
        return;
    ll_frame_ref r = ll_mov_frame_ref(ll_player_frame(&pl->p));
    if (r.blank || r.idx >= (int)s->count)   /* same rule as obj_elem     */
        return;
    const ll_seq_elem *e = &s->els[r.idx];
    int x = (int)(pl->p.x >= 0 ? pl->p.x + 0.5 : pl->p.x - 0.5) - cam;
    int y = (int)(pl->p.y >= 0 ? pl->p.y + 0.5 : pl->p.y - 0.5) - camy;
    /* PR-82: the lion turns with his facing, same rule as obj_draw's
     * obj_mirror_of (CH_DIR states set it from walkdir; direction-baked
     * states like A_GO_TO_BALON_L draw with their authored sign and keep
     * the facing as it was). */
    ll_seq_draw_elem_anchor(s, e, canvas, cw, ch, x, y,
                            ll_player_mirror_of(&pl->p, r.mirror), NULL);
}

/* Drop the placements that belong to the player's own art leaf: the player
 * draws that one himself, driven by LEW.MOV. PR-45 measured that NO shipped
 * placement names the player's leaf (the player's attached objects - POCISK,
 * harpun, arrow, balon - are listed in the player leaf's own .des, which is
 * the K_FIRE/POCISK feature, M4-D), so this is only the safety net that
 * keeps exactly one lion on screen. Scripts are shared with the rest of the
 * scene, so nothing is freed here: the placement goes, the script stays. */
static void play_claim_leaf(lvl_obj *objs, int *nobj, const play_ctx *pl)
{
    int w = 0;
    for (int k = 0; k < *nobj; k++) {
        if (objs[k].sc && ci_eq(objs[k].sc->leaf, pl->leaf))
            continue;
        if (w != k)
            objs[w] = objs[k];
        w++;
    }
    if (w < *nobj)
        printf("ll_level: play: claimed leaf %s (%d placement(s) now"
               " player-driven)\n", pl->leaf, *nobj - w);
    *nobj = w;
}

/* PR-194: THE STAGE-DRIVER EXIT CONTRACT IN THE PLAY LEG.
 *
 * The engine's level loop ends and its per-stage caller acts on the code;
 * ported/ll_stage.h states the contract with every address, and PR-193
 * F2/F3/F4 is the read behind it. What this block said when it shipped -
 * "The port has no exit-word PRODUCER: ... its runtime executor was
 * never read" - is ANSWERED and corrected here: PR-198 read the runtime
 * end to end (the row's apply method 0x45750 stores the number into the
 * level object's +0x4 slot; the level loop 0x4abd0 compares that slot
 * against zero at the top of every iteration at 0x4ac55 and returns the
 * constant 1 for any value; the published copy at 0x5d2dc has no reader)
 * and PR-199 implemented it: the port's producer is the authored row
 * (ll_play.c's parser + the row admit + pl.p.exit_word + the guard at
 * the top of the loop below). Two things this block still needs the
 * INJECTION for, and the reason is data, not ignorance: the 26 shipped
 * rows live in JUNCTION / TUNGA.11 / TUNGA2.12 only, none of which the
 * port can run end to end here (JUNCTION's own data says it has no
 * ground, and TUNGA's A_EXIT is entered by the AC_RECT,AC_BLOK signal
 * pair), so LEO_LEVEL_EXIT ("<tick>:<code>" = "the level ended at that
 * tick with that code") stays the way all FOUR outcomes are driven on
 * the RAN.01 leg, and LEO_PLAY_ENDLV (see mov_inject_endlv) drives the
 * one outcome a real row produces. With neither variable present nothing
 * is printed and nothing changes, so every existing pin stays
 * byte-identical.
 *
 * What each outcome does here, and where the port stays honest about not
 * knowing (the log lines say the same, so a test can pin them):
 *   code 3  reset, then run again. The engine calls its reset pair
 *     (0x5da60 + 0x5d050) whose internals are NOT read, so the port's
 *     reset is a NAMED ESTIMATE (rule 1): every object returns to what
 *     the placement parser authored (anchor position, declared start
 *     state, frame 0, row clock and signal pulses cleared, authored
 *     facing) and the lion goes back through play_spawn. The combat
 *     fields (hp / hit_cd / atc / dead) are NOT reset - no reading says
 *     whether the pair touches them.
 *   code 2  the queue leg. The port has no queue (ll_stage_queue_len()
 *     returns 0), which is also the engine's own empty-queue answer
 *     (0x5d38e -> return 0), so the answer is stop, and the line names
 *     that instead of hiding it.
 *   code 4  stop (0x5d3ec).
 *   any other value  run on, with NO reset (0x5d3dc).
 *
 * The code is checked AFTER the tick has been advanced, because that is
 * what makes the legs comparable: a run that stops at tick N has then
 * done exactly the work of a run of N ticks, so the leg can cmp its PPM
 * against the N-tick frame byte for byte.
 */
#define STAGE_MAX_INJ 8
typedef struct { int tick, code; } stage_inj;

static int stage_inj_parse(const char *spec, stage_inj *at)
{
    int n = 0;
    while (spec && *spec && n < STAGE_MAX_INJ) {
        int t = 0, c = 0;
        const char *p = spec;
        while (*p == ' ' || *p == ',')
            p++;
        if (!*p)
            break;
        if (sscanf(p, "%d:%d", &t, &c) != 2 || t < 1) {
            fprintf(stderr, "ll_level: stage driver: LEO_LEVEL_EXIT value"
                            " '%s' is not <tick>:<code> - the rest of the"
                            " list is ignored\n", p);
            break;
        }
        at[n].tick = t;
        at[n].code = c;
        n++;
        while (*p && *p != ',')
            p++;
        spec = p;
    }
    return n;
}

/* the reset half of code 3 - the ESTIMATE described above */
static void stage_reset_to_authored(lvl_obj *objs, int nobj, play_ctx *pl)
{
    for (int k = 0; k < nobj; k++) {
        lvl_obj *o = &objs[k];
        o->sti = (o->sc && o->sc->mov) ? o->sc->mov->start_st : 0;
        o->fi = 0;
        o->face = 1;
        o->row_t = 0;
        o->brk_arm = 0;
        o->sig = 0;
        o->x = o->bx;
        o->y = o->by;
    }
    if (pl && pl->have) {
        /* PR-199: the exit-word slot zeroes here. In the engine the zero
         * belongs to the level ctor (0x4a4c3/0x4a4cc over the freshly
         * allocated 0x3ea block at 0x5d1f5), and the reset pair is
         * followed by the caller re-entering the stage, which builds
         * exactly that new object. The port re-uses this context, so the
         * ctor's zero is applied at the same point in the sequence - and
         * nowhere else: the loop never clears the slot (PR-198). */
        pl->p.exit_word = 0;
        pl->p.end_req = 0;   /* PR-526: same argument for [obj+0x116] -
                              * the base lion's ctor 0x500b0 zeroes it at
                              * 0x5014c on the fresh object, and nothing
                              * else in the loop body ever writes it. */
        play_spawn(pl);
    }
}

/* One exit code, as the engine's wrapper answers it. Returns 1 to keep
 * ticking (AGAIN / QUEUED) or 0 to stop the play leg (STOP). */
static int stage_apply(int code, int tick, lvl_obj *objs, int nobj,
                       play_ctx *pl)
{
    int did_reset = 0;
    int out = ll_stage_outcome(code, ll_stage_queue_len(), &did_reset);

    if (did_reset) {
        printf("ll_level: stage driver: exit %d at tick %d: RESET to the"
               " authored start and run again (ESTIMATE: the engine's pair"
               " 0x5da60+0x5d050 is unread; combat fields are not"
               " touched)\n", code, tick);
        stage_reset_to_authored(objs, nobj, pl);
    } else if (out == LL_STAGE_QUEUED) {
        printf("ll_level: stage driver: exit %d at tick %d: the queue ran"
               " ONE item and the engine returns 2 - not reachable in the"
               " port, ll_stage_queue_len() is 0\n", code, tick);
    } else if (out == LL_STAGE_STOP && code == 2) {
        /* PR-531: the consumer IS the queue runner 0x5d2f0, and its
         * first act is the counter reset - `mov DWORD ds:0xa94f0,0x5`
         * at 0x5d2f9 - then the save write (0x5dcc0, called at
         * 0x5d303), and only then the branch on the queue count
         * (0x5d308) whose empty leg 0x5d38e answers stop. The port
         * reproduces the counter reset; it does not write a save. */
        pl->lives = LL_LIVES_START;
        printf("ll_level: stage driver: exit %d at tick %d: queue leg with"
               " an EMPTY queue (the port has no producer) - the engine's"
               " own empty-queue leg 0x5d38e answers stop; the queue"
               " runner FIRST set the lives back to %d (0x5d2f9; its"
               " save write 0x5dcc0 the port does not do)\n",
               code, tick, LL_LIVES_START);
    } else if (out == LL_STAGE_STOP) {
        printf("ll_level: stage driver: exit %d at tick %d: STOP (the"
               " engine's 0x5d3ec)\n", code, tick);
    } else {
        printf("ll_level: stage driver: exit %d at tick %d: run on, NO"
               " reset (the engine's 0x5d3dc)\n", code, tick);
    }
    return out != LL_STAGE_STOP;
}

/* PR-531: THE LIFE-CHAIN SELFTEST, HERMETIC. Pins the two-stage shape
 * of the engine's fall chain on a synthetic script with NO transitions:
 * (1) the below-line leg of the +0x90 verb ONLY writes the energy word
 * (0x54478 `mov DWORD [obj+0xad],-1`) - it does not respawn and spends
 * nothing; (2) the life is spent by the CONSUMER (the +0xd4 verb
 * 0x503d0: `dec ds:0xa94f0` 0x503fb/0x50406, energy back to 100
 * 0x503fc, the respawn through its [obj.vt+0x4] call at 0x5042b) on a
 * LATER step - the port's energy test stands in for that verb and its
 * exact gate (state index 2 + the list-wrap pulse) is NOT reachable
 * here yet (OI-132); (3) when the counter reaches zero the level loop
 * exits with CODE 2 (0x4ae82-0x4ae8b) and the code-2 consumer 0x5d2f0
 * sets the counter to 5 FIRST (0x5d2f9) and then stops through the
 * empty-queue leg (0x5d38e). No game data, no keys. */
int ll_lives_selftest(void)
{
    static const char cmv[] =
        "A_STAND\nA_HOLD\nA_FALL\n\n"
        "[A_STAND]\ncontrol=BY_TIME\n1,100\n\n"
        "[A_HOLD]\ncontrol=BY_TIME\n1,100\n\n"
        "[A_FALL]\ncontrol=BY_TIME\n1,100\n";
    play_ctx pl;
    ll_play_keys nokeys = { 0 };
    ll_mov_t *mm = calloc(1, sizeof *mm);
    int bad = 0, falls = 0;

    if (!mm)
        return 1;
    ll_mov_parse(cmv, mm);
    memset(&pl, 0, sizeof pl);
    pl.have = 1;
    pl.mov = mm;
    pl.xmin = -5000;
    pl.xmax = 5000;
    pl.ydead = 100.0;
    pl.sx = 0;
    pl.sy = 0;
    pl.energy = 100;
    pl.lives = LL_LIVES_START;
    ll_player_start(&pl.p, mm, 0, 0, 0);
    if (mm->nst != 3 || pl.p.sti != 0) {
        fprintf(stderr, "lives selftest: FAILED the synthetic script did"
                        " not parse three states (nst %d sti %d)\n",
                mm->nst, pl.p.sti);
        free(mm);
        return 1;
    }

    /* (1) below the line: energy -1 ONLY (0x54478). No respawn, no
     * life, no exit code. */
    pl.p.y = 200.0;
    play_advance(&pl, &nokeys, LL_ITER_MS, 0, 1, NULL, 0);
    if (pl.energy != -1 || pl.lives != LL_LIVES_START || pl.loop_exit) {
        fprintf(stderr, "lives selftest: FAILED the below-line leg did"
                        " not write ONLY energy -1 (energy %d lives %d"
                        " loop_exit %d, want -1/%d/0 per 0x54478)\n",
                pl.energy, pl.lives, pl.loop_exit, LL_LIVES_START);
        bad = 1;
    }
    /* (2) the NEXT step is spent by the consumer: one life, energy
     * 100, back at the spawn (the port's stand-in for 0x503d0). */
    play_advance(&pl, &nokeys, LL_ITER_MS, 0, 2, NULL, 0);
    if (pl.lives != LL_LIVES_START - 1 || pl.energy != 100 ||
        pl.p.y >= pl.ydead || pl.loop_exit) {
        fprintf(stderr, "lives selftest: FAILED the consumer leg (lives"
                        " %d energy %d y %.0f loop_exit %d, want"
                        " %d/100/above the line/0 per 0x503fb-0x5042b)\n",
                pl.lives, pl.energy, pl.p.y, pl.loop_exit,
                LL_LIVES_START - 1);
        bad = 1;
    }
    /* (3) spend the rest: the zero is reached, flagged as loop code 2,
     * and NOT before (0x4ae82 tests after every spend). */
    for (falls = 1; falls < LL_LIVES_START && !bad; falls++) {
        pl.p.y = 200.0;
        play_advance(&pl, &nokeys, LL_ITER_MS, 0, 3 + 2 * falls, NULL, 0);
        if (pl.loop_exit) {
            fprintf(stderr, "lives selftest: FAILED a zero exit armed"
                            " before the counter ran out (fall %d lives"
                            " %d)\n", falls, pl.lives);
            bad = 1;
            break;
        }
        play_advance(&pl, &nokeys, LL_ITER_MS, 0, 4 + 2 * falls, NULL, 0);
    }
    if (!bad && (pl.lives != 0 || pl.loop_exit != 2)) {
        fprintf(stderr, "lives selftest: FAILED the zero run (lives %d"
                        " loop_exit %d, want 0/2 per 0x4ae82-0x4ae8b)\n",
                pl.lives, pl.loop_exit);
        bad = 1;
    }
    /* (4) the code-2 consumer FIRST resets the counter to 5 (0x5d2f9)
     * and then stops through its own empty-queue leg (0x5d38e). */
    if (!bad) {
        int cont = stage_apply(2, 100, NULL, 0, &pl);   /* 0 = STOP */
        if (cont != 0 || pl.lives != 5) {
            fprintf(stderr, "lives selftest: FAILED the code-2 consumer"
                            " (continue %d lives %d, want stop and 5 per"
                            " 0x5d2f9/0x5d38e)\n", cont, pl.lives);
            bad = 1;
        }
    }
    free(mm);
    if (bad)
        return 1;
    printf("lives selftest: below the line writes ONLY energy -1"
           " (0x54478) + the next step spends exactly one life, energy"
           " 100 and the spawn (0x503fb-0x5042b stand-in) + the zero"
           " arms loop code 2 and not before (0x4ae82) + code 2 resets"
           " the counter to %d (0x5d2f9) and stops (0x5d38e) (ok)\n",
           LL_LIVES_START);
    return 0;
}

/* PR-199: DRIVING THE REAL end_level ROW PATH ON A REAL LEVEL.
 *
 * The path itself is the shipped one - parse, row admit, slot, the test
 * at the top of the iteration. What cannot be shown with the fixture as
 * it is: the 26 shipped rows live in THREE members only (JUNCTION's
 * A_TUNGA1/A_TUNGA2, TUNGA.11's and TUNGA2.12's A_EXIT - measured over
 * the 777 .MOV members of the HD ANIMACJE, and re-measured by the
 * verify-endlevel leg), and of those the port cannot run one end to
 * end: JUNCTION refuses to play for a reason of its own (its platformy=
 * names none.des, which declares 0 polylines - "this level has NO
 * ground", the port's existing rule), and TUNGA's A_EXIT is entered only
 * through the arm `A_EXIT(x)=AC_RECT,AC_BLOK`, signals this port's
 * object world does not raise yet. RAN.01, the level every play leg
 * runs, carries ZERO such rows (verify-endlevel pins that too).
 * So the leg INJECTS the tail into the level's own text:
 * LEO_PLAY_ENDLV="<state>:<row>:<value>" inserts `,end_level <value>`
 * at the end of one authored row line, where the state and row are
 * counted EXACTLY the way ll_mov_parse counts them ([STATE] headers in
 * order; frame lines by the parser's own test - the shared
 * ll_mov_row_line, which PR-330 put in one place because the engine's
 * test is the number pair OR that pair after the `s <seq>` field its
 * line cursor consumes first). Everything downstream of that
 * byte is shipped code. The line below names the injection, so no log
 * can read it as shipped data. Returns a NEW buffer (caller frees) with
 * the tail inserted, or NULL when nothing was asked for or the address
 * does not resolve - which is printed, never guessed. */
static char *mov_inject_endlv(const char *text, size_t len)
{
    const char *spec = getenv("LEO_PLAY_ENDLV");
    int want_st, want_row, value;
    size_t i = 0;
    int st = -1, row = 0;

    if (!spec)
        return NULL;
    if (sscanf(spec, "%d:%d:%d", &want_st, &want_row, &value) != 3 ||
            want_st < 0 || want_row < 0) {
        printf("ll_level: play: LEO_PLAY_ENDLV='%s' is not"
               " <state>:<row>:<value> - no injection, the script is"
               " parsed exactly as authored\n", spec);
        return NULL;
    }
    while (i < len) {
        size_t j = i;
        while (j < len && text[j] != '\n' && text[j] != '\r')
            j++;
        size_t ln = j - i;
        if (ln && ln < 160) {
            char line[160];
            memcpy(line, text + i, ln);
            line[ln] = '\0';
            if (line[0] == '[') {
                st++;
                row = 0;
            } else if (st == want_st) {
                int a, b;
                if (ll_mov_row_line(line, NULL, NULL, &a, &b)) {
                    if (row == want_row) {
                        char tail[32];
                        int tn = snprintf(tail, sizeof tail,
                                          ",end_level %d", value);
                        char *out = malloc(len + (size_t)tn + 1);
                        if (!out) {
                            printf("ll_level: play: LEO_PLAY_ENDLV: out of"
                                   " memory, the script is parsed as"
                                   " authored\n");
                            return NULL;
                        }
                        memcpy(out, text, j);
                        memcpy(out + j, tail, (size_t)tn);
                        memcpy(out + j + (size_t)tn, text + j,
                               len - j);
                        out[len + (size_t)tn] = '\0';
                        printf("ll_level: play: end_level INJECTED on"
                               " state %d row %d of the level's own .MOV"
                               " text: the line now reads \"%s%s\" (the"
                               " tail is INJECTED by LEO_PLAY_ENDLV, not"
                               " shipped data: the shipped end_level rows"
                               " are in JUNCTION/TUNGA.11/TUNGA2.12 only)"
                               "\n", want_st, want_row, line, tail);
                        return out;
                    }
                    row++;
                }
            }
        }
        i = j;
        while (i < len && (text[i] == '\n' || text[i] == '\r'))
            i++;
    }
    printf("ll_level: play: LEO_PLAY_ENDLV='%s' names state %d row %d,"
           " which this .MOV does not have (it declares %d state(s)) -"
           " no injection\n", spec, want_st, want_row, st + 1);
    return NULL;
}

/* PPM dump of the composited scene (CI hook, LEO_LEVEL_PPM; the parent
 * directory must already exist - CI recipes use /tmp/opencode directly) */
static void scene_ppm(const char *path, int w, int h, const unsigned char *idx,
                      const unsigned char pal[768])
{
    FILE *f = ll_fopen_ci(path, "wb");
    if (!f) {
        fprintf(stderr, "ll_level: cannot write %s\n", path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        unsigned p = idx[i] * 3;
        fputc(pal[p], f);
        fputc(pal[p + 1], f);
        fputc(pal[p + 2], f);
    }
    fclose(f);
    printf("LEO_LEVEL_PPM wrote %s\n", path);
}

/* PCM overlay layers (PR-53). tlo= names the background stack back to
 * front: the first .PCX is the backdrop, every .PCM is a TRANSPARENT overlay
 * painted over it in tlo order (ll_pcm.h). Strips wrap modulo the layer
 * width as in the engine; the .PCX backdrop itself does not scroll (PR-51,
 * viewer policy).
 *
 * PR-57: the per-layer parallax is the engine's own (FUN_00057460/57500/
 * 57530). The level .des carries `speed=x,y;x,y;...` - one percentage pair
 * per tlo layer (default 100*(i+1)/n) - and `main_back=` names the
 * reference layer R (the one that is the world). With camera (cx, cy):
 *     x_i = cx * X[i] / X[R]
 *     y_i = cy * Y[i] / Y[R] + (H_i - 200) - (H_R - 200) * Y[i] / 100
 * (C integer division; 200 = the view height, here ch). */
#define MAX_PCM_LAYERS 8
#define LVL_VIEW_W 320          /* the engine's view, world units */
#define LVL_VIEW_H 200

typedef struct {
    int n;                      /* tlo layers, present or not */
    int ref;                    /* main_back */
    int sx[16], sy[16];         /* percent */
} par_t;

static void par_parse(par_t *pp, const char *speed, const char *mainback,
                      int ntlo)
{
    memset(pp, 0, sizeof *pp);
    pp->n = ntlo > 16 ? 16 : ntlo;
    pp->ref = mainback && *mainback ? atoi(mainback) : 0;
    for (int i = 0; i < pp->n; i++)
        pp->sx[i] = pp->sy[i] = pp->n ? 100 * (i + 1) / pp->n : 100;
    int i = 0;
    const char *c = speed;
    while (c && *c && i < pp->n) {
        int a, b, used = 0;
        if (sscanf(c, " %d , %d ;%n", &a, &b, &used) < 2 &&
            sscanf(c, " %d , %d%n", &a, &b, &used) < 2)
            break;
        pp->sx[i] = a;
        pp->sy[i] = b;
        i++;
        c += used;
    }
    if (pp->ref < 0 || pp->ref >= pp->n)
        pp->ref = 0;
}

/* PR-208: `plany=` is the level's own statement of WHICH CLASSES RIDE WHICH
 * PARALLAX PLANE - the other half of the object-layer law PR-61 left open
 * ("probably the entry index i of the key's list" was never proven). Format,
 * read off all 12 shipped levels that carry a .des:
 *
 *   plany=<spec>|<spec>|...      ONE spec per tlo layer, in tlo order (the
 *                                spec count always equals the tlo= count,
 *                                1..4: JUNCTION "TL", KADMAR2 "TALABA",
 *                                ZARIA "T|TA|TALAB", ENA2 "T|T|TAAL|TBA").
 *   T                            once per spec: that layer's own art (the
 *                                .PCX/.PCM the spec belongs to).
 *   A                            the animacje= key: the k-th A in reading
 *                                order (layer 0 first, left to right inside a
 *                                spec) is the k-th cell of the `animacje=`
 *                                comma list - including a bare `none` cell in
 *                                the middle of a list (RAN.01 "none,wulkany,
 *                                dymy" with 3 A's proves the cell counts).
 *                                Checked: sum(A) == the list length on every
 *                                one of the 12 levels (0 for JUNCTION, whose
 *                                list is exactly one bare `none`: the game's
 *                                own "nothing here" keyword is not a cell).
 *   L                            once: the lew= lion. Its spec is always the
 *                                main_back= layer, i.e. the 100,100 reference
 *                                plane (true on all 12 levels).
 *   B                            once: the belka= belt/HUD strip; always on a
 *                                100,100 spec too (KARA.07 puts it on layer 3
 *                                while the lion is on layer 2 - the same 1:1
 *                                math either way).
 *
 * NO LETTER NAMES `zwierzaki=` OR `ani_plat=` on ANY level - and none needs
 * to: PR-212 read (engine pass 0x4b1d0, 0x4b23d-0x4b286) that the SEGMENT
 * STRUCTURE names the plane for every placement cell of all three keys at
 * once. Cell c of any pool rides the number of '|' before the (c+1)-th 'A'
 * in the raw spec (the last segment index when the A's run out). For
 * ZARIA.01 ("T|TA|TALAB", 3 A's at segments 1,2,2) that puts ZWIERZ2
 * (KAMIEN, cell 1) and ZWIERZ1 (the rhino, the spiders - cell 2) BOTH on
 * layer 2 = main_back, the lion's own plane - which is exactly what the
 * collision-plane gate needs (see the touch filter in the player code),
 * by law and not by the entry-index coincidence PR-61 left behind.
 *
 * PR-209 READ THE ENGINE'S OWN CONSUMER (build/leo/leo.img, exact bytes):
 * 0x4af10 walks the plany string the loader parsed into [level+0x30] (key
 * literal 0x91eb4 at 0x4d230, grammar ' %s' = one token, no spaces) and runs
 * it as a PROGRAM: 'T' drives the tlo controller [level+0x390] through
 * 0x57710/0x57780 (index = the T ordinal), 'L' drives the lion
 * [level+0x394] (its verb +0x74, then 0x4b2f0), 'B' the belt
 * ([level+0x38c] -> 0x448c0, plus the 0x4a170/0x49bd0 pair), 'A' calls
 * 0x4b360, whose k-th call walks cell k of EACH of the three placement-KEY
 * pools (chain-heads arrays at [level+0x58 + keyordinal*4], one pool per
 * key, [level+0x40]++ at 0x4ceb5 - our lvl_obj.src model, PR-60) and runs
 * verb +0x60 (predicate) then +0x74 on each entry; any other letter prints
 * "Bledny opis poziomu - plany" (0x91d2a) and rethrows. So the letters are
 * RUNTIME ACTIVATION, and the cell ordinals they use are the per-key cell
 * indices the loader itself keeps (cell count at [level+0x44 +
 * keyordinal*4], from the ' ' %[^, ' parser 0x4c720 - our des_list_cells).
 * PR-212 READ THE PLANE WRITER (OI-109 closed by PR-213's port): a SEPARATE
 * level-load pass, 0x4b1d0 (only call site 0x4d764), sets the plane of EVERY
 * entry of ALL THREE keys from the same string: entry i of a list chains at
 * cell i raw (0x4ce52/0x4dd50/0x4aaa0; the 'none' literal 0x91df6 leaves an
 * empty head and shifts no index), and cell c of any pool rides the count of
 * '|' before the (c+1)-th 'A' - the last segment when the A's run out -
 * dispatched through class-table slot +0x5c (setter 0x23b90). The letters
 * EXECUTE, the PIPES place: no letter ever named a key's plane. */
#define MAX_PLAN_SPECS MAX_PCM_LAYERS
#define MAX_PLAN_SPEC_LEN 40
#define MAX_ANI_CELLS 16

typedef struct {
    int ok;                         /* 1 = the specs line up with the list  */
    int law;                        /* 1 = cell_plane is filled (a plany=
                                     * string exists): the PR-212 engine law,
                                     * valid for ALL THREE placement keys and
                                     * regardless of `ok` (the engine applies
                                     * the raw string, never a refuted one) */
    int nlayer;                     /* '|'-separated specs, = the tlo count */
    char spec[MAX_PLAN_SPECS][MAX_PLAN_SPEC_LEN];
    int cell_layer[MAX_ANI_CELLS];  /* animacje cell -> layer, -1 = unnamed */
    int cell_plane[MAX_ANI_CELLS];  /* ANY pool cell -> plane (PR-212: the
                                     * '|' count before the (c+1)-th 'A', the
                                     * total '|' when the A's run out)      */
    int ncell_named;                /* cells the A letters name             */
    int nA;                         /* A letters seen, even when not ok     */
} plan_t;

/* Number of comma cells in a .des list value (same split the placement loop
 * uses: empty cells are skipped). *only_none is set when the whole value is
 * one bare `none` token (no dot, ci): that is not a cell, it is the game's
 * "nothing here" keyword. A bare `none` INSIDE a longer list IS a cell:
 * RAN.01's `none,wulkany,dymy` carries 3 planies A letters, so its first
 * cell counts (PR-208). PR-209 confirmed the engine's own cell parser:
 * 0x4c720 scans items with the format ' ' "%[^, " (literal 0x91d6c) into an
 * 8-byte-stride array and counts them (count out at 0x4cdd5's caller), and
 * the string "none" sits at 0x91d7a/0x91d7f - the same split these two
 * counters implement, and the reason a planies A count equals a key's cell
 * count. */
static int des_list_cells(const char *raw, int *only_none)
{
    if (only_none)
        *only_none = 0;
    if (!raw || !raw[0])
        return 0;
    char copy[512];
    snprintf(copy, sizeof copy, "%s", raw);
    int n = 0, none_tok = 0;
    char *save = NULL;
    for (char *tok = strtok_r(copy, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ')
            tok++;
        if (!tok[0])
            continue;
        n++;
        if (!strchr(tok, '.') && ci_eq(tok, "none"))
            none_tok++;
    }
    if (n == 1 && none_tok == 1) {
        if (only_none)
            *only_none = 1;
        return 0;
    }
    return n;
}

/* Fill p from the plany= value for a level with `nlayer` tlo layers and an
 * `animacje=` list of `ncell` cells. ok stays 0 (and nothing is remapped)
 * when the level's own data does not line up, so the caller can say so and
 * fall back. */
static void plany_parse(plan_t *p, const char *plany, int nlayer, int ncell)
{
    memset(p, 0, sizeof *p);
    for (int i = 0; i < MAX_ANI_CELLS; i++)
        p->cell_layer[i] = -1;
    /* PR-212: the engine's PLANE LAW, read at exact bytes (pass 0x4b1d0,
     * 0x4b23d-0x4b286) and applied here to the RAW string the way the
     * engine applies it: plane(c) = the number of '|' before the (c+1)-th
     * 'A', the total '|' (last segment) once the A's run out. It runs for
     * EVERY placement pool at level load, through the setter slot +0x5c;
     * the engine never refuses a spec, so `law` is independent of `ok`.
     * KADMAR2's "TALABA" has no '|', so all of its cells ride plane 0. */
    if (plany && plany[0]) {
        int pipes = 0, a = -1;
        p->law = 1;
        for (const char *c = plany; *c; c++) {
            if (*c == '|')
                pipes++;
            else if (*c == 'A') {
                a++;
                if (a < MAX_ANI_CELLS)
                    p->cell_plane[a] = pipes;
            }
        }
        for (int i = a + 1; i < MAX_ANI_CELLS; i++)
            p->cell_plane[i] = pipes;
    }
    if (!plany || !plany[0] || nlayer <= 0 || nlayer > MAX_PLAN_SPECS)
        return;
    p->nlayer = nlayer;
    char copy[512];
    snprintf(copy, sizeof copy, "%s", plany);
    char *save = NULL;
    int k = 0;
    for (char *tok = strtok_r(copy, "|", &save); tok;
         tok = strtok_r(NULL, "|", &save)) {
        if (k >= nlayer) {          /* more specs than layers: unread */
            p->nlayer = k;
            return;
        }
        snprintf(p->spec[k], MAX_PLAN_SPEC_LEN, "%s", tok);
        k++;
    }
    if (k != nlayer) {
        p->nlayer = k;
        return;
    }
    int cell = 0;
    for (int s = 0; s < nlayer; s++)
        for (const char *c = p->spec[s]; *c; c++) {
            if (*c != 'A')
                continue;
            p->nA++;
            if (cell < ncell && cell < MAX_ANI_CELLS)
                p->cell_layer[cell++] = s;
        }
    if (p->nA != ncell || cell != ncell) {
        p->nlayer = nlayer;
        for (int i = 0; i < MAX_ANI_CELLS; i++)
            p->cell_layer[i] = -1;  /* not ok: nothing is mapped at all */
        return;
    }
    p->ncell_named = cell;
    p->ok = 1;
}


static void layers_at(const ll_pcm *lay, const int *tix, int nlay,
                      const par_t *pp, int cam, int camy, int cw, int ch,
                      int world_w, int world_h, unsigned char *canvas)
{
    (void)world_w;
    for (int k = 0; k < nlay; k++) {
        const ll_pcm *L = &lay[k];
        int i = tix[k];
        long long xr = pp->sx[pp->ref] ? pp->sx[pp->ref] : 100;
        long long yr = pp->sy[pp->ref] ? pp->sy[pp->ref] : 100;
        long long sx = (long long)cam * pp->sx[i] / xr;
        long long sy = (long long)camy * pp->sy[i] / yr +
                       ((long long)L->h - ch) -
                       ((long long)world_h - ch) * pp->sy[i] / 100;
        ll_pcm_blit(L, canvas, cw, ch, (int)sx, (int)sy, 0, 0);
    }
}

/* PR-61: an object is NOT drawn at x - cam: FUN_00052d30/52dc0 take the
 * object's authored position in ITS layer and subtract that layer's own
 * scroll (57500/57530, the same x_i/y_i layers_at uses): screen x =
 * ox - cam*X[i]/X[R], screen y = oy - (cam_y*Y[i]/Y[R] + (H_i - ch) -
 * (H_R - ch)*Y[i]/100). H_i is the height of that tlo layer when its art is
 * on the CD, else the world height (offset 0).
 * CORRECTED BY PR-208, which read WHERE i COMES FROM: it is [obj+0x24], and
 * the level .des declares it in `plany=` - one spec per tlo layer, whose k-th
 * `A` letter is the k-th cell of the `animacje=` list (12 of 12 shipped
 * levels; ZARIA.01 "T|TA|TALAB" puts ANI3.DES on layer 1 and ANI2/ANI1.DES,
 * the balloons, ladder, rope and coconut chain, on layer 2 = main_back, the
 * 1:1 plane the `lew=` lion (`L`) and the `belka=` belt (`B`) are on). The
 * entry-index reading PR-61 guessed is NOT that law; it stays only for the
 * two keys no shipped plany= spec ever names (zwierzaki=, ani_plat=). */
static void obj_layer_cam(const lvl_obj *o, const par_t *pp, const ll_pcm *lay,
                          const int *lay_tix, int nlay, int cam, int camy,
                          int ch, int world_h, int *ocam, int *ocamy)
{
    *ocam = cam;
    *ocamy = camy;
    if (pp->n <= 0)
        return;
    int i = o->layer < pp->n ? o->layer : pp->n - 1;
    long long xr = pp->sx[pp->ref] ? pp->sx[pp->ref] : 100;
    long long yr = pp->sy[pp->ref] ? pp->sy[pp->ref] : 100;
    int hi = world_h;
    for (int k = 0; k < nlay; k++)
        if (lay_tix[k] == i)
            hi = (int)lay[k].h;
    *ocam = (int)((long long)cam * pp->sx[i] / xr);
    *ocamy = (int)((long long)camy * pp->sy[i] / yr + (hi - ch) -
                   ((long long)world_h - ch) * pp->sy[i] / 100);
}

static void paint_base(unsigned char *canvas, const unsigned char *base,
                       int bw, int bh, int cw, int ch, int scrolls,
                       const par_t *pp, int bg_tix, int cam, int camy,
                       int world_h);

/* PR-61 selftest: per-layer object scroll, hand-derived from 57500/57530.
 * speeds 40,80;70,70;100,100 (RAN.01), main_back 2, cam 1000/camy 100, view
 * 400, world 400, layers all 400 high: x_i = 1000*X/100 = 400/700/1000;
 * y_i = 100*Y/100 + 0 - 0 = 80/70/100. A layer 300 high shifts by H-ch =
 * -100, and the layer-2 reference keeps cam exactly. */
static int obj_layer_selftest(void)
{
    par_t pp;
    ll_pcm lay[1];
    int tix[1] = { 0 };
    lvl_obj o;
    int c, cy;
    static const int wantx[3] = { 400, 700, 1000 };
    static const int wanty[3] = { 80, 70, 100 };

    par_parse(&pp, "40,80;70,70;100,100", "2", 3);
    memset(lay, 0, sizeof lay);
    memset(&o, 0, sizeof o);
    for (int i = 0; i < 3; i++) {
        o.layer = (unsigned char)i;
        obj_layer_cam(&o, &pp, lay, tix, 0, 1000, 100, 400, 400, &c, &cy);
        if (c != wantx[i] || cy != wanty[i]) {
            fprintf(stderr, "layer selftest: FAILED layer %d: cam %d,%d (want"
                            " %d,%d)\n", i, c, cy, wantx[i], wanty[i]);
            return 1;
        }
    }
    lay[0].h = 300;
    o.layer = 0;
    obj_layer_cam(&o, &pp, lay, tix, 1, 1000, 100, 400, 400, &c, &cy);
    if (c != 400 || cy != 80 - 100) {
        fprintf(stderr, "layer selftest: FAILED short layer: cam %d,%d (want"
                        " 400,-20)\n", c, cy);
        return 1;
    }
    {   /* the backdrop: base 10x4, view 6x2, speeds 50,50;100,100 ref 1, cam
         * 14 -> x shift 7; camy 2 -> y = 2*50/100 + (4-2) - (4-2)*50/100 = 2 */
        unsigned char base[40], out[12];
        par_t p2;

        par_parse(&p2, "50,50;100,100", "1", 2);
        for (int i = 0; i < 40; i++)
            base[i] = (unsigned char)i;
        paint_base(out, base, 10, 4, 6, 2, 1, &p2, 0, 14, 2, 4);
        for (int y = 0; y < 2; y++)
            for (int x = 0; x < 6; x++)
                if (out[y * 6 + x] != (y + 2) * 10 + (x + 7) % 10) {
                    fprintf(stderr, "layer selftest: FAILED backdrop at %d,%d:"
                                    " %d\n", x, y, out[y * 6 + x]);
                    return 1;
                }
        paint_base(out, base, 6, 2, 6, 2, 0, &p2, 0, 14, 2, 4); /* PCM-only */
        if (memcmp(out, base, 12) != 0) {
            fprintf(stderr, "layer selftest: FAILED unscrolled base\n");
            return 1;
        }
    }
    return 0;
}

/* PR-208 selftest: the plany= law, hermetic (synthetic strings, the shipped
 * levels' own values quoted as data). Every case is one the shipped .des
 * files actually carry, plus the two malformed shapes the reader must refuse.
 * PR-213 adds the engine's PLANE LAW (pass 0x4b1d0) on the same strings:
 * every cell of EVERY pool gets `pref[]` then `tail` (the total '|' - the
 * last segment) as its plane, regardless of want_ok: the engine never
 * refuses a spec, and "TA|TAL" puts cell 0 BEFORE the first pipe, at 0.
 * ZARIA.01/RAN.01 ("T|TA|TALAB"): cell 0 on layer 1, cells 1,2 on layer 2.
 * RAN.01's list starts with a bare `none` AND has 3 A letters, so that cell
 * counts. JUNCTION's whole list is one bare `none`: 0 cells, 0 A letters.
 * ENA2 ("T|T|TAAL|TBA"): cells 0,1 -> layer 2, cell 2 -> layer 3.
 * TUNGA.11 ("T|TA|ATALABA"): 5 cells, 1 on layer 1 and 4 on layer 2. */
static int plany_selftest(void)
{
    struct {
        const char *plany;
        int nlayer;
        const char *list;
        int want_ok;
        int want[MAX_ANI_CELLS];      /* -1 = the cell is not named */
        int npref;                    /* PR-213 cell_plane: pref[] then tail */
        int pref[6];
        int tail;
    } cases[] = {
        { "T|TA|TALAB",   3, "ani3.des,ani2.des,ani1.des",        1,
          { 1, 2, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
          3, { 1, 2, 2 }, 2 },
        { "T|TA|TALAB",   3, "none,wulkany.des,dymy.des",         1,
          { 1, 2, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
          3, { 1, 2, 2 }, 2 },
        { "TL",           1, "none",                              1,
          { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
          0, { 0 }, 0 },
        { "T|T|TAAL|TBA", 4, "ani.des,ani1.des,anidemo.des",      1,
          { 2, 2, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
          3, { 2, 2, 3 }, 3 },
        { "T|TA|ATALABA", 3, "ani4.des,ani3.des,ani2.des,ani1.des,animdemo.des",
          1, { 1, 2, 2, 2, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
          5, { 1, 2, 2, 2, 2 }, 2 },
        { "T|TALAB",      2, "ani1.des,none",                     1,
          { 1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
          2, { 1, 1 }, 1 },
        { "TA|TAL",       2, "a.des,b.des,c.des,d.des",           0,
          { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
          3, { 0, 1, 1 }, 1 },
        { "T|TA",         3, "a.des,b.des",                       0,
          { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
          1, { 1 }, 1 },
    };
    plan_t p;

    for (unsigned c = 0; c < sizeof cases / sizeof cases[0]; c++) {
        int only_none = 0;
        int ncell = des_list_cells(cases[c].list, &only_none);
        plany_parse(&p, cases[c].plany, cases[c].nlayer, ncell);
        if (p.ok != cases[c].want_ok) {
            fprintf(stderr, "plany selftest: FAILED [%s] with %d cells:"
                            " ok=%d (want %d)\n", cases[c].plany, ncell,
                            p.ok, cases[c].want_ok);
            return 1;
        }
        for (int i = 0; i < MAX_ANI_CELLS; i++)
            if (p.cell_layer[i] != cases[c].want[i]) {
                fprintf(stderr, "plany selftest: FAILED [%s] cell %d:"
                                " layer %d (want %d)\n", cases[c].plany, i,
                                p.cell_layer[i], cases[c].want[i]);
                return 1;
            }
        /* PR-213: the engine's plane law, every cell of every pool, whether
         * or not this spec `lines up` - 0x4b1d0 never refuses. */
        if (!p.law) {
            fprintf(stderr, "plany selftest: FAILED [%s]: the plane law is"
                            " off (it runs whenever a plany= string exists)\n",
                    cases[c].plany);
            return 1;
        }
        for (int i = 0; i < MAX_ANI_CELLS; i++) {
            int wp = i < cases[c].npref ? cases[c].pref[i] : cases[c].tail;
            if (p.cell_plane[i] != wp) {
                fprintf(stderr, "plany selftest: FAILED [%s] plane cell %d:"
                                " %d (want %d)\n", cases[c].plany, i,
                                p.cell_plane[i], wp);
                return 1;
            }
        }
    }
    /* the list counter itself: a bare `none` alone is not a cell, one inside
     * a list is (RAN.01 needs it to count). */
    int on = 0;
    if (des_list_cells("none", &on) != 0 || !on ||
        des_list_cells("none,wulkany.des,dymy.des", &on) != 3 || on ||
        des_list_cells("", &on) != 0 ||
        des_list_cells("ani3.des, ani2.des", &on) != 2) {
        fprintf(stderr, "plany selftest: FAILED the list cell count\n");
        return 1;
    }
    return 0;
}

/* PR-64/66: the .PCX backdrop is the first tlo layer and scrolls like every
 * other (FUN_00057500/57530): x = cam * X[i] / X[R] wrapping modulo the image
 * width (RAN.01: 40% of a 28800 px world - a 960 px image necessarily wraps,
 * so the engine tiles it too); y = cam_y * Y[i] / Y[R] + (H_i - 200) -
 * (H_R - 200) * Y[i] / 100 with the engine's 200-row view, clamped to the
 * image. base is bw x bh; a view-sized base (PCM-only levels, bw == cw) is
 * copied as is. */
static void paint_base(unsigned char *canvas, const unsigned char *base,
                       int bw, int bh, int cw, int ch, int scrolls,
                       const par_t *pp, int bg_tix, int cam, int camy,
                       int world_h)
{
    long long sx = 0, sy = 0;

    if (scrolls && pp->n > 0 && bg_tix < pp->n) {
        long long xr = pp->sx[pp->ref] ? pp->sx[pp->ref] : 100;
        long long yr = pp->sy[pp->ref] ? pp->sy[pp->ref] : 100;
        sx = (long long)cam * pp->sx[bg_tix] / xr;
        sy = (long long)camy * pp->sy[bg_tix] / yr + (bh - ch) -
             ((long long)world_h - ch) * pp->sy[bg_tix] / 100;
    }
    sx %= bw;
    if (sx < 0)
        sx += bw;
    if (sy > bh - ch)
        sy = bh - ch;
    if (sy < 0)
        sy = 0;
    memset(canvas, 0, (size_t)cw * (size_t)ch);
    for (int y = 0; y < ch && sy + y < bh; y++) {
        const unsigned char *r = base + (size_t)(sy + y) * (size_t)bw;
        unsigned char *d = canvas + (size_t)y * (size_t)cw;
        for (int x = 0; x < cw; x++)
            d[x] = r[(sx + x) % bw];
    }
}

/* PR-67: the HUD strip (belka=<level .des>, HD ETAPY block). Grammar read
 * from all 12 shipped BELKA files: optional `key=on|off` lines (tools, crown),
 * then items, each an `x,y` line followed by an `[N ]<name>.seq` line - the
 * first item carries N = 10 (its ten frames). By position: 0 the energy head
 * (ten frames, front-facing at full energy turning away), 1 the lives digits,
 * 2 the tools icons, 3 the item-count digits, 4 the item frame, 5 the crown.
 * Coordinates are in the 320x200 view (285,165 -> the bottom-right box of the
 * playthrough video). The .SEQ art is the level's CD ETAPY record.
 * Not modeled: inventory (the item box shows its empty frame), the crown, and
 * the real lives/energy: the HUD shows full energy and the 6 lives the
 * playthrough video starts with (STATUS.LEO's fields are not mapped). */
#define HUD_MAXIT 8
typedef struct {
    int n;
    int tools_off;
    struct {
        int x, y;
        unsigned char *blob;
        ll_seq seq;
        int ok;
    } it[HUD_MAXIT];
} hud_t;

static void hud_free(hud_t *h)
{
    for (int i = 0; i < h->n; i++)
        if (h->it[i].ok) {
            ll_seq_free(&h->it[i].seq);
            free(h->it[i].blob);
        }
    memset(h, 0, sizeof *h);
}

static void hud_load(hud_t *h, llrs *hd_ety, const llrs_slot *hblk,
                     llrs *cd_ety, const llrs_slot *recs, unsigned nrec,
                     const char *belka)
{
    memset(h, 0, sizeof *h);
    if (!hd_ety || !hblk || !belka || !belka[0])
        return;
    llrs_slot *hr = NULL;
    unsigned nh = 0;
    if (llrs_block(hd_ety, hblk, &hr, &nh))
        return;
    char *txt = NULL;
    for (unsigned k = 0; k < nh && !txt; k++)
        if (hr[k].id == 1 && ci_eq(hr[k].name, belka)) {
            unsigned char *t = NULL;
            long n = llrs_read(hd_ety, &hr[k], &t);
            if (n > 0) {
                txt = malloc((size_t)n + 1);
                if (txt) {
                    memcpy(txt, t, (size_t)n);
                    txt[n] = '\0';
                }
            }
            free(t);
        }
    llrs_block_free(hr);
    if (!txt)
        return;
    int px = -1, py = -1;
    for (char *save = NULL, *ln = strtok_r(txt, "\r\n", &save); ln;
         ln = strtok_r(NULL, "\r\n", &save)) {
        int a, b;
        char nm[32];
        if (strncmp(ln, "tools=off", 9) == 0) {
            h->tools_off = 1;
        } else if (strchr(ln, '=')) {
            continue;
        } else if (sscanf(ln, "%d,%d", &a, &b) == 2 && !strstr(ln, ".")) {
            px = a;
            py = b;
        } else if (px >= 0 && h->n < HUD_MAXIT &&
                   (sscanf(ln, "%d %31s", &a, nm) == 2 ||
                    sscanf(ln, "%31s", nm) == 1)) {
            int i = h->n++;
            h->it[i].x = px;
            h->it[i].y = py;
            px = -1;
            for (unsigned k = 0; k < nrec; k++)
                if (recs[k].id == 1 && ci_eq(recs[k].name, nm)) {
                    unsigned char *raw = NULL;
                    long n = llrs_read(cd_ety, &recs[k], &raw);
                    if (n > 0 && ll_seq_parse(raw, (uint32_t)n,
                                              &h->it[i].seq) == 0) {
                        h->it[i].blob = raw;
                        h->it[i].ok = 1;
                    } else {
                        free(raw);
                    }
                }
        }
    }
    free(txt);
    printf("ll_level: hud: %s: %d item(s) (energy head, lives digits, item"
           " box...), tools %s\n", belka, h->n, h->tools_off ? "off" : "on");
}

static void hud_elem(const hud_t *h, int i, unsigned el, unsigned char *cv,
                     int cw, int ch, int x, int y)
{
    if (i >= h->n || !h->it[i].ok || el >= h->it[i].seq.count)
        return;
    ll_seq_draw_elem_anchor(&h->it[i].seq, &h->it[i].seq.els[el], cv, cw, ch,
                            x, y, 0, NULL);
}

static void hud_draw(const hud_t *h, unsigned char *cv, int cw, int ch,
                     int energy, int lives)
{
    int e = (100 - energy) / 10;

    if (h->n < 2)
        return;
    hud_elem(h, 0, (unsigned)(e < 0 ? 0 : e > 9 ? 9 : e), cv, cw, ch,
             h->it[0].x, h->it[0].y);
    if (h->it[1].ok && h->it[1].seq.count >= 10) {
        char dg[12];
        int x = h->it[1].x;
        snprintf(dg, sizeof dg, "%d", lives < 0 ? 0 : lives);
        for (const char *c = dg; *c; c++) {
            unsigned d = (unsigned)(*c - '0');
            hud_elem(h, 1, d, cv, cw, ch, x, h->it[1].y);
            x += (int)(short)h->it[1].seq.els[d].b15 + 2;
        }
    }
    if (!h->tools_off && h->n > 4)
        hud_elem(h, 4, 0, cv, cw, ch, h->it[4].x, h->it[4].y);
}

static void objs_draw(const lvl_obj *objs, int nobj, unsigned char *canvas,
                      int cw, int ch, int cam, int camy, const par_t *pp,
                      const ll_pcm *lay, const int *lay_tix, int nlay,
                      int world_h)
{
    /* PR-93: static images first - they are the scene the actors stand
     * on (the video: the lion in front of the stone shelves, the balloons
     * in front of them too), and the placement files order does not give
     * that: ANI1.DES comes after the ZWIERZ files in every level. */
    for (int k = 0; k < nobj; k++)
        if (objs[k].static_img) {
            int c, cy;
            obj_layer_cam(&objs[k], pp, lay, lay_tix, nlay, cam, camy, ch,
                          world_h, &c, &cy);
            obj_draw(&objs[k], canvas, cw, ch, c, cy);
        }
    for (int k = 0; k < nobj; k++)
        if (!objs[k].static_img) {
            int c, cy;
            obj_layer_cam(&objs[k], pp, lay, lay_tix, nlay, cam, camy, ch,
                          world_h, &c, &cy);
            obj_draw(&objs[k], canvas, cw, ch, c, cy);
        }
}

/* 4b) the authored scene: the level's own placement data (PR-45) ---------
 * A level says what stands where in sub-.des files that its main .des names
 * through THREE keys - measured over all 13 shipped levels. Every .des record
 * inside an HD level block is named by exactly one key of that level's main
 * .des (0 orphans), and the three keys below are the ones that name SCENE
 * objects; the rest belong to other features (lew= the player's own .des,
 * platformy= the ground, belka= the HUD bar, sample= / sufity= / blokady= /
 * demolist=). So the PR-36 fallback of inventing objects from every leaf .SEQ
 * is gone:
 *   zwierzaki=<up to 3 .des>  "animals": the scripted scene actors (RAN.01
 *                             chmury.des = 184 cloud placements, ptaki.des)
 *   animacje=<up to 3 .des>   "animations": props and effects (TUNGA.11
 *                             ANI3.DES = the 11 objects this viewer showed
 *                             by guessing before)
 *   ani_plat=<up to 3 .des>   platform animations: 10 of those files are
 *                             placement files, 1 holds polylines only and 2
 *                             carry both (moving ground: ENA2 plat.des has
 *                             31 polyline lines and 14 authored object
 *                             entries) - the polylines are ground, owned by
 *                             the platformy / AC_* signal work (M4-D)
 * A bare `none` in such a list is the game's OWN keyword for "nothing here"
 * (JUNCTION says animacje=none, RAN.01 says zwierzaki=chmury.des,ptaki.des,
 * none): it is NOT a missing record and never a data hole. `none.des` on the
 * other hand IS a real record name (JUNCTION's level block carries a
 * NONE.DES holding literally "0"), and the exact-name match below resolves it
 * normally. ONE exception, PR-208: a bare `none` in the MIDDLE of a list is a
 * CELL (RAN.01's none,wulkany,dymy carries 3 planies A letters), while a list
 * that is only a bare `none` names no cell at all.
 *
 * The same .des also says which plane those cells ride (PR-208):
 *   plany=<spec>|<spec>|...   one spec per tlo= layer; see plan_t above for
 *                             the letters and what they name. The port prints
 *                             `ll_level: planies: ...` with the cell->layer
 *                             mapping next to the PR-57 `parallax:` line.
 *
 * Inside a placement file (grammar measured over the 68 files the three keys
 * name - 18 zwierzaki + 38 animacje + 12 ani_plat references, none of them
 * naming a missing record - and 2188 entry lines, 2026-11-16): a count line,
 * then one entry per line
 *   "<LEAF>,<class>,<SCRIPT>.<ext>,<x>,<y>"        world x,y, y down
 * Names padded with spaces are the game's own formatting (109 of the lines,
 * e.g. RAN.01 dymy.des writes `, 16500, 360` and ZARIA.02 anipl2.des
 * `JROPE, dummy, jrope.mov, 490, 230`): the name is the same, so they are
 * parsed as the same name. 4 entries carry parameters after the coordinates
 * (KADMAR.07 `KPALMA,xxx,kpal.mov,640,640  -37,-7  -1 100  0`, ZARIA.01
 * `KAMIEN,dummy,kamien1.mov,1360,250,1308`): placed at their authored x,y,
 * their extra parameters printed verbatim and counted, never guessed at. 1
 * entry names a script with no extension at all (KADMAR.07 ANI1.DES
 * `BERLO,dummy,sdfsdf,61,54`) - nothing resolves from it, and it is reported
 * as the data hole in the placement that it is. Alongside the entries these
 * files also carry per-entry property lines ("damage=100", "bounds=40,6550",
 * "scope=0,0", "energy=150" for RAN.01's clouds: combat data this viewer does
 * not model), group header lines, the game's own comment lines starting with
 * ';' or '//' (SADES.06 ANIM2.DES carries the commented-out placement
 * `;ANIBACK,none,chmury.mov,270,90`), and polyline lines of the shape
 * platformy= files use. All of that is counted and named in the report.
 * The count line is NOT trusted: it disagrees with the number of entry lines
 * in 38 of the 68 files (ENA.03 ZWIERZ3.DES declares 88 and holds 134), so
 * every line is read and the real number is what gets reported.
 *
 * The first field names the ANIMACJE leaf table on BOTH installs of the same
 * level: the HD one holds <LEAF>/<SCRIPT>.MOV (the motion script, PLAN 8g),
 * the CD one holds the art - and the art is whatever the script names for the
 * state it is playing: every [STATE] block carries "seq=<name>", naming
 * <NAME>.SEQ in the SAME leaf. Measured over the 708 distinct (level, leaf,
 * script) triples those 2188 entries use: 681 have their .MOV on the HD
 * install (27 are real data holes, listed by exact name below); those 681
 * scripts name 960 distinct (leaf, seq=) art pairs, 953 of which are on this
 * CD (7 are true data holes). What those tables' elements carry (measured
 * 2026-11-16 over exactly those joins, at element level): 8048 carry patch art AND the per-row shape
 * margins (w0=15), 92 carry patch art only (w0=1), and 169 carry the margins
 * with NO patch array at all (w0=2) - invisible solids: the original game's
 * own draw paths are all gated on its patch pointer array, so it never drew
 * those either. The report counts all three, and names the shape-only tables
 * with the real geometry their margins give (ll_seq_shape_*): never a data
 * hole, and no longer called a draw gap. The .mov stem matches one of its
 * OWN seq= names in only 439 of the 681, so joining by stem (what PR-36 did)
 * invented the art of the rest and the 469 "not on these containers (data
 * hole)" notes it printed were false.
 * The <class> token (none / dummy / xxx / balonik.ani ...) is the game's
 * object-class hint; the *.ani names match NO record anywhere in either
 * container (0 .ANI records exist), so they are counted, never used to pick
 * art. */

static const char *const des_place_keys[] = { "zwierzaki", "animacje",
                                              "ani_plat" };
#define DES_KEY_ANIMACJE 1          /* index of the one key plany= names  */

typedef struct {                    /* ANIMACJE of both installs; either may
                                     * be absent on a partial install      */
    llrs *cd;                       /* CD install: the .SEQ art            */
    const llrs_slot *cd_blk;
    llrs *hd;                       /* HD install: the .MOV scripts        */
    const llrs_slot *hd_blk;
} anim_ctx;

typedef struct {                    /* counted while reading, never guessed */
    int files, file_missing, entries, placed, coord_bad;
    int nscript, nscript_missing;
    int nart, nart_missing, art_shapeonly, art_shapecarry, art_over,
        state_noart;
    int prop, group, comment, class_hint, shortline, poly;
    int param_extra, field_space, entry_noext, longline;
    /* PR-243: the authored contact-point list that follows an entry line
     * (the engine's [obj+0xa5] vector). Read and used, not noise: lines
     * = entries that carry one, read = points stored as absolute
     * positions, extra = points beyond OBJ_MAXPTS (counted, kept out). */
    int pts_lines, pts_read, pts_extra;
    /* PR-246: the authored rectangle / pair lines consumed with an
     * entry (the engine's [obj+0xb1] rect and [obj+0xc5] pair).
     * Consumed lines are no longer counted as polyline/group
     * noise: every shipped rect line was in st->poly before. */
    int rect_lines, pair_lines;
    /* PR-335: the entry's OWN SURFACE TABLE (0x45120's record array,
     * see the lvl_obj fields): lines = entries that author one, read =
     * records stored absolute, extra = records past OBJ_MAXSEAT (counted,
     * kept out), short = lines whose pairs run out before the count says
     * they should (a stale count: the engine reads what is there), trail
     * = table lines whose text is not consumed by the record grammar
     * (ZARIA.01's KATAPULT `-6300,-1350`), norec = a zero count, which
     * allocates no table at all (0x45141-0x45143). */
    int seat_lines, seat_read, seat_extra, seat_short, seat_trail,
        seat_norec;
    /* PR-328: the standalone rectangle-class placement line,
     * `TOKEN,x0,y0;x1,y1` - the eight class tables of PR-327 whose +0x18
     * verb is the one record verb 0x40c90. One such line IS an object:
     * the token names its class and the four numbers are its authored box
     * at [obj+0x3c]. Counted apart from the polyline noise they used to be
     * counted as. */
    int rectclass;
    /* PR-93: the two placement grammars the census proved. Static image
     * lines ("<LEAF>,<name>.seq,<x>,<y>", 3 commas) place one image whose
     * .SEQ is named by the line itself; attached children are the five-
     * field entries authored at exactly 0,0 - every one of the 130 in the
     * shipped data follows another entry or its property lines (a vulture's
     * egg, a saw-fish on a log, a drop on a leaf, demo text), so none of
     * them is a placement at the world origin. All counted here; the
     * statics are also placed and drawn, the children are not. */
    int statics, statics_placed, static_out, static_missing,
        static_shapeonly, attached, static_attached;
    int ntrans, ctrl_other, directive, wait_cap, field_other, drop_st,
        drop_fr, drop_tr, seq_switch;
    int fr_total, fr_blank, fr_mirror, fr_oob;   /* frame numbers: the game's
                                                  * 1-based/signed grammar */
} obj_stats;

typedef struct {                    /* the scene being built for one level */
    int cur_src;                    /* des_place_keys index being read      */
    int cur_layer;                  /* the tlo/parallax layer to place on   */
    plan_t plan;                    /* PR-208: this level's plany= mapping  */
    scene_owner *own;               /* owns the script set + placements     */
    anim_ctx ac;
    const char *level;
    int world_w;
    obj_stats st;
} place_ctx;

/* record of an exact name inside a level's ANIMACJE leaf table (leaf tables
 * are nested: block -> id==2 leaf table -> records). Exact name only:
 * nothing here is guessed by stem. */
static int leaf_record(const llrs *c, const llrs_slot *blk, const char *leaf,
                       const char *fname, llrs_slot *out)
{
    if (!c || !blk)
        return 0;
    llrs_slot *recs = NULL;
    unsigned n = 0;
    if (llrs_block(c, blk, &recs, &n))
        return 0;
    int found = 0;
    for (unsigned k = 0; k < n && !found; k++) {
        if (recs[k].id != 2 || !recs[k].name[0] || !ci_eq(recs[k].name, leaf))
            continue;
        llrs_slot *fr = NULL;
        unsigned nf = 0;
        if (llrs_block(c, &recs[k], &fr, &nf))
            continue;
        for (unsigned j = 0; j < nf; j++)
            if (fr[j].id == 1 && fr[j].name[0] && ci_eq(fr[j].name, fname)) {
                *out = fr[j];
                found = 1;
                break;
            }
        llrs_block_free(fr);
    }
    llrs_block_free(recs);
    return found;
}

static void seq_geom(obj_art *a)
{
    a->eidx0 = -1;
    for (unsigned e = 0; e < a->seq.count; e++)
        if (a->seq.els[e].arr_n > 0) {
            a->eidx0 = (int)e;
            break;
        }
    if (a->eidx0 >= 0)
        elem_size(&a->seq, &a->seq.els[a->eidx0], &a->sw, &a->sh);
    a->rx = a->ry = 0;
    for (unsigned e = 0; e < a->seq.count; e++) {
        const ll_seq_elem *el = &a->seq.els[e];
        if (el->arr_n == 0)
            continue;
        int ew, eh, ax = 0, ay = 0;
        elem_size(&a->seq, el, &ew, &eh);
        int tw = (int)(short)el->b19 * 0x50 + (int)(short)el->b15;
        if (tw > ew)
            ew = tw;
        if (el->f0 != 0x7fff) {
            ax = (int)(short)el->f0;
            ay = (int)(short)el->f2;
        }
        if (ax < 0) ax = -ax;
        if (ay < 0) ay = -ay;
        if (ew + ax > a->rx) a->rx = ew + ax;
        if (eh + ay > a->ry) a->ry = eh + ay;
    }

    /* the shape side of the same table: which elements carry the per-row
     * margin lists the game's hit/push geometry reads, and the bbox it
     * computes from them (ll_seq_shape_bbox = FUN_000420b0). Counted and
     * reported; the scene model does not use it to move anything yet. */
    a->sidx0 = -1;
    a->nshape = 0;
    for (unsigned e = 0; e < a->seq.count; e++) {
        int tw, rows, n;
        ll_seq_shape_wh(&a->seq.els[e], &tw, &rows, &n);
        if (!(a->seq.els[e].w0 & 3) || rows <= 0)
            continue;
        a->nshape++;
        if (a->sidx0 < 0) {
            a->sidx0 = (int)e;
            a->stw = tw;
            a->srows = rows;
            ll_seq_shape_bbox(&a->seq, &a->seq.els[e],
                              &a->sbx, &a->sby, &a->sbw, &a->sbh);
            a->nomargins = ll_seq_shape_empty(&a->seq, &a->seq.els[e]);
        }
    }
}

static obj_script *obj_script_new(void)
{
    obj_script *sc = calloc(1, sizeof *sc);
    if (!sc)
        return NULL;
    for (int k = 0; k < LMOV_MAXST; k++)
        sc->ast[k] = -1;
    return sc;
}

static void obj_script_free(obj_script *sc)
{
    if (!sc)
        return;
    for (int k = 0; k < sc->nart; k++) {
        ll_seq_free(&sc->art[k].seq);
        free(sc->art[k].blob);
    }
    free(sc->mov);
    free(sc->movtxt);
    free(sc);
}

static void scene_owner_free(scene_owner *own)
{
    if (!own)
        return;
    for (int k = 0; k < own->nsc; k++)
        obj_script_free(own->set[k]);
    free(own->set);
    free(own->objs);
    memset(own, 0, sizeof *own);
}

static obj_script *obj_script_find(const scene_owner *own, const char *leaf,
                                   const char *movname)
{
    for (int k = 0; k < own->nsc; k++)
        if (ci_eq(own->set[k]->leaf, leaf) &&
            ci_eq(own->set[k]->movname, movname))
            return own->set[k];
    return NULL;
}

/* Add one authored script, or return the one already loaded for it (RAN.01
 * places the same two cloud scripts 184 times: the .MOV parse and the .SEQ
 * art are loaded ONCE per distinct name pair; the placements stay separate
 * because each has its own position and its own place in the cycle).
 * Data holes are recorded on the script with the exact names searched and
 * reported once. */
static obj_script *obj_script_get(place_ctx *pc, const char *leaf,
                                 const char *movname)
{
    scene_owner *own = pc->own;
    obj_stats *st = &pc->st;
    obj_script *sc = obj_script_find(own, leaf, movname);
    if (sc)
        return sc;
    sc = obj_script_new();
    if (!sc)
        return NULL;
    snprintf(sc->leaf, sizeof sc->leaf, "%s", leaf);
    snprintf(sc->movname, sizeof sc->movname, "%s", movname);

    /* the script: HD ANIMACJE <level>/<LEAF>/<SCRIPT>.MOV, exact name */
    llrs_slot sl;
    if (leaf_record(pc->ac.hd, pc->ac.hd_blk, leaf, movname, &sl)) {
        if (llrs_read(pc->ac.hd, &sl, &sc->movtxt) < 0)
            sc->movtxt = NULL;
    }
    if (sc->movtxt) {
        sc->mov = calloc(1, sizeof *sc->mov);
        if (sc->mov)
            ll_mov_parse((const char *)sc->movtxt, sc->mov);
    }
    if (!sc->mov || !sc->mov->nst) {
        st->nscript_missing++;
        printf("ll_level: obj %s/%s: no .MOV script in that HD ANIMACJE leaf"
               " (data hole: the level places this object, the install does"
               " not carry its script)\n", sc->leaf, sc->movname);
    }

    /* the art: each state names its own .SEQ through seq= (measured: up to
     * 10 distinct names in one script), deduped by name. Three honest
     * outcomes, never a guess: the record is there and carries patch art
     * (drawable; counted further: how many of those ALSO carry the shape
     * margins), the record is there but carries shape margins only (kept,
     * counted, drawn as nothing - which is exactly what the original game
     * drew from it too), or the record is not on these containers (a data
     * hole, reported by exact name). */
    if (sc->mov) {
        for (int k = 0; k < sc->mov->nst && k < LMOV_MAXST; k++) {
            const char *key = sc->mov->st[k].seqkey;
            if (!key[0]) {                  /* state names no art at all */
                st->state_noart++;
                continue;
            }
            char artname[24];
            snprintf(artname, sizeof artname, "%s.SEQ", key);
            int idx = -1;
            for (int a = 0; a < sc->nart; a++)
                if (ci_eq(sc->art[a].name, artname)) {
                    idx = a;
                    break;
                }
            if (idx < 0) {
                if (sc->nart >= OBJ_MAXART) {
                    sc->nartover++;
                    st->art_over++;
                    continue;
                }
                obj_art *a = &sc->art[sc->nart];
                snprintf(a->name, sizeof a->name, "%s", artname);
                long asz = -1;
                int rec = leaf_record(pc->ac.cd, pc->ac.cd_blk, leaf,
                                      artname, &sl);
                if (rec)
                    asz = llrs_read(pc->ac.cd, &sl, &a->blob);
                if (asz >= 0 && ll_seq_parse(a->blob, (uint32_t)asz,
                                             &a->seq) == 0) {
                    a->found = 1;
                    snprintf(a->name, sizeof a->name, "%s",
                             sl.name);      /* report the record's own name */
                    seq_geom(a);
                    if (a->eidx0 < 0) {     /* shape margins, no patch art */
                        sc->nartshape++;
                        st->art_shapeonly++;
                    } else if (a->nshape > 0) {   /* art AND shape (w0=15) */
                        sc->nartcarry++;
                        st->art_shapecarry++;
                    }
                } else {
                    free(a->blob);
                    a->blob = NULL;
                    a->seq.count = 0;
                }
                if (!a->found) {
                    sc->nartmiss++;
                    st->nart_missing++;
                    printf("ll_level: obj %s/%s: its seq= names %s in that"
                           " ANIMACJE leaf: no such record (or no parsable"
                           " .SEQ in it) on these containers (data hole) -"
                           " the states that select it draw nothing\n",
                           sc->leaf, sc->movname, artname);
                } else if (a->eidx0 < 0) {
                    printf("ll_level: obj %s/%s: its seq= names %s: the"
                           " record is there, and element %d of it carries the"
                           " SHAPE margins only, no patch art: shape frame"
                           " tw %d, %d rows, bbox %d %d %d %d%s - an invisible"
                           " solid: every draw path in the original game needs"
                           " a patch array, so it drew nothing from this table"
                           " either, and this viewer has no object-collider"
                           " model to use the shape as yet (not a data hole,"
                           " not a draw gap)\n",
                           sc->leaf, sc->movname, a->name, a->sidx0,
                           a->stw, a->srows, a->sbx, a->sby, a->sbw, a->sbh,
                           a->nomargins
                               ? " (every margin byte of BOTH its lists is"
                                 " the empty marker, so the box above is the"
                                 " game's own degenerate result - w = 1 - tw"
                                 " and h = 1 - rows - a fact about its"
                                 " arithmetic, not a gap in the data)" : "");
                }
                idx = sc->nart++;
                st->nart++;
            }
            sc->ast[k] = idx;
        }
        for (int k = 0; k < sc->mov->nst; k++) {
            const ll_mov_st *s = &sc->mov->st[k];
            st->ntrans += s->ntr;
            st->ctrl_other += s->ctrl_other;
            st->directive += s->directive;
            st->wait_cap += s->wait_cap;
            st->field_other += s->field_other;
            st->seq_switch += s->seq_switch;
            int ai = k < LMOV_MAXST ? sc->ast[k] : -1;
            if (ai >= 0 && ai < sc->nart && sc->art[ai].found)
                for (int f = 0; f < s->nfr; f++) {
                    ll_frame_ref r = ll_mov_frame_ref(s->fr[f].frame);
                    st->fr_total++;
                    if (r.blank)
                        st->fr_blank++;
                    else if (r.idx >= (int)sc->art[ai].seq.count)
                        st->fr_oob++;
                    else if (r.mirror)
                        st->fr_mirror++;
                }
        }
        st->drop_st += sc->mov->drop_st;
        st->drop_fr += sc->mov->drop_fr;
        st->drop_tr += sc->mov->drop_tr;
    }

    st->nscript++;
    if (own->nsc == own->capsc) {
        int nc = own->capsc ? own->capsc * 2 : 128;
        obj_script **p = realloc(own->set, (size_t)nc * sizeof *p);
        if (!p) {
            obj_script_free(sc);
            return NULL;
        }
        own->set = p;
        own->capsc = nc;
    }
    own->set[own->nsc++] = sc;
    return sc;
}

/* PR-93: one static image entry. The census over all 13 level blocks found
 * this grammar in the placement files themselves: exactly three commas,
 * "<LEAF>,<name>.seq,<x>,<y>" - the SAME first field as a five-field entry
 * (the ANIMACJE leaf table), but instead of naming a .MOV script the line
 * names the single .SEQ to draw there. Measured joins: 69 of the 84 shipped
 * lines resolve to <level>/<LEAF>/<name>.SEQ on the CD install (the 15
 * misses are all LONA.05 WODOSPAD2, named in the report); ZARIA.01 resolves
 * 14 of 14, and those 14 are the visible scene: the stone shelves the
 * objects stand on (plan4.seq at 70,480 under the balloon at 78,460,
 * plan3.seq at 180,515 under the ladder at 180,510, plan6.seq at 287,430
 * under the vulture at 300,450), the leaves and the nest. The lines carry
 * no class hint, no properties of their own, no motion: the element to draw
 * is the first one of that table that carries patch art (obj_elem's no-
 * script fallback), at the authored x,y through the element's own anchor -
 * the same draw rule every other placement uses (PR-49). Whether the
 * original animated some of these tables stays an open question; drawing
 * frame one is the absence of motion data, not a guess at motion. */
static obj_script *obj_static_get(place_ctx *pc, const char *leaf,
                                  const char *seqname)
{
    scene_owner *own = pc->own;
    obj_stats *st = &pc->st;
    obj_script *sc = obj_script_find(own, leaf, seqname);
    if (sc)
        return sc;
    sc = obj_script_new();
    if (!sc)
        return NULL;
    snprintf(sc->leaf, sizeof sc->leaf, "%s", leaf);
    snprintf(sc->movname, sizeof sc->movname, "%s", seqname);
    /* no .MOV: the line IS the whole script */
    sc->mov = NULL;
    sc->movtxt = NULL;
    obj_art *a = &sc->art[0];
    snprintf(a->name, sizeof a->name, "%s", seqname);
    llrs_slot sl;
    long asz = -1;
    if (leaf_record(pc->ac.cd, pc->ac.cd_blk, leaf, seqname, &sl))
        asz = llrs_read(pc->ac.cd, &sl, &a->blob);
    if (asz >= 0 && ll_seq_parse(a->blob, (uint32_t)asz, &a->seq) == 0) {
        a->found = 1;
        snprintf(a->name, sizeof a->name, "%s", sl.name);
        seq_geom(a);
        if (a->eidx0 < 0) {
            st->static_shapeonly++;
            printf("ll_level: static %s/%s: the record is there but carries"
                   " the SHAPE margins only, no patch art: an invisible"
                   " solid, drawn as nothing by the original game too (not"
                   " a data hole)\n", leaf, seqname);
        }
    } else {
        free(a->blob);
        a->blob = NULL;
        a->seq.count = 0;
        st->static_missing++;
        printf("ll_level: static %s/%s: no such record (or no parsable .SEQ"
               " in it) in that CD ANIMACJE leaf (data hole: the placement"
               " asks for this image, the install does not carry it) - it"
               " draws nothing\n", leaf, seqname);
    }
    sc->nart = 1;
    sc->ast[0] = 0;
    if (own->nsc == own->capsc) {
        int nc = own->capsc ? own->capsc * 2 : 128;
        obj_script **p = realloc(own->set, (size_t)nc * sizeof *p);
        if (!p) {
            obj_script_free(sc);
            return NULL;
        }
        own->set = p;
        own->capsc = nc;
    }
    own->set[own->nsc++] = sc;
    return sc;
}

static lvl_obj *obj_slot(place_ctx *pc)
{
    scene_owner *own = pc->own;
    if (own->nobj == own->capobj) {
        int nc = own->capobj ? own->capobj * 2 : 64;
        lvl_obj *p = realloc(own->objs, (size_t)nc * sizeof *p);
        if (!p)
            return NULL;
        own->objs = p;
        own->capobj = nc;
    }
    lvl_obj *o = &own->objs[own->nobj++];
    memset(o, 0, sizeof *o);
    o->damage = o->b0 = o->b1 = o->scope = o->energy = o->hp = -1;
    o->hit_cd = 0;
    o->atc = 0;
    o->atc_hit = 0;
    o->dead = 0;
    return o;
}

/* One comma-separated name field of a placement line. Spaces around the field
 * are the game's OWN formatting (measured: RAN.01 dymy.des writes `, 16500,
 * 360` and ZARIA.02 anipl2.des `JROPE, dummy, jrope.mov, 490, 230`), so they
 * are not part of the name; a name with a space INSIDE it is not a name (that
 * is what distinguishes a polyline line "1 1495,147;..." from an entry).
 * Returns the position AFTER the ',' that must follow the field, or NULL. */
static const char *place_field(const char *p, const char *end, char *out,
                               size_t cap)
{
    while (p < end && (*p == ' ' || *p == '\t'))
        p++;
    const char *e = p;
    while (e < end && *e != ',')
        e++;
    if (e >= end)
        return NULL;                      /* a field list needs a separator */
    const char *t = e;
    while (t > p && (t[-1] == ' ' || t[-1] == '\t'))
        t--;
    size_t n = (size_t)(t - p);
    if (!n || n >= cap || memchr(p, ' ', n) || memchr(p, '\t', n))
        return NULL;
    memcpy(out, p, n);
    out[n] = '\0';
    return e + 1;
}

/* one integer of a placement line: spaces are allowed before it (see above);
 * returns the position AFTER its last digit, or NULL if there are no digits */
static const char *place_int(const char *p, const char *end, int *v)
{
    while (p < end && (*p == ' ' || *p == '\t'))
        p++;
    const char *s = p;
    if (p < end && (*p == '-' || *p == '+'))
        p++;
    const char *d = p;
    while (p < end && *p >= '0' && *p <= '9')
        p++;
    if (p == d)
        return NULL;
    *v = (int)strtol(s, NULL, 10);       /* digits only, no overflow: the
                                          * widest coordinate is 6 characters */
    return p;
}

/* the ',' between the two coordinates, spaces around it allowed */
static const char *place_comma(const char *p, const char *end)
{
    while (p < end && (*p == ' ' || *p == '\t'))
        p++;
    if (p >= end || *p != ',')
        return NULL;
    return p + 1;
}

/* PR-243: the AUTHORED CONTACT-POINT LIST that continues an entry: the
 * line right after a five-field entry names `N dx,dy[ dx,dy ...]`. The
 * engine reads it as part of that entry - the kind-table +0x18 verb
 * (0x3f0f0 for the placed DRABINKA, 2 of the 186 tables) calls 0x44f10,
 * which parses N into the vector count at [obj+0xa9] and allocates N
 * 12-byte elements at [obj+0xa5], and 0x44e90 then adds the placement to
 * every point, so the authored offsets become ABSOLUTE once, at load,
 * and never move with the object (PR-242). Measured grammar over
 * hd_files/ETAPY: 21 of its 2076 entry lines carry the list, in SIX
 * classes (DRABINKA 4, LINA 7, KLINA 3, XCHG 3, PILAGEN 2, GUARD 2),
 * and N is 1 in every one of them. The points are stored absolute on
 * the placement (o->cpx/cpy); the list line is CONSUMED, so it is no
 * longer counted as a group-header line the way this reader used to
 * classify it (a list line has no '=' and fewer than four commas, so it
 * fell through to st->group). Returns the end of the line it took, or p
 * unchanged when the line at p is not a list: a line this reader cannot
 * classify stays classified, nothing is guessed. */
static const char *place_points(const char *p, const char *end,
                                lvl_obj *o, obj_stats *st)
{
    const char *eol = p;
    int n = -1, dx[OBJ_MAXPTS], dy[OBJ_MAXPTS], i, got = 0;

    while (eol < end && *eol != '\n' && *eol != '\r')
        eol++;
    const char *b = p;
    while (b < eol && (*b == ' ' || *b == '\t'))
        b++;
    const char *e = eol;
    while (e > b && (e[-1] == ' ' || e[-1] == '\t' ||
                               e[-1] == '\r'))
        e--;
    if (e == b)
        return p;
    const char *q = place_int(b, e, &n);
    if (!q || n <= 0)
        return p;                       /* 0 or no count: not this grammar */
    for (i = 0; i < n; i++) {
        int vx = 0, vy = 0;
        const char *s1 = place_int(q, e, &vx);
        const char *s2 = s1 ? place_comma(s1, e) : NULL;
        const char *s3 = s2 ? place_int(s2, e, &vy) : NULL;
        if (!s3)
            return p;                   /* malformed: leave it classified */
        if (got < OBJ_MAXPTS) {
            dx[got] = vx;
            dy[got] = vy;
        } else
            st->pts_extra++;
        got++;
        q = s3;
        if (i + 1 < n) {
            while (q < e && (*q == ' ' || *q == '\t'))
                q++;
        }
    }
    while (q < e && (*q == ' ' || *q == '\t'))
        q++;
    if (q != e || got != n)
        return p;                       /* trailing text: not this grammar */
    for (i = 0; i < got && i < OBJ_MAXPTS; i++) {
        o->cpx[i] = (short)(o->x + dx[i]);      /* the engine's 0x44e90:  */
        o->cpy[i] = (short)(o->y + dy[i]);      /* absolute from load     */
        st->pts_read++;
    }
    o->npts = (unsigned char)(got > OBJ_MAXPTS ? OBJ_MAXPTS : got);
    st->pts_lines++;
    return eol;
}

/* PR-246/PR-247: the second extra line of an entry - the AUTHORED
 * RECTANGLE `x0,y0;x1,y1`, offsets from the placement. The engine
 * reads it in the record verb 0x3f0f0 through 0x49f90 into
 * [obj+0xb1..0xbd] and adds the placement there, so it is kept
 * ABSOLUTE. Measured uses: the class verb +0x64 (0x23140) in the
 * broad phase 0x4b970, against a layer's active window, writing
 * the contact node [obj+0x2c]; and the ladder's state-2 answer
 * (0x3f256). It is NOT what the ask walk filters on. A degenerate rect (right < left, or bottom <
 * top) is swapped: the engine's intersect at 0x23145-0x23170
 * compares [0]/[8] against the other rect's low edges and
 * [4]/[0xc] against its high edges, so an unsorted authoring
 * matches nothing at all; sorting is a counted model choice.
 * (PR-247 measured the other rectangle: a LAYER'S ACTIVE WINDOW
 * from 0x4b940, not a requester's box.) The
 * third authored line (`30,50`, the [obj+0xc5] set) is consumed
 * and counted, not modelled. */
static const char *place_rect(const char *p, const char *end,
                              lvl_obj *o, obj_stats *st)
{
    const char *eol = p;
    int v[4];
    int i;

    while (eol < end && *eol != '\n' && *eol != '\r')
        eol++;
    const char *b = p;
    while (b < eol && (*b == ' ' || *b == '\t'))
        b++;
    const char *e = eol;
    while (e > b && (e[-1] == ' ' || e[-1] == '\t' ||
                               e[-1] == '\r'))
        e--;
    const char *q = b;
    for (i = 0; i < 4; i++) {
        const char *s1 = place_int(q, e, &v[i]);
        if (!s1)
            return p;
        if (i == 0 || i == 2) {         /* ',' between the pair's */
            q = place_comma(s1, e);     /* two numbers            */
            if (!q)
                return p;
        } else
            q = s1;                     /* i==1: the ';' comes next */
        if (i == 1) {                   /* ';' between the pairs   */
            while (q < e && (*q == ' ' || *q == '\t'))
                q++;
            if (q >= e || *q != ';')
                return p;
            q++;
        }
        if (i < 3) {
            while (q < e && (*q == ' ' || *q == '\t'))
                q++;
        }
    }
    while (q < e && (*q == ' ' || *q == '\t'))
        q++;
    if (q != e)
        return p;                       /* trailing text: not this */
    o->arx0 = (short)(o->x + v[0]);
    o->ary0 = (short)(o->y + v[1]);
    o->arx1 = (short)(o->x + v[2]);
    o->ary1 = (short)(o->y + v[3]);
    if (o->arx1 < o->arx0) {
        short t = o->arx0; o->arx0 = o->arx1; o->arx1 = t;
    }
    if (o->ary1 < o->ary0) {
        short t = o->ary0; o->ary0 = o->ary1; o->ary1 = t;
    }
    o->nrect = 1;
    st->rect_lines++;
    return eol;
}

/* PR-246/PR-270: the third authored line, a bare `x,y` pair. The
 * record verb reads it through 0x4a0b0 - the scanf helper 0x49920
 * with the format at 0x91d0f (`%d,%d`), then 0x498f0 with the
 * tail at 0x91d15 (` , `) - and stores the pair ABSOLUTE at load:
 * 0x3f16a-0x3f175 builds (x0 = x1 = a, y0 = b-0x50, y1 = b),
 * 0x4a050 widens the x pair (see ladder_answer_for), and
 * 0x3f187-0x3f1a7 adds the placement to all four fields. The port
 * stores the same absolute pair point (apx, apy) that its
 * consumers re-derive that block from; the modelled consumer is
 * the edge block of the ladder/rope answer verb (PR-270, OI-078). */
static const char *place_pair(const char *p, const char *end,
                              lvl_obj *o, obj_stats *st)
{
    const char *eol = p;
    int vx = 0, vy = 0;

    while (eol < end && *eol != '\n' && *eol != '\r')
        eol++;
    const char *b = p;
    while (b < eol && (*b == ' ' || *b == '\t'))
        b++;
    const char *e = eol;
    while (e > b && (e[-1] == ' ' || e[-1] == '\t' ||
                               e[-1] == '\r'))
        e--;
    const char *s1 = place_int(b, e, &vx);
    const char *s2 = s1 ? place_comma(s1, e) : NULL;
    const char *s3 = s2 ? place_int(s2, e, &vy) : NULL;

    if (!s3)
        return p;
    while (s3 < e && (*s3 == ' ' || *s3 == '\t'))
        s3++;
    if (s3 != e)
        return p;
    o->apx = (short)(o->x + vx);       /* absolute, once, at load: */
    o->apy = (short)(o->y + vy);       /* 0x3f187-0x3f1a7 */
    o->npair = 1;
    st->pair_lines++;
    return eol;
}

/* PR-335: the entry's OWN SURFACE TABLE - the fourth continuation line,
 * the one the animated-platform family reads (0x1e4a0/0x1e560 call the
 * element method 0x45120 on the object's own point [obj+0xa5]). Every
 * claim here is 0x45120's own code, byte for byte:
 *   `%d` (0x9182d) into [point+8] is the record count N;
 *   N == 0 allocates NOTHING and returns (0x45141-0x45143);
 *   otherwise (N+1)*0xd+4 bytes are allocated (0x45149-0x4515a) and the
 *   first `%d,%d` pair (0x4a0b0, 0x45169) fills record 0;
 *   the loop at 0x4517e runs EXACTLY N times, each pass reading one
 *   ` %c` (0x91830) and then one pair: that character is record i's
 *   flag byte - ';' 0x3b stores 1, '#' 0x23 stores 2, any other
 *   character stores 0 (0x451a1-0x451e0) - and it does NOT end the
 *   loop, which is gated only by `cmp ebp,[point+8]` (0x451c9). So the
 *   separator between two points is a flag on the point before it, and
 *   the last record carries no flag at all (nothing writes [block +
 *   N*0xd + 0xc]); this port stores 0 there and says so.
 *   After the loop 0x451e2 asks the stream for one literal ';'
 *   (0x91834), which is why the shipped lines end in ';' cleanly; any
 *   other text is left where it is, with no reader found for it.
 * Then the record verb calls 0x45200(point, [obj+0], [obj+4]) at
 * 0x1e521-0x1e52a and 0x45217-0x4521a adds that pair to every record,
 * so the table is stored ABSOLUTE, exactly like the contact points
 * (PR-246's law, not PR-311's "kept relative").
 * A line this reader cannot parse stays classified the way it was:
 * place_points, place_rect and place_pair are tried first, so ZARIA.01's
 * ladder and rope lists (`1 33,-3`) still go to the contact-point
 * grammar and not here. Returns the end of the consumed line, or p. */
static const char *place_seat_table(const char *p, const char *end,
                                    lvl_obj *o, obj_stats *st)
{
    const char *eol = p;
    while (eol < end && *eol != '\n' && *eol != '\r')
        eol++;
    const char *b = p;
    while (b < eol && (*b == ' ' || *b == '\t'))
        b++;
    const char *e = eol;
    while (e > b && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
        e--;
    if (e == b)
        return p;
    int n = 0;
    const char *q = place_int(b, e, &n);
    if (!q)
        return p;
    const char *sp = q;
    while (sp < e && (*sp == ' ' || *sp == '\t'))
        sp++;
    if (sp == q || sp == e)
        return p;                   /* no count + records: not this line */
    q = sp;
    st->seat_lines++;
    if (n <= 0) {                   /* zero: 0x45141-0x45143 allocates no
                                     * table at all and returns after the
                                     * count. Nothing in the shipped data
                                     * authors a negative count; the
                                     * engine's allocation at 0x4514a
                                     * would run away on one, so the port
                                     * takes no records for it either. */
        st->seat_norec += n == 0;
        goto rest;
    }
    for (int i = 0; i <= n; i++) {
        int vx = 0, vy = 0;
        const char *s1 = place_int(q, e, &vx);
        const char *s2 = s1 ? place_comma(s1, e) : NULL;
        const char *s3 = s2 ? place_int(s2, e, &vy) : NULL;
        if (!s3) {                  /* the count is longer than the
                                     * pairs: the engine reads what is
                                     * there and stops. Six shipped
                                     * lines do this - LONA.05 ANI3's
                                     * `23 999,354;...` and `20 ...`
                                     * and TUNGA2.12 ZWIERZ's four
                                     * GORYL paths - and not one of
                                     * them is a line this reader
                                     * reaches. */
            st->seat_short++;
            break;
        }
        q = s3;
        int keep = o->nseat < OBJ_MAXSEAT;
        int at = o->nseat;
        if (keep) {
            o->seat_x[at] = (short)(o->x + vx);
            o->seat_y[at] = (short)(o->y + vy);
            o->seat_flag[at] = 0;   /* the last record carries no flag:
                                     * 0x451cb ends the loop first */
            o->nseat++;
            st->seat_read++;
        } else
            st->seat_extra++;
        if (i < n) {                /* the ` %c` before the NEXT pair is
                                     * THIS record's flag byte */
            while (q < e && (*q == ' ' || *q == '\t'))
                q++;
            char c = q < e ? *q++ : '\0';
            if (keep)
                o->seat_flag[at] =
                    (unsigned char)(c == ';' ? 1 : c == '#' ? 2 : 0);
        }
    }
    while (q < e && (*q == ' ' || *q == '\t'))
        q++;
    if (q < e && *q == ';')        /* 0x451e2: the literal ';' at 0x91834 */
        q++;
rest:
    while (q < e && (*q == ' ' || *q == '\t'))
        q++;
    if (q < e)                      /* text no reader claims: counted */
        st->seat_trail++;
    return eol;
}

/* L9 (PR-243): the authored contact-point grammar, tested next to
 * the reader it belongs to. */
static int place_points_selftest(void)
{
/* (PR-242 census: 21 of the 2076 entry lines carry one,
 * every shipped count is 1). The points come back ABSOLUTE:
 * 0x44e90 adds the placement at load. */
    static const struct { const char *text; int n; int dx[2],
                          dy[2]; const char *what; } pl[] = {
        { "1 33,-3\n", 1, { 33, 0 }, { -3, 0 },
          "ZARIA.01's DRABINKA list" },
        { "1 0,200\n", 1, { 0, 0 }, { 200, 0 },
          "ZARIA.01's LINA list" },
        { "2 -4,0 4,314\n", 2, { -4, 4 }, { 0, 314 },
          "a two-point list (the UZWIERZ ropes)" },
        { "30,50\n", 0, { 0, 0 }, { 0, 0 },
          "the third authored line is not a list" },
        { "23,30;53,160\n", 0, { 0, 0 }, { 0, 0 },
          "the authored rect is not a list" },
        { "1 33,-3 junk\n", 0, { 0, 0 }, { 0, 0 },
          "trailing text: refused, stays classified" },
        { "0 1,2\n", 0, { 0, 0 }, { 0, 0 },
          "a zero count: the engine's empty vector" },
    };
    for (unsigned q = 0; q < sizeof pl / sizeof pl[0]; q++) {
        obj_stats stt;
        lvl_obj o3;
        size_t len = strlen(pl[q].text);
        const char *r;

        memset(&stt, 0, sizeof stt);
        memset(&o3, 0, sizeof o3);
        o3.x = o3.bx = 180;
        o3.y = o3.by = 510;
        r = place_points(pl[q].text, pl[q].text + len, &o3, &stt);
        if (o3.npts != pl[q].n ||
            (pl[q].n > 0 &&
             (o3.cpx[0] != 180 + pl[q].dx[0] ||
              o3.cpy[0] != 510 + pl[q].dy[0] ||
              (pl[q].n > 1 &&
               (o3.cpx[1] != 180 + pl[q].dx[1] ||
                o3.cpy[1] != 510 + pl[q].dy[1])))) ||
            (pl[q].n > 0 ? r == pl[q].text : r != pl[q].text)) {
            fprintf(stderr, "contact-point parse selftest: FAILED the"
                            " contact-point parse of %s: npts %d"
                            " first (%d,%d) consumed %d\n",
                    pl[q].what, o3.npts, o3.cpx[0], o3.cpy[0],
                    r != pl[q].text);
            return 1;
        }
    }

    /* PR-246: the rectangle and pair lines of the same record. */
    {
        static const struct { const char *text; int kind; int v[4];
                             const char *what; } rl[] = {
            { "23,30;53,160\n", 1, { 203, 540, 233, 670 },
              "ZARIA.01's DRABINKA rect (180,510 + 23,30;53,160)" },
            { "23,30;31,200\n", 1, { 203, 540, 211, 710 },
              "ZARIA.01's first LINA rect" },
            { "53,160;23,30\n", 1, { 203, 540, 233, 670 },
              "a reversed rect: sorted, counted model choice" },
            { "23,30;53,160\n", 2, { 0 }, "rect parsed, pair refused" },
            { "30,50\n", 2, { 210, 560 }, "the third authored pair"
              " (180,510 + 30,50): stored ABSOLUTE, as 0x3f187-"
              "0x3f1a7 makes [obj+0xc5] absolute at load" },
            { "23,30;53,160 junk\n", 0, { 0 }, "trailing text: refused" },
            { "23,30\n", 1, { 0 }, "a bare pair is NOT a rect:"
                                    " refused, and it is consumed as"
                                    " the pair below" },
        };
        for (unsigned q = 0; q < sizeof rl / sizeof rl[0]; q++) {
            obj_stats stt2;
            lvl_obj o4;
            size_t len2 = strlen(rl[q].text);
            const char *r2;

            memset(&stt2, 0, sizeof stt2);
            memset(&o4, 0, sizeof o4);
            o4.x = o4.bx = 180;
            o4.y = o4.by = 510;
            if (rl[q].kind == 1)
                r2 = place_rect(rl[q].text, rl[q].text + len2,
                                &o4, &stt2);
            else
                r2 = place_pair(rl[q].text, rl[q].text + len2,
                                &o4, &stt2);
            if (rl[q].kind == 1) {
                int want = rl[q].v[2] > 0;
                if ((o4.nrect != 0) != want ||
                    (want && (o4.arx0 != rl[q].v[0] ||
                              o4.ary0 != rl[q].v[1] ||
                              o4.arx1 != rl[q].v[2] ||
                              o4.ary1 != rl[q].v[3])) ||
                    (want ? r2 == rl[q].text : r2 != rl[q].text)) {
                    fprintf(stderr, "contact-point parse selftest:"
                                    " FAILED the rect parse of %s:\n"
                                    "  nrect %d got (%d,%d)-(%d,%d)"
                                    " consumed %d\n", rl[q].what,
                            o4.nrect, o4.arx0, o4.ary0, o4.arx1,
                            o4.ary1, r2 != rl[q].text);
                    return 1;
                }
            } else {
                if (q == 3) {           /* pair asked of a rect line */
                    if (o4.npair) {
                        fprintf(stderr, "contact-point parse selftest:"
                                        " FAILED: a rect line parsed as"
                                        " a bare pair\n");
                        return 1;
                    }
                } else if (q == 5) {
                    if (o4.npair || r2 != rl[q].text) {
                        fprintf(stderr, "contact-point parse selftest:"
                                        " FAILED: trailing text accepted"
                                        " as a pair\n");
                        return 1;
                    }
                } else if (q == 6) {    /* "23,30" IS a well-formed pair */
                    if (!o4.npair || r2 == rl[q].text) {
                        fprintf(stderr, "contact-point parse selftest:"
                                        " FAILED: `23,30` refused as a"
                                        " pair (that is its grammar)\n");
                        return 1;
                    }
                } else {
                    if (!o4.npair || r2 == rl[q].text) {
                        fprintf(stderr, "contact-point parse selftest:"
                                        " FAILED the pair parse of %s\n",
                                rl[q].what);
                        return 1;
                    }
                    if (rl[q].v[1] &&
                        (o4.apx != rl[q].v[0] || o4.apy != rl[q].v[1])) {
                        fprintf(stderr, "contact-point parse selftest:"
                                        " FAILED the pair VALUES of"
                                        " %s: got (%d,%d) want"
                                        " (%d,%d)\n", rl[q].what,
                                o4.apx, o4.apy, rl[q].v[0], rl[q].v[1]);
                        return 1;
                    }
                }
            }
        }
    }

    return 0;
}

/* L13 (PR-335): the entry's OWN surface table, tested next to the reader
 * (0x45120's record grammar, 0x45200's placement step). Every text line
 * below is quoted verbatim from the .des member named, at that entry's
 * own authored position. The cases that are NOT shipped say so: the '#'
 * separator (no shipped line uses it), the smallest-case ladder list of
 * the ordering test, and the placement of the zero-count block, which
 * belongs to no entry at all. */
static int place_seat_selftest(void)
{
    static const struct { const char *text; int x, y;
                         int want; int px[4], py[4], flag[4];
                         int trail, shortc, norec; const char *what; } sl[] = {
        { "1 7,45;40,45 -6300,-1350\n", 300, 588, 2,
          { 307, 340, 0, 0 }, { 633, 633, 0, 0 }, { 1, 0, 0, 0 },
          1, 0, 0,
          "ZARIA.01 ANIPL2's KATAPULT seat: the only line in the"
          " shipped set whose text continues after its records" },
        { "1 -80,45;-18,45\n", 965, 335, 2,
          { 885, 947, 0, 0 }, { 380, 380, 0, 0 }, { 1, 0, 0, 0 },
          0, 0, 0,
          "ZARIA.01 ANIPL2's second PALMA seat (placement 965,335)" },
        { "1 18,40;80,40\n", 820, 435, 2,
          { 838, 900, 0, 0 }, { 475, 475, 0, 0 }, { 1, 0, 0, 0 },
          0, 0, 0,
          "ZARIA.01 ANIPL2's first PALMA seat" },
        { "1 -80,45;-18,45\n", 1010, 275, 2,
          { 930, 992, 0, 0 }, { 320, 320, 0, 0 }, { 1, 0, 0, 0 },
          0, 0, 0,
          "ZARIA.01 ANIPL2's top PALMA seat (placement 1010,275):"
          " its high end, 992, is 28 px short of the level's authored"
          " exit x = 1020" },
        { "1 -5,0!67,0\n", 389, 61, 2,
          { 384, 456, 0, 0 }, { 61, 61, 0, 0 }, { 0, 0, 0, 0 },
          0, 0, 0,
          "LONA.05 ANIPL3's CEBRZYK seat: '!' is NOT a table end,"
          " it is the flag-0 character (0x451cf-0x451db), and the"
          " loop keeps going (0x451c9 gates on the count only)" },
        { "2 -3,0;42,9;45,12;\n", 110, 131, 3,
          { 107, 152, 155, 0 }, { 131, 140, 143, 0 }, { 1, 1, 0, 0 },
          0, 0, 0,
          "TUNGA.11 ANIPL1's MOVEPLAT at 110,131 and its path:"
          " three records, and the trailing ';' is the literal"
          " 0x451e2 asks for" },
        { "3 0,16;26,2;44,-2;62,-2;\n", 1140, 440, 4,
          { 1140, 1166, 1184, 1202 }, { 456, 442, 438, 438 },
          { 1, 1, 1, 0 }, 0, 0, 0,
          "ZARIA.02 ANIPL2's JBRANCH at 1140,440: four records"
          " from a count of 3 (count + 1, exactly as the .PLT"
          " lines are counted). Its text is shipped; in the file it"
          " sits past `id=`/`R=`/`frame=` lines, which the entry"
          " walk does not step over - see OI-072" },
        { "23,30;53,160\n", 180, 510, -1,
          { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
          0, 0, 0,
          "the authored rect is NOT a table: no count + records" },
        { "30,50\n", 180, 510, -1,
          { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
          0, 0, 0,
          "the bare pair is NOT a table either" },
        { "0 21,1445;\n", 100, 1400, 0,
          { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
          1, 0, 1,
          "a zero count, the first line of KADMAR2 ANI1's block of"
          " 76: 0x45141-0x45143 allocates NO table and what follows"
          " has no reader (that block hangs off the file's `76`"
          " count line, so the (100,1400) here is a fixture, not a"
          " placement)" },
        { "1 0,0#20,0\n", 50, 70, 2,
          { 50, 70, 0, 0 }, { 70, 70, 0, 0 }, { 2, 0, 0, 0 },
          0, 0, 0,
          "the flag-2 separator '#' (0x451cf-0x451d4): NOT in any"
          " shipped line - counted here so the grammar stays tested" },
    };
    for (unsigned q = 0; q < sizeof sl / sizeof sl[0]; q++) {
        obj_stats stt;
        lvl_obj o5;
        size_t len = strlen(sl[q].text);
        const char *r;

        memset(&stt, 0, sizeof stt);
        memset(&o5, 0, sizeof o5);
        o5.x = o5.bx = sl[q].x;
        o5.y = o5.by = sl[q].y;
        r = place_seat_table(sl[q].text, sl[q].text + len, &o5, &stt);
        if (sl[q].want < 0) {           /* this shape belongs to another
                                         * grammar: it must be refused,
                                         * not re-classified here */
            if (r != sl[q].text || stt.seat_lines || o5.nseat) {
                fprintf(stderr, "surface-table parse selftest: FAILED %s:"
                                " the table reader took a line that is not"
                                " its grammar\n", sl[q].what);
                return 1;
            }
            continue;
        }
        if (r == sl[q].text) {
            fprintf(stderr, "surface-table parse selftest: FAILED %s:"
                            " the line was refused\n", sl[q].what);
            return 1;
        }
        if (o5.nseat != sl[q].want ||
            stt.seat_trail != sl[q].trail ||
            stt.seat_short != sl[q].shortc ||
            stt.seat_norec != sl[q].norec) {
            fprintf(stderr, "surface-table parse selftest: FAILED %s:"
                            " nseat %d want %d, trail %d want %d, short"
                            " %d want %d, norec %d want %d\n",
                    sl[q].what, o5.nseat, sl[q].want, stt.seat_trail,
                    sl[q].trail, stt.seat_short, sl[q].shortc,
                    stt.seat_norec, sl[q].norec);
            return 1;
        }
        for (int k = 0; k < sl[q].want; k++) {
            if (o5.seat_x[k] != (short)sl[q].px[k] ||
                o5.seat_y[k] != (short)sl[q].py[k] ||
                o5.seat_flag[k] != (unsigned char)sl[q].flag[k]) {
                fprintf(stderr, "surface-table parse selftest: FAILED"
                                " %s: record %d is (%d,%d) flag %d,"
                                " want (%d,%d) flag %d\n", sl[q].what,
                        k, o5.seat_x[k], o5.seat_y[k],
                        o5.seat_flag[k], sl[q].px[k], sl[q].py[k],
                        sl[q].flag[k]);
                return 1;
            }
        }
        if (sl[q].want == 0 && stt.seat_lines != 1) {
            fprintf(stderr, "surface-table parse selftest: FAILED %s:"
                            " the line was not counted as a table\n",
                    sl[q].what);
            return 1;
        }
    }

    /* The order of the four continuation grammars is part of the law:
     * the engine's record verbs read the contact list, the rect and the
     * pair FIRST (0x3f0f0: 0x44f10, then 0x49f90, then 0x4a0b0), so
     * ZARIA.01's ladder list `1 33,-3` belongs to place_points and never
     * reaches the table reader, even though its text is a degenerate
     * table shape. */
    {
        /* ZARIA.01 ANI2 lines 7 and 11: `1 33,-3` is the DRABINKA's
         * ladder list (authored at 180,510 - the position this
         * fixture uses for all three) and `1 0,200` the LINA's; the
         * third takes the same grammar to its smallest case, which
         * no shipped line uses. */
        static const char *const first[] = {
            "1 33,-3\n", "1 0,200\n", "1 0,0\n",
        };
        static const int fdx[] = { 33, 0, 0 }, fdy[] = { -3, 200, 0 };
        for (unsigned q = 0; q < sizeof first / sizeof first[0]; q++) {
            obj_stats stt;
            lvl_obj o5;
            size_t len = strlen(first[q]);

            memset(&stt, 0, sizeof stt);
            memset(&o5, 0, sizeof o5);
            o5.x = o5.bx = 180;
            o5.y = o5.by = 510;
            if (place_points(first[q], first[q] + len, &o5,
                             &stt) == first[q]) {
                fprintf(stderr, "surface-table parse selftest: FAILED:"
                                " the contact-point list %s was not taken"
                                " by its own grammar\n", first[q]);
                return 1;
            }
            if (o5.npts != 1 || o5.nseat != 0 ||
                o5.cpx[0] != 180 + fdx[q] || o5.cpy[0] != 510 + fdy[q]) {
                fprintf(stderr, "surface-table parse selftest: FAILED:"
                                " %s gave npts %d nseat %d first"
                                " (%d,%d)\n", first[q], o5.npts,
                        o5.nseat, o5.cpx[0], o5.cpy[0]);
                return 1;
            }
        }
    }
    return 0;
}

/* L14 (PR-342): the SURFACE-TABLE MOTION of the object class's +0xa4 verb
 * at the row admit. PR-341 read where that verb runs - 0x52338 calls the
 * row handler [[rec+0x44]+0x18] and 0x52341 is the +0xa4 call, both on the
 * admitted branch of the row law, with eax = [rec+0] the owner - and what
 * it switches on: the state record's row cursor at [rec+0x4], which is
 * this port's o->fi. PALMA's class (the view 0x96c38 its installer stores)
 * holds 0x1e5c0 at that slot: `cmp eax,7` then `mov esi,1` / `mov
 * esi,-1`, handed to 0x45200 as its y argument with x = 0, so the table
 * goes 1 px DOWN while the cursor is under 7 and 1 px UP from cursor 7.
 * KATAPULT's view 0x96d1c holds 0x1e690: `sub eax,3; cmp eax,4; ja` and
 * the jump table at 0x1e670, whose five arms move the table by (+13,+5),
 * (+13,+6), (+13,+10), (+3,+10), (-42,-31) at cursors 3,4,5,6,7 - a path
 * that closes. KAMIEN's view 0x96a88 holds 0x239c0 there and KILOF_PL's
 * 0x96b88 the query 0x45320 (PR-339), so those tables do not move.
 * The fixtures are ZARIA.01's own row lists and tables: PALM.MOV's
 * fourteen rows (thresholds 0,100,100,100,200,200,200,200,200,300,300,
 * 300,300,300) with ANIPL2's `1 18,40;80,40` for the PALMA at 820,435,
 * and KAT.MOV's ten rows (0 then nine of 300) with the KATAPULT's
 * `1 7,45;40,45` at 300,588. Both shipped states open their first row
 * with a `WAIT K_GEN1` clause and carry `seq=`/`sample=`/`smpauto`
 * art-and-sample directives; the clause and those directives are left out
 * here because this port clears o->sig at the top of every step (PR-262),
 * so a WAIT first row never admits in a fixture that raises nothing -
 * PR-333's leg owns that half. Nothing else about the lists is changed. */
static int seat_motion_selftest(void)
{
    static const char palm_mov[] =
        "A_RUN\n\n[A_RUN]\ncontrol=BY_TIME\n"
        "1,0\n2,100\n3,100\n4,100\n5,200\n6,200\n7,200\n8,200\n9,200\n"
        "10,300\n11,300\n12,300\n13,300\n14,300\n";
    static const char kat_mov[] =
        "A_RUN\n\n[A_RUN]\ncontrol=BY_TIME\n"
        "1,0\n2,300\n3,300\n4,300\n5,300\n6,300\n7,300\n8,300\n9,300\n"
        "10,300\n";
    static const struct {
        const char *type;      /* the entry's first field, as authored */
        const char *mov;
        const char *table;
        int x, y, rows;
        int peak_dx, peak_dy;  /* the farthest the table is taken */
        const char *what;
    } fx[] = {
        { "PALMA", palm_mov, "1 18,40;80,40\n", 820, 435, 14, 0, 7,
          "ZARIA.01 ANIPL2's PALMA at 820,435 with its own PALM.MOV"
          " list: 7 px down over cursors 0..6 and back over 7..13, so"
          " the table is never left displaced and never moves in x" },
        { "KATAPULT", kat_mov, "1 7,45;40,45\n", 300, 588, 10, 42, 31,
          "ZARIA.01 ANIPL2's KATAPULT at 300,588 with its own KAT.MOV"
          " list: the five jumps of the table 0x1e670 carry it to"
          " (+42,+31) at cursor 6 and put it back at cursor 7" },
        { "KAMIEN", kat_mov, "1 7,45;40,45\n", 1360, 250, 10, 0, 0,
          "the same list on a KAMIEN, whose class holds 0x239c0 at"
          " +0xa4 and no table verb: its table stays where it was"
          " authored" },
    };

    for (unsigned q = 0; q < sizeof fx / sizeof fx[0]; q++) {
        ll_mov_t *m = calloc(1, sizeof *m);
        obj_script sc;
        lvl_obj o;
        obj_stats stt;
        short ax[OBJ_MAXSEAT], ay[OBJ_MAXSEAT];
        unsigned char af[OBJ_MAXSEAT];
        int dx = 0, dy = 0, peak_dx = 0, peak_dy = 0, it, same;

        memset(&sc, 0, sizeof sc);
        memset(&o, 0, sizeof o);
        memset(&stt, 0, sizeof stt);
        if (!m) {
            fprintf(stderr, "seat motion selftest: out of memory\n");
            return 1;
        }
        if (ll_mov_parse(fx[q].mov, m) <= 0 || m->nst < 1 ||
            m->st[0].nfr != fx[q].rows) {
            fprintf(stderr, "seat motion selftest: FAILED %s: the row"
                            " list did not parse as authored (%d states,"
                            " %d rows)\n", fx[q].what, m->nst,
                    m->nst ? m->st[0].nfr : 0);
            free(m);
            return 1;
        }
        snprintf(sc.leaf, sizeof sc.leaf, "%s", fx[q].type);
        sc.mov = m;
        o.sc = &sc;
        o.x = o.bx = fx[q].x;
        o.y = o.by = fx[q].y;
        place_seat_table(fx[q].table, fx[q].table + strlen(fx[q].table),
                         &o, &stt);
        if (o.nseat != 2 || stt.seat_lines != 1) {
            fprintf(stderr, "seat motion selftest: FAILED %s: the authored"
                            " table line did not give 2 records (nseat"
                            " %d)\n", fx[q].what, o.nseat);
            free(m);
            return 1;
        }
        for (int k = 0; k < o.nseat; k++) {
            ax[k] = o.seat_x[k];
            ay[k] = o.seat_y[k];
            af[k] = o.seat_flag[k];
        }
        for (it = 0; it < 400; it++) {
            obj_advance(&o, LL_ITER_MS);
            dx = o.seat_x[0] - ax[0];
            dy = o.seat_y[0] - ay[0];
            if (dx > peak_dx)
                peak_dx = dx;
            if (dy > peak_dy)
                peak_dy = dy;
            if (o.brk_arm)              /* the list wrapped: one full cycle
                                           of cursors 0..rows-1 is done */
                break;
        }
        same = o.seat_x[1] - ax[1] == dx && o.seat_y[1] - ay[1] == dy;
        if (it >= 400 || !o.brk_arm) {
            fprintf(stderr, "seat motion selftest: FAILED %s: the row list"
                            " never completed a cycle (%d iterations)\n",
                    fx[q].what, it);
            free(m);
            return 1;
        }
        if (peak_dx != fx[q].peak_dx || peak_dy != fx[q].peak_dy ||
            dx != 0 || dy != 0 || !same) {
            fprintf(stderr, "seat motion selftest: FAILED %s: the table"
                            " peaked at (%d,%d) and ended at (%d,%d),"
                            " want peak (%d,%d) and end (0,0)%s\n",
                    fx[q].what, peak_dx, peak_dy, dx, dy, fx[q].peak_dx,
                    fx[q].peak_dy, same ? "" : " (its two records did not"
                                              " move together)");
            free(m);
            return 1;
        }
        for (int k = 0; k < o.nseat; k++) {
            if (o.seat_flag[k] != af[k]) {
                fprintf(stderr, "seat motion selftest: FAILED %s: record"
                                " %d's flag byte changed (%d -> %d) - the"
                                " move verb of 0x45200 writes x and y"
                                " only\n", fx[q].what, k, af[k],
                        o.seat_flag[k]);
                free(m);
                return 1;
            }
        }
        free(m);
    }
    return 0;
}

/* L15 (PR-352): the engine's OWN standing query, run over the seat tables
 * the port already stores. PR-348 measured the sampler 0x455a0, PR-344 the
 * bracket 0x45380 and the gates of 0x45400, PR-349 the point the first
 * clearance is measured at and PR-351 what the aim block's y really is.
 * These functions are that law, instruction for instruction: 0x45380 (the
 * bracket), 0x455a0 (the slope, with the `idiv` truncation) and 0x45400
 * (the flag gate on the FIRST record, the two clearances, the held-surface
 * waiver at 0x45479 and the incumbent at [requester+0xea]).
 * The requester of this call shape IS the ask block: 0x45320 passes
 * ebx = &block[1] at 0x4535f and edx = the same pointer at 0x45369, so the
 * winner triple [obj+0xe6]/[+0xea]/[+0xe2] lives in the caller's structure
 * (PR-344), and the snapshot point 0x45400 samples at 0x45468 is another
 * field of it - the class verb +0x6c is `add eax,0x89; ret` in 186 of 186
 * class views (PR-349, PR-351).
 * PR-362 read who FILLS that snapshot, and it is not a plain copy: the
 * class verb +0x54 is 0x55070 in 124 class tables (the lion's view among
 * them) and 0x53df0 in 31 more (PALMA, KATAPULT, SPLYW). 0x53df0 copies
 * [obj+0]/[obj+4] into [obj+0x89]/[obj+0x8d] (0x53e06-0x53e0d) and then
 * runs the element's anchor shift (0x42980) on the PAIR, `lea
 * esi,[edx+0x89]` … `mov edx,esi` (0x53dfa, 0x53e27); 0x55070 hands the
 * same pair to the aim filler 0x541f0 as its block (0x55084
 * `lea edx,[ebx+0x89]`), and 0x541f0's chain ends in the same 0x42980
 * with edx = that block (0x5423a). THE TWO ARE NOT THE SAME LAW, and L17
 * pins the difference: 0x53df0 has no gate at all, while the 0x541f0 chain
 * is gated - 0x54200 tests [obj+0xb6] bit 2 and, if set, RUNS the chain
 * regardless; otherwise the +0xbc held-surface verb (0x54216) and the
 * standing bit at 0x54218 each skip it. So for every lion the point is
 * anchor-shifted while it holds no surface and is not standing, or whenever
 * bit 0x02 is set; for PALMA, KATAPULT and SPLYW it is anchor-shifted
 * always. Anything here that ever fills a snapshot point has to carry that
 * offset. PR-364 read the COUNTERPART, the class verb +0xb0 - the point
 * provider: every one of the 124 tables that installs 0x55070 installs
 * the SAME handler 0x544f0 at +0xb0, and its three tests
 * (0x54507/0x54516/0x5451f) are the exact negation of this chain's -
 * the pair it hands its caller is the position with the element's LEAVE
 * shift exactly when this chain skips. So an object of those classes
 * keeps BOTH sample points at once, [obj+0x89] here and [obj+0xd6]
 * there, and the class verb +0x58 (0x54eb0, slot +0x58 of all 155
 * tables, one call site) moves the live position by their difference.
 * [view+0x98], which both writers call, is a no-op: 0x23a50 only calls
 * 0x534e0, and 0x534e0 is one instruction, `ret`. PR-365 measured the
 * OTHER family and it is a warning for anything that reuses this code:
 * of the 186 kind tables, 155 are the family above (+0x58 = 0x54eb0 and
 * +0xb0 = 0x544f0) and 31 are the other one - those put 0x53df0 at
 * +0x54 and the plain pair copy 0x23ba0 at +0x58, and their +0xb0 is
 * no provider at
 * all: 26 of the 31 hand back a kind ID, the same two-instruction
 * getter `mov eax, ds:0xID; ret` that +0xc uses engine-wide (PR-367:
 * all 186 anchor tables answer +0xc that way, and the ids come from
 * the counter 0x44350), and in those 31 tables it names ANOTHER
 * class; 0x2b9b0 hands back the singleton object, 0x2dd00 reads
 * [obj+0xbf], two mark the object asked ([obj+0x66] bit 0x40) and
 * 0x4fb00 tests [obj+0x5c]. And +0x54 is NOT part of that split
 * (PR-368): over the same 186 tables it holds 0x55070 in 124,
 * 0x53df0 in 31, 0x3ddf0 in 29 and one-off writers twice. A 0x3ddf0
 * class runs 0x55070 and then a second pass over surface records it
 * owns: [obj+0xee]/[+0xf2]/[+0xf6] is one embedded point-list
 * container of the 0xa0cc0 family - inline, at an offset that is per
 * class (+0xee, +0xa5, +0x44) - whose nine verbs sit at the start of
 * the class's own method array, 0x24 below its table. 37 classes own
 * one, all 29 of the 0x3ddf0 family and none of the 124 0x55070 ones
 * (PR-372). It is filled from authored text by its +0x20 verb, and
 * its 13-byte records are held sorted by x, translated by whatever
 * moves the holder (0x45200) and bracketed by 0x45380 into the pair
 * 0x455a0 measures - and copied whole into every object built from it
 * (PR-369, PR-370, PR-371). Nothing here models that. The legs below
 * model the 0x55070 half of the 155 family only. */
static int seat_bracket(const short *sx, int n, int x)
{
    int i;

    if (!n)                         /* 0x45388 `cmp DWORD PTR [eax+8],0` */
        return 0;
    if (x < sx[0])                  /* 0x45393 `jl 0x453c4`: only the left
                                     * end is tested at all */
        return 0;
    /* 0x453a3-0x453bd: edx starts at 1 and the compare at 0x453b8 is
     * `cmp [edi],[ebx+eax]` with `jg` KEEPING the scan, so the walk stops
     * at the first record whose x is > x and the value returned is that
     * record's index: the pair is (record i, record i-1). Off the end of
     * the array (edx passes the count at 0x4538c) the answer is 0. */
    for (i = 1; i <= n; i++)
        if (x < sx[i])
            return i;
    return 0;
}

static int seat_slope(int ax, int ay, int bx, int by, int x, int py)
{
    /* 0x455a0: (x - A.x) * (B.y - A.y) / (B.x - A.x) + A.y - point.y.
     * C's `/` is the engine's `idiv` (0x455be `sar edx,31`, 0x455c3):
     * truncated TOWARD ZERO, sign of the dividend, so on a falling line
     * the value sits one pixel ABOVE the real one (100 + trunc(-16.5) =
     * 84 at x 33 of (0,100)-(100,50)). Both call sites pass the pair in
     * this order, A = record i-1 and B = record i: the difference between
     * them is only the point in ebx (PR-392, against PR-350/PR-352's
     * "reversed pair"). There is no guard for B.x == A.x: PR-348's open
     * item, none here. */
    return (x - ax) * (by - ay) / (bx - ax) + ay - py;
}

/* The flag byte of record k: the record is 0xd dwords and the byte sits at
 * +0xc (0x451a6-0x451db), which is what the port keeps in seat_flag (L13). */
static int seat_rec_flag(const lvl_obj *o, int k)
{
    return o->seat_flag[k];
}

/* 0x45400. eax = the surface handle (records at +4, count at +8: here a
 * lvl_obj's seat arrays), ebx = the aimed {x, y}, ecx = the subtrahend of
 * 0x45477, measured as ZERO for all six askers (PR-350), and edx = the
 * requester, the same structure for this call shape. held is the object's
 * [obj+0xa9]: the query it is asked about is exempt from the first gate
 * (0x45479). Returns 1 when the query answers, having written the winner
 * triple; best is [requester+0xea], which the first ask leaves at 0.
 * snapx/snapy is the point of the FIRST call, what the asker's class verb
 * +0x6c hands back - 0x23a10, `add eax,0x89; ret`, the clamp sample pair,
 * in all 186 class views (PR-351, PR-392). */
static int seat_ask_core(const lvl_obj *o, int mode, int aimx, int aimy,
                         int snapx, int snapy, int held, int *best)
{
    int i = seat_bracket(o->seat_x, (int)o->nseat, aimx);
    int ax, ay, bx, by, c1, c2;

    if (seat_rec_flag(o, 0) > mode)     /* 0x4540c-0x45419: [handle+4]+0xc */
        return 0;
    if (!i)                             /* 0x4542c-0x4542e */
        return 0;
    /* Both 0x455a0 calls take the pair in the SAME order. The stack dance
     * at 0x45443-0x45464 leaves eax = array + (i-1)*13 and edx = array +
     * i*13, and 0x45481-0x4548f builds exactly the same two operands in
     * registers; only ebx changes, the +0x6c result in the first call and
     * the caller's point in the second (0x4548b `mov ebx,ebp`, the value
     * saved at 0x4540a). PR-352 had the second pair reversed. */
    ax = o->seat_x[i - 1]; ay = o->seat_y[i - 1];   /* A = record i-1 */
    bx = o->seat_x[i]; by = o->seat_y[i];           /* B = record i   */
    c1 = seat_slope(ax, ay, bx, by, snapx, snapy);   /* 0x45468: the +0x6c
                                                      * sample pair */
    if (c1 < 0 && held != 1)            /* 0x45477, 0x45479, 0x4547d: the
                                         * first gate reads the SNAPSHOT,
                                         * and the held surface skips it */
        return 0;
    c2 = seat_slope(ax, ay, bx, by, aimx, aimy);     /* 0x45491: the aim */
    if (c2 > 0 || c2 > *best)           /* 0x45498 `jg` on `test eax,
                                         * eax`, then 0x4549a `cmp eax,
                                         * [edi+0xea]` / 0x454a0 `jg`: the
                                         * candidate is dropped when its
                                         * value EXCEEDS the incumbent, so
                                         * the winner is the MOST NEGATIVE
                                         * c2 - the surface highest above
                                         * the aim line */
        return 0;
    *best = c2;                         /* 0x454a8 */
    return 1;
}

/* The measured half: ZARIA.01's four authored seat tables of ANIPL2.DES,
 * driven through the row lists their classes move on (L14 pins the motion;
 * PR-61 read PALML.MOV as the second PALMA list, and it is the same
 * balanced 14-row shape as PALM.MOV, so it is not repeated here). For each
 * table this records the x span and the TOP its two records cover over one
 * full cycle, and then asks the law above what y of a requester would put
 * it in contact - the only y the two gates of 0x45400 leave open, since
 * they bound the clearance to 0 or 1 (PR-350). */
static int surface_ask_selftest(void)
{
    static const char palm_mov[] =
        "A_RUN\n\n[A_RUN]\ncontrol=BY_TIME\n"
        "1,0\n2,100\n3,100\n4,100\n5,200\n6,200\n7,200\n8,200\n9,200\n"
        "10,300\n11,300\n12,300\n13,300\n14,300\n";
    static const char kat_mov[] =
        "A_RUN\n\n[A_RUN]\ncontrol=BY_TIME\n"
        "1,0\n2,300\n3,300\n4,300\n5,300\n6,300\n7,300\n8,300\n9,300\n"
        "10,300\n";
    static const struct {
        const char *type;
        const char *mov;
        const char *table;
        int x, y;
        int cx0, cx1, top0, top1;  /* the envelope over a whole cycle */
        const char *what;
    } fx[] = {
        { "PALMA", palm_mov, "1 18,40;80,40\n", 820, 435,
          838, 900, 475, 482, "ZARIA.01 ANIPL2's PALMA at 820,435" },
        { "PALMA", palm_mov, "1 -80,45;-18,45\n", 965, 335,
          885, 947, 380, 387, "ZARIA.01 ANIPL2's PALMA at 965,335" },
        { "PALMA", palm_mov, "1 -80,45;-18,45\n", 1010, 275,
          930, 992, 320, 327, "ZARIA.01 ANIPL2's PALMA at 1010,275" },
        { "KATAPULT", kat_mov, "1 7,45;40,45\n", 300, 588,
          307, 382, 633, 664, "ZARIA.01 ANIPL2's KATAPULT at 300,588" },
    };
    int xn[sizeof fx / sizeof fx[0]][2], yn[sizeof fx / sizeof fx[0]][2];
    int q, bad = 0;

    /* (1) THE LAW, case by case. Every expected value below is arithmetic
     * done by hand from the instructions quoted above. Write L for the line
     * value of 0x455a0 with the pair AS THE BRACKET GIVES IT (A = record
     * i-1, B = record i, the same in both calls): the first gate, 0x4547d,
     * requires L >= the SNAPSHOT y - the point the asker's class verb
     * +0x6c returns, [obj+0x89] - and waives that test for the surface at
     * [obj+0xa9]; the second gate, 0x45496/0x454a0, requires the same L at
     * the AIM y to be <= 0 and better than the incumbent. The value
     * `idiv` returns is not the real line: on a falling span it truncates
     * toward zero, so it sits one pixel above it, and the rows the model
     * accepts are exactly the rows the corrected truncation gives - which
     * is what separates these pins from PR-350's and PR-352's. The two
     * gates read two different POINTS of one line, so a query answers when
     * the snapshot is at or under the line and the aim at or above it. */
    {
        static const struct {
            int sx[2], sy[2], flag[2], mode;
            int aimx, aimy, snapx, snapy, held;
            int want, want_best;
            const char *what;
        } tc[] = {
            /* The bracket 0x45380. The left end is INSIDE (0x45393 is
             * `jl`); the right end is NOT: 0x453b8 is `jg`, so x equal to
             * the last record's x keeps the scan walking until it passes
             * the count at 0x4538c and 0x4542e rejects. */
            { { 838, 900 }, { 475, 475 }, { 1, 1 }, 2, 838, 475, 838, 475,
              0, 1, 0, "PALMA's authored span at its FIRST record's x" },
            { { 838, 900 }, { 475, 475 }, { 1, 1 }, 2, 860, 475, 860, 475,
              0, 1, 0, "and at an x in the middle of it" },
            { { 838, 900 }, { 475, 475 }, { 1, 1 }, 2, 899, 475, 899, 475,
              0, 1, 0, "and 1 px left of the last record" },
            { { 838, 900 }, { 475, 475 }, { 1, 1 }, 2, 900, 475, 900, 475,
              0, 0, 0, "and AT the last record's x: the scan runs off the"
              " array" },
            { { 838, 900 }, { 475, 475 }, { 1, 1 }, 2, 837, 475, 837, 475,
              0, 0, 0, "and 1 px left of the first: the ONLY range test the"
              " bracket makes" },
            /* The two gates at a level surface: the value is 475 - y, so the
             * only answerable row of that column is y 475 itself - from 1
             * px below the seat 0x4547d rejects, from 1 px above it
             * 0x45496 does. The standing query needs the requester AT
             * the seat. */
            { { 838, 900 }, { 475, 475 }, { 1, 1 }, 2, 860, 474, 860, 474,
              0, 0, 0, "1 px ABOVE the seat top: the value is +1, so"
              " 0x45496 rejects" },
            { { 838, 900 }, { 475, 475 }, { 1, 1 }, 2, 860, 476, 860, 476,
              0, 0, 0, "1 px BELOW the seat top: the value is -1, so 0x4547d"
              " rejects a surface the requester does not hold" },
            { { 838, 900 }, { 475, 475 }, { 1, 1 }, 2, 860, 476, 860, 475,
              0, 1, -1, "the snapshot ON the seat and the aim 1 px into it:"
              " the two gates read DIFFERENT points, so this answers with"
              " the incumbent -1" },
            { { 838, 900 }, { 475, 475 }, { 1, 1 }, 2, 860, 475, 860, 476,
              0, 0, 0, "and the same pair the other way: -1 at the"
              " snapshot, so it does not" },
            /* The flag gate 0x4540c-0x45417 reads record 0 ONLY. */
            { { 838, 900 }, { 475, 475 }, { 1, 1 }, 0, 860, 475, 860, 475,
              0, 0, 0, "mode 0 against a record 0 whose flag byte is 1" },
            { { 838, 900 }, { 475, 475 }, { 0, 2 }, 2, 860, 475, 860, 475,
              0, 1, 0, "a flag 2 on record 1 only - the authored `;` before"
              " a pair belongs to the record that FOLLOWS it (PR-344)" },
            /* A rising span (0,50)-(100,100). At x 33 the pair the bracket
             * gives is A = record 0, B = record 1, so the value is
             * 50 + trunc(33*50/100) = 50 + 16 = 66 - point.y. ONE row of
             * that column answers, y 66; the two gates separate only when
             * the two points have DIFFERENT y. */
            { { 0, 100 }, { 50, 100 }, { 0, 0 }, 2, 33, 66, 33, 66,
              0, 1, 0, "the rising line AT its own truncated value: both"
              " gates read the same 66, and the incumbent is 0" },
            { { 0, 100 }, { 50, 100 }, { 0, 0 }, 2, 33, 67, 33, 67,
              0, 0, 0, "one row under that line: the first gate 0x4547d sees"
              " -1. PR-352 read this column as answering y 67 too, from a"
              " reversed second pair - 0x45481-0x4548f does not reverse it" },
            { { 0, 100 }, { 50, 100 }, { 0, 0 }, 2, 33, 65, 33, 65,
              0, 0, 0, "one row above it: c2 = +1, 0x45496 refuses" },
            { { 0, 100 }, { 50, 100 }, { 0, 0 }, 2, 33, 68, 33, 68,
              0, 0, 0, "and two rows under it: c1 = -2" },
            { { 0, 100 }, { 50, 100 }, { 0, 0 }, 2, 33, 67, 33, 66,
              0, 1, -1, "the snapshot ON the line and the aim 1 px under it:"
              " the first gate reads the snapshot, so this answers at -1" },
            { { 0, 100 }, { 50, 100 }, { 0, 0 }, 2, 33, 66, 33, 67,
              0, 0, 0, "and the same pair the other way round: the snapshot"
              " is the row 0x4547d tests, so it does not" },
            { { 0, 100 }, { 100, 50 }, { 0, 0 }, 2, 32, 84, 32, 84,
              0, 1, 0, "a FALLING line, exact at x 32: 100 + trunc(-1600/100)"
              " = 84, so that column answers y 84" },
            /* The same falling line at x 33: 100 + trunc(33*-50/100) =
             * 100 + trunc(-16.5). `idiv` truncates TOWARD ZERO, so it is
             * -16 and the value is 84, not 83. A floor division or a
             * round-to-nearest would answer y 83 instead; this is what pins
             * the truncation. */
            { { 0, 100 }, { 100, 50 }, { 0, 0 }, 2, 33, 83, 33, 83,
              0, 0, 0, "1 px above that row: c2 = 84-83 = +1, 0x45496"
              " refuses - truncation toward zero, not floor" },
            { { 0, 100 }, { 100, 50 }, { 0, 0 }, 2, 33, 84, 33, 84,
              0, 1, 0, "at the row itself, x 33: c1 = c2 = 0. PR-352 had"
              " this column EMPTY" },
            { { 0, 100 }, { 100, 50 }, { 0, 0 }, 2, 33, 84, 33, 83,
              0, 1, 0, "the same column with the gates reading DIFFERENT"
              " rows: the snapshot 1 px above the line passes 0x4547d with"
              " +1 and the aim on the line passes 0x45496" },
            { { 0, 100 }, { 100, 50 }, { 0, 0 }, 2, 33, 84, 33, 84,
              1, 1, 0, "the same, with the 0x45479 waiver of the held"
              " surface: here it changes nothing, because c1 is 0 anyway" },
            { { 0, 100 }, { 100, 50 }, { 0, 0 }, 2, 99, 50, 99, 50,
              0, 0, 0, "near the end of the falling span: 100 +"
              " trunc(-4950/100) = 51, so at y 50 the value is +1" },
            { { 0, 100 }, { 100, 50 }, { 0, 0 }, 2, 99, 51, 99, 51,
              0, 1, 0, "and at that row: c1 = c2 = 0. The reversed second"
              " pair of PR-352 answered 50 here, and refused 51" },
            { { 0, 100 }, { 100, 50 }, { 0, 0 }, 2, 100, 50, 100, 50,
              0, 0, 0, "and AT the last record of a span: no bracket at all"
              " (0x4542e)" },
        };
        for (q = 0; q < (int)(sizeof tc / sizeof tc[0]); q++) {
            lvl_obj o;
            int best = 0, got;        /* 0 is the value a requester's
                                       * [obj+0xea] carries before its first
                                       * accepted ask: PR-351 left that
                                       * initialiser open and it is not
                                       * what decides these rows. */

            memset(&o, 0, sizeof o);
            o.nseat = 2;
            for (int k = 0; k < 2; k++) {
                o.seat_x[k] = (short)tc[q].sx[k];
                o.seat_y[k] = (short)tc[q].sy[k];
                o.seat_flag[k] = (unsigned char)tc[q].flag[k];
            }
            got = seat_ask_core(&o, tc[q].mode, tc[q].aimx, tc[q].aimy,
                                tc[q].snapx, tc[q].snapy, tc[q].held,
                                &best);
            if (got != tc[q].want || (tc[q].want && best != tc[q].want_best))
            {
                fprintf(stderr, "surface ask selftest: FAILED %s: the"
                                " query answered %d with the incumbent %d,"
                                " want %d with %d\n", tc[q].what, got, best,
                        tc[q].want, tc[q].want_best);
                bad = 1;
            }
        }
    }

    /* (1b) THE WINNER, and the one way the engine can climb. Two tables,
     * the first at the top of ZARIA.01's lower PALMA and the second at the
     * upper one 95 px higher, both asked at x 890 where both are bracketed
     * at rest. */
    {
        lvl_obj p, a;
        int best = 0, g1, g2;

        memset(&p, 0, sizeof p);
        memset(&a, 0, sizeof a);
        p.nseat = a.nseat = 2;
        p.seat_x[0] = 838; p.seat_x[1] = 900;
        p.seat_y[0] = p.seat_y[1] = 475;
        a.seat_x[0] = 885; a.seat_x[1] = 947;
        a.seat_y[0] = a.seat_y[1] = 380;

        /* asked while the requester stands at y 475: the seat 95 px ABOVE
         * him is not answerable, because the value is 380 - 475 = -95 < 0
         * and the 0x45479 waiver covers ONLY the surface at [obj+0xa9]. */
        if (!seat_ask_core(&p, 2, 890, 475, 890, 475, 0, &best)) {
            fprintf(stderr, "surface ask selftest: FAILED the seat the"
                            " requester is standing on must be answerable"
                            " with the incumbent 0\n");
            bad = 1;
        }
        if (seat_ask_core(&a, 2, 890, 475, 890, 475, 0, &best)) {
            fprintf(stderr, "surface ask selftest: FAILED a seat 95 px"
                            " above the requester: 0x4547d must reject it"
                            " while he does not hold it\n");
            bad = 1;
        }
        /* the waiver: asked about the surface HE HOLDS, the same query
         * answers and takes the incumbent to -95. That is the only path by
         * which 0x45400 can put a requester on a higher surface, and it
         * needs him holding the very table being asked. */
        g1 = seat_ask_core(&a, 2, 890, 475, 890, 475, 1, &best);
        if (!g1 || best != -95) {
            fprintf(stderr, "surface ask selftest: FAILED the 0x45479"
                            " waiver: holding the upper table must answer it"
                            " with the incumbent 380 - 475 = -95, got %d/%d"
                            "\n", g1, best);
            bad = 1;
        }
        /* and the ranking of 0x454a0 in the other order: the candidate is
         * dropped when its value EXCEEDS the incumbent, so the table whose
         * top is 95 px higher keeps it - the standing query answers with
         * the highest surface at or above the aim line. */
        best = 0;
        g1 = seat_ask_core(&a, 2, 890, 475, 890, 475, 1, &best);
        g2 = seat_ask_core(&p, 2, 890, 475, 890, 475, 0, &best);
        if (!g1 || g2 || best != -95) {
            fprintf(stderr, "surface ask selftest: FAILED the ranking of"
                            " 0x454a0: the upper table must hold the"
                            " incumbent -95 against the lower one, whose"
                            " c2 = 0 exceeds it (answers %d,%d, incumbent"
                            " %d)\n", g1, g2, best);
            bad = 1;
        }
    }

    /* (2) THE MEASUREMENT: the four authored tables under their motion. */
    for (q = 0; q < (int)(sizeof fx / sizeof fx[0]); q++) {
        ll_mov_t *m = calloc(1, sizeof *m);
        obj_script sc;
        lvl_obj o;
        obj_stats stt;
        int it, lo = 0, hi = 0, xlo = 0, xhi = 0, o0x, o1x, o0y, flat = 1;

        memset(&sc, 0, sizeof sc);
        memset(&o, 0, sizeof o);
        memset(&stt, 0, sizeof stt);
        if (!m) {
            fprintf(stderr, "surface ask selftest: out of memory\n");
            return 1;
        }
        if (ll_mov_parse(fx[q].mov, m) <= 0 || m->nst < 1) {
            fprintf(stderr, "surface ask selftest: FAILED %s: the row list"
                            " did not parse\n", fx[q].what);
            free(m);
            return 1;
        }
        snprintf(sc.leaf, sizeof sc.leaf, "%s", fx[q].type);
        sc.mov = m;
        o.sc = &sc;
        o.x = o.bx = fx[q].x;
        o.y = o.by = fx[q].y;
        place_seat_table(fx[q].table, fx[q].table + strlen(fx[q].table),
                         &o, &stt);
        if (o.nseat != 2) {
            fprintf(stderr, "surface ask selftest: FAILED %s: the authored"
                            " table line did not give 2 records (nseat %d)\n",
                    fx[q].what, o.nseat);
            free(m);
            return 1;
        }
        o0x = o.seat_x[0]; o1x = o.seat_x[1]; o0y = o.seat_y[0];
        lo = hi = o.seat_y[0];
        xlo = o.seat_x[0]; xhi = o.seat_x[1];
        for (it = 0; it < 400; it++) {
            obj_advance(&o, LL_ITER_MS);
            if (o.seat_y[0] != o.seat_y[1])
                flat = 0;         /* a tilted pair: the value depends on x */
            if (o.seat_y[0] < lo)
                lo = o.seat_y[0];
            if (o.seat_y[0] > hi)
                hi = o.seat_y[0];
            if (o.seat_x[0] < xlo)
                xlo = o.seat_x[0];
            if (o.seat_x[1] > xhi)
                xhi = o.seat_x[1];
            if (o.brk_arm)
                break;
        }
        if (it >= 400 || !o.brk_arm) {
            fprintf(stderr, "surface ask selftest: FAILED %s: the row list"
                            " never completed a cycle (%d iterations)\n",
                    fx[q].what, it);
            free(m);
            return 1;
        }
        xn[q][0] = xlo; xn[q][1] = xhi;
        yn[q][0] = lo; yn[q][1] = hi;
        if (xlo != fx[q].cx0 || xhi != fx[q].cx1 || lo != fx[q].top0 ||
            hi != fx[q].top1) {
            fprintf(stderr, "surface ask selftest: FAILED %s: over one full"
                            " cycle its records cover x %d..%d with the top"
                            " at y %d..%d, want x %d..%d and y %d..%d\n",
                    fx[q].what, xlo, xhi, lo, hi, fx[q].cx0, fx[q].cx1,
                    fx[q].top0, fx[q].top1);
            bad = 1;
        }
        printf("surface ask: %s authored at (%d,%d): its two records stand"
               " at x %d..%d y %d at rest, and every sampled row pair of"
               " its motion carries the SAME y in both records (%s), so over"
               " one full cycle the table covers x %d..%d with one value"
               " at its top, y %d..%d. 0x45400 answers such a table at an x in"
               " its span for ONE row: the requester's snapshot y must be <="
               " that top and his aim y >= it, and for a level table those"
               " two bounds meet at the top row itself\n", fx[q].type,
               fx[q].x, fx[q].y, o0x, o1x, o0y, flat ? "level" : "NOT level",
               xlo, xhi, lo, hi);
        free(m);
    }
    for (int p = 0; p < (int)(sizeof fx / sizeof fx[0]); p++) {
        for (int r = p + 1; r < (int)(sizeof fx / sizeof fx[0]); r++) {
            int ox = xn[p][0] <= xn[r][1] && xn[r][0] <= xn[p][1];
            int sep = 0;            /* gap between the two top bands */

            if (yn[p][1] < yn[r][0])
                sep = yn[r][0] - yn[p][1];
            else if (yn[r][1] < yn[p][0])
                sep = yn[p][0] - yn[r][1];
            printf("surface ask: %s at (%d,%d) -> %s at (%d,%d): over their"
                   " cycles their x spans %s and their top bands y %d..%d"
                   " and y %d..%d are %d px apart. 0x45400 answers a table"
                   " only where its top is AT the requester's own row, and"
                   " 0x45479 waives that bound for the single surface at"
                   " [obj+0xa9]; so the standing query is %s a carrier of"
                   " that step\n", fx[p].type, fx[p].x, fx[p].y,
                   fx[r].type, fx[r].x, fx[r].y,
                   ox ? "overlap" : "never overlap", yn[p][0], yn[p][1],
                   yn[r][0], yn[r][1], sep, (ox && !sep) ? "" : "NOT");
        }
    }
    return bad;
}

/* L16 (PR-359): the element's OWN land and leave methods - 0x42980 and
 * 0x429d0, slots +0x04 and +0x08 of the block 0xa0c08 that every .SEQ
 * element record carries at its last field +0x33 (PR-357 read the chain:
 * PR-353's class +0xB4/+0xB8 verbs, PR-354's four runners, the +0x54
 * frame step 0x53df0, the ask block 0x541f0, the +0xB0 provider 0x544d0).
 * No window and no game data. One synthetic blob, three elements, each of
 * them shape-only with ZERO rows (w0 = 2, b19 = b15 = b17 = 0) - the one
 * element shape the grammar reads as nothing but its 8 words: no margin
 * rows (the reader at 0x41da0 gets a count of 0) and no art arrays (the
 * array loop is skipped when w0&2 is set and w0&1 is not). The FIRST
 * keeps the WORD 0x7fff the element initializer 0x41ef3 stamps at +0, the
 * SECOND declares both anchors (f0,f2) = (10,20) and (f4,f6) = (30,8),
 * the THIRD a second anchor straight ABOVE its first with the same x.
 * The first block also pins the parser side, which the landing law needs:
 * f4 and f6 are read from the blob at 0x41fb9 and 0x41fd0 and nothing in
 * the grammar hides them. The last block joins the two halves the ledger
 * has kept apart so far - PR-357's step and PR-352's standing query (L15)
 * - by asking the same law from the position BEFORE and AFTER a landing. */
static int seq_landshift_selftest(void)
{
    static const unsigned char blob[] = {
        0x03, 0x00,                                   /* count 3            */
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* w0 b19 b15 b17 */
        0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* 7fff f2 f4 f6  */
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* w0 b19 b15 b17 */
        0x0a, 0x00, 0x14, 0x00, 0x1e, 0x00, 0x08, 0x00,   /* (10,20) (30,8) */
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* w0 b19 b15 b17 */
        0x0a, 0x00, 0x0c, 0x00, 0x0a, 0x00, 0x00, 0x00,   /* (10,12) (10,0) */
    };
    static const struct { int e; unsigned f0, f2, f4, f6; } fields[] = {
        { 0, 0x7fff, 0, 0, 0 }, { 1, 10, 20, 30, 8 }, { 2, 10, 12, 10, 0 },
    };
    /* (x, y, facing, want_x, want_y) for the element at index 1:
     * dy = f2 - f6 = 12 taken off y, dx = f0 - f4 = -20, and 0x429a0
     * sends the X leg the other way as soon as the facing is not
     * positive - that is the whole mirroring. */
    static const struct { int x, y, face, wx, wy; } steps[] = {
        { 100, 100, 1, 120, 88 },   /* facing right: 0x429ab sub of -20   */
        { 100, 100, -1, 80, 88 },   /* facing left:  0x429bd add of -20   */
        { 100, 100, 0, 80, 88 },    /* test ebx,ebx / jle: 0 is LEFT      */
    };
    int bad = 0, i, best, x, y, answered;
    int upx = 0, upy = 0, rgt_x = 0, rgt_y = 0, lft_x = 0, lft_y = 0;
    ll_seq sq;
    lvl_obj a;

    if (ll_seq_parse(blob, (uint32_t)sizeof blob, &sq) != 0) {
        fprintf(stderr, "land shift selftest: FAILED the synthetic .SEQ did"
                        " not parse\n");
        return 1;
    }
    if (sq.count != 3) {
        fprintf(stderr, "land shift selftest: FAILED %u elements, want 3\n",
                sq.count);
        ll_seq_free(&sq);
        return 1;
    }
    if (sq.consumed != (uint32_t)sizeof blob) {   /* the element IS 8 words
                                                   * here: no margins, no
                                                   * arrays (PR-357 read the
                                                   * reader's branches) */
        fprintf(stderr, "land shift selftest: FAILED the grammar consumed"
                        " %u of %d bytes\n", sq.consumed, (int)sizeof blob);
        ll_seq_free(&sq);
        return 1;
    }
    for (i = 0; i < 3; i++) {
        const ll_seq_elem *e = &sq.els[fields[i].e];
        if (e->f0 != fields[i].f0 || e->f2 != fields[i].f2 ||
            e->f4 != fields[i].f4 || e->f6 != fields[i].f6) {
            fprintf(stderr, "land shift selftest: FAILED element %d parsed"
                            " f0=%u f2=%u f4=%u f6=%u (want %u %u %u %u)"
                            " - 0x41fb9/0x41fd0 read the second anchor\n",
                    fields[i].e, e->f0, e->f2, e->f4, e->f6, fields[i].f0,
                    fields[i].f2, fields[i].f4, fields[i].f6);
            bad = 1;
        }
    }

    /* (1) THE SENTINEL MOVES NOTHING. 0x42984-0x4298d is `movsx
     * ecx,[eax]`, `cmp ecx,0x7fff`, `je` to the epilogue with no write at
     * all, and 0x429d7-0x429dd the same in the leave method: an element
     * that declares no first anchor cannot move the position by an anchor
     * delta, so EVERY shipped element of that kind lands in place. */
    for (i = 0; i < 4 && !bad; i++) {
        int face = (i & 1) ? -1 : 1;
        x = 100, y = 100;
        ll_seq_anchor_land(&sq.els[0], face, &x, &y);
        if (x != 100 || y != 100) {
            fprintf(stderr, "land shift selftest: FAILED a 0x7fff element's"
                            " land method moved the position to (%d,%d)\n",
                            x, y);
            bad = 1;
            continue;
        }
        x = 100, y = 100;               /* the methods are checked ONE AT A
                                         * TIME: land and leave cancel each
                                         * other, so running the pair would
                                         * hide a missing guard */
        ll_seq_anchor_leave(&sq.els[0], face, &x, &y);
        if (x != 100 || y != 100) {
            fprintf(stderr, "land shift selftest: FAILED a 0x7fff element's"
                            " leave method moved the position to (%d,%d)\n",
                            x, y);
            bad = 1;
        }
    }

    /* (2) THE STEP AND ITS EXACT INVERSE. */
    for (i = 0; i < (int)(sizeof steps / sizeof steps[0]) && !bad; i++) {
        x = steps[i].x, y = steps[i].y;
        ll_seq_anchor_land(&sq.els[1], steps[i].face, &x, &y);
        if (x != steps[i].wx || y != steps[i].wy) {
            fprintf(stderr, "land shift selftest: FAILED landing facing %d"
                            " at (%d,%d) gave (%d,%d), want (%d,%d)\n",
                    steps[i].face, steps[i].x, steps[i].y, x, y,
                    steps[i].wx, steps[i].wy);
            bad = 1;
            continue;
        }
        ll_seq_anchor_leave(&sq.els[1], steps[i].face, &x, &y);
        if (x != steps[i].x || y != steps[i].y) {
            fprintf(stderr, "land shift selftest: FAILED leaving facing %d"
                            " did not return (%d,%d) but (%d,%d)\n",
                    steps[i].face, steps[i].x, steps[i].y, x, y);
            bad = 1;
        }
    }

    /* (3) THE FACING IS READ AT EACH CALL, NOT REMEMBERED. The engine
     * passes ebx fresh every time - [obj+0x6d] in 0x238b0/0x23880, the
     * +0x50 verb in the other six sites - so turning around while
     * standing does NOT undo the landing: the X leg is added twice. That
     * is a consequence of the reading, and the port must reproduce it. */
    x = 100, y = 100;
    ll_seq_anchor_land(&sq.els[1], 1, &x, &y);
    ll_seq_anchor_leave(&sq.els[1], -1, &x, &y);
    printf("land shift: element (f0,f2)=(10,20) (f4,f6)=(30,8): landing"
           " facing right steps (100,100) to (%d,%d) and leaving facing"
           " LEFT ends at (%d,%d) - the y leg cancels, the x leg does not,"
           " because 0x429a0 takes the sign from the caller at each call.\n",
           120, 88, x, y);
    if (x != 140 || y != 100) {
        fprintf(stderr, "land shift selftest: FAILED a land-right/leave-left"
                        " pair gave (%d,%d), want (140,100)\n", x, y);
        bad = 1;
    }

    /* (4) AND WHAT THAT DOES TO THE STANDING QUERY. One seat table 12 px
     * ABOVE the requester's row, asked from the row itself: L15's first
     * gate (0x45477 with the 0x45479 waiver only for [obj+0xa9]) refuses
     * it, because seat_slope is surface_y - point_y = 463 - 475 = -12.
     * Landing through the THIRD element, whose second anchor is 12 px
     * above its first and whose x delta is 0, moves the point the ask is
     * made from onto the seat and the same law answers. Then the x leg,
     * mirrored: the same landing through element 1 with facing right also
     * steps 20 px along x, past the end of the table, where 0x4538c's
     * bracket finds no record pair and the ask is refused - facing left
     * it stays inside and answers. So the landing step is not cosmetic:
     * it chooses which surface the standing query can even see. */
    memset(&a, 0, sizeof a);
    a.nseat = 2;
    a.seat_x[0] = 838, a.seat_x[1] = 900;
    a.seat_y[0] = a.seat_y[1] = 463;
    best = 0;
    if (seat_ask_core(&a, 2, 890, 475, 890, 475, 0, &best)) {
        fprintf(stderr, "land shift selftest: FAILED the ask answered a seat"
                        " 12 px above the requester's row before any"
                        " landing\n");
        bad = 1;
    }
    x = 890, y = 475;
    ll_seq_anchor_land(&sq.els[2], 1, &x, &y);
    best = 0;
    answered = seat_ask_core(&a, 2, x, y, x, y, 0, &best);
    if (x != 890 || y != 463 || !answered) {
        fprintf(stderr, "land shift selftest: FAILED the ask did not answer"
                        " from the landed point (%d,%d)\n", x, y);
        bad = 1;
    }
    upx = x, upy = y;
    ll_seq_anchor_leave(&sq.els[2], 1, &x, &y);
    best = 0;
    if (x != 890 || y != 475 || seat_ask_core(&a, 2, x, y, x, y, 0, &best)) {
        fprintf(stderr, "land shift selftest: FAILED leaving did not put the"
                        " ask back where it was refused (%d,%d)\n", x, y);
        bad = 1;
    }
    x = 890, y = 475;
    ll_seq_anchor_land(&sq.els[1], 1, &x, &y);
    rgt_x = x, rgt_y = y;
    best = 0;
    if (x != 910 || y != 463 || seat_ask_core(&a, 2, x, y, x, y, 0, &best)) {
        fprintf(stderr, "land shift selftest: FAILED the ask from the"
                        " facing-right landing (%d,%d) answered, want refused"
                        " (0x4538c: past the last record there is no pair)\n",
                x, y);
        bad = 1;
    }
    x = 890, y = 475;
    ll_seq_anchor_land(&sq.els[1], -1, &x, &y);
    lft_x = x, lft_y = y;
    best = 0;
    if (x != 870 || y != 463 || !seat_ask_core(&a, 2, x, y, x, y, 0, &best)) {
        fprintf(stderr, "land shift selftest: FAILED the mirrored landing"
                        " through (%d,%d) did not answer\n", x, y);
        bad = 1;
    }
    printf("land shift: a seat table (838,463)-(900,463) asked from (890,475)"
           " is refused (seat_slope = 463-475 = -12 at 0x45477). Landing"
           " through the element (f0,f2)=(10,12) (f4,f6)=(10,0) asks from"
           " (%d,%d) instead and it ANSWERS; leaving puts (890,475) back and"
           " it is refused again. The X leg is the mirrored one: the same"
           " landing through (10,20)/(30,8) facing right ends at (%d,%d),"
           " outside the table's last record, so 0x45380 names no pair and"
           " the ask is refused, while facing left it ends at (%d,%d) and"
           " answers. The landing step therefore decides WHICH surface the"
           " standing query can see at all.\n",
           upx, upy, rgt_x, rgt_y, lft_x, lft_y);

    ll_seq_free(&sq);
    return bad;
}

/* L17 (PR-363): the POINT the standing query measures. PR-349 read the
 * class verb +0x6c as `add eax,0x89; ret` and PR-352 measured that
 * 0x45400's first clearance (c1) is taken at the pair it returns; PR-362
 * read who writes that pair. The class verb +0x54 has two implementations
 * in the image and they are NOT the same law:
 *   0x53df0, in exactly 31 class tables (PALMA 0x96c8c, KATAPULT 0x96d70,
 *     SPLYW 0x96e30 and 28 more): 0x53e00 calls [view+0x98], 0x53e06-
 *     0x53e0d copies [obj+0]/[obj+4] into [obj+0x89]/[obj+0x8d], then
 *     [view+0x48] for the current element and [view+0x50] for the sign,
 *     and 0x53e27 `mov edx,esi` + 0x53e2b `call [edi+0x4]` apply the
 *     element's anchor shift to the PAIR. No gate of any kind.
 *   0x55070, in 124 dwords of the image - every lion's view among them
 *     (0x96960+0x54 = 0x969b4): 0x55084 `lea edx,[ebx+0x89]` and 0x5508c
 *     `call 0x541f0` run the ask's own aim filler over the same pair, and
 *     THAT chain is gated: 0x541f6-0x541fd copy the position first and
 *     unconditionally, then 0x54200 `test BYTE PTR [obj+0xb6],0x2` with
 *     0x54207 `jne 0x54221` makes the `lock2` flag FORCE the chain past
 *     both predicates, and otherwise 0x5420e's +0xbc held-surface verb
 *     (0x23870 = `[obj+0xa9] != 0` for the lions, PR-350) and 0x54218's
 *     standing-bit test each skip it. 0x55096 then calls [view+0x98]
 *     AFTERWARDS, where the other writer calls it before.
 * So "the sample point is anchor-shifted exactly when the object holds no
 * surface" - the sentence PR-362 wrote - is the 124-table law only. No
 * window and no game data: the same three shape-only elements L16 builds
 * (w0 = 2, no margin rows, no art arrays), element 0 keeping the 0x7fff
 * the element initializer 0x41ef3 stamps at +0. */
static int seat_chain_runs(int b6, int holds)
{
    if (b6 & 0x02)                  /* 0x54207 `jne 0x54221` */
        return 1;
    if (holds)                      /* 0x54214 `test eax,eax`, 0x54216 `jne
                                     * 0x5423d` */
        return 0;
    return (b6 & 0x01) == 0;        /* 0x54218, 0x5421f `jne 0x5423d` */
}

/* 0x53df0: copy, then shift. Returns through sx/sy what [obj+0x89] and
 * [obj+0x8d] would hold. */
static void seat_sample_direct(const ll_seq_elem *e, int facing, int x, int y,
                               int *sx, int *sy)
{
    *sx = x;                        /* 0x53e06-0x53e08 */
    *sy = y;                        /* 0x53e0a-0x53e0d */
    ll_seq_anchor_land(e, facing, sx, sy);  /* 0x53e27, 0x53e2b */
}

/* 0x55070 -> 0x541f0: copy, then shift ONLY if the chain runs. */
static void seat_sample_chain(int b6, int holds, const ll_seq_elem *e,
                              int facing, int x, int y, int *sx, int *sy)
{
    *sx = x;                        /* 0x541f6-0x541f8, before any test */
    *sy = y;                        /* 0x541fa-0x541fd */
    if (seat_chain_runs(b6, holds))
        ll_seq_anchor_land(e, facing, sx, sy);    /* 0x5423a */
}

static int seat_sample_selftest(void)
{
    static const unsigned char blob[] = {
        0x03, 0x00,                                   /* count 3            */
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* w0 b19 b15 b17 */
        0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* 7fff f2 f4 f6  */
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* w0 b19 b15 b17 */
        0x0a, 0x00, 0x14, 0x00, 0x1e, 0x00, 0x08, 0x00,   /* (10,20) (30,8) */
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   /* w0 b19 b15 b17 */
        0x0a, 0x00, 0x0c, 0x00, 0x0a, 0x00, 0x00, 0x00,   /* (10,12) (10,0) */
    };
    /* (x,y) is the requester's OWN position; the pair the writer produces is
     * what the next ask measures. Element 2 steps (0,-12), element 1 steps
     * (+20,-12) facing right and (-20,-12) facing left, and 0x42980's guard
     * leaves element 0 alone. */
    static const struct {
        int chain, b6, holds, e, face, x, y, wx, wy;
        const char *what;
    } tc[] = {
        { 0, 0, 0, 2,  1, 890, 475, 890, 463, "the ungated writer, free" },
        { 0, 0, 1, 2,  1, 890, 475, 890, 463, "the ungated writer WHILE"
          " HOLDING a surface: no predicate exists in 0x53df0" },
        { 0, 1, 0, 2,  1, 890, 475, 890, 463, "the ungated writer WHILE"
          " STANDING (bit 0x01): the same, no test" },
        { 0, 0, 0, 1,  1, 818, 475, 838, 463, "the x leg facing right" },
        { 0, 0, 0, 1, -1, 818, 475, 798, 463, "the x leg mirrored" },
        { 0, 0, 0, 0,  1, 890, 475, 890, 475, "a 0x7fff element moves the"
          " sample point not at all (0x42987-0x4298d)" },
        { 1, 0, 0, 2,  1, 890, 475, 890, 463, "the gated writer, free:"
          " 0x54216 and 0x5421f both fall through" },
        { 1, 0, 1, 2,  1, 890, 475, 890, 475, "the gated writer while"
          "[obj+0xa9] names a surface: 0x54216 skips the chain" },
        { 1, 1, 0, 2,  1, 890, 475, 890, 475, "the gated writer while"
          " standing: 0x5421f skips it" },
        { 1, 2, 1, 2,  1, 890, 475, 890, 463, "bit 0x02 of [obj+0xb6]"
          " FORCES the chain past the held-surface predicate (0x54207)" },
        { 1, 3, 1, 2,  1, 890, 475, 890, 463, "bit 0x02 forces it past the"
          " standing test too" },
        { 1, 0, 0, 0,  1, 890, 475, 890, 475, "the sentinel through the"
          " gated writer" },
        { 1, 0, 0, 1, -1, 818, 475, 798, 463, "the gated writer mirrors the"
          " x leg the same way (ebx from [view+0x50], not from [obj+0x6d])" },
    };
    int bad = 0, i, best, sx, sy, got;
    ll_seq sq;
    lvl_obj o;

    if (ll_seq_parse(blob, (uint32_t)sizeof blob, &sq) != 0 ||
        sq.count != 3) {
        fprintf(stderr, "seat sample selftest: FAILED the synthetic .SEQ"
                        " did not parse\n");
        return 1;
    }

    for (i = 0; i < (int)(sizeof tc / sizeof tc[0]); i++) {
        const ll_seq_elem *e = &sq.els[tc[i].e];
        if (tc[i].chain)
            seat_sample_chain(tc[i].b6, tc[i].holds, e, tc[i].face,
                              tc[i].x, tc[i].y, &sx, &sy);
        else
            seat_sample_direct(e, tc[i].face, tc[i].x, tc[i].y, &sx, &sy);
        if (sx != tc[i].wx || sy != tc[i].wy) {
            fprintf(stderr, "seat sample selftest: FAILED %s: the sample"
                            " point from (%d,%d) is (%d,%d), want (%d,%d)\n",
                    tc[i].what, tc[i].x, tc[i].y, sx, sy, tc[i].wx, tc[i].wy);
            bad = 1;
        }
    }

    /* THE ASK, measured there. One seat table 12 px above the requester's
     * row, asked with the AIM at his own row throughout: the only thing
     * that moves is the pair 0x53df0/0x55070 write. Holding a surface
     * closes the gated chain, and the first clearance is 463 - 475 = -12 at
     * 0x45477, so the ask is refused. Setting bit 0x02 - the `lock2` flag
     * PR-354 read as the `2` variants' - forces the chain, the sample point
     * lands on the seat, c1 is 0 and the ask ANSWERS with the incumbent
     * -12. No position was ever changed: this is the same seat table L16
     * reaches by stepping the object, reached here by moving only the point
     * the query measures. */
    memset(&o, 0, sizeof o);
    o.nseat = 2;
    o.seat_x[0] = 838, o.seat_x[1] = 900;
    o.seat_y[0] = o.seat_y[1] = 463;
    seat_sample_chain(0, 1, &sq.els[2], 1, 890, 475, &sx, &sy);
    best = 0;
    got = seat_ask_core(&o, 2, 890, 475, sx, sy, 0, &best);
    if (sx != 890 || sy != 475 || got) {
        fprintf(stderr, "seat sample selftest: FAILED the ask from the"
                        " gated sample (%d,%d) answered %d, want refused\n",
                sx, sy, got);
        bad = 1;
    }
    seat_sample_chain(0x02, 1, &sq.els[2], 1, 890, 475, &sx, &sy);
    best = 0;
    got = seat_ask_core(&o, 2, 890, 475, sx, sy, 0, &best);
    if (sx != 890 || sy != 463 || !got || best != -12) {
        fprintf(stderr, "seat sample selftest: FAILED the ask from the"
                        " bit-0x02 sample (%d,%d) answered %d with the"
                        " incumbent %d, want 1 with -12\n", sx, sy, got,
                best);
        bad = 1;
    }
    /* And the x leg, at the sample point only: facing right puts the point
     * on the table's FIRST record, which the bracket of 0x45380 accepts,
     * and facing left puts it 40 px left of that record, where 0x45393's
     * `jl` answers no pair at all. The requester's position is (818,475)
     * in both cases - he never moves. */
    seat_sample_direct(&sq.els[1], 1, 818, 475, &sx, &sy);
    best = 0;
    got = seat_ask_core(&o, 2, sx, sy, sx, sy, 0, &best);
    if (sx != 838 || sy != 463 || !got || best != 0) {
        fprintf(stderr, "seat sample selftest: FAILED the ask from the"
                        " right-facing sample (%d,%d) answered %d with %d,"
                        " want 1 with 0\n", sx, sy, got, best);
        bad = 1;
    }
    seat_sample_direct(&sq.els[1], -1, 818, 475, &sx, &sy);
    best = 0;
    got = seat_ask_core(&o, 2, sx, sy, sx, sy, 0, &best);
    if (sx != 798 || sy != 463 || got) {
        fprintf(stderr, "seat sample selftest: FAILED the ask from the"
                        " left-facing sample (%d,%d) answered %d, want 0 -"
                        " 0x45393 rejects an x below the first record\n",
                sx, sy, got);
        bad = 1;
    }

    printf("seat sample: the class verb +0x54 is written twice. 0x53df0 (31"
           " class tables: PALMA, KATAPULT, SPLYW) copies (890,475) into the"
           " pair and applies the element's anchor delta there, giving"
           " (890,463), whatever the object holds or claims. 0x55070 (124"
           " tables, every lion) runs the ask's chain over the same pair and"
           " is gated: holding a surface or standing keeps the copy at"
           " (890,475), while bit 0x02 of [obj+0xb6] forces the chain and"
           " gives (890,463). A seat table (838,463)-(900,463) asked with"
           " its first clearance at the first pair is refused and at the"
           " second ANSWERS with the incumbent -12 - and the x leg alone"
           " decides whether 0x45380 names a pair at all: facing right the"
           " sample is (838,463) and the ask answers, facing left it is"
           " (798,463) and it does not. No position moved in any of these.\n");

    ll_seq_free(&sq);
    return bad;
}

/* read one placement file: every line is classified, nothing is skipped in
 * silence and nothing is invented */
/* PR-328: the rectangle-class placement line, `TOKEN,x0,y0;x1,y1`.
 * PR-327 read the eight class tables whose +0x18 record verb is the SAME
 * three instructions: 0x40c90 `add eax,0x3c; call 0x49f90; ret`, where
 * 0x49f90 pushes eax, eax+4, eax+8, eax+0xc and the format 0x91d00
 * `%d,%d;%d,%d , ` to the scan helper 0x49920. Such a line therefore IS
 * the object: its first field names the class - the exact strings the
 * engine's installer compares (0x90741 KILOF_PL, 0x90a40 BRECT, 0x90a46
 * BICZRECT, 0x90a4f WSPRECT, 0x91291 KRA1, 0x91f75 RECT, 0x91f7a DRECT,
 * 0x91f80 DRECT2) - and its four numbers are written straight into the
 * box at [obj+0x3c]. Two differences from the entry grammar the port
 * already knows, both read, both modelled here:
 *   - 0x40c90 adds NOTHING. The ladder's own record verb 0x3f0f0 reads the
 *     same four-number shape and then adds the placement (0x3f124-0x3f14b,
 *     PR-246), so place_rect keeps those numbers offset-from-placement;
 *     this verb has no such step, so these numbers are ABSOLUTE as
 *     authored. ZARIA.01's box 988,645;1340,750 contains that level's
 *     exit point 1020,692 (PR-317), and TUNGA.11_ANI2 line 110 authors a
 *     box whose x0 is -10, so no world-bounds rule may drop one either.
 *   - the numbers are stored UNSORTED, exactly as written. Sorting is not
 *     part of this verb; an authored box that runs backwards simply
 *     matches nothing under the element verb 0x23e00, which is the
 *     engine's behaviour and stays the engine's.
 * KRA2 is NOT in this list: its shipped lines (LONA.05_ANIPL2, eight of
 * them) are five-field ART placements whose box sits in a following
 * `1 -4,0;37,0` row, the entry grammar's own shape (PR-327). ZONE is not
 * either: its +0x18 verb is 0x41c50, which reads an extra `id= %d`. */
static const char *const rect_class_tokens[] = {
    "RECT", "DRECT", "DRECT2", "KILOF_PL",
    "BICZRECT", "BRECT", "WSPRECT", "KRA1"
};

static int rect_class_id(const char *tok)
{
    for (size_t k = 0; k < sizeof rect_class_tokens
                        / sizeof rect_class_tokens[0]; k++)
        if (ci_eq(tok, rect_class_tokens[k]))
            return (int)k;
    return -1;
}

/* The class record for one of those tokens. It has no .MOV, no states and
 * no art: the line is the whole object, so this is the same inert script
 * shape a placement with an unreachable .MOV already gets (obj_script's
 * mov = NULL), and every runtime path that gates on sc->mov steps over it.
 * ONE record per class, many instances - the engine keeps one table per
 * class and one instance per line. */
static obj_script *obj_rect_class_get(place_ctx *pc, const char *leaf)
{
    scene_owner *own = pc->own;
    obj_script *sc = obj_script_find(own, leaf, "");
    if (sc)
        return sc;
    sc = obj_script_new();
    if (!sc)
        return NULL;
    snprintf(sc->leaf, sizeof sc->leaf, "%s", leaf);
    sc->movname[0] = '\0';
    sc->mov = NULL;
    sc->movtxt = NULL;
    sc->nart = 0;
    if (own->nsc == own->capsc) {
        int nc = own->capsc ? own->capsc * 2 : 128;
        obj_script **p = realloc(own->set, (size_t)nc * sizeof *p);
        if (!p) {
            obj_script_free(sc);
            return NULL;
        }
        own->set = p;
        own->capsc = nc;
    }
    own->set[own->nsc++] = sc;
    return sc;
}

static const char *place_rect_class(place_ctx *pc, const char *p, size_t len)
{
    obj_stats *st = &pc->st;
    const char *eol = p;
    while (eol < p + len && *eol != '\n' && *eol != '\r')
        eol++;
    const char *b = p;
    while (b < eol && (*b == ' ' || *b == '\t'))
        b++;
    const char *e = eol;
    while (e > b && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
        e--;
    char tok[16];
    const char *q = place_field(b, e, tok, sizeof tok);
    if (!q || rect_class_id(tok) < 0)
        return NULL;
    int v[4];
    for (int i = 0; i < 4; i++) {
        const char *s1 = place_int(q, e, &v[i]);
        if (!s1)
            return NULL;
        if (i == 0 || i == 2) {             /* ',' between the pair's */
            q = place_comma(s1, e);         /* two numbers            */
            if (!q)
                return NULL;
        } else
            q = s1;                         /* i==1: the ';' comes next */
        if (i == 1) {                       /* ';' between the pairs   */
            while (q < e && (*q == ' ' || *q == '\t'))
                q++;
            if (q >= e || *q != ';')
                return NULL;
            q++;
        }
        if (i < 3) {
            while (q < e && (*q == ' ' || *q == '\t'))
                q++;
        }
    }
    while (q < e && (*q == ' ' || *q == '\t'))
        q++;
    if (q != e)
        return NULL;                        /* trailing text: not this */
    obj_script *sc = obj_rect_class_get(pc, tok);
    if (!sc)
        return NULL;
    lvl_obj *o = obj_slot(pc);
    if (!o)
        return NULL;
    o->sc = sc;
    o->src = (unsigned char)pc->cur_src;
    o->layer = (unsigned char)pc->cur_layer;
    o->sti = 0;
    o->face = 1;
    /* the line authors no position: the engine's ctor zeroes the object and
     * this verb writes the box only, so the instance carries no anchor. */
    o->bx = o->x = 0;
    o->by = o->y = 0;
    o->arx0 = (short)v[0];                  /* absolute, as authored: */
    o->ary0 = (short)v[1];                  /* 0x40c90 adds no placement */
    o->arx1 = (short)v[2];
    o->ary1 = (short)v[3];
    o->nrect = 1;
    sc->ninst++;
    st->rectclass++;
    return eol;
}

static void des_place_file(place_ctx *pc, const char *fname,
                           const char *txt, size_t len)
{
    obj_stats *st = &pc->st;
    const char *p = txt, *end = txt + len;
    int declared = -1, entries = 0;
    const char *skip_to = NULL;         /* PR-243: set when a line belongs
                                         * to the entry before it (the
                                         * contact-point list) and has been
                                         * read there, so it is not
                                         * classified a second time */
    int last_obj = -1;                  /* the placed object the following
                                         * key=value lines belong to */
    int after_prop = 0;                 /* PR-95: 1 while inside last_obj's
                                          * property block (the damage=/bounds=
                                          * lines that bind to it). A placement
                                          * line (five-field OR three-comma
                                          * static) that appears THERE is that
                                          * object's CHILD: its coordinates are
                                          * an offset from the parent. The vulture's
                                          * POCISK/jajo.mov,0,0 egg and the rhino's
                                          * PICT,panc.seq,20,29 mat both sit in the
                                          * property block of the object before
                                          * them. A top-level static (the ANI*.DES
                                          * shelves, or a WODOSPAD waterfall listed
                                          * as a SIBLING right after an ANIBACK
                                          * entry) has no property block before it:
                                          * after_prop is 0 there, so it keeps its
                                          * absolute world coordinates. */
    while (p < end) {
        const char *eol = p;
        while (eol < end && *eol != '\n' && *eol != '\r')
            eol++;
        const char *b = p;
        while (b < eol && (*b == ' ' || *b == '\t'))
            b++;
        const char *e = eol;
        while (e > b && (e[-1] == ' ' || e[-1] == '\t'))
            e--;
        size_t ll = (size_t)(e - b);
        char line[512];
        if (ll && ll < sizeof line) {
            memcpy(line, b, ll);
            line[ll] = '\0';
        } else {
            line[0] = '\0';              /* measured longest line in the 68
                                          * shipped files is 207 bytes */
            if (ll)
                st->longline++;
        }
        int commas = 0;
        for (const char *q = b; q < e; q++)
            if (*q == ',')
                commas++;

        if (!ll)
            ;                                   /* blank: nothing to say */
        else if (line[0] == ';' || (ll > 1 && line[0] == '/' &&
                                    line[1] == '/'))
            st->comment++;                      /* the game's comment marks */
        else if (declared < 0 && strspn(line, "0123456789") == ll)
            declared = atoi(line);      /* header count: kept for the report,
                                         * never used to stop reading */
        else if (commas < 4) {                  /* no entry shape at all */
            if (strchr(line, '=')) {
                st->prop++;                     /* damage= / energy= / ... */
                if (last_obj >= 0 && last_obj < pc->own->nobj) {
                    lvl_obj *lo = &pc->own->objs[last_obj];
                    after_prop = 1;             /* PR-95: we are inside this
                                                  * object's property block; a
                                                  * placement line that follows
                                                  * is its child (see above) */
                    int v0, v1;
                    if (sscanf(line, " damage = %d", &v0) == 1)
                        lo->damage = v0;
                    else if (sscanf(line, " bounds = %d , %d", &v0, &v1) == 2) {
                        lo->b0 = v0;
                        lo->b1 = v1;
                    } else if (sscanf(line, " scope = %d", &v0) == 1)
                        lo->scope = v0;
                    else if (sscanf(line, " energy = %d", &v0) == 1) {
                        lo->energy = v0;
                        /* PR-89: the spawn energy of the combat counter
                         * (engine field [obj+0xad]). The two-value
                         * authoring ("energy=20,50" once in ZARIA.01,
                         * "1000000,350" once in the census) is read as
                         * the FIRST value by the same sscanf the engine
                         * uses (" energy = %d" - its format string is at
                         * 0x90d9b); which value the engine really keeps
                         * at spawn stays an open question. */
                        lo->hp = v0;
                    }
                }
            }
            else {
                char sleaf[16], sfname[24];
                int sx = 0, sy = 0;
                const char *eos2 = line + ll;
                const char *q1 = place_field(line, eos2, sleaf,
                                             sizeof sleaf);
                const char *q2 = q1 ? place_field(q1, eos2, sfname,
                                                  sizeof sfname) : NULL;
                const char *qx = q2 ? place_int(q2, eos2, &sx) : NULL;
                const char *qc = qx ? place_comma(qx, eos2) : NULL;
                const char *qy = qc ? place_int(qc, eos2, &sy) : NULL;
                size_t fnl = strlen(sfname);
                if (qy && qy == eos2 && fnl > 4 &&
                    ci_eq(sfname + fnl - 4, ".SEQ")) {
                    /* the PR-93 static image line (grammar and joins in
                     * obj_static_get). Counted apart from the entry lines:
                     * this grammar has no script and no properties. */
                    st->statics++;
                    /* PR-95: a static line that directly follows a placed
                     * entry (or one of that entry's property lines, which do
                     * not clear last_obj) is that object's CHILD: its two
                     * numbers are an OFFSET from the parent, not a world
                     * position. This is the SAME child position in which the
                     * five-field 0,0 lines are authored (ZARIA.01 ZWIERZ1:
                     * the vulture's `POCISK,jajo.ani,jajo.mov,0,0` egg one
                     * entry up); here it is the rhino's pancake mat
                     * `NOSOROG,...,1700,600 ... energy=60` then
                     * `PICT,panc.seq,20,29` - the mat belongs under the
                     * rhino, not at the world corner (20,29), which is why
                     * the rhino "stood on nothing" (the exact case PR-93
                     * left open in its own record). A top-level static (last
                     * obj cleared: the ANI*.DES shelf lines, which follow the
                     * file's count line or another static, never an entry)
                     * keeps its absolute authored x,y. */
                    int px = sx, py = sy, child = 0;
                    if (last_obj >= 0 && after_prop &&
                        last_obj < pc->own->nobj &&
                        (pc->own->objs[last_obj].b0 >= 0 ||
                         pc->own->objs[last_obj].scope >= 0)) {
                        /* PR-95: the image is authored inside a bounded
                                          * patrol actor's property block (its
                                          * parent has bounds= / scope= - the
                                          * enemy-placement grammar, HANDOFF §
                                          * "Placement files"): it is the
                                          * actor's own attached prop, drawn
                                          * at the parent + this offset. The
                                          * rhino's pancake mat and the
                                          * vulture's egg are these. A static
                                          * that merely follows some OTHER
                                          * object's property line with that
                                          * object not a bounded actor (TUNGA
                                          * `XCHG ... xchg=5,2` then a
                                          * standalone `TABLICA` sign) is NOT
                                          * an attached child: it keeps its
                                          * absolute authored position. */
                        px = pc->own->objs[last_obj].x + sx;
                        py = pc->own->objs[last_obj].y + sy;
                        child = 1;
                    }
                    if (px < 0 || px >= pc->world_w) {
                        st->static_out++;   /* same rule as an entry's
                                             * coord_bad: outside the level's
                                             * own world, not placed */
                        goto next;
                    }
                    obj_script *sc2 = obj_static_get(pc, sleaf, sfname);
                    if (!sc2)
                        goto next;
                    lvl_obj *so = obj_slot(pc);
                    if (!so)
                        goto next;
                    so->sc = sc2;
                    so->static_img = 1;
                    so->src = (unsigned char)pc->cur_src;
                    so->layer = child
                                    ? pc->own->objs[last_obj].layer
                                    : (unsigned char)pc->cur_layer;
                    so->sti = 0;
                    so->face = 1;
                    so->bx = so->x = px;
                    so->by = so->y = py;
                    if (child) {
                        st->static_attached++;
                        if (st->static_attached <= 4)
                            printf("ll_level: placement %s: static %s/%s"
                                   " follows %s right after it: an image the"
                                   " engine draws attached to that object at"
                                   " the offset (%d,%d), placed at the parent"
                                   " (%d,%d) + offset = (%d,%d), not at the"
                                   " world corner\n", fname, sleaf, sfname,
                                   pc->own->objs[last_obj].sc ?
                                       pc->own->objs[last_obj].sc->leaf
                                       : "?", sx, sy,
                                   pc->own->objs[last_obj].x,
                                   pc->own->objs[last_obj].y, px, py);
                    }
                    last_obj = -1;    /* neither a top-level static nor an
                                       * attached one carries properties; a
                                       * following property line binds to
                                       * nothing and is only counted */
                    after_prop = 0;
                    st->statics_placed++;
                }
                else {
                    /* PR-328: a rectangle-class line goes first - the
                     * token names a class of 0x40c90's eight and the box
                     * is the object (place_rect_class). It is never a
                     * CHILD: that verb adds no placement, so the numbers
                     * keep their authored absolute values even when the
                     * line sits in another object's property block. */
                    const char *nx = place_rect_class(pc, line, ll);
                    if (nx) {
                        last_obj = -1;      /* nothing below it belongs
                                             * to a box-only object */
                        after_prop = 0;
                    }
                    else if (strchr(line, ';'))
                        st->poly++;         /* polyline data (ground) */
                    else
                        st->group++;
                }
            }
        } else {
            char leaf[16], cls[24], mov[24];
            int x = 0, y = 0;
            last_obj = -1;
            after_prop = 0;             /* a new entry line is not inside any
                                          * object's property block */
            const char *eos = line + ll;
            const char *c1 = place_field(line, eos, leaf, sizeof leaf);
            const char *c2 = c1 ? place_field(c1, eos, cls, sizeof cls) : NULL;
            const char *c3 = c2 ? place_field(c2, eos, mov, sizeof mov) : NULL;
            const char *rx = c3 ? place_int(c3, eos, &x) : NULL;
            const char *rc = rx ? place_comma(rx, eos) : NULL;
            const char *ry = rc ? place_int(rc, eos, &y) : NULL;
            if (!ry) {
                /* four commas but not an entry: a polyline line, or a line
                 * this reader cannot make sense of (both counted) */
                if (strchr(line, ';'))
                    st->poly++;
                else
                    st->shortline++;
            } else if (!strchr(mov, '.')) {
                st->entry_noext++;         /* the five authored fields parse,
                                            * but the script field names no
                                            * file: KADMAR.07 ANI1.DES carries
                                            * `BERLO,dummy,sdfsdf,61,54`.
                                            * Nothing resolves from it and
                                            * nothing is guessed. */
                if (st->entry_noext <= 4)
                    printf("ll_level: placement %s: an entry names the script"
                           " \"%s\", which is not a file name: nothing can be"
                           " resolved for it (a data hole in the placement"
                           " itself)\n", fname, mov);
            } else {
                entries++;                  /* a real entry line, whatever
                                            * its coordinates say */
                st->entries++;
                for (const char *q = line; q < ry; q++)
                    if (*q == ' ' || *q == '\t') {
                        st->field_space++;   /* fields written with spaces
                                              * around the commas: the name is
                                              * the same, so it is parsed */
                        break;
                    }
                if (strstr(cls, ".ani"))
                    st->class_hint++;  /* class hint, no .ANI record: never
                                        * used to pick art */
                const char *pm = ry;
                while (pm < eos && *pm == ' ')
                    pm++;
                if (pm < eos) {             /* parameters after the authored
                                             * coordinates: real data in 4 of
                                             * the 2188 entries (KADMAR.07
                                             * `kpal.mov,640,640  -37,-7 -1
                                             * 100 0`, ZARIA.01 `kamien1.mov,
                                             * 1360,250,1308`). Kept verbatim,
                                             * never guessed at. */
                    st->param_extra++;
                    if (st->param_extra <= 4)
                        printf("ll_level: placement %s: an entry's parameters"
                               " after the coordinates (%s) are authored data"
                               " this model does not act on; the object is"
                               " still placed at (%d,%d)\n", fname, pm, x, y);
                }
                if (x < 0 || x >= pc->world_w) {
                    st->coord_bad++;   /* outside the level's own back_rect:
                                        * counted, and not placed */
                    goto next;
                }
                obj_script *sc = obj_script_get(pc, leaf, mov);
                if (!sc)
                    goto next;
                if (x == 0 && y == 0) {
                    /* PR-93: an entry authored at exactly 0,0. Measured
                     * over all 13 level blocks: 130 such lines, and every
                     * one of them directly follows another entry or one of
                     * its property lines - the vulture's carried egg
                     * (ZARIA.01 `SEP ... POCISK,jajo.ani,jajo.mov,0,0`),
                     * the saw-fish on each authored RBLOK log (ENA.03), a
                     * drop on each leaf (KROPLA->KROPLAS), the seaweed
                     * segments, the volcano's smoke from each crater, the
                     * demo texts. Not one of them is a placement at the
                     * world origin; they are the children the engine
                     * spawns FROM their parent when some event fires, and
                     * which event is unresolved. The script and its art
                     * stay joined and counted above (the data authors
                     * them); what is not done is instantiating a world
                     * object at (0,0), which is what drew those fruits and
                     * texts in the top-left corner. */
                    st->attached++;
                    if (st->attached <= 4)
                        printf("ll_level: placement %s: %s/%s is authored at"
                               " 0,0 right after another entry: a child the"
                               " engine spawns from its parent, not a"
                               " placement at the world origin (the spawn"
                               " trigger is unresolved) - joined and"
                               " counted, not instantiated\n",
                               fname, leaf, mov);
                    goto next;
                }
                sc->ninst++;
                lvl_obj *o = obj_slot(pc);
                if (!o)
                    goto next;
                o->sc = sc;
                o->src = (unsigned char)pc->cur_src;
                o->layer = (unsigned char)pc->cur_layer;
                o->sti = sc->mov ? sc->mov->start_st : 0;   /* declared start */
                o->face = 1;     /* PR-81: base facing = the authored draw
                                  * (the frame sign already encodes it) */
                o->bx = o->x = x;      /* authored position, y down: no
                                        * invented bottom-anchor. Only the
                                        * script's own " M dx,dy" frames move
                                        * it (285 of them game-wide). */
                o->by = o->y = y;
                last_obj = pc->own->nobj - 1;
                after_prop = 0;         /* this entry is a placement, not a
                                          * property block; a property line
                                          * that follows re-arms after_prop */
                st->placed++;
                {   /* PR-243: the contact-point list of THIS entry,
                     * if it authors one (see place_points). The
                     * engine reads it as part of the same entry, so
                     * the port reads it here and consumes the line. */
                    const char *np = eol;
                    while (np < end && (*np == '\n' || *np == '\r'))
                        np++;
                    const char *ne = np;
                    for (;;) {  /* PR-246: the record's extra lines
                                 * continue one after another: list,
                                 * rect, pair (the engine's record
                                 * reads them in that order) */
                        const char *nx = place_points(ne, end, o, st);
                        if (nx == ne)
                            nx = place_rect(ne, end, o, st);
                        if (nx == ne)
                            nx = place_pair(ne, end, o, st);
                        if (nx == ne)
                            nx = place_seat_table(ne, end, o, st);
                        if (nx == ne)
                            break;
                        ne = nx;
                        while (ne < end && (*ne == '\n' || *ne == '\r'))
                            ne++;
                    }
                    if (ne != np) {
                        skip_to = ne;
                        if (o->nrect && st->rect_lines <= 3)
                            printf("ll_level: placement %s: %s/%s authors"
                                   " an authored rectangle: absolute"
                                   " (%d,%d)-(%d,%d); the broad"
                                   " phase 0x4b970 tests it against"
                                   " each layer's active window"
                                   " (class verb +0x64, 0x23140)\n",
                                   fname, o->sc->leaf, o->sc->movname,
                                   o->arx0, o->ary0, o->arx1, o->ary1);
                        if (st->pts_lines <= 3)
                            printf("ll_level: placement %s: %s/%s"
                                   " authors %d contact point(s); the"
                                   " engine turns them absolute at"
                                   " load (0x44e90) and tests them"
                                   " against the asker's box"
                                   " (0x23e00): first at (%d,%d)\n",
                                   fname, o->sc->leaf, o->sc->movname,
                                   o->npts, o->cpx[0], o->cpy[0]);
                    }
                }
            }
        }
next:
        p = eol;
        if (skip_to && skip_to > p)
            p = skip_to;        /* PR-243: read as part of the entry
                                 * above, so not classified again */
        skip_to = NULL;
        while (p < end && (*p == '\n' || *p == '\r'))
            p++;
    }
    if (declared >= 0 && declared != entries)
        printf("ll_level: placement %s: its own count line says %d but the"
               " file holds %d entry line(s) - count lines are stale in 38"
               " of the 68 shipped files, so every line is read\n",
               fname, declared, entries);
    if (st->pts_lines || st->rect_lines || st->pair_lines)
        printf("ll_level: placement %s: %d entry line(s) carry an authored"
               " contact-point list, %d a rectangle, %d a bare pair;"
               " %d point(s) read as the absolute positions the engine"
               " builds at load (0x44e90), %d point(s) beyond the %d the"
               " model keeps\n",
               fname, st->pts_lines, st->rect_lines, st->pair_lines,
               st->pts_read, st->pts_extra, OBJ_MAXPTS);
    if (st->seat_lines)
        printf("ll_level: placement %s: %d entry line(s) carry an OWN"
               " surface table (0x45120: a count, then one separator"
               " character and a pair per record - the separator after"
               " record i is record i's flag byte, ';' 1, '#' 2, any"
               " other character 0, and the last record has no flag);"
               " %d record(s) stored absolute as the placement plus the"
               " authored pair (0x45200 at 0x1e52a), %d beyond the %d"
               " the model keeps, %d line(s) whose count is longer than"
               " their pairs, %d line(s) whose text continues after the"
               " records with no reader for it, %d line(s) author a zero"
               " count: no table at all (0x45141-0x45143)\n",
               fname, st->seat_lines, st->seat_read, st->seat_extra,
               OBJ_MAXSEAT, st->seat_short, st->seat_trail,
               st->seat_norec);
    st->files++;
}

/* PR-93: the placement grammar legs. Lines are the shipped shapes, verbatim
 * in structure (ZARIA.01: `SEP,dummy,sep.mov,300,450` + its
 * `POCISK,jajo.ani,jajo.mov,0,0` child + `PICT,plan4.seq,70,480` /
 * `PICT,plan6.seq,287,430`; ZARIA.02 ZWIERZ2: the KAMIEN extra-parameter
 * line; the GRABG 3-comma non-entry): with no containers attached every
 * .MOV and .SEQ is a data hole, so what is asserted is exactly the
 * classification and the placement: entries, the 0,0 child joined but NOT
 * instantiated (no world object at 0,0 - that was the fruit in the
 * corner), static images placed at their authored positions, an x outside
 * the level's own world not placed, the 6-parameter KAMIEN line still an
 * ordinary entry, the GRABG line still only a group header. */
static int place_grammar_selftest(void)
{
    static const char txt[] =
        "5\n"
        "SEP,dummy,sep.mov,300,450\n"
        "damage=10\n"
        "energy=40\n"
        "POCISK,jajo.ani,jajo.mov,0,0\n"
        "PICT,plan4.seq,70,480\n"
        "PICT,nothere.seq,10,20\n"
        "PICT,outsidex.seq,9000,10\n"
        "GRABG,2113,350,kilof.grp\n"
        "KAMIEN,dummy,kamien1.mov,160,250,1308\n"
        /* PR-95: the rhino's mat. NOSOROG is a bounded patrol actor (its
         * bounds= gives b0>=0), so the 3-comma static inside its property
         * block is its child: the image is placed at the parent (700,600)
         * plus the authored offset (20,29) = (720,629), NOT at the literal
         * (20,29) world corner. This is the ZARIA.01 ZWIERZ1 shape. */
        "NOSOROG,dummy,nos.mov,700,600\n"
        "bounds=680,750\n"
        "scope=40\n"
        "PICT,panc.seq,20,29\n"
        /* PR-95 negative: a static that only follows a NON-bounded entry's
         * property line (XCHG has a switch property, no bounds/scope) is
         * NOT a child - it keeps its absolute authored (400,300), not the
         * parent+offset (150+400,535+300). The TUNGA ANI2 TABLICA/jajotab
         * after XCHG shape, scaled into this 1000-px fixture world. */
        "XCHG,dummy,malpa1.mov,150,535\n"
        "xchg=5,2\n"
        "TABLICA,jajotab.seq,400,300\n";
    place_ctx pc;
    scene_owner own;
    memset(&pc, 0, sizeof pc);
    memset(&own, 0, sizeof own);
    pc.own = &own;
    pc.level = "PR93.TEST";
    pc.world_w = 1000;
    des_place_file(&pc, "PR93.DES", txt, sizeof txt - 1);
    const obj_stats *st = &pc.st;
    int nstatic = 0, natzero = 0;
    for (int k = 0; k < own.nobj; k++) {
        if (own.objs[k].static_img)
            nstatic++;
        if (own.objs[k].x == 0 && own.objs[k].y == 0)
            natzero++;
    }
    int fail = 0;
    struct { const char *what; int got, want; } chk[] = {
        { "entry lines", st->entries, 5 },        /* SEP, its child, KAMIEN,
                                                   * the rhino, XCHG */
        { "entries placed", st->placed, 4 },      /* the 0,0 child is NOT one */
        { "attached children", st->attached, 1 },
        { "class hints", st->class_hint, 1 },
        { "property lines", st->prop, 5 },        /* damage/energy + the
                                                   * rhino's bounds/scope +
                                                   * XCHG's switch property */
        { "static lines", st->statics, 5 },
        { "statics placed", st->statics_placed, 4 },
        { "statics outside the world", st->static_out, 1 },
        { "static art holes", st->static_missing, 4 },
        { "attached statics", st->static_attached, 1 }, /* only the rhino's mat */
        { "group lines", st->group, 1 },          /* GRABG: not a static */
        { "objects total", own.nobj, 8 },         /* 4 entries + 4 statics */
        { "static instances", nstatic, 4 },
        { "instances at 0,0", natzero, 0 },
        { "param-extra entries", st->param_extra, 1 },
    };
    for (unsigned k = 0; k < sizeof chk / sizeof chk[0]; k++)
        if (chk[k].got != chk[k].want) {
            fprintf(stderr, "mov selftest: FAILED placement grammar: %s"
                    " counted %d, expected %d\n",
                    chk[k].what, chk[k].got, chk[k].want);
            fail = 1;
        }
    /* the static at 70,480 stands there with its art unresolved; the
     * child of SEP does not stand anywhere of its own, and its parent's
     * energy= property still bound the PARENT (an attached child never
     * becomes the property target either: last_obj was reset) */
    if (own.nobj >= 1 && own.objs[0].hp != 40) {
        fprintf(stderr, "mov selftest: FAILED placement grammar: the SEP"
                " entry's energy= property did not reach it (hp %d)\n",
                own.objs[0].hp);
        fail = 1;
    }
    if (own.objs[0].x != 300 || own.objs[0].y != 450) {
        fprintf(stderr, "mov selftest: FAILED placement grammar: the SEP"
                " entry moved (%d,%d)\n", own.objs[0].x, own.objs[0].y);
        fail = 1;
    }
    int found70 = 0;
    for (int k = 0; k < own.nobj; k++)
        if (own.objs[k].static_img && own.objs[k].x == 70 &&
            own.objs[k].y == 480 && own.objs[k].static_img)
            found70 = 1;
    if (!found70) {
        fprintf(stderr, "mov selftest: FAILED placement grammar: no static"
                " image stands at the authored (70,480)\n");
        fail = 1;
    }
    /* PR-95: the rhino's mat is attached - placed at its parent (700,600)
     * plus the authored offset (20,29), and never at the literal corner.
     * The XCHG case is the negative: its following static keeps its
     * absolute authored position, it is not pulled to parent+offset. */
    int found_panc = 0, found_corner = 0, found_abs = 0;
    for (int k = 0; k < own.nobj; k++) {
        if (!own.objs[k].static_img)
            continue;
        if (own.objs[k].x == 720 && own.objs[k].y == 629)
            found_panc = 1;
        if (own.objs[k].x == 20 && own.objs[k].y == 29)
            found_corner = 1;
        if (own.objs[k].x == 400 && own.objs[k].y == 300)
            found_abs = 1;
    }
    if (!found_panc || found_corner) {
        fprintf(stderr, "mov selftest: FAILED placement grammar: the rhino's"
                " attached static is not at parent+offset (720,629)%s\n",
                found_corner ? " or it also stands at the (20,29) corner" : "");
        fail = 1;
    }
    if (!found_abs) {
        fprintf(stderr, "mov selftest: FAILED placement grammar: the static"
                " after a non-bounded entry moved off its absolute (400,300)"
                " (it is not that entry's child)\n");
        fail = 1;
    }
    scene_owner_free(&own);
    return fail;
}

/* PR-328: the rectangle-class placement grammar (the read is PR-327). The
 * box lines are verbatim out of the shipped `.des` texts - ZARIA.01_ANI2
 * lines 18, 19, 20, SADES.06_ANIM line 45, TUNGA.11_ANI2 lines 110 and 52
 * (a box whose authored x0 is -10, so no world-bounds rule may drop it),
 * KADMAR.07_ANI1 line 11 (authored right AFTER an entry line) - plus the
 * two class tokens the engine carries and no shipped level places (BRECT,
 * WSPRECT). What is asserted, in one step each:
 *   1. such a line IS an object: its class is the token, and the four
 *      authored numbers are its box, kept ABSOLUTE. 0x40c90 is
 *      `add eax,0x3c; call 0x49f90` - it never adds a placement the way
 *      the ladder's own record verb 0x3f0f0 does (0x3f124-0x3f14b), so
 *      the KADMAR.07 case must NOT be pulled to parent+offset;
 *   2. the numbers are kept UNSORTED as authored (place_rect sorts for the
 *      ladder's intersect because 0x23145 reads low/high edges; the
 *      rectangle verb writes what the line says, nothing else);
 *   3. two lines of the same class share ONE class record and stay two
 *      objects - the engine's one table per class and one instance per
 *      line;
 *   4. what is NOT in the family is not placed: ZONE (its own +0x18 verb
 *      0x41c50, PR-327) and KRY (no such string in the engine at all) stay
 *      the polyline noise they were, and a token whose numbers do not form
 *      the box shape is not an object either. */
static int place_rectclass_selftest(void)
{
    static const char txt[] =
        "15\n"
        "DRECT,988,645;1340,750\n"        /* ZARIA.01_ANI2 line 18 */
        "RECT,1820,380;1970,410\n"       /* ZARIA.01_ANI2 line 19 */
        "KILOF_PL,1760,590;1832,620\n"   /* ZARIA.01_ANI2 line 20 */
        "DRECT2,450,0;610,250\n"         /* SADES.06_ANIM line 45 */
        "RECT,-10,200;50,500\n"          /* TUNGA.11_ANI2 line 110 */
        "DRECT2,510,400;820,600\n"       /* TUNGA.11_ANI2 line 52 */
        "GWIAZDKI,xxx,kadgwi2.mov,28,42\n"    /* KADMAR.07_ANI1 line 9 */
        "BICZRECT,218,345;230,368\n"     /* KADMAR.07_ANI1 line 11 */
        "BRECT,10,20;30,40\n"            /* engine-only token (PR-327) */
        "WSPRECT,5,5;9,9\n"              /* engine-only token (PR-327) */
        "RECT,900,300;800,250\n"         /* unsorted as authored */
        "ZONE,5044,444;5056,456\n"       /* NOT in the family */
        "KRY,851,410;1396,598\n"         /* NOT in the family */
        "DRECT,988,645\n"                /* the token, no box: not one */
        "DRECT,988,645;1340\n";          /* half a box: not one */
    static const struct {
        const char *cls;
        int x0, y0, x1, y1;
    } want[] = {
        { "DRECT",    988,  645, 1340,  750 },
        { "RECT",    1820,  380, 1970,  410 },
        { "KILOF_PL",1760,  590, 1832,  620 },
        { "DRECT2",   450,    0,  610,  250 },
        { "RECT",     -10,  200,   50,  500 },
        { "DRECT2",   510,  400,  820,  600 },
        { "BICZRECT", 218,  345,  230,  368 },
        { "BRECT",     10,   20,   30,   40 },
        { "WSPRECT",    5,    5,    9,    9 },
        { "RECT",     900,  300,  800,  250 },
    };
    place_ctx pc;
    scene_owner own;
    memset(&pc, 0, sizeof pc);
    memset(&own, 0, sizeof own);
    pc.own = &own;
    pc.level = "PR328.TEST";
    pc.world_w = 2000;
    des_place_file(&pc, "PR328.DES", txt, sizeof txt - 1);
    const obj_stats *st = &pc.st;
    int fail = 0;
    struct { const char *what; int got, want; } chk[] = {
        { "rectangle-class lines", st->rectclass, 10 },
        { "entry lines", st->entries, 1 },
        { "objects total", own.nobj, 11 },     /* 1 entry + 10 box objects */
        { "polyline lines", st->poly, 3 },     /* ZONE, KRY, the half box */
        { "group lines", st->group, 1 },       /* the token-only line */
    };
    for (unsigned k = 0; k < sizeof chk / sizeof chk[0]; k++)
        if (chk[k].got != chk[k].want) {
            fprintf(stderr, "mov selftest: FAILED rectangle-class grammar:"
                    " %s counted %d, expected %d\n",
                    chk[k].what, chk[k].got, chk[k].want);
            fail = 1;
        }
    /* every authored box above stands, on an object whose class IS the
     * token, with the numbers unchanged (absolute, unsorted) */
    size_t nfound = 0;
    for (size_t q = 0; q < sizeof want / sizeof want[0]; q++) {
        int hit = 0;
        for (int k = 0; k < own.nobj; k++) {
            const lvl_obj *o = &own.objs[k];
            if (!o->sc || !o->nrect || o->static_img)
                continue;
            if (!ci_eq(o->sc->leaf, want[q].cls))
                continue;
            if (o->arx0 == want[q].x0 && o->ary0 == want[q].y0 &&
                o->arx1 == want[q].x1 && o->ary1 == want[q].y1)
                hit = 1;
        }
        if (!hit) {
            fprintf(stderr, "mov selftest: FAILED rectangle-class grammar:"
                    " no object of class %s carries the authored box"
                    " %d,%d;%d,%d\n", want[q].cls, want[q].x0, want[q].y0,
                    want[q].x1, want[q].y1);
            fail = 1;
        } else
            nfound++;
    }
    /* the KADMAR.07 shape: the box after an entry is NOT pulled to
     * parent+offset - the parent stands at (28,42), so a pulled box would
     * read 246,387;258,410 */
    for (int k = 0; k < own.nobj; k++) {
        const lvl_obj *o = &own.objs[k];
        if (o->sc && o->nrect && ci_eq(o->sc->leaf, "BICZRECT") &&
            o->arx0 == 246 && o->ary0 == 387) {
            fprintf(stderr, "mov selftest: FAILED rectangle-class grammar:"
                    " the BICZRECT box was pulled to the preceding entry"
                    " (246,387;258,410) - 0x40c90 adds no placement\n");
            fail = 1;
        }
    }
    /* the two non-family lines place nothing */
    for (int k = 0; k < own.nobj; k++) {
        const lvl_obj *o = &own.objs[k];
        if (o->sc && (ci_eq(o->sc->leaf, "ZONE") || ci_eq(o->sc->leaf,
                                                          "KRY"))) {
            fprintf(stderr, "mov selftest: FAILED rectangle-class grammar:"
                    " %s was placed as an object - it is not one of the"
                    " eight class tokens of 0x40c90\n", o->sc->leaf);
            fail = 1;
        }
    }
    /* one class record per token, many instances: three RECT lines, one
     * class (the engine keeps ONE table per class and one instance per
     * line) */
    int nrect = 0, ninst = 0;
    for (int k = 0; k < own.nsc; k++)
        if (ci_eq(own.set[k]->leaf, "RECT")) {
            nrect++;
            ninst = own.set[k]->ninst;
        }
    if (nrect != 1 || ninst != 3) {
        fprintf(stderr, "mov selftest: FAILED rectangle-class grammar:"
                " %d class record(s) for RECT with %d instance(s), expected"
                " 1 and 3\n", nrect, ninst);
        fail = 1;
    }
    if (nfound != sizeof want / sizeof want[0]) {
        fprintf(stderr, "mov selftest: FAILED rectangle-class grammar: only"
                " %d of %d authored boxes stand\n", (int)nfound,
                (int)(sizeof want / sizeof want[0]));
        fail = 1;
    }
    scene_owner_free(&own);
    return fail;
}

/* PR-329: the rectangle-class ANSWER, the class table's +0x78 entry, run
 * against the player as the asker. Everything asserted here is read: the
 * +0x78 slots (0x40cf0 RECT, 0x40d50 DRECT, 0x40dc0 DRECT2, 0x1e3a0
 * KILOF_PL, 0x286a0 BICZRECT, 0x28710 BRECT, 0x287a0 WSPRECT), the walk
 * that calls them (0x4b500: channel bit from the requester's state, the
 * plane gate 0x4b5a1, self skipped), and the signal bits - the answer
 * record's ors at [ans+0x2] 0x2/0x8/0x10 and [ans+0x4] 0x1 are exactly
 * bits 17/19/20/32, the LL_SIG_* indexes of AC_RECT / AC_DRECT /
 * AC_DRECT2 / AC_KILOF. The boxes and positions are ZARIA.01's: the
 * DRECT (988,645)-(1340,750) of ANI2.DES line 18 over the exit point
 * 1020,692, the RECT (1820,380)-(1970,410) of line 19, the KILOF_PL
 * (1760,590)-(1832,620) of line 20. The player states stand in for the
 * lion's: one carries CHECK_ALL (which implies CHECK_ANI, the channel the
 * ANI2 placements sit on - des_place_keys index 1 = the `animacje` key),
 * one carries no CHECK bit (the walk is not run: the ask comes from the
 * requester's state words, PR-245), and one carries CHECK_ANI with
 * CHECK_ALL_PLANE (the walk's second gate lifted, PR-247). */
static int rect_class_ask_selftest(void)
{
    static const char PL_MOV[] =
        "ST_CHECK\nST_PLAIN\nST_PLANE\n\n"
        "[ST_CHECK]\ncontrol=BY_TIME|CHECK_ALL\n1,0\n\n"
        "[ST_PLAIN]\ncontrol=BY_TIME\n1,0\n\n"
        "[ST_PLANE]\ncontrol=BY_TIME|CHECK_ANI|CHECK_ALL_PLANE\n1,0\n";
    static const struct {
        const char *cls;             /* the class token (the .des line's) */
        int box[4];                  /* the authored box, unsorted         */
        int layer, src;              /* the placement's plane and channel  */
        int sti;                     /* the requester's state              */
        double x, y, vy;
        int face;                    /* -1/1 as ll_player.face             */
        unsigned long long want;     /* the exact bits expected            */
        const char *what;
    } cases[] = {
        { "DRECT", { 988, 645, 1340, 750 }, 0, 1, 0, 1020, 692, 3.0, 1,
          1ull << LL_SIG_AC_DRECT,
          "the ZARIA.01 DRECT: falling, inside, the exit point inside the"
          " box (0x40d50: askable probe, [asker+0xc6] > 0, point in box)" },
        { "DRECT", { 988, 645, 1340, 750 }, 0, 1, 0, 1020, 692, 0.0, 1,
          0ull, "standing in the same box: no fall, no AC_DRECT" },
        { "DRECT", { 988, 645, 1340, 750 }, 0, 1, 0, 1020, 692, -3.0, 1,
          0ull, "rising through it: [asker+0xc6] is not > 0" },
        { "DRECT", { 988, 645, 1340, 750 }, 0, 1, 0, 988, 645, 3.0, 1,
          1ull << LL_SIG_AC_DRECT,
          "the top-left corner itself: the point test is inclusive" },
        { "DRECT", { 988, 645, 1340, 750 }, 0, 1, 0, 1341, 692, 3.0, 1,
          0ull, "one px past the right edge" },
        { "DRECT", { 988, 645, 1340, 750 }, 0, 2, 0, 1020, 692, 3.0, 1,
          0ull, "the channel the line was placed on (index 2) matches no"
                " CHECK bit of the state" },
        { "DRECT", { 988, 645, 1340, 750 }, 1, 1, 0, 1020, 692, 3.0, 1,
          0ull, "another plane, CHECK_ALL_PLANE not set (0x4b5a1)" },
        { "DRECT", { 988, 645, 1340, 750 }, 1, 1, 2, 1020, 692, 3.0, 1,
          1ull << LL_SIG_AC_DRECT,
          "the same plane mismatch with CHECK_ALL_PLANE set: the gate is"
          " lifted" },
        { "DRECT", { 988, 645, 1340, 750 }, 0, 1, 1, 1020, 692, 3.0, 1,
          0ull, "a state that carries no CHECK bit does not run the walk"
                " (0x523f0 asks from the state's own control words)" },
        { "DRECT2", { 988, 645, 1340, 750 }, 0, 1, 0, 1020, 692, 0.0, 1,
          1ull << LL_SIG_AC_DRECT2,
          "the DRECT2 verb 0x40dc0: same probe, same box, NO fall test -"
          " the one difference PR-326 read between the two" },
        { "RECT", { 1820, 380, 1970, 410 }, 0, 1, 0, 1900, 400, 0.0, 1,
          1ull << LL_SIG_AC_RECT,
          "the ZARIA.01 RECT of ANI2 line 19: 0x40cf0 has no probe and no"
          " fall test, only the point-in-box (it does not even probe"
          " askable)" },
        { "RECT", { 900, 300, 800, 250 }, 0, 1, 0, 850, 275, 0.0, 1,
          0ull, "an authored box that runs backwards matches nothing:"
                " nothing in 0x40c90 or the point test orders the edges" },
        { "KILOF_PL", { 1760, 590, 1832, 620 }, 0, 1, 0, 1796, 605, 0.0, 1,
          1ull << LL_SIG_AC_KILOF,
          "the ZARIA.01 KILOF_PL of ANI2 line 20: 0x1e3a0 probes askable,"
          " the facing verb 0x239f0 = [obj+0x6d] must be > 0, and the"
          " raise is the register form at 0x1e422-0x1e42b (bit 32)" },
        { "KILOF_PL", { 1760, 590, 1832, 620 }, 0, 1, 0, 1796, 605, 0.0, -1,
          0ull, "facing left: the 0x1e3f6 jle drops it" },
        { "BICZRECT", { 218, 345, 230, 368 }, 0, 1, 0, 224, 356, 0.0, -1,
          1ull << LL_SIG_AC_DRECT,
          "BICZRECT 0x286a0: facing == -1 and the ANCHOR point (the +0x6c"
          " verb 0x23a10 = [obj+0x89]) in the box; its raise is the same"
          " AC_DRECT bit" },
        { "BICZRECT", { 218, 345, 230, 368 }, 0, 1, 0, 224, 356, 0.0, 1,
          0ull, "facing right: the 0x286c0 cmp 0xffffffff drops it" },
        { "BRECT", { 10, 20, 30, 40 }, 0, 1, 0, 20, 30, 0.0, 1,
          1ull << LL_SIG_AC_RECT,
          "BRECT 0x28710 raises the AC_RECT bit; its state-index arm"
          " ([asker+0x5c] in 9..13 chooses the anchor point over the"
          " presented one) is not observable in this port, where the two"
          " points are the same position block" },
        { "WSPRECT", { 5, 5, 9, 9 }, 0, 1, 0, 7, 7, 0.0, 1,
          0ull, "WSPRECT's +0x78 verb (0x287a0) is an anchor test plus an"
                " object-id state lookup ([rect+0x30], 0x55760) raising"
                " bit 12 - NOT read here, and no shipped level places one" },
        { "KRA1", { 0, 0, 50, 50 }, 0, 1, 0, 10, 10, 0.0, 1,
          0ull, "KRA1's +0x78 verb (0x37ea0) raises two signals this read"
                " never touched; its shipped lines are ART placements that"
                " go through the entry grammar, so no box-only KRA1 exists" },
    };
    ll_mov_t *m = calloc(1, sizeof *m);
    if (!m) {
        fprintf(stderr, "rect ask selftest: FAILED out of memory\n");
        return 1;
    }
    ll_mov_parse(PL_MOV, m);
    if (m->nst != 3) {
        fprintf(stderr, "rect ask selftest: FAILED the synthetic player"
                        " script did not parse (%d states)\n", m->nst);
        free(m);
        return 1;
    }
    int fail = 0;
    for (unsigned c = 0; c < sizeof cases / sizeof cases[0]; c++) {
        play_ctx pl;
        memset(&pl, 0, sizeof pl);
        pl.have = 1;
        pl.mov = m;
        pl.layer = 0;
        pl.p.sti = cases[c].sti;
        pl.p.x = cases[c].x;
        pl.p.y = cases[c].y;
        pl.p.vy = cases[c].vy;
        pl.p.face = (signed char)cases[c].face;
        obj_script sc;
        lvl_obj o;
        memset(&sc, 0, sizeof sc);
        strcpy(sc.leaf, cases[c].cls);
        memset(&o, 0, sizeof o);
        o.sc = &sc;
        o.nrect = 1;
        o.arx0 = (short)cases[c].box[0];
        o.ary0 = (short)cases[c].box[1];
        o.arx1 = (short)cases[c].box[2];
        o.ary1 = (short)cases[c].box[3];
        o.src = (unsigned char)cases[c].src;
        o.layer = (unsigned char)cases[c].layer;
        rect_class_asks(&pl, &o, 1, 0, 0);
        if (pl.p.sig != cases[c].want) {
            fprintf(stderr, "rect ask selftest: FAILED: %s at %d,%d;%d,%d"
                    " asked from state %d at (%.0f,%.0f) vy=%g face=%d:"
                    " set %llx, expected %llx  [%s]\n",
                    cases[c].cls, cases[c].box[0], cases[c].box[1],
                    cases[c].box[2], cases[c].box[3], cases[c].sti,
                    cases[c].x, cases[c].y, cases[c].vy, cases[c].face,
                    pl.p.sig, cases[c].want, cases[c].what);
            fail = 1;
        }
    }
    free(m);
    return fail;
}

static void des_placement(place_ctx *pc, llrs *hd_ety,
                          const llrs_slot *hd_blk2, const des_kv *kvs, int nk)
{
    obj_stats *st = &pc->st;
    if (!hd_ety || !hd_blk2) {
        printf("ll_level: no HD ETAPY block for %s: the level's own placement"
               " files are unreachable (data hole)\n", pc->level);
        return;
    }
    llrs_slot *hrec2 = NULL;
    unsigned nh2 = 0;
    if (llrs_block(hd_ety, hd_blk2, &hrec2, &nh2))
        return;

    for (unsigned kk = 0;
         kk < sizeof des_place_keys / sizeof des_place_keys[0]; kk++) {
        const char *key = des_place_keys[kk];
        const char *raw = des_get(kvs, nk, key);
        pc->cur_src = (int)kk;
        if (!raw[0])
            continue;      /* key absent: the author says nothing about it
                            * (no shipped level omits these three keys) */
        char list[512];
        snprintf(list, sizeof list, "%s", raw);
        char *save = NULL;
        int tix = -1;
        for (char *tok = strtok_r(list, ",", &save); tok;
             tok = strtok_r(NULL, ",", &save)) {
            while (*tok == ' ')
                tok++;
            if (!tok[0])
                continue;
            pc->cur_layer = ++tix;
            /* PR-212/213: EVERY pool's cell rides the planies segment that
             * carries its A - cell_plane is the engine's law verbatim (the
             * level-load pass 0x4b1d0: entry i chains at cell i raw, the
             * 'none' token occupies a head and shifts nothing, and the
             * segment count is dispatched to every entry through setter slot
             * +0x5c). The letter-named animacje= mapping PR-208 shipped is
             * the SAME law; cell_layer now only feeds that report line.
             * Where a level authors no plany= string (none shipped) the
             * entry-index fallback stays. OI-109 closed by this PR. */
            if (pc->plan.law && tix < MAX_ANI_CELLS)
                pc->cur_layer = pc->plan.cell_plane[tix];
            if (!strchr(tok, '.') && ci_eq(tok, "none")) {
                printf("ll_level: des %s: the %s list carries the bare 'none'"
                       " keyword (the game's own 'nothing here': skipped,"
                       " not a data hole)\n", pc->level, key);
                continue;
            }
            const llrs_slot *dr = NULL;
            for (unsigned k = 0; k < nh2; k++)
                if (hrec2[k].id == 1 && hrec2[k].name[0] &&
                    ci_eq(hrec2[k].name, tok))
                    dr = &hrec2[k];
            if (!dr) {
                st->file_missing++;
                printf("ll_level: des %s: %s (named by %s=) is not in the HD"
                       " level block (data hole)\n", pc->level, tok, key);
                continue;
            }
            unsigned char *txt = NULL;
            if (llrs_read(hd_ety, dr, &txt) < 0)
                continue;
            printf("ll_level: placement %s (%u B, from %s=)\n", dr->name,
                   dr->size, key);
            des_place_file(pc, dr->name, (const char *)txt, (size_t)dr->size);
            free(txt);
        }
    }
    llrs_block_free(hrec2);

    printf("ll_level: authored scene %s: %d placement file(s), %d object"
           " entry line(s) -> %d placed, %d at an x outside the view the"
           " level's own data defines (its back_rect, or its single screen"
           " where the data gives none), %d distinct script(s) / %d art"
           " table(s) (%d carry the patch art this viewer draws, %d of those"
           " also carry the shape margins, %d carry shape geometry only -"
           " invisible solids the original game never drew either); %d"
           " script(s) and %d art name(s) are not on these containers (data"
           " holes listed above)\n",
           pc->level, st->files, st->entries, st->placed, st->coord_bad,
           st->nscript, st->nart, st->nart - st->art_shapeonly,
           st->art_shapecarry, st->art_shapeonly, st->nscript_missing,
           st->nart_missing);
    if (st->statics)
        printf("ll_level: authored scene %s: %d static image line(s) (the"
               " 3-comma \"<LEAF>,<name>.seq,<x>,<y>\" grammar: the line"
               " names its own .SEQ in that CD ANIMACJE leaf, no script)"
               " -> %d placed, %d at an x outside the level's own world,"
               " %d whose .SEQ is not on these containers (data holes"
               " listed above), %d carry shape only (drawn as nothing), %d"
               " authored right after an object and drawn attached to it at"
               " the offset (a mat under the object the line follows: an"
               " offset from that object, not a world position)\n",
               pc->level, st->statics, st->statics_placed, st->static_out,
               st->static_missing, st->static_shapeonly,
               st->static_attached);
    /* PR-328: the rectangle-class placements, named apart because they are
     * neither entry lines nor static images nor polyline noise: the line
     * names a class of 0x40c90's eight and authors its box (PR-327). They
     * carry no script, no art and no states, so nothing draws or steps
     * them until the rectangle ask of 0x40cf0/0x40d50/0x40dc0 is ported. */
    if (st->rectclass)
        printf("ll_level: authored scene %s: %d rectangle-class line(s)"
               " (`TOKEN,x0,y0;x1,y1`, the eight class tables of the record"
               " verb 0x40c90) placed as objects carrying the class token"
               " and the authored box at [obj+0x3c]; nothing asks them yet,"
               " so they draw and step as nothing (the rectangle ask is the"
               " next step)\n", pc->level, st->rectclass);
    if (st->prop || st->group || st->comment || st->class_hint ||
        st->poly || st->shortline || st->param_extra || st->field_space ||
        st->entry_noext || st->attached || st->longline)
        printf("ll_level: placement data the scene model does not act on"
               " (counted, never invented): %d per-entry property line(s)"
               " (damage/energy/bounds/scope), %d group header line(s),"
               " %d comment line(s) (';' or '//'), %d object-class token(s)"
               " (no .ANI record exists in any container), %d polyline"
               " line(s) outside an entry (the .PLT shape written inside"
               " a .des: not an entry's OWN table, which the 0x45120"
               " reader above takes), %d"
               " entry line(s) with parameters beyond the coordinates"
               " (listed above), %d entry line(s) whose fields are written"
               " with spaces around the commas (parsed as the same names), %d"
               " entry line(s) whose script field is not a file name (listed"
               " above), %d entry line(s) authored at 0,0: all of them"
               " follow another entry or its properties - children the"
               " engine spawns from their parent (its trigger is"
               " unresolved), never a placement at the world origin (listed"
               " above), %d line(s) this reader cannot classify\n",
               st->prop, st->group, st->comment, st->class_hint, st->poly,
               st->param_extra, st->field_space, st->entry_noext,
               st->attached, st->shortline + st->longline);
    if (st->nscript)
        printf("ll_level: those scripts author %d state transition(s), %d"
               " control flag(s) with no model bit, %d frame"
               " directive(s) the model does not act on (the engine's own"
               " table 0x917c0 minus the modeled GEN (PR-88), ATC_ON/"
               " ATC_OFF (PR-89), end_level (PR-199) and pos (PR-317),"
               " and minus the CAPTURED-not-acted Wait/WAIT clause"
               " (PR-217): lock /"
               " ground_on / chdir / goto / F / FC / M / MOVE /"
               " smp*), %d"
               " sound-or-scale field(s); %d state(s)"
               " name no seq= art, %d state(s) / %d frame(s) / %d"
               " transition(s) over cap; %d row(s) carry a `Wait` clause"
               " the model CAPTURES but does not act on (PR-217) - all"
               " parsed and counted, acted on"
               " only once the rest of the AC_*/K_* signal bus lands"
               " (named-signal bits landed in PR-88; the cross-object"
               " routing and the engine-internal raisers have not);"
               " %d row line(s) open with the engine's `s <seq>` field"
               " (PR-330 reads this row too: 206 of them across the"
               " shipped 777 .MOV texts). Their ROW is modelled - frame,"
               " units, tails, and the units kept as authored because"
               " the engine builds that row with the constant 1000 -"
               " and the sequence record the field names is counted"
               " here, not drawn: the per-row art switch is OI-120\n",
               st->ntrans, st->ctrl_other, st->directive, st->field_other,
               st->state_noart, st->drop_st, st->drop_fr, st->drop_tr,
               st->wait_cap, st->seq_switch);
    if (st->fr_total)
        printf("ll_level: frame numbers of the scripts that have art (the"
               " game's grammar: 1-based, negative = drawn mirrored, 0 = no"
               " element): %d frame(s), %d mirrored, %d blank (drawn as"
               " nothing: the game's placeholder is not modeled), %d name an"
               " element their art table does not have (drawn as nothing,"
               " never clamped to another)\n",
               st->fr_total, st->fr_mirror, st->fr_blank, st->fr_oob);
}
/* paint one 8bpp canvas through the palette LUT (shared present path).
 * The LUT stays the scene's own business (level PCX palettes AND the
 * documented grayscale fallback for blocks without a palette donor): the
 * CPU expands the canvas into a persistent ARGB scratch and hands the
 * result to the platform (plat_present_argb) - pixels are exactly the
 * ones the scene computed; the platform never sees the palette. */
static uint32_t *s_paint_buf;
static size_t s_paint_n;

static void paint(const unsigned char *canvas, const uint32_t lut[256],
                  int w, int h)
{
    size_t need = (size_t)w * (size_t)h;
    if (s_paint_n < need) {
        uint32_t *nb = realloc(s_paint_buf, need * sizeof *nb);
        if (!nb)
            return;                 /* skip the frame; data work continues */
        s_paint_buf = nb;
        s_paint_n = need;
    }
    for (int y = 0; y < h; y++) {
        uint32_t *row = s_paint_buf + (size_t)y * w;
        const unsigned char *sp = canvas + (size_t)y * w;
        for (int x = 0; x < w; x++)
            row[x] = lut[sp[x]];
    }
    plat_present_argb(s_paint_buf, w, h);
}

/* the scene: the 320x200 world window shown 2x (nearest neighbour) */
static void paint2x(const unsigned char *canvas, const uint32_t lut[256],
                    int w, int h)
{
    size_t need = (size_t)w * 2 * (size_t)h * 2;
    if (s_paint_n < need) {
        uint32_t *nb = realloc(s_paint_buf, need * sizeof *nb);
        if (!nb)
            return;
        s_paint_buf = nb;
        s_paint_n = need;
    }
    for (int y = 0; y < h; y++) {
        uint32_t *r0 = s_paint_buf + (size_t)y * 2 * (size_t)w * 2;
        const unsigned char *sp = canvas + (size_t)y * w;
        for (int x = 0; x < w; x++)
            r0[2 * x] = r0[2 * x + 1] = lut[sp[x]];
        memcpy(r0 + (size_t)w * 2, r0, (size_t)w * 2 * sizeof *r0);
    }
    plat_present_argb(s_paint_buf, w * 2, h * 2);
}

/* ---------------- level help overlay (original POMOC flow) --------------
 * POMOC1.MNU (HD-install ETAPY block) wires the help: static=x,y,pomocNN.seq
 * lines list page art (text baked into .SEQ frame patches); the .SEQ
 * payloads themselves ship in the CD ETAPY level block (incomplete CD:
 * only some pages present). Left/Right/Enter cycle pages, ESC or any
 * click closes and the page otherwise stays (original behavior, E-02);
 * LEO_HELP_SECS=<sec> adds an auto-close timeout for CI/flow only. The
 * scene window starts when the help overlay closes.
 * cursor=mouse.seq / palette=<stem>.pal refer to payloads absent from both
 * trees (documented data hole) - bg palette is kept.                   */
typedef struct { int x, y; ll_seq seq; unsigned char *blob; } help_page;

static void help_pages(const llrs *hd, const llrs_slot *hdrecs, unsigned nhdrec,
                       const llrs *cd, const llrs_slot *cdrecs, unsigned ncdrec,
                       const unsigned char *bg, unsigned char *canvas,
                       const uint32_t lut[256], int w, int h)
{
    const llrs_slot *mnu = NULL;
    for (unsigned k = 0; k < nhdrec; k++) {
        const char *dot = strrchr(hdrecs[k].name, '.');
        if (hdrecs[k].id == 1 && dot && ci_eq(dot, ".MNU")) {
            mnu = &hdrecs[k];
            break;
        }
    }
    if (!mnu)
        return;
    unsigned char *raw = NULL;
    if (llrs_read(hd, mnu, &raw) < 0)
        return;
    help_page pages[16];
    char mnu_seq[16][16];
    int nms = 0;
    int np = 0;
    /* line scan (repeated "static=" keys - a key=table would collapse them);
     * bounded by the payload size: container payloads are not NUL-terminated */
    const char *p = (const char *)raw, *mend = (const char *)raw + mnu->size;
    while (p < mend && *p) {
        const char *eol = p;
        while (eol < mend && *eol && *eol != '\n' && *eol != '\r')
            eol++;
        if (eol - p > 7 && strncmp(p, "static=", 7) == 0) {
            int x, y;
            char seq[16] = "";
            if (sscanf(p + 7, "%d,%d,%15[a-zA-Z0-9.]", &x, &y, seq) == 3) {
                if (nms < 16)
                    snprintf(mnu_seq[nms++], sizeof mnu_seq[0], "%s", seq);
                const llrs_slot *ps = NULL;
                for (unsigned j = 0; j < ncdrec; j++)
                    if (cdrecs[j].id == 1 && ci_eq(cdrecs[j].name, seq)) {
                        ps = &cdrecs[j];
                        break;
                    }
                if (ps && np < 16) {  /* other pages absent from this CD */
                    pages[np].blob = NULL;
                    if (llrs_read(cd, ps, &pages[np].blob) >= 0 &&
                        ll_seq_parse(pages[np].blob, (uint32_t)ps->size,
                                     &pages[np].seq) == 0) {
                        const ll_seq_elem *fe = NULL;
                        for (unsigned el = 0; el < pages[np].seq.count; el++)
                            if (pages[np].seq.els[el].arr_n > 0) {
                                fe = &pages[np].seq.els[el];
                                break;
                            }
                        /* MNU x,y are placeholders (0,0) here; the .SEQ
                         * element header carries the real canvas anchor,
                         * screen pos = (-f0,-f2), like the menu leaves. */
                        if (fe) {
                            int ex = -(int)(short)fe->f0;
                            int ey = -(int)(short)fe->f2;
                            if (ex > 0 || ey > 0) {
                                x = ex > 0 ? ex : x;
                                y = ey > 0 ? ey : y;
                            }
                        }
                        pages[np].x = x;
                        pages[np].y = y;
                        printf("ll_level: help: %s page %d %s at (%d,%d)\n",
                               mnu->name, np, seq, x, y);
                        np++;
                    } else {
                        free(pages[np].blob);
                        pages[np].blob = NULL;
                    }
                }
            }
        }
        p = eol;
        while (*p == '\n' || *p == '\r')
            p++;
    }
    /* This CD's .MNU static= list is the generic set (pomoc00..pomoc10);
     * per-level help art is keyed by page number instead (TUNGA.11 ships
     * POMOC32.SEQ), so also collect POMOC<NN>.SEQ payloads present in the
     * CD block that the MNU does not name (deterministic numeric order). */
    for (unsigned j = 0; j < ncdrec && np < 16; j++) {
        const char *nm = cdrecs[j].name;
        if (cdrecs[j].id != 1)
            continue;
        char stem[16];
        stem_of(nm, stem, sizeof stem);
        if (strncasecmp(stem, "POMOC", 5) != 0)
            continue;
        int dup = 0;
        for (int k = 0; k < nms; k++)
            if (ci_eq(mnu_seq[k], stem) || ci_eq(mnu_seq[k], nm)) {
                dup = 1;
                break;
            }
        if (dup)
            continue;
        pages[np].blob = NULL;
        if (llrs_read(cd, &cdrecs[j], &pages[np].blob) < 0 ||
            ll_seq_parse(pages[np].blob, (uint32_t)cdrecs[j].size,
                         &pages[np].seq) != 0) {
            free(pages[np].blob);
            pages[np].blob = NULL;
            continue;
        }
        const ll_seq_elem *fe = NULL;
        for (unsigned el = 0; el < pages[np].seq.count; el++)
            if (pages[np].seq.els[el].arr_n > 0) {
                fe = &pages[np].seq.els[el];
                break;
            }
        pages[np].x = pages[np].y = 0;
        if (fe) {
            int ex = -(int)(short)fe->f0, ey = -(int)(short)fe->f2;
            if (ex > 0)
                pages[np].x = ex;
            if (ey > 0)
                pages[np].y = ey;
        }
        printf("ll_level: help: %s page %d %s at (%d,%d) [CD]\n",
               mnu->name, np, nm, pages[np].x, pages[np].y);
        np++;
    }
    free(raw);
    if (!np)
        return;

    const char *hs = getenv("LEO_HELP_SECS");
    /* E-02: production behavior is the original's - the page stays until
     * a key or click closes it. The auto-close timeout exists only for
     * CI/headless flow and applies only when explicitly set. */
    double lim = hs ? atof(hs) : -1.0;
    int auto_close = lim >= 0.0;              /* scene timer starts after */
    uint64_t t_end = plat_now_ns() +
                     (uint64_t)((auto_close ? lim : 0.0) * 1e9);
    int page = 0, quit = 0;
    uint64_t t_tick = plat_now_ns();   /* FIX-07: the same fixed grid as
                                        * the level loop and the CI legs:
                                        * one LL_ITER_MS iteration per
                                        * frame (PR-135) */
    while (!quit && (!auto_close || plat_now_ns() < t_end)) {
        ll_event e;
        while (plat_poll(&e)) {               /* F11/Alt+Enter are consumed
                                               * by the backend here too */
            if (e.type == LL_EV_QUIT)
                quit = 2;
            else if (e.type == LL_EV_BUTTON_DOWN)
                quit = 1;                     /* click closes, like a key */
            else if (e.type == LL_EV_KEYDOWN) {
                int k = e.key;                /* ASCII verbatim or LLKEY_* */
                if (k == LLKEY_ESC && !e.repeat)
                    /* FIX-03 (X-04): holding ESC must close the help
                     * page ONLY - the repeats must not leak through and
                     * also ESC the level itself. Page keys (RIGHT/
                     * RETURN/SPACE/PGDN) keep auto-repeat on purpose. */
                    quit = 1;
                else if (k == LLKEY_RIGHT || k == LLKEY_RETURN ||
                         k == ' ' || k == LLKEY_PGDN)
                    page = (page + 1) % np;
                else if (k == LLKEY_LEFT || k == LLKEY_PGUP)
                    page = (page + np - 1) % np;
            }
        }
        if (quit)
            break;
        memcpy(canvas, bg, (size_t)w * (size_t)h);
        const ll_seq *s = &pages[page].seq;
        for (unsigned el = 0; el < s->count; el++)
            if (s->els[el].arr_n > 0) {
                ll_seq_draw_elem(s, &s->els[el], canvas, w, h,
                                 pages[page].x, pages[page].y);
                break;
            }
        paint(canvas, lut, w, h);
        const char *hppm = getenv("LEO_HELP_PPM"); /* CI: help frame dump */
        static int hdumped = 0;
        if (hppm && !hdumped) {
            hdumped = 1;
            unsigned char pal[768];
            for (int i = 0; i < 256; i++) {
                pal[3 * i] = (unsigned char)(lut[i] >> 16);
                pal[3 * i + 1] = (unsigned char)(lut[i] >> 8);
                pal[3 * i + 2] = (unsigned char)lut[i];
            }
            scene_ppm(hppm, w, h, canvas, pal);
        }
        t_tick += (uint64_t)LL_ITER_MS * 1000000ull;  /* FIX-07: fixed grid */
        {
            uint64_t n = plat_now_ns();
            if (n > t_tick + 100000000ull)   /* >100 ms behind: resync, */
                t_tick = n;                  /* never fast-forward */
        }
        plat_sleep_until(t_tick);
    }
    for (int k = 0; k < np; k++) {
        ll_seq_free(&pages[k].seq);
        free(pages[k].blob);
    }
    if (quit == 1)
        printf("ll_level: help: ESC\n");
}

int ll_level_show(const char *dir, const char *level, double secs, int audio)
{
    const char *hs = getenv("LEO_LEVEL_SECS"); /* CI override */
    if (hs)
        secs = atof(hs);
    int rc = 0;                 /* every failure path still runs `done:` */
    char p[4096];
    llrs leveldes, etapy;
    snprintf(p, sizeof p, "%s/LEVELDES", dir);
    if (llrs_open(&leveldes, p)) {
        /* E-01: name the DATA problem, not just the file: a bare
         * "cannot open" with no context is where support threads start. */
        fprintf(stderr, "ll_level: cannot open %s - %s is not a DATA "
                        "directory (no LEVELDES container there)\n", p, dir);
        return 1;
    }
    snprintf(p, sizeof p, "%s/ETAPY", dir);
    if (llrs_open(&etapy, p)) {
        fprintf(stderr, "ll_level: cannot open %s - %s has LEVELDES but no "
                        "ETAPY container\n", p, dir);
        llrs_close(&leveldes);
        return 1;
    }

    /* 1) main .DES: exact name, "<level>.DES", stem ".DES", then a scan of
     *    all *.DES payloads matching their own dir= value (TUNGA.11 is
     *    carried by TUNGA1.DES: dir=tunga.11). */
    const llrs_slot *des = llrs_find(&leveldes, level);
    if (!des) {
        char cand[32];
        snprintf(cand, sizeof cand, "%s.DES", level);
        des = llrs_find(&leveldes, cand);
    }
    if (!des) {
        char stem[16];
        stem_of(level, stem, sizeof stem);
        char cand[32];
        snprintf(cand, sizeof cand, "%s.DES", stem);
        des = llrs_find(&leveldes, cand);
    }
    unsigned char *txt = NULL;
    if (des && llrs_read(&leveldes, des, &txt) < 0)
        txt = NULL;
    if (!txt) {                       /* dir= scan over every .DES payload */
        for (unsigned k = 0; k < leveldes.cap && !txt; k++) {
            const llrs_slot *s = &leveldes.slots[k];
            if (s->id != 1 || !s->name[0])
                continue;
            const char *dot = strrchr(s->name, '.');
            if (!dot || !ci_eq(dot, ".DES"))
                continue;
            unsigned char *cand = NULL;
            if (llrs_read(&leveldes, s, &cand) < 0)
                continue;
            des_kv tmp[8];
            int nt = des_parse((char *)cand, (size_t)s->size, tmp, 8);
            if (nt && ci_eq(des_get(tmp, nt, "dir"), level)) {
                txt = cand;
                des = s;
            } else
                free(cand);
        }
    }
    des_kv kvs[64];
    int nk = 0;
    if (txt) {
        printf("ll_level: %s wiring:\n", des->name);
        nk = des_parse((char *)txt, (size_t)des->size, kvs, 64);
        for (int k = 0; k < nk; k++)
            printf("  %-12s = %s\n", kvs[k].key, kvs[k].val);
    } else {
        printf("ll_level: no .DES for '%s' (showing ETAPY block only)\n",
               level);
    }

    /* World extent: back_rect=x0,y0,w,h (RAN 28800x400, TUNGA.11
     * 2560x600, ENA1 5169x574, JUNCTION 320x200 = static one-screen).
     * back_rect is absent on SADES/TUNGA2/ZARIA* (CD hole) -> no camera
     * (world = one view). */
    int world_x0 = 0, world_y0 = 0, world_w = 0, world_h = 0;
    {
        const char *br = des_get(kvs, nk, "back_rect");
        if (br && *br)
            sscanf(br, "%d,%d,%d,%d", &world_x0, &world_y0,
                   &world_w, &world_h);
    }

    /* 2) level block in ETAPY: exact, stem, then dir= from the .DES */
    const llrs_slot *blk = find_or_stem(&etapy, level);
    if (!blk && nk) {
        const char *d = des_get(kvs, nk, "dir");
        if (d[0])
            blk = find_or_stem(&etapy, d);
    }
    llrs_slot *recs = NULL;
    unsigned nrec = 0;
    if (blk)
        llrs_block(&etapy, blk, &recs, &nrec);

    /* 3) background layer stack: tlo= lists the layers in stack order
     * ("plan3.pcx,plan2.pcm,plan1.pcm").  Pick the FIRST listed layer
     * present in the level's ETAPY block: a .PCX wins the canvas; a
     * .PCM (parallax plane, ll_pcm) becomes the canvas only when no
     * .PCX precedes it in the stack (RAN.01 on this CD: plan3.pcx and
     * plan2.pcm are absent, plan1.pcm is present -> PCM canvas).
     * Fallback: any .PCX in the block (KADMAR2's M15.PCX intro art). */
    char bgname[32] = "";
    int pcm_rec[MAX_PCM_LAYERS];    /* present tlo .PCM records, tlo order */
    int pcm_tix[MAX_PCM_LAYERS];    /* each one's index in the FULL tlo list */
    int ntlo = 0;                   /* tlo tokens, present or not */
    int npcm_rec = 0;
    const llrs_slot *bg = NULL;
    int bgpref = -1;                /* block index of first present tlo layer */
    int bg_tix = 0;                 /* tlo index of the backdrop .PCX         */
    {
        char tlo[512];
        snprintf(tlo, sizeof tlo, "%s", des_get(kvs, nk, "tlo"));
        char *save = NULL;
        for (char *tok = strtok_r(tlo, ",", &save); tok;
             tok = strtok_r(NULL, ",", &save)) {
            while (*tok == ' ')
                tok++;
            const int tix = ntlo++;
            const char *dot = strrchr(tok, '.');
            if (!dot || !(ci_eq(dot, ".PCX") || ci_eq(dot, ".PCM")))
                continue;
            char stem[16];
            stem_of(tok, stem, sizeof stem);
            for (unsigned k = 0; k < nrec; k++) {
                char st2[16];
                stem_of(recs[k].name, st2, sizeof st2);
                if (recs[k].id == 1 && ci_eq(st2, stem)) {
                    if (bgpref < 0)
                        bgpref = (int)k;
                    if (!bg && ci_eq(dot, ".PCX")) {
                        bg = &recs[k];
                        bg_tix = tix;
                    }
                    if (ci_eq(dot, ".PCM") && npcm_rec < MAX_PCM_LAYERS) {
                        int dup = 0;
                        for (int q = 0; q < npcm_rec; q++)
                            dup |= pcm_rec[q] == (int)k;
                        if (!dup) {
                            pcm_tix[npcm_rec] = tix;
                            pcm_rec[npcm_rec++] = (int)k;
                        }
                    }
                    break;
                }
            }
        }
    }
    if (!bg && npcm_rec == 0)   /* PR-53: only when no PCM layer can be the
                                 * scene (KADMAR2's M15.PCX was a stand-in
                                 * for the layer that could not be shown) */
        for (unsigned k = 0; k < nrec; k++) {
            const char *dot = strrchr(recs[k].name, '.');
            if (recs[k].id == 1 && dot && ci_eq(dot, ".PCX")) {
                bg = &recs[k];
                break;
            }
        }
    if (bg)
        snprintf(bgname, sizeof bgname, "%s", bg->name);

    ll_pcx img;
    memset(&img, 0, sizeof img);
    int esc = 0;
    if (bg) {
        unsigned char *raw = NULL;
        if (llrs_read(&etapy, bg, &raw) >= 0) {
            if (ll_pcx_decode(raw, (size_t)bg->size, &img) != 0)
                printf("ll_level: PCX decode failed for %s\n", bg->name);
            free(raw);
        }
    }
    /* tlo= names the whole background stack back to front (.pcx + .pcm).
     * The .PCX is the backdrop; every present .PCM is a TRANSPARENT overlay
     * decoded here and painted over it each frame (layers_at, ll_pcm.h). */
    ll_pcm lay[MAX_PCM_LAYERS];
    int lay_tix[MAX_PCM_LAYERS];
    int nlay = 0;
    memset(lay, 0, sizeof lay);
    for (int q = 0; q < npcm_rec; q++) {
        const llrs_slot *rs = &recs[pcm_rec[q]];
        unsigned char *raw = NULL;
        if (llrs_read(&etapy, rs, &raw) < 0)
            continue;
        ll_pcm p2;
        int rc2 = ll_pcm_decode(raw, (size_t)rs->size, &p2);
        free(raw);
        if (rc2 != 0) {
            printf("ll_level: PCM %s: decode failed (malformed layer, size"
                   " %d) - not drawn\n", rs->name, rs->size);
            continue;
        }
        size_t painted = 0;
        for (size_t i = 0; i < (size_t)p2.w * p2.h; i++)
            painted += p2.op[i];
        printf("ll_level: PCM %s: transparent overlay layer %u px x %u rows"
               " (%u strips of 80, tiles of %u), %.1f%% painted, fnv=%016llx"
               " -> layer %d over %s\n", rs->name, p2.w, p2.h, p2.nstrips,
               p2.tile, 100.0 * (double)painted / ((double)p2.w * p2.h),
               (unsigned long long)ll_pcm_fnv(&p2), nlay + 1,
               bg ? bg->name : "an empty backdrop");
        lay_tix[nlay] = pcm_tix[q];
        lay[nlay++] = p2;
    }

    par_t par;
    par_parse(&par, des_get(kvs, nk, "speed"), des_get(kvs, nk, "main_back"),
              ntlo);
    printf("ll_level: danger_power=%s (the damage a DANGER object deals, the\n"
           "ll_level:   game keeps it in level+0x54)\n",
           des_get(kvs, nk, "danger_power")[0] ? des_get(kvs, nk, "danger_power")
                                              : "(not authored)");
    printf("ll_level: parallax: %d tlo layers, reference %d, speed%%", par.n,
           par.ref);
    for (int q = 0; q < par.n; q++)
        printf(" %d,%d", par.sx[q], par.sy[q]);
    printf("\n");
    /* PR-208: the same .des also says WHICH CELLS RIDE WHICH PLANE. */
    plan_t plan;
    {
        int ncell = des_list_cells(des_get(kvs, nk, "animacje"), NULL);
        plany_parse(&plan, des_get(kvs, nk, "plany"), par.n, ncell);
        printf("ll_level: planies: %d layer specs:", plan.nlayer);
        for (int q = 0; q < plan.nlayer; q++)
            printf(" .%s.", plan.spec[q]);
        if (!des_get(kvs, nk, "plany")[0])
            printf(": the .des authors no plany= key: nothing says which"
                   " plane the placements ride\n");
        else if (!plan.ok)
            printf(": the %d A letters in %d spec(s) do not line up with the"
                   " %d animacje cell(s) of the list; the segment law still"
                   " sets every placement plane from the raw spec (PR-213,"
                   " engine pass 0x4b1d0) - only the named-cell report is"
                   " dropped\n", plan.nA, plan.nlayer, ncell);
        else {
            printf(": animacje");
            for (int q = 0; q < ncell; q++)
                printf(" %d->layer %d", q, plan.cell_layer[q]);
            int nT = 0, nL = 0, nB = 0;
            for (int q = 0; q < plan.nlayer; q++)
                for (const char *c = plan.spec[q]; *c; c++) {
                    nT += *c == 'T';
                    nL += *c == 'L';
                    nB += *c == 'B';
                }
            printf(" (the A letters name %d of the %d cell%s in the list;"
                   " the specs' letters count T:%d L:%d B:%d - T is that"
                   " layer's own art, L the lew= lion, B the belka= belt;"
                   " the segment carrying a cell's A is that cell's plane"
                   " for all three keys - PR-213, engine pass 0x4b1d0)\n",
                   plan.ncell_named, ncell, ncell == 1 ? "" : "s",
                   nT, nL, nB);
        }
    }
    if (world_w == 0)
        for (int k = 0; k < nlay; k++)
            if (lay_tix[k] == par.ref) {
                /* PR-65: no back_rect (SADES, TUNGA2, ZARIA.01/.02: the CD
                 * hole). The engine's camera is clamped to the reference
                 * layer main_back (FUN_00057590 reads its extent), and the
                 * reference layer's own art is the world: ZARIA.01's PLAN1
                 * is 2240x700 - the same size KADMAR.07 authors as back_rect
                 * - and a playthrough video scrolls over such a world. */
                world_w = (int)lay[k].w;
                world_h = (int)lay[k].h;
                printf("ll_level: no back_rect: world %dx%d taken from the"
                       " reference layer (main_back %d)\n", world_w, world_h,
                       par.ref);
            }
    /* PR-66: the game's view is 320x200 world units (0x140/0xc8 in the camera
     * clamp FUN_00057590 and the layer offsets), shown 2x as 640x400 - a
     * playthrough video shows exactly that (art ~2x the size of the native
     * sprites, a 640x400 picture inside the 640x480 mode). The scene canvas
     * is the 320x200 world window; the window magnifies it. */
    int cw = LVL_VIEW_W, ch = LVL_VIEW_H;
    unsigned char *view = NULL;
    unsigned char scenepal[768];
    memset(scenepal, 0, sizeof scenepal);
    if (img.idx) {
        memcpy(scenepal, img.pal, 768);
    } else if (nlay > 0) {
        /* PCM-only level (no backdrop PCX in the tlo stack, KADMAR2): the
         * window is a viewer choice (the engine blits 400x200 at a time),
         * the palette is the block's BACK.PCX - it equals the backdrop
         * palette on 8 of the 11 levels that have both, and it is the one
         * that makes KADMAR2's layer natural masonry instead of noise. */
        view = calloc((size_t)cw, (size_t)ch);
        int gray = 1;
        for (unsigned k = 0; k < nrec && gray; k++)
            if (recs[k].id == 1 && ci_eq(recs[k].name, "BACK.PCX")) {
                unsigned char *rawb = NULL;
                ll_pcx bk;
                memset(&bk, 0, sizeof bk);
                if (llrs_read(&etapy, &recs[k], &rawb) >= 0) {
                    if (ll_pcx_decode(rawb, (size_t)recs[k].size, &bk) == 0) {
                        memcpy(scenepal, bk.pal, 768);
                        gray = 0;
                    }
                    ll_pcx_free(&bk);
                    free(rawb);
                }
            }
        if (gray)
            for (int i = 0; i < 256; i++)    /* documented fallback: gray */
                scenepal[3 * i] = scenepal[3 * i + 1] =
                    scenepal[3 * i + 2] = (unsigned char)i;
        snprintf(bgname, sizeof bgname, "%s", recs[pcm_rec[0]].name);
    }
    if (!img.idx && !view) {
        printf("ll_level: no background PCX for '%s' "
               "(wiring wants '%s' - missing from these containers)\n",
               level, des_get(kvs, nk, "tlo"));
        for (int q = 0; q < nlay; q++)
            ll_pcm_release(&lay[q]);
        llrs_block_free(recs);
        free(txt);
        llrs_close(&leveldes);
        llrs_close(&etapy);
        return 1;
    }

    /* 4) the authored scene: what the level's OWN placement data places
     *    (PR-45). The three placement keys of the main .des name the
     *    sub-.des files; every entry in them names an ANIMACJE leaf; that
     *    leaf's .MOV on the HD install is the motion script and each of its
     *    states names its own .SEQ art through seq= (on the CD).
     *    A missing HD install means no scripts (the placement files
     *    themselves live in the HD container); a missing CD ANIMACJE means
     *    the art of the scripts that are present is missing. Both are
     *    reported by exact name as data holes, and nothing is invented:
     *    PR-36's fallback scan of every leaf .SEQ and its one-script-per-
     *    leaf collapsing are gone, and so is the 24-object cap (measured:
     *    RAN.01 places 827 objects from 82 scripts, SADES.06 uses 264
     *    scripts and 277 art tables - the arrays below grow instead). */
    play_ctx pl;
    memset(&pl, 0, sizeof pl);             /* LEO_PLAY wiring (4c below)   */
    pl.layer = par.ref;                    /* the lion lives on main_back  */
    {   /* PR-71: the contact pass is ON for every level but RAN.01 (the
         * balloon-flight level: its walking start is not what the game
         * plays there); LEO_PLAY_SOLID=0/1 forces it either way */
        const char *sv = getenv("LEO_PLAY_SOLID");
        pl.solid = (sv && *sv) ? *sv != '0' : !ci_eq(level, "RAN.01");
        /* PR-90: the DANGER + class MAIN box arming stand-in is a MODEL
         * (the pinned box applied to DANGER-flagged states), not a
         * recovered driver (the engine's real per-class AI is open, see
         * obj_arm_atak) - OFF unless LEO_PLAY_DANGER=1 asks for it. */
        pl.danger = getenv("LEO_PLAY_DANGER")
                        ? atoi(getenv("LEO_PLAY_DANGER")) != 0 : 0;
    }
    pl.cam_ox = CAM_LEAD_R;
    /* PR-84: the follow-cam-y state (play_ctx camy_v/camy_set) starts
     * empty: follow_camy snaps the view to the measured feet anchor at
     * its first call. LEO_LEVEL_CAMY still seeds the PREVIEW camera and
     * is overridden in play mode, where the game's own camera rule runs. */
    pl.energy = 100;                       /* 0x503ac: energy 100 at (re)start */
    pl.lives = LL_LIVES_START;             /* 0x5d2f9, PR-530/OI-083:   */
                                           /* the shipped run start is 5; */
                                           /* PR-68's 6 came from the     */
                                           /* video and is now spent.     */
    hud_t hud;
    memset(&hud, 0, sizeof hud);
    scene_owner scene;                     /* scripts + placements, owned;
                                           * freed at done: (E-05)         */
    memset(&scene, 0, sizeof scene);

    llrs an, hd_an, hd_ety;
    memset(&an, 0, sizeof an);
    memset(&hd_an, 0, sizeof hd_an);
    memset(&hd_ety, 0, sizeof hd_ety);
    int an_open = 0, have_hd = 0, have_hd_ety = 0;
    snprintf(p, sizeof p, "%s/ANIMACJE", dir);
    const char *hd = getenv("LEO_HD_DIR");
    if (hd) {
        char hp[4096];
        snprintf(hp, sizeof hp, "%s/ANIMACJE", hd);
        have_hd = llrs_open(&hd_an, hp) == 0;
    }
    const llrs_slot *ablk = NULL, *hblk = NULL, *hblk2 = NULL;
    if (llrs_open(&an, p) == 0) {
        an_open = 1;
        ablk = find_or_stem(&an, level);
    }
    if (have_hd)
        hblk = find_or_stem(&hd_an, level);
    if (hd) {
        char hp[4096];
        snprintf(hp, sizeof hp, "%s/ETAPY", hd);
        have_hd_ety = llrs_open(&hd_ety, hp) == 0;
        if (have_hd_ety) {
            hblk2 = find_or_stem(&hd_ety, level);
            if (!hblk2 && nk) {
                const char *d = des_get(kvs, nk, "dir");
                if (d[0])
                    hblk2 = find_or_stem(&hd_ety, d);
            }
        }
    }

    place_ctx pc;
    memset(&pc, 0, sizeof pc);
    pc.own = &scene;
    pc.level = level;
    pc.plan = plan;                 /* PR-208: the level's own plane law   */
    pc.world_w = world_w > 0 ? world_w : (int)img.w;
    pc.ac.cd = an_open ? &an : NULL;
    pc.ac.cd_blk = ablk;
    pc.ac.hd = have_hd ? &hd_an : NULL;
    pc.ac.hd_blk = hblk;
    if (!an_open)
        printf("ll_level: no CD ANIMACJE for %s in %s: the art the object"
               " scripts name is unreachable (data hole)\n", level, dir);
    if (!have_hd)
        printf("ll_level: no HD ANIMACJE for %s (LEO_HD_DIR is not set or"
               " that install has no ANIMACJE): the object scripts of this"
               " level are unreachable (data hole)\n", level);
    des_placement(&pc, have_hd_ety ? &hd_ety : NULL, hblk2, kvs, nk);
    hud_load(&hud, have_hd_ety ? &hd_ety : NULL, hblk2, &etapy, recs, nrec,
             des_get(kvs, nk, "belka"));
    lvl_obj *objs = scene.objs;        /* the print/draw loops below */
    int nobj = scene.nobj;
    if (an_open) {
        llrs_close(&an);
        an_open = 0;                   /* done: must not close twice */
    }
    if (have_hd) {
        llrs_close(&hd_an);
        have_hd = 0;
    }
    /* hd_ety stays open: the help overlay (5b) reads the same HD block */
    /* hd_ety stays open: the help overlay (5b) reads the same HD block */

    /* 4c) playable level prototype (M4-B, PR-43): LEO_PLAY upgrades the
     * preview into the real controllable lion - art from the `lew=` leaf's
     * .SEQ scripts, motion from that leaf's HD .MOV (F=/T=/G= physics,
     * K_* transitions), ground from the `platformy=` target (a .PLT or a
     * sub-.des holding the same polylines), spawn from
     * `pozycja=`, bounds from `lew_rect=` (all of it RAN.01/TUNGA.11
     * data; simulation in ll_play.c). A missing piece is a data hole:
     * logged, and the level stays a preview. */
    if (getenv("LEO_PLAY")) {
        char ap[4096];
        char leafname[24] = "", movstem[16] = "";
        const char *lew = des_get(kvs, nk, "lew");
        if (lew[0]) {
            /* PR-526: the FIRST field is the lion class name the engine
             * matches in its factory chain; keep it verbatim so the play
             * path can honour the class's own rules (the end-of-stage
             * arm at 0x1e250 is ZARIA-TLewZaria-only). */
            const char *c0 = strchr(lew, ',');
            size_t n = c0 ? (size_t)(c0 - lew) : strlen(lew);
            if (n >= sizeof pl.lion_class)
                n = sizeof pl.lion_class - 1;
            memcpy(pl.lion_class, lew, n);
            pl.lion_class[n] = '\0';
            /* lew=<class>,<art leaf>,<mov>,<player des> (RAN.01:
             * "TLewRan,Lew.ran,lew.mov,lewran.des") */
            char tw[32] = "";
            const char *c1 = strchr(lew, ',');
            const char *c2 = c1 ? strchr(c1 + 1, ',') : NULL;
            if (c1) {
                size_t n = c2 ? (size_t)(c2 - c1 - 1) : strlen(c1 + 1);
                if (n >= sizeof tw)
                    n = sizeof tw - 1;
                memcpy(tw, c1 + 1, n);
                tw[n] = '\0';
                stem_of(tw, leafname, sizeof leafname);
                const char *dot = strrchr(tw, '.');
                if (dot && dot[1])
                    snprintf(leafname + strlen(leafname),
                             sizeof leafname - strlen(leafname), ".%s",
                             dot + 1);
            }
            if (c2) {
                const char *c3 = strchr(c2 + 1, ',');
                char mt[32];
                size_t n = c3 ? (size_t)(c3 - c2 - 1) : strlen(c2 + 1);
                if (n >= sizeof mt)
                    n = sizeof mt - 1;
                memcpy(mt, c2 + 1, n);
                mt[n] = '\0';
                stem_of(mt, movstem, sizeof movstem);
            }
        }
        if (!movstem[0])
            stem_of(leafname[0] ? leafname : "LEW", movstem,
                    sizeof movstem);

        /* CD: every .SEQ of the player art leaf (all states share the
         * leaf; each per-state seq= names one of them) */
        snprintf(ap, sizeof ap, "%s/ANIMACJE", dir);
        llrs can;
        memset(&can, 0, sizeof can);
        if (llrs_open(&can, ap) == 0) {
            const llrs_slot *cb = find_or_stem(&can, level);
            llrs_slot *crecs = NULL;
            unsigned ncrec = 0;
            if (cb && llrs_block(&can, cb, &crecs, &ncrec) == 0) {
                const llrs_slot *lf = NULL;
                for (unsigned k = 0; k < ncrec; k++)
                    if (crecs[k].id == 2 && crecs[k].name[0] &&
                        leafname[0] && ci_eq(crecs[k].name, leafname)) {
                        lf = &crecs[k];
                        break;
                    }
                if (!lf)
                    for (unsigned k = 0; k < ncrec; k++) {
                        char st[16];
                        if (crecs[k].id != 2 || !crecs[k].name[0])
                            continue;
                        stem_of(crecs[k].name, st, sizeof st);
                        if (strncasecmp(st, "LEW", 3) == 0) {
                            lf = &crecs[k];
                            printf("ll_level: play: no leaf '%s' in %s:"
                                   " using '%s' (data-hole note)\n",
                                   leafname, level, crecs[k].name);
                            break;
                        }
                    }
                if (lf) {
                    snprintf(pl.leaf, sizeof pl.leaf, "%s", lf->name);
                    llrs_slot *lrec = NULL;
                    unsigned nlrec = 0;
                    if (llrs_block(&can, lf, &lrec, &nlrec) == 0) {
                        for (unsigned j = 0; j < nlrec && pl.nart < 32;
                             j++) {
                            const char *dot = strrchr(lrec[j].name, '.');
                            if (lrec[j].id != 1 || !lrec[j].name[0] ||
                                !dot || !ci_eq(dot, ".SEQ"))
                                continue;
                            int ai = pl.nart;
                            if (llrs_read(&can, &lrec[j],
                                          &pl.art[ai].blob) < 0 ||
                                ll_seq_parse(pl.art[ai].blob,
                                             (uint32_t)lrec[j].size,
                                             &pl.art[ai].seq) != 0) {
                                free(pl.art[ai].blob);
                                pl.art[ai].blob = NULL;
                                continue;
                            }
                            stem_of(lrec[j].name, pl.art[ai].stem,
                                    sizeof pl.art[ai].stem);
                            printf("ll_level: play: art %s/%s: %u"
                                   " element(s)\n", lf->name,
                                   lrec[j].name,
                                   (unsigned)pl.art[ai].seq.count);
                            pl.nart++;
                        }
                        llrs_block_free(lrec);
                    }
                }
            }
            llrs_block_free(crecs);
            llrs_close(&can);
        }

        /* HD: the player .MOV (same leaf name inside HD ANIMACJE) */
        if (hd && *hd) {
            llrs han;
            memset(&han, 0, sizeof han);
            snprintf(ap, sizeof ap, "%s/ANIMACJE", hd);
            if (llrs_open(&han, ap) == 0) {
                const llrs_slot *hb = find_or_stem(&han, level);
                llrs_slot *hrecs2 = NULL;
                unsigned nhrec2 = 0;
                if (hb && llrs_block(&han, hb, &hrecs2, &nhrec2) == 0) {
                    const llrs_slot *hl = NULL;
                    for (unsigned k = 0; k < nhrec2; k++)
                        if (hrecs2[k].id == 2 && hrecs2[k].name[0] &&
                            pl.leaf[0] &&
                            ci_eq(hrecs2[k].name, pl.leaf))
                            hl = &hrecs2[k];
                    if (hl) {
                        llrs_slot *mrecs = NULL;
                        unsigned nmrec = 0;
                        if (llrs_block(&han, hl, &mrecs, &nmrec) == 0) {
                            const llrs_slot *mv = rec_find(mrecs, nmrec,
                                                           movstem,
                                                           ".MOV");
                            if (!mv) {
                                /* Safety net: the .des names the script
                                 * and all 13 of them name one that
                                 * exists (measured 2026-11-16), so this
                                 * never fires today - but we NEVER guess
                                 * a .MOV (ZARIA's leaves carry an extra
                                 * LEWPL.MOV that its .des does not
                                 * name), so if a future level's name
                                 * does not resolve, print what IS there
                                 * and make the gap diagnosable. */
                                for (unsigned k = 0; k < nmrec; k++) {
                                    const char *dot
                                        = strrchr(mrecs[k].name, '.');
                                    if (mrecs[k].id == 1 &&
                                        mrecs[k].name[0] && dot &&
                                        ci_eq(dot, ".MOV"))
                                        printf("ll_level: play: leaf %s"
                                               " has no %s.MOV, it"
                                               " carries %s (unused: the"
                                               " .des names the"
                                               " script)\n", pl.leaf,
                                               movstem, mrecs[k].name);
                                }
                            }
                            unsigned char *mtxt = NULL;
                            char *minj = NULL;
                            long mlen = mv ? llrs_read(&han, mv, &mtxt) : 0;
                            if (mlen > 0) {
                                /* PR-199: only when LEO_PLAY_ENDLV asks
                                 * (see mov_inject_endlv: why the
                                 * injection exists and what it does and
                                 * does not prove); with the variable
                                 * absent minj stays NULL and the parse
                                 * reads the member's own bytes. */
                                minj = mov_inject_endlv((char *)mtxt,
                                                        (size_t)mlen);
                                pl.mov = calloc(1, sizeof *pl.mov);
                                if (!pl.mov ||
                                    ll_mov_parse(minj ? minj
                                                      : (char *)mtxt,
                                                 pl.mov) <= 0) {
                                    free(pl.mov);
                                    pl.mov = NULL;
                                } else
                                    printf("ll_level: play: %s: %d"
                                           " state(s), fallback art"
                                           " '%s'\n", mv->name,
                                           pl.mov->nst, pl.mov->seqkey);
                            }
                            free(minj);
                            free(mtxt);
                            llrs_block_free(mrecs);
                        }
                    }
                }
                llrs_block_free(hrecs2);
                llrs_close(&han);
            }
        }

        /* HD ETAPY: the ground polylines named by platformy=. TWO forms
         * exist in the real data (measured 2026-11-16 over all 13 level
         * blocks):
         *   platformy=<stem>.plt  -> the level block's <stem>.PLT record
         *                           (11 levels: ran.plt, tunga.plt, ...)
         *   platformy=<name>.des  -> a SUB-.DES record of the same level
         *                           block whose TEXT carries the very
         *                           same polyline grammar: TUNGA2.12 and
         *                           SADES.06 say platf.des and their
         *                           PLATF.DES record starts
         *                           "66\r\n1 1495,147;1529,147;" /
         *                           "8\r\n1 863,166;959,166;" - count
         *                           header and all. JUNCTION says
         *                           none.des and its NONE.DES record is
         *                           literally "0": that level has NO
         *                           ground, which is data, not a gap.
         * Same parser either way (ll_plt_parse). */
        const char *pltspec = des_get(kvs, nk, "platformy");
        char pltstem[16];
        stem_of(pltspec, pltstem, sizeof pltstem);
        const char *pltdot = pltspec ? strrchr(pltspec, '.') : NULL;
        int plt_is_des = pltdot && ci_eq(pltdot, ".des");
        int ground_authored_empty = 0;
        if (have_hd_ety && hblk2) {
            llrs_slot *erecs = NULL;
            unsigned nerec = 0;
            if (llrs_block(&hd_ety, hblk2, &erecs, &nerec) == 0) {
                const llrs_slot *pr =
                    pltstem[0]
                        ? rec_find(erecs, nerec, pltstem,
                                   plt_is_des ? ".DES" : ".PLT")
                        : NULL;
                if (!pr && !plt_is_des)
                    for (unsigned k = 0; k < nerec; k++) {
                        const char *dot = strrchr(erecs[k].name, '.');
                        if (erecs[k].id == 1 && erecs[k].name[0] && dot &&
                            ci_eq(dot, ".PLT")) {
                            pr = &erecs[k];
                            printf("ll_level: play: no '%s.PLT' in %s:"
                                   " using '%s' (data-hole note)\n",
                                   pltstem, level, pr->name);
                            break;
                        }
                    }
                if (pr) {
                    unsigned char *ptxt = NULL;
                    if (llrs_read(&hd_ety, pr, &ptxt) > 0 &&
                        ll_plt_parse((char *)ptxt, &pl.plt) == 0) {
                        ground_authored_empty = pl.plt.n == 0;
                        printf("ll_level: play: %s: %d polyline(s)",
                               pr->name, pl.plt.n);
                        if (plt_is_des)
                            printf(" (ground inside the sub-.des that"
                                   " platformy= names)");
                        if (ground_authored_empty)
                            printf(" - the data says this level has NO"
                                   " ground");
                        printf("\n");
                    }
                    free(ptxt);
                }
            }
            llrs_block_free(erecs);
        }

        /* spawn: pozycja=x,y feet, snapped down onto the first surface */
        const char *pos = des_get(kvs, nk, "pozycja");
        int px = 0, py = 0;
        if (pos && sscanf(pos, "%d,%d", &px, &py) == 2) {
            pl.sx = px;
            pl.sy = py;
        } else {
            printf("ll_level: play: no pozycja= in %s: spawning at world"
                   " left edge\n", level);
            pl.sx = 40;
            pl.sy = 0;
        }

        /* bounds: lew_rect=x0,y0;x1,y1 (feet clamp + world-bottom death);
         * fall back to back_rect, else no clamp. PR-532 READ: the
         * engine's own death line is NOT lew_rect: 0x54464-0x5446e takes
         * the main_back .PCM descriptor ([[tlo-ctrl+0x44]+main_back*4),
         * sx16(WORD [desc+0x2]) - the layer's height, the same word the
         * verified PR-57 parallax law reads at 0x5747f (minus 200) - and
         * adds 0x1f4. Measured per level (census /tmp/opencode/
         * pr532_line_census.log): ZARIA.01/.02 1200, RAN.01 900, ...;
         * it equals back_rect-height + 500 on all 9 levels that author
         * a back_rect, and the height of the reference art on the 4
         * CD-hole levels. lew_rect y1 stays the stand-in until PR-533
         * ports the real line. */
        const char *lr = des_get(kvs, nk, "lew_rect");
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        if (lr && sscanf(lr, "%d,%d;%d,%d", &x0, &y0, &x1, &y1) == 4 &&
            x1 > x0) {
            pl.xmin = x0;
            pl.xmax = x1;
            pl.ydead = y1;
        } else if (world_w > 0) {
            printf("ll_level: play: no lew_rect= in %s: using back_rect"
                   " bounds\n", level);
            pl.xmin = world_x0;
            pl.xmax = world_x0 + world_w;
            pl.ydead = world_h > 0 ? world_h : 1e30;
        } else {
            pl.xmin = -1e30;
            pl.xmax = 1e30;
            pl.ydead = 1e30;
        }

        if (pl.mov && pl.mov->nst > 0 && pl.nart > 0 && pl.plt.n > 0) {
            pl.have = 1;
            play_spawn(&pl);
            {
                int moved = 0;
                for (int k = 0; k < nobj; k++)
                    moved += obj_settle(&objs[k], &pl.plt);
                printf("ll_level: play: %d ground-bound placement(s) settled"
                       " onto the .PLT (CHECK_LN start state)\n", moved);
            }
            printf("ll_level: play: %s %d state(s) on %d polyline(s):"
                   " spawn feet (%d,%d) grounded=%d bounds %d..%d"                   /* lew_rect */
                   " death below %d\n", pl.leaf, pl.mov->nst, pl.plt.n,
                   (int)pl.p.x, (int)pl.p.y, pl.p.grounded,
                   (int)pl.xmin, (int)pl.xmax, (int)pl.ydead);
            {   /* PR-60 census: placements whose script has a SOLID state,
                 * by the key that placed them (zwierzaki/animacje/ani_plat) */
                int cnt[3][3] = { { 0 } };
                for (int k = 0; k < nobj; k++) {
                    const obj_script *sc = objs[k].sc;
                    int has = 0;
                    if (!sc || !sc->mov)
                        continue;
                    for (int q = 0; q < sc->mov->nst; q++) {
                        unsigned cc = sc->mov->st[q].ctrl;
                        if (cc & LL_CTRL_SOLID2)
                            has = has < 2 ? 2 : has;
                        else if (cc & LL_CTRL_SEMI_SOLID)
                            has = has < 3 ? 3 : has;
                        else if (cc & LL_CTRL_SOLID)
                            has = has < 1 ? 1 : has;
                    }
                    if (has)
                        cnt[objs[k].src < 3 ? objs[k].src : 0][has - 1]++;
                }
                printf("ll_level: play: placements with a SOLID state"
                       " (SOLID/SOLID2/SEMI): zwierzaki %d/%d/%d animacje"
                       " %d/%d/%d ani_plat %d/%d/%d\n", cnt[0][0], cnt[0][1],
                       cnt[0][2], cnt[1][0], cnt[1][1], cnt[1][2],
                       cnt[2][0], cnt[2][1], cnt[2][2]);
            }
            play_claim_leaf(objs, &nobj, &pl);
        } else {
            printf("ll_level: play: level %s %s: staying preview\n", level,
                   !pl.mov ? "lacks player .MOV"
                           : !pl.nart ? "lacks player .SEQ art"
                           : ground_authored_empty
                               ? "has NO ground in its own data"
                                 " (platformy= names a file declaring 0"
                                 " polylines)"
                               : "lacks ground (platformy=)");
        }
    }

    int nstatic = 0, nrectc = 0;
    for (int k = 0; k < nobj; k++)
        if (objs[k].static_img)
            nstatic++;
        else if (objs[k].sc && rect_class_id(objs[k].sc->leaf) >= 0)
            nrectc++;           /* PR-328: an authored box, not an
                                 * animated thing: no script, no art */
    if (nstatic)
        printf("ll_level: show %s: bg %s %dx%d, %d animated object(s), %d"
               " static image(s)\n", level,
               bgname, img.idx ? (int)img.w : cw, img.idx ? (int)img.h : ch,
               nobj - nstatic - nrectc, nstatic);
    else
        printf("ll_level: show %s: bg %s %dx%d, %d animated object(s)\n",
               level,
               bgname, img.idx ? (int)img.w : cw, img.idx ? (int)img.h : ch,
               nobj);
    printf("ll_level: view %dx%d world units, shown 2x as %dx%d\n", cw, ch,
           cw * 2, ch * 2);
    /* Proof of the join, bounded: 24 placements in file order, then the
     * count of the rest (RAN.01 places 827). LEO_OBJ_ALL=1 prints them all
     * for probes. The line names leaf / script / the art the script's own
     * seq= selected / the state count / the authored position. */
    int obj_all = getenv("LEO_OBJ_ALL") != NULL;
    int obj_shown = 0;
    for (int k = 0; k < nobj; k++) {
        if (!obj_all && obj_shown >= 24)
            break;
        const obj_script *sc = objs[k].sc;
        /* PR-328: a rectangle-class placement has no script and no art by
         * authoring (PR-327: the class line IS the object, and all it
         * carries is the box), so it is neither "[mov]" nor a data hole;
         * what it has is named instead. */
        if (rect_class_id(sc->leaf) >= 0) {
            printf("ll_level: obj %s[rectangle class]: no script, no art -"
                   " the line authors the box %d,%d;%d,%d and nothing else"
                   " (the record verb 0x40c90 writes it at [obj+0x3c])\n",
                   sc->leaf, objs[k].arx0, objs[k].ary0, objs[k].arx1,
                   objs[k].ary1);
            obj_shown++;
            continue;
        }
        int ai = objs[k].sti >= 0 && objs[k].sti < LMOV_MAXST
                     ? sc->ast[objs[k].sti] : -1;
        const obj_art *a = ai >= 0 && ai < sc->nart ? &sc->art[ai] : NULL;
        const char *why =
            !a ? " (this state names no seq= art)"
               : !a->found ? " (no such record on these containers: data hole)"
               : a->eidx0 < 0 ? " (shape margins only, no patch art: an"
                              " invisible solid, undrawn by the original game"
                              " too)"
               : "";
        printf("ll_level: obj %s/%s%s: art %s%s, %d state(s), bbox %dx%d at"
               " (%d,%d)\n", sc->leaf, sc->movname,
               objs[k].static_img ? "[static image]"
               : sc->mov && sc->mov->nst ? "[mov]"
               : "[no script: data hole]",
               a && a->name[0] ? a->name : "(none)", why,
               sc->mov ? sc->mov->nst : 0, a ? a->sw : 0, a ? a->sh : 0,
               objs[k].x, objs[k].y);
        obj_shown++;
    }
    if (obj_shown < nobj)
        printf("ll_level: ... %d more placement(s) of those scripts"
               " (LEO_OBJ_ALL=1 prints them)\n", nobj - obj_shown);
    if (getenv("LEO_LEVEL_NO_OBJ")) {
        /* CI-only hook (PR-36): paint the bare background (no object
         * compositing, the logs and animation state above stay as-is). Kept
         * for the help-overlay oracle, which asserts "help frame = bare bg +
         * exactly the page window". */
        printf("ll_level: LEO_LEVEL_NO_OBJ -> %d objects not composited\n",
               nobj);
        nobj = 0;
    }

    /* Scroll camera over the back_rect world (horizontal only; the world
     * height exceeds the view vertically - no vertical pan recovered,
     * documented).  PCM layers pan through the engine strip wrap; .PCX
     * bgs pan by synthesised tiling (bg_at).  LEO_LEVEL_CAM = initial
     * camera x (CI); interactive: Left/Right pan, Home/End jump,
     * p toggles a 50 px/s autoscroll (the original walk speed). */
    int cam_max = world_w > cw ? world_w - cw : 0;
    int cam = getenv("LEO_LEVEL_CAM") ? atoi(getenv("LEO_LEVEL_CAM")) : 0;
    if (cam < 0)
        cam = 0;
    if (cam > cam_max)
        cam = cam_max;
    /* Vertical camera (PR-51, VIEWER POLICY - the game's own camera is not
     * recovered): the backdrop image is exactly the view size, so it cannot
     * scroll; objects and the lion scroll over it by camy. In play mode it
     * follows the lion's feet (65% down the view), LEO_LEVEL_CAMY sets it
     * for CI, Up/Down pan it in the viewer. Without this the lion spawned
     * below the view on 7 of the 12 playable levels (feet y 320..1504 vs
     * views 200..600 px tall) and was never on screen. Since PR-84 the
     * play-mode camera runs the measured game rule (feet-at-178 grounded
     * anchor, hold while airborne); LEO_LEVEL_CAMY seeds the PREVIEW only. */
    int cam_ymax = world_h > ch ? world_h - ch : 0;
    int camy = getenv("LEO_LEVEL_CAMY") ? atoi(getenv("LEO_LEVEL_CAMY")) : 0;
    if (camy < 0)
        camy = 0;
    if (camy > cam_ymax)
        camy = cam_ymax;
    int autoscroll = 0;
    if (world_w > 0)
        printf("ll_level: world %dx%d (back_rect %d,%d), view %dx%d, cam"
               " %d/%d [Left/Right pan, Home/End, p autoscroll]\n",
               world_w, world_h, world_x0, world_y0, cw, ch, cam, cam_max);
    else
        printf("ll_level: no back_rect (data hole): static one-screen view"
               " %dx%d\n", cw, ch);

    /* 5) composite canvas + present loop.  bg source: PCX plane, or the
     * PCM 400x200 rest view when the level is PCM-driven. */
    size_t csz = (size_t)cw * (size_t)ch;
    unsigned char *canvas = malloc(csz);
    if (!canvas) {
        rc = 1;                 /* E-05: OOM must still release everything
                                 * else allocated (done: below) */
        goto done;
    }
    const unsigned char *base = img.idx ? img.idx : view;
    paint_base(canvas, base, img.idx ? (int)img.w : cw,
               img.idx ? (int)img.h : ch, cw, ch, img.idx != NULL, &par,
               bg_tix, cam, camy, world_h);
    layers_at(lay, lay_tix, nlay, &par, cam, camy, cw, ch, world_w,
                  world_h, canvas);
    objs_draw(objs, nobj, canvas, cw, ch, cam, camy, &par, lay, lay_tix,
              nlay, world_h);

    const char *ppm = getenv("LEO_LEVEL_PPM");   /* CI: headless dump */
    if (ppm) {
        int ticks = getenv("LEO_LEVEL_PPM_TICK")
                        ? atoi(getenv("LEO_LEVEL_PPM_TICK"))
                        : 0;
        if (pl.have) {
            /* play leg (PR-43): one engine iteration per tick, i.e.
             * LL_ITER_MS = 50 ms (PR-135); LEO_PLAY_SCRIPT keys
             * (default "1:FWD": walk-right demo); camera stays 0 so the
             * frames are byte-deterministic for the PPM comparison legs.
             * Scene objects advance on the same grid. */
            const char *scr = getenv("LEO_PLAY_SCRIPT");
            if (!scr)
                scr = "1:FWD";
            /* PR-194: the stage-driver contract, inert unless
             * LEO_LEVEL_EXIT names codes (see the block at
             * stage_apply: for the reading and the honest gaps). */
            stage_inj inj[STAGE_MAX_INJ];
            int ninj = stage_inj_parse(getenv("LEO_LEVEL_EXIT"), inj);
            for (int tk = 1; tk <= ticks; tk++) {
                /* PR-526: THE FIRST GUARD, in the engine's own order:
                 * 0x4ac3b-0x4ac50 tests [[ecx+0x394]+0x116] - the
                 * LEVEL LION's end-request flag, raised on the
                 * transition pass by the class's own verb (ZARIA's is
                 * 0x1e250: the dive state A_WPAD's row list wrapped) -
                 * BEFORE the end_level slot, and takes ebx=3: the
                 * caller's 0x5d3c6-0x5d3d7 leg runs the reset pair
                 * 0x5da60/0x5d050 and the outer loop CONTINUES with
                 * eax=1 (0x5d3dc). This port's copy of that leg is
                 * stage_apply(3) - the same named-ESTIMATE reset PR-199
                 * documented for the injected codes, now reached by a
                 * real, authored producer. No shipped leg before this
                 * one can reach it: A_WPAD has no raiser in the port
                 * (AC_DRECT, OI-018), so every existing pin is
                 * byte-identical until LEO_PLAY_STATE injects the dive. */
                if (pl.p.end_req) {
                    if (!stage_apply(3, tk - 1, objs, nobj, &pl))
                        break;
                    printf("ll_level: stage driver: the level lion's own\n"
                           "ll_level: stage driver: end-request flag "
                           "(0x1e250 -> [obj+0x116]=1) was read at the "
                           "top of the\nll_level: stage driver: "
                           "iteration: loop code 3, the stage re-enters\n");
                    continue;
                }
                /* PR-199: THE REAL PRODUCER, tested where the engine
                 * tests it - at the TOP of the iteration, before any of
                 * its work (0x4ac55, after the 0x4ac3b loop head). A row
                 * that admitted `end_level <n>` in the PREVIOUS iteration
                 * ends the run here and the level function returns 1
                 * (0x4ac5b + the `jmp 0x4ae53` epilogue), the same for
                 * every value. The slot is NOT cleared here: PR-198
                 * measured that the loop body has no store to it and
                 * that only the next level object's ctor zeroes it.
                 * The injected codes below stay where they are (checked
                 * after the tick, which is what makes PR-194's legs byte
                 * comparable); a level whose own data carries no
                 * end_level row - every level in the fixture except
                 * three, none of them runnable here - never reaches
                 * this line, so every existing pin is byte-identical. */
                if (pl.p.exit_word) {
                    stage_apply(1, tk - 1, objs, nobj, &pl);
                    printf("ll_level: stage driver: exit word %d from an"
                           " authored end_level row: the level loop returns"
                           " 1 (0x4ac5b -> the single epilogue 0x4ae53) at"
                           " tick %d of %d - the value selects nothing"
                           " (PR-198: the copy published at 0x5d2dc has no"
                           " reader in the image); the engine's caller"
                           " re-enters the stage through a NEW level object"
                           " (0x5d1f5 allocates, 0x5d233 runs the ctor"
                           " 0x4a4c0 that zeroes the slot), and the port"
                           " has no such re-entry point, so the level run"
                           " ends here\n", pl.p.exit_word, tk - 1, ticks);
                    break;
                }
                ll_play_keys keys = script_keys(scr, tk);
                play_force_state(&pl, getenv("LEO_PLAY_STATE"), tk);
                play_advance(&pl, &keys, LL_ITER_MS, 1, tk, objs, nobj);
                for (int k = 0; k < nobj; k++)
                    obj_advance(&objs[k], LL_ITER_MS);
                int code = 0;
                for (int k = 0; k < ninj; k++)
                    if (inj[k].tick == tk)
                        code = inj[k].code;
                if (pl.loop_exit)           /* PR-531: the zero counter's */
                    code = 2;               /* own exit, 0x4ae82-0x4ae8b  */
                if (code && !stage_apply(code, tk, objs, nobj, &pl)) {
                    printf("ll_level: stage driver: the play leg ended at"
                           " tick %d of %d\n", tk, ticks);
                    break;
                }
            }
            camy = follow_camy(&pl, ch, cam_ymax);
        } else {
            int shown = 0;
            for (int k = 0; k < nobj; k++) { /* advance fixed-step then redraw */
                for (int m = 0; m < ticks; m++)
                    obj_advance(&objs[k], LL_ITER_MS);
                /* PR-93: static images are advanced (inert: no script) but
                 * never take one of the 12 proof slots: a static has no
                 * state or frame to prove; the proof belongs to the
                 * animated cycle, exactly as before PR-93. */
                if (shown < 12 && !objs[k].static_img) {  /* proof of the cycle, bounded */
                    printf("ll_level: tick %d: %s/%s state %d frame %d at"
                           " (%d,%d)\n", ticks, objs[k].sc->leaf,
                           objs[k].sc->movname, objs[k].sti, objs[k].fi,
                           objs[k].x, objs[k].y);
                    shown++;
                }
            }
            if (shown < nobj)
                printf("ll_level: tick %d: %d more placement(s)"
                       " advanced\n", ticks, nobj - shown);
        }
        paint_base(canvas, base, img.idx ? (int)img.w : cw,
                   img.idx ? (int)img.h : ch, cw, ch, img.idx != NULL, &par,
                   bg_tix, cam, camy, world_h);               /* same order as the live loop */
        layers_at(lay, lay_tix, nlay, &par, cam, camy, cw, ch, world_w,
                  world_h, canvas);
        objs_draw(objs, nobj, canvas, cw, ch, cam, camy, &par, lay, lay_tix,
                  nlay, world_h);
        play_draw(&pl, canvas, cw, ch, cam, camy);
        if (pl.have)
            hud_draw(&hud, canvas, cw, ch, pl.energy, pl.lives);
        scene_ppm(ppm, cw, ch, canvas, scenepal);
        goto done;                          /* canvas freed at done: (E-05) */
    }

    {
        /* T-01/T-12: the platform owns the window (logical viewport = the
         * scene view size cw x ch; resizable, HiDPI-ready, F11/Alt+Enter
         * desktop-fullscreen handled by the backend). */
        if (plat_init("Lew Leon - level scene", cw * 2, ch * 2) < 0) {
            fprintf(stderr, "ll_level: no window: %s\n", plat_last_error());
            goto done;
        }
        plat_raise(); /* FIX-02: now a documented no-op (window raised
                       * once at creation); call kept for API parity */
        uint32_t lut[256];
        for (int i = 0; i < 256; i++)
            lut[i] = 0xFF000000u | ((uint32_t)scenepal[3 * i] << 16) |
                     ((uint32_t)scenepal[3 * i + 1] << 8) | scenepal[3 * i + 2];

        /* 5b) help overlay first, like the original POMOC flow: the HD
         *     level block's POMOC1.MNU names page art (.SEQ) that ships in
         *     the CD level block; ESC closes, Left/Right page through. */
        if (have_hd_ety && hblk2) {
            llrs_slot *hrecs2 = NULL;
            unsigned nhrec2 = 0;
            if (llrs_block(&hd_ety, hblk2, &hrecs2, &nhrec2) == 0 && nhrec2)
                help_pages(&hd_ety, hrecs2, nhrec2,
                           &etapy, recs, nrec, base, canvas, lut,
                           cw, ch);
            llrs_block_free(hrecs2);
        }

        /* 5c) level music: the .des `muzyka=<stem>.mod` names a SOUND
         *     container '<STEM>.MOD' payload (standard MOD layout, see
         *     ll_mod.h).  Same singleton as the MAPKA screen; the scene
         *     loop below pumps it (platform queue path, see the note at
         *     the top of this file - callback delivery is unreliable on
         *     SDL2+PipeWire, so the backend opens devices queue-only).
         *     Engine comparison (PR-104): the grammar names ONLY this
         *     per-level tune.  Each level additionally carries a
         *     hard-coded death theme - LOOSER.MOD (ZARIA), ENA2ED,
         *     KADMDEAD, K2DEAD, SADESDEA, RAN1, KARADE, lonade, with
         *     DEATD.MOD as the default - played through the same
         *     music object (0xcf068) when the state machine reaches
         *     row 0 of the current state.  Those themes appear in no
         *     authored file, so this port cannot model them from
         *     data alone (NEXT_STEPS PR-104). PR-105 corrected WHERE
         *     that hook lives: not a lion object method but the
         *     +0x08 KEY-BUILDER slot of each level's RECORD-class
         *     sub-table - the method the record-step driver 0x523f0
         *     calls before the row step - so the themes belong to
         *     state records, not to objects (0x97c9c+8 = the DEATD
         *     default, 0x9e254+8 = KARADE, ...; NEXT_STEPS PR-105). */
        if (audio && !getenv("LEO_NO_MUSIC")) {
            const char *muz = des_get(kvs, nk, "muzyka");
            if (muz && *muz) {
                char stem[16], mname[24];
                stem_of(muz, stem, sizeof stem);
                snprintf(mname, sizeof mname, "%s.MOD", stem);
                ll_music_play(dir, mname);
            }
        }

        /* secs <= 0 = no time limit: the scene runs until ESC/q (desktop
         * behaviour; CI pins the window with LEO_LEVEL_SECS>0). All
         * timing on the single platform clock (ns). */
        uint64_t t_end = secs > 0 ? plat_now_ns() + (uint64_t)(secs * 1e9)
                                  : 0;
        uint64_t t_prev_ms = plat_now_ns() / 1000000ull; /* FIX-07 (X-08): ms clock: */
        uint64_t t_tick = plat_now_ns();   /* dt from absolute time, no loss */
        int scroll_acc = 0;                /* per-frame; sub-px scroll acc */
        while (!esc && (!t_end || plat_now_ns() < t_end)) {
            /* Focus: raised once at window creation (FIX-02); no
             * per-tick poll, no raise on pointer-enter. P-10: F11/
             * Alt+Enter are consumed by the backend and never
             * surface here. */
            ll_event e;
            while (plat_poll(&e)) {
                if (e.type == LL_EV_QUIT ||
                    (e.type == LL_EV_KEYDOWN && !e.repeat &&
                     (e.key == LLKEY_ESC || e.key == 'q')))
                    /* FIX-03 (X-04): exit keys act on first press */
                    esc = 1;
                else if (pl.have &&
                         (e.type == LL_EV_KEYDOWN ||
                          e.type == LL_EV_KEYUP) &&
                         (e.key == LLKEY_LEFT || e.key == LLKEY_RIGHT ||
                          e.key == LLKEY_UP   || e.key == LLKEY_DOWN ||
                          e.key == ' '        || e.key == LLKEY_LCTRL ||
                          e.key == LLKEY_LALT ||
                          (e.key >= LLKEY_F1 && e.key <= LLKEY_F4))) {
                    /* M4-B: in play mode the arrows drive the LION
                     * (held-key tracking, KEYUP releases); the camera
                     * follows the player instead (below the loop).
                     * PR-87 (viewer choice, names from the walkthrough):
                     * Ctrl = fire (attack), F1..F4 = the weapon-selector
                     * keys the comma groups gate with, Alt = K_ALT. */
                    int on = e.type == LL_EV_KEYDOWN;
                    if (e.key == LLKEY_LEFT)
                        pl.keys.k_bkwd = on;
                    else if (e.key == LLKEY_RIGHT)
                        pl.keys.k_fwd = on;
                    else if (e.key == LLKEY_UP)
                        pl.keys.k_up = on;
                    else if (e.key == ' ')            /* viewer choice: Space */
                        pl.keys.k_jump = on;
                    else if (e.key == LLKEY_LCTRL)    /* PR-87: attack */
                        pl.keys.k_fire = on;
                    else if (e.key == LLKEY_LALT)
                        pl.keys.k_alt = on;
                    else if (e.key >= LLKEY_F1 && e.key <= LLKEY_F4) {
                        int *kf = e.key == LLKEY_F1 ? &pl.keys.k_f1 :
                                  e.key == LLKEY_F2 ? &pl.keys.k_f2 :
                                  e.key == LLKEY_F3 ? &pl.keys.k_f3 :
                                                      &pl.keys.k_f4;
                        *kf = on;
                    }
                    else
                        pl.keys.k_down = on;
                }
                else if (e.type == LL_EV_KEYDOWN &&
                         (cam_max > 0 || cam_ymax > 0)) {
                    int k = e.key;      /* ASCII verbatim or LLKEY_* */
                    if (k == LLKEY_UP)
                        camy = camy > 20 ? camy - 20 : 0;
                    else if (k == LLKEY_DOWN)
                        camy = camy + 20 <= cam_ymax ? camy + 20 : cam_ymax;
                    else if (k == LLKEY_LEFT)
                        cam = cam > 20 ? cam - 20 : 0;
                    else if (k == LLKEY_RIGHT)
                        cam = cam + 20 <= cam_max ? cam + 20 : cam_max;
                    else if (k == LLKEY_HOME)
                        cam = 0;
                    else if (k == LLKEY_END)
                        cam = cam_max;
                    else if (k == 'p' && !e.repeat) {
                        /* FIX-03: camera motion (LEFT/RIGHT) keeps
                         * auto-repeat - holding an arrow scrolls. p is a
                         * TOGGLE: first press only, holding it must not
                         * flicker autoscroll on/off. */
                        autoscroll = !autoscroll;
                        printf("ll_level: autoscroll %s\n",
                               autoscroll ? "on (50 px/s)" : "off");
                    }
                }
            }
            uint64_t now_ms = plat_now_ns() / 1000000ull;
            unsigned dt = (unsigned)(now_ms - t_prev_ms);
            t_prev_ms = now_ms;
            if (dt > 200)
                dt = 200;
            if (autoscroll) {
                scroll_acc += 50 * (int)dt;  /* FIX-07: 50 px/s exact over */
                cam += scroll_acc / 1000;    /* time (old 50*dt/1000 lost  */
                scroll_acc %= 1000;          /* <1 px to truncation per tick) */
                if (cam > cam_max)
                    cam = 0;
            }
            for (int k = 0; k < nobj; k++)
                obj_advance(&objs[k], dt);
            if (pl.have) {
                unsigned used = 0;
                while (used < dt) {          /* the same fixed grid the
                                               CI legs run on: one engine
                                               iteration per substep */
                    unsigned st2 = dt - used > LL_ITER_MS ? LL_ITER_MS
                                                         : dt - used;
                    play_advance(&pl, &pl.keys, st2, 0, 0, objs, nobj);
                    used += st2;
                }
                cam = follow_camx(&pl, cw, cam_max);  /* camera follows */
                camy = follow_camy(&pl, ch, cam_ymax);
            }
            paint_base(canvas, base, img.idx ? (int)img.w : cw,
                       img.idx ? (int)img.h : ch, cw, ch, img.idx != NULL,
                       &par, bg_tix, cam, camy, world_h);
            layers_at(lay, lay_tix, nlay, &par, cam, camy, cw, ch, world_w,
                  world_h, canvas);
            objs_draw(objs, nobj, canvas, cw, ch, cam, camy, &par, lay, lay_tix,
                      nlay, world_h);
            play_draw(&pl, canvas, cw, ch, cam, camy);
            if (pl.have)
                hud_draw(&hud, canvas, cw, ch, pl.energy, pl.lives);
            paint2x(canvas, lut, cw, ch);
            ll_music_pump();
            t_tick += (uint64_t)LL_ITER_MS * 1000000ull;  /* FIX-07: grid */
            {
                uint64_t n = plat_now_ns();
                if (n > t_tick + 100000000ull)   /* >100 ms behind: resync, */
                    t_tick = n;                  /* never fast-forward */
            }
            plat_sleep_until(t_tick);
        }
        ll_music_stop();
        /* no window teardown: P-03 keeps it up for the next screen
         * (menu, next level, ...); the platform reuses it on the next
         * plat_init. */
    }
    if (esc)
        printf("ll_level: ESC -> advance\n");

done:                                 /* single cleanup path (E-05): every
                                       * way out of the scene runs this */
    free(canvas);
    scene_owner_free(&scene);            /* scripts (their art + parsed
                                          * .MOV) and placements: the only
                                          * owners of scene memory (PR-45) */
    if (an_open)
        llrs_close(&an);
    if (have_hd)
        llrs_close(&hd_an);
    for (int k = 0; k < pl.nart; k++) {
        ll_seq_free(&pl.art[k].seq);
        free(pl.art[k].blob);
    }
    hud_free(&hud);
    ll_plt_free(&pl.plt);
    free(pl.mov);
    ll_pcx_free(&img);
    free(view);
    for (int q = 0; q < nlay; q++)
        ll_pcm_release(&lay[q]);
    llrs_block_free(recs);
    free(txt);
    if (have_hd_ety)
        llrs_close(&hd_ety);
    llrs_close(&leveldes);
    llrs_close(&etapy);
    return rc;
}

# docs/OPEN_ITEMS.md - the open-items register

One row per open thread that matters across PRs: stable id, status,
origin, evidence pointers, and what would close it. `make verify-docs`
P6 pins the mechanics: every `OI-nnn` cited in the live docs must
resolve HERE, ids are appended-only (never renumbered, never deleted -
closing flips the status line), and every row carries a status.

STATUS VOCABULARY: `OPEN` (still a question), `PARTIAL` (a named
remainder of an answered whole - the row lists what remains), `CLOSED
BY PR-nnn` (kept so citations keep resolving), `FROZEN (owner)`
(owner ruled: do not touch), `DEFERRED (owner)` (a decision with a
number on it, waiting for the owner), `DEFERRED (ruled)` (parked by a
standing ruling, e.g. the Windows end pass).

WHAT IS NOT A ROW: per-PR counted items that live and die inside one
record - that record IS their register (records since PR-196 carry an
OPENS: line; older records list counted items in body). A row here
means a thread that spans or survived several PRs, or a decision
someone must eventually make. Task queues (HANDOFF section 8, the
plan) are plans, not questions. Method notes and measurement traps
(the one-byte listing trap, edited-clip cut points) stay with the
method notes where they are already written.

SEEDING (PR-201 pass, measured): HANDOFF section 7 items 0-15 -
that ledger is now FROZEN verbatim at
docs/archive/HANDOFF_SECTION7_OPEN_QUESTIONS.md (PR-202), and the
"HANDOFF 7 item N" origins below resolve there -, the journal's
`## Deferred big items` list, the OPENS/CLOSED fields and carried
lists of records PR-185..PR-200, the ZARIA.01 audit gaps
G-1..G-13, and the owner rulings of 2026-11-19. PR-195 estimated
"about 40 rows"; the measured seed is larger because the earlier
estimate never enumerated section 7's sub-threads.
ORIGIN ids that are not OI ids resolve at stable archive paths:
P/E/M- and T- at docs/archive/open-leo_native_port_review.md,
T- also at docs/archive/open-leo_porting_tips.md, X/W/L/B- at
docs/archive/PORT_DEEP_DIVE_2026-11.md, "HANDOFF 7 item N" /
"Q0" / "G-nn" at the frozen ledger named above (the
history-override table maps every superseded sha).

CITING RULE (for every writer): when a record or the plan restates a
thread that has a row, cite the id instead of re-describing it. A new
thread gets a row in the SAME commit, id = next number, and the
record's OPENS: line says so.

=== engine RE: motion, frame law, row gate ========================

OI-001: instance facing XOR for states WITHOUT a CH_DIR state header
  status: OPEN
  origin: HANDOFF 7 item 0.4 (PR-79/81/82 era)
  notes: both readings agree everywhere testable; the named settling
    tests (RAN crown/balloon dual-polarity, a left-facing STANDING
    lion, a worm standing after a turn) are all capture work.

OI-002: object state-header `T=` semantics against the object
  velocity law that was read later
  status: OPEN
  notes: the law itself is read (PR-99/100/129/130/133/135/168/178);
    the header-word interpretation PR-83 flagged "must be revisited
    in T1" was never re-pinned against it. THE VIDEO TEST IS RUN
    IN PR-206 and the register model is REFUTED on it: LONA.05's
    YETI (BY_X, A_RUN only F nonzero `F=100,0` `T=50,5`) walks on
    film through camera-locked windows; the shipped arithmetic
    predicts 19.8-20.0 px/s and every measured burst exceeds it
    (row 2 of OI-107). So the video does NOT settle T semantics
    through the register law - the contradiction is its own row,
    OI-107, whose first lead is the un-read second value of each
    pair, which PR-98's map leaves at rec+0x54/+0x5c.

OI-003: the F/FC frame-directive output pair: what it does to world
  motion (keep it distinct from the state-header force word)
  status: OPEN
  origin: HANDOFF 7 item 1; PR-98/99/100
  notes: do NOT port a velocity/impulse reading yet; PR-121/122 read
    parts of the machinery ([obj+0xea] one-shot amount, the
    two-point gap model) - modeling asks are OI-009.

OI-004: the row-admit gate: which half of the BY_X / `F=0,0` gate
  the engine keys on, and the row-cursor consumption semantics
  status: PARTIAL
  origin: PR-110/114 (HANDOFF 13.77); PR-130..164 cursor law
  notes: the row cursor and its two writers are read (PR-130/132/
    159/162); the mode-only reading has a concrete counter-example
    (5 no-exit states) and the final consumption semantics of a
    completed cursor stay counted.

OI-005: one row per record step vs the port's loop: the engine
  keeps row-debt, the port drops it
  status: OPEN
  origin: PR-115 (HANDOFF 13.78)
  notes: needs a capture referee (T5/T10), not a disassembly.

OI-006: strict-past row firing: `acc >= thresh` vs the engine's
  strict-greater
  status: DEFERRED (owner)
  origin: PR-116 (HANDOFF 13.79)
  notes: reading closed (the engine fires strictly past); flipping
    the two gates moves 15066-15317 authored rows by one whole feed.
    The decision is the owner's, with the number on it.

OI-007: the port's double accumulator drifts off its own exact-
  landing law (LONA.05_LEW, |F|=30, dt=20)
  status: OPEN
  origin: PR-118 (HANDOFF 13.81)
  notes: no selftest in either family reaches that case.

OI-008: which accumulator the engine keeps for a WALKING state
  status: OPEN
  origin: PR-119 (HANDOFF 13.82)

OI-009: engine fidelity asks, measured and awaiting an owner call:
  reproduce the truncated move-by-dx step (sub-pixel dropped at the
  divide 0x5435f), and the one-shot vertical amount as a gap to a
  two-point segment (0x45380, slot +0x6c provider, no zero-denom
  guard)?
  status: DEFERRED (owner)
  origin: PR-121/PR-122 (HANDOFF 13.84/13.85); PR-120's quantisation
    test measured the drop before the ask

OI-010: the `.MOV` parser clamps `T=` to 130 while 172 authored
  components sit above it
  status: OPEN
  notes: any future drag/velocity change must apply the cap (the cap
    itself was never re-examined); tools/re/mov_channels.py counts.

OI-011: shipped discrepancy: the port drives the player walk to
  T=20 (= 20 px/s) while the measured walk is ~25 px/s; the worm's
  4-unit poses reproduce at 25 (PR-135) but the lion leg disagrees
  status: OPEN
  origin: PR-79/99/131/135 (HANDOFF 13.88); G-1 referee windows
    (PR-131/134: LEWPL has no A_RUN state - the A_WALK->A_RUN entry
    is an .MOV row question, answer it from data not video)

OI-012: does the PLAYER object go through the 0x54250 step at all?
  PR-135 applies the law to both actors as the model; the three
  call sites do not check class
  status: PARTIAL

OI-013: the [obj+0x95] flag that turns `I`/`IC` rows on; the
  [vt+0xbc] test and byte +0xb6 bit 2 behind the drag floor 20;
  the caller of the row-list dispatcher 0x45df4; the [entity+0x95]
  latch consumer
  status: OPEN
  origin: PR-98/99 (HANDOFF 13.90); the [obj+0x95] reader was
    re-counted by PR-142/161/164 - still no reader found

OI-014: ds:0xcef40 +0x388 as a frame-advance distance accumulator
  status: CLOSED BY PR-100
  notes: a constant 100, re-confirmed by PR-168/178 (the rate is a
    literal 100/tick). Row kept for citation stability.

OI-015: the PR-135 non-modelled list: vertical constants not
  re-fitted (jump apex 20 px vs ~24 px - a known regression,
  overtaken by PR-253, see the note at the end of this row), the
  engine's conditional raise of T to >=20, partial `s` between 0 and
  +/-10, the object's own velocity register, velocity across state
  change, state-change feed consumption, the `nfr == 0` guard,
  LL_PLAY_V_EPS as a model choice, the `vy >= 0` ground-snap guard
  no leg distinguishes; and the 50 ms iteration being PR-134's
  video-chosen edge of PR-109's 28-33 ms code bracket, not a read
  of the engine's loop
  status: OPEN
  origin: PR-135 (HANDOFF 13.8)
  notes: PR-253 OVERTAKES the first item and moves the question.
    The vertical unit is no longer a fit: LL_PLAY_JUMP_DIV=4 and
    LL_PLAY_JUMP_G=1650 are gone, replaced by the measured
    conversions of the authored words (a row pull pair is raw/5
    px/s, an authored G= word raw*4 px/s^2, the pull verb being
    0x54170 with the one-shot/ungated split of PR-250/PR-251).
    ZARIA.01's own A_JUMP then reaches 108 px, which is the
    closed form of its own words (PR-253's
    tools/jump_law_measure.py pins that arithmetic, not a
    number). The ~24 u of PR-73/74 was fitted to ONE jump on
    another level and has never been re-derived from that
    level's authored state; what is open now is not a missing
    scale factor - PR-252 read the Y legs of 0x54250/0x54340 as
    UNSCALED - but whether the two footage readings ever
    described the same state. The row's other items are
    unchanged: the partial-`s` question, the object's own
    velocity register, velocity across a state change,
    state-change feed consumption, the `nfr == 0` guard,
    LL_PLAY_V_EPS, the `vy >= 0` ground-snap guard, and the
    50 ms clock. The T>=20 raise of the list is CLOSED by
    PR-252 (horizontal only, class-verb gated).
    PR-256 REPLACES THE UNIT READING ABOVE. [obj+0xc2]/
    [obj+0xc6] are integers in hundredths of a px per
    iteration: 0x54250 steps drag = idiv(T*v,100) from the OLD
    v, adds idiv(F*s,20) and the record's G word, subtracts
    the drag, and zeroes ONLY on a sign cross (0x5430a-
    0x54337). 0x54358-0x54372 write [obj+0xca]/[obj+0xce] =
    idiv(v,100) and 0x54827-0x54829 add those WHOLE numbers -
    the idiv remainder is DROPPED, and [obj+0xe2/0xe6/0xea] is
    not a sub-pixel carry. PR-281 CORRECTS that row's NAME for
    the triple: it is the surface snap link + gap (PR-280/
    PR-281), applied once at 0x54829 and zeroed in the same
    step - not a SEQ step-motion verb.
    Consequences measured on real data and NOT fitted: the
    A_JUMP apex is exactly 100 px; ZARIA's walk settles at
    register 250 = 2 px an iteration (3 iterations per 6-unit
    pose, PR-134's video count is 2.4 - 20% off); the worm
    pair settles at 125 = 1 px (4 per pose against 3.2
    measured, 25% off); a released key leaves a 4 px slide and
    a residue that never zeroes; an FC pair pulls the register
    in its own unit (the punch's FC 100 is a 1 px step, not
    14 px). NEW open questions from this read, named and not
    fitted: (a) either the engine carries the idiv remainder
    somewhere this read did not find, or the BY_X feed is not
    the post-truncation step - the gaits' 20-25% difference is
    that question; (b) the port's ground query looks only at
    this iteration's position, so a fall at 17-20 px an
    iteration (A_DRAP's own G=0,250 with T_y=15) steps THROUGH
    the authored line - the engine's ground answer is the
    shape-box test FUN_00042a70 / the push-out 0x42de0; (c)
    LL_PLAY_GRAVITY = 200 has no author and no video check -
    it is now only the fallback for a state that authors no G
    word (RAN.01's A_WALK_FWD G=0,0), while ZARIA's states
    author theirs (A_WALK G=30,350, A_DRAP G=0,250, A_JUMP
    G=0,55). PR-285 NARROWS IT AGAIN on the corpus: the
    fallback never runs where the state authors T_y >= 100,
    because that word consumes the register whole and IS the
    author's hold (39 player blocks - 26 at exactly 100, 13
    at 150/1000 - the A_DEAD/A_SCIAG/A_PODCIAG/A_WPAD/
    A_JUMP_DEAD/A_FINISH_LEVEL family); the census
    of 2318 state blocks finds NO default gravity anywhere -
    683 author a non-zero G_y, 697 author zero, 938 author no
    G line, and every state that falls authors its word. (d)
    the engine's y pass looks unconditional where
    this port gates it on !grounded, and G_x stays unmodelled.
    The "raw/5 px/s and raw*4 px/s^2" conversion note above is
    WITHDRAWN: the row pull words and the G words are register
    adds in that one unit; only the DISPLAYED u/s divides by
    LL_PLAY_RAW_VDIV.
    PR-321 MODELS the authored direction-word writes and their
    two x-side consumers: `ldir`/`rdir` (0x459f0/0x45a10) and
    the state-start 1 (0x50442) now scale the row-pull target
    and the UNGATED FC pair (0x54340) through
    ll_play_dir_word. What stays named in this row: whether
    the engine's key/CH_DIR path also writes [obj+0x6d] -
    until that is read the port's walkdir multiply stays the
    stand-in when no authored word is in force - and the G_x
    term 0x542c1, still not added to `add`.

OI-016: the BY_TY vertical feed law
  status: CLOSED BY PR-147/PR-148/PR-168/PR-178
  notes: the same constant 100 behind a `cmp [obj+0xbe],0` gate;
    shipped as LL_CTRL_BY_TY/vert_intent; no port gap. The climb
    BEHAVIOR remainder is OI-078. [PR-410 CONFIRMS THE READING FROM THE
    OTHER SIDE: BY_TY's keyword mask is 0x04, the mode field is bits
    0..2, and the dispatcher arm for 4 is 0x5441b, which is exactly the
    `cmp [obj+0xbe],0` guard before adding the constant; 12 shipped
    states use it and none uses BY_TX.]

=== engine RE: signals, conditions, event system ===================

OI-017: is the live signal set EVER CLEARED (pulse vs latching)?
  status: CLOSED BY PR-151
  notes: 0x749a0 clears it; the port's evaluate-then-clear ordering
    confirmed. Row kept for citation stability.

OI-018: raisers and semantics of AC_ONAIR, AC_DUP, AC_DRECT,
  AC_EDGE, AC_DRAB_CATCH
  status: PARTIAL (the three ground-query bits and the condition that
    raises them are fully read, PR-392: the condition is the authored
    separator byte; AC_EDGE and the identity question at OI-121 stay)
  notes: AC_ENEMY_ATC raised and ruled (PR-91/94/139); AC_BLOK/
    AC_BLOK_Y raisers PR-94 + relation providers PR-177; AC_EDGE
    via the ladder answer PR-141/149; AC_DRAB/AC_DRAB_CATCH raise
    PR-141, poster PR-146, and bits 12/22 have NO poster anywhere
    (counted negative, PR-146). PR-329 CORRECTS that for bit 12:
    the WSPRECT answer verb 0x287a0 posts `or BYTE PTR [ans+0x1],
    0x4` at 0x28805 - the byte-register form the census tool
    misses - so bit 12 HAS an engine raiser, only no shipped
    level places a WSPRECT box to run it. Bit 22 stays
    counted-negative. AC_ONAIR's raiser and the semantic
    modeling of all five remain.
    PR-313 NAMES THREE RAISERS FROM THE ENGINE'S PHYSICS STEP
    (PR-312's +0xac verb 0x546a0 and the queries it calls):
    AC_ONAIR (byte 2 mask 0x4) is raised when NO surface is found
    or an attached one is left - so it is the ground query
    FAILING, which is exactly the gate ZARIA.01's exit chain
    (A_DRAP/A_PRE_JUMP -> A_WPAD on AC_ONAIR,AC_DRECT) waits on;
    AC_DUP (byte 0 mask 0x10) on ATTACHING to a surface;
    AC_DUP_CD (byte 1 mask 0x20) when that surface's kind byte is
    not 0; and the blockade query 0x4b430 raises AC_BLOK (byte 0
    mask 0x1) after 0x45620 has pushed the object sideways by the
    penetration into [obj+0xca]. AC_DRECT is NOT raised by any of
    those three queries - still open, as is AC_EDGE's full
    semantics. The kind byte, the mode argument and the per-point
    flag byte of PR-312 are the unmodelled half of the semantics.
    PR-318 NAMES THE AC_DRECT RAISERS AND READS THEIR GATES, and
    the raiser half of this row is CLOSED. A census of every
    immediate-mask set-write in the image
    (tools/re/signal_raise_census.py, 194 candidate sites, names
    read from this port's own LL_SIG_* table) gives AC_DRECT
    exactly TWO: 0x40d99 and 0x286ec. Both are class +0x78 ASK
    VERBS of the rectangle family the placement factory 0x4df10
    installs by name - `DRECT` -> the table at 0xa1638 (installed
    0x4e1aa, again 0x4ec6a), `BICZRECT` -> 0x99f64 through the
    delegated factory 0x29090 (installed 0x295d0) - and the verbs
    0x40d50 / 0x286a0 answer bit 19 only when the asker's own
    geometry overlaps the instance's AUTHORED rectangle at
    [self+0x3c], which the class +0x18 verb parses from the
    placement line with the format at 0x91d00. The siblings raise
    the neighbouring bits by the same shape: AC_RECT (byte 2 mask
    0x2) at 0x40d23 from the RECT verb 0x40cf0, AC_DRECT2 (byte 2
    mask 0x10) at 0x40dff from 0x40dc0. The census also adds a
    THIRD candidate for AC_ONAIR, 0x54800, in the object command
    dispatcher 0x54560 next to the attach/detach of [obj+0xa9] -
    not confirmed as a raiser, the set base is set before the
    window that was read. What stays open here is the semantics,
    not the raisers: the gate's second condition is
    `[asker+0xc6] > 0`, and what that word is is OI-119.
    PR-319 CLOSES THE OTHER HALF OF THE GATE. The asker's +0x68
    verb, which the rectangle test reads, is the POINT PROVIDER:
    for the LION's classes it is 0x23900 - `add eax,0xd6; ret` -
    so AC_RECT/AC_DRECT/AC_DRECT2 compare the lion's PRESENTED
    POINT [obj+0xd6] (PR-305's field, re-derived by the class
    +0xb0 provider) against the instance's authored rectangle.
    The same read established the command bus that writes the
    `[asker+0xc6]` gate input: see OI-119.
    PR-327 COUNTS THE RECTANGLE-CLASS RAISERS, BY CLASS, AND THE
    SEMANTICS ARE NOW THE ONLY HALF LEFT. Eight class tables
    share the one +0x18 record verb 0x40c90, which scans
    `%d,%d;%d,%d` into [obj+0x3c]; their +0x78 ask verbs raise
    the CLASS's bit - AC_RECT from RECT 0x40cf0 and BRECT
    0x28710, AC_DRECT from DRECT 0x40d50 and BICZRECT 0x286a0,
    AC_DRECT2 from DRECT2 0x40dc0 alone, AC_KILOF from KILOF_PL
    0x1e3a0 (raise at 0x1e42b, a register form the census tool
    misses), from KRA1 0x37ea0 and from the non-ask body
    0x75390; AC_BECZKA from KRA1; AC_DRAB also from WSPRECT
    0x287a0. Their gates are not the geometry test: BICZRECT
    needs the class +0x70 verb == -1, KILOF_PL needs it > 0,
    BRECT branches on the asker's state index [asker+0x5c] in
    9..13. What this row still needs is AC_ONAIR/AC_DUP's
    surface-link semantics and AC_EDGE's; the raise sites of the
    rectangle signals are counted. PR-327 also reads the ASKER'S
    side of those gates: the probes and getters are the asker's
    class verbs, and for the ZARIA lion (its own pair is
    0x97a98/0x97a90, NOT the base 0x96874) +0x3c = 0x230a0
    returns 1 (askable), +0x68 = 0x23900 the presented point,
    +0x6c = 0x23a10 the frozen anchor [obj+0x89] and +0x70 =
    0x239f0 = [obj+0x6d], PR-321's facing word - so BICZRECT
    answers only a left-facing lion and KILOF_PL only a
    right-facing one.
    PR-331 READS THE SURFACE-LINK TAIL AND PORTS THE TWO EDGES. The
    physics body 0x54250 ends at 0x54760-0x54803, branching on the
    query result [obj+0xe6] against the held surface [obj+0xa9]. NO
    surface: detach through [sur+0x0]+0x4 if one was held, clear the
    slot, call the class +0xb8 verb, and in BOTH paths 0x54800 raises
    AC_ONAIR - so it is the ground query FAILING, posted EVERY
    iteration it fails, not an edge. A surface: from no attachment
    0x54785 raises AC_DUP and attaches (the surface's +0 verb, then
    the class +0xb4); SWITCHING surfaces raises NOTHING; the kind
    word the query leaves at [obj+0xe2], if not 0, posts AC_DUP_CD
    every such step (0x547d1); and with a kind-2 surface the
    object's own y velocity register [obj+0xc6] - OI-119's pair, read
    here at rest - at or over 0x58c takes the no-surface branch
    (0x54766-0x54779). The three bits are 4, 13 and 18 of the same
    mask the clause resolver 0x47d60 compiles against, which is what
    this port's LL_SIG_* table already says. So AC_ONAIR/AC_DUP's
    raisers are CLOSED as read and ported; what stays open is
    AC_EDGE's semantics, and the surface IDENTITY and kind word a
    .plt polyline cannot carry, named at OI-121. AC_DRECT's gate is
    untouched by this.
    PR-392 READS THE HALF THIS ROW LEFT OPEN AS ONE COMPARISON. The byte
    the ask keeps at [obj+0xe2] is the separator flag byte of PR-391
    (0x4519d writes it at record+0xc: ';' to 1, '#' to 2, anything else
    0), and the "mode" is compared against it TWICE, as the first
    instruction of both verbs: `movzx [record0+0xc] ; cmp reg,ecx ; jg
    return` at 0x4540c-0x45419 (the ask 0x45400) and 0x45255-0x4525e (the
    clamp 0x45250). Mode 0 therefore accepts only '!' lines, mode 2
    accepts everything the parser can write, and the level's ground array
    is walked one `call [element class + 0x1c]` per polyline at
    0x4b67e-0x4b697, with the mode from the state control word (OI-078)
    and the threshold pushed as 0. So the AC_DUP_CD condition "that
    surface's kind byte is not 0" is the authored separator, and no '#' is
    authored anywhere (PR-391: 0 of 1749 separators), which makes the
    flag-2 branch of 0x54766 dead in shipped data. What stays open here
    is only AC_EDGE's semantics and the identity question at OI-121.

OI-019: what the object-side AC_ENEMY consumers do (A_DEAD(1)/
  A_AWAY/A_SHOW class conditions)
  status: PARTIAL
  notes: raise and delivery are read (PR-94/143/146); consumer
    behavior is the named next feature candidate (PR-89 item 9.5).

OI-020: signal-record posting and delivery: the key-bit posting
  path ([vt+0xc](0x13), the 0x34a35 lead) and the queue-drain
  caller of 0x52b52
  status: PARTIAL
  notes: broadcast census, poster 0x287a0, pair-walk entry 0x4bd30,
    node builders all read (PR-143..149); the key-bit path and the
    drain caller were still counted when the ledger stopped (the
    climb chain is OI-078).
    PR-319 MEASURES THE OTHER POSTER, 0x52b10, WHICH IS NOT A
    SIGNAL POST BUT THE OBJECT COMMAND BUS: eax = the object,
    edx = a command opcode, ebx/ecx = two ints, and it builds a
    4-word message ([msg+0x4] the opcode, [msg+0x8]/[msg+0xc] the
    ints) for the object's class +0x10 verb. The base handler of
    that verb is 0x54560 and its opcode map is in OI-119. The
    signal message PR-235 read (type 0x10, payload = the AC_* bit)
    uses THE SAME message layout, and 0x54560 does not handle
    opcode 0x10 - it falls through to 0x53770. The census of the
    poster's call sites is a tool now,
    tools/re/command_post_census.py: 66 sites, opcode histogram in
    the record.

OI-021: the evaluator's remaining counts: [rec+0x10] threshold
  units vs row feeding, K_ANY marker honour, the `!` complement
  representation
  status: OPEN
  origin: PR-142 counted items, carried in PR-143/144 lists

OI-022: who fills the ds:0xcf464 queue and with what records - and
  whether the port ever should have one
  status: OPEN
  origin: PR-193 item (1), re-listed in PR-194 `## Next`

OI-023: event-id remainders: consumers for ids 17/29, the deep-form
  arms for ids 13/15/21/23, the id-9 FEED sender, the byte-form
  raise bit-numbering, targeted-arm bodies
  status: OPEN
  origin: PR-183/184/185 counts
  notes: id 30 dissolved (PR-185), id-25's pair-sync meaning was
    named (PR-184); the rest never re-mined.

OI-024: msg/event plumbing remainders: the [obj+0x102] reader, the
  [obj+0x28]/ds:0xcf2c8 registry, the [rec+4] guards
  status: OPEN
  origin: PR-171/172/182 counts

OI-025: link/group/pool field semantics: the [obj+0x30] array
  (index 1 was never proven to be the lion), [obj+0x112]/
  ds:0xcef8c, the token->family bookkeeping map
  status: OPEN
  origin: PR-146/184 counts

OI-026: energy: [obj+0xa9] semantics (site map complete), the
  [obj+0xad]/[obj+0x75] readers, the THIRD writer [obj+0xad] :=500
  vs init 500 / respawn 100, and the -30 energy signal - all
  video-gated checks, NOT port-changed
  status: OPEN
  origin: PR-173/186 counts

OI-027: lion flag-block field producers: [obj+0x10e] and the
  0xb/0xc passthrough, the [obj+0x120]/[obj+0x124] pair role
  status: OPEN
  origin: PR-185/186/189 counts; PR-190 carried "[0x120]/[0x124]
    role (lead extended)"

OI-028: flag values 4..7 and what triggers the 0x27410 :=3 arm
  status: OPEN
  origin: PR-188 item (7), carried PR-189/190/192

OI-029: input-table remainders: the key-bit 0x10/ee==6 names, the
  rest of the 0x582f0 key->bit map (gameplay keys)
  status: OPEN
  origin: PR-187/191 counts

OI-030: the [obj+0xf6] authoring map (1500/10000 per stage; the
  0x45200 counts)
  status: OPEN
  origin: PR-187 item (7), never re-listed

OI-031: the ds:0xa94ec load-sites' per-consumer semantics
  status: OPEN
  origin: PR-189 item (3), carried PR-190/192
  PR-300 NAMES TWO CONSUMERS AND THE ONLY WRITER. The image
  writes this global once, 0x4cda6-0x4cda9 (`eax =
  [esi+0x40]`; `mov ds:0xa94ec,eax`), under a name compare
  whose string is 'ani_plat' (0x91ded); the other load is
  0x4b897. The two load sites this record read are
  0x28412/0x2844b: there it is loaded into ebx and kept as an
  OBJECT across a call to the object's own class +0x80 verb
  (0x28423/0x2845c `call DWORD PTR [esi+0x80]`, 37 such sites
  exist), together with edx = [obj+0x10a] (then [obj+0x10e]).
  For the class whose +0x80 verb is 0x287a0, that global is
  the object whose +0x3c gate and +0x6c verb the verb calls,
  i.e. the "other object" of the pair answer. What the pool
  actually holds is still not established - only that it comes
  from an +0x40 field of something named 'ani_plat'.
  PR-302 CLOSES THE PROVENANCE. 'ani_plat' (0x91dd1/0x91ded)
  is the level record's own section key - ZARIA.01's record
  carries `ani_plat=none,anipl2.des,none` for its three
  layers - so ds:0xa94ec is that section's pool: the level's
  animation-platform objects. What is still open is which
  entries it holds per level and layer.
  PR-303 MOVES THAT POOL TO ANOTHER SECTION. The key dispatch
  0x4cd1d-0x4cdca compares the parsed key through 0x76d20
  (0 = equal) against 'zwierzaki' (0x91dda), 'animacje'
  (0x91de4) and 'ani_plat' (0x91ded), and on a match stores
  [esi+0x40] into a global: 0x4cd85 = ds:0xcef44 for
  'zwierzaki', 0x4cda6 = ds:0xa94ec for 'animacje', 0x4cdc7 =
  ds:0xa94e8 for 'ani_plat'. The compare that GUARDS a store
  is the one before it, not the one after, which is where
  PR-302 read it. So the pool the pair verbs test is the
  level's `animacje` object list, and the `ani_plat` pool this
  row was re-pointed at is ds:0xa94e8, whose consumers are
  still unread.

OI-032: level-meta remainders: the 0x43fb0 result stored to
  [level+0x398] (7 readers, none individually read), the 0x5df2b
  dynamic event code
  status: OPEN
  origin: PR-188/189 counts, carried unchanged into PR-190

OI-033: the +0x240/+0x2cc computed dispatch: no static entry into
  0x27d60 (negative scans for any +0x240 access)
  status: OPEN
  origin: PR-180 headline count; carried by PR-192's list

OI-034: the [[obj]+0x126] flag and the [mgr+0x394] sensor anchor
  (its producers / 0x1eb9a "stay" per the ledger)
  status: OPEN
  origin: PR-104 (HANDOFF 13.70), PR-177 item (4) remainder

=== engine RE: objects, AI, placement, classes =====================

OI-035: the engine's compiled per-class AI DRIVER: the [obj+0x106]
  MAIN/WALK/ATACK/TURN tables (0xa3054-0xa30a8), ATACK::check
  0x61cd2, the facing getter, the random gate 0x3fee0, the other
  classes' boxes (SNAKE 0x64830, PAJAK 0x626c0, NOSOROG 0x63750,
  PAPUGA 0x53ff0); whether the compiled AI reads DANGER for arming;
  whether armed A_ATAK walks the worm; the mailbox at [obj+0x0..0xb]
  vs the port feeding the object's own sig set; the [obj+0x102]
  countdown gating the raise
  status: PARTIAL
  notes: the port ships the pinned LICHA box (|dx|<40, |dy|<50) as
    a stand-in behind LEO_PLAY_DANGER. PR-90/91/92/94 counts.

OI-036: which factory-template instance is which .des TYPE (class
  vtables -> type numbers)
  status: PARTIAL
  notes: PR-96 traced the structure; many classes named PR-155/156/
    157/176/185/187; the port still ships the 0x34103 dispatcher
    default 30 until the decode completes (PR-89 item 9.3).
    PR-303 MEASURES THE WHOLE MAP, AND IT IS NOT A TABLE. A walk
    of .text for `mov edx,<address of a printable name>` finds
    781 compare branches, 177 of which allocate and install a
    class word at [obj+0x08]; they are grouped per level family
    (0x1f874 ZARIA, 0x24f8e/0x250d0 ENA, 0x29094/0x29631
    KADMAR, 0x2ea2e SADES, 0x3339c RAN, 0x34cfe KARA, 0x383c4
    LONA, 0x3a254 TUNGA/JUNCTION, plus the placement factory
    0x4df15..0x4e934). Each branch names its type, mallocs a
    per-class size, calls a base ctor and stores the class word
    (and a companion table at [obj+0x0c]), so a TYPE IN THE
    LEVEL TEXT is the class selector, one chain per family.
    Named this way: DOMEK 0xa0150, KALUZA 0xa0070, WSPRECT
    0x99ff0/0x99fe8, PALMA 0x96c38, KATAPULT 0x96d1c, LINA
    0xa1dfc, DZWIGNIA 0xa1cb4, ANITAB 0xa1eb4, KORONA 0xa1924,
    BAL 0xa17dc, and TLewZaria 0x96960 - the last named by the
    shipped ZARIA.01 record's own `lew=` first token.

OI-037: the 18 .MOV state blocks that open ATC_ON and never close
  it: what clears it in the engine
  status: PARTIAL
  notes: PR-139 named the applier 0x45c20 + the 30-tick countdown
    [+0xe4]; the per-state authoring question stays (PR-89 9.2).

OI-038: the class apply end pass: the other 22 class apply methods,
  the 6 pure-walk classes, and the second apply site 0x51101 - a
  DIFFERENT object shape whose type is unidentified (the top lead
  inside this row)
  status: OPEN
  origin: PR-198/PR-199 OPENS

OI-039: the engine TRIGGER that spawns attached children (every
  `0,0` placement entry: corner fruits, the ZARIA mat following the
  rhino, the two POCISK/jajo children - G-4's data half)
  status: PARTIAL
  notes: child machinery read (PR-156 F1 sub-lines, PR-167 child
    aggregator 0x553e0); the trigger/instance side and the follow-
    while-parent-walks question remain; the port joins and counts
    those scripts but never instantiates them (PR-93/95).

OI-040: are PICT/.SEQ statics ANIMATED?
  status: OPEN
  origin: PR-93 item 11.1
  notes: the absence of data in the fixtures is the constraint;
    other levels' trees would settle it.

OI-041: class-name remainders: 0x98078/0x9815c/0x981e8 semantics
    mapped but names pending, the 'XxXx' pair strings, parser-class
    table 0xa1310, TLewEna/TLewSades/TLewLona/TLewTunga2 class
    tables, the 15 class names, the CAMEL/0x98278 role - no
    longer "a generic decor/mover": PR-290 reads it as the
    geometry BASE class, whose verbs 0x20280 / 0x23ea0 /
    0x23e00 reappear at the movement-record class table
    0x97ca8+0x34/+0x38/+0x3c and which the base ctor embeds
    at [obj+0xd6..0xde] (OI-118)
  status: OPEN
  origin: PR-176/187/190/191/192 counts

OI-042: the full 0x49580 field map (38 non-+0x60 fields; "a full
  map needs per-caller data flow")
  status: OPEN
  origin: PR-190/191 counts, carried PR-192

OI-043: the [mgr+0x3ce]/[mgr+0x3d6] writers through the 0x4af36
  pointer
  status: OPEN
  origin: PR-191 item (4), carried PR-192

OI-044: the config-parser object class owning the 0x4cb92-0x4cbb6
  pointer bindings (table 0xa1310 mechanics were read PR-192 F4;
  the owning class stays counted)
  status: OPEN
  origin: PR-189 item (5)

OI-045: the carried-list remainder (each item last appears in a
  PR-185..193 CARRIED list; resolve the detail at those records):
  the class-byte/name map, the 0x54458 class-range energy rule, the
  0x2c670 placement, the state-copy helper, the [obj+0xb6] bit-2
  setter, the CFG windows 0x54790-0x547b5, the +0x54 hook bodies,
  0x550c0's ordering target, the 0x55130 tail, the +0x4c/+0x1c
  recount, the 73 dead 0x54910 tables, the +0xa0 census oddities,
  targeted-arm bodies, 0x45380/0x455a0, the pool chain-store hunt
  (third re-point), the ratio-law study (video-gated), the other
  +0xcc drivers, [rec+4]/ds:0xcec70 semantics
  status: OPEN

=== engine RE: level loop, exit, menu, plan =======================

OI-046: the end_level contract: authored node -> apply method
  0x45750 -> the exit word at [mgr+0x4] -> the level loop's return
  -> the reset/continue selection
  status: CLOSED BY PR-198/PR-199
  notes: engine side read end to end (PR-198, correcting PR-194's
    ruled-out claim); the port implements the contract (PR-199).
    Row kept: several records cite this thread.
    PR-316 confirms the contract from the node-class side: the
    end_level node class 0xa1048 differs from its sibling
    directive classes at slots +0x4 and +0x1c, and its +0x4 body
    is 0x45750, which stores [node+0x8] into [world+0x4]. No new
    fact about the contract, and the executor's call site is
    still not read.

OI-047: what the reset pair 0x5da60/0x5d050 does, and what the
  per-stage edx 0/1 (wrappers 0x5d3c8/0x5d436) selects - until
  then the port's reset stays the named ESTIMATE it is
  status: OPEN
  origin: PR-194 `## Next` (3); the combat fields were left out of
    the port's reset precisely because no reading covers them

OI-048: what raises code 0 (the driver's stop): who zeroes
  ds:0xa94f0 / what requests the stop
  status: OPEN
  origin: PR-198 OPENS

OI-049: what feeds the 0x603b0 menu-result leg (the ESC-menu
  result == 1 is named as the exit-4 source; the producer of the
  RESULT is not)
  status: OPEN
  origin: PR-190/PR-193/PR-198

OI-050: the `.mnu` opcode format and its interpreter: 0x5f310 is
  out; 0x603b0's [obj+0x2c]/[obj+0x34] object walk is the
  remaining candidate
  status: OPEN
  origin: PR-193 RETRACTION; the `.mnu` family is NOT shipped with
  the stage data (PR-192 F3/F5, standing measured fact)

OI-051: 0x5eb11's caller, and why it discards the two-dword pair
  after setting the stage index to 9
  status: OPEN
  origin: PR-193 FINDING 1, PR-198 OPENS

OI-052: what consumes the 0x23ea0 two-view cursor (the point
  writer itself is read whole); neighbours 0x23eb0/0x23ed0 noted,
  not followed
  status: OPEN
  origin: PR-198 OPENS

OI-053: the stage RE-ENTER path: the engine's caller loops with a
  NEW level object (0x5d1f5 allocates, 0x5d233 runs the ctor that
  zeroes the exit slot); the port has no re-entry and ends the
  level run at the guard
  status: OPEN
  origin: PR-199 OPENS

OI-054: a SCENE OBJECT's row admit cannot reach the exit-word slot
  (the port's owner is the LION; all shipped rows are LEW.MOV -
  counted, unmodelled design call)
  status: OPEN
  origin: PR-199 OPENS

OI-055: whether `end_level`'s 1/2 in A_TUNGA1/A_TUNGA2 is the
  TUNGA I <-> II exchange of T6 (AC_XCHG_LEVEL)
  status: PARTIAL
  origin: PR-194 `## Next` (5)
  notes: answered out-of-band by PR-198 (the values are behaviorally
    identical; code 1 is NOT the exit-4 leg) - the authored number
    selects nothing, but no record says CLOSED in those words and
    the T6 mechanism itself stays unread.

OI-056: the plan stream: 0x44xxx/0x49exx plan-data class record
  bodies and the level-load 0x4d5xx per-file open order
  status: OPEN
  origin: PR-191 item (5), half-closed by PR-192 F4, carried

OI-057: the boot registry table 0xad8c0-0xad91c entries:
  0x514e0/0x59dc0/0x5c260/0x611e0 unattributed
  status: OPEN
  origin: PR-192 item (5), never re-listed

OI-058: the 0x1c550 key-latch consume is read (PR-191 F1); the
  full 29-feed census's gameplay feeds stay counted
  status: OPEN
  origin: PR-187/191

=== engine RE: input, devices, language ===========================

OI-059: the device sample's third dword: axes vs buttons naming
  status: OPEN
  origin: PR-186 item (8), renamed PR-187 item (8), never resolved

OI-060: the 0xcde70 key-table format, xlat 0x9636c, 0x9a85c
  status: OPEN
  origin: PR-187 item (6) remainders (the 0x1c530 128-entry table
    itself closed by PR-188 F3)

OI-061: why the SECOND handle exists (the consumer of the [obj+0xc]
  alias) - the header-layout question itself closed as a NON-LAW
  status: OPEN
  origin: PR-186 item (9) remainder, PR-187 item (4)

OI-062: the language-selector writer (ds:0xa9aa8) and the runtime
  assembly of the `.TLO` names
  status: OPEN
  origin: PR-191 item (1), re-listed PR-193 item (7)

=== engine RE: data formats =======================================

OI-063: the `.IS` level-DATA binary chunk format (the container IS
  decoded - ported/is_stream.c; the chunk layout is not)
  status: OPEN
  origin: PR-192 item (1); PR-193 re-listed: "STAYS OPEN"

OI-064: the ANIMACJE record format: the per-tag description layout
  inside the member - bounded since PR-193: every flag-2 instance
  is 5200 bytes
  status: OPEN
  origin: PR-192 item (2), carried bounded PR-193

OI-065: the GRP format (WODOSPAD/KIJ/KILOF), the KAMIEN sixth
  field (1308/1218/1115) and the stone corridor 1299..1620 (G-3)
  status: PARTIAL
  origin: PR-93 item 11.4; the placement lines naming them are
    counted as group headers, not placed
  notes: PR-395 CLOSED the ZAR.BLK-fields half. A row's three
    numbers are a wall x, its TOP y and its height, PARSED by the
    loader into rec+0x4/+0x8/+0x10 with its own formats "%d,%d"
    (0x91d0f) and "%d" (0x9183d); ZAR.BLK is the two world-edge
    walls x 0 / x 2240 over the full 700 px plus the four
    stone-corridor walls 1299/341+118, 1462/244+28, 1540/194+27
    and 1620/0+200 - the corridor this row asks about, authored
    as walls, not floors. Shipped as a port model (ll_blkreg_parse,
    ll_block_hit, ll_block_walk, pinned in --play-selftest). The
    GRP/KAMIEN half of this row stays open.

OI-066: the 84-byte STATUS.LEO record: field meaning AND the field
  map vs the port's ll_save codec (no test verdict yet); the map's
  progress arrows and 'start' variant depend on it - needs a PAIRED
  capture (progress made + save read back)
  status: OPEN
  origin: PR-193 item (3) / FINDING 4; HANDOFF 7 item 5; T-17
    territory; G-8's map half

OI-067: the six stream classes' identity: what differs at
  +0x04..+0x18 and which file kind each carries; the +0x00 slot as
  a base-link (0x5f330) and the +0x24.. triple repeat
  status: OPEN
  origin: PR-191 item (2) -> PR-192 item (4) -> PR-193 item (4)

=== camera + ground ===============================================

OI-068: camera x lead: CAM_LEAD_R=100 CONFIRMED by landmark
  anchors; CAM_LEAD_L=220 sits in a weak band and is pinned by TEST
  not evidence
  status: PARTIAL
  origin: G-2; PR-136/137

OI-069: camera catch-up RATE and the eases (6 x / 2 y per
  iteration) are invented models; measured video says p50 5 /
  p95 60 / max 230 u/s against the port's 120
  status: OPEN
  origin: PR-136/137

OI-070: the airborne camera hold is "a model the data doubts"
  status: OPEN
  origin: PR-73/136/137

OI-071: the PCX-backdrop vertical camera against a TALL level
  (needs an SADES shaft capture)
  status: OPEN
  origin: HANDOFF 7 item 7.1 (the engine formula was adopted PR-65/
    84; the no-back_rect clamp source is settled)
  notes: PR-325 tried again on the two ZARIA.01 captures and the
    row stays open. Three more methods failed: registering the
    temporal-median frame of ZARIA.01.mkv against the raw
    PLAN3.PCX over the whole 640x276 field, matching silhouette
    templates built from the level's own static art
    (PICT/PLAN3.SEQ, PICT/PLAN5.SEQ), and registering the lower
    half only. They disagree (anchor rows 260-275, 291, 276,
    212) and none gets below rmse 19, because the authored
    tlo= chain composites PLAN2.PCM and PLAN1.PCM over PLAN3.
    The x-axis does register in ZARIA.01.mkv (camx 0, and the
    HUD box reproduces to one pixel), but ZARIA.01_b's valley
    is flat across camx 0..120. So no screen row in those
    captures can be turned into a world row.

OI-072: the ground SOURCE remainder (G-13): the port loads ground
  from `platformy=` only and never ACTS on the ani_plat= LOCAL
  ground lines (ANIPL2: KATAPULT/PALMA)
  status: OPEN
  origin: PR-136/138/140
  PR-302 SHARPENED THIS INTO A CONCRETE FILE: the section key
  'ani_plat' at 0x91dd1/0x91ded, and ZARIA.01's shipped record
  (LEVELDES +0x19b9c5c6) `ani_plat=none,anipl2.des,none`.
  PR-334 ANCHORED THE ENGINE SIDE and PR-335 PORTED THE PARSE,
  so what is left here is the USE of the records, nothing else.
  PR-342 PORTS THE AUTHORED MOTION of those records: the class
  +0xa4 verb runs at the row admit (0x52341) and moves a
  PALMA-typed object's table (0,+1) for cursors 0..6 and (0,-1)
  from 7, or a KATAPULT-typed one through the five jumps of
  0x1e670, and nothing at all for KAMIEN/KILOF_PL, whose class
  slots hold 0x239c0 and the query 0x45320. Leg L14 measures it
  on ZARIA.01's own texts. Still nothing READS the moved table.
  The parsed law (PR-335, byte-exact): the .des entry's lines
  after the coordinates are the object's OWN surface table -
  0x45120 reads a count and then N passes of one separator and
  one pair into N+1 13-byte records at the object's embedded
  point [obj+0xa5] (class 0x98278) and 0x1e630 is its third
  caller. The separator after record
  i IS record i's flag byte at +0xc: ';' 1 (0x451a6), '#' 2
  (0x451d4), any other character 0 (0x451db); NO separator ends
  the loop, only the count compare at 0x451c9, and the LAST
  record carries no flag byte at all; a zero count allocates
  nothing (0x45141-0x45143) and 0x451e2 then asks for one
  literal ';'. 0x45200 at 0x1e52a adds the placement to every
  record, so the table is stored ABSOLUTE, exactly like the
  ladder's contact points, and the same verb at run time
  (0x1e5c0 -> 0x45200(point, 0, ±1), 0x1e5f6) walks the seat
  one pixel per ADMITTED ROW, by the state record's row cursor
  - PR-341 corrected this row's "a tick", and PR-342 ports the
  shift. What the reader sees in the
  shipped install: 147 such lines in 15 .des files of 9 levels,
  309 records, and one line (ZARIA.01's KATAPULT) whose
  trailing `-6300,-1350` 0x1e630 reads as a PAIR into
  [obj+0xb1]/[obj+0xb5] with 0x4a0b0 (PR-336). Counted apart from it, 338
  lines of this shape sit in the 103 dumped .des members: 154
  after an entry line (this reader takes 147 - the other 7 hang
  off entries authored at 0,0), 153 after a bare count line or
  continuing another table line (KADMAR2 ANI1's `76` and its 76
  `0 x,y;` rows, TUNGA2.12 PLATF's `66`, SADES.06 PLATF's `8`),
  23 after a `bounds=`/`koniec=`/`R=`/`frame=` property line, 8
  after a `TRASA`/`SOUNDSOURCE,` group line. Separators: ';' 467
  / '!' 11 / '#' 0 - flag 2 is a grammar no shipped object uses.
  STILL OPEN HERE: whether a property line cuts the record - those
  23 lines behind `bounds=`/`koniec=`/`R=`/`frame=` include
  ZARIA.02's four JBRANCH and JROPE tables, and this port reads a
  continuation only while such lines run on directly (which verb
  reads those words, and where does it leave the stream?); the six
  lines whose count is one past their pairs (LONA.05 ANI3's `23
  ...`/`20 ...`, TUNGA2.12 ZWIERZ's four GORYL paths) and what
  the engine's failed `%d,%d` read leaves there; the ask chain
  the ask chain
  that would make a record a lion's ground - PR-336 READ THAT
  CHAIN END TO END and corrected its own sketch: 0x45320 adds
  the truncated velocity step [obj+0xca]/[+0xce] to the
  requester's OWN position (0x541f0 copies [obj]/[obj+4]),
  0x45400 keeps the MOST negative clearance at that pair as
  the triple [actor+0xe6]/[+0xe2]/[+0xea], and only after a
  first test at the requester's anchor [obj+0x89] that no
  shipped position passes - and
  the rider law PR-312 read at 0x54250/0x54827 applied to
  those seats; measured through PR-335, ZARIA.01's chain is the
  KATAPULT pad (307,633)-(340,633) and the PALMA spans
  (838,475)-(900,475), (885,380)-(947,380) and
  (930,320)-(992,320), so its top ends 28 px short of the
  authored exit column x 1020 and the crossing needs that
  per-tick seat motion as well as the surface; the meaning of
  the flag byte inside the query (0x45417's first-record mode
  compare) and what the last record's unwritten flag byte
  holds; and whether a moving platform re-translates its table
  horizontally (0x45200 is called with the placement only at
  load, the runtime calls pass (0, ±1)).
  PR-336 NARROWS THIS ROW TO TWO NAMED GAPS - now OI-122 and
  OI-123 - and both are needed before any of the above can be
  ported. (1) THE CLASS
  BINDING: the classes the installer chain gives the two
  shipped seat objects hold NO seat verb - 0x1fb32 names
  KATAPULT (0x90738) with size 0xc5 and [obj+8] = 0x96d1c,
  0x1fc08 names PALMA (0x9074a) with size 0xb5 and [obj+8] =
  0x96c38, and 0x96d1c's +0x18/+0x78/+0xa4 are 0x23ba0 /
  0x20230 / 0x53d80, 0x96c38's 0x1edb0 / 0x52570 / 0x20280 -
  while 0x1e4a0, 0x1e510, 0x1e570, 0x1e5c0, 0x1e630, 0x1e970
  and 0x1ea90 sit in runs at 0x95aa0, 0x95c50, 0x95cb0,
  0x95cdc, 0x95d34, 0x95d94 and 0x95e54 that NO dword and no
  immediate of the image names, unlike the eight rectangle
  classes PR-328 found. (2) THE POSTER: the exemption of that
  first test is [actor+0xa9] being the queried point itself,
  and apart from the copies at 0x5478e/0x547bc and the
  clearings at 0x547f0/0x54e6a/0x1f8e9 it is written at
  exactly one place - 0x54624, the +0x88 verb 0x54560's case
  [arg+4] == 0x1b, reached only through 0x53480's name-miss
  path - and no instruction stores 0x1b into a dword slot.
  WITH A LIVE PRODUCER THE TRIPLE IS ENOUGH: 0x54804-0x5482d
  adds [obj+0xca] to x and [obj+0xce]+[obj+0xea] to y and
  zeroes them, and since the clearance was taken at the
  predicted position, that lands the position point exactly on
  the seat line. MEANWHILE THREE THINGS ARE SETTLED AND THEY
  RULE OUT EVERY SHORT CUT: no seat-owning state is SOLID
  (none of the 290 states of the 22 seat-owning tokens names
  it in its `flags=` line), so PR-92's push-out cannot stand
  the lion on a seat; 0x1e570 answers only while its state's
  ROW CURSOR [[rec]+4] is under 13 and 0x1e5c0 moves the
  table (0,+1) below cursor 7 and (0,-1) from 7 on, so the
  shipped state's row count IS the motion shape (ZARIA.01
  KATAPULT 10 rows, PALMA PALM/PALML 14 each, TUNGA.11's
  A_MOVE_DN/A_MOVE_UP pairs 5 each - which never reach 7, so
  the authored direction of a short state is still unread);
  and every one of the 153 grammar-matching shipped tables is
  x-ascending, so 0x45380's endpoint gate never rejects.
  PR-337 REDRAWS THIS ROW'S REMAINDER. The engine's surface
  machinery has TWO verb families: the 0x1exxx seat verbs of
  OI-122's three unnamed tables, which stay unreachable, and the
  installed family 0x3dd60 / 0x3dcb0 / 0x3ddf0 / 0x3df00 over an
  inline header at [obj+0xee] - read at object build, moved by a
  message whose opcode word is 1, anchored through the class
  +0x6c verb, and asked through 0x45320 after the class +0xd8
  predicate answers. 61 shipped factory name branches carry it
  (the PR-337 record lists them), and 98 of the 161 objects the
  shipped .des set gives a surface line belong to those types -
  MOVEPLAT, KRA0, KRA1, KRA2, KWINDA, KGARNEK, LINA, DRABINKA -
  while ZARIA.01's single KATAPULT and three PALMA belong to the
  63 that carry none. So this row's "the USE of the records" is
  no longer one question but two: the live path, whose first leg
  is now nameable as port code, and why ZARIA.01's authored
  crossing is written in a data form its own objects never read -
  named as OI-124.
  PR-338 CORRECTS THAT PARAGRAPH. Its "two families" is wrong in
  one way and its install census is void. The table itself is an
  object of its own class: 0x44fe0 builds it into an inline header
  and names the class 0xa0cc0, whose verbs are +0x8 0x45250,
  +0xc 0x452d0, +0x14 0x453d0, +0x18 0x45320 (the query of
  PR-336, not a free function), +0x1c 0x45400 and +0x20 0x45120
  (the parser of PR-335); 0x45120 has exactly five call sites and
  0x45320 exactly six askers, and they address THREE inline homes
  of that header - [obj+0xa5] (0x1e510, 0x1e630; asked by 0x1e570
  and 0x1e970), [obj+0xee] (0x288d0, 0x3dd60; asked by 0x288e0,
  0x380d0, 0x3df00) and [obj+0x44] (0x3efe0; asked by 0x3f010,
  which first requires the asker's [obj+0x5c] to be 31 or 32) -
  plus a fourth shape, the authored rectangle at [obj+0x3c],
  moved by 0x28880 with the same message opcode-1 protocol. The
  "61 branches" figure came from reading the class block 0x1000
  late (OI-122). Read where the installers point, 65 of the 327
  factory name branches - 59 tokens - resolve one of the fifteen
  verbs: the [obj+0xee] family on most of them (0x3dcb0 on 201 of
  the 298 stored class views, 0x3ddf0 on 192, 0x3df00 on 147,
  0x3dd60 on 109), the [obj+0xa5] seat family on KAMIEN, KATAPULT,
  KILOF_PL and PALMA only, the rectangle read/ask pair 0x288d0 /
  0x288e0 on eleven and nine views, PR-328's rectangle move 0x28880
  on nine, and the [obj+0x44] ask 0x3f010 on ten. Crossed against
  the data, 131 of the 160 objects the shipped .des set gives a
  surface record line are on those types and 29 are not (KATPLAT,
  KRA2, LINA, DRABINKA, ZAPADNIA, GORYL). What survives the
  correction unchanged is the part that never depended on it: the
  header layout, the record stride 0xd, the message protocol, and
  that ZARIA.01's own seat objects carry the [obj+0xa5] family -
  which is what this row asked for.
OI-073: the engine's grounded-y bookkeeping: the instruction that
  turns the shape bottom into the grounded y is unread; the port's
  `pos = line - contact` is an inference
  status: OPEN
  origin: PR-97 item 12.1
  notes: PR-271 measures the first FOCUS-1 symptom: a lion
    falling at ~25-33 px/tick crosses the ZARIA.01 main ground
    (`1 -50,680!408,684;` in ZARIA_A.PLT) at x 171 and x 204
    WITHOUT landing - the port's landing search starts only
    LL_PLAY_STEP above this iteration's contact line - falls
    to the death line and respawns. The engine's terrain-side
    geometry is the shape-box hit test 0x42a70 / push-out
    0x42de0 (PR-97; modelled for OBJECTS by PR-58): read the
    falling-body path there and re-derive the landing law.
    PR-272 MODELS the continuity half of that law: the landing
    search top is the contact line the PREVIOUS iteration ended
    on (ll_player.prev_line), which is what the shape-box
    contact forces - a body cannot cross a line between two
    position steps without meeting it. The measured fast-fall
    tunnel above is CLOSED (the same runs land and stay).
    Still open in this row: the exact instruction that turns
    the shape bottom into the grounded y, the [obj+0xb6] bit
    semantics (row-directive writers 0x45c2d-0x45c45, physics
    tests 0x54200/0x54218/0x5426c, the pair/set/clear family
    0x54f40-0x55057), and whether the engine's contact tests
    a whole box or a point (PR-135 item 11 - only the
    continuity half is modelled).
    PR-273 DECODES the [obj+0xb6] family's directive layer
    (its record carries the instruction map): bit5 IS the
    row-directive `lock`/`unlock` flag (0x45c20 writes it);
    bits0/1 are the ground pair written by `lock2`/`unlock2`
    (0x54f40/0x54f90) and `ground_on`/`ground_off` (the
    target's vtable +0xc0/+0xc4, lion 0x54fe0/0x55020);
    CORRECTED BY PR-321, name table 0x918ef-0x9198e read in
    order against the apply bodies: that list is shifted by
    one directive. 0x45c20 is the `op` node (0 shipped lines)
    - `and dl,0xdf`, `and ebx,0x1`, `shl ebx,0x5`, `or
    [eax+0xb6],ebx` at 0x45c45: bit5 comes from the node's
    flag byte; `lock` is the class 0xa10d8 with apply 0x45c50
    calling 0x54f40, and 0x54f40 ends at 0x54f7b `or BYTE PTR
    [ecx+0xb6],0x1` on BOTH paths (its pre-test only skips
    the +0x48/[rec+0x33]/+0x50 trio) - value 1, not bit5;
    `unlock` 0x54f90 clears that same value; the real
    lock2/unlock2 pair is apply 0x45c80 to 0x54fe0/0x55020,
    values 1|2. PR-321 also MODELS value 1: the port's
    position store stops while it is set (the anchor branch
    0x54ee0-0x54f20, PR-320) and state entry clears it
    (0x54e00's `and ah,0xc0`).
    `chdir`/`ldir` write the direction word [obj+0x6d]. The
    bit0/1 pair gates the 0x5426c T-raise and the
    0x54c00/0x54d0c contact-and-draw pass; bit5's getter
    0x23860 (vtable +0x40) is PR-263's "window bit" in both
    answer gates. What stays open narrows to: the grounded-y
    instruction INSIDE the 0x54c00/0x54d0c pass ([obj+0xb5]
    is passed to [vt+0x50] there - unread), the A_PODCIAG
    lift-motion source (SEQ step-motion vs the contact pass,
    PR-273 OPENS (i)), the [obj+0xa9] list, and the
    bit2/3/6/7 semantics.
    PR-275 READS the layer behind [obj+0xa9]: 0x45120 parses
    a polyline text (`<segments> x,y<sep>x,y...`, coordinates
    read by 0x4a0b0's `%d,%d`, each point's kind byte from
    its prefix char ';'->1 '#'->2 else 0 - PR-276 replayed
    the grammar over the fixture's own lines) into 13-byte
    records in the pool at ds:0xa0c64 and stores the table
    AT [obj+0xa9] (via the obj+0xa5 slot), 0x45200
    localizes it, 0x45380 brackets a point by x, and 0x45320
    attaches a one-shot snap-to-line for the predicted
    position (0x541f0 + [obj+0xca]/[obj+0xce]) through the
    [obj+0xe6]/[obj+0xea] pair PR-121/PR-122 read; the
    0x5478e link store and the 0x45479 compare make
    [obj+0xa9] BOTH the object's own table and the current
    link - the one slot's two roles are PR-275 OPENS (iv).
    The A_PODCIAG lift lead narrows to the snap law (its
    gap is <= 0 = UP only); which provider holds the top
    platform line is PR-275 OPENS (i).
    PR-279 ANSWERS THAT PROVIDER (the .plt lines on the
    level manager, loaded by the wiring parser's platformy=
    branch); PR-280 DECIDES THE LAW (the 0x45400 rule is a
    two-point LINE-CROSSING landing test, so a line above
    both points is never chosen - this channel cannot lift);
    PR-281 DECODES THE APPLICATION (pos += vel + gap with
    the gap consumed same-step, [obj+0xa9] = the linked
    table with attach/detach at 0x45230/0x39d00, and the
    accepted order puts the position point exactly ON the
    line). The lift lead that remains is the contact/draw
    pass 0x54c00/0x54d0c under the lock bit and the
    [reader+0xb1]/[+0xb5] grounded-y pair.
    PR-282 MEASURES THAT PAIR AWAY: 0x54c00 is the DRAW
    path (world->screen blit-variant choice), [obj+0xb5] is
    a BLINK phase byte (the directive words [obj+0x11a]/
    [+0x11e] toggle it), and [vt+0x50] - 0x53e40 for lion
    AND ladder - IGNORES the argument and returns the draw
    flip sign. No grounded-y instruction exists there; the
    OI's remaining grounded-y question rides the ladder
    column of PR-282 (below) and the PR-281 application
    law.
    PR-286 PUTS A NAME ON IT FROM THE PLAY SIDE: the hang
    A_DRAP authors G=0,250, so a rider who reaches the top
    of a ladder is DROPPED by his own state's word unless
    something grounds him there. The writers of the
    [obj+0xb6] bit0/1 pair - ground_on/ground_off, the
    target's vtable +0xc0/+0xc4 (PR-273, lion
    0x54fe0/0x55020) - have no measured caller from an
    ANSWER, and no leg but ly_chang pins the drop as it
    now is.
    PR-321 RE-NAMES THAT WRITER PAIR too: 0x54fe0/0x55020
    (apply 0x45c80) belong to `lock2`/`unlock2`, values 1|2;
    `ground_on`/`ground_off` (names 0x9191b/0x91925) have 0
    shipped lines and no measured node class, so the
    bit0/1-gated paths stay unreachable in shipped data until
    a class is named. The [obj+0xb6] work that IS modelled
    now - value 1, the `lock` freeze of the position store -
    is PR-321's; bit2/3/6/7, `op`'s reader (0x23860's bit5
    law stands), and the grounded-y instruction stay open
    here.

OI-074: elements with no margin lists (the 34 all-empty-margin
  ones): contact 0 or something else - "no evidence either way"
  status: OPEN
  origin: PR-97 item 12.2

OI-075: re-grounding after an `M dx,dy` delta or `P oz,px,py`
  write: untested (the legs using them have states with no `.SEQ`
  art in the fixtures)
  status: OPEN
  origin: PR-97 item 12.3

OI-076: does the engine re-test EVERY object every tick (continuous
  re-grounding; the `G=`/`CHECK_LN` pair suggests it does; only
  obj_settle follows terrain in the port)
  status: OPEN
  origin: PR-97 item 12.4

OI-077: fixture hygiene: the five drawn-anchor test legs use a
  colour mask that catches same-coloured scenery - they are
  regression tripwires, not measurements; window the sprite from
  its own bbox
  status: OPEN
  origin: PR-97 item 12.5

=== gameplay behavior + capture-needed ============================

OI-078: the in-game ZARIA.01 climb, now MOVING to the top at
  1 px per iteration (PR-259 ships 0x542cd's y force) and
  stopped there by the ENGINE's own signal clear (PR-262: the
  transition pass 0x53520 memsets the 12 mask bytes at
  0x5358e, one call after the apply - the per-iteration
  `p->sig = 0` / `o->sig = 0` is the engine's law, not this
  port's stand-in): the lead was the top of a climb,
  `A_PODCIAG(1)=AC_EDGE,K_UP` with AC_EDGE unmodelled - PR-267
  measured that away, PR-268 re-opens it the other way: the
  ladder's answer verb IS alive and its two rectangles are
  AUTHORED text, and this port hands the verb its own element
  box instead (what
  raises K_GEN1 was PR-257: the lion's own rows, and
  `A_WSP(3)=AC_DRAB_CATCH,AC_ONAIR` is CLOSED as a non-lead,
  PR-258), the AC_LINA twins of AC_DRAB (the LINA class raises
  them, the port models only the AC_DRAB pair), node lifetime +
  the 0x54b6a merge guard, object-asked answers and mid-air
  asking
  status: PARTIAL
  notes: poster, pair-walk entry, node builders, feed law, guard,
    evaluator, ladder answer all read and shipped (PR-142..150);
    the named remainder lives in those records' counted items.
    ZARIA.01's lion climbs the whole ladder from PR-259;
    the chain ends at its top.
    PR-216 CLOSED the gate half of this row. The answer branch
    at 0x3f1c0 is gated on [obj+0x5c], the STATE index, and the
    loader says what numbers it: 0x53905 reads the list a script
    declares before its blocks into the vector [obj+0x3c],
    0x53440 resolves a state name to that list's ONE-BASED
    position (0 = not declared, 0x53471), 0x534b6 stores the
    block's record at that slot of [obj+0x50] (allocated
    length+1 at 0x5397a), the record-ctor loop starts at slot 1
    (0x533de) and the object ctor writes [obj+0x5c] = 1
    (0x538ab). Block order is not that order: ZARIA.01's
    DRAB.MOV declares A_UP,A_DOWN,A_DROP but writes the blocks
    A_UP,A_DROP,A_DOWN - so a placed ladder sits at index 1 =
    the ENTER pass's state-index-1 arm (0x3f1e4/0x44ec0 -
    PR-216 named it "the child-ANY branch, which answers
    nothing", and PR-291 corrects both halves: it raises
    signal bit 54 = K_GEN1, and it is the object class's +0xac
    ENTER verb 0x3f1c0, not a transition branch) and the ONLY
    state that answers the
    lion is index 2 = A_DOWN; A_DROP, the unrolling rope, is
    index 3 = inert. PR-150's row-cursor gate is gone, and the
    "binding order / encrypted pool" blocker of PR-157/158 is
    gone with it. (PR-290 narrows the one question left here:
    a row's match set comes from the record's `control=` query
    bits, and DRAB.MOV sets none of them, so no external set
    can reach that row - see this row's PR-290 item.) Census:
    172 of the 777 shipped .MOV texts
    differ between the two orders, 54 sites compare [obj+0x5c]
    with a literal (21 of them `== 1`, the initial state), and
    the pinned ZARIA.01 walk probe is unchanged, as expected:
    the placed ladder answers in neither the old nor the new
    model. What is left is ONE question: what raises K_GEN1 on
    a DRABINKA, whose only authored exit is `A_DROP(1)=K_GEN1`.
    The rest of this row's items are unchanged.
    PR-214 ANSWERED the "per-frame setter of the lion's
    contact target [obj+0xd2]" lead NEGATIVE: nothing writes
    it per frame. Full writer census (ELF sweep): the only
    appends are 0x3d618 (PR-146's asker-append inside the
    +0x78 entry 0x3d440, held at +0x78 in exactly two NON-
    lion tables 0x96a88/0x99e64) and 0x1ef71, inside the
    handler whose address appears exactly once in the image -
    0x97050 = LION base 0x96874 + 0x7dc - so the lion's
    list is written by the lion's own class at pair-touch
    time, gated on the counterpart's [obj+0]==2 and appending
    through 0x53f00 (kind 1). Heads are only zeroed
    (0x5415b/0x54e88) or popped (0x551fc/0x551f0). The
    reader 0x28590 (0x9a390 + 0x134) runs only when both
    sides' [obj+0x5c] == 4. TLewZaria 0x96960 overrides that
    slot with 0x531b0, which appends nothing - see OI-110.
    PR-217 touched no behaviour here. It captured the
    `Wait <signal>` clause ZARIA.01's DRAB.MOV authors
    (`[A_UP]` row 1 is `WAIT K_GEN1`, `[A_DOWN]` row 11
    `WAIT K_NEVER`), which is the other half of the same
    ladder, and named the ordinal the clause would use:
    K_GEN1 is signal bit 54 in the engine's own table, and the
    port's LL_SIG_* ordinals match all 68 numbered names name
    for name. How that clause is COMPILED and gated is
    OI-111; the "what raises K_GEN1" question stays here.

    PR-218 turned that one question into a MEASURED data fact
    with a named shape: `GEN K_GEN1` is authored in 20 of the
    777 texts and EVERY one of them is the lion's own script
    (ZARIA.01 LEW.MOV and LEWPL.MOV 36 rows each, TUNGA2.12
    36, TUNGA.11 32, ZARIA.02 32, KADMAR.07 29, KARA.07 26,
    SADES.06 10, LONA.05 9, RAN.01 8, KADMAR2 8, fewer below);
    ZERO DRABINKA member raises it, and ZARIA.01's DRAB.MOV has
    no `GEN` line at all, yet its `[A_UP]` row 1 is
    `1,0,WAIT K_GEN1`. Under a per-object signal set the bit
    can never be raised there, so the raiser is either engine
    code (the CATCH/DRAB contact of PR-216, which was read as
    setting [self+0x66] bit 6 and NOT a signal - worth
    re-reading with this in hand) or a set shared through the
    key registry the clause parser registers names into
    (0x57d30). Two more leads are closed: the lion's answer
    slot at its class table 0x96948+0xd4 is 0x544d0, an EMPTY
    override (the PR-153 boundary-artifact trap), so the
    `state index == 2` + AC_BREAK_NEXT case at 0x54482 is a
    no-op for the lion; and PR-218's read of the row step shows
    the `Wait K_GEN1` clause does not gate the row either - no
    shipped clause does (OI-111).
    PR-219 adds one FOCUS 1 datum about those very rows: in the
    engine a row whose SECOND authored number is 0 does not get
    a zero time - 0x516af-0x516c2 reads a key string at
    [scene+0x398] and substitutes a value looked up through
    0x44110 in the same name tables the clause parser uses
    (0x961fc/0x96200). ZARIA.01's ladder rows are exactly that
    shape (`[A_UP]` row `1,0,WAIT K_GEN1`, `[A_DOWN]` row
    `11,0,WAIT K_NEVER`), so their row time is a level-config
    value the port never reads, and which authored number is the
    row's time at all is now OI-112.
    PR-220 narrows that `,0` case further: the engine does not
    invent a constant there. The state record's row builder
    0x51eb0 loads ebx from the global 0xceff4 (0x51f48) and
    passes it to the row ctor 0x51f54; 0x516bd's registry lookup
    (through 0x44110) fills the same field when the authored
    value is 0. 0xceff4 is written at 0x51a35 by the `seq=`
    branch 0x51b0d: it registers the sequence name (0x57d30),
    then calls the settings block's +0x4 verb (0x51a32) with the
    name-table global 0xceff8, i.e. it looks the sequence's own
    `time=` default up. So ZARIA.01's `1,0` / `11,0` ladder rows
    carry the sequence's authored default time, a value the port
    never reads, and the row's own time lives at [row+0x8]/
    [row+0xc] with a sign flag - whose consumer PR-220 could not
    find (OI-112).
    PR-221 retracts one thing PR-220 briefly suggested here:
    the `,0` in ZARIA.01's `1,0,WAIT K_GEN1` / `11,0,WAIT
    K_NEVER` is NOT read by the engine as a row time and then
    replaced by the sequence default. The row ctor parses one
    number (0x51660, one destination at [row+0x10]); [row+0x8]
    is the sequence default or a settings-block name lookup at
    EVERY row regardless of the line, and the second authored
    number has no named engine consumer (OI-112).

    PR-224 names the engine's one-bit raiser: the handler
    0x53772 tests a message of type 0x10 with the tag
    0x8138764, takes the payload byte at [msg+0xc] as a SIGNAL
    INDEX and OR-s `1 << (idx % 32)` into
    [obj+0x60 + 4*(idx >> 5)] (0x53790-0x537d2) - the TEventSet
    of PR-88, raised one bit at a time. The engine's own key
    sends match this port's ordinals exactly: 0x2c5cd sends 0x21,
    0x2c611 sends 0x1c, 0x2c651 sends 0x1b = LL_SIG_K_FWD /
    K_UP / K_DOWN. NO sender passes 54 = K_GEN1: the 22 call
    sites of the sender 0x4bc40 use small AC_* indexes (7, 0x12,
    0x13, 0x19, 0x1a, 0x21) or values read from object fields
    ([obj+0xee] at 0x34a7f, [obj+0xf2] at 0x3a1cf, [obj+0x122]
    at 0x1f45c), and the broadcast loop 0x3eac6-0x3eaf4 sends
    0x39 to a list of objects. So the drabinka's K_GEN1 comes
    from one of those field-fed senders, from a different message
    type, or from the non-matching branch of the same handler
    (0x52bb0 at 0x537dd) - the named next read.

    PR-225 closes PR-224's "field-fed sender" candidate and
    closes the sender set itself: the magic signal tag 0x8138764
    occurs at EXACTLY six producer sites in the image (0x2882d,
    0x2c5e1, 0x2c613, 0x2c653, 0x2c99a, 0x3eacf) plus the one
    check at 0x53780. A signal raise is a message with type 0x10
    AND that tag; the payload is the signal index. The raisers:
    0x2c5cd/0x2c611/0x2c651 broadcast K_FWD/K_UP/K_DOWN (33/28/
    27) through 0x4bc40 (its parameter order is scene, TYPE in
    edx, TAG in ebx, selector in ecx, signal index pushed);
    0x2c99f raises index 21 on [scene+0x394]; 0x3a1dc raises 30
    on self; 0x2882d-0x2885a synthesizes the message on the
    stack and calls vtable+0x10 directly (index 10); the
    collision loop 0x3eac6-0x3eaf4 raises index 57 on every
    object of the collided object's neighbour list. NO sender
    passes 53/54/57-as-source/58 = K_JUMP/K_GEN1/K_NEVER, and
    the field-fed sites put a SUBSYSTEM constant in the TYPE
    slot (ds:0xcebfc, 0x967f4) with tag 0, so they never raise.
    Two non-message writers remain, and one is named: a
    collision ORs 4 and the set at [[obj+0x3c]+0x60] into
    [obj+0x60] (0x3edff-0x3ee11) - THE CURRENT STATE OBJECT
    CARRIES A TEventSet at +0x60 - and vtable+0x8c (base
    0x536f0, merged at 0x536ff-0x53725) adds a per-frame set.
    Next read: what the state's +0x60 set holds and how it is
    authored, or whether the shipped `.MOV` name lookup (0x57d30
    vs the block 0x961fc/0x96200) maps the WORD `K_GEN1` to an
    index the engine really raises.

    PR-226 names the raiser mechanism: the ordinary scene-object
    classes implement vtable slot +0x8c as the signal provider
    0x54910 (102 class tables hold it; the frame refresh 0x536f0
    calls it every frame and merges its result into [obj+0x60]).
    Its law, from machine code: [obj+0xa5] bit 0x1 raises 33 =
    K_FWD, bit 0x10 raises 41, BIT 0x20 RAISES 54 = K_GEN1; if
    [obj+0xba]!=0 then [obj+0xb6] bit 0x10 (kept as the SIGN of
    the speed word [obj+0x6d], 0x54bb0-0x54bd4) chooses 36 =
    K_FWD vs 35 = K_BKWD; [obj+0xbe]<0 raises 29, >0 raises 30.
    So the drabinka's WAIT K_GEN1 needs no message and no GEN
    row - the provider re-evaluates it continuously, which is
    why no sender passes 54. The ONE question left: who SETS
    bit 0x20 of [obj+0xa5]? The ctor zeroes it (0x2b6a0), the
    clone copies it (0x2b840), other code reads [obj+0xa5] as an
    index into [obj+0xa9] (0x2b727); the only dword writers are
    0x2b840 (copy) and 0x54be9 (stores [[ecx+0x8]], another
    class). Also named: state flag word +0x12 bit 0x800 = state
    MIRROR, entered by `neg [obj+0x6d]` + vtable+0x54 (0x53670),
    and the row word `rev` calls that same flip (0x459d0 ->
    0x53670) - two FOCUS 1 rules to port next.

    PR-227 completes both sides of the provider. The ONLY code
    that writes the provider's fields is the block 0x549f2-
    0x54bf2 (its entry is not referenced by any static xref or
    vtable - finding that dispatch is what is left): it stores
    [obj+0xa5] = [[arg+0x8]] - the CLASS ID of the related
    object handed in the argument's third slot - so bit 0x20
    (K_GEN1) means "attached to a class whose vtable word has
    bit 0x20"; it also ORs the related object's record set at
    [[[obj+0x10]-chain]+0x60] into [obj+0x60] (0x54b78-0x54b84)
    and keeps [obj+0xb6] bit 0x10 as sign([obj+0xba]*[obj+
    0x6d]). NEW GATE: 0x47e40 match is called from exactly two
    sites - PR-217's row step (0x522e6) and 0x52000, a first-
    pitch row scan inside the refresh (0x536f0 -> 0x53520 ->
    0x52000): walk the STATE's row table [S+0x1a] (count +0x22,
    stride 0x15, clause set at entry+0), first entry whose set
    intersects a copy of [obj+0x60] wins; its +0xd/+0x11 dwords
    are returned and fed to 0x535a0. The port models one gate;
    the engine has two, in series.

    PR-228 finds the entry PR-227 missed: the block starts at
    0x549f0 and is vtable slot +0xfc of 15 class tables (the
    dwords 0x96a2c/0x9a618/0xa22e4 hold 0x50340, which zeroes
    the triple when [obj+0x106]==0, negates it when
    [obj+0x11a]==5, then calls the block). Its callers are two
    switches: 0x41ae2 (on byte [obj+0x160]) whose arms call
    vtable+0xf4/+0xf8/+0xfc and OR the RETURNED TEventSet into
    [obj+0x60] (0x49580) - more signal providers like +0x8c -
    and 0x50d70, a message-type switch over types 9..0x15 (jump
    table 0x50d20, misaligned; best read: type 0xf routes to the
    +0xfc arm with (msg+8, msg+0xc) as the pair). Inside the
    block, [obj+0xa5]=[[second]] - the CLASS ID of the object
    the message carries - so K_GEN1 machine-semantics: attached
    to a class whose vtable low byte has bit 0x20 (true for the
    0x9a778-style tables); the provider re-tests it every frame.
    Also: 0x54c00 is a sound-effect method of the same class
    family - signed byte [obj+0xb5] indexes a name via
    vtable+0x50 and resolves it through [[0xcef40]+0x390] and
    0x57530 (PR-222's settings lookup); [obj+0xee]<3 gates it
    (0x1f100) - PR-225's "field-fed sender" objects are this
    player class.

    PR-229 names the last link. AC_RECT (type 0xf) senders:
    0x28654/0x28b9b/0x3740c/0x374c5/0x3dc7b - each right after
    the 4-comparison rectangle test, tag = THE OTHER OBJECT'S
    POINTER, payload 2 or 3. The +0xfc arm (0x50dc7) takes that
    pointer as the argument and the entry 0x549f0 stores
    [other+0]->[obj+0xba], [other+4]->[obj+0xbe], and
    [other+8] (0x54be6-0x54be9) -> [obj+0xa5] - the byte the
    provider tests. [other+8] is the KIND METHOD TABLE pointer
    (0x9a754 = a table of the TEventSet/interval verbs 0x45120/
    0x45250/... - the very "class id" PR-221/PR-225 compared to
    the class globals). Bit law from measured low bytes:
    0x97970/0x978dc/0x96874 have none of bits 0x1/0x10/0x20;
    0x96fd8 has 0x18 (raises 41 only); the 0x9a74c/0x9a754/
    0x9a778 family (0x54 = bits 0x10+0x20) raises 41 AND
    K_GEN1. So: "my rectangle overlaps a 0x9a754-family kind ->
    the provider raises K_GEN1 every frame" - no GEN row, no
    message. Final question: which authored kind the ZARIA.01
    DRABINKA carries; then port the overlap rule.
    PR-233 ANSWERS it, NEGATIVE: the class token is authored
    level text (factory 0x4df10 chain); 'DRABINKA' matches at
    0x1f8ac and its kind word is 0xa2498 (0x531f0 tail
    0x532a6; low byte 0x98 = bit 0x10 only), KDRABINA's is
    0x9d8a8 (0x34e39) - neither carries bit 0x20, and the
    drabinka kind table's provider slot +0x8c is the NULL
    provider 0x23a60 (empty TEventSet, nothing raised). The
    0x9a778-at-+8 stores (0x29a51/0x2a18d) are mid-chain ctor
    stores overridden one instruction later (0x29a69 ->
    0x99828) - PR-229 read them as final kinds. So the ladder
    `WAIT K_GEN1` can only be fed via [drab+0x60]; PR-226's
    bit-0x20 writer of [obj+0xa5] is still the unnamed
    question (word writers measured: 0x2b6a0/0x2b840/0x54be9/
    0x54e24 + the LION keyed-list region 0x1e5xx).
    PR-234: the name block 0x91b40-0x91c68 confirms the port's
    68 ordinals (K_GEN1 = bit 54 stands); K_NEVER is a parser
    special, NOT bit 57 (no contradiction with the collision
    raise of 57); no raiser of 54 exists anywhere. FOUND: the
    AC_DRAB raise on the LION - 0x287a0 (called only from the
    collision loop 0x3eac6), gated on the state record's
    authored +0x4 ID == 6 or 0x12, synthetic type-0x10 tag-
    0x8138764 payload-10 message to the lion's +0x10. The
    climb's missing named pieces now: that raise (PORT), the
    GEN-tail admission effect (READ), the PR-227 attach-merge
    reachability for [drab+0x10] (READ).
    PR-235 SHIPPED that raise: ll_mov_st.block_pos is the
    record-ID word (ds:0xceff8 counter, 0x5150b/0x51a2a,
    record +0x4, object +0x8d via 0x5386c, read by the +0x6c
    accessor 0x23a10), and play_collide raises AC_DRAB on the
    lion when its state ID is 6/0x12 (= A_DEAD/A_DRAP by the
    authored block order) and the other's ID <= 0x19. Open
    here: the [[other]+0x3c] probe + 0x28820's second clause
    word, K_GEN1 delivery to the drabinka, AC_DRAB_CATCH's
    raiser.
    PR-236 READ the feed machinery: the refresh calls
    [[kind]+0x8c] (0x536ff) - the rope classes carry the
    NULL provider 0x23a60; where the slot holds 0x549f0 it
    evaluates the [obj+0xd2] ATTACHMENT-entry queue: kind
    1/4 proximity factors to the entry point, kind 2 = OR
    the entry's own 12-byte key (COPIED at enqueue,
    0x53f50) into [obj+0x60] when entry+8 is 0 or the
    object's state ID, then pop. The enqueuer 0x3d440
    (PR-214's +0x78 entry) needs the player at state 1, a
    parent at [obj+0x30] (a type-0xe effect, 0x52bef) and
    the receiver's K_FIRE bit (provider 0x54910 synthesizes
    it from [obj+0xa5] bit 1; it also synthesizes
    K_BKWD/K_FWD from the factors). So rope-inherits-
    lion's-K_GEN1 is an ATTACH path, keyed at enqueue and
    state-gated. Unread, and the named blockers: who runs
    0x28590 (table 0x9a390), who sends the type-0xe
    message, who sets [obj+0xa5] bit 1, and whether any
    provider synthesizes K_GEN1 itself.
    PR-237 PINNED the ask itself: kind-table slot +0x78 IS
    the ASK/answer verb - 0x553e0/0x4b500 walk objects,
    call it, and OR the returned TEventSets. A class-table
    anchor (slot +0x6c = the record accessor 0x23a10) names
    every table and slot: PR-150/216's ladder answer 0x3f1c0
    is that verb for exactly ONE class, 0x97cdc, while the
    DRABINKA table's own +0x78 is 0x53c90 (shared by 14
    tables) - reading 0x53c90 is the next named step here.
    0x3d440 (the PR-214 pair 0x96a88/0x99e64, CONFIRMED)
    answers only when the asked is at state 1 and the
    requester has a parent and K_FIRE; AC_STONE is its
    true-branch answer; it is what queues PR-236's attach
    entries. 0x28590 = class 0x9a44c's +0x78 override
    (PR-214's 0x9a390+0x134 arithmetic corrected). Open:
    what kinds 0x97cdc/0x9a44c are, who reaches 0x1eba0/
    0x380d0's ask, 0x552e0's 0x53ae0/0x55230 gates.
    PR-238 NAMES the tables through the engine's own
    name->object factory (0x1f870, strcmp chain over the
    class-name block 0x906fa-0x9078b): the PLACED
    DRABINKA's kind word is 0x97cdc, not the ctor default
    0xa2498 - so 0x3f1c0 IS the placed rope's answer verb
    (the port's ladder_answer_for stands) and its +0x8c
    provider slot IS the null one: the PR-236 attach-merge
    is RULED OUT as the rope's K_GEN1 feed. 0x96a88 =
    KAMIEN, 0x9d8a8 = KDRABINA (same factory); 0x1eba0 is
    the 0x96874 family's ask-all method (vtable slot
    0x97954): if the answer union contains K_GEN1 it sets
    [self+0x28] - PR-225's frame-step gate. 0x53ae0 is
    the block-response gate and answers {AC_BLOK}/
    {AC_BLOK_Y}. Feed candidates now named: (i) a signal
    message with payload 54 (none found, PR-234); (ii)
    0x536f0's own ORs into the rope's row (next read);
    (iii) a class answering asks WITH K_GEN1. Still open:
    the authored name of class 0x9a44c.
    PR-239 READ the whole per-step composition and RULED
    OUT four feed candidates by measurement: the placed
    rope's owner kind-table +0xa8/+0xac verbs are bare
    `ret`s; its ASK channel (0x52790 -> 0x4bd30/0x4b890/
    0x4b870) is gated on the record's `control=` bits and
    ZARIA's rope declares only BY_TIME, so it is off; the
    row's own `WAIT K_GEN1` clause adds NOTHING to the key
    (0x521f0 uses it only to pick the row path, and a Wait
    node's execute answers empty - the "row key feeds the
    transition" model was refuted before it could ship);
    and no code writes bit 54 or sends a signal message
    with payload 54 (the five builders send 0xa/0x21/0x1c/
    0x1b/0x39), though 0x53770 shows a signal message ORs
    its payload bit straight into [obj+0x60]. Named next:
    0x2b4d0 (record class +0x30) and 0x54560 (the placed
    rope's +0xc8) - both handed the context struct, so the
    question is whether either writes [obj+0x60] itself -
    then the control-name to bit map, 0x4b890/0x4b870, and
    how 0x520c0 spends the row budget.
    PR-240 ANSWERS the headline (what raises K_GEN1 on a
    DRABINKA): the ladder's OWN ask answer does. The answer
    verb 0x3f1c0 at the asked's first declared state (A_UP,
    [asked+0x5c] == 1) gates on the requester having a
    PARENT (0x3f1e9) and 0x44ec0 - an ANY-test over the
    12-byte-element vector at [[asked+0xa5]], count
    [asked+0xa9], each element's class verb +0x8 against
    the requester's [[kind]+0x4c] object (0x23a20) - and on
    true 0x3f203-0x3f20c sets byte 6 mask 0x40 of the
    asked's OWN live set: bit 54 = K_GEN1 (the store the
    port has long counted as "sets [self+0x66] bit 6"), and
    the answer itself is empty. The next step's composition
    (0x53725) puts it in [obj+0x60], where the authored
    `A_DROP(1)=K_GEN1` line matches: the chain A_UP ->
    A_DROP -> A_DOWN is reachable and needs no external
    sender. Ruled out here: the record-class +0x30 verb
    0x2b4d0 is a bare `ret`; the rope's kind-table +0xc8
    0x54560 is a message handler that never writes
    [obj+0x60]. NEXT: the PORT of the self-raise, and the
    paired read of the ask that triggers it (whose ask
    channel is on, and the rope's [obj+0xa5] contact
    vector); also open, who sends type 0xe (the parent
    store, an ARM of the base handler 0x52bb0 at 0x52bef,
    not a table slot) and type 0x21 (which sets the
    frame-step flag [obj+0x28] - a second feed of it).
    PR-241 PORTS that self-raise, test first: the ladder
    answer selftest gains leg L7, which builds the authored
    `[A_UP]` pair (`A_DROP(1)=K_GEN1` + row `1,0,WAIT
    K_GEN1`), asks a placed ladder at its first declared
    state, and pins that it answers NOTHING, raises K_GEN1 on
    ITSELF, and reaches the A_DROP block in the following
    record steps - unasked, it stays put. The leg fails on
    the stub and passes with the raise; no fixture moves and
    the pinned ZARIA.01 walk is unchanged because it never
    reaches the rope's pixel (180,510 vs ground y=680 ending
    at x=408). NOT MODELLED, counted: the [obj+0xa5] vector
    (PR-150's pair contact stands for the 0x44ec0 ANY-test),
    the requester's parent gate, and the engine's one-record-
    step delay. NEXT: who writes [obj+0xa5] and what its
    elements' +0x8 verb tests; whose ask channel reaches a
    placed DRABINKA at all; and the other half of the climb,
    the lion getting to a ladder (OI-072/081/082/083).
    PR-242 finds the gate is AUTHORED DATA the port never
    reads. The placement record continues past its five
    fields with three more lines: ZARIA.01's ladder carries
    `1 33,-3` (ONE contact point, relative to the place),
    `23,30;53,160` (a rectangle) and `30,50`. Census over
    hd_files/ETAPY: 2076 entry lines, 21 with those lines
    (11 points+rect+pair, 10 rect only), SIX classes -
    DRABINKA 4, LINA 7, KLINA 3, XCHG 3, PILAGEN 2, GUARD 2.
    The kind-table +0x18 verb reads them (the rope's =
    0x3f0f0): 0x44f10 parses the count into [obj+0xa9] and
    allocates 12-byte elements at [obj+0xa5] - a header that
    is itself an object, its method table being the 0x97f50
    block PR-239 read as a value, which also holds 0x44ec0 -
    and 0x44e90 adds the placement to every point (relative
    -> absolute, once, at build time). The element class
    0x98274's verb is 0x23e00 = POINT-IN-RECTANGLE, so the
    0x44ec0 ANY-test asks: is any of my authored points
    inside the asker's box - that box being the plain field
    [asker+0x75..0x84] (+0x4c = 0x23a20 in all 186 tables).
    The other gate is a CLASS gate, not a parent: +0x3c is
    `xor eax,eax` in 171 tables and `mov eax,1` in 15, and
    the lion is one of the 15 (TLewZaria -> kind table
    0x96960). So the authored chain is: the placed rope at
    A_UP, its one point at (213,507), asked by the lion,
    raises K_GEN1 on itself when the lion's box covers that
    point - 3 px above the placement, i.e. only a lion up at
    the ladder top, never one walking the ground at y about
    680. NEXT: who writes [obj+0x75]; parsing the point list
    in the port, which now counts those three lines as
    poly/shortline noise (ll_level.c 8135-8141); and the
    authored rectangle it ignores - this ladder's is x
    203..233, y 540..670, down to the ground line, while the
    port collides with the art bbox 42x48 at (180,510).
    PR-243 READS and USES that list. The placement parser
    now takes the `N dx,dy ...` line with the entry, stores
    the points ABSOLUTE on the placement (the engine's
    0x44e90 does exactly that at load), and the K_GEN1
    self-raise of PR-241 is gated by 0x44ec0's test: some
    point must lie inside the ASKER'S live rectangle, and an
    object with no list answers FALSE, so 171 of the 186
    classes never raise. Measured on the shipped data: the
    port's run of ZARIA.01 reports the DRABINKA's point at
    (213,507) - the engine's 180+33, 510-3 - and its two
    LINA ropes at (125,492) and (525,525); 3 list lines per
    placement pass. The asker's rectangle is now read too:
    [obj+0x75..0x84], refreshed by the kind-table +0x94 verb
    0x53db0 -> +0x98 (0x23a50 -> 0x534e0), which takes the
    position from the +0x68 getter and the extents from the
    state record's +0x48/+0x50 verbs; the port stands in the
    frame's SEQ shape box for those extents (counted: art
    based, mirror not applied). Test first: the L8 leg
    (no points, a walking lion's box, a 1 px miss, an exact
    edge touch, a covering box) fails on the ungated build
    and passes with the gate; L9 pins the list grammar. No
    play-probe or fixture moves - the ladder's only point is
    3 px above its placement, so a lion walking the ground
    never covers it, which is the jump-onto-a-ladder half
    again.
    PR-244 READS THE ASK ITSELF. An ask is a record-class
    row-bit channel: 0x52790 - a method of the placed
    rope's own class table (slot +0x14 at 0x97cd0) - tests
    row bit 0x20000 and calls the delivery walk 0x4bd30
    (out-packet, the level singleton 0xcef40, the row's
    OBJECT word, that word masked 0x8000 as the layer).
    0x4bd30 walks [[requester+0x64]] and asks each
    candidate whose +0x60 verb answers true and whose layer
    matches, with ebx = the requester; every answer is
    OR-merged into the set the transition key is built
    from (0x523f0's +0x10 verb, 0x52435). The parent gate
    PR-242 censused is called at 0x3f1d1, on the REQUESTER
    (PR-240 cited 0x3f1e9, which is really the rectangle
    getter); re-census confirms 171 fail / 15 pass and the
    lion is among the 15 (0x9699c = 0x96960+0x3c), while
    the rope's own +0x3c (0x23270, unique) returns the
    never-null global 0xceeac. So a ladder only answers for
    one of those 15 requester classes - the port's ask
    sites are lion-contact-only, already right by
    construction. And the rope's STATE-2 branch (0x3f214 =
    its A_DOWN) ANSWERS: requester position inside its
    first authored element -> bit 8 = AC_DRABK; inside the
    built rectangle from [self+0xb1] (0x49fb0, class
    0x98274) -> bit 12 = AC_TIME; else bit 10 = AC_DRAB -
    names shipped in table order at 0x919ec, matching the
    port's enum. The lion side is authored: ZARIA.01
    LEW.MOV `A_WSP(1)=AC_BREAK_NEXT,AC_DRAB` (328) and
    `A_FALL_DOWN(1)=!AC_DRAB` (363); LONA.05
    `A_WSP_LIN_UP(1)=K_UP,AC_DRAB` / `A_WSP_LIN_DOWN(1)=
    K_DOWN,AC_DRABK` (58/59). No code shipped. NEXT:
    decode the control-name bit map (row bits 0x20000 /
    0x40 / 0x80), what writes [[obj+0x64]] and the +0x60
    filter, the authored rect as a live box so a hanging
    rope can ANSWER AC_DRAB, the other 18 list-bearing
    entries, and the jump-onto-a-ladder half.
    PR-245 NAMED THE CHANNEL: the per-step key builder
    0x523f0 calls the record class's +0x10 verb (0x52435,
    0x52790 for the placed rope's class), and 0x52790
    switches on the control word it already reads: bit
    CHECK_LEW -> the ask walk 0x4bd30 (with the
    CHECK_ALL_PLANE bit as its layer argument), bit
    CHECK_ZW -> 0x4b890, bit CHECK_ALL_PLANE -> 0x4b870.
    CHECK_ALL sets all four check bits (0x51c73). ZARIA.01's
    ropes author bare BY_TIME - their records never ask -
    while the lion's states carry CHECK_ALL, so the ask
    comes from the lion, per step: PR-150's shape, now with
    its enabling bit. The walk's first gate is the
    candidate's +0x60 verb: 156 of 186 tables answer
    0x23b80 = `mov eax,[obj+0x2c]` - the contact-node
    field - so only a TOUCHED object is asked; the second
    gate is the layer match unless the CHECK_ALL_PLANE bit
    is clear. And the rope's own +0x60 (0x23140) is a
    rectangle-intersect of the AUTHORED rect [obj+0xb1..
    0xbd] against the requester's live box - the second
    live use of that rectangle (with PR-244's state-2
    answer): the engine's collision box is the authored
    rect. No code shipped.
    PR-246 PORTS THE PARSE AND THE FILTER. The placement
    reader now follows the record: list, `x0,y0;x1,y1`
    rect, bare `x,y` pair - all consumed, all counted; the
    rect is stored ABSOLUTE like the points (the earlier
    one-line version saw none). Measured: ZARIA.01 now
    reports `DRABINKA/drab.mov authors an authored
    rectangle: absolute (203,540)-(233,670)`, TUNGA.11's
    six rect-only entries are XCHG x3/KLINA x2/GUARD x1,
    and the whole census 11 lists + 21 records is now read
    (PR-242's numbers exactly). ladder_ask_raise gained
    0x23140's filter first: with an authored rect present
    it must intersect the asker's live box (an entry with
    no rect keeps the generic contact-node path, which the
    port's shape meet already stands for). Test first: the
    new L8 case (box covers the point, misses the rect)
    fails without the filter, exit 1, passes with it; L9
    pins the rect/pair grammars incl. the sorted-degenerate
    model choice. Nothing else moved: the walking probe
    still prints no K_GEN1 (the authored point stays 3 px
    above a walking lion), verify-all 65 ok, fixture-free
    13 + 22. NEXT: use the parsed rect for PHYSICS
    (walking INTO the ladder), then the jump half so the
    point gate can pass, then the [obj+0xc5] pair's
    consumers.
    PR-247 READS THE FILTERS AND CORRECTS HALF OF THAT.
    0x23140 is class slot +0x64, not +0x60 (+0x60 = the
    generic [obj+0x2c] getter here too, like 156 tables).
    Its caller is the broad phase 0x4b970: per layer it
    builds the layer's SCREEN window (0x4b940: origin from
    0x57500/0x57530, expanded -0xa0,-0x78,+0x1e0,+0x140),
    then asks every instance of the object's class chain and
    of [obj+0x64] via [class+0x64]; the answer sets
    [obj+0x2c]. Census of +0x64: 109 empty rets (always
    yes), 21/16/4/2 others, and 0x23140 in exactly 2 tables
    (0x97cdc, 0xa1dfc - the ladder family). So the authored
    rect is an active-window/culling box, and PR-246's
    asker-box filter modelled a test the engine does not
    run: the ask is NOT gated by the authored rect. The
    rectangle's field, order (x0,y0,x1,y1) and absolute-
    ness are CONFIRMED by the record parser itself (0x3f0f0:
    list 0x44f10 -> 0x44e90 absolute -> 0x49f90 rect ->
    0x3f124 adds the placement), and its third block
    [obj+0xc5] = (v, y-0x50, v, y) plus a (width+1)-element
    container stays counted only. The asker's rect in the
    point gate is the live box [obj+0x75] via the class verb
    +0x4c = 0x23a20 (`add eax,0x75`) for lion and ladder
    alike - runtime state built by 0x534e0 from the +0x68
    verb (the lion's = `add eax,0xd6`, the current frame),
    so the port's shape-bbox stand-in is the right model.
    The walk also walks [[0xcef40]+0x64], the global scene
    list, not the requester's. NEXT: drop PR-246's asker-box
    filter (test first), then the jump half so the point
    gate can pass.
    PR-248 SHIPS THAT DELETION. ladder_ask_raise loses the
    rectangle test: the walk's filter 0x4bd81 is class verb
    +0x60, and for this family that is 0x23b80 - the plain
    [obj+0x2c] getter 156 classes use - so the port's shape
    meet stands for it unchanged; 0x23140 is +0x64, the broad
    phase 0x4b970's test of the rectangle against a LAYER'S
    ACTIVE WINDOW (0x4b940 expands the layer origin by
    -0xa0,-0x78,+0x1e0,+0x140), whose answer is what sets
    that contact node. Test first: the re-pinned L8 case (a
    box covering the authored point but missing the rectangle
    - the ZARIA.01 geometry, the point sits above the rect's
    top edge) fails on the filtered build, exit 1, and passes
    after the deletion; the other seven L8 cases and the L9
    grammar legs are untouched. Measured unchanged: the
    walking probe's log is identical to the filtered build
    (only the report line's wording moved), no K_GEN1; make 0
    warnings, verify-all 65, fixture-free 13 + 22. The rect
    parse and its absolute x0,y0,x1,y1 storage stay (0x3f0f0
    confirms both) as data for a future culling model. NEXT:
    the jump half, then the AC_DRAB answer branch.
    PR-249 FINDS WHY NOTHING EVER ASKS IT. The authored
    DRAB.MOV has `control=BY_TIME` in all three states - no
    SOLID - so the port's solid loop skips the ladder before
    the shape test (measured with a temporary probe print at
    the ask call: zero evaluations over a 6 s walk and an 8 s
    walk+jump run, including the pinned tick 91 at the ladder
    column 180,650). The engine reaches it from the LION's
    CHECK_ANI channel (his A_WALK/A_JUMP/A_LOOKUP/A_DRAP/
    A_WSP states all spell CHECK_ALL = ANI|ZW|LN): the walk
    0x4b500 iterates the class group's instance arrays,
    filters on [cand+0x2c] and the plane, skips self, and
    calls the candidate's +0x78 with ebx = the requester -
    with NO shape test anywhere (the animals' meet test
    0x53ae0 lives inside their own answer entry 0x552e0). The
    ladder's A_UP row is `1,0,WAIT K_GEN1`, so it waits
    forever for its own flag. NEXT (PR-252): drive the ask
    per step from the requester's CHECK_ANI/CHECK_ZW bit
    matched with o->src, not from the solid-contact shape
    meet; test first.
    PR-250 read the JUMP row-entry channel instead, and
    it is settled: the tail tokens carry a channel id at
    [node+0x10] ('FC' 1 -> the pull pair block+0x18/0x1c,
    'F' 0 and 'I' 2 -> gated, 'IC' 3 -> the force pair),
    the [obj+0x95] flag is reset at the head of every
    state step (0x52217) and set only by the row-admission
    tail (0x5234a), so an `I`/`F` pull is a ONE-SHOT at
    row entry, and the sums are of the current row (the
    chain head table is at [rec+0x2e] indexed by the row
    cursor [rec+4], 0x5268e/0x52221 - PR-98's inference
    [PR-404 questioned that indexing because every code site writing
    [x+0x2e] writes a class table; PR-405 resolved it: that field is a
    class-table slot in the 140 generated state classes, and the value
    0x520c0 computes, `[[T]+cursor*4]+4`, is exactly the +0x04 verb of
    the table T when the cursor is 0. Node classes keep their table at
    [node+0x04] - 128 sites install the 0xa0f28 rows there - so
    `mov ebx,[edi+4]` at 0x520da IS the node's class table and
    `call [ebx+4]` at 0x520e1 IS its +0x04 visit verb. The
    chain-head/node-list reading stands; what is unresolved is whether
    the 140 generated classes and the 14 record classes are one object,
    in which case the row index is a vtable slot number.]
    now fact). PR-251 CORRECTS the arc numbers PR-250
    printed: run right, one-shot, the authored A_JUMP arc
    is 109 px (apex iteration 19, land 40) - PR-250's
    "50 px" applied gravity before the drag. And PR-251
    measured that the `I` pairs' slot, block+0x20/0x24,
    has NO consumer in the record-class family: the base
    +0x30 post-integrate verb 0x2b4d0 is `ret`, and no
    provider reads that slot. From the ground the lion
    contacts at 649, so this arc peaks at 540 and the
    authored point is 507 - 34 px beyond the jump: the
    climb needs the A_PRE_JUMP/A_JUMP_S route or a higher
    take-off, not a re-fit. PR-252 then read the WHOLE
    integration surface: 0x54250 has exactly three call
    sites - the direct table slot in 127 class tables, and
    two decl-gated wrappers (0x24700: +60/+30 on the T
    pair for decl 1/3/4/0x10; 0x33310: +/-5*[obj+0xf2] on
    G_X) - none of which reads block+0x20/0x24. The `I`
    pairs are consumed nowhere in the engine as read;
    PR-98's pull-pair account stands as the only
    behaviour-making one. The same read measures OI-015's
    T>=20 clamp: 0x54266-0x54275 raises block+0x8 - T_X
    ONLY - to 20 when the class verb table+0xbc returns
    true and [obj+0xb6] bit 1 is clear. NEXT (PR-253,
    test first): the ask-channel port.
    PR-253 SHIPPED the pull law instead (the ask channel
    waits one more PR): `F`/`I` applies once when its row
    becomes CURRENT and `FC` every iteration its row stays
    CURRENT, in 0x54170's own shape (from rest or the same
    sign it SETS the axis to the target, from the opposite
    sign it climbs by |target| and never past), with the
    authored units taken seriously - a row pair is raw/5
    px/s, a G= word raw*4 px/s^2 - and the PR-73/74
    LL_PLAY_JUMP_DIV/LL_PLAY_JUMP_G fit retired. The
    measured run now agrees with PR-251's number on its own
    terms: the authored A_JUMP reaches apex y 541 from the
    649 line (108 px, 20 climbing iterations - the closed
    form, asserted by tools/jump_law_measure.py, RED at
    HEAD), the row 0/1 `F 0,350` heads break the contact
    one row early, and the list wrap exits the jump
    MID-AIR (A_WALK at 144,549, grounded 0, tick 63). The
    34 px gap to the authored point 507 stands unchanged;
    the row's next step is the ask-channel port (the
    requester's CHECK_ANI/CHECK_ZW matched with o->src,
    replacing the shape-meet ask sites).
    PR-254 SHIPS THAT ASK CHANNEL. play_collide's non-solid
    pass is split the way the engine is: the paired-shape test
    stays where the meet actually lives (the animals' own
    answer entry 0x552e0 -> 0x53ae0, the port's
    obj_touch_enemy), and the ask - the requester's
    CHECK_ZW/CHECK_ANI bits matched with o->src, the plane,
    the self skip - now runs with NO shape test at all. It is
    measured with a new opt-in probe (LEO_PLAY_ASK=1, stderr,
    pinned by the verify-play ly_zask leg): in the ZARIA.01
    `1:FWD 100:JUMP 122:NONE` run 120 ask steps reach the
    DRABINKA at its declared state 1 = the flag branch, every
    one of them without a meet. What stands is now only
    0x44ec0's point gate, and the numbers say how close: at
    tick 119 the authored point 213,507 is tested against the
    lion's live box 188,509..235,575 - x covered, y short by
    2 px. The 2 px are named, not fitted: (i) the engine's box
    is the CURRENT STATE's extents (0x534e0 through the class
    verbs +0x48/+0x50), the port's is the current frame's
    shape margins; measured over ZARIA's 11-frame A_JUMP list
    the extents are top -34 / bottom +34 against that frame's
    -31 / +32, one px of the gap; (ii) the surface under the
    ladder's own column is the 650 line (the .plt line steps
    649 -> 650 at x 180), so the take-off for an apex under
    that point is 1 px lower. K_GEN1 still never fires, so the
    `A_DROP(1)=K_GEN1` line and the `WAIT K_GEN1` row still
    wait. NEXT: the asker's box as the state's extents (test
    first, the ly_zask leg is the failing test), then the
    AC_DRAB/AC_DRABK half of 0x3f214.
    PR-255 READS THAT BOX AND THE LEAD IS WRONG. 0x534e0 takes
    the position twice through the +0x68 getter and the extents
    through +0x48 (0x23aa0: the state instance's frame vector
    at [inst+0x2e] indexed by its own cursor [inst+0x4] - the
    CURRENT FRAME, not the state), then calls through
    [frame+0x33], and that field has exactly one writer in the
    image (0x41f4d stores the shared block 0xa0c08): the box
    builder is 0x42930. It writes corners from the same four
    signed words FUN_000420b0 computes - the bbox the port
    already models - with an inclusive convention that matches
    0x23e00, and it MIRRORS by the sign from the +0x50 verb
    0x53e40 ([frame+0x14] = `w0 & 8`, negated only with
    [inst+0x12] bit 3 set and [obj+0x6d] == -1): sign > 0 gives
    x0 = x + m0c, x1 = x0 + m10, sign <= 0 gives x1 = x - m0c,
    x0 = x1 - m10. The port ships that branch (live_box, leg
    L10, plus the ly_zask2 leg): measured at pos 190,649 the
    box goes 169,618..216,683 -> 164,618..211,683, the same
    width mirrored about the position point. So the remaining
    2 px at the ZARIA.01 ladder are NOT box-model questions:
    the box is confirmed, and what is left is the arc (the port
    apexes at pos 542 where the authored point needs 541) and
    the ground line the ladder's own column sits on (the .plt
    surface steps 649 -> 650 at x 180). The state-instance bit
    3 / [obj+0x6d] == -1 negation is not modelled - the port
    passes its own drawn polarity - and stays counted here.
    PR-256 MOVES THAT GAP THE WRONG WAY, AND THE SHAPE OF THE
    GAP CHANGES THE LEAD. At the register's own unit the
    authored A_JUMP words apex at exactly 100 px, not 108, and
    the integer walk reaches the ladder column later: the press
    at FWD 100 lands at x=189 and the apex at 189,550, so the
    live box on the ask frame reads 168,517..215,583 - the
    authored point 213,507 is INSIDE its x span at last and 10
    px of y away from it. PR-254's state-extents idea cannot be
    10 px. What the geometry says: the ladder's art hangs
    510..558 and its authored point sits 3 px above that top,
    143 px above the walk line under it, while A_JUMP's own
    words measure 100 - so the point gate is probably not the
    trigger at all. The leads, in the order PR-257 works them:
    (1) the ladder's own A_UP/A_DROP/A_DOWN states, where
    `A_DROP(1)=K_GEN1` is authored - the asker lives in the
    ladder's .MOV, not in the lion's box; (2) the
    `A_WSP(3)=AC_DRAB_CATCH,AC_ONAIR` and `A_WSP_LIN(3)=
    AC_LINA_CATCH,AC_ONAIR` transitions ZARIA's A_JUMP
    declares, which name a CATCH SIGNAL, not a position; (3)
    RAN.01's A_LADOW, the BY_TIME state its lion authors right
    next to A_START_LAD1 (`G=0,200`,
    `A_STAY(1)=AC_BREAK_NEXT`, 23 rows of 400) - a CLIMB STATE
    entered on a signal, not a jump arc. The state-instance bit
    3 / [obj+0x6d] == -1 negation stays unmodelled and counted
    here. NEXT: those three, then the AC_DRAB/AC_DRABK half of
    0x3f214.

    PR-257 ANSWERS the question PR-216 left open, "what
    raises K_GEN1 on a DRABINKA": the lion's OWN row
    directives - the K_GENn family is the level's signal bus
    in the authoring itself. ZARIA.01's raisers: A_JUMP rows
    4..7 `GEN K_GEN1`, the A_DRAP idle loop, A_FALL_DOWN's
    `s idle 1,300,GEN K_GEN1`, A_DEAD, A_KILOF, the fight
    states. Its consumers in the same level: both ladder
    .MOVs' A_UP (`A_DROP(1)=K_GEN1` + `1,0,WAIT K_GEN1`),
    KORONA's A_TWIST, every BAL's A_DEAD, POCISK, SURPRISE,
    WODA, ZARROW. A row's GEN admit now lands the K_GEN1..5
    bits on every scripted object (ll_level.c
    gen_bus_raise, one beat), and the placed ladder takes its
    authored A_DROP line at tick 106 of the pinned script -
    derived from the row's own thresholds (row 6 admits at
    106) - with NO shape, point or distance in the chain.
    Lead (1) is DONE. The 0x3f1c0 branch gate is implemented
    as PR-216 worded it: flag branch only at index 1, AC
    answer only at index 2, the tail elsewhere - so the point
    gate's 10 px were never the trigger, only the flag
    branch's own gate. What is left, in order: what index 2
    ANSWERS on a meet (the probe answers 0 at every meet of
    that leg) and the A_WSP entry those answers must feed;
    the object-side settling - the unrolled rope reaches
    A_DOWN in the port's first object step because of the
    `(1)` start-row token, an object-side row-start mapping
    named here, untouched; PR-217's persistent WAIT meter;
    the bus is one beat only; and 0x53db0's [obj+0x54]
    asker-memory (the verb clears its stale asker link, that
    asker's +0x14 = 0x550a0 zeroes its [obj+0xe6]/
    [obj+0xea]) is named, not modelled. Leads (2) and (3)
    stand as written; the [obj+0x6d] negation stays
    unmodelled and counted.
    PR-258 closes lead (2) as a NON-LEAD, read to the byte:
    ZARIA.01 has no AC_ONAIR poster. Nothing in the image
    OR-s an immediate into a live set; a raise is a 0x49580
    merge of a stack mask, one of two register read-modify-
    write sites (0x3f203, 0x3b601), or PR-224's kind-0x10
    record. A literal bit 18 would be a 0x00040000 dword or a
    0x04 byte at mask +2: the image holds 6 uses of that
    dword and none is a mask, and every 0x04 byte write at a
    mask base sits at +1 (AC_DRAB), +3 (bit 26 - and 0x3e0a8
    raises it onto ANOTHER object, [ecx+0x112]+0x60, an
    "attached object" field this project has not named) or
    +7. PR-224's 0x12 senders are class entries of 0x977f0
    (JBRANCH's sub-object), 0x9de64 (KBLOKZ) and 0x9fb24
    (XCHG); ZARIA.01 ships no script and places no object of
    any of them, so `A_WSP(3)=AC_DRAB_CATCH,AC_ONAIR` can
    never match there. What that level DOES reach in the port
    at HEAD: the index-2 answer `set 400` from 127,
    `A_WALK -> A_DRAP` on the walk's own `A_DRAP(1)=AC_TIME`
    row, `A_DRAP -> A_LOOKUP` on K_UP, and from A_TURN_BACK
    a real A_WSP through `A_WSP(1)=AC_BREAK_NEXT,AC_DRAB` -
    PR-148's wrap-armed AC_BREAK_NEXT plus the pair's
    AC_DRAB - whose rows admit at the authored 300,200,...
    while every one of its 17 ticks reads `pos 214,648`: the
    climb runs IN PLACE. Lead (2) is therefore ONE law,
    PR-148's open item 1: the v_y force at 0x542cd fed by the
    vertical-intent word [obj+0xbe]. 0x54910 (one caller,
    0x506d6, the lion family) is the input bridge that names
    that word - it turns the live input block into the key
    mask (K_UP/K_DOWN/K_FWD/K_BKWD/K_JUMP/K_FIRE/K_F1) - and
    the param field behind 0x542cd stays the open question.
    New and unmodelled from the same reading: 0x55130, the
    mask builder the lion's own +0x78 entry calls, raises
    AC_DRAB_CATCH on a comparison of the pair's [obj+0xad]
    words and AC_ENEMY otherwise.
    PR-259 ships the MOTION that paragraph left open. The
    add is `idiv([ebx+0x4] * [obj+0xbe], 20)` at
    0x542cd-0x542e3, and [ebx+0x4] is the SECOND number of
    the state's authored F pair: the x side of that same
    body, 0x542ab, multiplies [ebx+0x0] by the horizontal
    intent word and is the law PR-132 shipped, and the
    `G=` pair's second word is added after the idiv at
    0x542ee. The pass's door is widened by the STATE's
    authoring - a BY_TY or BY_Y state with a non-zero second
    F number and G_y = 0 runs the drag and the force even
    with ground under it, because what holds a standing body
    still in the engine is its ground contact at 0x54c00,
    which stays unread; 15 player-authored blocks qualify,
    every one of them a climb, so the reach is bounded (the
    census is in the PR-259 record). The project constant
    LL_PLAY_GRAVITY, which CHECK_ALL would otherwise put on
    a released climb, is scoped to `!p->grounded`. Measured
    at HEAD: the chain reaches `A_TURN_BACK -> A_WSP` at
    tick 151 and climbs 1 px per iteration - 141 px over 147
    ticks - and the ladder's authored contact point 213,507
    is inside the asker's live box from tick 263, the DOWNED
    rope answering `set 1400` = AC_DRAB,AC_DRAB_CATCH. It
    ends at 298: the ask PASSES on 297 and 298 with NO
    answer line, so no bit is raised on those iterations and
    the authored `A_FALL_DOWN(1)=!AC_DRAB` row takes him
    down. The next links, in order: why the answer verb is
    silent two ticks running (PR-257's [obj+0x54] asker
    memory), the lifetime of the port's player signal set
    against the engine's persistent 12 bytes at [obj+0x60],
    0x54c00, then AC_EDGE / A_PODCIAG at the top.
    PR-260 reads that set to the byte and turns the tail of
    the paragraph above into the PORT's deviation. The live
    set is a TEventSet: 12 mask bytes at [obj+0x60] plus a
    match-OP byte at [obj+0x6c] (PR-258's "count byte" -
    corrected: 0 subset, 1 disjoint, 2 equal, 3 intersects,
    >3 the error branch at 0x47fd8, whose string is
    "TEventSet::match :Bledna operacja"). 0x47e40 is that
    test, four operations over the same three-dword layout,
    and it has exactly two users: the transition-list walker
    0x52000 (21-byte entries = TEventSet + two payload
    dwords, at [[record+0x1a]], count [record+0x22]) and
    PR-100's row step, which asks match(row's own compiled
    condition key, the object's live blob OR the caller's
    key). RAISED BITS ARE NEVER LOWERED: raises OR a bit
    straight into the blob (0x3f203, 0x3b5f3 = bit 54), the
    per-frame key assembler 0x536f0 ORs two more masks in and
    zeroes nothing, every write that clears those bytes is an
    allocator/ctor path (0x32535, 0x32825 after 0x52860;
    0x4457b), and the dedicated 21-byte clear 0x495e2 has
    ZERO references in the shipped image (no data site holds
    its address, no rel32 call/jmp targets it). So the engine
    still holds tick 296's AC_DRAB when A_WSP's
    `A_FALL_DOWN(1)=!AC_DRAB` row is evaluated and cannot
    take him down; the port clears p->sig (ll_play.c:1327)
    and o->sig (ll_level.c:1020) every tick, and that is what
    ends the climb at 298. Two questions come before dropping
    the clear in PR-261: who sets the op byte of a COMPILED
    condition (the row class's slot +8, `call [[row+4]+8]` at
    0x52235, fills the key - the clause parser's `!` tag at
    0x47d60 has no read path to +0xc, and `!AC_DRAB` as op 1
    versus op 3-negated is not the same test), and re-deriving
    the AC_TIME / K_GENn / AC_BREAK_NEXT legs a persistent
    set would move. New and unmodelled from the same read:
    the input-key builder 0x54910 maps [obj+0xa5] bits 0/4/5
    to K_FIRE/K_ALT/K_JUMP, [obj+0xb6] bit 4 to K_FWD else
    K_BKWD, and the vertical-intent word [obj+0xbe] to K_UP
    (negative) and K_DOWN (positive) - the port has no signal
    side of that word at all - and the name resolver 0x48000
    gives K_ANY the mask of those six keys minus K_ALT and
    K_NEVER the mask of K_UP,K_DOWN.
    PR-261 closes that op-byte question and finds a FOCUS 1 gap
    in the same place. A .MOV transition line is compiled at
    0x51e4d through the thunk 0x49610 into the clause parser
    0x47d60, so an authored condition IS a 13-byte TEventSet,
    carried in the state record's vector at [rec+0x1a] as the
    21-byte entries 0x52000 walks, and matched with the
    ENTRY's op byte: ',' folds names into one mask with op 0
    (ALL of them), '!' sets op 1 (NONE), '|' sets op 3 (ANY).
    The port's grammar (parse_cond ll_play.c:398,
    obj_cond_true ll_level.c:825: OR-of-AND-groups with a
    per-name neg) agrees with the engine on every shipped
    shape except 7 clauses that mix '|' with a ',' list
    (ENA.03 A_TO_SWIM twice, ENA2 A_WALK, KADMAR.07 and
    ZARIA.02's two texts A_DRAP, KADMAR2 A_UPADEK) - the
    census over all 777 texts is in the PR-261 record. The
    live gap is K_ANY: 41 clauses ask for it, 40 written
    `=|K_ANY` = "any key pressed", and the port only records
    it (LL_GOP_ANY) - no evaluator reads gop, so every one of
    those rows is dead, including 8 in ZARIA.01's own lion
    (A_WALK(x), A_DEAD(33), A_DRAP(1), A_BORRING(10) in each
    of ZAR_LEW.MOV and ZAR_LEWPL.MOV). The six keys that mask
    names are exactly the intent words this port already
    carries, so a leg is writable; the mechanism row is
    OI-114.
    PR-263 writes that leg and corrects the paragraph above in
    two places. The any-rows were never dead: cond_true has
    answered K_ANY since PR-87; what was wrong is that it
    answered ELEVEN keys where the engine's constant
    (0x2001f8 in dword 1) names SEVEN - the four weapon
    selectors are outside it and K_ALT is inside it, so
    PR-261's "six keys ... minus K_ALT" goes too. The dead
    half was obj_cond_true, which had no case at all, and no
    shipped text puts an any-row in an object. No shipped leg
    holds a selector key in one of those states, so the whole
    suite is unchanged by the fix (measured); the fixture pair
    in the play and mov selftests is what pins the mask.
    PR-264 removes the group-grammar lead: both evaluators now
    match the way the engine does, ONE clause mask against the
    composed live set under the clause's operation byte
    (OI-114 CLOSED). ZARIA.01 has no mixed pipe/comma clause
    - its eight pipe-rows are all the single-name `=|K_ANY` -
    which read identically under the new grammar, and the
    climb chain, the ASK legs and the whole fixture suite are
    byte-identical (measured). What the replacement leaves
    open is the empty-set pair of questions, now OI-115.
    PR-262 withdraws the PR-260 sentence above ("RAISED BITS
    ARE NEVER LOWERED") and with it the first paragraph's
    claim that the climb ends on this port's clear. Re-read
    one call deeper: the object step 0x536f0 merges two sets
    into [obj+60h] (0x53705-0x5370c from the class table's
    +8Ch verb, 0x53725 the record key 0x523f0 builds), calls
    the transition pass 0x53520 - whose stack copy at
    0x53536-0x5354f is the ONE and only operand ever handed to
    the walker 0x52000 - and 0x53583-0x5358e then run
    `mov ebx,0Ch / mov eax,[esp+10h] / xor edx,edx / call
    749A0h`: memset(obj+60h, 0, 12) through the image's fill
    77C00h, which has exactly that one caller. It runs whether
    or not an entry matched. So the mask is lowered after
    every transition pass, and the port's clears are the
    engine's own: AC_DRAB is absent on 297/298 because the
    engine's fill emptied it, exactly as the port does. The
    data demanded it anyway: [A_WSP]'s `A_FALL_DOWN(1)=
    !AC_DRAB`, [A_JUMP]'s self-entry `A_JUMP(11)=AC_DUP` and
    [A_WALK]'s six `GEN AC_TIME` rows all assume a bit that
    goes away. Experiment, not shipped: with every clear but
    the fresh-object one removed, 5 selftest lines fail and
    the ladder ASK rows stop running from 107 on. The three
    state-entry clears (ll_level.c 1020, 1159, 1241) have no
    engine counterpart - 0x535A0 passes the mask pointer to
    the class table's +ACh verb and stores nothing - and are
    inert duplicates here; they matter only if a raise ever
    lands between the pass and the switch.

    PR-267 removes this row's AC_EDGE lead by measurement and
    opens a new one. (PR-268 shows the middle of this
    paragraph is wrong - the blocks it calls constants are
    rewritten by the record verb; kept for the count it
    carries.) The ZARIA.01 ladder's class (DRABINKA)
    is built at 0x1f8ac: its [obj+0xc5] block - the one P1 of
    its answer verb 0x3f1c0 tests, the branch that would
    answer AC_EDGE - is four ZEROS at the constructor, and
    PR-267 read no site as writing it again; its [obj+0xb1]
    block is (1,1,1,1). The
    predicate 0x23e00 is inclusive, so for any real position
    P1 and P2 are both false and the verb takes the inert
    exit 0x3f364: a placed ladder answers NOTHING, in any
    state. (The class's +0x8c collector is 0x23a60, the
    empty-set stub, so it raises nothing of its own either.)
    Counted, exactly six sites in the image raise AC_EDGE:
    the two answer verbs' P1 branches (0x3f24d, 0x3f41d -
    dead as above), the position-comparison pair 0x27ea3 /
    0x2811c, which also raise K_GEN1/K_GEN2 at 0x28127-
    0x2812a, the rect test at 0x3c2d3, and one self-raise
    0x41aa0 (`lea eax,[ecx+0x60] / or BYTE PTR [eax+1],1`)
    gated by an fcmp of the floats [ecx+0x10b] / [ecx+0x12f]
    and [ecx+0x5c] == 1. None of the six is in the lion's own
    0x96874 or any TLew* table. PR-268 reads the six owners and
    settles the question for this level. By table+slot they
    are 0x3f1c0 at 0x97cdc+0x78 (DRABINKA), 0x3f390 at
    0xa1dfc+0x78 (LINA), 0x3c270 at 0xa0230+0x78 (DRAB),
    0x27e40 at 0x9a778+0xa8 (CAMEL, ZOLW), 0x27fd0 at
    0x99828+0xa8 (ZOLW) and 0x41a50 at 0x97f64+0xd8 (the
    ZWIERZ animal base). ZARIA.01 places none of the last
    four, so its AC_EDGE can only come from the two climbable
    objects' own answer verbs - and those verbs are NOT dead:
    PR-267's constants are the ctor state only, and the record
    verb at table slot +0x18 (0x3f0f0) refills both blocks
    from the authored placement text (OI-117). NEW LEAD, with
    the numbers: the engine's P1 asks whether the ASKER's live
    position (the block at [asker+0xd6], filled from [obj+0]/
    [obj+4] at 0x54e0a) lies inside the ladder's edge band -
    for `DRABINKA,...,180,510` plus the bare pair `30,50`
    that band is x 195..225, y 480..560, the top of the
    authored ladder, and its P2 column is (203,540)-(233,670)
    with P3 the middle 70% of it. PR-269 feeds that column to
    `ladder_answer_core` (leg L11 pins the merged pair, the lone
    AC_DRAB in the outer band, the inclusive corner and the 1 px
    beyond), and PR-270 feeds it the EDGE BAND from the `30,50`
    line - x apx +/- trunc(n/2), y apy-0x50..apy; leg L12 pins
    the inclusive corners, P1's priority INSIDE the column, the
    n = 30 / n = 31 parity of 0x4a050's truncation, and that
    with both lines authored the object's OWN pixel answers
    NOTHING (the band covers neither the art anchor nor the
    ground under it). Both answer rectangles now come from the
    authored text. What is left before any of this shows in
    play is the THIRD lead, and it decides whether any of
    this port's ladder geometry is reachable at all: in this
    port the answer is reached only
    at a shape meet with the ladder (the two `ladder_answer_for`
    sites in play_collide), while the engine's candidate walk
    0x4b500 asks the +0x78 entry with no shape test at all
    (PR-249); ZARIA.01's ladder art hangs at (180,510) and no
    lion ever meets it, which is why no in-game leg moved for
    PR-269 or PR-270. Model the ask channel and the climb's
    `A_PODCIAG(1)=AC_EDGE,K_UP` becomes reachable.
    Second lead: the same level places two LINA ropes whose
    verb 0x3f390 raises AC_LINA and AC_LINA_CATCH (bits 21, 22)
    at 0x3f4ac/0x3f525, which the lion's script asks for by name
    and this port does not model.
    PR-271 MEASURES the ask channel LIVE in play and maps the
    authored chain end to end (its record carries the state
    lines): the ladder settles into A_DOWN one tick after the
    lion's own K_GEN1 bus and answers on the PR-269/270
    rectangles in real runs (AC_EDGE at the apex band, the
    lone AC_DRAB on the ground outside the middle 70%); the
    lion's A_WALK -> A_DRAP -> A_LOOKUP already runs. The
    chain to the top is A_DRAP -(K_UP + the merged AC_DRAB_
    CATCH, position point in x 207..229)-> A_TURN_BACK ->
    (list end + AC_DRAB)-> A_WSP -> (AC_EDGE band + K_UP) ->
    A_PODCIAG -> A_DRAP on top. FIRST BLOCKER, measured: a
    fast fall (~25-33 px/tick) tunnels through the .plt main
    ground without landing (x 171 and 204, y 682; respawn at
    spawn) - that is OI-073's unread grounded-y law, now on
    this row's critical path. Second lead stays the CATCH
    timing (position point inside x 207..229 at the ask tick).
    PR-272 REMOVES the first blocker: the landing search is
    now SWEPT from the contact line the previous iteration
    ended on (ll_player.prev_line), the continuity the
    engine's shape-box contact (0x42a70/0x42de0) forces.
    Measured on the very runs that tunneled: the lion lands
    (195,654 grounded 1) and STAYS; the cam-y leg's pins are
    re-derived from the law, not fit (grass at x 128 is 681,
    the feet seat at 653 + the idle shape's 28 contact, x
    drifts only by the LICHA SEMI push-out). The next lead
    is the CATCH timing (position point x 207..229 at the
    ask tick) and then the A_TURN_BACK -> A_WSP -> A_PODCIAG
    leg.
    PR-273'S EXPLORATION RUNS WALK THAT LEG END TO END:
    `1:FWD 98:JUMP 100:NONE 136:FWD 150:FWD,UP` raises the
    K_GEN1 bus (the JUMP), settles the ladder to A_DOWN,
    walks the position point into x 207..229 (x 210) and
    holds UP: A_WALK -> A_TURN_BACK (150, the CATCH answer)
    -> A_WSP (climbs 652 -> 560 at 1 px/t) -> AC_EDGE band
    + K_UP -> A_PODCIAG (266) -> A_DRAP (290) - the whole
    authored chain the port had never run. ONE gap stays
    before the level is climbable: A_PODCIAG's LIFT is not
    modelled - the lion drifts down (568 -> 608) and lands
    back on the ground (212,654) instead of the top platform
    (the .plt line `5 98,514;288,520;...`, y 518 at x 212).
    PR-273 reads the directive layer the lift runs on:
    `1,300,lock ... 7,500,rdir ... 8,300,unlock` = +0xb6
    bit5 raised over rows 1-7 (its record maps the bits and
    their readers); the lift's UPWARD motion source is the
    open question (SEQ step-motion word +0xea vs the
    contact pass). That is the FOCUS-1 finish lead now.
    PR-281 ELIMINATES BOTH HALVES OF THAT ALTERNATIVE: the
    step-motion word is the snap link (PR-280/PR-281), and
    the A_PODCIAG words F=0,0 T=100,100 G=0,0 feed the
    0x542cd force law NO pair, so the T channel produces
    no lift either. The FOCUS-1 lead is now the contact/
    draw pass 0x54c00/0x54d0c with the lock bit set (PR-
    273's other branch: the grounded-y instruction through
    [obj+0xb5]) and what raises or carries the pair at
    [reader+0xb1]/[+0xb5].
    PR-282 MEASURES THE DRAW-PATH HALF AWAY (blink/draw,
    OI-073 notes) AND FINDS THE LIFT AUTHORED: the level's
    ANI2.DES places a real DRABINKA at 180,510 whose record
    verb 0x3f0f0 builds an edge band {203,540,233,670} (the
    port's tested band, PR-270 - confirmed) and a CLIMB
    COLUMN x 210 +/- 15, y 480..560 - bottom 560 is exactly
    where today's A_WSP hands off to A_PODCIAG, top 480 is
    9 px above the platform-grounded position 489 of
    PR-279/PR-281: the engine's finish is "carry the rider
    to the column top, then let the platform's crossing-snap
    land it on 489". The missing piece has ONE name: the
    writer that moves the rider up the column - the rest of
    the ladder answer 0x3f1c0 (P3+ at 0x3f2a4.., the AC_LINA
    twins) and its caller 0x3ee2d.
    PR-283 EXCLUDES FOUR MORE CHANNELS BY MEASUREMENT - the
    pair-contact verb 0x3df00 (its 0x45320 tail is the same
    crossing law, cap 2), the block set (ZAR.BLK's six rows
    are nowhere near the wall), and the ONLY position-writer
    in the lion class: the carry law 0x54eb0 (pos = arg +
    snapshot - attach, with the save/attach trio 0x55070/
    0x54dfd) - a static ladder carries nothing. What remains
    over exactly these states: the [obj+0xb6] bit1 attach-
    skip gate (0x546ac/0x54eb9/0x54c14) and PR-273's bit5
    `lock` answer window, authored INTO A_PODCIAG's own
    rows - the lift hunt is now at the answer-window law.
    PR-284 ENDS THE HUNT: the answer verbs (slot +0x78,
    corrected from PR-282's +0x88 label) raise SIGNALS for
    TRANSITIONS, never motion; rdir is a randomizer (0x52070;
    CORRECTED BY PR-321: the rdir node applies 0x45a10,
    `mov DWORD PTR [eax+0x6d],0x1` - 0x52070 is called from
    the ADJACENT 0x45a50, the apply of the goto-family name
    that follows `pos` in the table 0x918ef-0x9198e);
    and the authored block says it - A_PODCIAG's T=100,100
    consumes any carried velocity in ONE iteration with
    F=0,0/G=0,0 and NO moving row, so its position is frozen
    by the port's own PR-256 law, while A_SCIAG (the
    descent) authors `m 0,1` rows for its slide. The port's
    A_PODCIAG fall is the LL_PLAY_GRAVITY stand-in replacing
    an authored zero; the FOCUS-1 finish is climb -> frozen
    pull-up -> hang -> the hang's own K_JUMP hop onto 489
    through the EXISTING ground path.
    PR-285 SHIPS THAT FINDING: the LL_PLAY_GRAVITY stand-in
    no longer runs in a state whose authored T_y >= 100 (its
    own words consume the register whole - the author's hold;
    the corpus census of 2318 state blocks finds no default
    gravity anywhere, and 39 player blocks use the hold
    idiom). ly_climb is RE-PINNED on the authored tail:
    A_PODCIAG holds 212,560 for all 26 of its remaining
    iterations (263..288), and the drop that follows is
    A_DRAP's OWN G=0,250 against its T_y=15 - 250, 463,
    644, 798, ... 1340 register units, steps 2,4,6,7,9,10,
    11,12,12,13 - landing grounded at 299 (654 + the 28
    contact = the .plt line 682 at x 212). No project word
    is left in that chain. The platform top is still not
    reached, and the named route to it is the hang's authored
    `A_JUMP_S(1)=K_JUMP` row - the next FOCUS-1 leg.
    PR-286 MEASURES THAT ROUTE AND FINDS IT IS NOT AN AUTHORED
    ROUTE TO THE PLATFORM: from the hold, K_JUMP does take
    A_JUMP_S, whose rows carry `I 120,-1200` five iterations
    in, but against the A_DRAP fall's carried 463-unit register
    the arc is 27 px - apex 553 at 302, which leaves 63 px of
    feet-clearance under the platform line 518 (a lion standing
    on it sits at 489); with K_UP held instead the chain is
    A_TURN_BACK -> A_WSP -> A_PODCIAG with NO position gain,
    because A_WSP's first row leaves itself the moment AC_EDGE
    is true. No state in the family rises 71 px; the 30 px at
    the top is LIFT.SEQ art. ly_chang now pins the whole chain
    plus the negative (no position in 212,4xx). The ladder's
    OWN script also explains the PR-216 answer gate - only
    state 2 = A_DOWN, the LOWERED ladder, answers (A_UP
    `1,0,WAIT K_GEN1` -> A_DROP frames 2..10 = 2500 units ->
    A_DOWN `WAIT K_NEVER`) - and the port reaches A_DOWN ONE
    iteration after A_DROP, not 25: both halves are open.
    [PR-287 CORRECTS that sentence twice over: 2700 units,
    27 iterations, and the port now walks all of them - see
    its note at the end of this row.] What
    the probe re-opens is PR-273's GROUND PAIR: A_DRAP authors
    G=0,250, so unless the ladder's answer runs ground_on on
    the rider ([obj+0xb6] bit0/1, the target's vtable
    +0xc0/+0xc4 = the lion 0x54fe0/0x55020), the port drops
    its rider off the top of every ladder it climbs - counted
    at OI-073, where the grounded-y instruction and the
    ground_on/ground_off caller are the open pair.

    PR-287 CLOSES the timing half of PR-286's "both halves are
    open" and CORRECTS its arithmetic: the authored A_DROP list
    is 500+400+400+300+300+200+200+200+200 = 2700 units, NOT
    2500, and the port now walks all of it - 27 of its own
    iterations - so the ZARIA.01 ask log reads A_UP through
    tick 103, A_DROP (state index 3) 104..131 and the
    answerable A_DOWN from 132; ly_climb now pins those three
    ticks plus the negative that no tick below 132 reads state
    2. Why the port dropped the ladder in ONE iteration before
    is PR-286's other half, and it was a bug of the port's own:
    A_UP's `1,0,WAIT K_GEN1` row was admitted every step, its
    one-row list wrapped, and that list-end bit - armed by the
    previous step, tested by the next (PR-135) - reached
    A_DOWN's `A_DOWN(1)=AC_BREAK_NEXT` at once. The row gate
    of OI-111 (ll_mov_row_gate, in obj_advance before the
    clock, as 0x52235 precedes 0x52329) is what holds it. The
    ground_on/ground_off pair this row re-opened is untouched
    by it: the rider is still dropped by A_DRAP's own G=0,250.
    PR-274 PINS THAT CHAIN as a leg (ly_climb, its record
    derives every tick from the fixture's own arithmetic):
    the whole chain runs deterministically and the leg also
    pins the wrong finish - A_PODCIAG's drift down and the
    682-ground landing - so the lift fix has a net to land
    on. The two end-pins (289's exit, 294's grounded 654)
    are the ones that move when the lift is modelled.
    PR-275 NARROWS THAT MODELLING: the only engine path
    found that moves a body UP to a line exactly is the
    surface-table snap - a provider object holding a
    polyline table (0x45120/0x45200) is attached to a
    target, whose PREDICTED position (pos + [obj+0xca]/
    [obj+0xce]) is measured against the bracketed segment
    line (0x45380, 0x455a0) and, when at or below it, is
    snapped exactly onto the line by the one-shot
    [obj+0xea] in the position step. Which provider is
    attached to the lion during the `lock` window, and what
    table holds the ZARIA.01 polylines, is PR-275 OPENS (i)
    - the port's landing search only looks DOWNWARD, so no
    current leg can even see a top-platform snap.
    PR-276 NARROWS (i) FURTHER: the table text is exactly
    the placement-file polyline line (KATAPULT/PALMA author
    seat lines this way; DRABINKA/LINA author one-point
    lists that can never bracket = non-surfaces), but NO
    ZARIA.01 placement line carries the top-platform
    polyline, and none of the five 0x45120 call sites is a
    ground-file loader - the loader that splits ZARIA_A.PLT
    and the object that holds its lines remain the named
    missing pieces before the lift can be ported from law
    rather than modelled as a choice.
    PR-279 ANSWERS IT: the wiring parser itself loads
    platformy= (0x4dc00 - header %d, one 12-byte surface-
    table object per line on a manager array, each parsed by
    the element method 0x45120), and the position step
    attaches every actor against EVERY ground line
    (0x4b600/0x4b660 at 0x546d7/0x54719, closest non-positive
    gap wins at 0x454a2). The lion carries no table - its
    lew.des names only the carried-coconut child - so the
    provider is the .plt lines themselves. What remains named
    before the lift is ported from law: where the feet-
    contact term enters the snap point, and the state gate
    that keeps A_WSP (feet below the line for 90 ticks) out
    of the attach.
    PR-280 RESOLVES BOTH AND MOVES THE LEAD: the snap
    channel is a LINE-CROSSING LANDING law - a ground line
    is accepted only when the two evaluation points (pos,
    pos+velocity) sit on OPPOSITE sides of it - so a line
    above both (the top platform, all of A_PODCIAG) can
    never be chosen. The port's ly_climb finish (grounded
    654 at 294) is what the engine's law produces; the
    "expected 489" framing is withdrawn. The climb-to-the-
    top law lives elsewhere: the ladder answer channel or
    the block queries (0x4b430/0x4b7f0) are next.
    PR-277 PINS THE MID-CLIMB JUMP CHAIN as a second leg
    (ly_cjump): K_JUMP at 200 takes A_WSP's own row, the
    I 250,-1100 launch, the mid-air re-CATCH at 228, the
    652 landing, the climb resuming - all derived, all
    green. It also names AC_DUP as a producer gap: two
    ZARIA.01 lion rows (`A_PRE_JUMP(11)=AC_DUP`, A_WSP's
    `A_TURN_BACK(1)=AC_DUP,K_DOWN`) wait on a bit nothing
    in the model raises; the PR-23x channel 0x3df00 ->
    [asked+0xd8] -> 0x45320 with [asked+0xee] is the
    measured shape of that raise, producer unnamed. And it
    records that releasing keys mid-climb today lands the
    climber back inside A_WSP - A_WSP grounding/walking is
    a model question, not a leg pin.
    PR-278 PINS THE DOWN-DESCEND CHAIN as a third leg
    (ly_cdown): mid-air AC_EDGE + K_DOWN enters A_SCIAG, its
    `m 0,1` rows descend one px an admit at PR-100's row
    clock, the completed list's AC_BREAK_NEXT re-enters
    A_WSP, and A_WSP's own `A_FALL_DOWN(1)=!AC_DRAB` takes
    it inside the edge band (PR-270's P1 precedence) into a
    .plt-grounded fall. The ladder chain is now pinned
    three ways; the ONE thing ZARIA.01 still lacks is the
    lift itself.
    PR-290 CLOSES THE WRONG-DOOR HALF of that. The set a
    `Wait` clause is matched against is composed per record
    from its own `control=` keywords (OI-111 item (5), the
    record class's +0x28 verb 0x52790): CHECK_LEW asks a
    neighbouring object for its set through 0x4bd30, CHECK_ANI
    and CHECK_ZW through the shared body 0x4b500 behind
    0x4b890 / 0x4b870, and the answer verb 0x23920 hands back
    field +0x12 of `[obj+0x50] + [obj+0x5c]*4`. DRAB.MOV's
    three states carry only `control=BY_TIME`, so all three
    queries are skipped and the composed set stays EMPTY: the
    lift's `1,0,WAIT K_GEN1` and `A_DROP(1)=K_GEN1` are
    matched against the lift's own raised bits alone - exactly
    what PR-287's `ll_mov_row_gate` reads, so that gate is
    right for this record. A signal bus is therefore no longer
    a candidate explanation; what remains is which code
    performs the ASK that sets bit 54 in the lift's own set
    (PR-241's 0x44e70 shape is the only writer found) and why
    the engine performs it there and this port's probe does
    not.
    PR-291 FINDS A CODE-SIDE PRODUCER. The state-change pass
    (0x53520, PR-262) writes the new state index at 0x535ed
    and then calls `[[obj+0x08]+0xac]` - the object's own
    class word, the OI-118 field - whose entry for this class
    family is 0x3f1c0 (installed at 0x97d54 = 0x97ca8+0xac,
    the record-table block of PR-290). That verb branches on
    `[obj+0x5c]`; at index 1 - the state a placed object
    starts in (the ctor writes 1 at 0x538ab), and for the lift
    that is A_UP - it calls its own +0x4c verb 0x3f0f0, which
    works over the object's AUTHORED ZONE ([obj+0xa5], built
    by the `platformy=` element method 0x45120 at 0x1e51c; the
    rectangle at [obj+0xb1], the base-class point of 0x98278,
    the zone test 0x4a0b0 of PR-279/PR-280), and asks every
    element of that zone through 0x44ec0 (element +0x08 verb);
    on the first true answer it sets `[obj+0x66] |= 0x40` =
    signal 54 = K_GEN1 - the bit the `1,0,WAIT K_GEN1` row and
    the `A_DROP(1)=K_GEN1` transition test. A census of that
    raise idiom finds exactly 18 sites (0x1e546, 0x1ec96,
    0x24836, 0x24a38, 0x24f07, 0x2ca41, 0x2f48f, 0x2f5cc,
    0x2f628, 0x2fac7, 0x306da, 0x3b5f3, 0x3b6c1, 0x3bba1,
    0x3c321, 0x3f203, 0x3f3d3, 0x6ebf7); two raise it from a
    contact record of kind 0x19, one at |Δx| < 150. The next
    reading is the test itself: what value 0x3f0f0 passes and
    what a zone element's +0x08 verb answers.
    PR-292 ANSWERS BOTH AND CHANGES WHAT THIS ROW SHOULD ASK.
    The ENTER verb's argument is `[obj+0x60]` itself - the
    object's own signal set, merged a moment earlier by the
    frame step 0x536f0 through two 0x49580 OR-s (its +0x8c
    verb's set at 0x5370c and OI-118's 0x523f0 at 0x53725) -
    carried from 0x53705 to 0x535f0. And the measured
    behaviour in the shipped ly_climb probe is that the CONTACT
    TEST NEVER FIRES for this object: every tick reads `asked
    ladder DRABINKA/drab.mov at state 1: its authored contact
    point 213,507 tested against the asker's live box ... ->
    no (0x4b500 -> 0x3f1c0 -> 0x44ec0)`, while the lift does
    start - `play tick 104: DRABINKA/drab.mov takes its own
    K_GEN1 line: state A_DROP` - through the lion's GEN rows
    of PR-257's chain, which PR-287's gate now holds until
    then. So the row's question is no longer "who raises
    K_GEN1 on the lift" (measured answer: the PR-257 route)
    but whether the port's PR-243/PR-255 `live_box` is the box
    the engine tests with: 0x3f0f0 builds its own from the
    object's authored rectangle ([obj+0xb1] normalised by
    0x49f90, and a degenerate {x, y-0x50, x, y} box at
    [obj+0xc5] through 0x4a050), not the asker's art box.
    Authored data, read by the port's own tool: ANI2.DES
    places DRABINKA/drab.mov at (180,510) with the rect
    (203,540)-(233,670) and exactly one contact point,
    absolute (213,507).
    PR-294 OPENS THE ONE CHANNEL THIS ROW HAD NEVER MEASURED -
    THE LADDER'S OWN COLLISION GEOMETRY - AND IT CHANGES WHERE
    THE FINISH IS LOOKED FOR. ANIMACJE/ZARIA.01/DRABINKA holds
    exactly one record, DRAB.SEQ, id=1, 18428 B; it parses to
    11 elements and ALL 11 carry shape margin lists, matched
    1:1 by the script's own frame numbers (A_UP 1, A_DROP
    2..10, A_DOWN 11). Measured boxes relative to the position
    point, by the port's own 0x420b0: el0-3 (10,2,43,48), el4
    (10,2,43,74), el5 (10,2,43,90), el6 (10,2,43,105), el7
    (10,2,43,119), el8 (12,2,43,134), el9 (12,2,43,149), el10
    (12,2,43,168) - x +10/+12, width 43, and y = +2 IN ALL
    ELEVEN. At (180,510) that is a 43-px column whose TOP EDGE
    IS INVARIANT AT y 512 while its bottom moves 560 -> 680:
    A_UP is the ladder stowed (its solid column ending 94 px
    above the ground the lion stands on), the A_DROP walk is
    the ladder being let DOWN, and A_DOWN holds it at the
    ground - PR-286's "the LOWERED ladder" now has its
    geometry. Cross-checks: ANI2.DES's authored rectangle ends
    at x 233 = the shape box's right edge at frames 1-4
    exactly, and PR-270's AC_EDGE band x 195..225, y 480..560
    has its lower edge at the bottom of the STOWED column - so
    the port stops the climb (PR-286: "A_WSP's first row
    leaves itself the moment AC_EDGE is true") exactly at the
    stowed ladder's foot. And the invariant top 512 is 6 px
    below PR-286's platform line 518: the target 489 is a body
    standing on the fixed top edge of the ladder's art. What
    the port does NOT do with any of this: seq_geom keeps one
    `sidx0` = the first element with margins (hence the wiring
    line's single "bbox 42x48" for all three states) and
    ladder_answer_for consults only the authored rectangle and
    band - obj_shape_elem would resolve the current frame's
    element, but no ladder path calls the shape geometry. The
    two named unreads are then (i) whether the engine's ladder
    answer or 0x44ec0 test ever consults the margins, and (ii)
    which point the ask is made with: the port asks with the
    rider's position point and its stop at 212,560 follows
    directly from that.
    PR-295 REMOVES ONE HYPOTHESIS FROM THIS ROW BY CENSUS. The
    engine's signal-name table is at 0x91a0e..0x91c5e (69
    names, index = bit, matching the port's LL_SIG_* order),
    and the inline bit-set idiom PR-291 used for K_GEN1 exists
    at exactly 24 places in the image, for 7 signals only:
    AC_TIME, AC_ENEMY_ATC, AC_KOREK, K_GEN1 (18), K_GEN2,
    K_GEN3, AC_XCHG_LEVEL. AC_EDGE, AC_DRAB, AC_DRABK,
    AC_DRAB_CATCH, AC_ONAIR, AC_LINA and AC_LINA_CATCH have NO
    inline raise site anywhere, and there is no 32-bit form of
    the idiom. So the answer signals this row is about can only
    arrive as SETS - through the EventSet OR 0x49580 (69 call
    sites), the mechanism PR-292/PR-293 found in the frame step
    and in the set composer. Which of those call sites feeds a
    rider's set is now the named FOCUS-1 reading; the port's
    inline AC_EDGE/AC_DRAB raises stay the counted model
    choices they are documented as.
    PR-296 ANSWERS THAT CENSUS AND CORRECTS THIS ROW'S ANSWER
    MODEL IN ONE POINT. The answer signals are built into
    SETS: the EventSet ctor 0x749a0 is called at 192 places and
    45 set-bit raises over 33 signals follow them - AC_EDGE at
    0x3c2d3/0x3f24d/0x3f41d, AC_DRAB at 0x28805/0x36a1f/
    0x3c2b9/0x3f355, AC_LINA at 0x342ff/0x3f525 (PR-282's
    twins), plus K_UP/K_DOWN/K_FIRE/K_ALT/K_JUMP at 0x5492c
    and K_F1..K_F12 at 0x50643, i.e. the engine turns keys
    into the same signal vocabulary its rows test. Inside this
    object's own 0x3f214 path the geometry PR-268/PR-269/
    PR-270 derived is confirmed word for word (the band
    [obj+0xc5] at 0x3f228, the authored rectangle
    [obj+0xb1..bd] with the class word 0x98274 and
    `edx = 0x46` into 0x49fb0 at 0x3f278-0x3f293) - but the
    POINT TESTED IS THE OBJECT'S OWN POSITION, taken through
    its +0x68 getter at 0x3f222, not the asker's. On the
    shipped placement (180,510) that point lies in neither the
    band x 195..225 y 480..560 nor the rectangle
    (203,540)-(233,670), so this path returns an EMPTY set for
    this ladder: `ladder_answer_core`'s `pt_of(asker)` is the
    port's own construction, and it is what makes the probe's
    0x1400/0x100 answer sets possible at all. PR-292's
    "the contact test never fires" is the same fact from the
    other side. The real producer shape is 0x3c293, which
    raises AC_DRAB and AC_EDGE into ONE set beside a K_GEN1
    raise site at 0x3c321 - that, not 0x3f214, is the next
    FOCUS-1 reading.
    PR-297 READS THAT PRODUCER. 0x3c270 is an ask in exactly
    the sense the port's answer model means: eax = the object
    holding two authored rectangles, at +0x3c and +0x50 (a
    THIRD layout, not the DRABINKA's +0xb1/+0xc5), ebx = the
    other object, edx = the caller's out-set pointer. It
    builds an empty 12-byte set on its stack (0x3c27e-0x3c293),
    is gated on that other object's +0x3c verb (0x3c29d),
    takes ITS POSITION through the +0x68 getter (0x3c2a9) and
    tests it with the point's own +0x08 verb: inside
    [obj+0x3c] sets `or BYTE PTR [esp+0x1],0x4` = AC_DRAB
    (0x3c2b9), inside [obj+0x50] sets `...,0x1` = AC_EDGE
    (0x3c2d3), and the set is copied to the out pointer by the
    rep-movs idiom at 0x3c2d8-0x3c2f7. It never writes an
    object's bits, and it has no caller in .text: it is stored
    at 0xa02a8 inside a block of repeated 68-slot (0x110-byte)
    class tables running 0x9fd38..0xa0744 - 0x45320 at
    0x9fd58/0x9fe68/0x9ff78, 0x45120 always at +8, 0x23e00
    always at +0x14, and the fourth instance at 0xa03b0 holds
    0x3c310 at slot +0x20 and 0x52a00 (PR-293's distance
    helper) at +0x30. No immediate anywhere in .text names any
    of those tables. The sibling 0x3c310 calls 0x3dcb0, tests
    `cmp DWORD PTR [rec+0x4],0x19` (0x3c31b) and on a match
    raises K_GEN1 inline at 0x3c321 - PR-291's kind-0x19 raise
    read to its test.
    PR-298 REDIRECTS THE QUESTION AGAIN, AND THIS TIME IT IS A
    MECHANISM, NOT A MISSING SIGNAL. The engine branches on the
    CURRENT ROW NUMBER of the current record: [rec+0x04] is
    compared at 49 places with 20 distinct constants (1..7,
    10..16, 20, 21, 25, 38, 45, 60 - all valid row indices,
    since a shipped state carries up to 97 rows), and two of
    those are jump tables over row ranges. Read examples:
    `row == 6` builds a set containing AC_DRAB (0x287ea ->
    0x28805), `row == 25` raises K_GEN1 inline (0x3c31b ->
    0x3c321). So an answer signal can be produced at a
    specific authored row with no `GEN` token anywhere, which
    is exactly the shape PR-296 was looking for. Tying a row
    number to an authored row still needs the class identity
    (PR-297 OPENS (i)), and the port models no row-number
    dispatch at all - ly_climb and ly_chang are pinned on the
    row machine as it is, so any change there is a pinned-leg
    decision, not a free fix.
    PR-299 READS THE SECOND PRODUCER AND THE PATTERN IS NOW
    COMPLETE ENOUGH TO NAME. 0x287a0 (no E8/E9 caller, so a
    class-table verb) uses the same three-register convention
    as 0x3c270 and the SAME gate - the other object's +0x3c
    verb (0x287b1, as at 0x3c29d) - so being askable is a
    property of the object asked about. Its geometry half
    differs: it calls the +0x6c slot (not the +0x68 position
    getter) and tests THAT against this object's [obj+0x3c]
    box. Its signal half is what matters: through the
    three-instruction accessor 0x55760 (`mov eax,[eax+0x40];
    mov eax,[eax+edx*4]; ret`, 7 call sites, two of them
    inside the collision loop 0x3eac6 the port already models)
    it takes entry 1 of the parent/neighbour array at
    [obj+0x30], reads THAT entry's state index and its current
    record's row cursor, and `row == 6` sets AC_DRAB in the
    returned set (0x287ea -> 0x28805); the other branch wants
    the other object at state index 18 with row <= 25
    (0x28813/0x28826). So the engine's answer machinery is:
    gate on the asked object, choose by geometry OR by the row
    number the paired object is standing on, return a SET -
    never an authored `GEN` line, never an inline bit. Whether
    either verb is reachable for the ZARIA.01 DRABINKA is
    still the class-identity question.
    PR-300 MOVES THAT QUESTION AND SHARPENS IT. The class
    tables are now measured: 26 tables in 0x99054..0xa0394
    share one slot map (0x45250 at +0x10, 0x45120 at +0x28,
    0x23e00 at +0x34, 0x52a00 at +0x4c), and 21 of them hold
    0x3dcb0 in the +0x3c slot that both pair verbs use as
    their gate - so "is this object askable" is a class-shape
    and zone test, not a state test. The overrides are named
    (0x27700, 0x52bb0 twice, 0x34620, and 0x3c310, which is
    the derived gate of the table at 0xa0394 and calls the
    base gate first). 0x287a0 is the +0x80 verb of the table
    at 0x99fe8, and at the +0x80 call sites its second object
    is the global ds:0xa94ec, not the loop entry - so the
    port's acdrab_law caller story (PR-235, "reached ONLY from
    the collision loop") does not hold: the loop
    0x3eac2-0x3eb0c walks entry indices of the array at
    [[obj+0x30]+0x40], bounded by [[obj+0x30]+0x3c] and
    stopped at self, and calls the +0x10 slot, which for these
    classes is 0x45250. Its numbers stay; its caller is now
    named as unsupported.
    PR-301 CLOSES THE GATE QUESTION AND HANDS FOCUS 1 A HANDLE.
    The +0x3c verb is a four-byte constant: 146 of the 176
    installed class words hold 0x23c10 (`xor eax,eax; ret`,
    never askable) and exactly 14 hold 0x230a0 (`mov eax,1;
    ret`, always askable). Nothing about an object's state,
    rows or position can change the answer, so the askable set
    is a closed list of fourteen vtables - 0x96960, 0x97970,
    0x97a98, 0x98f34, 0x9a54c, 0x9b4d4, 0x9c958, 0x9d338,
    0x9e134, 0x9f170, 0x9f540, 0x9f668, 0xa0624, 0xa2218 -
    each named by its constructor sites (0x1ff15/0x20495,
    0x1e0a6, 0x269c2/0x26a75, 0x2703e/0x2743f, and so on), all
    fourteen carrying 0x23a10 at slot +0x6c. Pair answers can
    only come from those classes, so the remaining step for
    this row is the level loader's per-object kind to
    constructor mapping: with that, the ZARIA.01 DRABINKA's
    class word is computable and its answer path is either in
    the list or provably not.
    PR-302 TAKES THE NAME STEP HALF-WAY AND REMOVES A
    WRONG TURN. Classes are chosen by NAME: the loader compares
    31 names (ANITAB, PLANT, GRABOBJ, DEMOTEXT, ANIBACK,
    MOVEPLAT, MOVERECT, RECT, DRECT, DRECT2, KATPLAT, DZWIGNIA,
    WODOSPAD, ZWIERZ, GRUPA, PICT, POCISK, IDPOCISK, GRABG,
    GWIAZDKI, NIESP, ZONE, KORONA, SURPRISE, BAL, DNGANI,
    TRASA, TRASA2, LINA, TABLICA, BERLO, at 0x91f3d..0x92012)
    in one chain at 0x4df15..0x4e934, so there is no kind-to-
    constructor table to find. Measured through the port's own
    wiring dump, ZARIA.01 places ZWIERZ x9, LINA x6 and
    POCISK/PICT/KORONA/BAL/ANITAB/ANIBACK x2 - and no DZWIGNIA
    ("the lift") and no MOVEPLAT anywhere: the lift is the
    animated object named DRABINKA, so no lift-class object can
    be the signal source. The remaining candidates are that
    animated object itself and the level's `ani_plat` set
    (anipl2.des) on the middle layer - and the pair-answer
    verb 0x287a0 turns out to test exactly that pool.
    PR-303 NAMES BOTH SIDES AND LEAVES ONE FUNCTION TO READ.
    The ladder's class is DRABINKA = 0x97cdc (name 0x906ff,
    factory branch 0x1f8ac, class-word store 0x1f947) and its
    +0x3c slot is 0x23c10: the ladder can never be asked. The
    fourteen ASKABLE classes of PR-301 are the per-level lion
    classes - TLewZaria 0x96960 (0x9078b, branch 0x1fef4),
    TLewZariaB 0x97a98, TLewEna2 0x98f34, TLewKadmar2 0x9b4d4,
    TLewKara 0x9e134, TLewTunga1 0x9f668, TLewJunction 0x9f540
    and seven installed by the same construction path - and the
    ZARIA.01 record names its own lion, so `askable` means
    "is a lion". The ladder's ask verb is 0x97cdc+0x78 =
    0x3f1c0 (the port's ladder_answer_for); the lion's is
    0x96960+0x78 = 0x50f80, UNREAD, and that is now the single
    named step between this row and the climb: whichever of
    those two answers with AC_DRAB is the ZARIA.01 lift.
    PR-304 ANSWERS THAT QUESTION AND SETTLES THE SLOT QUESTION
    TOO. It is the LADDER'S verb that answers AC_DRAB: 0x3f1c0
    tests the asker's own point (handed back by the asker's
    +0x68 verb) first against the edge band [self+0xc5] (a hit
    sets bit 8 = AC_EDGE, 0x3f24d) and on a MISS does NOT exit:
    0x3f235 falls through to 0x3f256, which tests the authored
    rectangle [self+0xb1..bd] and then its 70% core - outside
    the core sets bit 10 = AC_DRAB at 0x3f355, inside it merges
    AC_DRAB with AC_DRAB_CATCH (0x3f2dc/0x3f304 through
    0x49580). That is this port's ladder_answer_core P1/P2/P3
    line for line, so the model needs no change and the
    "(counted)" hedge on the state-1 arm is gone from the code
    comment. The ask is gated on the ASKER's +0x3c (0x3f1cc),
    which PR-303 measured as "is a lion": that is the scope
    PR-235 left unknown.
    MEASURED BY POINTER OCCURRENCE, 0x3f1c0 SITS AT +0x78. The
    little-endian dword occurs exactly once in the whole image,
    in the cell 0x97cdc+0x78, while every ladder-family class
    holds 0x23930 at +0xac; 0x3f0f0 occurs exactly twice, in
    the cells 0x97cdc+0x18 and 0xa1dfc+0x18. So PR-291's "it
    is the object class's +0xac ENTER verb 0x3f1c0", repeated
    above in this row, is wrong about the SLOT, and its reading
    of the index-1 arm - it raises bit 54 = K_GEN1 on the
    ladder itself - is confirmed unchanged. (A class-word walk
    that reads past a table's end finds nine "owners" of
    0x3f0f0; the occurrence count is the measure.)
    THE LADDER CLASS CARRIES NO MOTION, WHICH MOVES THE LEAD
    OUT OF THIS ROW. Only the rope class 0xa1dfc holds a path
    positioner and its timer: +0x1ac = 0x3efe0, which walks a
    13-byte-point array indexed through [obj+0x44] (0x3ee80,
    `imul ...,0xd`), and +0x21c/+0x228 = the 5-tick counter at
    the class-scoped word [obj+0x50]. The ladder's own cells
    there are a draw method (0x400b0) and a store (0x3d1e0),
    and nothing in 0x97cdc indexes a path array - the same
    answer PR-294's 11 invariant frames gave from the art side.
    So the lift is not the ladder moving, and what is left for
    this row is the lion side: its climb rows and its landing
    on the platform line 518 that puts it at 489.
    PR-305 NAMES THE TWO FIELDS THIS ROW'S ASK PATH USES, AND
    THE PORT'S DIFFERENCE WITH THEM. The ask arms do not read
    an object directly: they call through two accessors. +0x4c
    is 0x23a20 = `add eax,0x75`, the object's OWN rect, in
    every class sampled (the exception found is WSPRECT
    0x99ff0, whose +0x4c is 0x23bd0 = `mov eax,0xcf03c` and
    hands a global rect). +0x68 splits by family: 0x23900 =
    `add eax,0xd6` for the eight lion tables and DOMEK,
    0x23b60 = `ret` (the object itself, so its x,y at
    [obj+0x00]/[obj+0x04]) for the ladder, rope, KDRABINA,
    KATAPULT, PALMA and WSPRECT. The test verb 0x23e00 reads
    the point at [p+0]/[p+4] and the rect at [r+0],[r+4],
    [r+8],[r+0xc] with INCLUSIVE bounds. So the ladder's
    state-1 arm asks whether its authored contact point lies in
    the asker's rect at [asker+0x75] - which is exactly what
    the port's PR-243 L8 leg does - while its state-2 arm asks
    whether the asker's point at [asker+0xd6] lies in the band
    or the column, and THIS PORT hands it the lion's position
    instead (pt_of, ported/ll_level.c:5503). No instruction
    anywhere writes either field by an offset (a scan of .text
    for +0x75/+0x79/+0x7d/+0x81/+0xd2/+0xd6/+0xda finds zero
    hits), so who fills them - and with what offset from the
    lion's position - is the first named read for this path.
    The +0xac ENTER cell of the whole ladder family is 0x23930
    = `ret`, an empty method (the lions' is 0x546a0), which
    settles PR-291's slot attribution for good: that arm is
    reachable only through an ask.
    PR-306 FIXES THAT NOTE'S ZERO-HITS CLAIM AND NAMES THE
    WRITER. The scan it quoted built its pattern as '0x' plus
    an already-prefixed offset, so it matched nothing. Measured
    again: [reg+0xd6] occurs exactly 3 times in the image
    (0x54e0a, 0x54efa, 0x55078) and [reg+0xda] 3 times
    (0x4d33d, 0x4d36d, 0x54f06), while the box at [obj+0x75]
    is indeed reached only through its two accessors (0x23a20
    and 0x23a50). The point is written by the class's +0x1c
    verb 0x54e00 - the lion base holds it at 0x96890; the
    ladder family's +0x1c is 0x52c70 instead - which copies
    [obj+0x00]/[obj+0x04] into [obj+0xd6]/[obj+0xda] and
    resets a batch of fields with it. So this port's pt_of is
    right about the VALUE and wrong about the MOMENT: the
    engine asks with the position as of the last +0x1c run.
    The same note's move half: the +0x58 verb 0x54eb0 writes
    the position as target - [obj+0xd6] + [obj+0x89]/[obj+0x8d]
    (0x54f18/0x54f20), so an authored move is measured from the
    stored point, and the frame step's +0x54 verb 0x55070 hands
    the point's address to the class's +0xb0 verb.
    PR-307 ANSWERS THAT LAST HALF: +0x1c IS A NOTIFY/SELECT
    VERB, NOT A CTOR. 108 class tables hold the base reset
    0x54e00 directly (the lion base 0x96874 at 0x96890); the
    fourteen per-level lion classes hold a wrapper 0x50380
    instead (TLewZaria's cell 0x9697c), which first copies the
    position to [obj+0x14]/[obj+0x18] and then calls it; 0x3dd20
    (30 tables) chains it and clears the walk-state words from
    [obj+0x112]; 0x1ee00 (one cell, 0x96ff4) clears [obj+0x128].
    The caller found is the layer-selection helper 0x564a0: it
    asks the layer's +0x08 verb for a count, walks [[layer]+0x40]
    comparing each object's +0x08 result, and notifies the
    winner through +0x1c (0x564f0). The two calls that sit near
    a state change (0x535ea, before the [obj+0x5c] write at
    0x535ed, and 0x539fb) are the RECORD class's verb at
    [rec+0x44], not the object's. So the point is refreshed on
    a notify/select and by nothing else, and this port samples
    the live position instead - the divergence is bounded and
    measurable, and the next named question is who calls
    0x564a0 and whether the ZARIA.01 climb ever does.
    PR-308 ENDS THIS THREAD WITH A NEGATIVE FOR THE PORT. The
    ask protocol does not read [obj+0xd6] at all: it calls the
    class's +0xb0 verb, whose lion handler 0x544f0 (the cell of
    0x96874 and of TLewZaria 0x96960) opens by copying
    [obj+0x00]/[obj+0x04] - the LIVE position - into the
    asker's buffer. Three ask-side callers 0x2b881/0x2b900/
    0x4552f pre-seed that buffer as {0, 0, 0x98278} - the rect
    class word of PR-305 - and 0x3e9ba passes the object
    itself; a zero return makes them bail. The ladder family
    hands back a global instead (DRABINKA's cell is 0x232e0 =
    `mov eax,ds:0xcee8c; ret`), and its frame step is a
    different verb (0x53df0), so it never runs 0x55070;
    WSPRECT's handler 0x52a50 compares the point of
    [[ds:0xcef40]+0x394] with the asker's. So this port's
    pt_of is right about the value AND the moment for a lion
    asker: no stale-point effect can explain the climb stopping
    at 560, and FOCUS 1's route is the one PR-304 named - the
    lion's climb rows and the landing on the platform line 518.
    PR-309 CLOSES THAT ROUTE'S LAST HOP FROM THE AUTHORED SIDE
    AND MOVES THE QUESTION. Across the 777 extracted .MOV texts
    (18,617 frame rows, the same count the port's own report
    cites) the `m dx,dy` row tail occurs 237 times and its
    values are step sizes: `m 0,2` x66 and `m 0,-2` x65 (the
    TUNGA.11 MOVEPLAT platforms), `m 0,1` x39, `m -1,0` x8, and
    larger pairs only in unrelated states. ZARIA.01's own
    A_SCIAG authors `lock` then six rows of `m 0,1` then
    `unlock` - the descent is a row-level 1 px step, and this
    port models `m`, so it runs - while A_WSP, A_WSP_LIN and
    A_PODCIAG carry no motion word at all, so PR-286's "no
    state in the family rises 71 px" holds with the tails
    included. ZARIA.01's DRAB.MOV declares `control=BY_TIME`
    and NO SOLID-family word (257 of the 777 scripts do declare
    one), so the ladder's non-solidity is the script's own
    silence and PR-294's "treat the column as solid" option is
    an authored negative. And the level's OWN end is now read:
    ZAR_LEW.MOV declares 37 states (A_WYJSCIE at index 33), has
    no `end_level` row, and reaches A_WYJSCIE only through
    A_WPAD's `A_WYJSCIE(1)=AC_BREAK_NEXT` - A_WPAD (`seq=wet`)
    itself entered by `AC_ONAIR,AC_DRECT` - whose rows are
    `lock`, `ldir`, `pos 1020,692`, `wait AC_TIME` and then the
    reverse-numbered walk-off. `pos` exists exactly TWICE in the
    whole shipped set, both in the two ZARIA.01 lion scripts,
    both in that state, both with the same pair - and this
    port counted `pos`, `lock`, `unlock`, `ldir`, `rdir` and
    `goto` without acting on any of them (AS OF PR-317/PR-321
    that is obsolete except for `goto`: `pos` is modelled, and
    so are the four word tails). What this row needs next is
    therefore not another ask-path read: it is the apply of
    those six directives, the AC_ONAIR/AC_DRECT raisers, the
    water-as-floor question (OI-082, since the exit point
    1020,692 sits below the main ground line), and the seven
    ZARIA_A.PLT "moving ground" polylines.
    PR-311 CHANGES WHAT "MOVING GROUND" MEANS AND MEASURES AWAY
    A HUNCH. The ten .PLT records in ETAPY hold 458 polyline
    lines (ZARIA_A 45, ZARIA_B 25, RAN 153, KADMAR2 114, ...);
    parsed with whitespace and the `!` separator allowed, EVERY
    line's leading number equals its point count minus one - it
    is the SEGMENT COUNT, as ll_plt_parse's own comment says, so
    nothing was read as a line-type word and no ground-line kind
    is missing from the port. The polyline channel is real but is
    NOT in .PLT: it is in the `ani_plat` placement files, and
    each entry's polyline points are RELATIVE to the placement
    point. ZARIA.01's ANIPL2.DES is four entries: the KATAPULT at
    (300,588) - directly under the x span of the platform line the
    climb was aimed at - plus three PALMA, and only the catapult's
    line carries the trailing pair `-6300,-1350`. Its class
    0x96d1c is in the same family as the ladder's (0x23c10
    never-askable, 0x23a20 own rect, 0x23b60 self, frame step
    0x53df0, empty +0xac stub) but its ask verb +0x78 is its own
    0x1e970: `add eax,0xa5` then 0x45320 with ebx=2, then the
    empty set (mode byte cleared, PR-296's n=0xc composer) - it
    registers the asker in its contact list and raises NO signal on
    any state. Its kat.mov declares one state A_RUN, `1,0,WAIT
    K_GEN1` then ten frames, with no transitions at all. So
    ZARIA.01 has exactly two answerers, the ladder and this one,
    and the catapult's effect on a rider can only be in 0x45320's
    list insert with mode 2, in the `-6300,-1350` pair, or in its
    own seq=nr walk - three named reads, none of them the climb
    states.
    PR-312 ANSWERS THE "WHAT COULD MOVE A RIDER" QUESTION, AND
    MOVES THE NEXT READ. The class slot +0xac, which PR-305 read
    as "ENTER", is the per-frame PHYSICS STEP: 108 of the 151
    census classes hold 0x546a0 there - every lion class
    (0x96874, TLewZaria) plus PLATF, WINDA, SCHODY and the
    enemies - while the whole ladder family holds the empty stub
    0x23930 and the WSPRECT family 0x52a00. Its body: skip the
    queries if [obj+0xb6] bit 2 is set (the carry still runs);
    read the current state's control word at state+0x12 - bit
    0x4000 runs 0x4b600 with mode 0 then 0x4b430, bit 0x0100 runs
    0x4b600 with mode 2, 0x4b430 and 0x4b7f0 - all against the
    world object ds:0xcef40, and 0x49580 ORs each result's three
    dwords into the caller's signal set, so a GROUND QUERY RAISES
    SIGNALS. The result travels in a triple: [obj+0xe6] the
    surface, [obj+0xe2] its flag byte, [obj+0xea] the signed
    clearance, with [obj+0xa9] the attached one. The attach law
    calls the surface's class [+0x0]/[+0x4] enter/leave verbs and
    the object's own [+0xb4]/[+0xb8], and raises the signal bits
    0x10 (landed), 0x20 (kind != 0), 4 (airborne); a kind-2
    surface is ignored while [obj+0xc6] < 0x58c. Then the carry:
    pos.x += [obj+0xca], pos.y += [obj+0xce] + [obj+0xea], then
    all three are zeroed - a rider is moved by the surface's frame
    delta and by a SNAP to a polyline. And the surface may come
    from an ask: 0x45320 (the contact-list insert PR-311 named)
    gets the point through the +0xb0 provider, adds the pending
    delta, and 0x45400 treats the list as a polyline of 13-byte
    records (x, y, flag byte at +0xc) - first-entry flag vs the
    mode, segment lookup 0x45380, interpolated clearance 0x455a0 -
    filling the same triple. So PR-294's "treat the column as
    solid" was the wrong shape of question: solidity in this
    engine is a per-point flag byte and a query MODE. Next read,
    in order: 0x4b430/0x4b600/0x4b7f0 (which level polylines the
    queries walk and how mode and the flag byte filter them - the
    direct route to the ZARIA.01 platform line), then the
    `control=` meaning of bits 0x4000/0x0100, then whether those
    queries raise the AC_ONAIR/AC_DRECT pair this row's exit chain
    waits for.
    PR-313 READS THOSE THREE QUERIES. 0x4b430 is a BLOCKADE test:
    it takes the object's rect, adds the pending delta, and walks
    an array (base [world+0x70], count +0x7c, stride 0x14, entry
    x at +0x4, y at +0x8, length at +0x10) through 0x45620 - a
    vertical barrier inside the rect is answered by adding
    (entry.x - the farther edge) into [obj+0xca], a sideways push
    the carry then applies - and raises AC_BLOK. 0x4b600 is the
    ask sweep: the point through the +0xb0 provider, the attached
    surface first through its class +0x8 verb with mode 2, then
    every object in [world+0x74] (count +0x80, stride 0xc)
    through its class +0x1c verb. 0x4b7f0 asks the objects of a
    third array (+0x78, count +0x84) through their +0x4 verb and
    raises AC_UP_DUP; 0x4b6d0/0x4b762 are the nearest-below /
    nearest-above searches over the second array using the entry's
    +0x14 verb. The ZARIA.01 route stays the same: which arrays
    the level builds (0x44370 / 0x44540, not yet read) and what
    the queried objects answer.
    PR-314 NAMES THE FIRST OF THOSE ARRAYS AND CLOSES A BRANCH.
    ZAR.BLK is 76 bytes: `6` then six `x,y,len` walls (the level
    edges at x 0 and 2240 spanning y 0..700, and four interior
    walls at x 1299..1620) - the triple exactly matches the entry
    0x45620 reads (x at +0x4, y at +0x8, len at +0x10), so
    [world+0x70] is the `blokady=` array; ZARIA.01 asks for
    `blokady = zar.blk` and `sufity = none.des` (no ceilings).
    No wall is anywhere near the ladder column x 190..233, and the
    level places NO carrier object (no WODA, STRUMIEN, KRY, PLATF,
    WINDA), so no object carries the rider at the ladder: the only
    floor-like data there is zaria_a.plt. The port parses the key
    and models nothing from it - AC_BLOK is a table entry with no
    raising site, and the sideways push into [obj+0xca] has no
    counterpart. Also a limit on PR-312: +0xac is family-specific
    (0x52a00 for the WSPRECT family is |dx|+|dy|, 0x35e20 for
    KTRASA an accessor, 0x23930 a no-op held by the ladder family
    and by WODA).
    PR-315 ATTEMPTS PLAN STEP F1 AND REPORTS A METHOD NEGATIVE.
    A colour-band read of R2 t=45.5-48.9 and levels/ZARIA.01_b
    t=199.9-200.8 (game resolution, the R3 crop recipe) finds one
    band 25-35 px wide and 56-65 px tall that translates UP about
    60 px in 2.1 s in R2 (top 126->63, bottom 182->120 - the
    whole structure, not a rider sliding along a fixed rope) and
    DOWN about 42 px in 0.93 s in the level cut, at 0.5-1.4 px
    per game frame - the row-step magnitudes, not a fall. But the
    fixed-looking band at x 6..31 y 159..197 is HUD (it is there
    in TUNGA.11 too), and the ladder region contains two oranges
    35 apart in one channel, (253,94,0) and (250,129,0), so the
    lion and the ladder cannot be separated and screen motion
    cannot be turned into level motion. F1's three questions need
    SHAPE templates: tools/seq_render.py already returns a
    touched-pixel mask per .SEQ frame - that is F1b. And the
    authored side is now counted: only three ladder scripts exist
    (ZARIA.01 DRAB.MOV, ZARIA.02 DRAB.MOV and DRABK.MOV) and not
    one of them has an `m` row.
    PR-316 READS THE EXIT'S OWN MECHANISM, WHICH IS THE SHORTEST
    ROUTE TO A GREEN ZARIA.01. The directive parser's node classes
    (0xa11b0 pos, 0xa10d8 lock, 0xa0f70 rdir, 0xa10f0, and
    0xa1048 for end_level) differ at slots +0x4 and +0x1c, and
    each +0x4 body starts with the shared helper 0x457e0, which
    calls the OWNING object's class +0x4 verb - a directive is
    bound to an object, not to the level. `pos x,y` (parsed via
    the format at 0x91944 into [node+0x8]/[node+0xc]) calls the
    target's class +0x4 verb, which for the lion classes is
    0x23850: `mov [eax],edx; mov [eax+0x4],ebx; call
    [class+0x54]` - write the position, then run the row verb. No
    carry, no clearance, no signal: it bypasses the physics step
    of PR-312 entirely. `lock`/`unlock` are the pair
    0x54f40/0x54f90 guarded by the class +0xbc verb and the
    [obj+0xb6] bit-1 flag (the physics skip is bit 2), and `rdir`
    writes [obj+0x6d] = 1. So a port step exists that needs no
    physics at all: apply the authored `pos 1020,692` of
    A_WYJSCIE.
    PR-317 SHIPS THAT APPLY. `pos x,y` is parsed into
    ll_mov_fr.has_pos/posx/posy and applied at the row admit as an
    ABSOLUTE set of p->x/p->y - no velocity term, no clearance, no
    signal, no ground query: the engine's setter 0x23850 for the
    lion classes is four instructions and then the +0x54 refresh,
    0x55070, which re-derives the presented point [obj+0xd6]
    through the class +0xb0 provider, the ask path of PR-313. The
    shipped census is 4 rows in 3 files and BOTH spellings: the
    authored exit pair (`pos`, lowercase, in ZAR_LEW and ZAR_LEWPL
    `[A_WYJSCIE]` row 3) and RAN.01's death teleports (`POS
    220,140` in `[A_DEAD]`, `POS 200,140` in `[A_DEAD1]`). Red
    first inside `--play-selftest`, with two mutation controls
    (parse off = 5 checks fail, apply off = the 3 runtime ones),
    and the timing derived from PR-110's row clock: a 250-unit row
    holds three feeds, so the set lands on the fourth iteration and
    re-applies on every later admit, the list wrap included. The
    chain convention is also corrected against PR-316: 0x457e0
    walks the node's [node+0x0] PARENT link into ITS +0x4 verb,
    and the acted-on object always arrives as the second argument
    ([[ctx]+0x0] = the object, its [0x8] = the class word). And
    the exit is now read as DATA: `zaria_a.plt` runs
    (953,654)(994,644)(1007,646)(1051,633) and then NOTHING until
    (1115,662), so the authored `pos 1020,692` is 39-59 px BELOW
    the terrain, inside a 64 px hole, with only the catch-all line
    (-100,1500)-(3000,1500) under it. The chain is still not
    walkable: A_WYJSCIE needs AC_BREAK_NEXT from A_WPAD and
    A_WPAD needs AC_ONAIR,AC_DRECT from A_PRE_JUMP, and AC_DRECT
    has no raiser (F4, and OI-018); both exit states also author
    `lock`/`unlock` on their first and last rows, so the walk-off
    needs PR-316's pair too.
    PR-321 CLOSES THE DIRECTIVE HALF of that list: `pos` (PR-317)
    plus `lock`/`unlock` and `ldir`/`rdir` are now modelled -
    the lock freezes the position store for the whole locked
    window (value 1 of [obj+0xb6], 0x54f40/0x54f7b, with the
    anchor branch 0x54ee0-0x54f20 PR-320 read), the words reset
    at state entry (0x54e00, 0x50442), and the FC/pull x sides
    scale by the authored word (0x54340). What this row still
    needs is the AC_ONAIR raise path on the hole's fall
    (PR-318 named the raisers, the port does not raise them
    yet) and the ly_exit leg end to end.
    PR-325 CLOSES THE GROUND-TRUTH HALF of F1 for this row:
    ZARIA.01's ladder is a route DOWN. The object script is a
    chain, not a static prop - A_UP holds art frame 1 behind
    `1,0,WAIT K_GEN1`, K_GEN1 (the lion's own rows, PR-257)
    takes it to A_DROP, which walks art frames 2..10 with row
    thresholds 500/400/400/300/300/200/200/200/200 and NO m
    row, so the DRAWN ladder grows 48 px to 149 px (PR-294's
    560 -> 680 boxes are those frames' own bottoms), then
    A_DOWN (index 2, the only state that answers the lion)
    holds frame 11 at 168 px behind `WAIT K_NEVER`. The authored
    contact point is (213,507), five rows above the shape top
    512, and the authored rectangle runs through the platform
    hole ANI1.DES authors as `PICT,plan3.seq,180,515`. The
    player's press is `A_SCIAG(1)=AC_EDGE,K_DOWN`; no state of
    the lion's script rises (the only negative dy in the file
    is A_KILOF's -80). What this row still needs: the same
    AC_ONAIR raise path and the ly_exit leg; plus, from
    PR-325's OPENS, which mechanism carries a descent at about
    1.7 screen rows per video frame - A_SCIAG's six px per
    24-iteration pass or A_DROP's 4 px per iteration unroll -
    a question the captures cannot settle (the camera-y is
    OI-071).
    PR-326 REMOVES THE LAST UNKNOWN ON THE EXIT LEG. SEVEN states of
    the ZARIA.01 lion script - A_JUMP_S, A_JUMP, A_FALL_DOWN, A_WALK,
    A_TURN, A_PRE_JUMP, A_DRAP - carry the SAME transition
    `A_WPAD(1)=AC_ONAIR,AC_DRECT`; A_WPAD's list end feeds
    AC_BREAK_NEXT into A_WYJSCIE, whose rows are `lock`, `ldir`,
    `pos 1020,692`, `wait AC_TIME`. OI-119's answer makes the second
    term checkable: AC_DRECT is raised by the DRECT ask verb 0x40d50
    only when the asker's own velocity y is positive (falling, PR-99's
    sign) inside the authored box, and ZARIA.01 ANI2.DES line 18
    authors the shipped set's only DRECT, (988,645)-(1340,750), over
    the exit point and the three ZWODOSPD platforms. The remaining
    gap was a PORT gap with no unknown in it: ll_level.c counted
    that line as polyline data (st->poly), so no object with an
    authored box existed for the ask walk. PR-328 closes THAT half:
    the token names the class and the four numbers are the box at
    [obj+0x3c], absolute and unsorted as 0x40c90 writes them, so
    ZARIA.01's DRECT, RECT and KILOF_PL now stand in the port's
    scene, and PR-329 runs their +0x78 answer verbs from the
    requester's ask walk: AC_DRECT, AC_DRECT2, AC_RECT and
    AC_KILOF are raised with the exact gates of 0x40d50/0x40dc0/
    0x40cf0/0x1e3a0/0x286a0/0x28710. What is left for this row
    is the AC_ONAIR term, which still
    needs the surface-link half (the pass at 0x5476x-0x54800:
    AC_DUP on attach, AC_DUP_CD by surface kind, AC_ONAIR when
    [obj+0xa9] is dropped), named in OI-119's closed note.
    PR-333 makes that signal-set lifetime load-bearing for the ROW
    step and names what the port does with it: the player's row pass
    asks ll_mov_row_admit with `row_live`, the set the transition
    pass of the SAME iteration matched with, captured in
    player_iterate just before its `p->sig = 0`. The engine has one
    blob to consult - 0x52255-0x522a6 merges the caller's keys with
    [obj+0x60], the 12 bytes PR-260 read as never lowered - while
    this port keeps a per-tick set, so without that snapshot an
    edge-only signal is invisible to the row that needs it: ZARIA.01's
    A_JUMP row 10 `8,0,WAIT AC_DUP` is exactly such a row, AC_DUP
    existing only for the iteration of the surface attach (PR-331),
    and the row pass runs AFTER the clear. The unread half is
    unchanged - the engine's clear sites are the ctor/allocator paths
    only - so the per-tick clear and this snapshot of it are both the
    named deviation here, and the A_JUMP, A_FALL_DOWN, A_LOOKDOWN and
    A_DOSTAL holds the row gates produce are the first legs whose
    pins depend on it.
    PR-392 CORRECTS ONE POLARITY AND ONE NAME IN THIS ROW. The gate at
    0x5476f is `cmp [obj+0xc6],0x58c; jge 0x547d7`: a flag-2 answer is
    dropped AT OR ABOVE that v_y, not "while [obj+0xc6] < 0x58c" as the
    paragraph above has it (OI-018's PR-331 wording is the correct one).
    The "kind" of that answer and of each record is the authored
    separator byte of PR-391, gated against the mode as the first
    instruction of both 0x45400 (0x45417) and 0x45250 (0x45258); and
    0x4b600 walks the level's own ground array with one
    `call [element class + 0x1c]` per element (0x4b67e-0x4b6a8), which is
    the walk this row's ground-query description assumed but had not read.
    PR-394 READS THE TWO SIBLING QUERIES THIS ROW SENT NEXT, AND FIXES
    THE BLOCK RECORD'S SHAPE. 0x4b430 builds a 20-byte request block
    (0x4b441-0x4b455, [0x10] = the view 0x98274), fills it from the
    ENTRY through 0x534e0, shifts both its x fields by [obj+0xca] and
    both its extent fields by [obj+0xce] (0x4b462-0x4b48c), and asks
    every entry of [world+0x70] at stride 0x14 through the DIRECT call
    0x4b4aa to 0x45620 - no verb. 0x45620 treats the entry as a
    vertical barrier: its x at [entry+4] must lie between [block+0] and
    [block+8] and its y at [entry+8] between [block+4] and
    [block+4]+[block+0xc]; the push then moves [obj+0xca] by the signed
    distance to the NEARER edge ([block+8]+1 or [block+0]-1), refused
    when that distance is <= 1 (0x45677-0x45690) - the same field the
    position carry at 0x54825 applies, which is why a block answer
    slides a body instead of stopping it. On a hit 0x4b4ec sets bit 0 =
    AC_BLOK in the result block, and 0x4b7f0's is 0x4b844 `or BYTE
    [result+3],0x2` = AC_UP_DUP, index 25 of the port's table - the
    same first dword, a different bit. The record's third word is NOT a
    length to the engine: 0x4db50 copies the file's three numbers
    verbatim into [0x00], [0x04], [0x08] (via 0x455e0) and 0x45620 uses
    [0x08] as the absolute RIGHT EDGE. ZAR.BLK (dumped to
    /tmp/opencode/pr394_zar.blk from the HD ETAPY, sha be5d8b92; 10
    `.BLK` members there, none in the CD-stage container) authors
    `2240,0,700`, which under that reading spans x 2240..2940 - past
    the world edge. Either the loader completes it elsewhere or PR-313's
    and PR-314's `len` naming was wrong; named open. The ladder itself
    is unchanged, and both branches of 0x546c5 still end at the shared
    0x4b7f0 + 0x49580 tail (0x54704 `jmp 0x54746`), the second one with
    a pre-filled [esp+0x10] buffer.
    PR-395 RE-READS THOSE BYTES AND CORRECTS THE PARAGRAPH ABOVE.
    The record's three numbers are PARSED, not copied: 0x455e0 hands
    rec+4 and rec+0x10 to 0x4a0b0, which scans the format "%d,%d"
    (0x91d0f) into rec+4 and rec+8, and 0x455e0 then scans "%d"
    (0x9183d) into rec+0x10 and probes a "," (0x91840); rec+0 and
    rec+0xc stay zero from the 0x4dbb9 allocation and 0x45620 never
    reads them. The third file word IS a length, at +0x10, and
    `2240,0,700` is the wall x 2240 spanning y 0..700 - the level's
    right edge - not x 2240..2940: PR-313's shape was right and
    PR-394's "right edge at +0xc" was not. The y test is an OVERLAP
    of the wall's [rec+8, rec+8+rec+0x10] with the requester's
    [block+4, block+0xc] (0x4563a-0x4565a), not a point test. The
    side test is ([rec+4]-[block+0]) >= ([block+8]-[rec+4]) with the
    tie taking the far edge (0x4566a), and the distance is measured
    from the wall's x to the REQUESTER's own edge plus one; 0x45683
    refuses at <= 1 and on a refusal nothing writes [obj+0xca] at
    all. And the walk does NOT ask every entry - 0x4b4b1
    `jne 0x4b4d6` leaves AT the first record that answers, and
    AC_BLOK is raised once. A blockade therefore TELEPORTS a body
    out of an overlapped wall through [obj+0xca]; it never stops it.
    All ten `.BLK` members of the HD ETAPY carry exactly 1+3*count
    integers (94 records over 10 files, max header 49) and their
    walls span each whole world - RAN.BLK even authors one at x -1,
    outside it. The port's model lives in ported/ll_play.c
    (ll_blkreg_parse, ll_block_hit, ll_block_walk), pinned in the
    data-free --play-selftest leg.
    PR-396 READS THE CEILING QUERY THIS ROW SENT NEXT, AND FOR THIS
    CLIMB THE ANSWER IS NEGATIVE. 0x4b7f0 walks [world+0x78] for
    [world+0x84] records at stride 0x14 and asks each through
    `[[entry+0x10]+4]` = 0x45500, where [entry+0x10] is the record's OWN
    method table 0xa218c, written by its constructor 0x4fbd0 (the class
    record 0xa14d8 keeps its fields at C+1/C+5/C+9/C+0xd, and 0x76ab0 is
    what runs that constructor over the array). 0x45500 is given the
    BODY, not a box: it takes the body box through the class slot +0x68
    (0x23900 = `add eax,0xd6`, the stored presented point of PR-305 and
    PR-306) and keeps only its near y, and takes two numbers from slot
    +0xb0 (0x544f0, the LIVE position, PR-308) and
    ADDS [obj+0xca] and [obj+0xce] to them (0x4553c-0x4554f) - the
    anchor where the body would be once the pending move is applied. A
    record answers when that anchor's x lies in [entry+0, entry+0 +
    entry+0xc] inclusive at both ends (0x45555, 0x45562), the body's top
    is at or below the record's y (0x4556c `cmp [esi+4], [entry+4] /
    jl`), and the anchor is STRICTLY above it (0x45576 `sub` / 0x4557a
    `test, jle`); 0x45582-0x45584 then add entry.y - anchor_y to
    [obj+0xce], which lands the anchor on the line. A ceiling therefore
    pulls a body DOWN to a line it is sticking above - no flush refusal
    like 0x45683's, and no upward move - and the walk stops at the first
    answer (0x4b82d). The record's three words are x, y and LENGTH ALONG
    x at +0, +4, +0xc: a ceiling is a horizontal line. For this climb it
    is settled, not merely absent: ZARIA.01
    and ZARIA.02 each ship a NONE.DES member in their own level slot
    whose whole content is the token `0`, so 0x4b808 skips their walk and
    the level AUTHORS no ceiling. The shipped picture, in full, is 13
    registries and every one of them exists - all 52 `.DES` members of
    LEVELDES (13 level descriptors in the ENG, GER, POL and POL.ORG
    language slots) carry one `sufity=` line, and the provider it names
    is a member of the level's own container slot: kadmar.suf
    `0,0,3000`, kara.suf `0,0,1600`/`125,290,10`/`1535,40,10`,
    ran.suf `20,100,28800`, ENA.03's SUFITY.DES `0,20,6720`, SADES.06's
    six rows, TUNGA2.12's four, and seven levels' NONE.DES holding `0`
    (census: /tmp/opencode/pr396_suf_des.txt). PR-396's first reading of
    this - that ten of the thirteen names point at a file nobody ships -
    was wrong, and the record corrects it.
    The port's model of the query lives in
    ported/ll_play.c (ll_sufreg_parse, ll_ceiling_hit, ll_ceiling_walk),
    pinned in the same data-free leg.
OI-079: AC_DUP: the engine's attach machine is named (PR-179) -
  model it? what ends a JUMP early (does the original cancel
  recovery on the next input?)
  status: OPEN
  origin: PR-86 item 8.8, PR-150's ladder-overlap model, PR-179
  notes: PR-313 found the PRODUCER: the physics step 0x546a0 (the
    +0xac verb, installed in 108 of the 151 classes) raises
    AC_DUP when it attaches to a surface, AC_DUP_CD when that
    surface's kind byte is non-zero, and AC_ONAIR when no surface
    is found; the attach also calls the surface's class +0x0
    enter / +0x4 leave verbs and the object's own +0xb4/+0xb8.
    Modelling that machine stays open; what is settled is that no
    row-tail directive raises AC_DUP.

OI-080: SURPRISE '?' firing (G-12); the factory branch lead is
  0x4e62f
  status: OPEN
  origin: PR-131/G-12; PR-149 named the route

OI-081: the level-end scene (purple-robed NPC + bird + ball,
  levels/ZARIA.01.mkv t=440-457) and the mid-level bird sequences
  (zax37 t=306-315, 385-413): no placement line names them and the
  ZARIA.01 ANIMACJE tree has NO bird leaf - scripted chain reusing
  other levels' leaves, or engine code: find which
  status: OPEN
  origin: G-10/G-11

OI-082: the water surface as a floor (the lion walks WODA at pool
  level; zax37 t=305-307)
  status: CLOSED BY PR-320 (NEGATIVE, with the mechanism)
  origin: G-7
  notes: PR-314 is a measured NEGATIVE for ZARIA.01: the level
    places no WODA object at all (its own placements are DRABINKA,
    two LINA, the KATAPULT, three PALMA, SEP2, POCISK, PICT and
    NOSOROG), and WODA's class 0x96f20 holds the no-op 0x23930 at
    +0xac - the water runs no physics. The floor-like data in that
    level is only zaria_a.plt's polylines, the six ZAR.BLK walls
    and the objects' answer verbs. The row stays open for the
    levels that DO place WODA (the zax37 clip this row came from).
    [CORRECTED BY PR-318 ON THE PLACEMENT HALF.] ZARIA.01 DOES
    place WODA: its own ANI2.DES line 29 is
    `WODA,woda.ani,woda.mov,955,651`, and the HD install carries
    the script it names (the ANIMACJE leaf ZARIA.01/WODA, with
    WODA.MOV, 174 bytes) - the placement file's inventory PR-314
    listed was incomplete, and this port's own reader already
    parses that line as an entry and resolves that script. The
    other half of the paragraph stands: WODA's class holds the
    no-op 0x23930 at +0xac, so the water runs no physics, and the
    floor question is still open - it is now a question about an
    object the level really places, not about a missing one.
    PR-320 CLOSES IT AS A NEGATIVE. (1) WODA's whole authored
    script (ZARIA.01_WODA_WODA.MOV) is two states, A_RUN/
    A_RUN_AF, `seq=wod1`/`seq=wod2`, no transition, no motion
    row, no SOLID/SEMI_SOLID/ANTI_SOLID word, and ZARIA.01's 32
    .MOV texts spell those words only in KAMIEN1/2/3, LICHA3,
    PAJAKZ and NOS. (2) Its class 0x96f20's +0x8 verb is the
    authored-rectangle test 0x23e00 (geometry answer only), its
    +0x4 verb 0x23ea0 only sets a flag at [obj+0xb6], its +0x10
    verb 0x1ec90 chains to 0x53770 - and its +0xac physics verb
    is the no-op. ZWODOSPD - the three `ZWODOSPD,992,610,
    wodospad.grp` lines of ANI1.DES - is worse: a 4-field line
    with no script at all, made at 0x1fb32 with primary class
    0x96cf0 and the subobject table 0x98278 at [obj+0x8], whose
    only verbs are the WODA flag-setter 0x23ea0 and the
    authored-rectangle test 0x23e00, and whose primary class's
    +0xac is the generic chain verb 0x531b0 with no authored state
    to run in it. Neither can
    be a contact surface, and the PR-60 contact gate would have
    nothing to answer with. (3) The exit chain works BECAUSE the
    ground fails: PR-317's 64 px hole is the mechanism,
    AC_ONAIR (0x54800) plus the AC_DRECT gate fire on the fall,
    and the pool is `seq=wet` art plus the `lock` anchor-freeze
    of PR-320(c) - not a floor. The zax37 "walks on water"
    observation is the same class of picture in another level;
    if a future clip work needs it, that is video work, not
    this mechanism question.

OI-083: HUD display truth: both R3 clips start at 5 lives (the
  port prints a hard-coded 6) and the HUD energy at level start is
  unknown - both are video-gated checks
  status: OPEN
  origin: G-8; PR-186 item (10)
  notes: the lives half resolves through OI-066 (STATUS.LEO) rather
    than a HUD read.

OI-084: is the rounded cave picture the WHOLE Mode X frame or a
  letterboxed copy? It decides what "the same picture" means for
  every movie comparison
  status: OPEN
  origin: HANDOFF 7 item 14 (found measuring for PR-111; frame
    /tmp/opencode/refcap/ep1_120.png, never in git; the PR-55/56
    decoder + PR-19/21 capture rig could answer it)

OI-085: the SEP life cycle (G-6: fight burst at t=299.5, kill not
  reproduced) + SEP2's missing script + the ZARROW/SEP semantics
  from the carried lists (G-4's script half)
  status: OPEN
  origin: PR-131 audit (G-4/G-6 exist only in that record); the
    jajo child-spawn part of G-4 is under OI-039

OI-086: spawn-state rule: does the original force standing, or
  start at the `.MOV`-declared first name (RAN.01 A_FLY_PTAKI1)?
  The port ships the declared-name spawn
  status: OPEN
  origin: PR-83 item 8.2; settling test = a RAN.01 level-start
    capture

OI-087: K_FWD/K_BKWD facing-relative vs literal: shipped as MODEL
  CHOICE (PR-85), still UNPROVEN on video (settling: facing-left
  lion + RIGHT key on a capture); AC_BREAK_PREV stays unmodeled
  (false)
  status: PARTIAL

OI-088: `I ix,iy` impulses multiplied by walkdir: nothing in the
  original confirms the mirroring
  status: OPEN
  origin: PR-83 item 8.4

OI-089: do F=0 states FREEZE momentum, or does the original coast?
  status: CLOSED BY PR-129/PR-130/PR-133/PR-135
  notes: superseded by the velocity+drag law itself (v decays to
    v_eq; F=0 => v_eq=0). The residual non-modelled halves are
    OI-015. Row kept: PR-83's text cites the question.

OI-090: K_F6..K_F12 stay OTHER (7 uses total); whether the original
  treats F-keys as HOLDED (the port's model) or TOGGLED weapon
  state; what ENDS A_FIGHT early; the comma-group AND as model
  choice
  status: PARTIAL
  origin: PR-87 item 8.6

OI-091: does the corpse (A_DEAD) still deal contact damage? The
  port says no (model choice)
  status: OPEN
  origin: PR-89 item 9.4
  notes: the contact-damage RULE for live objects is closed (G-5 by
    PR-139: authored `damage=` through the ATC window, 30-tick
    countdown); only the corpse case stays.

OI-092: the transition-apply SIGNAL legs stay unread: the
  AC_TIME count arm 0x53626 (the player models it through its
  own frame-list wrap) and the +0xac verb 0x53619-0x53620.
  The row-seek half is CLOSED (objects PR-174, the lion
  PR-215). New named question from PR-215: the engine's
  same-state arm stores the raw -1 of an absent token with no
  negative test (0x53616; the refusal at 0x52381 is the
  cross-state setter), so what a -1 cursor does on the next
  step (wrap 0x521c5) is unread; the player and object paths
  differ there today.
  PR-295 CLOSES THE AC_TIME CLAUSE. The authored `time=lo
  [,hi]` is read into record +0x8/+0xa by the scanner at
  0x51944 (formats `%hi` 0x9211e and `,%hi` 0x92122, the
  keyword `time` at 0x9211b - PR-102 confirmed), and the
  record verb at 0x52072-0x520b7 rolls a gate into record
  +0x0c: it zeroes the owner's counter [owner+0x9d] (0x52077),
  and for hi != 0 computes gate = rand()*(hi-lo)/32767 + lo
  with 0x76bcc = Borland's rand (state*0x41C64E6D+0x3039,
  (state>>16)&0x7FFF); for hi == 0 it stores lo with no roll at
  all, which answers PR-163's open max=min case exactly.
  0x53626-0x53659 then reads [obj+0x9d], increments it and
  stores it back, and raises bit 9 when the (non-zero) gate at
  [rec+0x0c] is <= that counter: AC_TIME is the engine's own
  passes-since-the-state-was-entered timeout, the counter
  cleared at 0x535c2 on a state change and at 0x52077. The same
  pass answers this row's second half in part: 0x53616 is
  reached when [rec+0x04] differs from the caller's argument,
  and 0x53620 then calls the OBJECT class's +0xac verb with
  edx = [obj+0x60] - a third call site of the ENTER verb
  (after 0x535f7 and 0x52416). The -1 cursor's next step is
  still unread, so the row stays open.
  PR-298 ANSWERS THE COMPANION QUESTION - WHO READS THAT
  CURSOR. A strict search for the record-derivation idiom
  `mov r,[o+0x50]` then `mov r,[i+r*4]` finds 83 sites, 79 of
  which read a record field, and 49 compare groups are made on
  field +0x04 alone, with two jump-table dispatches (0x1e6aa
  `sub eax,3; cmp eax,4; ja` + `jmp [eax*4+0x1e670]`, a
  switch over rows 3..7 inside a class verb). The 20 distinct
  constants are 1..7, 10..16, 20, 21, 25, 38, 45, 60 - all
  inside the authored row range (a single shipped state carries
  up to 97 row lines; 20..57 all occur), so the field is the
  ROW INDEX and the engine attaches behaviour to row NUMBERS:
  row 6 answers AC_DRAB into a set (0x287ea/0x28805), row 25
  raises K_GEN1 inline (0x3c31b/0x3c321), a row equals 60
  (0x285c2). The port's ll_mov_tr models a row as frame +
  duration + clauses + directives and nothing else; the
  row-number dispatch half is a systematic gap, not one leg's
  defect.
  status: OPEN
  origin: PR-107/172/174/215

=== port-side + archived-review ids ===============================

OI-093: P-03 single window / single SDL lifecycle, P-04 focus
  stealing
  status: CLOSED BY PR-10/PR-12

OI-094: P-05 Wayland/X11: the automatic pipewire->alsa pin was
  removed; SDL's own choice is the default, LEO_PIN_ALSA=1 opts in
  status: CLOSED BY PR-35

OI-095: E-08/T-14 palette codec fidelity
  status: FROZEN (owner)
  notes: pixel output must not change.

OI-096: WINDOWS PASS: `make -k verify-windows` + one ci-windows
  dispatch + `make win` warning-free, ONCE at the end of
  implementation; do NOT fold them back into a per-PR gate (they
  measured 53.7% of suite time)
  status: DEFERRED (ruled)
  origin: owner ruling 2026-11-19 (HANDOFF section 2 rule 8)

OI-097: T-25 parser fuzzing
  status: CLOSED BY PR-11
  notes: the extension named at close (more kinds (.des text) + a
    longer seed list) is not a row until someone claims it.

OI-098: T-17 remainder: wire a config file to the PR-13 user-dir
  helper once a settings subsystem exists
  status: PARTIAL
  notes: the STATUS.LEO codec itself is done; its 84-byte field map
    is OI-066.

OI-099: T-24 remainder: the golden-master tool + port pins are
  DONE (PR-19/37/40); a same-frame comparison against the ORIGINAL
  engine under QEMU remains unbuilt
  status: PARTIAL

OI-100: the still-untouched items of the archived review list:
  T-03 config knobs cleanup, T-04 globals, T-05/T-06 fixed-step
  timing + precise sleep, T-07 clock ownership, T-16 Polish text,
  T-19 startup data validation, T-23 structured RE traces, T-26
  engine state machine
  status: OPEN
  notes: each origin-id resolves at
    docs/archive/open-leo_native_port_review.md; split this row
    when a first PR claims one item. (T-05/T-07 were noted as
    falling out for free in PR-18 - never verified.)

OI-101: the 'levels' config file loader (the 0x91ee5 error text
  names it)
  status: CLOSED BY PR-193
  notes: closed WITH A CORRECTION - there is no 'levels' file; the
    mechanism is the driver's named-entry lookup (0x5c320(0x3ea) +
    0x57990 + 0x4a4c0), keyed by the stage name.

OI-102: re_index's correction pointers stay prose-co-occurrence
  until records carry a CORRECTS: line the generator can read
  status: CLOSED BY PR-355
  origin: PR-197 OPENS
  notes: records DO carry CORRECTS: lines since PR-198; the
    generator upgrade landed in PR-355. tools/re_db.py parses the
    structured CORRECTS: field into a queryable corrections table
    (147 correction edges over the 340 records) and jq.py's `corr`
    reads it, so the
    correction graph is now read from the field the records already
    carry, not from prose co-occurrence. re_index itself is unchanged.

=== documentation program + tooling ===============================

OI-103: doc step 4: HANDOFF sections 5/6/7 become pointers (this
  register is their target; removes the 41 over-79 table rows)
  status: CLOSED BY PR-202
  notes: measured before doing it: the "41 over-79 table rows" sat
    10/6/25 across HANDOFF sections 1/3/6, not all in section 6 -
    this step removed only its own 30 lines (25 table rows reflowed
    into ESTIMATE blocks, 4 section-5 lines re-wrapped, 1 section-7
    line frozen into the archive); the ratchet dropped 71->41 and
    the 16 section-1/3 rows stay under it.

OI-104: doc step 5: NEXT_STEPS becomes journal-only: the stale
  status block -> a pointer, `## Next` moves to its one home
  (HANDOFF section 8) with doc_census updated in the SAME commit,
  `## Deferred big items` joins THIS register, standing rules join
  the rules
  status: CLOSED BY PR-203
  notes: done as measured: the P2 inversion (journal must carry
    NONE of the four section headings; HANDOFF one-homes pinned to
    exist) was red first on the old file; the live half of
    `## Next` became the section-8 lead, its shipped summaries were
    left to the records (one home each), STOP CONDITIONS joined
    HANDOFF section 10, the Standing bullets joined section 2 as
    rule 10 verbatim, and the Deferred list's live parts were
    already rows OI-093..OI-101/OI-095/OI-096 - deleted, not
    restated.

OI-105: retrofit: records PR-7..PR-195 carry no field block, so
  register sourcing for their threads leans on body prose
  status: OPEN
  notes: deliberately not retrofitted (PR-196's ruling); this row
    exists so the limitation stays visible.

OI-106: verify-utf8 needs the python-oracle frames that only
  `make py-refs` produces, and does not list it as a prerequisite
  (counted, NOT fixed here)
  status: OPEN
  origin: PR-193 GATES note

OI-107: the measured YETI ground run does not match the shipped F/T
  register: three camera-locked bursts measured 22.0-24.9 px/s while
  the register law with s=10 predicts 19.8-20.0 px/s for the same
  authored pair; two bursts land within 0.4% of |F|/4 = 25.0 px/s,
  the pre-PR-135 law
  status: OPEN - PR-211 read BOTH named leads and both are
    NEGATIVE; the row now stands or falls on one named lead,
    the physics-step call rate
  notes: the two leads this row carried are ANSWERED NO by
    PR-211's machine-code read. (1) The pairs are (X,Y): the
    parser 0x52590 -> merge 0x52680 -> integrator 0x54250
    chain takes the x legs from the FIRST of each F/T/G pair
    and the y legs from the second, so the seconds of
    T=50,5 / F=100,0 are the Y axis and cannot scale the
    ground x walk. (2) s ([obj+0xba]/[obj+0xbe], read at
    0x542ab/0x542cd) has exactly ONE writer in the image, the
    input-apply 0x549f0, and every IN path into it is a
    multiple of 5 with |s| <= 10 on x: the chase driver
    0x27d60 passes round(10*dx/(|dx|+|dy|)) (the 10.0 double
    at ds:0x909ef; =10 only when dy=0), the terrain driver
    0x34495 passes 10*[obj+0x6d], the dead-band branches
    store only 10/-10/0. Also dead: the integrator's T-raise
    gate needs T_x < 20 (authored 50), and A_RUN authors no
    FC row so the 0x54170 target leg is 0 for the walk. A
    ground walk at 22.0-24.9 needs s = 11.0..12.5 - no such
    s exists on any path; the register law itself matches the
    shipped port exactly (steady x = 5*F_x*s/T_x per step).
    THE ONE LEAD LEFT is steps per second: the engine's step
    wrapper 0x52750 integrates through the single +0xc8 slot
    call at 0x52774 and has exactly 10 call sites (the
    per-class frame drivers 0x23fc8, 0x264a8, 0x2701d,
    0x2c3ad, 0x2df08, 0x31fd8, 0x33c98, 0x3620d, 0x4fff1,
    0x61238); how often those run per display frame, against
    the display cadence PR-134 bounded, is the unread that
    1.1-1.25 steps-per-frame would explain. The 2.5-2.6 s
    bursts have a constant feet row (centroid y constant
    within the tracker's noise, blob height 55-60), so this
    is not the jump arc. The 22.0 burst is the one the record
    must explain; the 24.9/24.9/25.0 agreement with |F|/4 is
    a second unexplained spread inside 13%, still unstated by
    any law read so far.
  origin: PR-206 referee measurement

OI-108: ZARIA's `animacje=` objects drew through the WRONG PLANE in
  the port; PR-208 READ THE LAW (the level's own `plany=` key) and
  fixed it; PR-210 re-checked the corrected scene against the film
  and the contradiction FELL - the film AGREES with the shipped law
  status: CLOSED BY PR-210 (the plane law ANSWERED and shipped,
  PR-208:
  the [obj+0x24] plane of an `animacje=` placement is the layer whose
  planies spec carries that cell's A letter; ZARIA's balloons,
  ladder, ropes and coconut chain now draw at 1:1, main_back 2, as
  their own .des says. The re-check, PR-210, is DONE and POSITIVE:
  PR-207's scan pipeline resized the full 1280x720 frame to
  320x200 - 0.9y SQUASH, an artifact - and its cameras
  (28..37,150)/(240..260,207) were that artifact; with the correct
  /4+pad pipeline the locks are t=22.0 -> (30,162) sharp and
  law-free (no-obj and obj-on curves agree within 0.1 MAD across
  the whole y sweep), and PR-207's VALID disc box at t=22.0
  (279..287.8,131.5..141.8) sits ON the shipped law's design point
  POCISK (310,298) -> (280..292,136..148): left edge 1 px, top
  edge 4.5 px, the bullet chain moves. At t=31.25 the lock is
  (235..242,214..216) (shallow) and both discs sit within chain
  travel of the design points. The claimed "no single plane +
  in-range camera explains both frames" is RETRACTED.)
  notes: THE LAW, PR-208, from the shipped .des files themselves
  (12 of 13 levels; CHEATS has no .des): `plany=<spec>|<spec>|...`
  is ONE spec per tlo= layer (the spec count always equals the tlo
  count), letters ONLY {T,A,L,B}: T once per spec = that layer's
  own art, L once = the lew= lion and it sits on the main_back=
  spec on all 12 (that spec is the 100,100 speed pair), B once =
  the belka= belt and it too sits on a 100,100 spec, and A appears
  EXACTLY as often as the `animacje=` list has cells (3,3,3,2,3,3,
  5,4,4,4,3,0 vs list lengths 3,3,3,2,3,3,5,4,4,4,3,1-whose-only-
  cell-is-a-bare-none): the k-th A in reading order is the k-th
  cell and rides the layer of its spec. A bare `none` in the
  MIDDLE of a list is a cell and consumes a letter (RAN.01); a list
  that is ONLY `none` names no cell (JUNCTION). Port side:
  plan_t/plany_parse/des_list_cells + the `ll_level: planies: ...`
  report + one mapping in the placement loop; pinned by a two-run
  verify-level leg (lv_plany/lv_plany2) and by plany_selftest in
  the always-on --mov-selftest bundle. Engine side, raw machine
  code: 0x57500/0x57530 take the layer in EDX (0x5750a indexes
  [scene+0x20] by it, 0x57516 takes the divisor row from
  [scene+0x1c] = main_back); 0x52d67/0x52d72 load it from
  [obj+0x24]; 0x23b90 (`mov [eax+0x24],edx; ret`) is verb +0x04 of
  211 class tables. The un-lettered keys are OI-109.
      SUPERSEDED PORT FACT (PR-207 probe, pre-fix): every ANI2
    object - POCISK x16 fruit, BAL x6 balloons, the DRABINKA
    ladder, LINA ropes - reached obj_draw with `layer=1` and the
    85/66 draw-cam (printed: (26,169) at camera (31,150), (207,206)
    at (244,207)); that was PR-61's entry-index INFERENCE, printed
    as a camera. PR-208's fixture prints the same objects at
    layer=2 draw-cam=(37,250) and ANI3's ANIBACK at layer=1
    draw-cam=(1020,254).
      FILM FACTS, re-measured PR-207 at NATIVE resolution (the
    level clip is 1280x720 = exactly 4x the 320x200 view; every
    number below is render-space = video px / 4): SUPERSEDED BY
    PR-210 FOR THE CAMERAS (their scans resized 1280x720
    straight to 320x200: /4 horizontal but /3.6 vertical, a 0.9y
    squash that drags the picture's black bottom rows in - the
    y-locks 150/207/501..540 are that artifact). The disc boxes
    and the traps below STAND - they were measured in native /4
    space. background-locked
    cameras (full MAD scan of `ll_level LEO_LEVEL_NO_OBJ=1` renders):
    t=22.0 -> x in 28..37 flat (30.4; x=0 37.4, x=44 37.4), y=150
    sharp (30.4; 147:35.2, 153:35.0); t=31.25 -> x in 240..260 flat
    (30.2; 288:39.6), y=207 sharp (30.2; 204:31.4, 213:31.4);
    t=6.0 (level start) -> (4..24, 501..540) plateau, and at that
    camy both y-laws coincide - no discrimination. THE TRAP: the
    ZARIA recording carries a SCREEN-FIXED red graphics column
    (x 289..320 y 53..128) plus red in the bottom-left corner -
    identical in every frame (diff-tested), and the column sits
    EXACTLY where fruit would fall in view at those cameras. Red
    blobs surviving the overlay diff: t=22.0 exactly ONE KOKOS-sized
    disc (279..287.8,131.5..141.8); t=31.25 two (52.5..65.2,
    100.8..113.8) and (77..90.2,99..106.8). Predicted disc boxes
    from the authored points: at 1:1 with the locked cameras
    (279..288,148..159)+(291..302,148..159) at t=22, (66..75,
    91..102)+(78..87,91..102) at t=31.25; the port's 85/66 draws
    measured in its own renders (284..307,129..140)+(69..80,151..
    162)+(89..100,151..162) at t=22's cam, (103..126,92..103) at
    t=31.25's. The film's discs match NEITHER set consistently:
    t=31.25's second disc sits within ~1 px of the 1:1 x of
    POCISK (322,298) (y +7; the two y-laws coincide at camy 207 so
    y cannot discriminate there), while t=22's disc is 16 px ABOVE
    any 1:1 prediction and fits the 85/66 x only if the camera sits
    at the far end of its flat range (x~37). The contradiction has
    an observed source: POCISK/kokos.mov is the bullet chain (`
    pocisk` = bullet; jajo children spawn from it) - the film's
    fruit MOVES, and at t=22 the pair at design (95,320)/(115,320)
    is absent from the film at every plane. The set the port draws
    is definitively NOT what the film shows at either locked
    camera; the exact law is not pin-able from moving fruit.
      RETRACTION: the numbers in PR-206's OI-108 row (the "(310..
    319,146..157) match", the x=1/x=231 locks) were computed from
    intermediate extracted frames in /tmp (576x360) that no longer
    exist; re-extraction from the git reference at t=22.0 shows the
    x-lock was NOT sharp at x=1 and the disc coordinates differ.
    Use native 1280x720 = /4 extraction and the overlay diff.
      PR-210 CLOSED IT (the corrected pipeline, the disc match,
    the ANI3 waterfall window): the pipeline is scale /4 THEN PAD
    the 20 black rows (tools/re/video_camera_read.py --native
    encodes exactly this); cameras come from full-frame MAD with
    the two screen-fixed overlay boxes masked - t=22.0 y-curve at
    x=30: 22.83@162 MIN (28.46@150, 25.22@156/168), and the
    no-obj and obj-on curves agree within 0.1 across the whole
    sweep - a placed object is worth ~0.05 MAD on the full frame,
    so a full-frame lock cannot be a law artifact; t=31.25: x
    24.69@235/24.72@240, y 23.76@216 (shallow). The t=22.0 disc
    sits ON the POCISK (310,298) design box (280..292,136..148)
    - 1 px x, 4.5 px y - and the "absent at every plane"
    (95,320)/(115,320) pair is exactly what a bullet chain that
    already fired looks like. The waterfall window (t=70.5/72.5/
    75) puts ANI3's WAZ column on screen: the port draws its e0
    at (168..199,28..61) at cam (1725,520), the film's blue
    runs are 257..284 / 257..284 / 185..233 (width ~ the 32 px
    art); the shipped L1 law predicts the 1:1 lisc4 crown
    235.75-0.15C px RIGHT of the water edge = adjacent in the
    film, and the water's 72 px run = 0.85 x the implied camera
    run; while PR-61's entry-index (ANI3 -> layer 0) puts that
    water at 1634-0.39C, on screen only for camx 3315..4190 -
    impossible here - so under the old rule the waterfall could
    NEVER have been in this window. SUPPORTING, not the close:
    that window's camera never locked sharply by a second method.
    LEADS: the 1:1 lisc y-anchor there implies film camy ~520,
    above the port's YMAX=500 clamp, while ZARIA.01 authors
    lisc2.seq at y=1546, far outside its own 700-row world -
    unmeasured, start there if level-bottom geometry is ever
    revisited; ZARIA.02/03 clips are unmined (ZARIA.02 has only
    5 usable static landmarks).
      PR-207's LEADS FOR THE LAW, NOW RESOLVED AND CORRECTED:
    FUN_0005b040 `case 0x10` writing [rec+0x24] 0/1 on token 0x5e
    and FUN_0005da60 testing [rec+0x24]==0 are NOT this field -
    their +0x24 is a 0/1 flag paired with a bit-mask test at
    0x5b040+0x276, in a text-run record. The real field's getter/
    user pair is 0x52d67 -> 0x57530 and its setter is 0x23b90
    (above). PR-209 read the planies CONSUMER (0x4af10: a
    per-invocation letter PROGRAM driving tlo/lion/belt/pool
    cells; it never writes [obj+0x24]) - the plane law above is
    unaffected, and the engine-side writer of the plane is still
    open (OI-109). The film plan (the three ZARIA I references + the
    stateless ANI1 PICTs, template-matched by their own art) is
    what a re-check must use; the fruit chain moves.
  origin: PR-205/206 opened; PR-207 re-measured (the film facts
  below ARE that measurement, cameras excepted - see the PR-210
  block); PR-208 answered the law and fixed the port; PR-210
  re-checked the corrected scene against the film and CLOSED it

OI-109: the plane of the two placement keys that `plany=` NEVER
  NAMES - `zwierzaki=` and `ani_plat=` - is still not read from the
  engine; the port keeps PR-61's entry-index model there
  status: CLOSED BY PR-213 - the law was read by PR-212 (below)
    and the port now applies it; the still-unread leads moved
    to OI-110
  notes: THE LAW (PR-212, exact bytes): one level-load pass,
    0x4b1d0 (its ONLY call site 0x4d764, in the same flow
    that builds the lion at 0x4d668 and sets the lion's
    plane at 0x4d68b). A key list's entry i chains at cell i
    RAW ('none' occupies a head and shifts nothing;
    0x4ce52/0x4dd50/0x4aaa0 - neither builder writes +0x24).
    For cell c of each pool id 0..2 the pass counts the '|'
    bytes before the (c+1)-th 'A' in the level's planies
    program ([[level+0x30]] - the same string the letters
    run, PR-209's consumer 0x4af10) and dispatches that
    count as the plane to EVERY entry in that cell through
    class-table slot +0x5c (0x23b90 `mov [eax+0x24],edx` in
    150 of the 177 tables long enough; propagation variants
    0x1ecb0/0x332f0/0x554f0/0x531b0 - the image's ten
    `call [reg+0x5c]` sites are all accounted for in the
    PR). When the A's run out the plane is the LAST segment
    (total '|'). The lion rides separately: [lion+0x24] =
    main_back (0x4d68b); children created at runtime inherit
    their owner's plane at once. The letters never named a
    key's plane - the program's SEGMENTS name it for all
    three keys at once - which is why no shipped spec
    carries a letter for those keys. PR-209's own note
    mis-named 0x23e00 as a +0x24 getter; it is PR-106's
    bounds test (corrected in the record).
      DATA: ZARIA.01 `T|TA|TALAB` -> cells 0,1,2 -> planes
    1,2,2, so KAMIEN (cell 1), ZWIERZ1 and ANIPL2 all ride
    plane 2 = main_back = the lion's plane - their collision
    agreement with the lion is a CONSEQUENCE of the law, not
    evidence. LONA.05 `T|TAAALAB` -> every cell of every
    pool -> plane 1 (FOKA/PINGWIN/YETI and ANIPL2/22/3 ride
    the lion's own plane 1). KADMAR2 `TALABA` -> everything
    0. The engine's cell maps equal the port's PR-208
    animacje maps everywhere checked (TUNGA.11 1,2,2,2,2;
    ENA2 2,2,3) - the port's animacje law IS the engine's
    law, extended verbatim to the other two keys.
      THE PORT NOW RIDES THE LAW (PR-213): plany_parse fills
    cell_plane[] = the raw segment law for every cell of every
    pool (independent of `ok` - the engine never refuses a
    spec) and the placement loop takes cur_layer from it for
    ALL THREE keys. Measured after the change: ZARIA.01
    KAMIEN layer 1 -> 2 (with ZWIERZ1/ANIPL2 unchanged at
    2), LONA.05 PINGWIN/IGLO layer 2 -> 1 = the lion's own
    plane, KADMAR2 unchanged at 0. The lv_plany2 KAMIEN pin
    and the planies selftest were re-derived honestly to the
    engine values BEFORE the code changed (failing test
    first); verify-play's ZARIA LICHA legs are unmoved (that
    entry sits in cell 2, the one plane where the old index
    model already agreed). All 12 shipped levels author a
    plany= key, so the no-spec fallback never fires on
    shipped data.
      origin: PR-208; consumer read PR-209; law read PR-212;
      port PR-213

OI-110: the planies ACTIVATION chain is unread: what feeds the
  [level+0x64] chain the 'L' program walks, what sets the
  [obj+0x2c] flag the +0x60 predicate reads, and what the +0x74
  verb performs
  status: OPEN (opened by PR-213, which moved the named
    remainders here when OI-109 closed)
  notes: PR-212 mapped the surface exact-bytes: the k-th 'A'
    calls 0x4b360, which walks cell k of ALL THREE pools and
    runs +0x60 (0x23b80, `mov eax,[eax+0x2c]` - the
    predicate) then +0x74 on entries whose flag is set; 'L'
    runs the same pair on the [level+0x64] chain through
    0x4b2f0; [level+0x64] is zeroed at level load (0x4d75d),
    so whatever feeds it does so after that point (PR-105
    item 7). The placed animals move without any 'A' ever
    firing for their benefit, so creature motion does not
    route through that pair - the pair is specifically the
    per-cell-index activation path for placement entries.
    Also named: nothing besides the draw-list insert (PR-208:
    0x52d67 -> 0x57530 keyed on [obj+0x24]) and the
    collision-plane gate is known to consume [obj+0x24]; the
    propagation verbs +0x5c variants (0x1ecb0 -> [obj+0x120],
    0x332f0 -> [obj+0xee], 0x554f0 -> child array
    [obj+0x40]x[obj+0x3c]) imply the plane flows to attached
    and child objects for some consumer still unnamed.
    PR-214 NAMEDS THE FIRST SUCH CONSUMER: the class slot
    +0x7dc pair-answer responder. TLewZaria (0x96960) holds
    0x531b0 there, which calls [[asker+8]+0x5c] with
    edx = [self+0x24] (the plane), then 0x4b140 - a second
    reader of the planies string [level+0x30]: it returns the
    count of 'A' before the first 'L' when the object is the
    lion ([level+0x394]), else the first index whose pool
    cell exists (0x4b0f0: i < [level+0x44+k*4] and
    [[level+0x58+k*4]+i*4] != 0, k = 0..2) - and 0x4aa70
    resolves that (key, cell) through 0x52c10. So the ZARIA
    lion answers pair-asks by plane-matching against the
    planies segment that carries the 'L' (= 2 for ZARIA, the
    same number PR-213's cell_plane gives). Still unread
    here: the [level+0x64] feeder, what sets [obj+0x2c], and
    the 13-byte answer sets the +0x78 entries build (the
    0x3d990 variant at 0x3d9e5, held at +0x78 by the five
    lion-base tables 0x96874/0x991dc/0x9a1a4/0x9ad48/
    0x9e740).
  origin: PR-212 remainders; row opened PR-213


OI-111: which row-step path the port's row clock models -
  a `Wait` clause SELECTS the path, it never refuses the row
  status: OPEN (opened by PR-217; REWRITTEN by PR-218, whose
    read settled the half this row asked; the port half of its
    OPENS line was closed by PR-333, which is what the row pass
    does now)
  notes: SETTLED by PR-218, on the engine and on the shipped
    data, so the clause stays captured and un-actioned:
    (1) The clause's MODE byte at [set+0xc] is written by the
    clause parser: 0x47d73 clears it, 0x47db2-0x47dd0 write 1
    for `!`, 2 for `*`, 3 for `|`, and a comma writes nothing
    (it adds a second signal to the same clause). (2)
    TEventSet::match 0x47e40 (its name: the error string
    0x919ca) switches on that byte through 0x47e30: mode 0
    (0x47e63) is true when (self AND other) EQUALS self byte
    for byte, i.e. the clause is a SUBSET of the set handed
    to it; mode 1 (0x47edc) when the AND is EMPTY (shares
    nothing); mode 2 (0x47fb2) when the two sets are
    byte-identical; mode 3 (0x47f42) when the AND is NOT
    empty - PR-261 re-read the table at 0x47e30, whose slots
    send 2 to 0x47fb2 and 3 to 0x47f42, the pairing this row
    had the other way. (3) The
    row step 0x521f0 asks the ROW object ([rec+0x2e][rec+0x4],
    class 0xa2638) through its test slot +0x8, which IS the
    chain delegate 0x45800; the ctor 0x45780 stores each
    directive node's operand at [node+0x0] under the base class
    word 0xa1000, 0x45800/0x45870 answer with the operand's
    answer (0x45800 EMPTY if there is none), and a clause node
    COMPOSES - 0x45970 and 0x45920 call the chain getter
    0x45870 and OR their own [node+0x8] in (0x49580 at
    0x4593c). (4) 0x5223c then tests that answer with 0x495b0
    (true only if all three data dwords are zero) and
    `je 0x522fc` sends the NON-EMPTY answer to the execute slot
    +0xc and OR-s its result into the CALLER's set (0x5230c);
    the row then runs only if its authored frame [row+0x10] is
    BELOW the object's clock [obj+0xa1] (0x52320-0x5232b), the
    difference being its budget (0x52338). Only an EMPTY answer
    falls through to 0x52251, which OR-s the caller's key with
    the object's own EventSet [obj+0x60] and calls match at
    0x522e6: 0 skips the row (0x522ed), 1 runs it with a ZERO
    budget. (5) On shipped data the match test is INERT: 230 of
    the 251 captured clauses are mode 0 and 21 mode 1, `*`/`|`
    appear in zero rows, and every clause leaves a NON-empty
    set (K_NEVER OR-s 0x60 into data byte 4 at 0x4806f-0x4807d;
    K_ANY OR-s the mask 0x2001f8 into dword 1 at
    0x48037-0x48048), so no shipped row reaches match WITH a
    clause, and the clause-less rows that do reach it carry an
    empty clause, which is a subset of anything. The row clock
    for a row that has no clause is therefore not consulted at
    all.
    WHAT IS LEFT, and it is now a single question: the port's
    row clock (PR-163's record step, fed 100 by the state
    record's reset 0x52362 at [rec+0xa1], and PR-216's index
    gate) corresponds to WHICH of the two branches - the
    frame/clock compare at 0x52320 that a clause-bearing row
    takes, or the zero-budget immediate path that a clause-less
    row takes? Every ZARIA.01 ladder row is of one of those two
    shapes (`[A_UP]` row 1 `1,0,WAIT K_GEN1`, `[A_DOWN]` row 11
    `11,0,WAIT K_NEVER`), so the answer decides the climb. The
    port keeps the clause (ll_mov_fr.gsig_id/gneg/gop) and acts
    on none of it, so no frame moves until that is read.
    Census through the port's own parser (pinned in
    verify-endlevel): 251 Wait rows in 133 of the 777 members,
    4 tails NOT captured (1 tokenless `Wait ;`, 3 two-token
    incl. `WAIT |K_ANY,rdir`), 21 negated rows, 161 no-bit
    operands - all 161 K_NEVER after PR-218's terminator fix,
    0 K_ANY, 0 unknown. The LION's answer slot at its class
    table 0x96948+0xd4 is 0x544d0, an EMPTY override (the
    PR-153 boundary-artifact trap), so the `state index == 2` +
    AC_BREAK_NEXT case at 0x54482 is a no-op for the lion.
    PR-219 named the clock itself, and CORRECTS the two claims
    above about it: the field is the OBJECT's [obj+0xa1], not
    [rec+0xa1]; the row reset entry is 0x52360 (0x52362 is a
    2-byte dump artifact) and at 0x5236b it writes ZERO, not
    100. Its writers are the row-run verb 0x520c0 at
    0x52114-0x52118 (it sets [obj+0xa1] to the budget the row
    step handed over: clock - frame on the clause path, 0 on the
    clause-less path) and its readers are the row step at
    0x52323 and the accumulator 0x543a0, which is its FEEDER:
    it reads the state's control bits [rec+0x12] & 7, jumps the
    table 0x54380, and adds |[obj+0xca]| (0x543c6-0x543d1),
    |[obj+0xce]| (0x543dc-0x543e7) or the scene frame delta
    [scene+0x388] gated by [obj+0xba]/[obj+0xbe]
    (0x543f2-0x54414, 0x5441b-0x5443d) - the port's own
    distance-fed vs wall-time-fed split (PR-110/PR-114), now
    with names. 0x520c0 is the record class 0xa260c's +0x18,
    reached as `call [esi+0x18]` with esi = [rec+0x44] = the
    record's own class word (the vtables here are the base+8
    kind, which is also why 0xa11c8 = 0xa11c0+8 and
    0xa11e0 = 0xa11d8+8); it clones the row and runs it on.
    The key composition PR-218 described as "composes the key
    with the global 0x98278" is 0x520e4-0x52100 inside that
    verb: a twelve-byte buffer gets the class word 0x98278 and
    is handed to `[[rec]+0x28]`, which the record's own class
    table 0x97ca8 resolves to 0x52790 - a WORLD-QUERY set
    built from the row's CHECK_LEW / CHECK_ANI / CHECK_ZW
    keywords (PR-289; PR-288 read the same six instructions
    first and called the buffer a POINT, which PR-289
    corrects). It then calls the row class's
    +0x10 with edx = 6 (0x5213d), tests the answer against 6
    (0x52140), and does the list-end/wrap: 0x521b6-0x521d4
    compares [list+0x8] - 1 with the cursor and OR-s bits 4 and
    8 into the CALLER's set byte 0 (0x521a1, 0x521c1) -
    PR-135's list-end bit, now named. What the row class's
    +0x10 verb DOES, and which authored number the row step's
    compare actually gates on, is OI-112.
    PR-322 ANSWERS THE +0x10 HALF. The row descriptor (0xa2638)
    holds the base chain walker 0x458e0 there; it walks the
    row's directive nodes' +0x10 verbs with ebx = &[rec+4] (the
    row CURSOR), ecx = &answer and dl = 6. Only three directive
    families carry a non-base verb there: goto absolute
    (0xa1030+0x10 = 0x45d90, cursor = stored 0-based number,
    answer 6), goto relative (0xa1018+0x10 = 0x45dc0, cursor +=
    authored delta, answer 6) and the F-row hold (0xa1090+0x10
    = 0x45d50, PR-224/250); an answer of 6 at 0x52140 SKIPS
    the normal step, so a goto row never wraps and never raises
    the list bits. PR-327 names that +0x70 verb for one class: on
    the ZARIA lion's own table 0x97a98 it is 0x239f0, `mov
    eax,[obj+0x6d]; ret`, PR-321's facing word - so that half of
    the reverse test is "the object faces left". The normal step
    is fully read now: forward
    (cursor+1, last row -> OR 0x4 = AC_BREAK_NEXT + wrap to 0)
    or REVERSED - (class +0x70 verb == -1 AND [rec+0x12] bit
    0x10 SEQ_REV_X) XOR ([obj+0x71] == -1 AND bit 0x20
    SEQ_REV_Y) XOR ([obj+0x99] == -1) - which steps the cursor
    DOWN and raises bit 0x8 (index 3, name unverified) at the
    top. The port models neither the cursor jump nor the
    reverse step; modelling the answer-6 protocol is the new
    F2b lead (HANDOFF 8 T5), and what `zero`/`zero_t`
    (0xa0f88/0x45a50/0x52070 - the verb PR-320 mis-called a
    goto apply) actually zeroes, [obj+0x9d] and the
    [rec+8]/[+0xa]/[+0xc] ramp, stays unread here.
    PR-323 MODELS the answer-6 cursor jump on the PLAYER path:
    ll_mov_fr carries gotocmd/gotoval, the arms parse the three
    names (`goto` storing the 0-based number, the -1 folded in
    at parse exactly like 0x46777), and the row-close branch
    steps fi to the authored target (or adds the delta) INSTEAD
    of the +1/wrap - no list bit on that close, the 0x52140
    skip. The port's own additions are named, not read: the
    [0, nfr-1] clamp (the engine reads the heap around the row
    table), and re-entry through a jump counts as an admit for
    the PR-253 one-shot pull (no shipped row distinguishes).
    The object row clock (ll_level.c's o->fi/o->row_t) has NO
    such branch yet, and the SEQ_REV reverse step stays
    unmodelled.
    PR-324 ADDS that object half: obj_advance's row close now
    takes the same answer-6 branch (the wrap, its base-position
    reset and brk_arm belong to the NORMAL step and are skipped
    wholesale), so RYBA/KAMIEN/SEP/SPLYW/ANIBACK_*/KADMAR2_-
    SCHODY run their authored loops; the object legs pin the
    `GOTO_REL -2` cursor cycle, the through-seam motion, and
    that a jump DELAYS the list end. What stays unread here:
    the engine's out-of-table cursor read (both ports' clamps
    are named nets), whether re-entering the CURRENT row re-
    runs the [obj+0x95] one-shot gate (the object path models
    no one-shot pulls at all), and the FC/`zero` row-entry
    effects; the reverse step remains the only open F2b item.
  origin: PR-217 record; rewritten from the PR-218 record; the
    ladder chain stays OI-078
    PR-220 names the control bits that switch picks between:
    the `control=` parser 0x51b0d takes the first token of the
    line (` %[^,| \r\n\t]` at 0x9215d) and OR-s it into the
    record byte [rec+0x12]: BY_X = 1 (0x51b6f), BY_Y = 2
    (0x51b89), BY_TIME = 4 (0x51ba3), BY_TX = 8, BY_TY = 0x10,
    then FRAME_REV 0x20, SEQ_REV_X 0x40, SEQ_REV_Y 0x80, with
    more at [rec+0x13] (0x51c59). CORRECTED BY PR-320: that
    keyword-to-value list is shifted - the parser loads the
    keyword strings in its strcmp chain, and each OR-site
    follows the strcmp that loaded THAT string: 0x9216e
    "BY_X" jumps to a sets-nothing branch (mode 0 is the
    default), and 0x51b6f/0x51b89/0x51ba3/0x51bbd/0x51bd7/
    0x51bf1/0x51c0b/0x51c3f/0x51c25 follow strcmps against
    "BY_Y"/"BY_TIME"/"BY_TX"/"BY_TY"/"FRAME_REV"/"SEQ_REV_X"/
    "SEQ_REV_Y"/"CHECK_ANI"/"CHECK_ZW" setting 1/2/3/4/8/0x10/
    0x20/0x40/0x80; 0x51c73 CHECK_ALL ORs the WORD 0x1c0,
    0x51d5f "SOLID2" the DWORD 0x20400, 0x51d79 "SEMI_SOLID"
    0x40400. The original's keyword names and mode numbers are
    crossed one slot in this row's list - `BY_X` sets nothing
    (mode 0 is the default), and the four mode keywords set
    1/2/3/4 - exactly the table ported/ll_play.h:104-105 already
    ships. So 0x543a0's switch is
    case 0 -> |[obj+0xca]| (walked X), 1 -> |[obj+0xce]|
    (walked Y) (0x54380-0x54390), 2 -> 0x543fb, 3 -> 0x543f2
    and 4 -> 0x5441b, both scene-frame-delta paths gated by
    [obj+0xba]/[obj+0xbe]. NOTE for the port: its
    LL_CTRL_BY_TIME = 1 / LL_CTRL_BY_X = 2 constants are its
    own vocabulary and do NOT equal these engine bit values.
    PR-224 splits the clause verbs cleanly and narrows this
    row again: a clause node's TEST (+0x08) answers with its own
    authored set (0x45970 copies [node+0x8..0xc]) and its
    EXECUTE (+0x0c) is the base delegate 0x45870, EMPTY because
    a clause node has no operand - the node ctor 0x45780 writes
    0 at [node+0] and only clones an operand it is given, while
    the clause parser 0x47d60 fills the set INLINE at
    [node+0x8]. The `GEN` clause 0xa11e0 keeps the base test and
    its execute 0x45920 ORs [node+0x8] in through 0x49580 - the
    engine's raise path for `GEN <signal>`, OR-ed into the
    CALLER's set at 0x5230c. The clause's CHAIN verb is also the
    base walker with no operand, so a `Wait` clause cannot be
    what holds a row in the answer-6 repeat loop: the answer-6
    holders are the SIXTH (+0x14) verbs of the per-directive
    classes - 0xa1048 (`goto` family) = 0x45d90, jump on the row
    cursor, 0xa1030 = 0x45dc0, delta, and 0xa1090 (`F`) =
    0x45d50, gated on the node's own trigger byte. The clause
    selects the PATH; a directive holds the row.
    PR-287 CORRECTS item (4) and the INERT reading of (5), and
    acts on the clause. The branch is `test eax,eax; sete al; and
    eax,0xff; je 0x522fc` (0x52241-0x5224b): 0x495b0 answers 1
    for an EMPTY set and the `je` takes that case, so an EMPTY
    answer goes to the execute + clock-compare path 0x522fc and
    only a NON-empty answer - a `Wait` clause's own set - falls
    through to the match at 0x522e6, whose refusal is `je
    0x52354` = the epilogue (no execute, no clock compare, no
    class verb, no frame, no list wrap). PR-218 paired them the
    other way, and its consequence cannot be the shipped game:
    under that pairing the rows that carry NO clause - every row
    but the 251 the port's own parser captures - would run on a
    zero budget with no clock, and no authored frame duration
    would be honoured, while the durations PR-109 and PR-110
    MEASURED on screen sit exactly there. So the clause gates:
    a plain name must be live (mode 0, a subset), a `!` name
    must not be (mode 1), the no-bit K_NEVER never admits, and
    the tokenless `Wait ;` compiles to an empty set and gates
    nothing. The port acts on it now - obj_advance asks
    ll_mov_fr's gate before its clock, as 0x52235 precedes
    0x52329 - and the ZARIA.01 ladder is the net: A_UP holds
    frame 1 and wraps NOTHING until the ask raises K_GEN1,
    A_DROP then runs its authored 2700 units (5+4+4+3+3+2+2+2+2
    = 27 of this port's iterations, ly_climb 105..131, its last
    row arming the list-end bit at 0x521c1-0x521c5), and A_DOWN
    holds frame 11 for good on `WAIT K_NEVER`. Two halves stay
    open and are now this row's whole remainder: (a) the gated
    path SKIPS the threshold compare at 0x52329 and runs with a
    ZERO budget (0x522ef-0x522f8), which the port does not -
    the two agree only where the authored threshold is 0, and
    the shipped exceptions are real object rows with a live
    operand: `1,100,WAIT K_GEN1` (ENA.03_SKALA, ENA2_DEMOTEXT,
    TUNGA2.12_DOMEK), `1,1000,WAIT K_GEN1` (LONA.05_SKJUMP),
    `WAIT AC_TIME` rows at 300..4600 (LONA.05_FLAGA, ZARIA.02
    and SADES.06's STULBIA family), `WAIT AC_DUP` at 200/300
    (LONA.05_PINGWIN, LONA.05_YETI, KADMAR.07_0_0); a
    K_NEVER row's threshold never matters, its clause never
    matches; and (b) the LION's own row admit does not consult
    the clause at all, while its scripts carry it too - 46 of
    the 201 `Wait` rows a line-anchored grep matches (the
    port's parser captures 251) are in lion scripts, ZAR_LEW's
    `8,0,WAIT AC_DUP` and `8,200,WAIT !K_UP` among them; and
    (c) the OTHER half of the set match is the caller's
    argument, and its origin is open - NOT a level-wide set.
    PR-287 repeated PR-218's phrase that the row walk 0x520c0
    composes the caller's set "with the level-global set at
    0x98278"; PR-288 read those six instructions (0x520e4-
    0x520f6) and 0x98278 turned out to be the engine's shared
    POINT class - its +0x04 verb 0x23ea0 writes two dwords,
    its +0x08 verb 0x23e00 is a point-in-rectangle test, and
    the base ctor 0x53ff0 embeds one at [obj+0xd6..0xde]
    (OI-118) - so the walk builds a stack POINT from
    [[obj+0]] and [[obj+0]+4] and hands it to the record verb
    [[rec]+0x28]; no three-dword OR 0x49580 appears in the
    walk at all. Inside the row step the only OR-s are
    0x522bc (the caller's argument unioned with the object's
    own [obj+0x60], which is what the gate reads) and 0x5230c
    (the execute result back into the argument). Measured
    consequence unchanged: with the gate in, a 200-tick
    ZARIA.01 probe traces ZERO `KAMIEN [A_STOP]->[A_GO]`
    where the same probe on the pre-fix build traced three -
    the stones are held by their own `1,0,WAIT K_GEN1` row,
    because nothing in this port raises K_GEN1 on a KAMIEN
    except the contact ask of PR-241. What is open is the
    argument, not a bus: 0x521f0 has no direct caller and the
    image names no reference to its class table 0x97ca8.
    PR-289 found the only set built in that path - the record
    class's +0x28 verb 0x52790 fills a caller buffer from the
    record's CHECK_LEW / CHECK_ANI / CHECK_ZW keywords through
    the scene queries 0x4bd30 / 0x4b890 / 0x4b870 on the object
    at [0xcef40] - so the argument is most likely that set, but
    the call into 0x521f0 is still not seen and the link stays
    inferred. PR-290 read those three queries: each walks the
    objects the level keeps, gates every candidate through its
    +0x60 verb, skips self and (unless CHECK_ALL_PLANE) same-
    container entries, and copies the FIRST candidate's answer
    - its +0x78 verb 0x23920, four instructions returning
    field +0x12 of `[obj+0x50] + [obj+0x5c]*4` - into the
    caller's buffer. With no CHECK_* keyword the composed set
    stays empty, which is the DRABINKA case of OI-078.
    PR-291 adds one more entry to this row's table block: the
    word at 0x97ca8+0xac = 0x97d54 is 0x3f1c0, called by the
    state-change pass at 0x535f7/0x53620 through the OBJECT's
    class word [obj+0x08] right after the state index
    [obj+0x5c] is written (0x535ed); only three sites in the
    image call `[…+0xac]`. So the block PR-290 reached through
    [rec+0x44] is also what [obj+0x08] points at - one class,
    two dispatch words (sharpening OI-118's question; inferred
    from the single instance of 0x3f1c0, not seen).
    PR-293 gives this row's caller argument a NAMED PRODUCER:
    the set composer 0x523f0 (called by the frame step
    0x536f0 at 0x5371c, OR-ed into [obj+0x60] at 0x53725)
    builds one 12-byte set buffer on its own stack and hands it
    to five verbs in order - the record's +0x08 (0x52570), the
    record's +0x0c, which IS the row walk 0x520c0, the owner
    object's +0xac and +0xa8 through [[rec+0x00]+0x08], and the
    record's +0x10 (the ENTER 0x4fef0 of PR-172) - and returns
    it. The walk is therefore reached with that buffer, and the
    row step 0x521f0's [esp+0x68] has no other candidate. The
    walk-to-step call is still unseen.
    PR-332 SETS THE BRANCH BYTE-EXACT AND INVERTS THE ASSIGNMENT above.
    0x5223c calls 0x495b0, which answers 12 when all three data dwords
    are zero (its loop 0x495bd-0x495cc) and 0 the moment one is not
    (0x495ce); 0x52241-0x5224b then do `test eax,eax / sete al / and
    eax,0xff / je 0x522fc`, so the jump to 0x522fc - the execute slot
    +0xc, the OR into the caller's set, the class +0xa0 verb, and the
    frame/clock compare at 0x52320-0x5232b with the budget clock minus
    threshold handed to the record's +0x18 at 0x52334-0x52338 - is
    taken by the EMPTY answer, the clause-LESS row. A row that DOES
    carry a `Wait` clause falls through to 0x52251, which copies the
    caller's set (0x52255-0x52278) and the object's [obj+0x60]
    (0x52279-0x522a6), merges them at 0x522bc and calls
    TEventSet::match 0x47e40 at 0x522e6; the mode byte that drives
    0x47e40's table is read from [eax+0xc] at 0x47e4b - the CLAUSE's
    own mode. No match: `je 0x52354` at 0x522ed leaves the whole step -
    no execute slot, no clock compare, no [obj+0x95] admit flag (set to
    1 at 0x5234a, zeroed at entry 0x52217). Match: `xor ebx,ebx` at
    0x522f8 and `jmp 0x52338` - the row runs with a ZERO budget, so
    the threshold compare at 0x52329 is never reached. PR-219's
    parenthetical ("clock - frame on the clause path, 0 on the
    clause-less path") is exactly the other way round, and so is the
    question at this row's head. The node's answer is its clause:
    0x45970 copies [node+0x8]'s 12 data bytes and its mode byte
    (0x45981-0x4598f) and never calls the operand, so only a bit-less
    clause - the one tokenless `Wait ;` - is EMPTY. And the two
    special names are MASKS, not meanings: the resolver 0x48000 matches
    the strings AC_K_ANY (0x919ec) / K_ANY (0x919f5) and OR-s
    0x2001f8 into data dword 1 (0x48029-0x48048); AC_K_NEVER (0x919fb)
    / K_NEVER (0x91a06) OR-s 0x60 into data byte 4 (0x4806f-0x4807d);
    both return 1 through 0x49543 and both are NON-EMPTY, so both take
    the match path. In this port's bit indexes that is K_ANY = bits
    35..40, the six arrows, plus bit 53, and K_NEVER = bits 37,38 =
    K_UP and K_DOWN together. What a name means is the mode byte's
    business: plain subset (0x47e63), `!` shares nothing (0x47edc),
    `*` identical (0x47fb2), `|` shares anything (0x47f42), PR-218's
    table unchanged - so `|K_ANY` is "an arrow is held", plain K_ANY is
    "all six at once" and plain K_NEVER is "UP and DOWN at once", both
    in practice never, which is the idiom the 115 `WAIT K_NEVER` rows
    are written with. WHAT WAS LEFT OPEN THERE was the port's own use
    of the two paths, not the reading: ll_play.c's player row clock
    never asked ll_mov_row_gate, and PR-287's object-side gate kept
    the threshold compare on the match path, which PR-287 itself
    named and which only shows on a gated row with a non-zero
    threshold -
    170 of the 203
    Wait rows counted in PR-332's record. PR-333 CLOSES that half and
    is now what this row describes:
    PR-333 PORTS BOTH PATHS. ported/ll_play.c gained ll_mov_fr_mask
    (the clause as the mask the resolver builds: LL_SIG_K_ANY_MASK for
    K_ANY at 0x48037-0x48048, LL_SIG_K_NEVER_MASK = K_UP|K_DOWN for
    K_NEVER at 0x4806f-0x4807d, one bit for a name in a table, and 0
    for a name in NO table or for the one tokenless `Wait ;` - 0x495b0
    answers EMPTY for those, so they are not gated at all),
    ll_mov_row_gated (mask non-zero = this row carries a clause),
    ll_mov_row_gate rewritten as the engine compiles it (the clause's
    OWN mode byte: plain is the subset arm 0x47e63, `!` is shares-
    nothing 0x47edc), and ll_mov_row_admit, which IS the branch: a
    gated row that does not match admits NOTHING (no frame, no cursor
    step, no wrap, no list-end bit, no threshold consumed - 0x522ed),
    a gated row that matches runs with the ZERO budget of 0x522f8 (its
    threshold compare at 0x52329 never runs and the accumulator is left
    at zero), and only a clause-less row reaches the clock compare and
    carries the remainder (0x52320-0x52334). The player's row pass
    (player_iterate section 7) and obj_advance's row step in
    ported/ll_level.c both call it, which is PR-287's corrected half.
    WHAT THE CLAUSE IS MATCHED AGAINST is the port's own named
    deviation, and PR-333 makes it load-bearing: 0x52255-0x522a6
    merges the CALLER's set with the object's persistent [obj+0x60],
    so the player row pass captures `row_live` - the set the
    transition pass of the SAME iteration matched with, taken just
    before player_iterate's `p->sig = 0` - because a row clause must
    see an edge-only signal: ZARIA.01's A_JUMP row 10 is
    `8,0,WAIT AC_DUP` and AC_DUP exists for one iteration, raised at
    the surface attach (PR-331). The engine's clear points are still
    the ctor/allocator paths only (PR-260's read), so the port's
    per-tick clear and this snapshot of it stay the named deviation at
    OI-078. --play-selftest's LEG U pins the two paths with no data
    at all: the same `2,1000,WAIT K_NEVER` row parks forever with
    neither arrow held, admits in one iteration with both (the 1000
    units unread), `!K_DOWN` admits unless DOWN is held, `K_DOWN`
    admits with it, the clause-less twin of the same row takes ten
    feed iterations, and no gated admit ever leaves a non-zero clock
    behind. Play effect, one line each: A_FALL_DOWN's
    `7,0,WAIT K_NEVER` parks the ZARIA.01 pit fall on frame 7 instead
    of wrapping into A_DRAP; A_DOSTAL's `5,200,WAIT !AC_ENEMY_ATC`
    matches a clause instead of waiting for its clock, so the hit
    state's list closes one iteration before its 200 units are owed;
    A_JUMP's row 10 holds the
    flight and the landing happens INSIDE A_JUMP, then the authored
    `A_JUMP(11)=AC_DUP` restart puts the recovery row next and its
    banked clock admits at once, so its own `A_WALK(7)=AC_BREAK_NEXT`
    runs at the start row that line names; A_LOOKDOWN's
    `5,300,WAIT !K_DOWN` holds the look-down exactly as long as DOWN
    is held. Seven Makefile legs re-pinned by that (PR-90, PR-140,
    the PR-86/253/256 jump leg, the PR-274/285/287 climb chain,
    PR-277, PR-278, PR-286) and two data-free legs of
    --play-selftest. WHAT THIS ROW STILL HOLDS OPEN: what the class
    verbs [obj+0x8]+0xa0 and +0xa4 (0x52317, 0x52341) do and whether
    either touches the clock; bit 53 inside K_ANY's mask, the one bit
    this port has no name for; OI-112's row TIME number and the row
    descriptor's +0x10 verb; and the modes 2 and 3 (`*`, `|`), which
    no shipped row uses (PR-218's count) and this port still does not
    implement.
    PR-401 READS THE ROW ADVANCE'S VISIT. The row step's other half -
    the `[class+0x18]` call at 0x52338, with the class read from
    [ctx+0x44] - enters 0x520c0, which PR-100 called the row-change
    handler and which sits at +0x18 of exactly 16 record-class tables
    (0x97c9c and 15 others) with no direct call anywhere. It is a VISIT
    in PR-400's sense: it takes the node at `[[rec+0x2e]+[rec+4]*4]` -
    the CURRENT ROW of the record's own node list - and calls that node's
    +0x04 verb with the record as the second argument (0x520cf-0x520e1).
    The row descriptor 0xa2638 this row already names has the bare chain
    walk 0x457e0 at +0x04, so a row-node visit runs the +0x04 verbs of
    the clause nodes hanging off the row and does nothing itself. Four
    instructions later the same function calls `[0x98278+4]` = 0x23ed0,
    PR-101's open item (4): read from its own entry it is a base-class
    DELETE with flags - `test dl,0x4` to the free pair 0x74bc0/0x76c00,
    `test dl,0x2` to 0x5c370 - so the row-advance disposes of what it
    visited. The three sites that start a visit anywhere in the image are
    that one plus 0x74a98 and 0x74b63 indexing the global slot 0xcf518;
    what that slot indexes over stays open. [PR-402: it does not index
    over rows. 0xcf518 holds the static base-implementation table at
    0xa9ab8, and 0x74a98/0x74b63 are calls of a base thunk inside the
    invoke-by-descriptor helper 0x74a4c - see OI-128's correction.
    PR-403 went further: that helper is the engine's C++ runtime - class
    records carrying a universal base-constructor field (0x74bac, 481
    copies) - so of the three sites only 0x520e1 belongs to the row
    machinery at all.]

OI-112: which authored number a row's TIME is - the engine
  scales and gates on the value it scans FIRST, the port
  scales and gates on the value written SECOND
  status: OPEN (opened by PR-219)
  notes: PR-219 read the row object end to end. The ctor entry
    is 0x515e0 (the 0x515e2 PR-217 quoted is a 2-byte dump
    artifact): push esi/edi, call the node ctor 0x45770, then
    +0x8 = 0, +0xc = 0, +0x10 = 1, +0x14 = 0, the class word
    0xa2638 at +0x4, then the line parser 0x51660 on the
    pushed text. 0x51660's first act is a scan of the line with
    the format at 0x92115, whose bytes are `%hi` (its
    neighbours are `time` 0x92119, `,%hi` 0x92122, `sample`
    0x92127, `time_scale` 0x92131), writing the FIRST number of
    the row line straight into [row+0x10]; 0x51679-0x5168f then
    multiply that value by a factor taken from the caller's
    frame (`imul edx,ebp`) and divide by 1000 (`mov ebp,0x3e8`,
    `idiv ebp`) and store the result back at [row+0x10] - that
    is PR-110's `* time_scale / 1000`, the only such site in the
    image. And [row+0x10] is what the row step compares against
    the object clock at 0x52320-0x5232b. The SECOND value is
    handled at 0x51692-0x516d2: its magnitude goes to [row+0xc]
    with a sign flag at [row+0x14], and when it is ZERO the
    engine does not store 0 - 0x516af-0x516c2 reads a key
    string held at [scene+0x398] and looks it up through 0x44110
    (the same name tables 0x961fc/0x96200 the clause parser
    uses) and stores THAT at [row+0x8]. The clone 0x51622-0x51655
    copies +0x8/+0xc/+0x10/+0x14.
    WHY THIS IS A QUESTION AND NOT A FACT: the shipped row
    grammar puts an INDEX first and a TIME second - KADMAR.07
    `_0_0.MOV` line 121-124 is `time_scale=500` then
    `0,100` and `1,1000,gen K_GEN1`, and line 231 is
    `-1,200,I 100,0` (NEGATIVE first values exist, and they are
    PR-107's authored start-row numbers) - while the port parses
    `.frame` from the first and `.thresh` from the second,
    scales `.thresh` by time_scale (ported/ll_play.c:896,
    PR-110) and gates the row on `.thresh`
    (ported/ll_level.c:1506-1514, `o->row_t < f->thresh`).
    Either the engine's [row+0x10] is NOT the row's main gate
    (a delay, with the real gate inside the row-class verb the
    row-run verb calls at 0x5213d with edx = 6, which PR-219
    did not read), or the port's field mapping is mirrored.
    DO NOT change behaviour from this row until that verb is
    read: PR-110/PR-135's fixtures pin the second-number model
    and pass, and the row step's compare is the one place the
    first number is known to be used. The experiment that
    settles it is named in the PR-219 record: a KADMAR.07-style
    block with time_scale far from 1000 and a first number that
    is not 0, measured against the port's row clock.

    PR-220 resolves the FIRST half of this row and narrows the
    rest. The two callers of the row ctor are 0x51ac1 and
    0x51f54, both inside the state record's row-list builder
    0x51eb0 (called from the state ctor 0x517e0 at 0x517c9 and
    from the row rebuild at 0x52568). 0x51f54 is the one that
    builds authored rows: it passes edx = the row line, ecx = a
    string copy of it (0x57fd0 -> 0x76fa3) and ebx = the global
    0xceff4, and pushes `[rec+0xe]` as the ctor's scale factor
    (0x51f37-0x51f3e) - [rec+0xe] is the block's `time_scale`
    (written at 0x519c3 by the block-settings parser 0x51910).
    So time_scale multiplies the value scanned into [row+0x10],
    the FIRST authored number, and nothing scales the second.
    That gate is a compare, not a subtraction: 0x52320 tests
    `[row+0x10] >= [obj+0xa1]` and if the clock has NOT reached
    it the step runs nothing; when it has, the row runs with the
    OVERSHOOT `[obj+0xa1] - [row+0x10]` (0x52334) and the clock
    is not decremented. On KADMAR.07 `[A_DEAD]` (`time_scale=500`
    then `0,100`, `1,1000`, `2,400`...) the engine's gate is
    0, 0, 1, 2... on a monotonic clock while the port's is
    50, 500, 200... subtracted from the accumulator: the two
    agree only by accident of the authored first column counting
    0,1,2,3. The second number's consumer is still unnamed:
    PR-220 found no reader of [row+0x8]/[row+0xc] anywhere on
    the step path (the row's own +0x8/+0xc verbs are PR-218's
    chain delegates, its +0x10 is a third chain delegate).
    PR-220 also ruled out the obvious places: dumping the class
    tables (the node base 0xa1000, the row class 0xa2638, the
    record class 0xa260c) shows no verb reading [row+0x8] or
    [row+0xc]. Same PR named 0x52072, the record's `time=`
    bounds consumer, which answers PR-163's open half: with
    max != 0 it stores [rec+0xc] = min + rand * (max - min) /
    0x7fff (0x52081-0x520aa); with max = 0 - the shipped
    single-value `time=220` form - it stores min.
    PR-221 CORRECTS the premise of that search: 0x51660's scan
    has exactly ONE parse destination ([row+0x10], three pushes
    and one `add esp,0xc`), so the row ctor never parses a
    second authored number; [row+0xc] receives +2/-2/-1 with a
    flag (0x5169d-0x516d2) and [row+0x8] the caller's default
    (ds:0xceff4 at 0x51f48, or a settings-block name lookup at
    0x51a92) - not a row-line value. Shipped data: all 18617
    row lines carry two numbers, 0 carry one, the first column
    reaches at most |82| and walks up and down (ENA.03_ROZGW
    `-5,4` ... `-8,4` ... `-6,4`). Where the second number goes
    is now the question, and the row's LENGTH is separately
    explained without it: 0x52140 skips the list walk entirely
    while the chain answers 6, so the row is re-run (PR-135's
    bit-setting walk is the else branch, 0x52195-0x521d4).
    Candidates not settled: the clause/directive node family
    (which makes PR-218's "the clause selects the path, never
    refuses the row" worth rereading) and state kept in the row
    CLONE 0x520e1. DO NOT change behaviour on any of this: the
    port's `.thresh` model is unpinned by the engine, not
    refuted - its claimed source (`%hi` into node+0x10) is the
    FIRST number, and the second has no named engine site.
    PR-222 adds the negative datum that nothing COULD read it:
    a row line's third field is always a directive word from the
    engine's own keyword table (0x91847: `rev`, `zero_v`,
    `zero`, `zero_t`, `vol_music`, `smp`/`smp2`/`smp3`,
    `smpauto`/`smpauto2`/`smpprior`, `vol`, `smp_loop`,
    `smp_unloop`, `smp_stop`, `atc_on`, `atc_off`, `lock`,
    `lock2`, `unlock`, `unlock2`, `ground_on`, `ground_off`,
    `chdir`, `ldir`, `rdir`, `pos`, `goto`, `gotorel`,
    `goto_rel`, `I`, `F`, `IC`, `FC`, `M`, `MOVE`, `GEN`,
    `Wait`), an unknown word ends the tail (0x46c1c reports it
    and returns 0), and ZERO shipped row lines carry a bare
    number there. The row is re-run while one of ITS OWN
    directive nodes answers 6 (0xa1038/0xa1040 verbs at
    0x45dc0/0x45d90 write [node+0x8] into the caller's row
    cursor and answer 6), so no row duration field is needed
    by the engine at all. PR-222 also found where the rolled
    `time=` number IS used as an index: the record's row-reset
    verb 0x52360 (class +0x1c) rolls it through 0x52070 and
    writes the row cursor [rec+0x4] = k, or count-1-k when the
    state's 0x10/0x20 bits say so (0x523bc-0x523cb) - entering
    a state starts at a row chosen by that number, which is a
    row-sequence rule, not a row duration.

    PR-224 adds one named consumer of a row tail's argument,
    which shows the answer-6 mechanism working on data: the
    `end_level <n>` node 0xa1048 has 0x45d90 as its SIXTH verb
    (+0x14) and the chain walk calls exactly that slot -
    `*[ebx] = [node+0x8]` with ebx = the row cursor [rec+0x4],
    answer <- 6 - so the authored number is written into the ROW
    CURSOR and the row is re-run instead of advanced. The same
    number also reaches the scene's exit-word field through
    0x45750 (PR-194/PR-199). TUNGA.11's lion rows
    `10,6,end_level 1` ... `12,6,end_level 1` are that shape.

OI-113: the engine control bit table is read (PR-230) and the
  port's ctrl table lags it: CHECK_ALL must also set the
  CHECK_LN ground-rule bit (engine 0x1c0), and
  CHECK_BASE_LN/INV_V/BY_TX/SEQ_REV_X/SEQ_REV_Y have no
  entries or bits; separately the row-order word [obj+0x99]
  (reset 1 at 0x52377, negated by `rev` 0x459b0, backward
  cursor walk 0x521a1-0x521b4) and the ldir/rdir/chdir apply
  verbs writing the +/-1 direction word [obj+0x6d] are
  unmodelled - ZARIA.01's lion [A_WALK] uses bare CHECK_ALL
  and its A_DEAD row 181 and A_LOOKUP row 629 use rdir/rev
  status: OPEN (opened by PR-230)
  origin: PR-230
  notes: half one IS DONE (PR-231): ctrl_flag now sets the
    CHECK_LN rule bit from CHECK_ALL (engine 0x1c0), and the
    ZARIA.01 walk legs re-pinned to the authored slope and its
    edge at x=408. Half two stays open: no entries/bits for
    CHECK_BASE_LN/INV_V/BY_TX/SEQ_REV_X/Y; the [obj+0x99]
    row-order word, the backward cursor walk, and the
    ldir/rdir/chdir/rev apply verbs are still unmodelled.
    PR-245 adds to half one: the engine's CHECK_ALL
    (0x51c73, `or word [rec+0x12],0x1c0`) covers FOUR
    engine bits - ANI + ZW + LN + LEW - so the port's
    CHECK_ALL must also set its LL_CTRL_CHECK_LEW; that
    bit is the ask channel (PR-245), so the ask port sets
    it. PR-245 also corrects PR-239's letter list: BY_X is
    the DEFAULT name setting nothing, BY_Y = 0x1, BY_TIME =
    0x2 (0x51b56-0x51b8d) - the PR-230 table is right.
    PR-247 measures the walk itself: the candidate list is
    [[0xcef40]+0x64] (the global scene object), the layer
    test is [candidate+0x24] vs [requester+0x24] unless the
    CHECK_ALL_PLANE-masked bit is set, and the asked
    object's +0x60 is the plain [obj+0x2c] getter for the
    ladder too - the authored rectangle sits at +0x64 and is
    used by the broad phase, not by the ask.
    PR-249 WITHDRAWS the CHECK_LEW addition above: the
    keyword table pairs every name with its own OR, and
    CHECK_ALL is `or WORD PTR [esi+0x12],0x1c0` (0x51c73) =
    ANI|ZW|LN - CHECK_LEW (0x200) is a separate keyword
    (0x51c8f) that no ZARIA.01 lion state spells, though 535
    authored control lines in 205 .MOV files do. The port's
    ctrl_flag (ll_play.c:338, CHECK_ALL -> ANI|ZW|LN) is
    already exact; half one is CLOSED as measured, no code
    change. The dispatcher 0x52790 has three channels:
    CHECK_LEW -> the walk 0x4bd30, CHECK_ANI (0x40) ->
    0x4b890, CHECK_ZW (0x80) -> 0x4b870; the latter two walk
    the class groups at [ds:0xa94ec]/[ds:0xcef44] through the
    shared walk 0x4b500. CHECK_BASE_LN is 0x4000 (0x51ca9),
    not 0x400000; CHECK_ALL_PLANE is 0x8000 (0x51d2b).

OI-114: the OPERATION byte of a compiled transition condition was
  read end to end (PR-261), its K_ANY half ported (PR-263),
  and the operation itself ported (PR-264): one mask plus one
  op (0 subset, 1 disjoint, 2 equal, 3 intersects) tested by
  both evaluators against a composed live set - the port's
  AND-group grammar is gone from the match
  status: CLOSED BY PR-264
  origin: PR-261 (the mechanism itself is OI-111 items (1) and
    (2); what is new here is the compiler path, the corpus
    census, and the K_ANY consequence)
  notes: The chain, all read: the state block's transition line
    at 0x51e07 resolves the target state (0x53440), captures
    the `(...)` text (0x9225c, copied by 0x57fd0) and calls
    0x51e4d -> the thunk 0x49610 (`call 0x47d50`, then
    `mov [eax+0xd], ebx` = the target index, `mov [eax+0x11],
    ecx` = that captured text), and 0x47d50 calls the clause
    parser 0x47d60. 0x47d60 clears the op byte and the 12 mask
    bytes (0x47d73, 0x47d77), pulls tokens with 0x57d30 and
    the format ` %[^, \r\n\t]` (0x919bc), resolves names
    through 0x48000 - which has exactly ONE caller in the
    image, 0x47de5 here - and writes the operation for the
    whole mask: '!' -> 1 (0x47dd0/0x47dc8), '*' -> 2
    (0x47dc2), '|' -> 3 (0x47dbc); a ',' (0x47dfd) folds the
    next name into the same mask and leaves the byte alone. No
    second mask, no node tree. The 21 bytes are then pushed
    into the record's vector at [rec+0x1a] (0x51e54, `rep
    movs` 5 dwords + 1 byte).
    Corpus census (PR-261, script /tmp/opencode/pr261_census.py
    over the 777 .MOV texts): 409 texts carry transition
    clauses, 2764 clauses, 429 naming two or more signals. 87
    use '!' and NONE of them combines it with ',' or '|'; 47
    use '|'; 0 use '*'; 0 name K_NEVER. Exactly 7 clauses mix
    '|' with a ',' list - the only shipped rows where
    "any of A,B,C" (the engine) and "(A and B) or C" (the
    port) differ. K_ANY appears in 41 clauses, 40 as `=|K_ANY`
    and once as `!K_ANY`; PR-260 read that mask (signals
    35..39 + 53 = K_BKWD, K_FWD, K_UP, K_DOWN, K_FIRE,
    K_JUMP), which is "any key" exactly under op 3 and "all
    six at once" under the default op 0 - the corpus writing
    the pipe 40 times out of 41 is the data-side proof of the
    operation table.
    The port: parse_cond (ll_play.c:398) splits '|' into
    groups and ',' into terms with a per-term neg, and
    obj_cond_true (ll_level.c:825) ORs the groups. Equivalent
    on every shipped shape but those 7. The real hole is
    LL_GOP_ANY (set at ll_play.c:453, matched by the PR-218
    selftest only): nothing evaluates gop, so all 41 K_ANY
    clauses are dead rows. Two separable changes, each with
    its own leg: (a) carry the operation with the compiled
    clause instead of the group split; (b) test K_ANY as
    "intersects the input-key mask", whose six bits are the
    intent words the port already keeps (the horizontal word,
    PR-259's vertical word [obj+0xbe], and the fire/alt/jump
    flags of 0x54910's [obj+0xa5]).
    PR-263 does (b) and corrects the two sentences above that
    called the any-rows dead. THE MASK: the resolver's K_ANY
    branch (0x48029-0x48048) memsets the 12 mask bytes and OR-s
    the immediate 0x2001f8 into dword 1, which is signals
    35,36,37,38,39,40,53 = K_BKWD, K_FWD, K_UP, K_DOWN,
    K_FIRE, K_ALT, K_JUMP - SEVEN keys, not six: bit 8 of that
    dword is K_ALT, and PR-260 (f)/PR-261's "minus K_ALT" is
    withdrawn. The constant is bit-for-bit the set the input
    bridge 0x54910 raises from [obj+0xa5] bits 0/4/5,
    [obj+0xb6] bit 4 and the intent words [obj+0xba]/[obj+0xbe]
    (0x54931-0x549b4), so K_ANY names the input-key mask and
    not "some key is down". The sibling branch answers
    K_NEVER with `or ah,0x60` into mask byte 4 (0x4806f-0x4807d)
    = signals 37,38 = K_UP,K_DOWN - PR-218's "EMPTY data set"
    is wrong, and no model acts on it. 0x48000 never writes the
    op byte at +0xc (the `mov edx,1 / jmp 0x49543` at 0x48043
    is its resolved-return; 0x49541 is the not-found return),
    which leaves PR-261's "the clause parser owns that byte"
    standing. REACH, recounted: 41 any-clauses in 14 texts (40
    `=|K_ANY`, one `=!K_ANY`), every one of them in a block of
    the lion's shape - including KADMAR.07_0_0.MOV, whose
    blocks are [A_ZABLOK]/[A_DEAD]/[A_TURN_BACK]/[A_BORRING];
    no object-only text asks for one. THE PORT, and what was
    actually wrong: cond_true has answered the name since PR-87,
    but with ELEVEN keys - the seven plus k_f1..k_f4 (signals
    41..44, bits 9..12, outside the constant), so the lion's
    any-rows fired on a weapon-selector key the engine would
    not accept and its `!K_ANY` mirror refused one it would;
    and obj_cond_true had no case at all, so an object any-row
    was dead - with no shipped row behind it. Now: the mask is
    defined once as LL_SIG_K_ANY_MASK (ll_play.h, with the
    0x2001f8 provenance), the player's set drops the four
    selectors, and the object evaluator tests the intersection
    against the bits a `GEN <name>` row put in the set. Red
    legs: two play checks (`|K_ANY` must refuse K_F2,
    `=!K_ANY` must take it) and one object check (`|K_ANY`
    must take a `GEN K_FWD`), all three failing at HEAD. Still
    open in this row: item (a), carrying the operation instead
    of the group split - the 7 clauses that mix '|' with a ','
    list still read differently here - and the K_NEVER mask.
    PR-264 does that item (a) and closes the row. THE MATCH,
    followed into its arms: 0x47e40 reads the byte (`mov al,
    [eax+0xc] / cmp al,3 / ja` to the error word 0xa92d4,
    0x47e4b-0x47e50) and jumps through the four-entry table
    at 0x47e30. Arm 0 (0x47e63) ANDs the two sets through the
    AND method 0x49550 (dword loop 0x49559-0x4956a) and
    requires the AND byte-identical to the CLAUSE mask
    (`repz cmps` 0x47ec1): a SUBSET, every name it names is
    up. Arm 1 (0x47edc) runs the same AND and hands it to the
    empty-test 0x495b0 (0x47f35): DISJOINT. Arm 2 (0x47fb2)
    `repz cmps` of the two masks: EQUAL. Arm 3 (0x47f42) runs
    the AND, hands the copy to the empty-test 0x495b0 at
    0x47f9b and answers `sete` of it (0x47fa2): INTERSECTS.
    The op chain gains one detail: a token whose first char
    is alnum (the ctype-table
    `test BYTE PTR [edx+0xa68b4],0xc0` at 0x47da9) goes
    straight to the name path, so an operator is its OWN
    token; one byte per clause, last write wins. CENSUS,
    re-run: all 47 pipe clauses LEAD a comma list - the 7
    mixed ones are ENA.03:551/575 `|K_JUMP,K_UP`, ENA2:213
    `|K_FWD,AC_BREAK_NEXT`, KADMAR.07:660 `|K_GEN1,
    AC_BREAK_NEXT`, KADMAR2:288 `|AC_DUP,AC_TIME`, ZARIA.02
    LEW:119/LEWPL:116 `|K_GEN1,AC_BREAK_NEXT` - no '!'
    clause combines with a ',' and none uses '*', so the
    shipped corpus exercises operations 0 and 1 ONLY and the
    only rows whose reading changes are those 7. THE PORT:
    ll_mov_tr carries `op`, parse_cond writes it the way
    0x47d60 does (the row-clause capture passes NULL);
    ll_mov_tr_mask (ll_play.c) builds the clause mask - each
    name's bit, K_ANY's REPLACE, K_NEVER's union, an
    unresolved name nothing; cond_true and obj_cond_true
    answer with the four arms over a composed live set (the
    lion's p->sig plus the facing-relative keys plus the
    broke/ac_time bits, mirroring what 0x54910 and the rows
    put into [obj+0x60]; the object's o->sig plus the brk
    bit). Red legs: the `|K_JUMP,K_UP` play pair (K_UP alone,
    K_JUMP alone) and the object pair (`|AC_TIME,AC_EDGE`
    must take a lone raised AC_TIME; `=!AC_TIME,AC_ONAIR`
    must take a set holding only AC_EDGE) - all red at HEAD,
    green now. The whole fixture suite, ZARIA.01's climb legs
    included, is byte-identical: none of the 7 clauses is
    driven anywhere. The two empty-set questions this
    replacement makes visible are now OI-115; the K_NEVER
    mask has no model action (counted: no shipped clause
    names it) and stays recorded there.

OI-115: the transition pass was proven UNCONDITIONAL (the
  empty composed set still asks, PR-265) and [obj+8] is now
  named - the class behavior table, with its +0x8c collector
  and +0x54 method read (PR-266); what an EMPTY compiled mask
  matches, and the no-data operations stay open
  status: OPEN
  origin: PR-264 (the match arms, the mask builder and the
    operation port are OI-114, CLOSED; what is new here is
    the empty-set pair the replacement makes visible)
  notes: (a) CLOSED at PR-265. The pass 0x53520 has exactly
    ONE call site in the image, the DIRECT call at 0x5373c
    inside the step 0x536f0, and the step reaches it in a
    straight line: merge 0x53705-0x5370c, 0x523f0 + merge
    0x5371c-0x53725, `call [[obj+8]+0x54]` 0x53733, the
    pass, the apply 0x535a0 0x5374b, +0x54 AGAIN 0x53755,
    `[[obj+8]+0x28]` 0x5375d. NO test of [obj+0x60]
    between the merges and the pass, and the walk 0x52000
    asks EVERY entry (0x52020-0x5203a). PR-264's framing -
    that 0x53520's absence from the data sites left the
    path's reachability or a possible guard open - is
    resolved: the pass is called directly, not reached
    through a table, and nothing guards it. The port's
    `(o->sig || brk1)` guard had no engine counterpart and
    is removed. Red leg, the KADMAR.07 stairs shape
    (SCHODY_SCH01..03 line 10): an object's `=!K_GEN3` row
    is taken at its FIRST, empty-set step under the
    disjoint arm; the control, an unraised subset row, is
    not. REACH, counted: of the 87 shipped `!`-clauses, 74
    sit in `*_LEW*` texts, 10 in KADMAR.07_0_0.MOV (that
    level's PLAYER text, PR-263), and 3 - the three
    identical stairs files - in true scene-object scripts;
    none is driven and no shipped number moves (measured).
    The `A_FALL_DOWN(1)=!AC_DRAB` row named in this
    paragraph when the row opened is a LION row, and the
    lion side never had a gate (ll_play.c:1302-1303, one
    ungated cond_true call site).
    (d) CLOSED at PR-266. [obj+8] is the CLASS BEHAVIOR
    TABLE: a plain data array of code pointers (stride 4),
    installed at construction by a factory that strcmp's a
    class name, allocates, runs the base ctor 0x62e40 and
    stores the table at [obj+8] (and its base minus 8 at
    +0xc) - 408 such sites, 138 named to class strings
    (ARMATA, SEP2, TLewZaria, ...). The slots are
    compile-time data, which is why no code site writes them.
    For SEP2's table 0x96fd8: +0x28 = 0x52a90 (a frame/index
    computation), +0x2c = 0x53d80 (fills the object's own
    [obj+0x54] with a 40-byte sub-object - the writer
    PR-265 looked for), +0x54 = 0x55070 (three calls, NO
    test of the set - PR-265 confirmed at this level too),
    +0x8c = the signal collector. The collector is
    class-specific: 0x54910 in 207 tables - it raises 35/36
    from [obj+0xba] and [obj+0xb6] bit 4, 37/38 from the
    sign of [obj+0xbe], 40 and 53 from [obj+0xa5] bits 4 and
    5, and NEVER 39 K_FIRE - and 0x504e0 in the 18 TLew*
    tables, which calls 0x54910 and unions one of K_F1..K_F12
    chosen by [obj+0xee]. PR-265's (d) wording conflated two
    +0x54 slots (the state-record class's, whose ctors zero
    it, and the object's); see that record's CORRECTS. The
    F-family's source and the K_FIRE raiser move to OI-116.
    (b) a clause whose names are ALL in no table compiles to
    an empty mask, and the subset arm takes it VACUOUSLY;
    both evaluators now carry `if (!mask) return 0;` - the
    PR-88 convention kept, and counted: no shipped clause
    does it. (c) operation 2 ('*') and the K_NEVER mask are
    implemented but no shipped clause writes either; unit
    fixtures only.
OI-116: who raises the F-family (K_F1..K_F12) and K_FIRE, and
  on what state: the lion's collector 0x504e0 picks ONE of
  twelve signals from three object bytes and twelve globals,
  and this port raises F-keys from the raw keys instead
  status: OPEN
  origin: PR-266 (reading the +0x8c slot of the TLew* class
    tables; the name pool at 0x91a0e fixed the numbers)
  notes: (a) MEASURED. The TLew* tables (18: TLewZaria,
    TLewZariaB, TLewEna2, TLewKadmar2, TLewKara,
    TLewJunction, TLewTunga1) put 0x504e0 at +0x8c, not
    0x54910. 0x504e0: twelve tests of the form ([obj+0xa5]
    bits 0x2/0x40/0x80, [obj+0xa6] bits 0x1/0x2/0x40/0x80,
    [obj+0xa7] bits ... AND ds:0xcef5c, 0xcef60, 0xcef64,
    0xcef68, 0xcef6c, 0xcef70, 0xcef74, 0xcef80 ... != 0)
    each store an index 0..11 at [obj+0xee] (0x50514-0x50631,
    last match wins); `jmp cs:[eax*4+0x504b0]` then ORs bit
    41+i - K_F1..K_F12 - into a fresh set, and 0x54910 is
    called and merged (0x506d6-0x506ee). OPEN: what the three
    bytes are (they are the same bytes 0x54910 reads for
    K_BKWD/K_ALT/K_JUMP), who writes them, what the twelve
    globals hold (one writer: the indexed store at 0x39df4 /
    0x39e03), and what [obj+0xee] means elsewhere (the
    TLewZaria +0xf8 getter 0x1e2f0 tests it for 2 or 3).
    (b) K_FIRE 39 is in NO collector: 0x54910 raises 35, 36,
    37, 38, 40, 53 only, yet the texts name K_FIRE 56 times
    in ZARIA.01 alone. Some other writer of [obj+0x60] - or
    of the row-builder output 0x523f0 (0x5371c/0x53725) -
    exists and is un-named.
    (c) REACH, counted over the 777 texts: 199 active
    condition lines name K_F1..K_F12 (111/55/14/7 for F1..F4,
    and F6 1, F7 1, F8 2, F10 1, F12 7) in 12 files, all of
    them lion texts (10 *_LEW* files + KADMAR.07_0_0's
    player-shaped block; ZARIA.01's two texts 24 each). No
    scene-object text names one, matching (a). This port
    raises K_F1..K_F4 from the raw F1..F4 keys (ll_play.c:
    1108-1118) and never K_F5..K_F12, so those 12
    occurrences can never match here. No climb row in the
    ZARIA.01 chain names an F-signal, so OI-078's lead is
    unchanged.
OI-117: the answer verb's two rectangles - where the engine
  gets them, and why this port's verb answers nothing
  status: CLOSED BY PR-268
  origin: PR-267 (reading the DRABINKA class to its
    constructor)
  notes: RESOLVED by PR-268, and PR-267's reading was wrong.
    The blocks are ctor constants ONLY until the placed
    object's record verb runs: slot +0x18 of the class's own
    table is the record verb (it is the only slot in any of
    the 197 named tables that calls the scanf helper 0x49920),
    and the ladder/rope verb 0x3f0f0 reads both blocks out of
    the object's authored placement block - 0x49f90 with the
    format at 0x91d00 = `%d,%d;%d,%d , ` into
    [obj+0xb1..0xbd], then 0x3f124-0x3f14b adds the placement
    x,y, then 0x4a0b0 (`%d,%d`, 0x91d0f) reads a bare pair
    (a,b) and 0x3f159-0x3f182 builds [obj+0xc5..0xd1] as
    x0 = x1 = a, y0 = b-0x50, y1 = b, then 0x4a050 widens
    that to the authored column's width centred on a - a band
    inclusive 15..45 for `30,50` with the column 23..53 -
    also made absolute. ZARIA.01's own text authors
    `23,30;53,160` and `30,50` after
    `DRABINKA,drab.ani,drab.mov,180,510`, i.e. the column
    (203,540)-(233,670) and the edge band (195,480)-(225,560)
    - and this port already parses those numbers
    (PR-243/PR-246/PR-247); it just does not hand them to
    `ladder_answer_core`, which builds both roles from
    `blk_of_obj` - the object's current art box (ll_level.c
    :5204). The consequence is in OI-078. Nothing in the suite
    moved while this row was open or closed: both in-game asks
    are pinned `-> no` and print the 0x44ec0 contact-point
    test, not this verb. FOLLOW-UP, not a reopen: PR-269 feeds
    the column (o->arx0/ary0/arx1/ary1) to P2/P3 - leg L11 -
    and PR-270 models the edge band from the `a,b` line too
    (place_pair stores the absolute pair, ladder_answer_for
    widens it per 0x4a050, leg L12) - the follow-up is done;
    only the pair-without-rectangle n (unwritten [obj+0xb1])
    stays unmeasured, noted in OI-078's next record.
OI-118: 0x523f0 raises signals through a SECOND object: the
  per-step pass calls [[obj+0x00]+8]+0xac and +0xa8
  (0x52416/0x52423), and no constructor in the image installs
  a behavior table at [obj+0x00]. PR-292 locates a CALL SITE
  of this row's 0x523f0: the per-object frame step 0x536f0
  calls it at 0x5371c with the current record instance
  `[[obj+0x50]+[obj+0x5c]*4]` and a stack set, and OR-s the
  answer into [obj+0x60] at 0x53725 - one of two such OR-s in
  that step, the other being its +0x8c verb's set at 0x5370c.
  The raise belongs to every object's frame step, before the
  transition pass 0x53520 clears the set (PR-262) and calls
  the ENTER verb named in OI-078.
  status: OPEN
  origin: PR-268 (walking the class factory and the +0xa8
    verb owners)
  notes: The port has always read that pair of calls as the
    object's OWN table - 0x96960+0xa8 = 0x50720 is the lion's
    invulnerability countdown decrement (PR-105), and 0x9a778
    +0xa8 = 0x27e40 is CAMEL/ZOLW's AC_EDGE+K_GEN1/K_GEN2
    raiser - and those slots do hold exactly those functions,
    so the reading has been harmless so far. But the machine
    code loads the table through [obj+0x00], the only such
    dereference in the step path, and no factory or ctor in
    the image ever writes [obj+0x00]: [PR-288: it is NOT the
    only such dereference - the row walk 0x520c0 reads the
    same field at 0x520e4-0x520ef (`mov ebx,[esi]`,
    `mov edx,[ebx]`, `mov ebx,[ebx+4]`) and its 0x52103-
    0x5210f call goes through that table's +0x08 verb with two
    dwords in edx/ebx. PR-288 called the buffer those dwords
    land in a position; PR-289 corrected that - the buffer is
    the caller-side buffer of the world-query SET that the
    record class's +0x28 verb 0x52790 fills, and 0x5282f-
    0x52849 overwrite those two dwords wholesale. So the field
    is read twice in the path that runs every record step, and
    passed as ebx to the three scene queries 0x4bd30/0x4b890/
    0x4b870; what installs it is still unfound.] the three base ctors
    (0x52860, 0x531f0, 0x53ff0) and all 218 named classes
    leave it alone. Either every object self-points through a
    path this read did not find, or the +0xa8/+0xac verbs run
    on some other object and the port's +0xa8 uses (the
    countdown, and any future signal raise from that pass)
    need re-reading. OPEN: find what writes [obj+0x00]. The
    question is sharper because the engine DOES nest objects
    inside objects with their own tables: the base ctor 0x53ff0
    ends with `add eax,0xd6` and 0x54004-0x5400b stores a
    zeroed point object at [obj+0xd6]/[obj+0xda] with the
    shared table 0x98278 at [obj+0xde] (the same three stores
    are repeated by the other default initializer 0x54030 at
    0x5403c-0x54045), 0x54e0a refreshes
    its x,y from [obj+0]/[obj+4] every step, and 144 of the
    204 class tables hand that object out as their +0x68 verb
    (0x23900 = `add eax,0xd6; ret`), 0x98278's +0x8 verb
    being the point-in-rectangle test 0x23e00. So a second
    table at [obj+0x00] is exactly the shape the engine uses
    elsewhere - but nothing installs it. PR-293 reads the
    field AS AN OWNER POINTER in the only place that matters:
    the set composer 0x523f0 takes `[rec+0x00]` at 0x5240f,
    loads `[[rec+0x00]+0x08]` as a CLASS WORD at 0x52411 and
    calls that table's +0xac and +0xa8 verbs (0x52416/0x52423)
    with the composing set; +0xac in the 0x97ca8 block is the
    ENTER verb 0x3f1c0 of OI-078. So the record's [rec+0x00]
    and the object's own dispatch reach the same verb, and the
    two class words are one table - inferred from that offset
    arithmetic, still not observed, and what installs the
    pointer is still unfound.
    PR-297 FINDS THE SAME ABSENCE AT A THIRD PLACE, and it is
    systematic: a whole family of class tables lives in the
    block 0x9fd38..0xa0744 (repeated 68-slot tables, stride
    0x110, identical layout - 0x45320 at 0x9fd58/0x9fe68/
    0x9ff78, 0x45120 at +8, 0x23e00 at +0x14, a fourth
    instance at 0xa03b0), and the image contains NO store of
    any of those table addresses and no code immediate naming
    them, so the objects that use them get their class word
    from computed values. Any answer to this row has to
    account for those instances too, not only for [obj+0x00].
    PR-301 ANSWERS THE INSTALLER HALF, AND THE ABSENCE WAS AN
    ARTEFACT. `mov DWORD PTR [<reg>+0x8],0x9...` occurs at 408
    sites installing 188 distinct values, 176 of them code
    tables in 0x91000..0xb0000, and a constructor is exactly
    the shape 0x3caf0-0x3caf5: `call 0x53ff0` then `mov
    [eax+0x8],0xa0150`. So the class word IS a plain immediate
    store wherever a class is constructed directly, and a class
    is identified by its constructor sites. The addresses
    PR-297 searched for are absent because they are not class
    words: PR-300's anchors sit 0x2c BELOW the class word that
    addresses the same bytes (anchor 0x997fc vs the installed
    0x99828). What still has no immediate installer is the run
    at 0x99fe8 that holds 0x287a0, and [obj+0x00] itself.
    PR-303 ADDS A NEAR MISS THAT IS NOT THE ANSWER. Factories
    do install a second table by immediate store - every one of
    the 177 name branches writes [obj+0x0c] beside [obj+0x08]
    (for WSPRECT: 0x2967f installs 0x99ff0 at +0x8 and 0x29686
    installs 0x99fe8 at +0x0c) - but that is the +0x0c field,
    not [obj+0x00], and no site in that census writes +0x00.
    What the +0x0c table is for is now an open item of its own.

OI-119: what the object field pair [obj+0xc2] / [obj+0xc6] is -
  the value the AC_DRECT ask verb gates on
  status: CLOSED BY PR-326
  origin: PR-318 (the F4 read of OI-018's raisers)
  notes: The DRECT ask verb 0x40d50 refuses to answer unless
    `[asker+0xc6] > 0` (0x40d65-0x40d6d), so ZARIA.01's exit
    chain cannot be walked to its end without knowing what that
    word means. Measured here, as a census of the image: the two
    words are always written as a pair. The object command
    dispatcher 0x54560 assigns both from the request
    (0x54593/0x5459c, the opcode-7 case), adds to both
    (0x545aa/0x545bb, the opcode-6 case) and zeroes both
    (0x545f4/0x545fe); the same pair is zeroed at every state
    start (0x5042e/0x50438, right after the `[class+0x4]` state
    call of PR-238's 0x50420 block) and zeroed then copied
    +0xc6 -> +0xc2 at 0x27bde/0x27bee, in the window that reads
    the float position shadow at [obj+0x122] and writes the
    deltas [obj+0xca]/[obj+0xce] into the point at [obj+0x14a].
    Readers of the pair: 0x27366/0x273d6, 0x3674b/0x367bb,
    0x5020c/0x5025c, 0x50753/0x50764, 0x4cb28/0x4cb5d (both by
    `lea`), and this gate at 0x40d65. What is NOT decided: the
    unit (a position offset? a distance since leaving a surface?
    a frame count?), and whether the lion's own value can be
    non-zero at all while it stands on a platform. That question
    is load-bearing for the exit chain of the level this project
    is aimed at, so it is a row, not a counted item.
    PR-319 ANSWERS THE FIRST HALF: IT IS A COMMAND-WRITTEN (x,y)
    OFFSET, NOT A PHYSICS FIELD AND NOT AN AIR COUNTER. The engine
    posts commands to an object through 0x52b10 (eax = the object,
    edx = the opcode, ebx/ecx = two ints; it builds a 4-word
    message, [msg+0x4] the opcode and [msg+0x8]/[msg+0xc] the
    ints, and calls the object's class +0x10 verb). The base +0x10
    handler 0x54560 maps opcode 2 = ASSIGN the pair, 0xd = ADD to
    it, 8 = ZERO it, 6 = write [obj+0x6d]/[obj+0x71], 3 and 4 =
    set bits 2 and 3 of [obj+0xb6], 0x1b = the [obj+0xa9] surface
    attach/detach; everything else falls to its default 0x53770.
    The LION reaches that handler: his class 0x96874 has +0x10 =
    0x3d8f0, which handles opcode 5 itself and chains the rest to
    0x54560 at 0x3d906. The pair is never written by the physics
    step. Its readers are the AC_DRECT gate (a boolean `> 0`) and
    two state-indexed point-provider verbs, 0x27350 (installed at
    +0xe8 of the class 0x9a54c) and 0x36732, which return the
    asker's +0x68 provider's point plus `[obj+0xc2] * 15 / 100`
    plus `[obj+0x6d] * 60` plus `[obj+0xfe]`. WHAT REMAINS: which
    of the poster sites can run against the lion - opcode 2 is
    posted from 0x1e952/0x1f3ce/0x3e03d/0x3e446/0x3e840, opcode 8
    from 0x1f3b6/0x3e01e/0x3e364/0x3e424/0x45a84, opcode 0xd from
    0x1ea65/0x1eaef/0x3c3b8/0x53ba4 (the 0x3exxx ones are in the
    contact/attachment family, 0x53ba4 inside the two-object
    comparison 0x53ae0, 0x45a84 in a `.MOV` directive apply) - and
    the pair's UNIT, which those four sites decide.
    PR-320 MEASURES THE PAIR'S FULL LIFECYCLE. (i) STATE ENTRY
    ZEROES IT: 0x54e00 - called from 0x1ee03/0x1f0fa/0x3dd23 and
    from the state-init family at 0x50392 - syncs the presented
    point and zeroes [obj+0xa5], the intents [obj+0xba]/[obj+0xbe],
    this pair with [obj+0xca]/[obj+0xce], the attach [obj+0xa9],
    then sets the defaults [obj+0xad]=100/[obj+0xb1]=22 and clears
    [obj+0xb6] except its top two bits. So the AC_DRECT gate's
    `[lion+0xc6] > 0` must be satisfied WITHIN the asking state,
    by one of the opcode 2/0xd posters. (ii) A DECAY FUNCTION
    0x54860 is called from the per-object physics updates (its
    listed callers run 0x28377/0x28547/0x2c634/0x2fc46/0x30036/
    0x32a9c/0x3bbe8/0x3d807...): its eax is the object and its
    edx a four-byte parameter block whose identity is open; if
    parameter byte [ecx+0x2] carries 0x4 the pair is untouched,
    else a positive [obj+0xc6] goes to floor(-2*c6/3) when both
    [ecx] carries 0x10 and control byte [rec+0x13] carries 0x10,
    and to 0 otherwise, with further zeroings gated on [ecx+0x3]
    bit 2 (when the value is negative), [ecx] bit 2 and [ecx]
    bit 1. (iii) It is compared at 0x5476f
    (`cmp [obj+0xc6],0x58c`).
    (iv) The +0x48 getter the lock trio calls descends the CURRENT
    ROW: record [[obj+0x50]+[obj+0x5c]*4], its row-node vector
    [rec+0x2e] indexed by the row cursor [rec+0x4], then
    [row_node+0x8]/[row_node+0xc] and the array at
    [[row_node+0x8]+8] via the 0x229c0 pointer helper; the sibling
    +0xa8 verb 0x23940 returns the index word itself. WHO WRITES
    [row_node+0x8]/[row_node+0xc] is this row's open half, with
    0x53ae0 the open half on the unit.
    PR-326 CLOSES THIS ROW. THE PAIR IS THE VELOCITY REGISTER, x at
    +0xc2 and y at +0xc6, in 1/100 px per iteration. The physics step
    0x54250 integrates it every pass - force `([rec+0x0]*[obj+0xba])/20
    + [obj+0x6d]*[rec+0x10]` added at 0x542c7 and
    `([rec+0x4]*[obj+0xbe])/20 + [rec+0x14]` at 0x542fa, drag
    `v*[rec+8]/100` subtracted with a sign-flip kill at
    0x54300-0x54337, then the ceiling ramp through 0x54170 - and it
    ends by storing `v/100` into the step words [obj+0xca]/[obj+0xce]
    (0x54365/0x54372), which is the unit proof; 0x5476f's
    `cmp [obj+0xc6],0x58c` (14.2 px per iteration) is the second one.
    PR-319's "never written by the physics step" is corrected: the two
    adds above sit inside that body, and this port's ll_play_v_step is
    the same arithmetic, so the register the gate reads is normally the
    integrated velocity. THE GATE: 0x40d50 tests ASKABLE, then
    `[asker+0xc6] > 0`, then the geometry; 0x40dc0 (DRECT2) is the
    SAME verb without the velocity test and 0x40cf0 (RECT) is the
    geometry alone. AC_DRECT is therefore AC_DRECT2-plus-falling
    (positive y is down, PR-99). What this row still holds open, in
    PR-326's OPENS and none of it load-bearing for the exit leg: the
    0x54250 parameter block's remaining words and its class +0xbc
    guard, which classes reach 0x54250 (the call sites
    0x24731/0x321fe/0x33348), the 0x54860 block above, and the
    surface words [obj+0xa9]/[obj+0xe6]/[obj+0xe2] that decide when
    AC_ONAIR, AC_DUP and AC_DUP_CD fire.

OI-120: the `s <seq>` row field names a SEQUENCE - the art that
  row's frames come from - and the port reads the field without
  acting on it
  status: OPEN
  origin: PR-330 (the .MOV line-cursor read)
  notes: The engine's state-block reader 0x51910 has a field arm
    for the token `s` at 0x92146. Its name scan is
    " %[^ ,]%*[ ,]" (0x92148) at 0x51a6a; the name is resolved
    through the record table at 0x51a7b-0x51a95, and the row
    record built at 0x51ac1 keeps it at [row+0x8] on every frame
    path (0x5169e negative, 0x516cf positive, and for an authored
    frame 0 the record comes from 0x44110 at 0x516af-0x516c2) next
    to the frame at [row+0xc] and the
    units at [row+0x10]. A ROW can therefore name the sequence
    its frames come from, while the state's own `seq=` line names
    only the default. PR-330 models the row - frame, units,
    tails, the same threshold rules - and counts the switch as
    ll_mov_st.seq_switch, which the object census reports; no
    part of the port changes the art from it. Measured over the
    777 extracted .MOV texts: 206 such lines, 26 of the files,
    36 states, in two written forms - `s name 1,300` (164) and
    `s name,1,200` (42) - the lion's A_FALL_DOWN and A_TO_BOR in
    ZARIA.01, ZARIA.02, TUNGA.11 and TUNGA2.12, KARA.07's
    A_PUT_BOMB (27 rows), LONA.05's T_TELEPORT trio and
    A_WSP_LIN_DOWN2 (42), ENA.03's A_BORING4 (24), the six
    KADMAR.07 ninjas' A_DEAD, the three SCHODY A_RUN and seven
    SADES.06 objects' A_DEAD.
    WHAT WOULD CLOSE IT: the consumer of [row+0x8] - which draw
    path reads it, whether the row's record replaces the state's
    sequence for that row's frames or only for the element list
    it carries, and how it composes with [row+0x14], the
    sign flag the engine writes at 0x51696 and this
    port draws mirrored. Until that is read, a row that switches
    sequence mid-state is drawn from the state's art: the frame
    NUMBER and the row's timing are right, the art is not. It is
    a row and not a counted item because it spans the play work:
    it is the draw half of what PR-330 read, and the 36 states
    include the states of the level this project is aimed at.

OI-121: the ground query has no SURFACE IDENTITY in this port, so
  only the two edges of the engine's surface-link tail are
  modelled - and the authored rows that wait on the rest stay
  unrun
  status: OPEN
  origin: PR-331 (the AC_ONAIR/AC_DUP surface-link raises)
  notes: The engine's physics body 0x54250 ends with a tail at
    0x54760-0x54803 that reads the query result [obj+0xe6] and the
    held surface [obj+0xa9]. No surface: if one was held, call its
    detach ([sur+0x0]+0x4), clear the slot, call the class +0xb8
    verb, and in BOTH cases raise AC_ONAIR (0x54800 `or BYTE PTR
    [edi+0x2],0x4`, bit 18). A surface: from no attachment, raise
    AC_DUP (0x54785 `or BYTE PTR [edi],0x10`, bit 4) and attach;
    SWITCHING from one surface to another detaches and attaches
    with NO signal; and the kind word the query leaves at
    [obj+0xe2], if not 0, posts AC_DUP_CD every step (0x547d1, bit
    13). A kind-2 surface with the object's OWN y velocity register
    [obj+0xc6] at or over 0x58c is treated as NO surface at all
    (0x54766-0x54779): falling fast, it does not hold you. PR-331
    models the pair this port can see: AC_ONAIR while the polyline
    query fails, AC_DUP on the attach edge. What it cannot model is
    the IDENTITY the tail switches on: the port's ground is a .plt
    polyline with no surface object, no kind word and no detach
    verb, so the SWITCH
    arm, AC_DUP_CD and the kind-2 exception have no input; and a
    surface ASK that raises AC_DUP (0x3df00 -> the asked's +0xd8
    verb -> 0x45320 with [asked+0xee], PR-275) is a second
    producer no port code calls. Two shipped consequences, both
    measured in PR-331's own re-pinned legs. (a) At the foot of
    ZARIA.01's ladder the port lands in A_WSP, whose row list has
    no AC_BREAK_NEXT line, and the PR-86 fallback in ll_play.c
    hands the state to A_DRAP; the authored line for that moment is
    A_WSP's `A_TURN_BACK(1)=AC_DUP,K_DOWN` - 3 of the 777 texts
    author it, ZARIA.01 among them - and it needs the ASK-raised
    AC_DUP, so it never runs. (b) A_SCIAG's entry rows (`A_WALK`'s
    and `A_DRAP`'s `A_SCIAG(1)=AC_EDGE,K_DOWN`, 6 texts) need a
    state that survives mid-air inside the edge band; with AC_ONAIR
    posted every airborne iteration and `A_FALL_DOWN(1)=AC_ONAIR`
    in those same blocks, that window is one iteration, and the
    only leap that reaches the band - the ladder jump - is ended
    one iteration in by the jump states' own
    `A_WSP(3)=AC_DRAB_CATCH,AC_ONAIR` row (5 of the 777 texts
    author it in A_JUMP, A_FALL_DOWN and A_JUMP_S). The PR-278
    A_SCIAG port work - the state and its `m 0,1` per-admit law -
    is unchanged and untested end to end now. WHAT WOULD CLOSE IT:
    a surface record with identity and kind in place of the bare
    polyline (the object asks, the answer names the surface), the
    detach and +0xb8 verbs, and the [obj+0xe2] kind word read from
    the shipped data; then the ladder-foot landing, the A_SCIAG
    route and the AC_DUP_CD and fast-fall rules become testable.

OI-122: the three class tables that hold the seat verbs are named
  by no pointer and no immediate in the image, so no shipped
  object is built from them
  status: CLOSED BY PR-338
  origin: PR-336 (the seat-ask chain read)
  notes: PR-336 read the tables slot by slot. 0x95c38 holds
    +0x18 0x1e510, +0x78 0x1e570, +0xa4 0x1e5c0; 0x95d1c holds
    +0x18 0x1e630, +0x78 0x1e970, +0xa4 0x1e690; 0x95ddc holds
    +0x78 0x1ea90. That is the family's real verb layout, and it
    confirms the slot map PR-334 used: 0x4b5b7 calls
    [[cand+8]+0x78] with the requester in ebx, so +0x78 IS the
    ask. The three carry a signature shared with 20 tables that
    ARE installed: +0x4 0x23ea0, +0x8 0x23e00, +0x10 0x53770,
    +0x84 0x23a90, +0x88 0x534c0, +0x8c 0x23a60 (the set with
    +0x10 = 0x53770 and +0x88 = 0x534c0 is 23 tables, 0x96cdc,
    0x9907c, 0x9c168 ... 0xa1498, plus these three) - and
    0x534c0 is no event verb: it allocates 0x48 and calls
    0x51770. The shipped seat objects are built from OTHER
    tables: the installer chain 0x1fb32/0x1fc08 names the tokens
    at 0x90738 'KATAPULT' and 0x9074a 'PALMA' and stores
    [obj+8] = 0x96d1c and 0x96c38, whose +0x78 slots this row
    read as 0x20230 and 0x52570 - no seat ask - and whose +0x88
    it read as the event verb 0x54560. (PR-338: both readings are
    one page early; at the base the installer uses, those +0x78
    slots are 0x1e970 and 0x1e570 and both +0x88 slots are
    0x534c0.) Right
    before 0x95c38, at 0x95c1c-0x95c34, the image lays the
    surface family out in one row: 0x45250, 0x452d0, 0x453d0,
    0x45320, 0x45400, 0x45120. And NOTHING points at any of it:
    no dword of the image holds 0x95c38, 0x95d1c or 0x95ddc; no
    .text instruction loads any address of that 0x95a00-0x96000
    blob as an immediate (the imm index filtered to prov='code'
    stops at 0x95b49); the blob has no printable byte at all;
    and the only code naming it is the guarded pair at
    0x74980/0x74bb2, passing 0x95c20 and 0x95c40 to 0x77bdc,
    which calls 0x7be36 and, when that answers 0, 0x7b8f1 - the
    static-object init pattern, not a class install, and unlike
    the eight rectangle classes PR-328 found stored by immediate.
    WHAT WOULD CLOSE IT: the install path of this family - a
    table of class pointers built at run time, a computed base,
    or a second installer for these tokens (the level-object path
    PR-270 models is the other half of that question). Until it
    is found, no shipped object can answer a seat ask, and the
    triple [obj+0xe2]/[obj+0xe6]/[obj+0xea] has no producer.
    PR-337 CORRECTS WHAT THIS ROW WAS TAKEN TO MEAN, AND PR-338
    REDRAWS THAT CORRECTION. The three tables are the shipped seat
    objects' own class views (below), and the surface machinery has
    a second verb family too: 0x3dd60 (read) / 0x3dcb0 (move by a
    message whose opcode word is 1) / 0x3ddf0 (anchor) / 0x3df00
    (ask), hanging off an inline header at [obj+0xee] - [obj+0xf2]
    the record block, [obj+0xf6] the count, records 13 bytes with x
    at +0 and y at +4 - not off the [obj+0xa5]/[obj+0xb1] fields
    of the 0x1exxx seat verbs. 65 of the 327 factory name branches
    install classes that resolve one of the fifteen verbs of that
    family and the seat and rectangle families, so many shipped
    object types read a surface table (the corrected census is in
    the PR-338 record); PR-337's '61 of the 335' came from the
    shifted read below and is void. 131 of the 160 objects the
    shipped .des set gives a surface record line belong to those
    types, and of the 29 that do not, ZARIA.01 has none - both of
    its surface-line objects, 'KATAPULT' and 'PALMA', are in the
    first group, which is what OI-124 asks about.
    PR-338 CLOSES THIS ROW: IT WAS AN ADDRESS ERROR, NOT A GAP.
    Every address in this image is file offset + 0x10000 -
    tools/re/leodb.py builds the index with `--adjust-vma=0x10000`
    and its dword/str/hex readers all use va - 0x10000 - but the
    table read that produced this row used a 0xf000 base. Its
    '0x95c38' and the bytes the installer names '0x96c38' are the
    same bytes: 0x95c38 - 0xf000 = 0x86c38 = 0x96c38 - 0x10000.
    So the three 'unnamed' tables ARE the shipped seat objects'
    own class views, and they resolve the seat verbs at exactly
    the slots this row named: 0x96c38 +0x18 = 0x1e510, +0x78 =
    0x1e570, +0xa4 = 0x1e5c0, +0xfc = 0x1e630 (the PALMA view,
    stored at [obj+8] by the 'PALMA' branch at 0x1fc3b) and
    0x96d1c +0x18 = 0x1e630, +0x78 = 0x1e970 (the KATAPULT view,
    stored at 0x1fb83). The search found no pointer to 0x95c38
    because the name is 0x96c38, and that one is stored by the
    factory branch that builds the level object. Nothing is left
    to explain: the seat family is installed, on the two seat
    objects of ZARIA.01.

OI-123: what the 0x1b attach attaches, and why no shipped seat object
  and no ZARIA lion takes it - the held-surface slot [obj+0xa9]
  status: PARTIAL
  origin: PR-336 (the seat-ask chain read), answered in part by PR-343
  notes: [obj+0xa9] is the surface an actor holds. PR-336
    enumerated every writer over the full instruction index: the
    physics tail copies it from the query result [obj+0xe6] at
    0x5478e/0x547bc and clears it at 0x547f0/0x54e6a/0x1f8e9,
    and apart from those it is written at ONE place - 0x54624,
    inside the verb 0x54560's case `[arg+4] == 0x1b`. That case
    is a full attach: 0x5460e calls the held surface's +0x4
    (detach) when one is held, stores [arg+8] - the message's
    payload dword IS the surface object - and calls its +0x0
    (attach) when non-null.
    PR-343 READS THE MECHANISM AND VOIDS THIS ROW'S CENTRAL CLAIM.
    The verb is CLASS SLOT +0x10, and a message is queued, never
    called: 0x52b10 links a {0, value, argB, argC} node onto the tail
    of the list at [obj+0x10] (0x52b13-0x52b3f) and the drain 0x52b50
    hands each node to [[obj class]+0x10] (0x52b67) and frees it;
    0x52b80 runs that drain and is called from the object walk
    0x4ba90. 0x54560 is the BASE of the verb, at +0x10 of 81 of the
    176 ctor-pattern views and at +0x88 of none - this row's '87
    tables at +0x88' is wrong on both counts. Its value table is 2,
    3, 4, 6, 8, 0xd and 0x1b, everything else to 0x53770, and the 20
    direct calls of it are subclass fall-throughs and forward thunks,
    not posts. The named-message dispatch this row found at
    [class+0x84]/[class+0x88] is a different, real namespace; it is
    not what carries 0x1b.
    NOBODY WRITES THAT OPCODE IS FALSE: five sites post it. 0x1ea52
    is SPLYW's ask verb (the view 0x96ddc at +0x78) with payload 0;
    0x1eadc is STRUMIEN's (0x96e94 +0x78), payload 0; 0x36533 is the
    +0xe0 verb of the lion class 0x9f170, payload 0; 0x45d28 is a
    method stored only at the data slot 0xa11d0 and posts to the
    object at [edi]; and 0x2c252 is the +0x90 verb of TLewKadmar2 -
    the only one carrying a payload, the best-scoring entry of the
    pair of scans 0x4b6d0/0x4b760 run over [0xcef40+0x74], which is
    OI-127. The other two
    `mov edx,0x1b` sites this row named, 0x4a1fd and 0x54ec8, write
    0x1b as a payload, not a value word, as it said.
    WHO CAN TAKE IT: PALMA's 0x96c38, KATAPULT's 0x96d1c and SPLYW's
    0x96ddc hold 0x53770 at +0x10 and STRUMIEN's 0x96e94 holds
    0x52bb0; 0x53770 answers value 0x10 only with the constant
    0x8138764 and otherwise calls 0x52bb0, whose table handles 1 (a
    move), 0xe and 0x21 and returns every other value UNHANDLED. So
    no seat object ever takes the attach. ZARIA's own lion does not
    either: TLewZariaB 0x97a98 holds 0x1e130 at +0x10 and that verb
    forwards nothing. Five lion classes - 0x98f34, 0x9b4d4, 0x9f540,
    0xa0624, 0xa2218 - hold 0x50d60, whose table covers values
    9..0x15 and forwards the rest, 0x1b included, to the base.
    Also corrected: the ask reads the held surface at [obj+0xa9]
    (0x45471) and waives its negative-clearance rejection per record
    when the asked surface IS the held one (0x45479 `cmp esi,ebx; je
    0x45481`); there is no first test at [obj+0x89], and 0x45400's
    real first test at 0x4540c compares the FIRST RECORD'S FLAG BYTE
    of PR-335 with the ask's mode dword.
    WHY IT MATTERS: the attach exists, is reachable, and is written
    only by a lion-class verb under a state gate - so the held-surface
    half of the seat chain is real but belongs to KADMAR II, not to
    ZARIA.01, and PR-336's measured rejection at shipped coordinates
    still stands as the reason ZARIA's chain does not carry the lion.
    PR-345 REMOVES THAT CONDITIONAL. The same slot is written on the
    standing path by the class ground-contact verb +0xAC, which every
    lion class and KAMIEN install as 0x546a0 and which copies the query
    answer [obj+0xe6] into [obj+0xa9] each step (0x5478e, 0x547bc) -
    the 0x1b message at 0x54624 is the scripted exception, not the
    only door. The seat classes' own +0xAC is 0x23930, a bare `ret`,
    so a seat never holds a surface for itself, and the question for
    this level is no longer whether a message is posted but WHICH
    SURFACE the lion's standing query resolves to: if it resolves to a
    seat's own handle at that seat's obj+0xa5, the waiver 0x45479 is
    live and the negative-clearance rejection of PR-336 stops applying
    to that seat.
    WHAT WOULD CLOSE IT: what the entries at [0xcef40+0x74] are and
    what their +0/+4/+0x14 verbs do (OI-127); whether the state gate
    of 0x2c1cd-0x2c1ef - held surface nonzero, state index 1, the
    flag bit 0x40 at [obj+0x64] - ever holds in a shipped level. PR-347
    answers the first clause: that array is the belt record's tool
    list, not terrain, so this path is the lion taking a tool and it
    is not the carrier of this chasm. Two
    of the questions this row raised are now answered: the flag-byte
    test at 0x4540c is INERT, all six askers passing the mode 2 while
    0x45120 never writes a flag above 2, so no shipped table is
    rejected before the bracket test and no census is owed (PR-344);
    and the 0x1b payload is a surface handle, not a surface object
    (PR-344, OI-127). What is left is whether ZARIA.01 owns any
    attachable surface at all.

OI-124: ZARIA.01's first chasm has no measured carrier
  status: OPEN
  origin: PR-337 (the surface-verb install census)
  notes: HANDOFF section 8 T5's F6 paragraph named the ANIPL2
    seat-line chain as the authored crossing of ZARIA.01's first
    chasm: the KATAPULT pad (307,633)-(340,633) and the three
    PALMA spans, a rising chain ending 28 px short of the exit
    column at x 1020. PR-337 claimed the other half - that those
    two objects' classes hold no surface verb - and PR-338 VOIDED
    that: it came from reading the class block 0x1000 early (the
    same error as OI-122). Read where the installer points, the
    'KATAPULT' view 0x96d1c resolves the table reader 0x1e630 at
    +0x18 and the ask 0x1e970 at +0x78, and the 'PALMA' view
    0x96c38 resolves 0x1e510 at +0x18, the gated ask 0x1e570 at
    +0x78, the row-admit (0,+-1) table move 0x1e5c0 at +0xa4 and
    0x1e630 at +0xfc. The seat chain IS the mechanism the shipped
    game has at that place. What still keeps the crossing un-
    modelled is the rest of PR-336's read, unchanged by this:
    0x45400 grants its waiver only when the surface being asked
    about is the one the requester already holds - [obj+0xa9], read
    at 0x45471, tested per record at 0x45479; this row's "first
    test ... at the requester's own held anchor [obj+0x89]" was
    wrong on the field and on the position of the test, PR-343
    read both - and at ZARIA.01's coordinates it
    rejects the lion at every position the level can put him in;
    the only writer of that attachment apart from the query's own
    copies is 0x54624, the case [arg+4] == 0x1b of the event verb
    0x54560, and no poster of that message is in the image
    (OI-123). And the chain's top still ends 28 px short of the
    authored exit x 1020, so the seat's own motion matters as much
    as the table - and PR-341 measured it, correcting PR-339: the
    class +0xa4 verb takes its sign from the state record's ROW
    CURSOR at [rec+0x4], not from any row threshold, so 0x1e5c0
    shifts the table 1 px down for cursors 0..6 and 1 px up from
    cursor 7 - and ZARIA.01's PALM.MOV and PALML.MOV are 14-row
    states, which balances to exactly zero net with a 7 px
    in-cycle descent. KATAPULT's +0xa4 dispatcher keys the same
    cursor and its five jumps (+13,+5) to (-42,-31) sum to exactly
    (0,0): its table is displaced up to (+42,+31) inside the cycle
    and then put back (OI-125). No seat drifts, so the chain's
    authored heights come from the four placements alone and the
    crossing stays out of reach. The port's walk (2 px a tick) and
    jump (measured vertical) cannot cross either way.
    WHAT WOULD CLOSE IT: PR-336's question - the poster of the 0x1b
    message that attaches a requester to a surface - is ANSWERED and
    it is not this level's: five sites post it, the only one with a
    payload is a verb of TLewKadmar2 (0x9b4d4 +0x90), and no seat
    class and no ZARIA lion dispatches the value at all (PR-343). But
    the attachment slot is not message-only: the ground-contact verb
    +0xAC, which every lion class installs as 0x546a0, copies the
    standing query's answer into [obj+0xa9] every step (PR-345), so
    the carrier question becomes WHICH HANDLE the lion's standing query
    resolves to at each chasm position - if it resolves to a seat's own
    handle at that seat's obj+0xa5, the waiver of PR-344 applies and
    the seat can hold the lion below its table. Measuring that needs
    the +0x6c verb of the lion class (PR-344) and the ground the
    chasm's own tiles give, which is T6. The seat chain's own motion
    is ported (PR-342) and returns to its authored place every row
    cycle, so the 28 px is not coming from it.
    THE ARITHMETIC IS NOW MEASURED (PR-348). The clearance the seat
    chain grants is not a lookup: the sampler 0x455a0 returns
    `(x - A.x) * (B.y - A.y) / (B.x - A.x) + A.y - point.y` for the
    bracketing record pair A and B - a slope, truncated toward zero by
    `idiv`, with no guard when B.x == A.x - and the point it samples
    is whatever the requester's class verb +0x6c hands back as a
    POINTER to {x, y} (0x455b1, 0x455c8), which answers PR-344's
    second open item. A ZARIA.01 crossing leg therefore has to
    reproduce that expression and its truncation; what is still
    unmeasured is the point +0x6c returns for the lion and for PALMA.
    AND IT IS NOW MEASURED (PR-349): that verb is `add eax,0x89; ret`
    (0x23a10) for every lion class, for KAMIEN and for PALMA,
    KATAPULT and SPLYW, so the query samples the position snapshot of
    PR-159 at [obj+0x89]/[obj+0x8d] - and it is that point the
    REJECTING sampler call uses (0x45462, 0x45468), while the aimed
    point and the incumbent test use the caller's block (0x4548b,
    0x45491). STRUMIEN is the one difference: its +0x6c is a bare
    `ret` (0x23b50) and samples the live position. Every input of the
    crossing law is now known; the open half is whether any ZARIA.01
    position and table satisfy it, which is a port leg. PR-350 narrowed
    what that leg has to satisfy: the ask is aimed at a three-dword
    block 0x45320 builds on its own frame (0x4532c-0x45338), whose
    y-component is only filled by the class verbs +0x48/+0x50 and the
    script method at [[obj+0x48]+0x33]+4 - a chain that runs exactly
    when the object holds no surface, because the lions' and KAMIEN's
    +0xbc verb 0x23870 is `cmp [eax+0xa9],0; setne al` - so an aimed
    y of 0x98278 + vy, a value no coordinate competes with, is what a
    holder asks with; the two 0x455a0 calls measure one clearance with
    the record pair reversed, and the first one's subtrahend is zero
    because all six askers `xor ecx,ecx` before `call 0x45320`
    (0x1e594, 0x1e983, 0x2890b, 0x38100, 0x3df2d, 0x3f026). So first
    contact needs the AIMED point on the slope to within the `idiv`
    truncation, and a surface already held at [obj+0xa9] re-accepts
    itself at any non-positive clearance at the snapshot - which is the
    carrying mechanism, and the leg is to test it against ZARIA.01.
    PR-351 censused that chain and removed its last unknown: all 186
    class views in the data region carry the SAME +0x48 (0x23aa0, the
    current-script accessor over [obj+0x50]/[obj+0x5c]) and +0x50
    (0x53e40), and 0x53e40 writes its state-active test of +1 or -1
    through edx at the block's HEAD, not at its y-component, which
    therefore stays the seeded code pointer 0x98278 plus [obj+0xce];
    155 of the 186 gate the chain with the held-surface predicate
    0x23870, and only 31 carry a different +0xbc. So the aim is not a
    prediction of the lion's next position, the second clearance test
    does no spatial filtering for these classes, and the crossing leg
    decides on the first test at the snapshot plus the held-surface
    waiver.
    THAT LEG HAS NOW BEEN RUN, AND IT ANSWERS NO FOR THE STANDING
    QUERY (PR-352). The port models 0x45380, the flag gate and 0x45400
    (seat_bracket, seat_slope, seat_ask_core in ported/ll_level.c) and
    a new selftest leg, L15 `surface_ask_selftest` in the
    `./ll_level --mov-selftest` chain, drives ZARIA.01's four ANIPL2
    tables through the row lists their classes move on. Two measured
    facts decide the row. (a) THE ACCEPT WINDOW OF A COLUMN IS BOUNDED
    BY THE TWO SAMPLER CALLS: 0x45468 (the bracket's own record order,
    at the position snapshot) requires the requester's row y <= the
    table's top at that x, and 0x45491 (the pair REVERSED, at the
    aimed block) requires his aim y >= that top; `idiv` truncates
    toward zero, so the two forms put the line at its true value or at
    the whole pixel beside it, and depending on the sign of the slope
    a column answers no row, exactly one, or two - measured both ways
    on a 1:2 slope at x 33 (empty there, two rows at x 33 of the
    mirrored line, one row where the interpolation is exact). A LEVEL
    table, which is what all four authored seat tables are (both
    records of every sampled row pair carry the same y), answers
    exactly its top row. (b) A STEP FROM ONE SEAT TABLE TO ANOTHER
    THEREFORE NEEDS THEIR TOPS AT THE SAME ROW AND AT AN OVERLAPPING
    x. Measured over one full cycle of their own row lists the four
    tables cover x 838..900, 885..947, 930..992 and 307..382 with
    tops y 475..482, 380..387, 320..327 and 633..664; every pair's
    top bands are 53..306 px apart, so 0x45400 carries none of the
    authored steps of the chain. The 0x45479 waiver is the only path
    to a surface above the requester and it covers ONLY the table at
    [obj+0xa9]; 0x454a0 keeps the most negative clearance, so among
    answerable tables the HIGHEST wins, and 0x45498 is indeed never
    taken for the classes PR-351 censused. What closes this row is
    therefore the land/attach path of OI-128 - PALMA's +0xB4 IS the
    ask verb - or the lion's launch through KATAPULT's landing verb,
    and the .plt ground of T6, not the standing query.
    PR-392 CORRECTS THE GEOMETRY OF THE TWO SAMPLER CALLS IN THIS ROW.
    Nothing is reversed: both 0x455a0 calls take A = record i-1 and
    B = record i (the first builds them across stack slots at
    0x45443-0x45464, the second in registers at 0x45481-0x4548f), so the
    two expressions are not negatives of each other and not within 1 px
    as (a) above has it. What differs is the sampled point: the first
    call measures at the +0x6c result, which is 0x23a10 = `add
    eax,0x89; ret` in all 186 class views (PR-351) - the clamp sample
    pair [obj+0x89]/[obj+0x8d] of OI-128 - and only the second measures
    at the aimed block that 0x4541f's bracket test used. The gate
    reading also needs fixing here: 0x4547b skips that first test for the
    held element, so a held table is not re-accepted "at any non-positive
    clearance at the snapshot"; the value tested for everything, held
    included, is the AIMED clearance, rejected above 0 and above the
    incumbent. The port followed that old reading in seat_ask_core,
    and L15 pinned it in the test table.
    PR-393 CLOSES THAT PORT HALF AND MOVES THE PINS. seat_ask_core
    now takes A = record i-1 and B = record i for both clearances and
    changes only the sampled point, and L15's case table is re-derived by
    hand: the rising span (0,50)-(100,100) answers y 66 only at x 33 (not
    66 and 67), the falling span (0,100)-(100,50) answers y 84 at x 33
    (100 + trunc(-1650/100), `idiv` toward zero - that column is NOT
    empty) and y 51 at x 99 (not 50), and two new rows pin the two points
    now that the value is one. What this row concludes is untouched: every
    sampled pair of the four ANIPL2 tables is level, so the measured
    envelopes and the 53..306 px gaps between their top bands are
    identical under either reading, and the standing query is still not a
    carrier of the authored chain.

OI-125: the element half of the object class +0xa4 verb - the five
  row-index table jumps of the catapult dispatcher and the two fields that
  gate their element calls
  status: OPEN
  origin: PR-339 (the seat row-clock read), re-keyed by PR-341
  notes: PR-339 found the object class's +0xa4 verb has exactly two
    runners in the image: 0x52341, on the admitted branch of the row
    law 0x52320 (the current row's threshold [row+0x10] against the
    feed accumulator [obj+0xa1]), and 0x535db, in PR-172's state
    applier 0x535a0 before the new state's ENTER. For PALMA that slot
    resolves 0x1e5c0, a table shift of (0,+-1); KATAPULT's slot
    resolves 0x1e690, a five-way dispatcher with the jump table at
    0x1e670, whose targets 0x1e6be, 0x1e723, 0x1e788, 0x1e7f3 and
    0x1e886 each move the whole table by a fixed amount through
    0x45200 and then, when [obj+0xbd] or [obj+0xc1] is non-null, run
    [[0xcef40+0x394]+8]+0x10 with an element block.
    PR-341 FIXED WHAT BOTH SWITCH ON. Neither uses the row threshold:
    both read [[state record]+0x4], and that word is the state's ROW
    CURSOR - 0x53616 writes the transition's authored start row into
    it, 0x521c5 zeroes it and 0x521d7 increments it, on the record
    whose control byte is at +0x12 and row list at +0x2e. So 0x1e5c0
    moves the table 1 px DOWN for cursors 0..6 and 1 px UP from
    cursor 7 on - the row POSITION, as PR-336 said - and 0x1e690
    acts only at cursor 3,4,5,6,7, by (+13,+5), (+13,+6), (+13,+10),
    (+3,+10) and (-42,-31) - PR-339's fifth offset was a misread -
    which sum to exactly (0,0). Measured on the shipped texts:
    ZARIA.01's PALM.MOV and PALML.MOV are 14-row states, so their
    four PALMA tables oscillate +7 px down and back with zero net,
    and KAT.MOV's ten rows carry the catapult's cursor through 3..7
    once per cycle, so that table is displaced up to (+42,+31) and
    then put back exactly. Install-wide, 1890 states net down, 61
    exactly balanced - the 61 fourteen-row states - and 366 net up.
    The same cursor gates the ask: 0x1e570 runs the table query only
    while the cursor is below 13; KATAPULT's 0x1e970 has no gate.
    PR-340 ANSWERS PART OF IT. [obj+0xbd] and [obj+0xc1] are real
    fields but only KATAPULT-sized - the object is allocated 0xc5
    bytes at 0x1fb42 while PALMA gets 0xb5 at 0x1fc18 - and what
    they hold is an ID, not a pointer: the getters that feed them,
    0x22e30 (`mov eax,ds:0xcebb8`) stored at +0xc of KATAPULT's own
    class view 0x96d1c among ten seat views, and 0x22dd0
    (ds:0xcebc0), read globals written ONLY at 0x1e001/0x1e015,
    which sit in the static-init block 0x1dfd9-0x1e029: eight
    stores to 0xceba8..0xcebc8, each `call 0x44350`, and 0x44350
    is a counter (`mov eax,ds:0xa90ac; lea edx,[eax+1]; mov
    ds:0xa90ac,edx`). The object actually managed is a singleton:
    0x1e6e0-0x1e713 loads the game-object anchor 0xcef40, takes
    [anchor+0x394], its +0x8, and calls that +0x10 verb with the
    block {0,1,dx,dy} - so [obj+0xbd] only decides WHETHER the call
    happens. The setter shapes are 0x20964 (`mov [ebx+0xbd],edx`,
    eight bytes after `or BYTE PTR [obj+0xc2],0x40`) and 0x22dfa
    (`or BYTE PTR [obj+0x66],0x40` then `mov [eax+0xbd],edx`); the
    clearer 0x22de0 (`mov [eax+0xbd],0`) IS stored - +0xb4 of
    0x96d1c, +0xd8/+0xe0 of the KATAPULT table and sub-object
    views, +0x198/+0x1bc/+0x1c4 of the PALMA trio, +0x248/+0x250
    of the KILOF_PL pair - and 0x22e18 clears both fields after
    `call 0x52c70`; neither setter is stored in any of the ten
    views, so their caller is the open part.
    WHY IT MATTERS: the seat's authored motion is now fully known and it
    is a cycle, not a carry - no seat can lift the lion anywhere.
    PR-342 PORTED THE ROW-ADMIT RUNNER: ported/ll_level.c now shifts
    those tables at the admitted row, keyed to o->fi, for the two types
    whose class stores a table verb, and leg L14 pins it on ZARIA.01's
    own texts. What is left here: the element half of both verbs; the
    slot's second runner 0x535db, whose fixture needs a seat script with
    two states and none ships; and the reads below.
    WHAT WOULD CLOSE IT: the callers of 0x20964/0x22dfa/
    0x22e18 and what 0x52c70 does; whether [obj+0xbd]/[obj+0xc1] are
    the rectangle words of PR-246 - the cursor-7 arm 0x1e93e-0x1e944
    hands [obj+0xb5] and [obj+0xb1] to 0x52b10, and PR-246 stores the
    authored rectangle x0,y0,x1,y1 at [obj+0xb1..0xbd] - or the ids of
    PR-340's counter block; what 0x52b10 does with that pair; the
    element blocks each of the five arms passes; which cursor value
    pairs with the admitted row, since the cursor step of 0x520c0 runs
    before the step of 0x521f0 - PR-342 keys the move to the admitted
    row and says so; and what [obj+0xc2] bit 7 and
    [obj+0x66] bit 6 gate. Then the second runner at the state
    change, which no shipped seat script can measure.

OI-126: the golden-master menu pose-set pin is a wall-clock capture, so
  a loaded machine can truncate it and the leg then fails on poses the
  port still draws
  status: OPEN
  origin: PR-342 (the full gate of this tree)
    verify-golden runs ./ll_cutscene --demo with LEO_MENU_PPM_DIR and
    LEO_MENU_PPM_MAX=300 under a 120 s timeout, and
    tools/golden_menu_cmp.py then compares the SET of distinct composed
    frames against the 30 hashes of tools/golden_expect.json. The set is
    meant to be timing-independent - the map animation is a fixed cycle
    - but the capture is not: it ends at a wall clock. Two of the three
    "make -k -j$(nproc)" verify-all runs of the PR-342 tree failed ONLY
    this leg: "port pose set changed: 28 vs 30 hashes (extra [] missing
    ['7d5a74c394d2', '5651642ef3fa'])" and, in another run, "27 vs 30
    ... missing ['5651642ef3fa', '85dd304dcffa', '7d5a74c394d2']".
    Nothing extra was ever drawn and the missing hashes are WHOLE poses,
    not near-matches. The same tree's FIRST -j run passed 71 of 71,
    "make verify-golden" alone passes and "make -k verify-all" with no
    -j passes rc=0 71 (ok), so the failure is the window, not the port.
    WHY IT MATTERS: this is the one gate leg whose result depends on
    machine load, so a -j verify-all EXIT=0 is not reproducible on a
    busy box and a red there is not evidence about the port. Every PR
    record that reports 71 (ok) reports a run that happened to fit.
    WHAT WOULD CLOSE IT: make the capture a frame count and not a clock
    - the port writes those frames itself, so a fixed count, or a stop
    tied to the cycle (its 30th distinct pose), takes the timeout out
    of the law; or pin frame indices as well as the set, so a truncated
    capture reads as a gap and not as a missing pose. Then re-measure
    this leg over several -j runs of one tree.

OI-127: what a 0x1b payload is - the entry array at [0xcef40+0x74],
  its code blocks, and the group it mirrors
  status: PARTIAL
  origin: PR-343 (the object message queue read), the type answered
    by PR-344
    The one payload-carrying 0x1b post, 0x2c252, hands over what
    0x4b760 wrote to its out-parameter: not an object pointer but an
    ENTRY ADDRESS inside the array at [game anchor 0xcef40 + 0x74],
    scanned for [anchor+0x80] entries at stride 0xc (0x4b78c-0x4b7d5).
    Each entry's first word is a CODE TABLE, not an object: 0x4b7ad
    calls `[[entry]] + 0x14` with the id that the requester's own
    class verb +0x6c returned (0x4b7a3), and an entry counts only if
    that scores above 0 and below the running best. 0x5460e, the
    0x1b arm, uses the same table the other way: it calls
    `[[old entry]] + 4` before replacing (0x5461e) and the code AT
    `[[entry]]` after (0x54632), both with edx = the object. So an
    entry is a provider record with +0 attach, +4 detach and +0x14
    score - and 0x45400's waiver at 0x45479 compares that same slot
    against a SURFACE OBJECT pointer, which is the conflict to settle.
    0x4b6d0 is the same scan with the opposite sign: it keeps the most
    NEGATIVE score from a start of 0x80000001 (0x4b720-0x4b737) where
    0x4b760 keeps the most positive from 0x7fffffff, and the caller
    accepts only a score above -5 or below 5 (0x2c209, 0x2c230).
    PR-344 ANSWERS THE TYPE AND DISSOLVES THAT CONFLICT. A surface is
    a three-word handle: +0 a code table, +4 the record array, +8 the
    count - 0x45120 writes the count at [handle+8] and the array at
    [handle+4] (0x4512c, 0x4513b, 0x45166), 0x45380 reads the count
    at +8 and the array at +4, and a seat's own handle is EMBEDDED in
    its object at obj+0xa5, so its record array IS [obj+0xa9], the
    slot 0x54624 writes - the ladder's ctor initialises that triple at
    0x1f8de, 0x1f8e9 and 0x1f8f9. The waiver 0x45479 `cmp esi,ebx`
    compares a handle against [requester+0xa9], so the attached value
    is a handle too - which is also the shape of the 0xc-stride
    entries this row describes: {code table, records, count}, and
    0x4b7ad's `[[entry]+0x14]` is the same code table 0x5461e and
    0x54632 call at +4 and +0. Those tables are in the image: 0x96c28
    is {0x453d0, 0x45320, 0x45400, 0x45120} - height-at-x, query,
    query core, parser - sixteen bytes before PALMA's class view, and
    the dwords that hold 0x453d0 number 38 across the class-view
    region, which is the census of the classes that own a queryable
    surface. Each is a four-method table immediately FOLLOWED by the
    class view, so the +0x14 of this row is the class view's own +4.
    PR-346 READS THE SIDE THAT ASKS THEM. The standing path's
    candidate collector 0x4b600, called from the ground-contact verb's
    state bits, first asks the surface the object already holds through
    slot +8 of its handle table (0x4b64f-0x4b669 with the mode 2) -
    and slot +8 of the 38 handle tables is 0x45400, the query core, so
    the table PR-344 described is a method table by dispatch and not
    only by layout - and then walks the SAME array this row names,
    [anchor+0x74] for [anchor+0x80] entries at stride 0xc, calling
    each entry's `[[entry]]+0x1c` (0x4b697). One method table
    therefore answers all five questions: +0 attach, +4 detach, +8 the
    query core, +0x14 the score, +0x1c "am I under this point". The
    sibling registries are a second question, not a copy: 0x4b7f0
    walks [anchor+0x78] for [anchor+0x84] entries at stride 0x14, and
    0x4b430 asks through 0x534e0. What still is not known is the
    entry's own table - the 38 tables are the seat classes' own, so an
    entry's table is elsewhere in the image and unnamed - who fills
    the array, and what 0x4b500 makes of the candidate list with the
    global at 0xa94e8.
    The two "notices" of 0x5460e are methods of that table, not
    notifications: 0x5461e calls the OLD surface's +4, which is the
    query 0x45320, so detaching re-runs the standing-surface query for
    the object, and 0x54632 calls the NEW surface's +0, which all 38
    tables hold as the height probe 0x453d0, passing the object itself
    as the position block - [obj+0] is its x (PR-159), and 0x45380
    reads the point as [edx].
    0x5da60 shows where the array comes from: it rebuilds the
    mirrors at [anchor+0x6c..0x78] and [0x7c..0x80] from the anchor's
    group heads at [0x14]/[0x18]/[0x1c]/[0x20] and their counts at
    [0x24]/[0x28]/[0x2c] - the same group-array mechanism the object
    walk 0x4ba90 uses for the heads at [0x58..0x64]. 0x4a621/0x4a63f
    zero the pair.
    WHY IT MATTERS: this is the object the lion would be standing on
    if the attach path ran, and the only F6 candidate that is written
    by code rather than assumed. Whether ZARIA.01's chasm can use it
    depends entirely on what that group is.
    WHAT WOULD CLOSE IT: which of the four group heads the mirror at
    +0x74 follows and what fills that group - the installer or the
    level record that creates those entries; whether those entries use
    a class's own four-method table, as a seat's embedded handle does,
    or a table of their own; and whether [obj+0xa9] is per-object or
    per-class, since a seat uses that same offset as its own record
    array while a lion uses it as the surface it is attached to.
    PR-347 ANSWERS THE FIRST OF THOSE AND CHANGES THE QUESTION. The
    array at [x+0x74] with its count at [x+0x80] is not a terrain
    registry: that field pair belongs to a record-parsed family built
    at 0x44448-0x44600, whose fields are literal in the image - "
    tools = %s " at 0x917d2 and " crown = %s " at 0x917e3, the word
    "off" at 0x917df - which is the belt record of PR-67. The anchor's
    own constructor zeroes the whole family, [x+0x64], [x+0x70],
    [x+0x74], [x+0x78], [x+0x7c], [x+0x80], [x+0x84] (0x4a613-
    0x4a64f), and 0x5da60 clears it again when the group heads are
    empty. So the entry 0x4b760 scores for the 0x1b attach is a TOOL,
    and this row's "provider record" reading is corrected: the attach
    of 0x2c252 is the lion taking a tool, not standing on ground. What
    is left: whether the anchor's copy of those fields is ever filled
    - the builder shown carries the vtable 0xa0c5c, the anchor's own
    instance 0xa1350 - and what a tool entry's method table is.
    PR-353 and PR-354 RULE OUT ONE CANDIDATE for that last part and
    answer nothing of it: the 0xa0f28 + 0x18*k block of six-dword verb
    tables is the level script's own clause-node table, installed at
    [node+4] by the .MOV builder, not a provider or tool entry. The
    shapes do look alike - verbs at +0, +4 and +0x14 with the triple
    0x45800/0x45870/0x458e0 shared - and 0x457e0, which walks one
    level and calls [[x]+4], is the +4 verb of the base node table
    0xa1000; whether tool entries use tables from the same verb block
    is not measured. The row stays open.
    PR-394 ANSWERS THE ENTRY-TABLE HALF for the two registries the
    physics step asks. 0x4b600's `call [element class + 0x1c]` reads
    the table at [entry+8], NOT [entry+0]: 0x443b2 fills such arrays
    with 12-byte records {0, 0, 0x98278} (0x443ce), and 0x98278 is a
    shared view {0x23ed0, 0x23ea0, 0x23e00} - the surface descriptor's,
    also written by 0x23e67 when 0x23e30 turns a record's x0/x1 centre
    into that descriptor. 0x98274, the other view, belongs to the
    request block 0x4b430 builds, not to an element: it is stored at
    [obj+0x10] by 33 class ctors and its +0x8 slot is the value 154 of
    158 ctor tables store at [obj+0x8]. The [world+0x70] and
    [world+0x78] registries are a different shape again (0x14 stride;
    0x4b430 asks directly, 0x4b7f0 through [[entry+0x10]+0x4]), and
    0x5da60 only copies group heads into the mirrors when they are
    null. Tool entries are still unnamed; the row stays open.
    PR-396 ANSWERS THE [world+0x78] HALF OF THAT PARAGRAPH, AND THE
    TABLE QUESTION WITH IT. Those 0x14 records come from the `sufity=`
    file through 0x4da90, and their method table is the record's own:
    the class record at 0xa14d8 keeps its fields one byte in - C+1 the
    constructor 0x4fbd0, C+5 the destructor, C+9 an unread verb, C+0xd
    the element size 0x14 - and 0x76ab0 runs that constructor over the
    whole array (`call [class+1]` per element, `add ecx,[class+0xd]`
    between them), so [entry+0x10] = 0xa218c: +0 the row parser 0x454d0
    (the same scanner `.BLK` uses - "%d,%d , " at 0x91d0f through
    0x4a0b0 into entry+0 and entry+4, then "%d" at 0x91836 into
    entry+0xc, then the probe " , " at 0x91839) and +4 the query
    0x45500. The `.BLK` pair is the mirror image and PR-395's shape
    survives intact: class 0xa14c0, constructor 0x4fb70, its table
    0xa2178 at entry+0 (NOT +0x10) and the same shared view 0x98278 at
    entry+0xc. That also corrects what PR-395 said about the zeroes:
    [their text: "the block comes from calloc (0x4dbb9)"] - 0x76a64 is
    the plain malloc chain (0x5c320 -> 0x7521f -> 0x78ce0), 0x76b24
    writes the count at [mem] and returns mem+4 as the array, and every
    field that is zero is zero because the record's constructor writes
    it zero. What stays open here: the attachment tail of the +0xb0 verb
    (0x54500-0x5453a, class slots +0xbc, +0x48, +0x50 and the table at
    [that+0x33]) - CLOSED IN FULL BY PR-399, which reads the tail to its
    last instruction, gives its three tests as the boolean `!bit 0x02 &&
    (held || bit 0x01)`, and identifies the table at [element+0x33] as the
    element's own method table, the one PR-357 named; and the two views
    themselves - PR-293 names "the
    shared table 0x98278" and PR-305 names "the view 0x98274", but those
    are the same bytes four apart, and the run reads 0x23e30, 0x23ed0,
    0x23ea0, 0x23e00 and then 0x74bac one byte PAST a dword boundary
    (0x98285). PR-397 MEASURES THAT LAST GUESS AND FINDS IT WRONG: those
    two addresses do not match the class-descriptor shape at their first
    byte (0x30 and 0xd0) nor at their first field (0xd000023e and
    0xa000023e, outside the code range), and the same test fails at the
    LION view 0x96874 (0x70), while 0xa14c0 and 0xa14d8 match it exactly.
    So they ARE method tables, as PR-293, PR-305 and PR-386 used them, and
    the only thing left open about them is which table a caller means. The
    loose byte pattern PR-397 tried instead of that guess matches 344
    places in 0x90000-0xb8000, which is why the negative test is the byte
    test and the positive test is the code that consumes a descriptor.
    What PR-396 left as "C+9 an unread verb" is read now: 0x4fba0 and
    0x4ee20 are the two classes' copy-with-release verbs - `test dl,4`
    takes a reference on the source's allocation header at [src-4] through
    0x74bc0 and calls the delete 0x74be0, `test dl,2` calls 0x5c370 - they
    write no field of the record, and 0x7b791 keeps [class+9] in the guard
    the array builder registers, which is where the container holds a
    class's copy verb. Tool entries are still unnamed; the row stays open.

OI-128: what landing and leaving a surface does - the class verbs
  +0xB4 and +0xB8, the '#' ejection counter, and the unread head of
  the ground-contact verb
  status: PARTIALLY CLOSED (PR-353 read the pair, PR-354 named the
    clause-node tables and the flag byte, and PR-357 read the methods
    themselves: the +0x33 word is the last field of the .SEQ element
    record, its table is 0xa0c08, and slot +4/+8 are one inverse pair
    that shifts the position by the element's second anchor minus its
    first and undoes it on leaving; PR-359 models that pair in the port
    (ll_seq_anchor_land/leave, leg L16) but does not run it; PR-360
    measured the shipped data - 98.7% of elements carry an anchor, half
    of them spell (f4,f6) == (f0,f2) so the pair is a no-op, and for the
    lion's own ZARIA.01 standing states the step is a DOWN move of about
    31 px; PR-399 reads the rest of the verb band - the lock-bit pair
    +0xc0/+0xc4, the [obj+0xb6] bit map, and the five thunks that
    dispatch into the pair, each named once by field +0x04 of a row of
    PR-101's node-class run, and PR-400 reads all 25 of those rows'
    +0x04 verbs: one shape, walk the chain then act once on [[edx]], with
    no direct caller anywhere. Still open: what the second anchor NAMES
    (OI-129), the 0x58c
    timer posters, the block 0x546de-0x54700, the '#' census, and who
    starts a visit)
  origin: PR-345 (the class verb +0xAC read)
    The ground-contact verb 0x546a0 ends by telling the object about
    its surface: when the standing query's answer differs from the
    kept one it calls the old surface's +4 method, copies the new
    answer into [obj+0xa9] (0x5478e, 0x547bc) and calls the object's
    own class +0xB4 at 0x5479f, and when there is no answer it calls
    the kept surface's +4, zeroes the field (0x547f0) and calls
    +0xB8 at 0x547fa. The installed pair is 0x238b0/0x23880 for every
    lion class and KAMIEN, 0x1e560/0x208d0 for PALMA, 0x22de0/0x20a40
    for KATAPULT, 0x23290/0x20230 for the ladder - and PALMA's +0xB4
    IS the seat ask verb of PR-341, so a seat's ask belongs to
    landing, not to the tick. A second dispatcher, 0x45cb1, calls the
    same pair keyed on the surface's record count at [handle+8]
    (0x45cc6, 0x45cd4), and nothing in the image names it. [CORRECTED BY
    PR-399: the dispatcher is 0x45cb0, not 0x45cb1 - the real entry, read
    from `ret`, four padding bytes, `push ebx; push ecx` - and IT IS
    NAMED, once, as field +0x04 of row 16 of PR-101's node-class run at
    0xa0f28, the dword at 0xa10ac. Four of its siblings are named the same
    way and are listed in that record.] [CORRECTED AGAIN BY PR-400: what
    that dispatcher tests is not a surface's record count. Its eax is a
    NODE of PR-101's chain, and the test is `cmp DWORD PTR [ecx+0x8],0`
    on the node's own +8 word (0x45cbb) - the field PR-101's channel
    query 0x45dc0 adds into an output. It is also not a one-sided gate:
    both arms act on the object, non-zero runs +0xB4 and zero runs
    +0xB8.]
    The other half is a timer. 0x54766 leaves the surface whenever the
    winning record's flag byte [obj+0xe2] is 2 - a '#' record of
    PR-335 - and [obj+0xc6] has reached 0x58c, 1420. That word is one
    of the two the message values 2, 8 and 0xd write through 0x54560
    (PR-343), so some poster is counting the ticks a thing may stay on
    a '#' surface. The same tail also sets bit 0x10 of [edi] when it
    acquires a surface and bit 0x20 of [edi+1] whenever the winning
    flag byte is non-zero (0x54785, 0x547cf) - two state bits the port
    has no name for.
    WHY IT MATTERS: '#' is the third separator the .MOV table grammar
    writes and the port reads but never acts on; if a shipped seat's
    table uses it, that seat drops the lion on a timer, which is a
    crossing mechanism the ladder has not modelled. And the ask verbs
    being land verbs changes what F6 has to drive: a standing lion,
    not a per-tick query.
    WHAT WOULD CLOSE IT: the bodies of the land and leave verbs,
    PALMA's 0x1e560 first; who posts the messages that write
    [obj+0xc6] and what counts there; the unread block 0x546de-0x54700
    of 0x546a0 and the three scan-and-apply pairs behind its state
    bits, which read the provider tables of OI-127 from the standing
    path; and a census of the '#' separator over the shipped .MOV
    tables, which the port's parser already sees.
    ONE ITEM OF THAT LIST IS ALREADY ANSWERED, in the port itself:
    the surface-table parse selftest carries the '#' shape with the
    note "NOT in any shipped line - counted here so the grammar stays
    tested" (ported/ll_level.c, the sl[] table of that leg). So no
    shipped seat table ejects its rider on the 0x58c timer, and this
    row's crossing lead is the land verb, not the timer.
    PR-353 ANSWERS THE FIRST ITEM OF WHAT WOULD CLOSE IT, and the answer
    is not engine code. Taking the views the way PR-351 takes them (+0x48
    = the current-script accessor 0x23aa0), there are 186, and 155 of
    them install ONE pair, +0xB4 = 0x238b0 and +0xB8 = 0x23880 - exactly
    the 155 that gate the aim chain with the held-surface predicate
    0x23870, the same set. That pair is a dispatcher: it skips when
    [obj+0xb6] bit 0 is set, else it runs the accessor, takes the script
    object's +0x33 table, and calls slot +4 (land) or +8 (leave) of it
    with [obj+0x6d] as the argument. The 31 other views install private
    pairs; 29 of them carry the embedded-record step-back `sub eax,0xc`
    and the bit-2 test of the caller's argument, and the two read in full
    are one-word resets - PALMA's land verb 0x1e560 clears [obj+0xb1]
    and its leave verb 0x208d0 loads the global ds:0xcebb8, KATAPULT's
    0x22de0 clears [obj+0xbd]. So no shipped land verb moves anything:
    the crossing of ZARIA.01's first chasm is script code reached through
    that +0x33 table, or the attach path below, or the .plt ground of T6.
    And the second dispatcher is named after all: it is 0x45cb0, slot +4
    of the record headed 0xa10a8 in the family of 25 records at stride
    0x18 from 0xa0f28, whose other +4 verbs 0x45d10 and 0x45ce0 are the
    posters of the 0x1b attach and of value 1.
    WHAT STAYS OPEN HERE is now the script object, not the verb: what
    fills its +0x33 table and who writes [obj+0x6d]; which code sets
    [obj+0xb6] bit 0; what the shared verbs 0x457e0, 0x45800, 0x45870,
    0x458e0 of that record family do and whether the family is the
    provider table OI-127 asks for; the 0x58c counter; and the block
    0x546de-0x54700.
    PR-354 ANSWERS MOST OF THAT LIST AND NAMES WHAT DRIVES IT. Those
    records are the CLAUSE NODES of the level script: a node is twelve
    bytes, [node+4] is the per-command table and [node+8] the scanned
    argument (the `end_level` arm is the proof - allocate 0xc at 0x46bd5,
    ctor 0x45770, `mov [node+4],0xa1048` at 0x46beb, `sscanf "%d"` into
    [node+8] at 0x46bf9), and the builder arms name 24 of the tables by
    word (`/tmp/opencode/pr354_words.txt`): `rev` 0xa0f28, `ground_on`
    and `ground_off` 0xa10d8 PLUS 0xa10a8, `lock`/`unlock` 0xa10d8,
    `lock2`/`unlock2` 0xa10d8 PLUS 0xa10c0, `MOVE` 0xa1078 plus 0xa1060,
    and so on. The verb at [node+4] is one shape - resolve, `cmp
    DWORD PTR [node+8],0`, run the ON effect or the OFF effect - and the
    effects are 0x54f40/0x54f90, the class pair +0xB4/+0xB8, and the
    class pair +0xC0/+0xC4 = 0x54fe0/0x55020 in the same 155 views. All
    four of those run the SAME script methods: slot +4 (land) or +8
    (leave) of the script object's +0x33 table, through the PR-351
    accessor and aim pair - and they maintain [obj+0xB6]: 0x54f40 sets
    bit 0x01 (0x54f7b), 0x54f90 clears it (0x54fcb), 0x54fe0 sets
    0x01|0x02 after calling leave, 0x55020 calls land only when both are
    set and clears both (0x55057). `atc_on`/`atc_off` write bit 0x20 of
    that byte from [node+8] bit 0 (0x45c45), which is the bit the verb
    0x23860 answers at class slot +0x40 in 116 of the 189 class tables.
    So `ground_on`/`ground_off` are a scripted command, not a per-tick
    engine query, and the port's counted-but-unmodelled `ground_on`,
    `ground_off`, `lock2` and `unlock2` all act on the byte the landing
    chain reads. WHAT IS LEFT: the script object's +0x33 table itself
    and the land/leave methods stored in it, [obj+0x6d]'s -1/+1 (the
    field `ldir`/`rdir` write, PR-320), the 0x58c counter, and the
    block 0x546de-0x54700.
    PR-357 READS THAT +0x33 TABLE, AND IT IS NOT SCRIPT CODE. The word
    has exactly ONE writer in the image - 0x41f4d `mov DWORD PTR
    [ebx+0x33],0xa0c08` - and it is the last field of a 0x37-byte
    record allocated at 0x43f59 inside the .SEQ loader 0x43e70: the
    element record of the port's own ll_seq.h grammar. The record's
    initializer 0x41ef0 stamps 0x7fff at +0, +4 and +8 and installs the
    block; its reader 0x41f60 then reads w0, the shape-frame words, and
    f0, f2, f4, f6 straight out of the blob (0x41f9d, 0x41fa3, 0x41fb9,
    0x41fd0). Slot +4, 0x42980, is the land method and slot +8, 0x429d0,
    the leave method, and they are one inverse pair: if f0 is the 0x7fff
    sentinel nothing moves; otherwise the pair adds or subtracts
    (f4 - f0, f6 - f2) from the position pair - the vector from the
    element's FIRST anchor to its SECOND, the X leg negated when the
    facing is not positive. Ten sites reach them, all engine code: the
    class +0xB4/+0xB8 verbs (0x238c6, 0x2389c), the +0x54 frame step
    0x53df0 (which first copies the position to [obj+0x89] at
    0x53e06-0x53e0d and shifts the copy), the ask block 0x541f0 (which
    is PR-350's open item, now answered), the +0xB0 provider 0x544f0
    (PR-364 corrects this line's earlier 0x544d0, which is the empty
    +0xd4 override documented in two earlier rows of this file),
    one private +0x78 verb 0x3c330, and PR-354's four runners. The
    argument is the object - so [obj+0]/[obj+4], the live position
    PR-308 named - or a copy of it. So landing is: set the standing bit
    (PR-354) and, when the current element declares a second anchor,
    step the position onto it; leaving steps back. PR-360 MEASURED THE
    DATA over the staged fixtures and the answer is that the mechanism is
    the normal case, not an exotic one: over 1655 .SEQ payloads and
    14715 elements, only 185 (1.26%) hold the 0x7fff in f0 (the guard,
    and they are one element per file, mostly ANIBACK background strips),
    7487 of the anchored ones spell (f4,f6) == (f0,f2) so the pair is a
    deliberate no-op, and 7043 spell a real step (median magnitude 28 px,
    p90 48). The second anchor is authored, not padding: only 146
    elements have f4 == f6 == 0. What (f4,f6) is NOT: not the shape
    height (f6-f2 == int16(b17) for 0.1%), not the frame's bottom row
    (exact 15%, within a row 28%), and f4 == f0 in 71% of cases, which
    is why most steps are pure-y. What it IS, per frame: a body line -
    its distance from f2 is constant within one state and tracks the
    pose. Read through the ZARIA.01 scripts (the .MOV records are in the
    HD overlay ANIMACJE, and each [STATE] names its art with seq=), the
    lion's A_WALK/A_ZABLOK frames all step (0,-31), A_FIGHT_DOWN about
    (0,-22), A_WSP_LIN (0,-38..-46), A_ODLOT a constant -59, and A_DEAD
    runs from -55 down to -4 as he collapses; KAT.MOV's ten frames run
    (-6,-21) to (-28,-44) and PALM.MOV/PALML.MOV's fourteen (-5,-11) to
    (-5,-5). So when the lion lands, his position steps DOWN by about
    31 px, and when he leaves it steps back. WHAT IS LEFT HERE: what
    point of the art the second anchor names (a body line, not a shape
    edge - see OI-129), the 20 LITM.SEQ elements (ZARIA.02, ENA.03) that
    spell f4/f6 as 0x7fff and whose step would be 32767 px, the block's
    slot +0x10 (12 sites) and +0x14 (9) verbs, and the timer items
    above.
    PR-359 PUTS THAT PAIR IN THE PORT and answers the ask-model half:
    `ll_seq_anchor_land`/`ll_seq_anchor_leave` (ported/ll_seq.c) are the
    two methods instruction for instruction, and leg L16
    (`./ll_level --mov-selftest`, prefix `land shift:`) shows the delta
    is not cosmetic - a seat table 12 px above the requester's row is
    refused by 0x45400 from his own row and ANSWERED from the point the
    landing steps him to, while the mirrored x leg of another element
    takes the ask past the table's last record and it is refused again.
    So the landing step decides WHICH surface the standing query can
    see. It is deliberately NOT wired into the level runtime path: the
    engine's argument is the current frame's element through
    [view+0x48], which the port does not track per object. PR-360 has
    since measured the data half, and it says the step is real and big
    (~31 px for the lion's standing states), which makes OI-129 the
    blocker for wiring it, not the measurement.
    PR-364 READS THE POINT PROVIDER THAT OI-128 KEPT HITTING. The
    class verb +0xb0 has FIVE indirect call sites (0x2b881, 0x2b900,
    0x3e9ba - PR-308's ask side - plus 0x5507e inside the frame step
    and 0x4552f), and slot +0xb0 pairs perfectly with slot +0x54:
    every one of the 124 tables that install the gated frame step
    0x55070 installs the SAME provider 0x544f0, and the 31 tables
    that install 0x53df0 each install their own (31 distinct handlers,
    three of them already read by PR-308: DRABINKA's 0x232e0, LINA's
    0x4f6f0, and the store+flag stub 0x1e540 in PALMA's table, which
    ignores the buffer, writes its address to [obj+0xb1] and sets bit
    0x40 of [obj+0x66]). PR-308 read 0x544f0's body but not which
    way its tests branch; they do, and they are the exact negation
    of the +0x54 aim chain's: the provider applies the element's
    LEAVE method to the copy it hands back exactly when that chain
    skips - standing, or holding, or both - and skips it exactly when
    the chain runs (/tmp/opencode/pr364_census.txt section 5 prints
    the nine rows side by side: 0x54207 `jne` INTO the chain against
    0x54507 `jne` PAST it; 0x54216 `jne` PAST against 0x54516 `jne`
    INTO; 0x5421f `jne` PAST against 0x5451f `je` PAST). So every
    object of those 124 classes keeps two sample points at once:
    [obj+0x89], position + the step while free (PR-362/363), and
    [obj+0xd6], position - the step while standing or holding, and
    their difference is the anchor delta exactly when the object is
    NOT free. The third method with the identical gate is the class
    verb +0x58: 0x54eb0 installs at slot +0x58 of every one of the
    155 tables in that census, has exactly ONE call site
    (0x3ede2 - PR-366 read the function: the latched +0x14 handler
    0x3ed70 of five record-class tables, which moves the companion it
    resolved BY NAME and keeps at [obj+0x3c] to its OWN position,
    taken through its +0x90 verb; and for the one link that can be
    named, ANITAB = 0xa1eb4, the +0x58 that runs is 0x23ba0, the
    plain pair copy), raises token 0x1b = AC_BUTLA through +0x10
    (PR-183's name), then writes the caller's {x, y} pair to
    [obj]/[obj+4] and, under that same predicate, adds the
    difference of the two sample points - the live position moves to
    the given aim point PLUS the step when the object is not free.
    That is the third mover of the live position, after PR-354's
    ground_on and lock2 runners. The second point-derivation PR-364
    found at 0x4552f is NOT part of that query: PR-365 measured the
    boundary (0x45400's body ends at 0x454c6 `ret 0x4`, 0x454c9-0x454ff
    is padding) and read 0x45500 as its own function - it runs the
    requester's +0x68 accessor and its +0xb0 provider, adds the velocity
    words, tests the result against three words of the callee
    ([ecx], [ecx]+[ecx+0xc], [ecx+4]) and on a hit raises the asker's
    Y velocity [ebx+0xce] by the remaining gap; it is the shared helper
    of the BAL (balloon) class 0xa17dc, reached only through that
    class's verb +0xc, and nothing in the image calls it directly.
    PR-365 also split the two object families: over PR-237's 186 anchor
    kind tables, +0x58 is 0x54eb0 in 155 and 0x23ba0 - `mov [eax],edx;
    mov [eax+4],ebx; ret`, a plain pair copy - in the other 31, and the
    +0xb0 slot of those 31 is not a provider at all (28 return a
    companion object, 2 mark themselves asked, 1 tests state).
    The +0x98 verb the two frame steps call is
    now answered too: 0x23a50 in all 155 tables, `lea edx,[eax+0x75]`
    then a call to 0x534e0, and 0x534e0 is one instruction, `ret`.
    It is a no-op, everywhere.
    PR-391 CLOSES THE `.PLT` HALF OF THIS ROW. The provider table the
    landing chain reads is the level's own ground file: the level-config
    parser matches the keys `platformy`, `blokady` and `sufity`
    (literals 0x91e1e/0x91e2c/0x91e38, compared by 0x76d20, values kept
    at [ebp+0x7a]/[ebp+0x8a]/[ebp+0x9a]) and the same function opens
    each name with 0x4a910 and calls 0x4dc00, 0x4db50, 0x4da90.
    0x4dc00 makes one 12-byte container per polyline of the `.PLT` text
    (count*12+4 bytes at [world+0x74], the file's header count at
    [world+0x80]) and calls `[element class + 0x20]` = 0x45120 per line;
    0x45120 allocates the 13-byte records whose count is at [element+8].
    Measured live in ZARIA.01: 45 of 45 elements equal ZARIA_A.PLT's 45
    lines in file order - every x, every y, and every flag byte the
    parser writes (tools/golden_surface_read.py). The authored census is
    answered too: no `#` separator exists in any of the ten shipped
    `.PLT` files (0 of 1749), so the parser's flag-2 branch is dead in
    shipped data. WHAT IS LEFT is what that flag byte decides: 0x45250,
    slot +0x8 of the SAME container class 0xa0cc0, gates on the FIRST
    record's byte (`movzx edi,[rec+0xc]`, `cmp edi,ecx`, `jg`) and on a
    pass writes that record's x,y to [obj+0x89]/[obj+0x8d] - the sample
    pair of OI-129 - plus deltas at [obj+0xca]/[obj+0xce].
    PR-392 ANSWERS THE REST OF THAT PARAGRAPH AND NARROWS IT. The byte is
    a MINIMUM QUERY MODE: the same comparison is the first instruction of
    both verbs (0x45417 in the ask 0x45400, 0x45258 in the clamp 0x45250),
    the mode comes from bit 0x4000 / bit 0x0100 of the state control word
    at state+0x12 (OI-078), and the array is the level's own ground table,
    walked one `call [element class + 0x1c]` per element at
    0x4b67e-0x4b6a8. The write is a clamp to the line's two ENDPOINTS,
    not a per-record write: record 0 when the query x is left of it, the
    last record - array + [element+8] * 13 - when the query x is right of
    it, nothing in between. The delta pair and the ask's clearance
    [obj+0xea] are added to the live position once, at 0x54825-0x5482d,
    and then all three are cleared (0x5481b, 0x54830-0x5484a). And the
    "0x58c counter" this row keeps listing is not a counter: 0x58c is
    compared against [obj+0xc6], the signed v_y, at 0x5476f - a flag-2
    answer is dropped at or above terminal fall.
    PR-394 MEASURES THE GETTER CENSUS THIS ROW'S +0x6C SENTENCE NEEDS.
    It is not universal. Two clean families, found by walking every view
    in 0x96000..0xa7000 from its +0x48 slot: 186 views with +0x48 =
    0x23aa0, +0x50 = 0x53e40, +0x68 = 0x23900 and +0x6c = 0x23a10 -
    PR-351's and PR-392's reading, confirmed for that family - and 33
    views (0x96e94 first, the STRUMIEN family) with +0x48 = 0x23be0
    (`mov eax,0xcf004`), +0x50 = 0x23bc0 (`mov eax,1`), +0x68 =
    0x23b60 (`ret`) and +0x6c = 0x23b50 (`ret`). For those 33 the state
    block the aim chain walks is the all-zero data block 0xcf004 and
    the sample pair is the object's own position, so any law stated as
    "[obj+0x89] is the sample point for every asker" needs that
    qualifier. 0x23b80/0x23b90 are a setter/getter pair on [obj+0x2c],
    the clamp output of 0x23b70 (4 views at +0x68, 1 at +0x6c).
    PR-399 READS THE OTHER HALF OF THE BAND AND THE FLAGS THE WHOLE BAND
    SHARES. The six slots +0xb0, +0xb4, +0xb8, +0xbc, +0xc0, +0xc4 are one
    contract, not six: the five dwords 0x544f0, 0x238b0, 0x23880, 0x23870,
    0x54fe0 in order occur 155 times, and in every one of those tables
    +0x44 = 0x23920, +0x48 = 0x23aa0, +0x50 = 0x53e40, +0x98 = 0x23a50,
    +0xbc = 0x23870, +0xc0 = 0x54fe0 and +0xc4 = 0x55020; the two new
    verbs are the LOCK-BIT pair. +0xc0 runs the sample chain and the
    element's +0x8 verb only when bit 0x01 of [obj+0xb6] is clear, and
    then ALWAYS `or BYTE [obj+0xb6],0x3` (0x5500e); +0xc4 runs the chain
    and the +0x4 verb only when bits 0x01 AND 0x02 are both set, and then
    ALWAYS `and BYTE [obj+0xb6],0xfc` (0x55057). The pair-keying dispatcher
    this row named is one of five thunks at 0x45c00, 0x45c20, 0x45c50,
    0x45c80, 0x45cb0 that all open with PR-101's chain walk 0x457e0 and
    then test the surface record count at [handle+8]: 0x45cb0 chooses
    [CORRECTED BY PR-400: the value tested is the NODE's own +8 word,
    `cmp DWORD PTR [ecx+0x8],0` at 0x45cbb, not a surface's count, and
    the pair is not gated - non-zero runs the first member and zero runs
    the second, so one of them always runs. 0x45c20 tests nothing at all.
    PR-400 also reads the other 19 verbs of that row: all 25 are the same
    three steps - walk the chain with 0x457e0, then one action on
    [[edx]] - and none of the 24 per-type verbs has a direct caller.]
    +0xb4/+0xb8, 0x45c80 chooses +0xc0/+0xc4, 0x45c50 chooses the two
    single-bit functions 0x54f40 and 0x54f90 (which are NOT table slots:
    0 occurrences as a dword), 0x45c20 is the mirror of bit 0x20. Each is
    named exactly once, by field +0x04 of a row of PR-101's run at 0xa0f28
    - rows 25, 19, 18, 17, 16 - and nothing else in the image points
    there, so +0xc0 and +0xc4 each have exactly ONE call site, 0x45c96 and
    0x45ca4, both inside 0x45c80. The [obj+0xb6] bit map, every one of its
    44 sites re-read from an aligned slice: mask 0x01 has one literal
    setter (0x54f7b) and mask 0x03 one (0x5500e); masks 0x04 and 0x08 are
    set only inside the control-message dispatcher 0x54560 (0x545c6,
    0x545d2); clearing is 0xfe at 0x54fcb, 0xfc at 0x55057, 0xdf at
    0x5266c; the only computed write is 0x45c45, bit 0x20 from (edx&1)<<5;
    mask 0x10 is tested twice and never set by a literal. Bit 0x02 is
    therefore never set alone but CAN stand alone, because 0x54fcb clears
    bit 0x01 and leaves it. What stays open here (PR-400 read the 25
    +0x04 verbs and the six thunks, so the middle items below are closed;
    see PR-400, and PR-401 ANSWERS the first of them: three sites start
    a visit and all three select the row BY INDEX - the row-advance
    0x520c0 visits `[[rec+0x2e]+[rec+4]*4]` at 0x520e1, reached only
    through `call [ctx+0x44]+0x18` at 0x52338, and 0x74a98 and 0x74b63
    index a global slot 0xcf518 that the image fills once, with the
    static row at 0xa9ab8. No `call [reg+4]` in the image takes its base
    from a `[x+4]` load. [CORRECTED BY PR-402: 0xa9ab8 is not a class
    row. It is a four-entry BASE-IMPLEMENTATION table - `add eax,4; ret`,
    the virtual-this thunk `shl edx,3; add edx,[eax+4]; add
    eax,[edx]`, `mov BYTE [eax+8],0; ret`, and the initializer that
    writes 0xa9ab8 into 0xcf518 - belonging to an invoke-by-descriptor
    helper at 0x74a4c whose 12-way jump at 0x74b4d branches on a
    CALLING-CONVENTION kind word. Only 0x520e1 is a node-class visit.]
    [PR-403 SETTLES IT. That helper is the engine's own C++ runtime, not
    a game mechanism. 0x961fc/0x96200 is one engine class's vtable pair -
    ctor 0x1034c and deleting destructor 0x57a00: `test dl,0x4` to the
    free pair 0x74bc0/0x74be0 with the record 0xa27b8, else
    `mov DWORD PTR [eax+4],0x96200` - a switch to the base vtable - then
    `call 0x579e0` and `dl&2` to 0x5c370. That is exactly the shape of
    0x23ed0, which PR-401 met inside the row-advance, so 0x23ed0 is a
    deleting destructor too and not a "free-with-flags" of anything
    game-side. The bytes `00 ac 74 00` - the base constructor 0x74bac -
    occur 481 times from 0x961dc to 0xa6b6c, once per CLASS RECORD, and
    that is the field PR-313 counted "at +0x0c of 206 class tables". 257
    code sites name these vtables and every one is a ctor, a dtor or a
    vptr store. So the 0xcf518 calls at 0x74a98/0x74b63 are base-class
    legs of the engine's own construction, not visits of authored clause
    nodes: 0x520e1 remains the only node-class visit in the image.]
    [PR-404 QUESTIONS THE OPERAND ITSELF. The visit at 0x520e1 is still
    real, but `[rec+0x2e]` as a node array is no longer settled. 141
    instruction sites write DWORD PTR [reg+0x2e] and 140 of them are
    `mov [eax+0x2e], T` with T one of 140 DISTINCT class tables from
    0xa09f0 to 0xa67d8, emitted by 35 generated constructor functions in
    0x618b0-0x7412c that each end in `call 0x3fb30` - a base constructor
    that allocates through 0x216c0, sets a cursor at +0x16 to -1 and runs
    the PR-403 engine-class pair 0x96200/0x961fc with 0x57990. All 140
    tables share three verbs (+0x08 = 0x3fcc0, +0x10 = 0x3fbb0, +0x14 =
    0x3fc20): index an element array by the cursor, read the element's
    class at [element+8], call its +0x08/+0x0c/+0x10. So the field the
    row-advance indexes as data is written as a class table everywhere it
    is written in code, and neither reading is proven. Also note the 16
    record classes that hold 0x520c0 at +0x18 are NOT what those 140
    constructors build.]
    [PR-405 SETTLES THE APPARENT CONTRADICTION. There are three table
    fields, not one: node classes at [obj+0x04] (128 sites, of which
    106 are `mov DWORD PTR [eax+0x4],<row>`), the 140 generated state
    classes at [obj+0x2e] (0x3fb80 is such a class's destructor - it
    stores 0xa09f0 at +0x2e, then frees [obj+0x2a] through 0x74be0 and
    destructs the base subobject at +0x1e through 0x57a00; 135 of the
    141 objects are `mov eax,0x32; call 0x5c320`), and a state's
    implementation table at [obj+0x44] (42 sites, 17 of the 18 values
    are class tables; 0x52338 calls its +0x18). With that, PR-401's
    reading stands: `[[rec+0x2e]+cursor*4]` then `[+4]` is the
    class-table lookup of a node whose table is at [node+0x04], and the
    visited function is the +0x04 verb PR-400 named. Also new: 136 of
    the 140 generated tables have a `call <own +0x08 slot>; ret` thunk
    at their own +0x04, so calling their +0x04 runs the method at
    +0x08 - the mirror of the node classes.]
    [PR-406 NAMES THEM. A state record is a 0x60-byte object: 27
    functions fetch an authored string through `mov eax,IMM; call
    0x76a69` and then `mov eax,0x60; call 0x5c320`, and 42 sites attach
    a class table at [rec+0x44]. The strings fetched in those functions
    are A_DEAD, A_LOOKDOWN, A_LOOKUP, A_SQUAT, looser.mod, sadesdea.mod,
    TLewEna2, TLewKadmar2 - so THE AUTHORED STATE NAME SELECTS THE
    RECORD CLASS. Exactly 16 tables hold the row-advance 0x520c0 at
    +0x18 (the 14 known plus 0xa2e9c and 0xa2ed4), and their first
    sub-table is uniform: +0x0c 0x521f0 row step, +0x18 0x520c0, +0x28
    0x52470, +0x30 0x52790, with only +0x08 varying. 0xa25d4 is the
    DEFAULT, attached by the record ctor itself (0x524e0: `call 0x51700;
    mov [eax+0x44],0xa25d4; call 0x52540`). A_DEAD -> 0x97c9c;
    A_LOOKDOWN/A_LOOKUP -> 0x9a8cc/0x9a894/0x9a85c; A_SQUAT ->
    0x9e254. Those tables carry a SECOND sub-table from +0x38 with the
    PR-403 base-constructor field at +0x3c - 4 copies in 0x9a8cc, i.e.
    multiple inheritance - and the record shares its [obj+0xb6] lock
    byte with the game object (0x1e0a0 clears bit 0x04 and sets
    [obj+0xad] = 0x1f4).]
    [PR-407 FOUND THE CURSOR ITSELF. The row index that 0x520d2 reads at
    [rec+0x04] is set once, at state entry: the verb at class slot +0x1c
    (0x52360 for the four unnamed record classes, the per-state 0x4fef0
    for A_DEAD's 0x97c9c) ends with `lea eax,[ebx+0x2e]; mov eax,[eax+8];
    sub eax,edx; dec eax; mov [ebx+0x04],eax` at 0x523bc-0x523c5 - the
    cursor starts at the COUNT at [rec+0x36] minus the argument minus
    ONE, so authored clause rows are indexed from the last row downward.
    [rec+0x36] is that count, and 0x73090 fills it (with [obj+0x32])
    from `DWORD [k*12+0xced1c]` where k = [obj+0x3a] - PR-411 CORRECTS
    "is a kind": k is the ENTRY INDEX returned by the interval lookup
    0x37150, which scans a 12-byte-stride table of (low, high, anchor)
    x-ranges at 0xced18 and returns the first entry whose range contains
    its KEY, or 0x11 for none; 0x73082 stores that index at [obj+0x3a],
    and [obj+0x32] / [rec+0x36] are that entry's LOW and HIGH ends.
    PR-411 also said the high end is filled at run time and the third
    field is an unused group id - PR-413 CORRECTS BOTH: 0x36d90 copies
    all three fields of image entries 1..18 (0xa8f94..0xa9060 ->
    0xced18..0xceddc, image minus BSS = 0x25d84 uniformly), the lookup's
    signature is `0x37150(key=eax, probe=edx, start=ebx)` with the
    matcher 0x36d60 taking key=edx and probe=ebx, so at 0x73073 KEY =
    [[obj+0x26]] and PROBE = [[obj+0x26]+4]; the match additionally
    requires `|probe - [[entry+8]*4+0xa906c]| < 0xa`, the anchor table
    holding 61,154,256,358,451,547 (steps of about 96). The scan bound
    0x11 covers indices 0..16 - the 17 real ranges - and skips image
    entry 0 (4,0,3), which is not copied, and image entry 18
    (-10000,10000,0), which is copied to 0xcede4 but never scanned. The
    object then measures 10 units in from both ends and asks its
    container a point question at each (0x730c6-0x730f4). Also: a record class
    differs from another only at +0x08 (six values) and +0x1c (two), and
    0x523f0 shows the call-in `mov esi,[eax+0x44]; call [esi+0x08];
    call [esi+0x0c]`. THE PORT WALKS ITS LADDER FORWARD - this is a
    checkable difference.]
    [PR-412 NAMES THE DIRECTION LAW AND ITS TWO GUARDS (PR-101 had read
    the expression; ported/ll_play.c:763-775 records it as unmodelled).
    The row-advance 0x520c0 computes, at 0x5214b-0x52193, `dir =
    ([[obj+0x26] vtable +0x70]() == -1 AND control bit 0x10) XOR
    ([obj+0x71] == -1 AND control bit 0x20) XOR ([obj+0x99] == -1)`, and
    then 0x5219a-0x521d7: dir != 0 walks the cursor DOWN from count-1
    with `[obj flag] |= 0x8` at the bottom, dir == 0 walks it UP and sets
    `|= 0x4` and returns from the verb at the top. THE FIRST GUARD IS THE
    SEGMENT PREDICATE OF PR-411 - the same vtable slot is called one
    instruction after `+0xa`/`-0xa` against the two ends of the interval
    at 0x730d3 and 0x73438, answering -1 or +1 - so SEQ_REV_X means
    "reverse at my segment end", not "walk backwards". THE SECOND GUARD IS
    THE OBJECT'S OWN DIRECTION: [obj+0x71] is written only 1 (0x54ba5)
    and -1 (0x54b9a), in the same block of 0x549f0 that sets the
    horizontal and vertical speed accumulators to 10 and -10. NOTE THE
    TWO SITES DISAGREE: at state entry 0x523a0/0x523ad enables the same
    operands on `>= 0`, in the row-advance on `== -1`; and [obj+0x99] is
    written 1 by 0x52377 and 0x53883 but only ever tested against -1, so
    the third operand has no known producer.]
    [PR-414 READS THE STATE-CLASS TABLES THEMSELVES. The 140 tables
    stored at [obj+0x2e] are 7 dwords each, not an opaque block: slot
    +0x08 is 0x3fcc0, +0x10 is 0x3fbb0 and +0x14 is 0x3fc20 IN ALL 140,
    while slots +0x00, +0x04, +0x0c and +0x18 differ in all 140. The
    shared trio is base-class plumbing over the child index at
    [obj+0x16] - 0x3fcc0 is `if ([obj+0x16] != -1) call
    [[[[obj]] + [obj+0x16]*4]+8 +0x08]`, i.e. delegate to the child -
    so a generated state class contributes exactly four functions. The
    tables are 28-byte objects in 34 runs (biggest 12 at 0xa44ac, 11 at
    0xa6600, 10 at 0xa3dd8), and a ctor may install TWO overlapping
    views of one table at other fields (0x727a7/0x727ae store 0xa6520 at
    [obj+0x8] and 0xa6518 at [obj+0xc], which are the same table at
    entry 2 and entry 0). THE SEGMENT MACHINERY OF PR-411 RUNS AS THE
    +0x0c VERB: 0x72d50 is at 0xa66a8+0x0c, with siblings 0x72cb0
    (0xa66c4) and 0x72330 (0xa640c).]
    [PR-415 READS THE BASE HALF OF THOSE TABLES. The table at 0xa09f0 is
    not a generated one: its slot +0x18 is 0x74970 = `if (!word
    ds:0xa9bac) { word ds:0xa9bac = 1; call 0x77bdc(1, 0x95c20) }` and
    0x95c20 is `pure virtual function called!`, so the state-class base
    is an ABSTRACT class and the 140 tables are its subclasses; two sites
    install it (0x3fb73, with [obj+0x2a] = 0, and the base ctor 0x3fb80
    which chains to 0x74be0). The three "shared" verbs are ONE cursor
    mechanism, and [obj+0x16] is an ITEM CURSOR, not a child index:
    [[obj]] is the item-pointer array, [obj+0x08] the item count, -1
    means no current item, [obj+0x1a] holds the boolean returned by the
    current item's +0x0c verb (0x3fc53). The object also owns an
    INSTANCE ARRAY at [obj+0x2a], one 8-byte entry per item: the base
    table's own +0x0c verb 0x3fce0 allocates it as count*8 via the
    `jmp 0x5c320` thunk 0x76a64 and 0x3fd70 fills each entry with
    (sub-index 0, item index). The helper 0x3fd00 is the lookup: if
    `[[[obj+0x2a]+idx*8]+4]` is zero it calls the STATE'S OWN +0x18 verb
    - one of the four per-state functions - and caches the answer
    (0x3fd3c-0x3fd4e), so slot +0x18 is a per-item factory, not a tick.
    THE PORT HAS ITEM LISTS BUT NO PER-ITEM INSTANCE LAYER.]
    [PR-416 FINDS EVERY USER OF THE SEGMENT LOOKUP: SIX CALLS TOTAL, AND
    ONLY ONE REAL OWNER. 0x37150 is called from its own seeder's function
    (0x36f42), from one wrapper 0x371b0 (0x371bd, 0x371cb) and three
    times inside FUN_00072d50 - the state-class +0x0c verb of PR-414
    (0x7307d, 0x733f4, 0x735bb). The matcher 0x36d60 has one caller and
    the seeder 0x36d90 exactly one: 0x38a60, a FOUR-CALL sequence `call
    0x36650; call 0x36d90; call 0x37af0; call 0x37ff0` - so the
    intervals are loaded once from the executable's own data and never
    re-seeded. AT EVERY SITE THE KEY IS THE CONTAINER'S FIRST DWORD AND
    THE PROBE ITS SECOND: `eax=[[obj+0x26]], edx=[[obj+0x26]+4]`
    (0x73073), the same pair through a dispatcher arm (0x733ed), or two
    calls of the container's vtable +0x6c getter (0x735a8, 0x735b6), with
    the answer cached at [obj+0x3a] or [obj+0x42] and every caller
    branching on the 0x11 result. 0x733fe-0x73407 also writes the identity
    `entry = 0xced18 + idx*12` in two instructions. THE WRAPPER 0x371b0
    looks one probe up against two keys, [obj-0x14] and [obj+0x5a], and
    passes the two indices to 0x371e0, which PR-416 reads exactly: it
    SWAPS two table entries - high end into high end at 0x37202/0x37208
    and low end into low end at 0x3720f/0x37216 - then returns at once if
    the segment count 0xcedf0 is 0. PR-413's phrase that 0x371e0 "fills"
    high ends is replaced: its writes are swaps.]
    [PR-410 SETTLES THE control= TABLE, AFTER TWO WRONG DRAFTS. The
    state-header parser 0x51913 reads `time`, `sample`, `time_scale`,
    `seq` and `control`, zeroes [rec+0x12] at 0x51b2e, then chains
    `mov edx,<keyword>; call 0x76a69; test eax,eax; jne <next>` - and
    0x76a69 upper-cases both strings and returns the byte difference
    (0x76a92-0x76aa9), so ZERO IS EQUAL and the OR after the jne is the
    MATCH ARM of the keyword literal loaded at the top of that block.
    The complete table, by the record byte the engine writes:
    [rec+0x12] BY_X sets NOTHING (its match arm is `je 0x51dc3` at
    0x51b58, mode 0 is the zeroed default), BY_Y 0x01 (0x51b6f),
    BY_TIME 0x02 (0x51b89), BY_TX 0x03 (0x51ba3), BY_TY 0x04 (0x51bbd),
    FRAME_REV 0x08 (0x51bd7), SEQ_REV_X 0x10 (0x51bf1), SEQ_REV_Y 0x20
    (0x51c0b), CHECK_ANI 0x40 (0x51c3f), CHECK_ZW 0x80 (0x51c25);
    [rec+0x13] CHECK_LN 0x01 (0x51c59), CHECK_LEW 0x02 (0x51c8f),
    SOLID 0x04 (0x51cc3), CH_DIR 0x08 (0x51cdd), INV_V 0x10 (0x51cf7),
    DANGER 0x20 (0x51d11), CHECK_ALL_PLANE 0x80 (0x51d2b);
    [rec+0x14] ANTI_SOLID 0x01 (0x51d45), ANTI_POCISK 0x08 (0x51d93);
    macros CHECK_ALL = WORD 0x1c0 (0x51c73), SOLID2 = DWORD 0x20400
    (0x51d5f), SEMI_SOLID = DWORD 0x40400 (0x51d79). The separator is
    `,` OR `|` (0x51dd8 tests 0x2c, 0x51ddd tests 0x7c) and an unknown
    token prints `Bledny control %s w pliku %s.` (0x9223b at 0x51dac).
    THIS IS WHAT PR-99 AND PR-100 AND tools/re/mov_channels.py ALREADY
    HAD: the tool's ENGINE_CTRL is correct bit for bit, and the mode
    field is clean - recomputing the census from these masks reproduces
    the published 2121 BY_TIME / 110 mode-0 / 13 BY_Y / 12 BY_TY / 0
    BY_TX, and no shipped line reaches a mode outside 0..4. THE
    ROW-CURSOR MIRROR IS SELECTED BY SEQ_REV_X XOR SEQ_REV_Y (0x10 at
    0x52385, 0x20 at 0x523a0, XOR at 0x523b8; repeated at 0x52158 and
    0x52171) - NOT by FRAME_REV, whose 0x08 no code in the image tests
    at all. Rare on shipped data: SEQ_REV_X 7 states, SEQ_REV_Y 22. The
    only bit tests anywhere on these three bytes are 0x10, 0x20 (row
    path), 0x40, 0x80 and 0x02 at 0x527db/0x52805/0x527b3 in the record
    class's +0x30 collision-check method 0x52790; what sets
    [rec+0x12]&0x8000 and the other 22 bits is still open.]
    WHICH NODE TYPE EACH ROW IS - each row is installed at
    [x+4] by two to nine ctor sites in 0x45750-0x47d50, and those are the
    way back to the authored directive, but none has been tied to a name;
    why the point provider runs the element's LEAVE verb; and a setter
    for bit 0x10.

OI-129: the shipped landing step is ~31 px for the lion's standing
  states, the port does not model it, and its fixture legs are green -
  which of those three statements is wrong
  status: PARTIAL (PR-390 answers the ordinary landing, PR-391 names the
    provider table and the geometry of the ask; the named ground_on
    script remains unexercised and the separator flag byte is unexplained)
  origin: PR-360 (the .SEQ element anchor census)
  notes: PR-357 read the land/leave pair (0x42980/0x429d0) as one
    inverse transform over the object's live position; PR-359 modelled it
    in the port without wiring it; PR-360 measured what shipped data
    puts in it. Through the ZARIA.01 HD-overlay scripts
    (ANIMACJE/ZARIA.01/LEW.ZAR/LEWPL.MOV, each [STATE] naming its art
    with seq=), every frame of the lion's A_WALK and A_ZABLOK states
    carries the step (0,-31) - with the engine's `sub [edx+4],(f2-f6)`
    that moves his position DOWN 31 px when he enters the standing state
    and 31 px back when he leaves it - and the neighbouring states scale
    with the pose: A_FIGHT_DOWN ~22, A_WSP_LIN 38-46, A_ODLOT a constant
    59, A_DEAD from 55 down to 4 as he collapses. So this is not a
    rounding detail: it is a body-height-sized offset applied at the same
    instant the standing bit flips (PR-354's [obj+0xB6] bit 0x01).
    The contradiction: the port does NOT model it (it counts ground_on,
    ground_off, lock2 and unlock2 unmodelled, PLAN 8g) and yet the
    fixture suite is green - 69 legs ok with the CD and HD overlay staged
    (the three named SKIPs are the no-regression baseline and the two
    STATUS.LEO legs, not data holes). So either (a) none of the covered
    legs crosses a standing transition of the lion, (b) the port's own
    ground/contact model lands him at the same place by a different
    route, or (c) the legs would notice and something else is off.
    WHAT WOULD CLOSE IT: one targeted measurement, not more reading.
    Take a level moment where the lion transitions into standing - the
    KATAPULT launch or a PALMA step in ZARIA.01, or any landing in
    TUNGA.11 - and compare the original's frame (the golden rig's
    pose-dump half, verify-golden) with the port's [obj+0x00]/[obj+0x04]
    trace across that instant. If the original's position steps ~31 px
    and the port's does not, the pair has to be wired (which needs the
    current frame's element per object, PR-357's [view+0x48] chain) and
    the port's own seat/ground offsets re-read against it. If the
    original does not step, then the element the engine passes at that
    moment is NOT the current frame's, and PR-357's chain needs re-read
    at the call site.
    SECOND, SMALLER: what point of the frame the second anchor names.
    It is not the shape height (f6 - f2 == int16(b17) in 0.1% of
    elements) and not the bottom edge (f6 == int16(b17)-1 in 15%); f4 ==
    f0 in 71%, so the x leg is usually absent. The ZARIA.01 per-state
    numbers (constant within a state, pose-tracking across states) say
    body line, and the 20 LITM.SEQ elements that hold 0x7fff in f4/f6
    with f0 of -1 or -4 are the oddity that would explain the rest.
    PR-362 REORDERED THIS and the nearer half is no longer the video
    comparison but the ask itself. The pair's position argument is a
    POINTER (0x4299c writes [edx+4] in place, 0x4299e/0x429a0 use ebx
    only as a sign test), and of its five call sites two do not pass the
    object: the class verb +0x54 - 0x53df0 in exactly 31 class tables
    (PALMA, KATAPULT, SPLYW) copies the live position into
    [obj+0x89]/[obj+0x8d] and then applies the shift to THAT pair
    (0x53dfa, 0x53e27, 0x53e2b), and 0x55070, which 124 tables install
    (the lion's view among them), hands the same pair to the ask's aim
    filler 0x541f0 as its block (0x55084, 0x5508c), which ends in the
    same method at 0x5423a. PR-363 READ THOSE TWO WRITERS TO THE
    INSTRUCTION and they are not one law: 0x53df0 has no test at all, so
    for the 31 classes that install it (PALMA, KATAPULT, SPLYW) the pair is
    always anchor-shifted; the 0x541f0 chain has THREE tests, and
    0x54200/0x54207's test of [obj+0xb6] bit 0x02 - the `lock2` flag
    PR-354 read as the `2` variants' extra bit - FORCES the chain past
    both the held-surface predicate and the standing test. The boolean is
    `bit0x02 || (!held && !bit0x01)`, and leg L17 (`./ll_level
    --mov-selftest`, prefix `seat sample:`) pins it with 13 cases plus the
    ask on top: the seat (838,463)-(900,463) is refused with the sample
    point at the requester's own row and ANSWERS with the incumbent -12
    once bit 0x02 forces the shift, and the x leg alone decides whether
    0x45380 names a pair at all (838,463 answers, 798,463 does not) - all
    of it without the object's position moving a pixel. THE COUNTERPART
    IS NOW READ (PR-364): the class verb +0xb0, slot of the point
    provider, pairs perfectly with +0x54 - all 124 tables install the
    SAME handler 0x544f0 there, and its three tests
    (0x54507 `jne` PAST, 0x54516 `jne` INTO, 0x5451f `je` PAST) are
    the exact negation of the chain's: the provider returns the
    position shifted by the element's LEAVE method exactly when that
    chain skips. A lion-class object therefore keeps two sample
    points at once, [obj+0x89] = position + the step while free and
    [obj+0xd6] = position - the step while standing or holding, and
    their difference IS the live step; the class verb +0x58 (0x54eb0,
    slot +0x58 of all 155 tables, one call site at 0x3ede2) is a
    third mover of the live position, adding that difference to a
    caller's point and re-running +0x54. And [obj+0x89] is exactly where
    the standing query measures its first clearance - +0x6c is `add
    eax,0x89; ret` (PR-349) and 0x45462 feeds it to 0x455a0 (PR-352's
    c1). So the engine's ask is aimed ~31 px below the lion's position
    while he holds no surface, and the live position is shifted only by
    PR-354's two runners 0x54f40 (the `lock`/`ground_on` ON runner, which
    sets bit 0x01 immediately after), 0x55020 (`lock2`) - neither of
    which the port models at all (PLAN 8g) - and now by 0x54eb0 too.
    THAT TEST IS WRITTEN (PR-363, leg
    L17, prefix `seat sample:`): the same ask with c1 at the
    anchor-shifted pair, refused or answered purely by whether the chain
    ran, and the x leg deciding whether 0x45380 names a pair at all - no
    position moved. So what is left of this item is the ORIGINAL's number,
    not the port's model: a pose-dump across an instant where the script
    runs `ground_on` (PR-354's 0x54f40, which applies the shift to the
    LIVE position and sets bit 0x01 immediately after), because the
    engine's ask is aimed one body-offset below the position while the
    object holds no surface. THE PR-362 OPEN LIST IS ANSWERED: the
    "unread leave-shift sites" split - 0x5453a is the provider's chain
    (now read), and 0x3c3a3 and 0x5efdb are NOT element leaves at all,
    they are `call [class+0x08]` ask verbs the PR-362 window heuristic
    confused with the element block's +8 (PR-362's own record named
    that false-positive risk); +0x54 has EIGHT indirect call sites, not
    one (PR-364 lists them), and 0x541f0 itself is called per step
    through 0x45340 and 0x4b62c (the latter is PR-346's candidate
    collector, which adds the velocity words [obj+0xca]/[obj+0xce] to
    the aimed point - the ask looks one frame ahead); [view+0x98] is
    0x23a50 in all 155 tables, a no-op (its call target 0x534e0 is
    `ret`); [view+0xb0] is the point provider above; and [obj+0x6d]
    has measured writers now - the `neg DWORD PTR [eax+0x6d]` at
    0x53683, reached when the state record's +0x12 byte bit 3 is set
    (0x5367e `test dh,0x8`), on top of the ldir/rdir writes PR-320
    named (the immediates 0x1 seven times and 0xffffffff four). WHAT
    IS NEW OPEN: who calls the vftable slot +0x18 that holds 0x45500 -
    PR-365 read that function (its own body, not 0x45400's: 0x45400
    ends at 0x454c6) and it is the BAL class's shared helper, reached
    only through the class's verb +0xc, with no direct call anywhere in
    the index; what PALMA's provider stub stores into [obj+0xb1] and
    feeds to bit 0x40 of [obj+0x66] (PR-365 found a second handler
    0x22df0 doing the same to [obj+0xbd], so it is a marker shape, not
    a one-off), who calls the +0x58 placement path at 0x3ede2's
    function, and what the six singletons ds:0xcee60..0xcee74 that the
    BAL's helper belongs to are. CLOSED by PR-365: whether those
    tables install a different +0x58 - all 31 install 0x23ba0, the
    plain pair copy without the anchor delta. CLOSED by PR-366: the
    +0x58 caller. Its one call site sits in 0x3ed70-0x3ee27,
    the latched +0x14 handler of five record-class tables (two named by
    PR-338: KTRASA 0x9dbfc, TUNTRAS 0x9f7b4); the companion it moves was
    resolved BY NAME at the class's +0x18 (0x3ed40 matches the string
    'ANITAB' = PR-303's 0xa1eb4) and stored at [obj+0x3c]; the pair it
    passes is the owner's own position, returned by its +0x90 verb
    (0x36150); and for ANITAB the +0x58 that runs is 0x23ba0, the plain
    pair copy - so the anchor delta is not what carries the rider. What
    is left of that item: which of the 33 `call [reg+0x14]` sites runs
    the handler, and what the three unnamed tables are.
    CLOSED by PR-367, the rest of the +0xb0 question and the
    "singletons": those 31 handlers are 26 getters of the form `mov
    eax, ds:0xID; ret` (PR-365 said 27; 26 is what the bytes give),
    and what they hand out is an ID, not an object. EVERY one of the
    186 anchor kind tables answers the class verb +0xc with exactly
    that getter; the ids come from 16 startup runs of `call 0x44350`
    (a post-increment counter over ds:0xa90ac, statically 1 in the
    image) writing 134 globals across ds:0xceba4..0xcf4e8, one per
    call site; and the engine tests an object's kind by ASKING +0xc
    and comparing that global - 14 of the 62 `call [reg+0xc]` sites
    do exactly that, and the test at 0x24743-0x24751 against
    ds:0xcee70 accepts the lion's four tables (0x96874 plus PR-214's
    0x9a134, 0x9ad48, 0x9e740). 107 ids serve 186 classes, 30 of
    them sharing ds:0xceec8, so an id is a family tag and not a type
    name. In those 31 tables the +0xb0 id is NOT the table's own:
    DRABINKA 0x97cdc is ds:0xceeac and names ds:0xcee8c, KPAJECZ
    0x9da40 is ds:0xcecf4 and names KKULA's ds:0xcecf8 (PR-338's
    names), BAL 0xa17dc is ds:0xcee68 and names ds:0xcee74 - so
    +0xb0 in that family is a companion reference, and the remaining
    five handlers are the asked-markers 0x1e540/0x22df0, the object
    getters 0x2b9b0 and 0x2dd00 and the state test 0x4fb00. PR-365's
    "singleton ds:0xcee68 with vftable 0xa2178" was two things
    merged: ds:0xcee68 is one of those ids (written only at 0x3d119,
    read only by 0x4fb60), and 0xa2178 belongs to a REQUEST BLOCK -
    0x4fb70 fills {0xa2178, 0, 0, 0x98278, 0} into a caller-supplied
    buffer, which is PR-237's ask signature. PR-368 ANSWERS THE FIRST OF
    THOSE and corrects the family counts above. What an id IS, measured:
    the tables of one id share every class verb - over the 11 id groups
    with more than one table (90 tables) 32 slot offsets are identical in
    every member, including +0x54, +0x58 and +0x6c, and no group mixes a
    position writer or a position mover (0 of 11); what differs per class
    is a short hook list - +0x90, +0x14/+0x18, +0x10, +0x60, +0x64,
    +0x78, +0x7c, +0xa8, +0xd0, +0xd8, +0xdc. The id is finer than the
    behaviour, never coarser: 0x55070 serves 66 distinct ids. Two more
    corrections: the two-family law is +0x58/+0xb0 (those two 155-table
    sets were checked to be identical), NOT +0x54 - that slot holds
    0x55070 in 124, 0x53df0 in 31 and 0x3ddf0 in 29 of the 186; and the
    kind-table set is 219, not 186, because +0x6c = 0x23b50 anchors a
    second family of 33 whose position verbs are bare `ret` (0x23b50,
    0x23bb0) - classes that are carried and do not carry, and PR-366's
    five record tables are among them, all five with an id getter at
    +0xc. 0x3ddf0 is the standing writer PLUS a second pass: it runs
    0x55070, asks +0x6c for &[obj+0x89], issues the request-block entry
    [0x98278+4] = 0x23ea0 with the offsets at [obj+0xfa]/[obj+0xfe]
    added to that point, stores the result at [obj+0x106]/[obj+0x10a]
    and translates the array at [obj+0xee] through 0x45200. PR-369 READ
    THAT ARRAY AND IT IS SURFACE DATA: 13-byte records holding x at +0
    and y at +4 and a flag byte at +0xc, kept SORTED BY x - 0x45200
    translates every record by the delta (that is why a plain add is
    legal) and 0x45380 brackets a query x in it, handing the two
    bracketing records to 0x455a0, PR-352's c1, or returning 0x7fffffff.
    PR-370 read the paths that copy that field and PR-371/PR-372 what it
    IS and WHO HAS ONE: [obj+0xee]/[+0xf2]/[+0xf6] is ONE EMBEDDED
    container - vtable, record array, count - built in place by 0x44fe0
    (PR-279's 0xa0cc0 family); the argument handed to its verbs is always
    the ADDRESS of the field (`lea reg,[obj+off]` or `add reg,off`, never
    a loaded pointer), so it is inline, and the offset is per class: 47
    build sites at +0xee, 7 at +0x44, 4 at +0xa5, and one translate at
    +0x106. The ten 0x45200 sites of PR-340's element region all use
    +0xa5 and pass COMPILE-TIME deltas (5,13) (6,13) (10,13) (10,3)
    (-0x1f,-0x2a), while the 0x3ddf0 writer at 0x3dccd passes the
    object's own position from [obj+0x8]/[obj+0xc]. Ownership is now a
    structural test: a class owns a point list iff a table with 0x45250
    at +0x8 sits at its method array MINUS 0x24 - the container's nine
    verbs are a PREFIX of the class's own method array, and one fixup
    function writes the three bases of that one array: class array at
    [obj+0x8], the verb block at [obj+0xee], the block minus one slot at
    [obj+0xc] (0x29a51-0x29a70 with 0x9a778/0x9a754/0x9a74c; its twin at
    0x2a183). 37 classes own one: ALL 29 of the 0x3ddf0 family, 2 of the
    31 0x53df0, 5 of the 33 second family (KTRASA 0x9dbfc and TUNTRAS
    0x9f7b4 among them) and 1 of a third writer - and NONE of the 124
    0x55070 classes, which is PR-368's two-writer split restated as an
    ownership law. The other half of PR-372, corrected by PR-373:
    [obj+0x8] and [obj+0xc] are ONE method array at two bases, and which
    base is used is the ownership test - a class whose objects store T and
    then T-8 (452 sites, 150 classes, the LION's at 0x1ed08: 0x96874 and
    0x9686c) has no point list, and a class that stores T, T-0x24 and
    T-0x2c (75 sites, exactly the 37 owners, disjoint sets, 32 classes in
    neither) owns one: the constructor writes the class table at
    [obj+0x8], the verb block OVER the 0xa0cc0 that 0x44fe0 had just left
    in the container field, and the second base at T-0x2c = block-8, in
    that order (0x1fc31 class 0x96c38, 0x35136 KTRASA, 0x1f302 class
    0x977f0; 5 of the 75 write the three bases with no build call
    nearby). So PR-370's "view pair 0xa23b8/0xa23b0" is one array read
    twice, and a call at [obj+0xc]+0x10 reaches the container verb
    0x45250. The records are
    authored as TEXT: 0x45120 reads "%d" then a " %c" separator - ';'
    storing flag 1, '#' flag 2 - and grows the array by (count+1)*13+4;
    0x45002 copies a container and 0x45070 asks twice and stores the
    answers. Arrays are shared, not private: 0x24aa0, 0x259b0, 0x1ece0,
    0x20200 and 0x23360 copy the three words - and the LION, being of the
    0x55070 family, owns no container at all, exactly as the law predicts.
    So the port, which measures every class against the level's array, is
    missing the object that answers surface questions for 37 kinds of
    owner. Seven of the 23 +0xb0 companion ids of the
    31-family are named as the +0xc id of a second-family table (ANITAB
    0xa1eb4 names ds:0xceea4 = 0xa1f90). The companion verb is 0x45320
    - the container's +0x18 verb, which PR-369 mistook for a class verb
    at +0x104 of 12 of the 29: it calls the aim filler
    0x541f0, then adds [other+0xca]/[other+0xce] and calls 0x45400.
    0x45200 has ten call
    sites, eight in PR-340's element region; 0x52b10 appends {next, ?,
    x, y} nodes to a list at [x+0x10] (66 sites) which 0x52b50 drains
    by running each node target's +0x10 verb, and 0x52b80 drains and
    then runs the owner's +0x14 - six call sites. Still open: what the
    flag byte means where 0x45258 compares it with ecx (`cmp edi,ecx;
    jg`) against PR-344's mode 2, and what the installed +0x0 override
    does instead of the base ask hook 0x45230 (32 of the 37 owners
    replace it; the five that keep it are exactly the second-family
    owners) - the +0x0/+0x4 hooks are one-instruction forwarders into the
    class (0x96c38: `call 0x1e540`/`call 0x1e560`, PR-340's element
    region) or, for the 0x3ddf0 family, into 0x3df50 and 0x3df90
    (PR-375): they OR one byte into the 13-byte signal key at
    [obj+0x60] (bit 0x20 of its byte 3, of its byte 0), 0x3df50
    stores its argument as the companion at [obj+0x112], and 0x3df90
    then asks that companion through its own verbs +0x3c and +0x70 and
    queues [obj+0x11a]/[obj+0x11e] scaled by the answers. PR-376 read
    those two verbs: they are PR-322's askable test (0x23c10 `xor
    eax,eax` in 204 of the 219 tables, 0x230a0 `mov eax,1` in 15 - a
    constant the author chose, never computed) and PR-321's facing word
    (0x239f0 `mov eax,[obj+0x6d]` in 186, and constant 1 for the 33
    anchored tables) - and all 15 askable tables are 0x55070-family and
    none of them owns a point list, while the four link wrappers
    (0x1f490, 0x1f6f0, 0x27626, 0x34623) only take the link after the
    CANDIDATE's +0x3c answers 1, recording the result as bits 0x40/0x80
    of the key byte at [obj+0x66]. PR-377 added that accepting a
    companion is an opt-in CLASS VERB PAIR: 64 of the 219 tables carry a
    slot pair reaching 0x3df50/0x3df90 - 31 of the 37 point-list owners
    (28 at table +0xdc/+0xe0, their block's own +0x0/+0x4) and 33
    classes with no point list at all (20 of the 0x55070, 7 of the
    0x53df0, 6 second-family; 23 of the 33 hold the two functions
    directly), and 6 owners carry no link verb anywhere - 0x96c38,
    0x96d1c, 0x977f0, 0x9dbfc (KTRASA), 0xa1f90, 0xa2054. Only 2 of the
    15 askable tables carry the pair, so askability and linking are
    independent switches. PR-378 settled the key question: ` F = %d, %d `
    at 0x91446 is one of 14 named property keys read by the same scanner
    0x498f0 that the container parser 0x45120 uses (` id = `,
    ` spawn = `, ` damage = `, ` expldist = `, ` type = `, ` sample = `,
    ` div = `, ` hit_time = `, ` coef = `, ` levels = `, ` Fcontrol = `,
    ` F = `, ` group = `, ` new_bounds = `), each with compiled
    defaults - `coef` eight floats 0.6/0.02/0.12/0.08/0.06/0.04/0.02/
    0.01, `levels` 0.1/0.2/0.35, `hit_time` 0.6 - and the ` F = ` scan
    runs on the SAME cursor as the point list, right after it. So still
    open: which signal bit that
    is (the ledger's numbering stops at 54), which class reads which
    property key - PR-379 said "none, per class", and PR-380 corrected
    that: the 19 scan positions it found are not table values because
    they are not function entries, but the 11 functions containing them
    ARE installed as class verbs, at slots that differ per class
    (0x41180 hit_time/coef/levels/Fcontrol in 83 slots of 53 tables,
    usually +0xd0; 0x40eb0 damage and the `, %d` pairs 73 of 45, +0x18;
    0x3e060 50 of 38, +0xa8; the `F`-chain 0x3dd60 47 of 34, +0x18;
    0x6fb10 group/new_bounds in none). The class is chosen by a chain of
    360 token compares of which 165 build an object - pr380m_pairs.tsv
    maps 185 token/table pairs onto 157 of the 219 class tables, and
    33 of the 37 point-list owners are named by a token. That settles
    the askable-token question: SEVEN of the 15 are named, and they are
    exactly the 'TLew' group tokens - TLewZaria 0x96960, TLewZariaB
    0x97a98, TLewEna2 0x98f34, TLewKadmar2 0x9b4d4, TLewKara 0x9e134,
    TLewJunction 0x9f540, TLewTunga1 0x9f668 - and eight (0x97970,
    0x988ec, 0x9a54c, 0x9c958, 0x9d338, 0x9f170, 0xa0624, 0xa2218) are
    named by nothing. Still open there: which classes the 11
    `F`-chain call sites (0x1f583, 0x2783c, 0x34477, 0x378c3, 0x3b506,
    0x3e776 and 5 more) belong to, and the 62 class tables no token
    names. PR-381 read what sits in those verbs and PR-384 named them
    correctly: the verb at +0x78 is the ASK ANSWER verb of
    PR-244/PR-249, not a load step. 0x552e0 (78 tables) builds the
    12-byte key, calls 0x53ae0 and tail-calls [T+0x9c], where 181 of
    the 219 tables hold the always-allow stub 0x23a40 (`mov eax,1;ret`)
    and 5 hold 0x3d980 (`cmp edx,[obj+0xee]` - is the point-list
    container set?); PR-388 identified that slot as where a class keeps
    its destructor: the 181 are one shared placeholder, and 7 tables hold
    destructor thunks - the four this-adjusted ones are exactly the road
    family (KTRASA, TUNTRAS, TRASA, TRASA2), and each thunk passes its
    own PR-387 descriptor record to 0x74bc0 before the free; 0x3df00
    (23 classes) asks [obj+8+0xd8] first and
    clears that container with 0x45320, 0x50f80 is the answer verb of
    exactly the 15 askable ones and asks [T+0xd0] with [obj+0xb1] when
    [obj+0x11a]==7, and 0x3f010 serves the road classes on the
    [obj+0x44] layout. The dispatch census over the image: +0x70
    (facing) 215 sites in 76 functions, +0x4 146, +0x68 145, +0x6c 91,
    +0x3c (askable) 74, +0xd0 15, +0x9c 5, +0x78 only 4 - and PR-384
    showed those 4 sites are the three ask walks 0x4b500, 0x4bd30 and
    0x553e0: the candidate filter at +0x60 is 0x23b80
    (`mov eax,[obj+0x2c]`) in 178 of the 219 tables - [obj+0x2c] is the
    contact node the port names, written from the class's +0x8 verb at
    0x1eb9a, so an object passes only while a shape meet stands; the
    plane gate is the [obj+0x24] equality at 0x4b5a1 ([obj+0x24] is the
    parallax plane the authored `plany=` names); an answer is 3 words
    plus a flag byte merged by 0x49580, and 0x4bd30 (CHECK_LEW) stops
    at the first non-empty answer while the other walks merge them all.
    PR-385 closed the filter census: of the 41 tables installing
    something else, 37 are `mov eax,1; ret` - the road group (KTRASA x2,
    TUNTRAS, TRASA, TRASA2 and the shared 0x9e294) takes the
    always-allow 0x36170 and the machine block 0x35d50-0x35eb0 covers
    KMASKA, KBLOKZ, KBOMBA, KKULA, KWINDA, KMIOTLA - and only four test
    anything: KROPLA/KROPLAS need |x - [[0xcef40]+0x394].x| < 100 via
    0x52d30, PILAGEN needs that member's +0x68 point inside its own
    rectangle at [obj+0xf2], PIOR_RAN needs [obj+0x1ba9] >= 0. LION,
    KAMIEN, DRABINKA and the shared animal class keep the default.
    Still open: who dispatches the property readers (their offsets are
    dispatched only from inside the answer verbs, and the same
    functions also have direct call sites), and why 0x553e0 asks every
    member of [[obj+0x40]] with no filter at all. PR-383 showed the 25
    functions that call the base answer 0x552e0 directly are themselves
    class verbs and 23 of them hold +0x78 - per-class answer verbs that
    delegate to the base one; it also settled the direction of the
    companion verbs: the accept verb at +0xdc is
    asked of the COMPANION's class (0x50c8a dispatches through
    [obj+0x112]+8), and 0x3b8a0 uses [obj+0x5c] as an index into the
    array at [obj+0x50], asking +0xe0 when element 5's +4 word is 2
    and +0xdc when element 6's is 0xc. Still open there: the type-code
    table, what indexes 5 and 6 are, and the verb [T+0x118] applied to
    an accepted companion;
    what 0x3e060's [obj+0x116] flag from
    a signal byte is, and what 0x1f330/0x1f590 test
    and what [obj+0x5c] == 9 selects (6 is a candidate-array index,
    PR-383), the six scanner sites whose
    format is not pushed in front of the call, the eight askable classes
    with no authored token (their ctor stores are at 0x1ff15, 0x1e0a6,
    0x22ee5, 0x2413d, 0x269c2, 0x2703e, 0x2bfd6, 0x2dfa0, 0x3204d,
    0x33dcd, 0x3624d, 0x3a489, 0x3a455, 0x3b1d2, 0x5011b), and why
    0x3dd60 has
    only 11
    sites for 29 [obj+0xee] owners when 0x3efe0 parses the same text at
    [obj+0x44] and 0x288d5 is unread. Settled at the same time:
    [obj+0xfa]/[obj+0xfe] are the FIRST RECORD of the object's own
    authored list, copied there by the load hook, and 0x3df90 subtracts
    [obj+0xfa] from [obj+0x106] - the standing second query is about the
    object's own authored home, and 0x3dcd2-0x3dcff repeats it against
    the companion. Still open there: the destructor's descriptor
    block (0x96560, 0x993ac), the one-word form of PR-370's five-dword
    blocks; the second base's own slot +0 (0x20750 only returns).
    Older open items stand: the 5
    sites that write the three bases with no build call nearby (0x29f4d,
    0x2b312, 0x37c7b, 0x37fa7, 0x38d6d) and whether those objects keep
    the array and count the base part left; what 0x531f0 does before
    `add eax,0xa5` in the same ctor; why the ctor at 0x1f302 is addressed
    from obj+0x106, the field pair the 0x3ddf0 writer stores its ANSWERS
    at; the 32 kind tables that pair neither -8 nor -0x2c - how their
    objects get a table at all; and the 8 owners outside the 0x3ddf0
    family by name: 0x96c38, 0x96d1c (the 0x53df0 pair whose methods are
    PR-340's element region), 0x99938 (+0x54 = 0x2b1a0), 0x9ef38,
    0xa1f90 (ANITAB's +0xb0 companion) and 0xa2054 besides KTRASA and
    TUNTRAS; settled since, what 0x98274 is: PR-150/PR-239 named it the
    element class of the authored point list and PR-386 showed it is no
    separate block but the tail of the owner's own table (0x98278 =
    0x981e8 + 0x90, 0x97f50 = 0x97e74 + 0xdc), the element being 12
    bytes {x, y, class} made at 0x23eb0; and what the 281 five-dword blocks
    are: PR-387 read them as per-class construction descriptors {ctor, the
    ctor/dtor slot or the 0x74bac stub that prints "undefined constructor or
    destructor called!", the class's vtable slot 0, the object size, 0},
    20-byte records at stride 0x14 with every record at 1 mod 4, naming 200
    of the 219 class tables (54 of the 62 no-token ones) and giving the same
    object size as PR-380's token branch in 144 of 144 cases. PR-397
    MEASURES ONE READER: 0x76b24 is the engine's array builder, and it and
    the two element-construction paths it chooses between (0x76ab0, which
    hands each constructor the element size in edx, and 0x7b82c, which
    hands it 0) read the first byte at [class+0], the constructor at
    [class+1] and the element size at [class+0xd]; the byte at [class+0]
    is 0 for 287 of the 290 records pr397_classbyte.py finds in
    0x96000-0xa7000 and 4 for exactly three array starts (0xa68a0,
    0xa6a74, 0xa6aa4), which no code passes to a builder - so no class
    that takes the 0x7b82c path is known. Two caveats for anyone reusing
    the shape: the fifth dword is not always zero (the two registry
    descriptors at 0xa14c0/0xa14d8 carry 0x01000000 and 0xb0000000 there
    and are 0x18 apart, so they are NOT such a record), and the byte a
    builder tests at [class+0] is the previous record's zero for any
    interior record, so a 4 can only appear at an array start. Whether
    anything else reads them stays open, as does whether 0x5c320 returns a
    pointer inside a larger block, whether the lion's own standing query ever
    consults such a per-object array (0x96874 carries neither the list
    pass nor a container sub-table), what the [x+0x10] queue drives, what
    0x57500's per-plane scale tables are authored by (PR-390 corrects
    PR-389's formula: it is [d+0x10] * T[plane] / T[[d+0x1c]], where
    T=[d+0x20]; 0x57532 is [d+0x14] * U[plane] / U[[d+0x1c]] +
    W[plane], where U=[d+0x24] and W=[d+0x28]. [d+0x1c] is a
    reference-plane index, not a table), and what 0x42de0's frame-list
    walk decides,
    which classes the 37 owners are by name - only KTRASA and TUNTRAS
    are - 16 of the 23 +0xb0
    companion ids, the six
    seeded ids no table names (ds:0xcee6c, 0xcee80, 0xceed4, 0xcf05c,
    0xcf4e0, 0xcf4e4) and the two ids named but never seeded
    (ds:0xceba0, 0xcf0e4).
    PR-390 MEASURED AN ORDINARY LANDING. The player object named by
    the live TLewZaria table 0x96960 changed `[obj+0xa9]` from the
    surface provider's element 0 to 0 when leaving the floor and back
    to that element when landing. Its `[obj+0xB6]` byte stayed 0xc0,
    and its live position showed no separate `(0,-31)` step in that
    transition; the measured `[obj+0x89]/[obj+0xd6]` sample pair was
    31 px while grounded, 32 px during the fall, and 27 px after the
    landing pose was selected. The question's ordinary-landing half is
    answered: `ground_on` is not the normal platform-landing path.
    Still open: whether authored script state ever invokes the
    0x54f40 runner, and what provider-record geometry makes the
    ask choose a particular element.
    PR-391 ANSWERS THE LAST CLAUSE AND MAKES THE PROVIDER READABLE. The
    provider array is the level's `platformy=` ground file: 0x4dc00 reads
    its header count into [world+0x80] and builds one 12-byte container
    per polyline at [world+0x74], 0x45120 parses one line into 13-byte
    records. In ZARIA.01 the live 45-element table at 0x51f144 equals
    ZARIA_A.PLT's 45 lines in file order, point for point, and the
    handle PR-390 followed is element 0, the line `1 -50,680!408,684;`.
    The element's third word is the line's vertex count at [element+8] -
    PR-343's `[handle+8]` - and the record class word is the container
    method array at image 0xa0cb4/0xa0cc0, the same table PR-372 used as
    the ownership test. Geometry-wise the ask is already explained by
    0x45380 (it rejects an x outside the first or last record and
    returns the first record index >= x) and 0x453d0 (hands out the two
    bracketing records). What is new open is the separator flag byte: the
    parser writes it at record+0xc for the separator FOLLOWING point i
    (';' to 1, '#' to 2, anything else 0) and never writes the last
    record's byte, so the byte 0x45258 tests is the first point's
    separator, authored - '!' on 7 of ZARIA.01's 45 lines including the
    one the lion stands on, ';' on the other 38. `tools/golden_surface_read.py`
    reads this table from a running level without stopping the guest.
    PR-392 READS THE STEP THIS ROW ASKS ABOUT. The ground query's own
    contribution to [obj+0x00]/[obj+0x04] is one addition per call: the
    clamp delta and the ask's clearance, x += [obj+0xca] and y +=
    [obj+0xce] + [obj+0xea] at 0x54825-0x5482d, after which all three
    words are cleared (0x5481b, 0x54830-0x5484a). That clearance is
    y_line(qx) - qy, interpolated by 0x455a0 between the two records that
    bracket the query x, so an ordinary landing step is however far the
    aim point sat below the polyline - not a constant, and not the .SEQ
    anchor pair. What it is in a running level is still unmeasured: the
    two PR-392 sessions with the extended tracer reached a level boundary
    but no playable ZARIA.01, so no TLewZaria object was sampled (see PR-
    392 MEASURED (i)).
     PR-398 NAMES THE TWO CLASSES BEHIND THAT TABLE, AND SAYS WHERE THEY
     COME FROM. Both arrays are built by the engine's one array loop, and
     the method arrays PR-391 saw at 0xa0cb4 and 0xa0cc0 are written by
     constructors, not by the loader: the .PLT loader 0x4dc00 builds its
     12-byte containers at 0x4dc72 through the class descriptor 0xa0c78,
     whose constructor 0x44fe0 zeroes +4 and +8 and stores 0xa0cc0 at +0,
     and the 13-byte records are built at 0x4515f, inside the parser
     0x45120, through 0xa0c64, whose constructor 0x456e0 puts the shared
     view 0x98278 at +8 and then overwrites it with 0xa0cb4. Of the 257
     constructors that these classes and the construction records name, none
     reads the second argument the loop leaves in edx
     (pr398_ctorargs4.py), and no class named at any build site selects the
     loop's second path (pr398_direct.py), so the fields these elements
     start with come from the class constructor alone and the two paths can
     be modelled as one.

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

OI-016: the BY_TY vertical feed law
  status: CLOSED BY PR-147/PR-148/PR-168/PR-178
  notes: the same constant 100 behind a `cmp [obj+0xbe],0` gate;
    shipped as LL_CTRL_BY_TY/vert_intent; no port gap. The climb
    BEHAVIOR remainder is OI-078.

=== engine RE: signals, conditions, event system ===================

OI-017: is the live signal set EVER CLEARED (pulse vs latching)?
  status: CLOSED BY PR-151
  notes: 0x749a0 clears it; the port's evaluate-then-clear ordering
    confirmed. Row kept for citation stability.

OI-018: raisers and semantics of AC_ONAIR, AC_DUP, AC_DRECT,
  AC_EDGE, AC_DRAB_CATCH
  status: PARTIAL
  notes: AC_ENEMY_ATC raised and ruled (PR-91/94/139); AC_BLOK/
    AC_BLOK_Y raisers PR-94 + relation providers PR-177; AC_EDGE
    via the ladder answer PR-141/149; AC_DRAB/AC_DRAB_CATCH raise
    PR-141, poster PR-146, and bits 12/22 have NO poster anywhere
    (counted negative, PR-146). AC_ONAIR's raiser and the semantic
    modeling of all five remain.

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
    tables, the 15 class names, the CAMEL/0x98278 role (a generic
    decor/mover in NEITHER key lineage)
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

OI-065: ZAR.BLK fields + the GRP format (WODOSPAD/KIJ/KILOF), the
  KAMIEN sixth field (1308/1218/1115) and the stone corridor
  1299..1620 (G-3) - read ZAR.BLK like a .PLT
  status: OPEN
  origin: PR-93 item 11.4; the placement lines naming them are
    counted as group headers, not placed

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

OI-072: the ground SOURCE remainder (G-13): the port loads ground
  from `platformy=` only and never parses the ani_plat= LOCAL
  ground lines (ANIPL2: KATAPULT/PALMA); engine-side the entity-
  record table chain (0x34850, [obj+0x48]/[obj+0x4c]) and its
  MOVEPLAT-chain provider are unread
  status: OPEN
  origin: PR-136/138/140

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
    the child-ANY branch (0x3f1e4/0x44ec0, sets [self+0x66]
    bit 6, signals nothing) and the ONLY state that answers the
    lion is index 2 = A_DOWN; A_DROP, the unrolling rope, is
    index 3 = inert. PR-150's row-cursor gate is gone, and the
    "binding order / encrypted pool" blocker of PR-157/158 is
    gone with it. Census: 172 of the 777 shipped .MOV texts
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
    TRANSITIONS, never motion; rdir is a randomizer (0x52070);
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
    iteration after A_DROP, not 25: both halves are open. What
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
OI-079: AC_DUP: the engine's attach machine is named (PR-179) -
  model it? what ends a JUMP early (does the original cancel
  recovery on the next input?)
  status: OPEN
  origin: PR-86 item 8.8, PR-150's ladder-overlap model, PR-179

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
  status: OPEN
  origin: G-7

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
  differ there today
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
  status: OPEN
  origin: PR-197 OPENS
  notes: records DO carry CORRECTS: lines since PR-198; the
    generator upgrade never happened, so the thread stands.

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
    read settled the half this row asked)
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
    0xa11e0 = 0xa11d8+8); it clones the row, composes the key
    with the global 0x98278, runs the row through its class's
    +0x10 with edx = 6 (0x5213d), tests the answer against 6
    (0x52140), and does the list-end/wrap: 0x521b6-0x521d4
    compares [list+0x8] - 1 with the cursor and OR-s bits 4 and
    8 into the CALLER's set byte 0 (0x521a1, 0x521c1) -
    PR-135's list-end bit, now named. What the row class's
    +0x10 verb DOES, and which authored number the row step's
    compare actually gates on, is OI-112.
  origin: PR-217 record; rewritten from the PR-218 record; the
    ladder chain stays OI-078
    PR-220 names the control bits that switch picks between:
    the `control=` parser 0x51b0d takes the first token of the
    line (` %[^,| \r\n\t]` at 0x9215d) and OR-s it into the
    record byte [rec+0x12]: BY_X = 1 (0x51b6f), BY_Y = 2
    (0x51b89), BY_TIME = 4 (0x51ba3), BY_TX = 8, BY_TY = 0x10,
    then FRAME_REV 0x20, SEQ_REV_X 0x40, SEQ_REV_Y 0x80, with
    more at [rec+0x13] (0x51c59). So 0x543a0's switch is
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
    (c) the OTHER half of the set match is handed. 0x52251
    copies the CALLER's key set, and the row walk 0x520c0
    composes that with the level-global set at 0x98278
    (0x520e6-0x520f6, named by PR-219) - so a clause can be
    answered by a LEVEL-wide signal, not only by the object's
    own raised bits. This port has no level-wide signal set;
    its gate reads o->sig alone. Measured consequence: with
    the gate in, a 200-tick ZARIA.01 probe traces ZERO
    `KAMIEN [A_STOP]->[A_GO]` where the same probe on the
    pre-fix build traced three - the stones are held by their
    own `1,0,WAIT K_GEN1` row, because nothing in this port
    raises K_GEN1 on a KAMIEN. Whether the engine's global set
    carries K_GEN1, and who writes it, is unread.

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
  a behavior table at [obj+0x00]
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
    the image ever writes [obj+0x00]: the three base ctors
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
    elsewhere - but nothing installs it.

# Lew Leon (Longsoft, 1997) — recompile project

Source-project replica of the Polish DOS game "Lew Leon". Original game
files stay out of day-to-day development (user-supplied CD, see
.gitignore); the upstream public snapshot additionally tracks a small
vendored core under `data/` (see below) - never add more copyrighted
game data.

Layout (mirrors the upstream repo at github.com/AdamIzydorczyk/open-leo):

- `ported/` — the whole native codebase, shared by every target:
  `.IS` codec (`is_stream.*`, byte-exact vs `tools/is_decode.py`),
  `llrs` container reader (`llrs.*`, byte-exact vs `tools/llrs_unpack.py`),
  container PCX reader (`ll_pcx.*`, byte-exact vs `tools/pcx_dump.py`),
  SDL2 cutscene player (`platform_sdl/`), title menu (`ll_menu.*`),
  level intro viewer (`ll_level.*`), shared entry (`main.c`),
  variant knobs (`ll_config.h`).
- `Makefile` (root) — single build for both native platforms:
  `make` → `ll_cutscene` (Linux ELF, SDL2 via pkg-config),
  `make win` → `ll_cutscene.exe` (Windows x86-64, MinGW-w64 or vendored
  `vendor/llvm-mingw`, static, no SDL2.dll needed).
- `original/` — faithful-DOS variant blueprint (Ghidra decompilation,
  frozen, not buildable).
- `data/` — the vendored CD core tracked by the public snapshot
  (ANIMACJE/ETAPY/SOUND/LEVELDES containers + LOGO/PRAWA/INTRO_P.IS);
  the full playable data set stays local/user-supplied.
- `tools/` — python reference implementations + oracle/self-test harnesses.
- `build/` — RE artifacts (leo.img, Ghidra project/logs; regenerate-only).

## Status (2026-11-16) - read HANDOFF.md before changing anything

**HANDOFF.md** is the map for whoever continues: what works, the rules
(never guess data, no game data/frames in git, every change with a
hand-derived test), the engine facts established from the machine code, every
estimate still in the code, the open questions and the ordered task list.

In short: the movies, sprites, parallax layers, music, title menu and
chapter map are done and cross-checked; a level shows the game's own 320x200
view (shown 2x) with per-layer scrolling, each authored object on the
plane its level's own `plany=` key names (PR-208), the objects as state
machines, the HUD from `BELKA`, and a lion that walks on the level's ground,
jumps (units estimated), collides with solids/enemies (their authored damage)
and picks balloons. Still missing for a playable game: the lion's attack and
other moves, enemy behaviour, pickups/inventory/signals, ladders/vines, level
flow, RAN's balloon mode, sound effects. Play mode keys: arrows, Space = jump,
Ctrl = attack, F1..F4 = weapon select, Alt = throw/secondary (viewer
mappings from the walkthrough; the .MOV data calls them K_JUMP/K_FIRE/
K_F1..K_F4/K_ALT - see NEXT_STEPS PR-87).
The arrow walk model is facing-relative (MODEL CHOICE, PR-85: K_FWD = the
arrow along the lion's current facing); ZARIA-style data with a single walk
state only reads as left-walking under that model — see NEXT_STEPS PR-85.

## Build

```
make          # linux
make win      # windows .exe (needs vendor/SDL2-2.32.10-mingw + llvm-mingw,
              # or a system x86_64-w64-mingw32-gcc; both gitignored)
```

Supported Windows baseline: **Windows 10 1903+** — the embedded
`resources/app.manifest` activates the UTF-8 code page, so game-data
paths with non-ASCII characters (Polish folders etc.) resolve; on older
Windows the game runs but only ASCII-only data paths work (the binary
logs a `main: ANSI code page …` warning in that case).

## Run

```
./ll_cutscene /path/to/LEO/INTRO_P.IS      # data dir = user's original files
./ll_cutscene --demo /path/to/LEO          # boot chain: LOGO.IS -> PRAWA.IS
                                           # -> INTRO_P.IS -> title menu ->
                                           # level intro (NEW GAME = ZARIA.01)
./ll_cutscene A.IS --then B.IS --menu /path/to/LEO   # manual scene chain
                                           # (`make ll_level` builds it: the default
                                           # `make` target builds ll_cutscene only;
                                           # set LEO_HD_DIR=<HD install> for the
                                           # authored scene and the lion's scripts)
./ll_level /path/to/LEO RAN.01             # level wiring (.DES) + intro art
./ll_level /path/to/LEO RAN.01 --play      # PLAYABLE level (M4-B): the lion walks
                                           # on the level's own ground (its .plt,
                                           # or the sub-.des carrying the same
                                           # polylines) with the real LEW.MOV
                                           # physics (arrows; LEO_PLAY_SCRIPT
                                           # drives it headless, see verify-play)
```

CLI contract (FIX-13): `--help`/`-h` prints usage (exit 0), `--version`
prints the version (exit 0); unknown `--flags` are rejected with
`main: unknown option` + usage (exit 2 — as are "nothing to run" bare
launches and `--demo <dir>` on a directory without ANIMACJE/ETAPY;
more than 16 `--then` entries warn on stderr and the extras are
ignored). stdout is line-buffered from startup (logs survive a crash);
on Windows `--console` attaches before any output and uses an
unbuffered stream. A missing cutscene in an interactive menu surfaces
as a message box.

q mid-cutscene QUITS THE APP (owner ruling, FIX-01/C3): the scene chain
and the menu never come back - same exit as closing the window.
ESC mid-cutscene SKIPS it and the chain advances to the next scene or the
title menu (original semantics): no frozen mid-intro frame, the app never
dies. Menu keys: up/down, Enter picks; ESC = Enter. The restart-confirm
dialog answers (tak/nie/x) are selected with LEFT/RIGHT (the words sit
side by side; up/down ride the same cycle) - the selected word is blitted
at the BRIGHT end of the palette's green ramp (idx22 #10f91b), gets a
bright underline, and pulses its authored 2-frame animation; unselected
words sit dim (top idx15) and frozen. nie starts out selected (the safe
default answer; also the word the original art ships bright). Enter
confirms, t/n/x are direct keys.

## Tests (both platforms)

Day-to-day the gate is the **Linux** half only (`make verify-all`); the
Windows legs are run ONCE at the end of implementation - owner ruling
2026-11-19, recorded as HANDOFF.md section 2 rule 8. The legs themselves
are unchanged and still exist; `make -k verify-all WINTEST=1` reproduces
the old single-command gate.

```
make verify-all           # the Linux gate (day-to-day)
make -k verify-windows    # the deferred Windows pass: verify-win + verify-mod
# verify       linux --ppm frame 1020 == python reference (byte-exact)
# verify-is-synth  data-free movie codec test: a synthesised .IS checked
#              against hand-derived values (LUT1 polarity, palette-only
#              redraws) - cannot pass by the twins agreeing
# verify-skip  linux ESC-skip final frame == python reference
# verify-win   [Windows pass] same two proofs for the .exe, run under wine
# verify-llrs  ported/llrs.c == tools/llrs_unpack.py on all 4 containers
# verify-pcx   ported/ll_pcx.c == tools/pcx_dump.py on all 65 container PCX payloads
# verify-seq   ported/ll_seq.c == tools/seq_parse.py on EVERY container .SEQ
#              payload (grammar walk + patch rows + element shape geometry)
# verify-pcm   ported/ll_pcm.c == tools/verify_pcm.py on all 23 PCM parallax layers
# verify-mod   [Windows pass] MOD player: parse of all 22 tracks == an
#              independent spec-derived python summary; render deterministic
#              linux == wine (determinism only - no independent mixer oracle)
# verify-level level scene: .DES wiring + .SEQ/.MOV anims + scroll camera
# verify-play  playable prototype (M4-B): real LEW.MOV physics + the level's
#              own ground (.plt, or the sub-.des that carries the same
#              polylines)
#              (hermetic selftest always; data legs on RAN.01 + live input +
#               an 8-level matrix + ground-from-.des (TUNGA2.12, SADES.06) +
#               the authored-empty ground (JUNCTION says "0" = no floor) +
#               TUNGA.11 - 12 of 13 levels run their own lion)
# verify-help  help overlay: POMOC pages == python oracle
# verify-menu  hooked demo chain: cutscene -> skip -> menu -> restart dialog
#              + bright-green answer selection (tools/verify_green.py oracle)
# verify-sel   LEVELS/MAPKA submenu + mapka.mod
# verify-save  STATUS.LEO/LONGSOFT.CFG byte-exact round-trip (skips when
#              the user-supplied originals are absent; never in git)
# verify-music MOD player behind plat_audio (threaded session, dummy-safe)
# verify-arch  structural pin: only platform_sdl*/ll_view/ll_audio may
#              include SDL, and nothing else may include those two headers
#              (T-01 end-state; compile-free grep)
# verify-quit  closing the window / quit propagation through every screen
# verify-utf8  UTF-8 and case-insensitive data paths (byte-exact read/write)
# verify-fuzz  parser fuzzing (llrs/pcx/seq/pcm/mod), sanitizers on
# verify-platform  headless engine path: clock/audio/input/fopen contract
# verify-userdir  per-user save dir: import-once, byte-identical, LEO_SAVE
# verify-cmake the alternate CMake build produces the same game
# py-refs      (prerequisite, not a test) regenerates the python reference
#              frames the byte-exact legs compare against
# verify-golden T-24: ORIGINAL game in the QEMU rig vs the port's chapter
#              map; the repo pins (tools/golden_expect.json) are HASHES
#              only - never frames. Skips entirely when the game fixture
#              is absent; without a QEMU capture it still checks the port
#              pose pins (make golden_rig builds the original-side capture)
# journal-db   NOT a test: rebuilds build/journal.db (the generated
#              ledger search layer) from NEXT_STEPS.md via
#              tools/re_db.py; `make verify-docs` calls it so it is
#              never left stale behind a committed ledger change;
#              tools/jq.py queries it. The DB is gitignored output,
#              a client of the ledger, never a source of truth.
```

CI hooks used by verify-menu (env): `LEO_DEMO_SKIP_FRAMES` (auto-skip demo
scenes), `LEO_SKIP_USER=1` (auto-skip reported as a user ESC skip),
`LEO_MENU_AUTO=N` (auto-pick menu item), `LEO_LEVEL_SECS` (intro hold),
`LEO_MENU_KEY=<char>` (auto-press a map corner: r = restart dialog;
`corner` = feed real Down keys + Enter through the map key handler,
walking every chapter pose then activating RESTART — verifies the
keyboard-only arrow/corner cycle headlessly),
`LEO_MENU_SEL=i` (pre-select answer zone i, highlight only),
`LEO_MENU_PPM=PATH` + `LEO_MENU_PPM_TICK=N` (dump the composed 320x200
frame), `LEO_MENU_PPM_DIR=DIR` + `LEO_MENU_PPM_MAX=N` (dump every composed
map frame map_NNNN.ppm for the golden-master pose scan),
`LEO_MENU_PICK=<char>` / `LEO_MENU_NAV=k` (fire / cycle + Enter),
`LEO_QUIT_AFTER_MS=N` (inject a window close N ms after the first frame —
proves the FIX-01 quit-unwind path headlessly; used by verify-quit),
`LEO_TEST_KEY_AFTER_MS=N` + `LEO_TEST_KEY=q` (inject one synthetic KEYDOWN
N ms after the first poll — pushed through SDL's queue, so it travels the
full map_key/repeat-filter path; the other half of verify-quit proves q
quits the app (FIX-01/C3)). `LEO_TEST_KEY` also accepts the names
`esc`/`space` and, since PR-43, the arrows `left`/`right`/`up`/`down`
(the verify-play live leg needs a real arrow press). Since PR-83 the
pair may carry `LEO_TEST_KEY_UP_AFTER_MS=M`: the matching SDL_KEYUP is
pushed M ms after the first poll too — without a KEYUP translation no
backend ever reported a release, so every key stuck down forever (the
verify-play key-release leg proves the walk stops on release).

Playable-level hooks (M4-B, PR-43, used by verify-play): `LEO_PLAY=1`
(the `--play` flag sets it) upgrades a level preview to the real
controllable lion, `LEO_PLAY_SCRIPT="<tick>:<FWD|BKWD|UP|DOWN|JUMP|FIRE|F1..F4|ALT|NONE> ..."`
(comma-combinable per entry, e.g. `100:FIRE,F1` - the AND-groups of the
real .MOV conditions, PR-87) holds each entry until the next one on the
fixed 20 ms grid (the same
grid `LEO_LEVEL_PPM`/`LEO_LEVEL_PPM_TICK` dump frames on, so CI frames
are deterministic); in a live window the arrows drive the lion directly.
`ll_level --play-selftest` runs the hermetic .plt/.MOV/player checks
(no data needed).
Stage-driver hook (PR-194, used by verify-stage): `LEO_LEVEL_EXIT=
"<tick>:<code>[,...]"` states that the level ENDED at that tick with that
exit code, and the play leg then answers it the way the engine's per-stage
caller does (the contract with every address is in `ported/ll_stage.h`; the
read is PR-193 F2/F3/F4): 3 resets the stage to its authored start and runs
on, 2 takes the queue leg - with no queue producer in the port that is the
engine's OWN empty-queue answer, stop -, 4 stops, any other value runs on
with NO reset.
That last answer now has a PRODUCER (PR-199; the read is PR-198): an
authored `.MOV` row tail `end_level <n>` is parsed as data, the row's
admit writes the level's exit-word slot (the engine's apply method
0x45750), and the play loop compares that slot against zero at the TOP of
every iteration - the engine's own test at 0x4ac55. A nonzero ends the
level run at that boundary and the level loop returns 1 for EVERY value of
n (PR-198 measured that the published copy of the number has no reader),
which is the "run on, NO reset" answer above; the port then stops the
level run there, because the engine re-enters the stage through a NEW
level object (its ctor 0x4a4c0 zeroes the slot) and the port has no such
re-entry point. `make verify-endlevel` counts the shipped rows through the
port's own parser (26 rows in 3 of the 777 `.MOV` members - JUNCTION,
TUNGA.11, TUNGA2.12; 25 x value 1, 1 x value 2 - and ZERO in the levels
these legs run, so no existing frame moves), and drives the path on
RAN.01 through `LEO_PLAY_ENDLV="<state>:<row>:<value>"`, which appends
such a tail to one authored row of the level's own `.MOV` text (the three
levels that carry the rows cannot be played here: JUNCTION's own data
says it has no ground, and TUNGA's `A_EXIT` is entered only by the arm
`A_EXIT(x)=AC_RECT,AC_BLOK`). From that byte on the code is the shipped
code, and the log names the injection. Without either variable the driver
prints nothing and changes no byte, which is why every existing play pin
is untouched.
`./stage_selftest` (built by `make stage_selftest`) pins the table itself
with no data at all.
Scene hooks (M4-C, PR-45, used by verify-level): the level preview now
runs the level's OWN authored scene - the .MOV scripts of the objects
its .des placement files (zwierzaki=/animacje=/ani_plat=) name, drawn
with the .SEQ art each script state selects through its own `seq=`.
`LEO_OBJ_ALL=1` prints EVERY placed object (the default proof listing
is bounded: 24 placements, then the count of the rest; the same bound
applies to the per-tick advance lines). `LEO_OBJ_POS=1` prints one
stderr line per scene-object DRAW (leaf, authored x,y, the parallax
layer index, the layer camera it is drawn through, the selected state/
frame and art element, the .SEQ anchors, and the exact screen origin) -
a read-only probe added by PR-207; it changes no pixel and no state.
`LEO_LEVEL_CAMY=<px>` sets the
initial VERTICAL camera (CI; in play mode the camera follows the lion's
feet, Up/Down pan it in the viewer). In play mode Space jumps (viewer key; `LEO_PLAY_SCRIPT` token `JUMP`/`FWDJUMP`); the jump
impulse units are estimates (see NEXT_STEPS PR-74). `LEO_PLAY_SOLID=0|1` (play mode) forces the object-contact pass off/on:
solid objects push the lion (ll_seq_shape_push, the game's FUN_00053ae0),
enemies hurt him by their authored damage= (HUD energy/lives follow) and
balloons heal him. Default: ON for every level except RAN.01 (the
balloon-flight level, whose walking start is not what the game plays).
`LEO_PLAY_ROW_PHYSICS=0` (play mode; the hook is PR-518's, its default
is PR-523's) is the physics-cadence switch. Default — unset, or any value
without a leading `0` — is the engine's own cadence, measured off the live
capture: the row clock is fed every 50 ms iteration, while the velocity
registers, the position, the ground queries and the bounds run ONLY when
the authored row step answers (a threshold crossed in a BY_X/BY_TIME state,
or a `Wait` clause matched), plus exactly one pass at the rising edge of
the intent word and one at a row whose clause refuses (a parked row parks
the cursor, not the body). `=0` restores the older per-iteration model for
comparison only: every selftest and play leg now carries the row-admit
arithmetic, so that path reads red against them (82 checks) and is not a
supported mode. The same PR moved the player's intent word
(`LL_PLAY_INTENT_FULL`) from 10 to the keyboard's 20 — the walk's row feed
is `v//100`, so below 14 no BY_X walk cold-starts at all. Scene objects
keep the measured object scale (PR-79/134) spelled out in `obj_walk`; it
does not follow the lion's constant.
`ll_level --mov-selftest` runs
the hermetic .MOV duration/cycle + per-state art-selection + patch-codec
+ anchored-draw + element shape-geometry checks (no data needed).

Data, save and CI hooks (every `LEO_*` variable the code reads is listed
here or in the paragraphs above):
- `LEO_DATA=<dir>` - the game data directory (the CD's `LEO` tree with
  `INTRO_P.IS`, `ANIMACJE`, `ETAPY`, ...), overriding the discovery chain.
- `LEO_HD_DIR=<dir>` - the HD-install overlay (`ANIMACJE`/`ETAPY` holding
  the `.MOV` scripts and level blocks). Without it a level has no authored
  scene and no player scripts (`ll_level` says "no HD ANIMACJE"); the
  `--play` examples above need it.
- `LEO_USER_DIR=<dir>` - per-user save/config directory, used verbatim;
  `LEO_SAVE=<file>` - the `STATUS.LEO` to read instead of the user dir's.
- `LEO_NO_MUSIC=1` - no level/menu music. `LEO_DEMO_LEVEL=<LEVEL>` - the
  level a confirmed restart starts (default `ZARIA.01`, the first level).
- CI/headless drivers: `LEO_DEMO=1` / `LEO_MENU_AUTO=1` (auto-pick NEW
  GAME), `LEO_LEVELSEL_AUTO=<n>` (pick level n in the level list),
  `LEO_MENU_SECS=<s>` and `LEO_HELP_SECS=<s>` (auto-close the menu / the
  help overlay after s seconds; unset = wait for a key, as the original),
  `LEO_HELP_PPM=<file>` (dump the help frame), `LEO_MENU_BG=placeholder`
  (flat menu background instead of the decoded art), `LEO_LEVEL_NO_OBJ=1`
  (do not composite scene objects), `LEO_LEVEL_CAM=<px>` (initial camera
  x), `LEO_DUMP_SKIP=<file>` (write the frame at an ESC skip and quit).
- intro-player timing diagnostics: `LEO_IS_PERIOD_MS=<ms>` (uniform frame
  hold), `LEO_IS_FPS_DIRECT=1` (the oldest "byte = fps" reading).

Runtime policy hooks: `LEO_FORCE_X11=1` (under a Wayland session pin the
video backend to X11/XWayland — the pre-FIX-11 default; default now is
SDL's own choice, X11-first on SDL2 classic), `LEO_PIN_ALSA=1` (opt in
to pinning the audio backend to alsa when SDL would pick its native
pipewire backend — the pre-PR-35 default; the default is now SDL's own
choice, which the owner's A/B ear test showed is clean on the
native-rate device. `LEO_KEEP_PIPEWIRE=1` is accepted as a no-op for
compatibility),
`LEO_AV_DEBUG=1` (per-second `is_player: av min_queued/err` A/V sync
diagnostics) + `LEO_AV_LATENCY_MS=N` (nominal output latency used in
that comparison, default 46 ms), `SDL_AUDIODRIVER`/`SDL_VIDEODRIVER`
(user pins always win over our hints/defaults).

Audio runs as ONE device for the whole process, opened at the hardware's
native rate (FIX-09 Part 2): each cutscene/MOD playback is a stream
converted on the fly, scene changes never close/reopen the device (no
WASAPI/Bluetooth reopen latency, no pop). The `ll_audio: device …` log
line therefore appears exactly once per run. `LEO_IS_WAV`/`LEO_MUSIC_WAV`
dumps stay byte-identical — they hold the un-converted source bytes.

## Naming

`is_*` = .IS cutscene format (oracle `tools/is_decode.py`); `llrs_*` = the
"llrs" resource container format (oracle `tools/llrs_unpack.py`); `ll_*` =
project glue that is neither wire format. One SDL2 platform layer
(`ported/platform_sdl/`) for both targets - no per-platform source forks.

## Both native builds are no-CD and use fixed-timestep pacing (the original's
execution-rate-bound busy loop would run thousands of times too fast on
modern hardware).

## Documents - a CLOSED set, pinned by `make verify-docs` P5: a tracked
`.md` outside this list fails the build.

Living:
- `README.md` - you are here: what this is, how to build, run and
  verify it.
- `HANDOFF.md` - the current map: subsystem truth, rules, estimates,
  the ordered task list. Read it first.
- `NEXT_STEPS.md` - the journal: one `## PR-nn` record per step with
  its evidence and gate counts, append-only, newest last. It carries
  no plan/status/rule sections (verify-docs P2 fails them): the
  ordered plan lives in HANDOFF section 8.
- `PLAN.md` - the RE facts (formats, grammars, measured counts);
  code comments cite its sections ("PLAN 8b", "PLAN 8g").
- `docs/RE_INDEX.md` - generated index of every address, field token
  and authored name the records mention; regenerate with
  `python3 tools/re_index.py docs/RE_INDEX.md`, pinned by
  `make verify-index`.
- `docs/OPEN_ITEMS.md` - the open-items register: every open thread
  that spans PRs is one row with a stable OI-nnn id and a status;
  P6 in verify-docs resolves every OI citation and pins ids as
  appended-only. Records cite ids, they do not restate threads.
- `original/README.md`, `ported/README.md`, `tools/*/README.md` -
  what each directory holds.

Frozen history - `docs/archive/`, never edited; its README is the
index and supersession table:
`open-leo_native_port_review.md`, `open-leo_porting_tips.md`,
`IMPROVEMENTS.md`, `PORT_DEEP_DIVE_2026-11.md`,
`IMPLEMENTATION_PLAN.md`, `TEST_REPORT.md`,
`VERIFICATION_2026-11-16.md`,
`HANDOFF_SECTION7_OPEN_QUESTIONS.md` (the pre-register
open-questions ledger, extracted verbatim by PR-202).

Searching the ledger - a generated tool, not a document (so it is not
in the closed set above). The journal is append-only and now ~49k
lines, so `make verify-docs` also builds a disposable SQLite index
over it (build/journal.db, gitignored like build/*.raw) and
`tools/jq.py` queries it. Both sit BESIDE docs/RE_INDEX.md and change
neither it nor P4. The generator (`tools/re_db.py`) imports doc_census
and re_index and reuses their regexes and record boundary, so it
cannot read a different ledger than the index and `make verify-docs`
do (the PR-55 "two parsers drift" rule). Records: NEXT_STEPS.md
PR-355 (the layer, closing OI-102), PR-356 (its indexes), PR-358 and
PR-361 (its coverage and routing fixes, from field feedback).

    python3 tools/jq.py 0xcef40                       # records citing an
           # address, each as NEXT_STEPS.md:LINE-LINE (read that slice)
    python3 tools/jq.py free "audio sync" --snippet   # ranked full text
    python3 tools/jq.py corr PR-350                   # correction graph
    python3 tools/jq.py show PR-355                   # one record: Read range

A bare token auto-routes: 0x of 4-6 hex -> the address table; short
hex like 0x33 -> substring search (a field offset is not an address
key, so `+0x33`/`0x33` match through the text instead); [..]->field,
OI->open item, PR->show, else full text. `oi OI-nnn` lists every
record that NAMES the id - prose mentions and the structured
OPENS/CLOSES graph - matched by integer, so OI-128 and 128 agree.
`name` covers authored game tokens only (A_*/AC_*/K_*, .des keys WITH
their trailing `=`); a ported C identifier like ll_seq_anchor_land is
not a name, and the verb says so and points at the text door.
jq.py rebuilds the DB first if
NEXT_STEPS.md is newer, so an answer never comes from a stale index.
`corr` reads the structured CORRECTS: field into a queryable graph -
the generator upgrade that closed OI-102, deferred since PR-197.

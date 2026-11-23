# HANDOFF - state of the Lew Leon native port and the plan from here

Written 2026-11-16 at commit `7db0cab` (PR-74) for whoever continues - a person
or a smaller model. Read sections 1-4 first (15 minutes), then pick ONE task
from section 8. Everything else in this repo (NEXT_STEPS.md = per-PR
journal, PLAN.md = the RE facts the code cites, docs/archive/ =
frozen history and superseded work orders) is history; this file is
the map.

## 1. What this project is, and where it stands

A native (Linux + Windows, SDL2) port of the 1997 Polish DOS game "Lew Leon"
(Longsoft). The original game files are NOT in git; the owner supplies them.
The goal is a faithful, playable port whose every claim about the original
is traceable to its machine code, its data, or a reference capture.

What exists and works (all verified by `make verify-all`, section 3):

| area | state |
| --- | --- |
| containers (`ANIMACJE`, `ETAPY`, `SOUND`, `LEVELDES`) | read byte-exact vs a python reader |
| movies (`.IS`, 15 files) | decoded and played; codec twins in C and python; two real bugs fixed in PR-55/56 (LUT1 polarity, Mode X EOB); data-free synthetic test `verify-is-synth` |
| `.SEQ` sprites | two row codecs, anchors, frame-number grammar (1-based, negative = mirrored), shape margins; oracle over 1656 payloads |
| `.PCM` parallax layers | format decoded (tile-opcode overlays), oracle 23/23 |
| music (`.MOD`) | played; parse oracle 22/22; the mixer has NO independent oracle |
| title menu + chapter map | drawn from data (11 chapters, titles from `LEVELnn.SEQ`, arrow colours); progress/"start" variants not done |
| level scene | 320x200 world view shown 2x; world size from `back_rect` or the reference layer; per-layer parallax (`speed=`, `main_back=`) for PCM layers, the PCX backdrop and every object; HUD from `BELKA*.DES` |
| objects | 827+ placements, art by each state's `seq=`, `.MOV` state machines (loop until a transition fires; `GEN K_GENn`, `AC_BREAK_NEXT`); PR-93: the 3-comma static image lines (`<LEAF>,<name>.seq,x,y` -> that CD leaf's `.SEQ`, drawn at the authored x,y behind the actors) and the 0,0 entries = attached children (joined+counted, never instantiated at the world origin); PR-107: a `TARGET(N)=<cond>` line carries an authored START ROW and the target state is entered at that row (same rule for the lion and scene objects) |
| lion (play mode) | walks on the level's own ground (`.PLT`) with his current ANIMATION's shape-box bottom as the contact line (PR-97: `ll_player.contact`, `ll_seq_shape_contact`, `mov_state_contact`), runs the engine's OWN velocity law once per 50 ms iteration - `v -= T*v/100` then `v += F*s/100` with the key intent `s` = ±10 (PR-135, replacing the fitted target-velocity model) - and his row list admits AT MOST ONE row per iteration, fed by travelled distance in a BY_X state and by the engine's constant 100 otherwise; jumps (ESTIMATED units), keys facing-relative (Model M, PR-85), camera follows (lead 100/220 - PR-136's absolute camera read CONFIRMS the right lead at 95-108; PR-137 APPLIED the grounded anchor at 128 + the world clamp (the 175..180 band was the clamp, not the anchor), pinned hermetically by `ll_level --follow-selftest`), HUD shows energy/lives |
| collision | push-out ported from the machine code (`FUN_00042de0/42cf0`), solid/semi-solid response, damage from an enemy ONLY through its own ATC window (the authored `atc_on` rows) and the 0x550c0 clash rule - a bare touch never hurts the lion (PR-139 deleted the port's unconditional contact damage); the enemy's own ATC-window attack when armed (PR-90, off unless `LEO_PLAY_DANGER=1`), balloon pickup; ON by default except RAN.01 |

What does NOT exist yet (the real gap to "a playable game"): combat
motion beyond the hits (the lion's attack landed in PR-89 and enemies
land through their own windows in PR-90 stage 1 - what is missing is
the engine's per-class AI driver and the attack/charge motion), the signal bus's
routing between objects and its engine-internal raisers (the bit table and
named `GEN`/condition wiring landed in PR-88), pickups/inventory,
ladders/vines, level flow (end of level, exchange between
TUNGA I and II), RAN's balloon mode, sound effects, save/progress semantics.
Section 8 lists them as tasks.

## 2. Ground rules (non-negotiable)

1. **Never guess data.** A claim about the original must match the machine
   code (`build/leo/leo.elf`), the data files, or a capture. Unknown things are
   counted/reported, or implemented as a clearly named ESTIMATE (section 6)
   that prints a log line and is listed in NEXT_STEPS. If you cannot decide a
   number, write down what is missing and stop.
2. **No game data and no game frames in git.** Not the CD files, not
   screenshots, not video frames, not the reference video. The reference
   captures themselves live in `reference/video/` (gitignored, section 3)
   so they survive a reboot; everything cut from them - frames, crops,
   scripts - stays under `/tmp/opencode/...`. Pins in tests are hashes/strings.
3. **Every behaviour change ships with a test** that cannot pass by the C and
   python twins agreeing on the same mistake: hand-derived expected values in
   a data-free self-test, and a mutation check (break the code, see it fail).
   The twins already hid two real bugs (PR-55).
4. **PR discipline** (one logical step per commit, "PR-NN" numbering continues
   at 75): `make` and `make win` warning-free; `make -k verify-all` EXIT=0 with
   the fixtures AND fixture-free (`DATA=/tmp/opencode/nope HD=/tmp/opencode/nope`
   -> EXIT=0 with named SKIPs); a new `LEO_*` hook is documented in README; a
   record in NEXT_STEPS.md (what was found, what was ported, what is
   estimated, what is open); push; do NOT wait for CI (owner ruling
   2026-11-20): continue to the next PR; its run is read opportunistically
   before the next push.
   NOTE (2026-11-19): the Windows halves of this gate - `make win`, the
   wine legs - are deferred to the end-of-implementation pass by rule 8.
   GATE PROFILE (owner, 2026-11-17 exception, profiled 2026-11-20): the
   gate FOLLOWS THE RECORD KIND (section 10a `KIND:`). A **read- or
   docs-kind PR whose diff touches only `.md` files** (nothing under
   `.gitignore`, `Makefile`, `tools/`, `ported/` or `.github/`) runs
   ONLY `make verify-index verify-docs` EXIT=0 - no build, no
   `verify-all`, no waiting - and its record carries the single light
   line `GATE: light: verify-index verify-docs rc=0 (ok);` (section
   10a) instead of the two-line gate; `gh` still runs the full suite
   on its own push. A **port- or gate-kind PR** (and any PR whose diff
   touches code, Makefile, tools/, .github/ or .gitignore - a pattern
   there can hide a fixture) runs the full gate above and carries the
   two-line gate. A read/docs PR MAY still run the full gate; if it
   does, it reports the two-line form, never both.
5. `gh` token has no `workflow` scope: a commit touching `.github/workflows/*`
   cannot be pushed.
6. The owner supplies reference material (screenshots, video timestamps).
   Ask for a specific clip rather than inferring.
7. **This working tree is local.** Every build, test and measurement of
   this project runs on the machine that holds this repo (`make`,
   `make win`, `ll_level`, the python readers, `gh`). Do NOT use the
   remote-host skills here - `debian-server` (the `lin` wrapper) and
   `openwrt-router` (the `wrt` wrapper): those hosts hold neither this
   repo nor the game data, so a command run there checks nothing about
   the port. (`windows-handheld` / `win` is different and stays in use:
   ADAM-HANDHELD is where the WASAPI audio measurement of PR-34/35 was
   taken.)
8. **Linux-first: the Windows version is not part of the day-to-day gate**
   (owner ruling, 2026-11-19). Implementation targets Linux; the Windows
   target is verified ONCE, at the end of implementation, deliberately:

   - end-of-implementation pass: `make -k verify-windows` (the Makefile
     moved the pure-Windows legs - `verify-win`, `verify-mod` - out of
     `verify-all` and into that target; `make -k verify-all WINTEST=1`
     still expands to the old full gate, byte for byte), plus `make win`
     warning-free and the `.exe`/MSYS2 job in
     `.github/workflows/ci-windows.yml` ("Run workflow", or push a
     `windows-gate-*` tag). `make -k verify-all-windows` = both at once.
   - per-PR gate that REMAINS: `make` warning-free; `make -k
     -j"$(nproc)" verify-all` EXIT=0 with the fixtures AND fixture-free
     (parallel measured 2026-11-19: 124-159 s serial -> 29-40 s, identical
     verdict sets; the Windows/wine half stays serial); `ci.yml` (now
     `linux` + `asan` only) stays green. This rule supersedes the
     `make win` and Windows-leg halves of rule 4 for day-to-day work.
   - NOT deferred, and this bit is load-bearing for CI: Windows
     BUILDING wherever a Linux leg's prerequisite chain asks for it.
     The Makefile names .exe targets as prerequisites of the MIXED
     legs - `verify-menu: ll_cutscene ll_cutscene.exe` (Makefile:1744;
     the same shape in verify-skip/-music/-pcm/-help/-cmake) - so a
     Linux-only CI job still cross-compiles Windows binaries and still
     needs the pinned vendor SDL2 (`ci.yml`'s "Provision SDL2" step,
     3 s). Measured: dropping that step failed the LINUX gate with
     `fatal error: SDL2/SDL.h` at ported/ll_audio.h:38 and
     ported/ll_view.h:41, then `make: *** [Makefile:78:
     ll_cutscene.exe] Error 1` (run 44536198608; reproduced locally
     over all 18 TUs of $(SRC) with SDL_WIN=/nonexistent, and 0 errors
     with the pinned tarball). Rule 8 changes what is TESTED per push,
     not what make is allowed to build.

   Measured effect of this rule, for the record (2026-11-19, runs
   44523679146 before / 44536707174 after): the per-push `linux` job
   went 304s -> 59s, because the 247s `Install toolchain` step is now
   a 25-30s image pull; `asan` 46s -> 48s; and the end-of-implementation
   gate `ci-windows.yml` is proven green when dispatched (run
   44536359825: cross 54s, native MSYS2 smoke 75s).

   Measured reason (2026-11-19, fixtures staged, every leg timed, all
   EXIT=0 except as noted): the local suite is 307 s and `verify-win`
   alone is 164.8 s = 53.7 % of it. The cost is neither the compiler nor
   wine machinery - a wine start measures 0.1 s and the .exe decode of
   1020 frames 0.1 s - it is real-time-paced demo/menu/level playback.
   The port is ONE shared codebase, so every engine path those legs
   exercise is already exercised by the Linux legs; what the Windows half
   adds is second-target build proof, which does not change while the
   port changes. Honest note on the same run: one full-gate run failed at
   `Makefile:139` (the wine `--skip-at 10` half of `verify-win`), a leg
   that had passed ~25 min earlier when run on its own - it is the
   suite's most expensive and wall-clock-paced leg, which is itself an
   argument for running it on purpose, once, instead of per PR.

9. **Commit subjects carry a kind tag** (owner ruling, 2026-11-19),
   right after the number: `PR-193 [READ]: ...`, `PR-194 [PORT]: ...`,
   `PR-195 [GATE]: ...`. `[READ]` = engine reading (docs + probe
   scripts, nothing under `ported/`, no behaviour change - the record
   and the `## Next` rewiring are the deliverable); `[PORT]` = port
   implementation (touches `ported/`); `[GATE]` = build/verify/tooling
   (`Makefile`, `tools/`, `.github/`, fixtures). The tag is chosen by
   what the commit TOUCHES, never by intent, so `git log --grep
   '\[READ\]'` is an honest count of reading-only PRs. A maintenance
   commit that is not a ledger record carries no number and uses a
   bare `[GATE]`/`[DOCS]` prefix.
   HISTORY OVERRIDE (same day, owner-approved): all 316 commits of
   `main` were re-written to add these tags (`git filter-branch
   --msg-filter`); messages only - the tree of every commit was
   compared byte-identical before and after (0 mismatches), and the
   176 sha references in this file (28) and NEXT_STEPS.md (148) were
   re-pointed to the new values. Old->new table:
   `tools/history_override_2026-11-19.txt`; pre-override ref:
   `refs/original/refs/heads/main`. CI run ids recorded in
   NEXT_STEPS.md were measured against the PRE-override sha of the
   same tree, so they stay valid evidence for the content.

10. **STANDING rules, joined from the journal (PR-203, doc step 5:
    one home for rules).** These three bullets were the whole of
    `## Standing rules` in NEXT_STEPS.md, kept verbatim:
    verify-llrs/seq/pcm/pcx are byte-exact vs tools/*.py and
    parser acceptance is never changed;
    verify-menu/level/sel keep their pinned log strings + PPM
    frames (rebuild ll_cutscene.exe before verify-menu!).
    LEO_* hooks and return codes are API (is_player_run 0/1/2,
    menu -2..-6, dialog -10..-12, usage exit 2).
    Builds stay -std=gnu11 -Wall -Wextra warning-free on gcc and
    the vendored mingw clang - the same duty rule 4's gate states,
    kept whole on purpose.

11. **No remote-host tooling** (owner ruling, 2026-11-20): the
    `lin` and `wrt` wrapper skills - the SSH paths reaching the
    Debian server and the OpenWrt router - are NOT to be used
    for this project's work: not for builds, tests,
    measurements, staging or checks. Rule 7 already states
    where work runs (this machine); this rule names the
    specific paths that must not be reached for. Any future
    need for a remote host is an owner decision, recorded in a
    record first.

## 3. Machine layout and commands

Paths on the development machine (edit for another one; tools hard-code them):

| what | path |
| --- | --- |
| repo | `/home/aizy/Projects/LL_RECOMPILE` |
| CD data (`DATA`) | `/tmp/opencode/iso_stage/LEO` (ANIMACJE, ETAPY, SOUND, LEVELDES, the `.IS` movies) |
| HD install (`HD`, scripts/placement text) | `/tmp/opencode/hd_files` (ANIMACJE, ETAPY, LEO.EXE, STATUS.LEO ...) |
| golden captures / owner screenshots | `/tmp/opencode/golden/` (`user_refs/` = the owner's screenshots; never in git) |
| reference captures | `reference/video/` (in the repo tree but **gitignored**, PR-111): the full playthrough `OvbbJTmp79I`, playlist ep.1 `xPiXBmPEdXg`, and the ladder clip `ladder-DomCTOtx2rg` - ids, URLs, formats and what each is good for are under "Reference captures" below |
| scratch cut from them | `/tmp/opencode/yt/` (frames, crops, measurement scripts; older `-f 135` copies of ep.1/ep.2 live there too) |
| disassembly | `/tmp/opencode/vis/text.asm` (make it with `tools/re/make_text_asm.sh`) |

### Agent skills in this repo (local tooling, never in git)

Three project-local agent skills live under `.opencode/skills/`
(gitignored on purpose: verify-docs P5 inventories tracked `.md` and
these SKILL.md files must stay outside that closed set). Sessions
discover them automatically from that directory; each is a ROUTER to
the one home named below - the docs stay the authority, and a skill
that drifts from them is wrong, not the other way round.

- `leo-verify-gate` - which gate a change must pass: rule 4's GATE
  PROFILE (full Linux gate vs the docs light gate), the rule-8
  Windows deferral, the never-change standing facts. Home: section 2.
- `leo-pr-record` - the PR-nn journal procedure: numbering, the
  kind-tagged commit (rule 9), the record field block, index
  regeneration. Home: sections 10 and 10a; the block itself is
  defined ONLY there.
- `leo-ghidra-re` - the engine-reading pipeline: le_dump -> Ghidra
  headless, VA/file-offset arithmetic, leodb vs objdump, the vtable
  method, the evidence rules. Home: section 9 and tools/re/README.md.

These skills change nothing about the rules above; rule 11's ban on
the remote-wrapper skills (`lin`/`wrt`) stays absolute.

### Reference captures (never in git, but kept on disk)

The three captures the port's footage-based claims rest on are stored in
`reference/video/` in the working tree, so they survive a reboot. Nothing
there is ever staged: `.gitignore` excludes the whole directory (the
videos, their `.info.json` and their `.description` are all copyrighted).
Fill any of them in again with the exact `yt-dlp` line under its entry.

**R1. `reference/video/xPiXBmPEdXg.mkv` - playlist ep.1, the standing
reference (it used to be `/tmp/opencode/yt/gp.mp4`).**
`https://www.youtube.com/watch?v=xPiXBmPEdXg` - "Let's play with Zax37: Lew
Leon (Leo the Lion) PL - Pierwsze Kroki", uploader Zax37, uploaded 2010-12-21.
414.82 s, h264 640x480 at 25 fps + opus, 41 034 044 bytes, matroska. It is
episode 1 of `PL6B7604DEF5B643F9` (ep.2 is `ti6IYfhh1bI`; the other ids and
the measurement method are in section 9).

    ~/.local/bin/yt-dlp --no-playlist --write-info-json --write-description \
      -f "bv*[height<=480]+ba/b[height<=480]" -o "xPiXBmPEdXg.%(ext)s" \
      "https://www.youtube.com/watch?v=xPiXBmPEdXg"

GEOMETRY, re-measured on this file (a 6 s window, t=314-320, per-row
motion profile): motion spans the full frame width - columns 0..639, no
pillarbox - and rows 40..439 (the profile is flat at rows 0..37 and
442..479, steps up at 40-41 and down at 439-440). So a GAMEPLAY view is
the whole width at rows 40..439, exactly 2x, no overlay: the old recipe
`crop=640:400:0:40` is exactly right, and section 9's method needs no
change for gameplay. One caveat found while measuring: that box is NOT
right for the other kinds of segment in the same file. In the window
t=118-124 the black bands reach to column 36 and row 65, and the frame
there is one of the game's own animated cutscenes (the `.IS` movies) - a
rounded cave-interior picture with the lion, a dog and a spotted big cat
around a table, on black; it measures 4:3 inside the game-view band and
whether that rounded picture is the movie decoder's whole frame or a
letterboxed copy of it is NOT pinned, and would be worth checking
against PR-55/56's movie decoder. The level title cards ("ZARIA II",
"ENA I / underwater journey") are black-background text, and their band
is not pinned either. So: measure the band per segment, never carry one
crop across kinds.
PICTURE IDENTITY with the old `/tmp/opencode/yt/gp.mp4`: 75 consecutive
frames decoded at t=300-303 from both files are md5-identical (mean abs
difference 0.0), both are h264 640x480 at 25 fps, so every measurement
ever made on gp.mp4 is a measurement on this file. The containers differ:
gp.mp4 reports 414.680 s, this one 414.821 s, the extra ~0.14 s is the
audio stream's length in matroska. Take timestamps from frame counts, not
from the container duration.

**R2. `reference/video/ladder-DomCTOtx2rg.webm` - the rung-ladder clip PR-109
decided the row unit from.** `https://www.youtube.com/watch?v=DomCTOtx2rg`,
481.633 s, vp9 576x360 at 30 fps + opus, 26 902 029 bytes. NOT the same
bytes as PR-109's `/tmp/opencode/yt/ladder.webm` (same total size,
different sha256), but the SAME encode of the same video: the extracted
video streams are both 17 868 921 bytes with identical codec parameters,
and 90 consecutive frames decoded at t=46-49 - the climb - from the two
files are md5-identical, mean absolute difference 0.0. The audio streams
differ. So PR-109's numbers (climb from about 46.6 s, off the top at
about 48.3 s, pose changes every 2-4 video frames) apply here unchanged.

    ~/.local/bin/yt-dlp --no-playlist --write-info-json --write-description \
      -f "bv*[height<=480]+ba/b" -o "ladder-DomCTOtx2rg.%(ext)s" \
      "https://www.youtube.com/watch?v=DomCTOtx2rg"

GEOMETRY: the whole 576x360 frame is the game view, at exactly 1.8x, with no
bars and no overlay (`crop` is a no-op; `scale=320:200` puts it back at game
resolution). 1.8x is not an integer, and 30 fps is the coarsest sampling of
the three - use it for period/sequence claims, not for positions.

**R3. `reference/video/OvbbJTmp79I.mkv` - the whole game, uncut (new in
PR-111).** `https://www.youtube.com/watch?v=OvbbJTmp79I` - "Lew Leon ( Leo
the Lion ) - DOS (Playthrough)", uploader NontimetisStreams, uploaded
2025-03-05, described as "Streamed 26.1.2024". 20389.02 s = 5 h 39 m 49 s,
h264 1280x720 at 60 fps + opus, 4 774 166 785 bytes, matroska. 95 views on
YouTube; it is the only long-form capture of the game the project has.

    ~/.local/bin/yt-dlp --no-playlist --write-info-json --write-description \
      -f "bv*[height<=720]+ba/b[height<=720]" -o "OvbbJTmp79I.%(ext)s" \
      "https://www.youtube.com/watch?v=OvbbJTmp79I"

GEOMETRY, measured (not taken from the uploader): the game's 320x200 view is
x 0..1151, y 0..719 - exactly 3.6x, no letterbox - so
`crop=1152:720:0:0,scale=320:200` puts it back at game resolution. 3.6 is not
an integer: 1 video px = 0.28 game px of position. 60 fps is the finest
sampling the project has - 16.7 ms, finer than the 28-33 ms frame-loop
iteration of PR-109, which is what makes this file worth having.

THE BAR ON THE RIGHT - KEEP IT IN MIND. The owner asked for this to be noted
(2026-11-17, quoted): "on comment in that 5+ hrs video is some bar on right
site, worth to note it to keep it in mind". It is real, and it is not game
art: x 1152..1279, a 128 px column, is the uploader's decoration and must be
cropped off before ANY measurement on this file. Measured twice over; both
agree.
- First pass: per-column temporal std over 2 s bursts falls from 19-21 to
  2.3-2.8 at x=1152, the same at t=120, 3600, 7200, 14400 and 20040.
- Stronger pass (40 consecutive frames, 0.67 s, at t=120, 7200 and 20040):
  the mean per-column temporal std drops from 12.3 to 0.1 at x=1152 and
  stays at 0.0-0.1 to the right edge, while columns inside the game window
  range 3-24. So those 128 columns carry no motion anywhere in the 5 h 39 m.
- It is opaque art, not a black bar: mean luma 128.9 and spatial std 65.0,
  byte-for-byte the same statistics at all three times, and 718 of 720 rows
  have luma > 8 (only the top and bottom row are dark). That is why an
  "all-black columns" test would MISS it - test for temporal std, not for
  blackness.
- The comment theory does not hold up: this video has no comments at all
  (`--write-comments` extracts 0, `comment_count` 0), and a keyword scan of
  the 34 comments on ep.1 (`xPiXBmPEdXg`) found no mention of a bar. So the
  note is the owner's own observation, recorded as such.
- Practical consequence for any other upload from this channel: do not
  assume the boundary is at 1152, find it - take ~40 frames from a few
  times in the file, compute per-column temporal std, and crop at the last
  column that moves. A plain per-column brightness or std scan can also
  mislead inside the game window (at t=7200 columns x 544..704 sit at
  std 3.2-4.0, low but clearly not a bar), so require "static at every
  sampled time", not merely "low".

WHAT IT SHOWS. The owner supplied a level-by-level time map for this
video (2026-11-17), transcribed exactly as given, one entry per line:

    0:00 zaria 1     7:77 zaria 2     13:17 ENA 1     26:27 ENA 2
    31:43 MENU      32:11 ZARIA 1     39:13 ZARIA 2   45:10 ENA 1
    43:59 ENA 2     1:14:32 LONA      1:22:28 SADES   1:23:36 MENU
    1:24:24 LONA    1:36:47 SADES     2:00:33 KADMAR 1
    2:23:42 KADMAR 2                  4:16:20 RAN
    4:23:31 KARA    4:31:36 TUNGA 1   4:42:29 TUNGA 2 5:37:54 MENU

Read it as approximate minute marks, not frames. Two entries need care:
`7:77` is not a valid mm:ss; and `43:59 ENA 2` sits BEFORE `45:10 ENA 1`.
That second one was read frame by frame: at 43:59 the lion stands on a
tree stump in jungle, with the same HUD art as at 0:00 and 43:30; 74 s
later, at 45:13, the game shows its own "ENA I / underwater journey"
card, and that level has a swimming bird where the lion was. So the ENA
I boundary is 45:13 (matching the map's `45:10`) and `43:59` is not an
ENA boundary as written. Check a boundary against a title card before
pinning anything to it.

WHY THE LABELS ARE CREDIBLE: the names are the game's own. The 13 level
blocks in `ETAPY` (census in the PR-46 game-wide measurement,
NEXT_STEPS.md:2016 and :2024-2029) are ZARIA.01, ZARIA.02, ENA.03, ENA2,
KADMAR.07, KADMAR2, KARA.07, LONA.05, RAN.01, SADES.06, TUNGA.11,
TUNGA2.12 and JUNCTION, and `LEVELDES` records are named after the
CHAPTER (`ZARIA1.DES`, `ENA1.DES`, `KADMAR1.DES` ...) while `ETAPY`
blocks are named after the stage - so "zaria 1", "ENA 2", "KADMAR 1" are
the engine's words, not a viewer's guess. Only `JUNCTION` has no entry in
the map.

WHAT WAS CHECKED AGAINST THE FILE: three boundaries were read directly
off the capture, and all three agree with the owner's marks to about a
second - frame 2353 (39:13) shows the card "ZARIA II", frame 2713
(45:13) shows "ENA I / underwater journey" against the map's 45:10,
frame 7233 (2:00:33) shows "KADMAR I / ancient mysteries" against
2:00:33. The detector below also flags a card-shaped frame at 790 s
(13:10), against the map's `13:17 ENA 1` - a statistical hit, its frame
not read. Twelve more marks were probed by reading the
brightest frame within +-25 s of each one (`/tmp/opencode/refcap/
sheet_marks.png`); none of those twelve happened to land on a card, but
their neighbourhoods agree with the map everywhere except one entry -
see the `43:59` note above. The rest of my independent evidence is a
sampling of one frame every 5 minutes (68 frames,
`/tmp/opencode/refcap/tiles/`, contact sheets `sheet_0.png` ..
`sheet_3.png`), which does not contradict the map either: penguins and
snow from about 75:30 (inside `LONA`), the KADMAR I card at 120:30, a
green rope-ladder level from about 280:00 to past 335:00 (inside
`TUNGA 2`).
Level starts carry the game's own full-screen title card, which is the
cleanest level-start marker the project has. There is a first-pass
detector for them (`/tmp/opencode/refcap/cards.sh` + `cards.py`: one
320x200 grayscale frame per 10 s of the 1 152 px game column - 2039
frames - then per-frame black/bright fractions), but it is a candidate
finder, not a verdict: it flagged 104 card-shaped frames in 68 windows
across the whole file, many of them plain fade-to-black, and it MISSED
the cards at most of the marks above. A card still has to be read frame
by frame.

WHAT IT DOES NOT GIVE YOU: YouTube itself carries no chapters and no
subtitles for it (`chapters` is null in the `.info.json`) - the map above
is the owner's, supplied out of band; it is one player's route at one
difficulty, not a set of deliberate measurements; the stream's own overlay
covers part of the frame (above); and it was captured through an
emulator, so anything about real hardware timing (the PIT, the display
rate) is out of scope, as it was for the other two.

THE LEVEL INDEX - WHERE EACH LEVEL CAN BE WATCHED (2026-11-18, at the
owner's request: "should be references to levels", "on videos",
"names"). The 13 names are the `ETAPY` stage ids, and the stage-to-
chapter correspondence is not a guess: every LEVELDES record carries a
`dir=` key giving its stage, and the port already reads that key
(`ported/ll_level.c:5724`, `:5758`, `:6023`). All 13 pairs, from the
extracted records in `/tmp/opencode/lvdes`:

    ZARIA1.DES   dir=zaria.01    ZARIA2.DES   dir=zaria.02
    ENA1.DES     dir=ena.03      ENA2.DES     dir=ena2
    KADMAR1.DES  dir=kadmar.07   KADMAR2.DES  dir=kadmar2
    KARA.DES     dir=kara.07     LONA.DES     dir=lona.05
    RAN.DES      dir=ran.01      SADES.DES    dir=sades.06
    TUNGA1.DES   dir=tunga.11    TUNGA2.DES   dir=tunga2.12
    JUNCTION.DES dir=junction

Each level's own title card was READ frame by frame at the times below.
The card's first line is the chapter name the LEVELDES record is named
after. Its second line is a subtitle, and R3's cards are drawn in
ENGLISH while the Zax37 episodes' are in POLISH: the game ships ENG,
GER and POL tables next to the LEVELDES records (`/tmp/opencode/lvdes/
ENG`, `GER`, `POL`, `POL.ORG`), which is why both appear. The two
agree word for word - R3 "LONA / land of penguins" is ep.5 "Kraina
Pingwinow" (= Kraina Pingwinow), "SADES / deep down" is ep.6 "Segajac
Dna" (= Siegajac Dna), "KADMAR II / egyptian dark" is ep.8 "Egipskie
Ciemnosci", "ENA II / pirate treasure" is ep.4 "Pirackie Skarby", and
ep.2's own card was read at t=49 s of what is now
`reference/video/zax37/ZARIA.02.mkv`:
"ZARIA II / zar tropikow" (= zar tropikow). (This file drops the Polish
diacritics to keep the columns narrow; the `.info.json` files beside
the videos keep the upstream titles exactly as upstream has them.)

The Zax37 chapter files themselves now sit in a folder of the same
names: `reference/video/zax37/`, named by the stage ids above (owner's
request, PR-127 - they used to be `epNN_<id>`), `.mkv` or `.mp4`;
each file's original YouTube id lives on in its paired `.info.json`.
They are edited and cover more than one level each - see the episode
notes below the R3 ranges; they are references for behaviour, never
for a level's length.

RANGES IN R3 (`OvbbJTmp79I.mkv`). Times are seconds in the file; at
60 fps a frame number is seconds x 60. All of them are +-2 s, because
the detector samples once a second and an accurate-seek extraction lands
up to 2 s away. The split files are `reference/video/levels/<stage
id>.mkv`, made from R3 with `-c copy` at exactly these boundaries (each
starts about 2 s before its card); the 4.77 GB master is KEPT and stays
the source of record for any measurement. A range listed twice means the
player entered that level twice in one sitting.

  ZARIA.01    "ZARIA I / first steps"      0-458, 1924-2344. The 0-458
            range rests on the owner's map alone: no card was detected
            there. The card at 1931 was read.
  ZARIA.02    "ZARIA II"                   458-786, 2344-2699. 2353 was
            read by PR-111; at 462 there is only a black hold.
  ENA.03      "ENA I / underwater journey" 786-1577, 2699-3229
  ENA2        "ENA II / pirate treasure"   1577-1899, 3229-4455
  LONA.05     "LONA / land of penguins"   4455-4937, 5062-5792
  SADES.06    "SADES / deep down"          4937-5012, 5792-7170
  KADMAR.07   "KADMAR I / ancient mysteries" 7170-8619. The card is
            re-shown at 7637: a second attempt at the same level.
            Censused end to end by PR-126 and PR-128: the placed stage-1
            BOSS object is on screen in NO window of this file. The boss
            FIGURE that repeats here (283-293, 358-369, 411-419, 565-576,
            762-774, 790-802, 1162-1172, 1334-1344 s) stands in a black
            room between two white-outlined wardrobe doors with no HUD
            drawn - a scripted scene, not that placed object.
  KADMAR2     "KADMAR II / egyptian dark"  8619-15363
  RAN.01      "RAN / two weeks in a baloon" 15363-15764 - "baloon" is
            the game's own spelling. A still picture of a pink balloon
            over a night desert at 15651 is in-level art, not a card.
  KARA.07     "KARA / magic castle"        15764-16280
  TUNGA.11    "TUNGA I / so close"         16280-16944
  TUNGA2.12   "TUNGA II / the last battle" 16944-20263. The two TUNGA
            cards re-appear at 16933, 17111, 17718, 17759, 17795,
            17907, 18889, 19284: repeated attempts, not new levels, and
            none of them is in the owner's map.
  JUNCTION    NOT IN THIS CAPTURE - no card and no candidate anywhere
            in the 5 h 39 m; the owner's map has no entry for it
            either. Recorded as absent from this file, not as absent
            from the game.
  not a level: 1899-1924 and 5012-5062 are menus (the map's `31:43`
            and `1:23:36` MENU marks), 20263-20389 is the ending and
            the end screens.
  The 24 files, named exactly as listed here: the stage id for the
            first visit, `<stage id>_b` for the second visit in the
            same sitting, `TUNGA_replay` and `TUNGA2_replay` for the
            stretches where a TUNGA card is re-shown but no new level
            starts, `UNRESOLVED_19282_20265` for the stretch after the
            last identified card, and `MENU_intro`, `MENU_2`, `ENDING`.
            Cut with `-ss <start> -i OvbbJTmp79I.mkv -t <length>
            -c copy`, so each starts at the keyframe at or before its
            start second; 4.5 GB total, master kept, nothing staged
            (`reference/video/` is gitignored).
   BEHAVIOUR MEASUREMENT INSIDE THESE FILES NEEDS A STILL WINDOW. The
            game camera does NOT scroll smoothly in R3: it holds dead
            still for 20-30 s stretches and jumps discretely between
            them (PR-125, tools/re/video_stills.py: ENA.03 20.5-50.0 s
            at mean drift 0.54 px/s, LONA.05_b 480.0-504.5 s at 0.35,
            KADMAR.07 still runs at 712.5-728.5 / 763.0-774.0 /
            791.0-802.0; drift is game px/s of the 640x400 frame, and
            across a whole cut its distribution is BIMODAL - runs at
            0, then jumps saturating the search window, a p50 that is
            contamination from moving decor, not a camera speed).
            Screen dx equals world dx only inside a window
            like those; `tools/re/video_lane_shift.py` then measures
            the band translation. Numbers in the PR-125 record.
            PR-128 adds the other half of the same question, whole-file:
            tools/re/video_blob_scan.py labels every sampled frame "HUD
            drawn" or "picture" (on KADMAR.07 the two classes separate
            with a 34x margin, no ambiguous frame) and lists where one
            chosen RGB appears, so a named class can be searched across
            24 minutes in a 23 s pass. On that file the 93 still windows
            split 78 gameplay + 15 picture windows, one per no-HUD
            segment, and the class its .des places is in none of them. A
            colour names a CANDIDATE only: one palette value serves the
            boss robe, the HUD lion portrait, the jungle decor and the
            player's own robe costume.

HOW THE BOUNDARIES WERE FOUND. `/tmp/opencode/stillscan.py` samples one
cropped 320x200 grayscale frame per second and reports runs of 3 or
more BYTE-IDENTICAL samples. A title card is a 3-5 s pure-black hold
(black fraction 1.0000, bright pixels 0) followed by 4-8 s of static
text (black 0.90-0.94, bright 1100-1830, text band at rows 50..146 of
the cropped window); gameplay is never identical for 3 s, because the
lion idles even when nobody moves. Over the whole file that gives 63
runs, of which about 25 are cards; requiring at least 200 bright pixels
in the band is what separates cards from fades. Two cards are missed by
it, "ZARIA I" at 1931 and "ZARIA II" near 2353, because those cards are
not static - both were read anyway, so a negative from this detector is
not evidence of absence. The same pass finds NOTHING on the 480p
episodes, because YouTube's re-encode makes a still card differ by
0.4-0.6 mean luma every second; for those files the usable test is
`/tmp/opencode/cardshape.py` (black fraction plus a compact bright
band), and its hits include the uploader's own "LEW LEON" intro, which
appears at 9-13 s of EVERY episode - it was read, it is not a game
card. Neither detector is a verdict: every name above came from reading
the frame.

THE MAP IS NOW CONFIRMED, including the two entries PR-111 flagged as
bad. Every mark in the time map above is within a few seconds of a
detected card except `43:59 ENA 2`, which stays wrong exactly as
PR-111 read it. And `7:77` reads as 7 min 77 s = 477 s: there is a
black hold at 462, consistent with a level change there. That is an
inference about a malformed mark, not a read card, and it is the only
remaining doubt about the map.

ADDITIONS TO THE THREE ENTRIES ABOVE (new records, not edits of shipped
wording):
- `frame 2353`, `frame 2713` and `frame 7233` in the "WHAT WAS CHECKED
  AGAINST THE FILE" paragraph are SECONDS (39:13, 45:13, 2:00:33), not
  frame numbers: this file is 60 fps, so those cards are at frames
  141180, 162780 and 433980. The paragraph's own conclusions are
  unaffected.
- R1's footage is also on disk as `reference/video/zax37/ZARIA.01.mkv`
  (it came down with the playlist; PR-127 renamed it from
  `ep01_xPiXBmPEdXg`). Same container size 41 034 044 B,
  different container sha256, and the h264 elementary streams are BYTE-
  IDENTICAL: both 34 046 474 B, sha256
  4fbab8d52dc75cc14f49e9b45a9bf8a29403cd66de33c3cc4a52456f0a3399e3.
  The audio streams differ (6 917 407 B each, sha256 6895526a04b46836
  against 1eac4609fbcf3c6a). So they are the same encode of the same
  video and every measurement in this file applies to either. R1's path
  stays the name of record, because the entry above cites it; the
  duplicate is kept, not deleted.
- R2 (`ladder-DomCTOtx2rg`) is inside the ZARIA chapter. The owner
  supplied the mark `https://www.youtube.com/watch?v=DomCTOtx2rg&t=50s`
  with the words "here only zaria" (2026-11-18), so PR-109's climb at
  46.6-48.3 s is in a ZARIA level. It is still unfit for position
  claims (1.8x, 30 fps), so nothing measured on it changes.
- The 11 playlist episodes are all on disk, kept whole and named by
  the level each one's title names - the stage ids of the index above
  (owner's request; PR-127 renamed them from `epNN_<id>`), as
  `reference/video/zax37/`: ZARIA.01, ZARIA.02, ENA.03, ENA2,
  LONA.05, SADES.06, KADMAR.07, KADMAR2, RAN.01, KARA.07,
  TUNGA2.12. 546 468 452 B in total, byte-identical to the count
  PR-124 took: 8 are `.mkv` and 3 are `.mp4` (SADES.06, KADMAR.07,
  KADMAR2), all h264 640x480 at 25 fps, so they belong to R1's
  geometry class, not R3's, and R1's `crop=640:400:0:40` warning
  about measuring the band per segment kind applies to each of them.
  Their `.info.json` files keep yt-dlp's pairing rule
  (`<basename-without-extension>.info.json`, NOT `<file>.info.json`
  with the extension - that is why a first extraction printed "?" for
  every field), and each id and upstream title lives on INSIDE its
  `.info.json`, so the rename loses nothing. The two playlist-level
  metadata files are kept as `playlist_PL6B7604DEF5B643F9.info.json`
  and `.description`. To refill the whole set: PR-124's download line
  (whole episodes, playlist order), then the rename table:

    ~/.local/bin/yt-dlp --ignore-errors --write-info-json \
      --write-description -f "bv*[height<=480]+ba/b[height<=480]" \
      -o "ep%(autonumber)02d_%(id)s.%(ext)s" \
      "https://www.youtube.com/watch?v=xPiXBmPEdXg&list=PL6B7604DEF5B643F9"

    ep01_xPiXBmPEdXg -> zax37/ZARIA.01    (same extension; rename
    ep02_ti6IYfhh1bI -> zax37/ZARIA.02     the .info.json and
    ep03_uSZ9HekkBiw -> zax37/ENA.03       .description sidecars
    ep04_HJLaOqiLeys -> zax37/ENA2         to the same base name,
    ep05_fxJrvBwtL-g -> zax37/LONA.05      or yt-dlp stops seeing
    ep06_v2LElzvk_pQ -> zax37/SADES.06     them; the ep00_/ep11_
    ep07_v-cJ0HcyHfg -> zax37/KADMAR.07    playlist metadata goes
    ep08_af0puAqzcMA -> zax37/KADMAR2      to playlist_PL...*)
    ep09_HGmAqEa5FpQ -> zax37/RAN.01
    ep10_O6-GbC1ZVmk -> zax37/KARA.07
    ep11_p8TWx_PXanM -> zax37/TUNGA2.12

  Which level each episode is about comes from its title: the titles
  are the game's own card subtitles (proved by reading ep.2's card -
  `zax37/ZARIA.02.mkv` - and cross-checked against R3's English
  cards for ep.4, ep.5, ep.6, ep.8; "ep.N" below means the Nth
  line of that table, upstream playlist order).
  Reading the cards inside the rest is still open: the shape test found
  candidates at 181/203 s of ep.1, 114/193 s of ep.4, 106/396 s of
  ep.5, 62-282 s of ep.6, 67/265/294 s of ep.8, 146-420 s of ep.9,
  54/300 s of ep.10 and 124/298/506 s of ep.11, and it finds NO card
  in ep.3 or ep.7 at all. Episodes cover more than one level and are
  edited, so they are references for behaviour, never for a level's
  length.

`/tmp` is a tmpfs: after a reboot restage the data (recipe in NEXT_STEPS.md,
"restaging"). The reference captures are NOT in /tmp any more - they live
in `reference/video/` on disk; if they are ever lost, the `yt-dlp` lines
under "Reference captures" rebuild them (pip install yt-dlp).

Build and test (day-to-day, Linux-first - rule 8):

    make                      # ll_cutscene (linux);  make ll_level  builds the level tool
    make -k -j"$(nproc)" verify-all \
         DATA=/tmp/opencode/iso_stage/LEO HD=/tmp/opencode/hd_files
    make -k verify-all DATA=/tmp/opencode/nope HD=/tmp/opencode/nope   # fixture-free
    make verify-play verify-level ...                                 # single legs

The day-to-day gate is run IN PARALLEL (owner, 2026-11-19, measured):
serial 124-159 s vs `-j22` 29-40 s and `-j4` 38 s, every run rc=0 with
the SAME verdict lines. Safe because the legs' scratch paths are disjoint -
the one shared file is the read-only py-refs oracle frame, produced by a
prerequisite. Caveat stated honestly: raw log TEXT is not bit-reproducible
under load (one music-pump verdict line prints a wall-clock-derived
duration; its assertions, e.g. peak=12706, do not move), so the gate is
rc FIRST (that is authoritative: `make -k` returns non-zero if any leg
failed) and then the legs' own verdict lines. Do NOT reduce that to a
count of `(ok)` lines - measured 2026-11-19, that pattern undercounts,
because verdict wording is per-leg: verify-mod prints `MOD <name>:
parse+render linux == wine (fnv=<hash>)`, others print "byte-exact" or
"== python reference". A pattern that covers the fixture-backed proofs is
`grep -cE "\(ok\)|byte-exact|== (python|wine)|render 3000ms"`, and the
real check is that the SET of verdict lines matches the previous run.
The Windows half stays SERIAL: verify-win and verify-mod share one
WINEPREFIX.

The Windows pass, run ONCE at the end of implementation (rule 8, owner
ruling 2026-11-19 - not part of the day-to-day gate):

    make win                                        # cross build, warning-free
    make -k verify-windows DATA=... HD=...          # verify-win + verify-mod,
                                                    # WITH the game data
    make -k verify-all-windows                      # both halves at once
    make -k verify-all WINTEST=1                    # the old full gate

    # No extra MOD staging is needed for that pass - verify-mod pulls the
    # three records out of the SOUND container itself (the same
    # tools/pull_record.py CONTAINER KEY OUTFILE the Makefile calls, into
    # /tmp/opencode/<NAME>.mod). Checked by running it here 2026-11-19:
    # with DATA staged it takes 4.5s and prints three cross-target verdicts
    # (MOD TUNGA/RAN/ENA2: parse+render linux == wine (fnv=...)).
    # An earlier note here claimed it "proves nothing, 0.2s, zero
    # verdicts" - that was MY grep, not the suite: see the gating note
    # directly below, which is the correction.
    # CI side of the same pass: .github/workflows/ci-windows.yml (cross .exe
    # build + the MSYS2 native smoke), run from Actions or by pushing a
    # windows-gate-* tag. ci.yml itself is linux + asan only now.

Headless run of a level (CI style; frames are dumped as PPM at the logical
320x200):

    env -u DISPLAY SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LEO_HD_DIR=$HD \
      LEO_HELP_SECS=0 LEO_LEVEL_SECS=2 LEO_PLAY_SCRIPT="1:FWD 60:JUMP 63:FWD" \
      LEO_LEVEL_PPM=/tmp/opencode/f.ppm LEO_LEVEL_PPM_TICK=200 \
      ./ll_level $DATA ZARIA.01 --play --no-audio

`ll_level DATA LEVEL` takes DATA as argv[1] and the HD install via
`LEO_HD_DIR`; set `LEO_HELP_SECS=0` for headless runs. A live run closes itself after 5 s unless
`-t 0` is given. Live viewer: arrows walk,
Space jumps, Up/Down/Left/Right also pan in preview mode.

Pitfalls seen:
- The Makefile did not track `ll_play.h`/`llrs.h`/`ll_pcx.h` for `ll_level` until
  PR-75; if a header edit "has no effect", `touch` a `.c` file.
- `verify-win` has one flaky Wine leg (`win restart-confirm`); rerun before
  suspecting your change.
- A SECOND flaky leg, now counted: CI's `windows-smoke` job, step "Run them
  (committed fixtures only; no game-data staging)", check `sleep_until
  honors the deadline (>=20ms)`. Out of all 125 `ci` runs the workflow has
  ever had, exactly 2 failed on that one check: run 101 / 44043011593 at
  07:59 UTC and run 124 / 44176302862 at 21:06 UTC, both on
  `windows-latest` msys2 MINGW64 (the three other failures in the history
  - runs 1, 2, 8 - are real setup breakage: SDL2 headers absent, the
  data-backed fixtures absent, a link error). Rerunning the failed job of
  run 124 passed on attempt 2. Both failing trees had byte-identical leg
  code (`git diff bb5cdc9..HEAD --stat -- ported/ tools/ Makefile
  cmake/` empty for 124), so it is a timing assertion, not a regression.
  Fixed in PR-112: the headless backend trusted the CRT's
  absolute-deadline sleep instead of re-checking its own clock (the SDL
  backend has always re-checked); the selftest now samples the contract
  20x at zero tolerance and prints the raw CRT call's undershoot as a
  diagnostic. The mechanism is now SEEDED by a direct measurement, two
  runs after the fix: that diagnostic on a real Windows runner reads
  `diag raw-CRT clock_nanosleep 20ms x20: early=1 unsupported=0
  worst_undershoot_us=40` (`ci.yml` run 127 / 44180333781, job
  134382654217, log line 444) - a raw `clock_nanosleep(CLOCK_MONOTONIC,
  TIMER_ABSTIME)` call returned 40 us BEFORE its absolute deadline. It
  has printed `early=1 worst_undershoot_us=11` again on run 128 /
  44181397463 (job 134386350669) and `early=0` on run 126 /
  44179955442; all three glibc legs print `early=0`. Two of three
  Windows runs, none of three glibc runs: exactly the intermittent,
  Windows-only, sub-millisecond shape the two failures had. Not proof
  they were caused that way (11 and 40 us are small samples), but the
  old code was wrong to trust that call, and the fix covers it whatever
  the true magnitude. If the leg ever fails again, the diag line in that
  job's log is the first thing to read.
- peak `ll_level` RSS is ~1.1 GB (RAN.01); run one level at a time.

## 4. Architecture map

    ported/main.c            entry, CLI, demo chain, menu -> level hand-off (NEW GAME = ZARIA.01)
    ported/ll_menu.c         title menu + chapter map (reads LEVELnn.SEQ titles, arrow colours)
    ported/ll_level.c        THE LEVEL: wiring (.DES), layers, scene objects, play loop, HUD
                             (the big file ~4400 lines; selftests at the bottom of its first third)
    ported/ll_play.c/.h      .MOV parser (states, transitions, frame tails) + the lion's simulation
    ported/ll_seq.c/.h       .SEQ codec, anchors, shape margins, push-out (ll_seq_shape_push)
    ported/ll_pcm.c, ll_pcx.c, llrs.c, ll_mod.c, is_stream.c   formats
    ported/platform_sdl*.c, ll_view.c, ll_audio.c   the only files allowed to touch SDL (verify-arch)
    tools/*.py, *.c          python reference twins, oracles, self-tests (verify-* legs)
    tools/re/                RE and video helpers (scratch, see its README)
    Makefile                 every verify-* leg and its pins; README.md hooks list

Key data flow of a level (ll_level.c, function that begins "4) the authored
scene"): `LEVELDES` block -> key/value pairs (`des_get`) -> CD `ETAPY` block
(art: PCX/PCM/SEQ, HUD art) + HD `ETAPY` block (text: placement files,
`BELKA`, `.PLT`) + CD/HD `ANIMACJE` leaf tables (object art / `.MOV` scripts).

## 5. Facts established about the original (all with a source)

Machine code facts use virtual addresses (VA = file offset + 0xf000 in
`build/leo/leo.elf`). "FUN_" names are the addresses.

This is the CURRENT-TRUTH MAP: one line per settled fact, each
pointing at the record that sourced it. What is OPEN is not here -
it is a row in docs/OPEN_ITEMS.md; the settled halves of partly-
open threads are stated here and their rows say so.

Level data
- Order: 01 ZARIA I, 02 ZARIA II, 03 ENA I, 04 ENA II, 05 LONA, 06 SADES,
  07 KADMAR I, 08 KADMAR II, 09 RAN, 10 KARA, 11 TUNGA I, 12 TUNGA II (no title
  image). `JUNCTION` is a hub screen. Map arrows are red 1-4,
  green 5-8, blue 9-11.
  Names in the archives: ZARIA.01/.02, ENA.03, ENA2, LONA.05, SADES.06,
  KADMAR.07, KADMAR2, RAN.01, KARA.07, TUNGA.11, TUNGA2.12.
- `LEVELDES` keys: `tlo` (layers back to front), `speed=x,y;...` (percent per
  layer, default 100*(i+1)/n), `main_back` (reference layer = the world),
  `back_rect`, `lew=class,art leaf,mov,des`, `lew_rect`, `pozycja` (spawn),
  `plany`, `zwierzaki/animacje/ani_plat` (comma lists of placement files; entry
  i is on layer i; each key makes one object list), `belka` (HUD file),
  `platformy` (ground `.PLT`), `muzyka`, `paleta`. `danger_power` is read by
  the loader but authored in no level.
- No `back_rect` (SADES, TUNGA2, ZARIA.01/.02): the world is the reference
  layer's own art (2240x700, 2560x800, 960x3000).
- Camera/view: 320x200 world units shown 2x (640x400 inside the 640x480 mode).
  Layer scroll: x_i = cam*X[i]/X[R]; y_i = cam_y*Y[i]/Y[R] + (H_i-200) -
  (H_R-200)*Y[i]/100 (FUN_00057460/57500/57530).
- Placement files: `class,dummy,script.mov,x,y` lines, then `damage=`,
  `bounds=x0,x1`, `scope=`, `energy=` lines for enemies.
- Placement grammar, the two extra forms (PR-93, census RE): the 3-comma
  `<LEAF>,<name>.seq,x,y` line places a STATIC image - the first field is
  the same class-named ANIMACJE leaf table as a 5-field entry, the second
  names the single `.SEQ` to draw at x,y (84 lines game-wide, 69 join on
  the CD; the 15 misses are all LONA.05 WODOSPAD2 asking elem26/27/29.seq
  the install lacks; HD ANIMACJE has no PICT/TABLICA blocks - this art is
  CD-only). And 5-field entries authored at exactly 0,0 (130 of them, all
  shipped) always follow another entry or its properties - they are
  children the engine spawns from their parent (the vulture's egg, the
  saw-fish on each log, drops on leaves, demo text), never placements at
  the world origin; the spawn trigger is unresolved.
- HUD (`BELKA`): `x,y` + `[N ]name.seq` items: energy head (10 frames), lives
  digits, tools icons (4), count digits, item frame, crown.

Engine (objects have a vtable at +8; the common slots. PR-94 caveat:
that slot table is the layout of the CORE 0x52000-0x55xxx object family
only - 111 factory-installed vtable bases exist; the answer-A/touch
pair keeps a constant +0x58 spacing in 56 of them but sits at different
absolute slots per family, and +0xd0 resolves to different targets
(hp-sub 0x54550 in 19, a 0x23xxx family's 0x23840 in 19, the generic
0x50ed0 in 10, enemy-AI 0x41180 in 8, lion 0x3dae0 in 5). Full census
in the PR-94 NEXT_STEPS record.)
- +0x10 message handler (`{next, kind, a, b}` nodes; Lew: 0x1e130 -> 0x50d60
  jump table; kind 9 = balloon heal +32 and the sender's signal bit; 13 =
  velocity add; 1 = displace), +0x3c "is Lew", +0x40 "is enemy", +0x44 flag
  word, +0x48 shape element, +0x4c rect, +0x50 facing, +0x68 position,
  +0x78 touch, +0x9c filter, +0xd0 hit(source, ., damage).
- Contact/answer protocol (PR-94, verbatim, all addresses in /tmp/opencode/
  vis/text.asm): the touch method 0x50ed0(obj, other, damage) gates on
  [obj+0x11a] type/NULL/[obj+0x126], keeps a per-object contact list at
  [obj+0xd2] (0x551f0 pops it; empty + [vtable+0x3c] -> [obj+0x106]=1),
  and applies damage ONLY when the [obj+0x102] invulnerability countdown is
  0 (sink 0x54550 `sub [obj+0xad],ebx`; else [vtable+0xe4](1)). The lion's
  contact dispatch 0x340b5 (id compare vs global 0xced04 = the player)
  stomps: hp[other]=-1 when feet land within 0..20 above its top and
  [lion+0x5c]!=2, else calls 0x50ed0(receiver=object, other=lion, 30).
  The answer protocol = 13-byte packets (12-byte named set + flag byte at
  +0xc; build 0x749a0, OR 0x49580, rep-movsd copy) from three vtable-only
  handlers: 0x50f80/0x55232/0x55130 - answer-A first runs the SOLID
  responder 0x53ae0 (raises AC_BLOK 0x53bc8 / AC_BLOK_Y 0x53c36, push
  0x52b10 kind 1 + velocity kind 13 depth*30, SEMI_SOLID special), then
  optional hit via [vtable+0xd0], then merges AC_ENEMY unconditionally;
  answer-C raises AC_ENEMY_ATC iff the routed hit actually reduced hp;
  answer-B handles ANTI_POCISK (hit with damage 0) and DANGER. The class
  name table at 0x90f3d holds the .des class tokens verbatim (ANITAB..
  BERLO) and the entry-parsing helpers 0x325b0/0x328c0 split with patterns
  pointing INTO it.
- Flag word (token parser 0x51b30..0x51dc3): SOLID 0x400, SOLID2 0x20400,
  SEMI_SOLID 0x40400, ANTI_SOLID 0x10000, ANTI_POCISK 0x80000, DANGER 0x2000,
  CHECK_ANI 0x40, CHECK_ZW 0x80, CHECK_LN 0x100,
  CHECK_LEW 0x200, CHECK_ALL 0x1c0,
  CH_DIR 0x800, INV_V 0x1000, CHECK_BASE_LN 0x4000, CHECK_ALL_PLANE 0x8000.
- Signals (TEventSet, PR-88): the name->bit function at 0x48000 maps the
  name block at 0x919ec..0x91c40 onto explicit bit constants of the
  object's set at [obj+0x60] (full table in the section-7 ledger,
  now frozen in the archive - see section 7's pointer - / ll_play.h
  LL_SIG_*; K_GEN1 = 54 CONFIRMS the PR-70 assumption). The condition
  grammar itself: clause parser 0x47d60 scans tokens with format
  ' %[^, \r\n\t]', folds a ',' run into ONE node bit-mask (AND - the
  engine's design matches PR-87's model choice), '|' starts a new node
  (OR), '!' negates, '*' = and-with-another-set (unused in shipped
  condition texts); K_ANY/K_NEVER (+AC_ aliases) are op special cases,
  not bits; unknown names print "TEventSet::match :Bledna operacja".
- Touch dispatch (FUN_0004b500, FUN_00052780): the mover's CHECK_ANI/CHECK_ZW
  pick the animacje/zwierzaki list; objects are tested only on the
  mover's layer unless CHECK_ALL_PLANE; the touch method (slot 0x78)
  runs the push-out (FUN_00053ae0 -> 00042de0) and then the hit
  effects (FUN_00055230).
- Damage: energy -= damage (FUN_00054550); enemies hit with their own
  `damage=`; DANGER objects with the level's `danger_power`. Lion energy starts
  at 100 (0x503ac), lives = global 0xa94f0.
- Frame tails in `.MOV` (the engine's directive token table at
  0x917c0..0x919c7, matched case-insensitively - the comparison at
  0x76a69 lowercases A-Z first, so the table's lowercase spellings match
  the data's upper ones): `atc_on`/`atc_off`, `lock`/`lock2`/`unlock`/
  `unlock2`, `ground_on`/`ground_off`, `chdir`, `ldir`/`rdir`,
  `pos i,i`, `goto N`/`gotorel`/`goto_rel N`, `I i,i`, `F`, `IC`, `FC`,
  `M i,i`, `MOVE`, `GEN` (names from the separate 0x919ec block), `Wait`,
  `end_level i`, sound words `smp i`, `smp2 i,i`, `smp3 i,i,i,i`,
  `smpauto`, `smpauto2`, `smpprior`, `vol`, `smp_loop`, `smp_unloop`,
  `smp_stop`; state/other words `rev`, `zero_v`, `zero`, `zero_t`,
  `vol_music`; an unknown token prints "Bledna komenda %s w pliku %s".
  Old note: `GEN <sig>` (any signal-table name - PR-88),
  `I vx,vy`, `F`, `FC`, `M dx,dy`, `WAIT`,
  `lock/unlock`, `ATC_ON/OFF`, `GOTO_REL`, sound words (survey in
  NEXT_STEPS PR-69).

Walk speed: the `.MOV` caps (T=90) match a measured ~75-125 units/s on video.

## 6. Estimates and assumptions in the code (revisit when evidence arrives)

The same information, one block per estimate, greppable:
`grep -n "^ESTIMATE" HANDOFF.md` enumerates them. PR-202
REFLOWED the table verbatim (its 25 data rows ran up to 3,512
columns); the words inside the cells are the table's own, only
re-wrapped. (Measured correction of the audit's claim: the "41
over-79 table rows" were never all here - 25 in this table, 10
in section 1, 6 in section 3; those 16 belong to a status/map
refresh, not to this step.) The correction paragraphs below
the blocks are dated history and are NOT edited: read them as
written, with their "the row above" wording pointing at these
blocks.

ESTIMATE: jump impulse unit
  where: ll_play.h `LL_PLAY_JUMP_DIV`
  value: `I`/4 u/s
  evidence: ONE measured jump (vine, ep.1 318.6 s); PR-86 re-confirmed on ZARIA
    live: A_JUMP `I 150,-1200` -> apex ~24 u / 0.36 s, matching the video's
    jump population (gp.mp4 feet-track scan: clean candidates 23-30 u over 8-13
    frames). PR-135 note, counted open: this pair is per-second arithmetic and
    the vertical half of the law was NOT re-fitted to the 50 ms iteration grid,
    so the peak the port reaches is now 20 px above take-off (min y 628 vs the
    648/649 walk line) - 4 px below the ~24 u PR-73/86 fitted against, though
    within 4% of the 21 u single jump PR-74 measured (the hermetic jump leg's
    own comment). The constants are unchanged on purpose: one measured frame
    cannot fit four constants

ESTIMATE: jump gravity
  where: `LL_PLAY_JUMP_G`
  value: 1650 u/s^2 (= G.y 55 x 30)
  evidence: same jump; same PR-135 note as the row above

ESTIMATE: jump-state ending
  where: ll_play.c
  value: a state that DECLARES an AC_BREAK_NEXT exit ends at its frame-list
    WRAP (the landing no longer cuts it); forced ->A_STAY/A_DRAP kept ONLY as
    fallback for states with no such line
  evidence: PR-86: real A_JUMP/A_JUMP_S declare
    A_WALK(7)/A_DRAP(1)=AC_BREAK_NEXT; ZARIA.01 leg pins it

ESTIMATE: `TARGET(N)=COND` entry
  where: ll_play.c `ll_mov_row_start` (used by BOTH the player path and
    ll_level.c `obj_transition`)
  value: numeric (N) = start ROW (1-based) in the target's frame list; non-
    numeric = top; an N past the end clamps to the top
  evidence: PR-86 evidence chain in the struct comment (PRE_JUMP skips A_JUMP's
    impulse row; A_WALK(7) mid-cycle re-join pinned on real data); PR-107 read
    the store (`entry-1` at 0x51e3b..0x51e49) and the use: the state record's
    live row field `rec+0x4`, written raw at 0x523cb by the row-setter, which
    is the record's own vtable slot +0x1c in the table every record stores at
    rec+0x44 (0xa25d4, from its constructors 0x524cf/0x52508), and which the
    transition step 0x535a2 calls on the new state (0x535ea). Two MODEL
    CHOICES, recorded in the helper's comment: the engine writes the value RAW
    there, so clamping an over-long row is ours; and "absent / non-numeric" is
    -1 = "leave the row alone" (0x51e3e, 0x52381), which only COINCIDES with
    the top because a row list wraps. One row counter per object (`o->fi`) vs
    the engine's one per state record is also ours

ESTIMATE: The unit of the row clock, and the BY_* mode feed (0 BY_X / 1 BY_Y /
    2 BY_TIME / 3 BY_TX / 4 BY_TY)
  where: ll_play.c `ll_player_step` sections 4 and 7 + `LL_ITER_MS`
    (ll_play.h), ll_level.c `obj_row_tick` / `obj_advance`
  value: MODELLED since PR-110, as ONE accumulator per moving thing fed BY
    MODE, mirroring the dispatcher 0x543a2: a BY_X state with a non-zero state
    `F=` adds travelled distance (`|vx| * dt`) and its time accumulator stays
    at 0; every time mode adds 100 per ITERATION and never any distance, an
    iteration being `LL_ITER_MS` = 50 ms of wall time (PR-134 measured that
    edge, PR-135 set it), fed through a whole-millisecond carry (`row_rem`) so
    no step size drifts, and AT MOST ONE row admitted per iteration on BOTH
    sides (0x521f0 / 0x5232b / 0x523f0) - PR-135 removed the port's multi-admit
    loop and the `4 * nfr` cap; the engine gates mode 3 on `[obj+0xba] != 0`
    and mode 4 on `[obj+0xbe] != 0` (0x543f2 / 0x5441b, PR-108) and the port
    does NOT model that guard, nor any vertical force — a BY_TY state is simply
    time-fed, and the climb's vertical DISTANCE law stays open (T5). A row
    stays current while `acc < thresh`; the compare is AFTER the feed and the
    threshold is SUBTRACTED on a hit, so the remainder carries into the next
    row. `time_scale=` is a parse-time scale on the stored thresholds (`thresh
    * time_scale / 1000`, default 1000, rows before the line unscaled), NOT a
    duration
  evidence: The dispatcher and its mode jump table are PR-99's
    (NEXT_STEPS.md:5439-5447); the feed constant 100 has exactly one writer,
    0x4c8b7 (PR-100), and the row comparison at 0x52320-0x5232b hands `acc -
    thresh` to the row handler rather than zeroing the accumulator (PR-100).
    PR-108 read the mode-3/4 guards above and that `[obj+0xbe]` is also v_y's
    force register (0x542cd). PR-109 decided the unit from the owner's clip
    (the `[A_WSP]` rows of 200/300 measure 66-100 ms on screen, the pacer
    0x4b0e0 -> 0x4a120/0x1dba0 holds one iteration to two display frames, the
    engine's own fallback setpoint is 27) and corrected PR-106's "55-unit
    steps" and "sound driver's 70.0 Hz PIT" (that write is 7.00 Hz; during play
    the counter is a real 1 ms clock, 0x5b9e0 set-period called at 0x5d195);
    PR-110 takes 28 ms, the low end of that bracket, and implements the mode
    split. OPEN, from the PR-110 record except where marked: whether a row
    fires AT `acc == thresh` (13932 of 14881 authored BY_TIME thresholds are
    exact multiples of the 100 feed) - PR-117 pinned the object side at exact
    equality and PR-135 keeps `>=` after the feed on both sides; the engine's
    mode-0 feed is the TRUNCATED `v_x/100` of the tick while the port feeds the
    travelled distance (PR-135 CLOSED the multi-admit loop and the cap, the
    truncation stays open); the `fx != 0` half of the walk gate (30 shipped
    BY_X blocks with `F=0,0`, no leg covers one); the AC_TIME pending-flag
    lifetime; the climb's vertical DISTANCE law. PINNED by: the RC fixture
    walked on two grids with the exact carry identity and `facc` read at every
    step (0 in a BY_TIME state), the `time_scale=2000` leg plus its control,
    the ZM leg reading both accumulators at both ends of the mode gate (no key
    in a BY_X state: everything stays 0; key held: the time accumulator stays 0
    while the list runs on distance), 7 mutations caught, plus one recorded
    survivor (`mut_nogate4`, section 4's feed-site condition removed: `facc` is
    only read through the walk gate, so no leg can see it), and every verify
    leg re-derived by hand (the derivation is written beside each leg in the
    Makefile)

ESTIMATE: The player's OWN velocity register, and the three numbers in it that
    bytes do not give
  where: ll_play.h `LL_PLAY_DRAG_DEN` 100, `LL_PLAY_INTENT_FULL` 10,
    `LL_PLAY_V_EPS` 0.2, `LL_ITER_MS` 50; ll_play.c sections 2 and 7;
    ll_level.c `obj_walk`
  value: per iteration `v -= clamp(T,.,130)*v/100` (sign flip or a remainder
    under one register unit -> 0), then `v += F*s/100` with s = +/-10 only
    while a facing-relative direction key is held in a BY_X state, or for an
    auto_walk state; NO lower clamp on `T`; NO register on the object side,
    which steps straight to the steady rate
  evidence: PR-129/130 read the chain, PR-133 the passes and PR-134 the clock;
    the steady state F*s/T units/s reproduces PR-134's measured lion (50 u/s at
    ZARIA's F=100 T=20) and worm (25 u/s at F=100 T=40) inside those brackets;
    the epsilon, the partial-intent values of s, the engine's raise of T to >=
    20, the object register and 50 ms as PR-134's video-chosen edge of PR-109's
    28-33 ms code bracket are all named in the PR-135 record

ESTIMATE: jump key
  where: ll_level.c
  value: Space
  evidence: viewer choice

ESTIMATE: fire keys
  where: ll_level.c `play loop` + platform_sdl `map_key`
  value: Ctrl=K_FIRE, F1..F4=K_F1..4 (weapon selectors), Alt=K_ALT; script
    tokens FIRE/F1..F4/ALT comma-combinable
  evidence: PR-87; names from the walkthrough, the .MOV conditions are the
    contract

ESTIMATE: `,` in transition COND = AND
  where: ll_play.c `parse_cond`/`cond_true` (groups)
  value: `|` ORs groups, `,` ANDs tokens within one; OTHER poisons only its
    group
  evidence: PR-87 MODEL CHOICE (evidence in ll_play.h: AC_ONAIR/AC_DRECT pairs,
    every K_FIRE,K_Fn attack line); A_FIGHT entry pinned real

ESTIMATE: hurt lock
  where: ll_level.c `play_collide`
  value: 30 ITERATIONS of the engine clock (PR-135: that function runs once per
    iteration of `LL_ITER_MS` = 50 ms, and `hit_cd` counts the same way)
  evidence: 0x50304 `[+0x102]=0x1e` constant, unit read in PR-133/135

ESTIMATE: `AC_TIME`
  where: ll_level.c `obj_cond_true`, ll_play.c
  value: obj: = end of frame list OR a raised bit; lion: raised by frame `GEN
    AC_TIME` (bit 9 = LL_SIG_AC_TIME)
  evidence: `time=min,max` unit unknown

ESTIMATE: signal bits
  where: ll_play.h `LL_SIG_*`, `ll_sig_id()`
  value: engine table 0x48000: AC_BLOK 0..AC_EV_MAX 68, K_GEN1 54; `GEN <name>`
    raises any, conditions test the object's own set
  evidence: PR-88 (verified constants); pulse-vs-latch + cross-object routing
    OPEN

ESTIMATE: lives / energy HUD
  where: ll_level.c
  value: 6 lives, energy frames (100-e)/10
  evidence: video start shows 6

ESTIMATE: camera
  where: ll_level.c `follow_camx/camy`
  value: lead 100/220, ease 6 u/tick, grounded contact-line-at-128 anchor since
    PR-137 (178 before it; PR-97: the anchor point is `p.y + p.contact`, the
    shape-box bottom of the animation being shown (max over its state's frames)
    = the paws PR-84 measured; y-ease 2 u/tick invented). PR-136 MEASURED the
    camera in world units from the level's own static placements: leads HELD
    frame-for-frame 95-108 right (n=57, CAM_LEAD_R=100 confirmed) and 199-251
    left (n=19, CAM_LEAD_L=220 inside the band, weak); catch-up rate p50 5 /
    p95 60 / max 230 u/s vs the port's 6 u/iteration = 120 u/s; the grounded
    paws row sits at 128 (mode, n=3990 surface frames; 128 unclamped) and 180
    only where camy is at the world clamp - one constant 128 plus the existing
    clamp reproduces both; the view does NOT hold rigidly through jump arcs
    (camy change p05 -26 / p50 0 / p95 +12, n=139). PR-137 APPLIED the anchor:
    `CAM_FEET_Y` 178 -> 128, and the old comment's claim that the 175..180 band
    "is NOT the world clamp" was WRONG - it is, at 681-500 = 181; one constant
    plus the clamp reproduces both observed rows. No pixel pin moved (the
    ground-line case clamps either way); every unclamped surface now shows the
    lion at row 128 as the game does. The leads stay 100/220 (right CONFIRMED,
    left a weak n=19 band); the eases (6 x / 2 y per iteration) stay models
    with the measured band beside them, and `./ll_level --follow-selftest` now
    pins the whole law hermetically - 9 mutation controls, all caught
  evidence: PR-136 record (tools/re/level_landmarks.py + video_camera_read.py +
    camera_law.py, /tmp/opencode/pr136/law_v3.txt); PR-137 record

ESTIMATE: ground source on the first screens (G-13)
  where: ll_level.c `platformy=` -> `ZARIA_A.PLT`
  value: DECIDED PR-138: the engine's ground records ARE the authored `.PLT`
    text - the loader makes one object per polyline line and the line parses
    itself into the 13-byte records the segment lookup (0x45380) then searches;
    class 0xa0c78 has exactly one builder in the image, no second ground-record
    builder exists. PR-136's "surfaces 7-51 units ABOVE at x 128-392" is
    RETRACTED: those were screen-x values from false-matched single-landmark
    (tier-2) cameras - frame 4370 proves it (the authored balloon at (1270,460)
    cannot be on view at the reader's camx 173; the port's own render matches
    at cam ~(1100,335), where the paws sit on line "1 1171,506;1305,503" within
    the estimate). Every VERIFIED (tier-1) frame - and they exist only for
    camera x 1..122 - stands the lion on a .PLT line at d+0 (125 samples, y
    681). The remaining ground item is a NAMED PORT gap: the port places
    ani_plat= objects but never parses their local ground lines (ANIPL2.DES
    KATAPULT/PALMA entries; engine entity-record tables 0x34850
    [obj+0x48]/+0x4c and the MOVEPLAT provider chain unread)
  evidence: PR-136, PR-138

ESTIMATE: contact pass default
  where: ll_level.c
  value: on except RAN.01
  evidence: RAN is the balloon level

ESTIMATE: SEMI_SOLID
  where: `play_collide`
  value: only the r/5 shift
  evidence: velocity add (kind 13) unit unknown

ESTIMATE: SOLID push polarity
  where: `obj_shape_elem`
  value: shape uses the DRAW polarity incl. CH_DIR facing
  evidence: consistency, not decompile-pinned (PR-81)

ESTIMATE: player facing model
  where: ll_play.c `face`, ll_level.c `play_draw/play_collide`
  value: face follows walkdir only in CH_DIR states; draw/shape = authored XOR
    face, face sticks
  evidence: flags real (LEW census, PR-82); the always-XOR rule for no-CH_DIR
    states is a model-consistency choice, NOT decompile- or video-pinned
    (HANDOFF Q0)

ESTIMATE: K_FWD/K_BKWD key model
  where: ll_play.c `cond_true` (MODEL CHOICE note)
  value: facing-relative: K_FWD = arrow along current face; walkdir = the
    literal arrow; a left-facing turn settles via the state pair or
    A_TURN+AC_BREAK
  evidence: PR-85; forced by data (one A_WALK, no BKWD-walk state); NOT yet
    confirmed on video

ESTIMATE: lion attack damage / object hit rule
  where: ll_level.c `LL_LION_HIT_DAMAGE` 30, `obj_hit`, `LL_OBJ_HIT_CD` 30,
    `ll_play.h` `atc`/`atc_hit`
  value: damage 30 = the fixed `mov ebx,0x1e` of the stomp branch (0x34103,
    reached ONLY when the attacker's class id is ds:0xced04 - 0x340c8/0x340db;
    PR-139 read the whole receiver 0x34020, and its default at 0x340ce forwards
    the incoming damage UNCHANGED); hp<0 forces A_DEAD by name; energy -1 never
    hit-able; the COUNTDOWN IS 0x1e = 30, NOT 14 (PR-139): `[obj.vt+0xe4]` is
    0x50300 in 14 of the 15 answer-A tables and 0x50300 is `edx ?
    [obj+0x102]=0x1e : [obj+0x102]=edx` (0x50300..0x50315); the lion's own
    class table 0x96960 ("TLewZaria", factory 0x1fef2) is one of them, so
    `pl->lock = 30` is exact, while PR-89's 14 is 0x27260 - reached by exactly
    one +0xe4 slot (table 0x9a54c); the RATE (one per record step, 0x50720)
    stays PR-135's pin; the window bit IS bit 5 of `[obj+0xb6]`, written only
    by the authored `atc_on` record through its applier 0x45c20 (parser 0x4636e
    -> vtable 0xa10f0+4), and the getter 0x23860 is the +0x40 slot of 155 sane
    class tables while 64 others return a constant 0; one hit per ARMED
    episode, `atc_hit` consumed only when a hit lands, and 0x550c0 cancels the
    object's hit exactly while the lion's own window is open AND he is turned
    toward it (equal x does not cancel)
  evidence: addresses verified; the 30 stays until the factory-template id
    array (0x44350 -> ds:0xcecf0..0xced04; the proportional hit at 0x34037 is
    energy*30/100+10, NOT *45 - see PR-96) maps to a .des type; what the engine
    really passes is `[deliverer+0xb1]` (its only setter 0x1e540) and the .des
    `damage=` grammar into it is unpinned; the OBJECT-side countdown for
    ZWIERZ-family animals is a NAMED stand-in (their +0xd0/+0xe4 pairs are
    0x1ecd0/0x4ffb0 for `SEP`, 0x41c30/0x41460 for the ZWIERZ base, and neither
    writes 0x1e); the window-clear rule and the corpse-deal-contact rule stay
    model choices (PR-89)

ESTIMATE: enemy attack (T3 st. 1)
  where: ll_level.c `obj_arm_atak`, `play_collide` window block; `ll_play.h`
    `LL_CTRL_DANGER`
  value: a current state carrying the authored DANGER flag arms the script's
    OWN A_ATAK by name when the lion is inside abs(dx) < 40 AND abs(dy) < 50
    (strict); the enemy lands ONE hit through its own ATC window (authored
    damage=, 30-step lion lock) and NOTHING otherwise - the engine's DANGER
    damage is the level's global `danger_power` (`[ds:0xcef40+0x54]`, read at
    0x55290/0x5529f in the animal answer 0x55230 and at 0x5102d in answer-A),
    which no shipped level authors, so the receiver's `damage == 0` gate
    (0x50eef) answers "nothing"; OFF unless LEO_PLAY_DANGER=1
  evidence: the box numbers are RE-pinned FOR THE CLASS "LICHA" (class chosen
    from the .des token at 0x616d0; box at 0x61b6e) - the flag-as-precondition,
    A_ATAK-by-name entry and window-hit mirror are the port's stand-in (engine
    driver open, §7 item 10) (PR-90). PR-139 names where the engine reads that
    authored flag: `[obj.vt+0x44]` = 0x23920 returns the CURRENT row record's
    flag word `[[obj+0x50]+[obj+0x5c]*4]+0x12`, and both answers test its
    0x2000 bit (0x5528b / 0x51019); the same word's 0x80000 bit makes the
    animal call the hit with damage 0 (0x55245)

ESTIMATE: AC_ENEMY_ATC raiser
  where: ll_level.c `obj_hit`, `play_collide` window block
  value: the hit raises it on the HIT object's own set (PR-88 mechanism) so its
    `A_DOSTAL(1)=AC_ENEMY_ATC` line fires; PR-91 mirrors the raise onto the
    LION for the enemy's WINDOW hit (never contact) - his authored A_DOSTAL
    chain then runs itself
  evidence: PINNED, no longer an inference: answer-C 0x55130 saves `[obj+0xad]`
    at 0x55140 and compares it at 0x5516a - hp DROPPED -> bit 16 (`or bl,0x1`
    0x5518c), unchanged -> bit 7 (`or dl,0x80` 0x551ad), read verbatim again by
    PR-139 together with the delivery itself (0x5514f..0x55164 call the
    receiver's `[+0xd0]` with `[deliverer+0xb1]` as the damage); the answer
    entries merge the plain bit for any shape meet (PR-92/94). PR-139 corrects
    which family owns which answer: answer-A 0x50f80 is in 15 tables only, the
    animals' entry is answer-B 0x552e0 -> 0x55230 (78 tables), and BOTH reach
    the same answer-C

ESTIMATE: AC_ENEMY raiser (PR-92)
  where: ll_level.c `obj_touch_enemy`, called from `play_collide` (solid loop +
    post-loop non-solid pass)
  value: the lion's touch raises bit 7 on the TACHED object's own set and its
    OWN table reacts (raise-consume like the balloon path); gated by `hit_cd`
    (the [obj+0x102] countdown), default-on like contact
  evidence: PINNED raise sites (PR-94 verbatim): answer-A 0x51050 and answer-B
    0x52be merge {bit 7} unconditionally into a 13-byte answer packet (12-byte
    set + flag byte) copied to the answer buffer ARGUMENT (0x51062..0x51083);
    the answer handlers are reached through family vtables (PR-139: the entry
    slots are +0x78 = 0x50f80 in 15 tables and 0x552e0 in 78; 0x50ed0 is a
    RECEIVER at +0xd0, rets at 0x50f79 and raises nothing); which object's head
    finally reads the buffer is OPEN, so feeding the object's own set is MODEL,
    the second non-solid pass is data-forced (no consumer state carries SOLID);
    a pure touch NOT setting the hit_cd countdown is still an open item (item
    10)

ESTIMATE: static image draw + order (PR-93)
  where: ll_level.c `obj_static_get`, `objs_draw` (two-pass)
  value: frame-one draw: first patch-art element of the named .SEQ at the
    authored x,y through the element's anchor; statics compose BEFORE animated
    objects
  evidence: the join and positions are RE (census in the code comment); frame-
    one (no motion data exists) and the behind-actors order (matches the ZARIA
    footage) are model choices; whether the original animated any static is
    open (item 11)

ESTIMATE: shape payloads for lion-vs-object tests
  where: ll_seq.c `ll_seq_shape_push2`, ll_level.c `play_collide`
  value: each element's margin lists read from its OWN .SEQ payload (paired
    form)
  evidence: model choice: how the original resolved two elements into its
    single-payload routine is unpinned; same-payload pairings stay byte-
    identical (selftest-pinned, PR-89)


CORRECTION TO THE ROW-ABOVE CLAIM, added 2026-11-17 by PR-114 and
quoted here rather than edited in the table (the row still says "the
`fx != 0` half of the walk gate (30 shipped BY_X blocks with `F=0,0`,
no leg covers one)"): there is now a test in front of that half in both
engines, and the count is a tool output - `tools/re/mov_byx_gate.py`
reads rows the way `ll_mov_parse` does and measures 105 blocks naming
`BY_X` in 69 files, 30 of them `F=0,0` (290 rows, 21 files), 29 of
those 30 with no motion source of their own, and 97 states / 357 rows /
80 files where the engine's mode bit and the port's gate disagree
(62 of them by getting mode 0 by default, with no `control=` line in
the text at all). What the legs pin is the port's model, not the
engine's: `A_PUSH`/`A_COAST` in `ll_play_selftest` (velocity frozen at
the entry value, rows on the iteration clock, one 100-unit row per
feed, the sixth wrapping the list, `facc` exactly 0.0) and `glide`/
`snaps` in `obj_motion_selftest` (one row per call, `facc`/`xa`
zeroed, no anchor snap at the wrap because 1212 tests the mode bit -
the BY_TIME control does snap). Five mutations caught; the recorded
survivor is that `obj_motion`'s leg-3 "F=0 does not move" check still
passes when the object gate's `fx` term is deleted, which is the gap
PR-114's leg 6 closes. Which half the engine keys on stays OPEN.
NEXT_STEPS.md, PR-114 record.

CORRECTION TO THE ROW-CLOCK ROW ABOVE, added 2026-11-18 by PR-115 and quoted
here rather than edited in the table (the row still says "the engine admits ONE
row per record step, the port loops"): the engine half is right and the port
half is about ONE of the port's two loops. The compare at 0x52329 `39 d8` /
0x5232b `7d 27` admits at most one row per record step, and 0x52334 `29 c3`
hands `acc - thresh` to the row handler in registers only - the only writes to
[obj+0xa1] anywhere are the feeds at 0x543d1 / 0x543e7 / 0x54412 / 0x5443b, so
the accumulator is monotonic and the engine has no cap and no resync. The
port's player loop (ll_play.c:1286-1288) does cap a step at `4 * nfr` closures
and then zeroes what the cap leaves over (:1318-1319, "guard hit (data safety):
resync") - that resync DROPS a debt the engine keeps; the object loop has no
cap and no resync, only the FIX-13 4096-iteration guard (ll_level.c:1188-1189),
and it closed all 17 rows of a 1700-unit feed in one call in the new leg. And
the threshold table the row's OPEN list leans on needs its own correction: 170
rows at authored threshold 0 in 73 files is a count of the port's parse-time
clamp (ll_play.c:481 turns any authored value <= 0 into 100 before the scaling
at :735, so the port never stores a 0, and for those rows both engines close
exactly one row per fed step); 0 thresholds are negative anywhere. What
actually lets one feed close two rows is a threshold STORED under 100: 108
time-fed states in 77 files whose stored minimum is <= 50, and 1682 rows in
1..99 as written, where PR-110's record quotes "1715 ... after scaling (111
files)" from its own scaling rule. Two data-free legs now pin the port's
current behaviour in both engines without changing it (`MULTI`/`BOUND` in
`ll_play_selftest`, leg 7 of `obj_motion_selftest`), and six mutations of the
loop bounds, the resync and the parse-time clamp are all caught. Census and
arithmetic: `tools/re/mov_byx_gate.py`, NEXT_STEPS.md, PR-115 record. Whether
the game closes one row per step or all that fit is STILL open - that needs a
capture, not a compare.
CORRECTION, added 2026-11-18 by PR-135, for the last sentence above: that
question IS settled from the machine code, and the port now matches it.
PR-105 read the record-step driver (0x521f0 has ONE call site, 0x5240c,
inside the driver 0x523f0, which itself has one call site at 0x5371c): one
key composition + one feed + at most ONE row admit per step, and PR-133
confirmed no other scheduler exists. PR-135 REMOVED the port's multi-admit
loop, its `4 * nfr` cap and its `resync` on the player side, and the
multi-admit loop and the 4096-iteration guard on the object side, and
re-derived the `MULTI`/`BOUND` legs and `obj_motion` leg 7 for that shape
(one 1706 ms call is 34 iterations, 34 admits, each moved by its authored
`M` delta). What a capture would still decide is the SECOND half of the
same paragraph: whether a row fires AT `acc == thresh`, and what the
engine's own iteration length is inside a running game (PR-109's bracket,
28-33 ms, against the port's 50).
CORRECTION TO THE PR-115 PARAGRAPH ABOVE, added 2026-11-18 by PR-116, for
the same reason (that paragraph still says what it said when it shipped): it
quotes "0x52334 `29 c3` hands `acc - thresh` to the row handler in registers
only - the only writes to [obj+0xa1] anywhere are the feeds at 0x543d1 /
0x543e7 / 0x54412 / 0x5443b, so the accumulator is monotonic". Part of that
is wrong. The handler writes the remainder back: it saves its entry ebx at
0x520cb (`89 5c 24 14`), reloads it at 0x52114 (`8b 54 24 14`) and stores it
at 0x52118 (`89 90 a1 00 00 00`, `mov [eax+0xa1],edx`). And 0x54412 /
0x5443b are `add reg,eax` on a loaded copy, not stores: the mode-2 feed is
0x54400 load, 0x5440c `add ebx,0xa1`, 0x54412 add, 0x54414 `89 3b`
`mov [ebx],edi`, and mode 4's is 0x54429 load, 0x54435 `add ebx,0xa1`,
0x5443b add, 0x5443d `89 0b` `mov [ebx],ecx`. Those stores carry no
displacement at all, which is why a text search for `0xa1]` missed them.
Every instruction that does mention that displacement, listed in
`/tmp/opencode/pr116_write_set.txt`: WRITES 0x52118 (the remainder),
0x5236b / 0x533b0 / 0x5389e (the three zeroes PR-100 already listed),
0x543d1 / 0x543e7 (the mode-0 and mode-1 `add` feeds), plus the two
computed-pointer stores 0x54414 (ungated) and 0x5443d (gated on
[obj+0xbe] != 0 at 0x5441b); READS 0x52323, 0x54400, 0x54429. So the
accumulator is NOT monotonic - every row that runs rewrites it and three
paths zero it - and PR-115's "no cap and no resync in that path" is still
true but was not justified by that sentence.

THE OPEN HALF OF THE ROW-CLOCK ROW ("whether a row fires AT `acc == thresh`
(13932 of 14881 authored BY_TIME thresholds are exact multiples of the 100
feed)") is answered at the machine-code level, 2026-11-18, PR-116: it does
not. The compare at 0x52329 `39 d8` / 0x5232b `7d 27` is threshold minus
accumulator and jumps to the epilogue when the accumulator has only REACHED
the threshold, so a row runs only when the accumulator is strictly past it;
at equality the step keeps the accumulator, and the next feed runs the row
100 iterations later with a remainder of a whole feed instead of 0. The
reading that goes the other way needs the operands swapped and contradicts
itself, and PR-110's "which also makes the `sub ebx,eax` remainder negative"
belongs to that swapped reading, not to the bytes. Related wording in the row
above: "A row stays current while `acc < thresh`" should read `acc <= thresh`
for the engine. Measured population: 15317 of the 18617 row lines store a
threshold that is an exact positive multiple of the 100-unit feed (BY_TIME
15142 of 17011 rows, in 754 files), in 2012 states. `Wait`, the one
directive whose node skips the compare, is spelled `WAIT` in this text and
the name compare at 0x46b59 is 0x76a69, which lower-cases both sides, so
251 row lines do reach it and the honest figure is a bracket: 15066 to
15317. The port keeps `>=` (this PR changed no behaviour): flipping the
gates breaks 41 play legs and the two object legs that pin the current
rule, and no clip in the repo separates the two, so the flip stands as an
owner decision with a number on it. PR-116 record; census section 5 of
`tools/re/mov_byx_gate.py`.
PR-117 added 2026-11-18, and it closes a gap PR-116 named about its own
fixtures, quoted above: "nothing at all breaks if only the object's
walking half is flipped: no leg puts a distance-fed accumulator exactly
on a threshold, which is the fixture gap PR-116 records" That sentence is
no longer current. A leg 8 of `obj_motion_selftest` (ported/ll_level.c,
before the `rc = 0;` of that function) puts a distance-fed accumulator
exactly on a threshold: fixture
`[A_RUN] control=BY_X / F=100,0` with one 100-unit row, and `obj_walk`'s
feed is `|F| * dt_ms / 4000.0` (ll_level.c:1101-1104), so two dt=2000
calls give 50.0 and then exactly 100.0 - both products are exact in
binary64, so it is a real equality. The first call closes nothing, moves
x 20 -> 70 and leaves `row_t`/`row_rem` at 0, because a walking state
never runs `obj_row_tick` (:1169-1181); the second is the boundary, and
the port's rule closes that one row, lands `facc` on 0.0, wraps the row
list without an anchor snap (:1212 skips it for a walking state) and ends
at x 120. PR-116's mutation program (g2b, `<` -> `<=` at :1230) now fails
exactly that leg and nothing else, with a clean control run first; the
mutant leaves `facc` at 100.0 while x still reaches 120, because the walk
moves the object independently of this gate. No behaviour changed - not
one logic line of ported/ was touched. The play half of the same question
is still open and was measured, not assumed: a probe at the player's row
loop (ll_play.c:1286) printed 0 equalities over the whole play selftest,
and that feed is `p->vx * (dt_ms / 1000.0)` (:913, :1100, :1119), which
cannot hand an integer threshold exactly at the shipped 28 ms step.
PR-117 record in NEXT_STEPS.md.
PR-118 added 2026-11-18, and it qualifies the sentence above, quoted
verbatim: "Measured population: 15317 of the 18617 row lines store a
threshold that is an exact positive multiple of the 100-unit feed (BY_TIME
15142 of 17011 rows, in 754 files), in 2012 states." That count is right
and its arithmetic is unchanged - but "the 100-unit feed" is the feed of a
TIME-fed state only, and the sentence as written covers every mode. The
port distance-feeds a state when it carries the `BY_X` token AND a
non-zero first `F=` (the `walk` expression, ll_level.c:1169); measured
with that rule, the corpus has 75 such states, 992 rows, in 57 files, and
their stored thresholds are 2, 3, 4, 5, 6, 7, 9 or 10 units - so the
multiple-of-100 test selects NONE of them (0 of 992), and the 67 rows the
census's BY_X line counts are all time-fed ones. A distance-fed row does
have the same `cmp`/`jge` boundary, at a multiple of its OWN per-call
distance, which is `|F| * dt / 4000` (obj_walk, ll_level.c:1101-1104):
measured per grid this port really calls `obj_advance` with, it is reached
for 0 of 992 rows at dt=28 (`LL_ITER_MS`, the selftest grid: the feed is
`7|F|/1000` and the reachable thresholds are the multiples of
`7|F|/gcd(7|F|,1000)`, which never coincide with any (|F|, threshold)
pair this corpus authors), 713 of 992 at dt=20 (the play/PPM grid,
:6425: there |F|=100 feeds exactly 1.0, so every integer threshold is a
whole number of calls), 689 at dt=40, and 0 at the dt=2000 PR-117's leg
had to invent. So "flipping the two gates moves 15066-15317 authored rows
by one whole feed of 100 iterations" is the cost for the time-fed rows; a
complete figure would add a distance-fed population that depends on the
caller's dt, and PR-116's headline stands only as the time-fed count.
New in the same section 6: at 2 states the port's double accumulator
drifts OFF a landing its own feed law says is exact (`LONA.05_LEW`,
|F|=30 at dt=20 feeds 0.15, a 3-unit row needs 20 calls, and repeated
addition reaches 2.9999999999999991 at call 20 and crosses only on call
21), while the engine's accumulator is an integer dword (0x543d1 `add
[obj+0xa1], eax`) and cannot drift - untested anywhere, since no
selftest runs a distance-fed row at a grid that reaches a threshold. No
behaviour changed and no code changed: `tools/re/mov_byx_gate.py` section
6 only, output `/tmp/opencode/pr118_census.txt`, whose sections 1-5 are
byte-identical to `/tmp/opencode/pr116_census.txt`. PR-118 record in
NEXT_STEPS.md.
PR-119 added 2026-11-18, and it closes the last sentence of the paragraph
above, quoted verbatim: "untested anywhere, since no selftest runs a
distance-fed row at a grid that reaches a threshold." Leg 9 of
`obj_motion_selftest` is that fixture: `control=BY_X`, `F=30,0`, one row
`1,3`, then 20 calls of `obj_advance(&e, 20)` and a 21st - the same
(|F|, dt, threshold) the census found in `LONA.05_LEW`, on the real 20 ms
grid (:6425) rather than a 28 ms one. What it pins is what the port
already does: the row is held back at call 20 with `facc` at
2.9999999999999991 and closes on call 21, and the subtract at :1233
carries 0.14999999999999902 into the next row - checked as a band
(`2.99 < facc < 3.0`, then `0.14 < facc < 0.15`), so both an exact
accumulator and one that drops the remainder fail it. Two mutations catch
it and one control does not (transcript /tmp/opencode/pr119_mut.txt):
`o->facc = 0.0` at :1233 and quantising the feed to exact thousandths
each fail one check of leg 9 alone, while the g2b flip of :1230 still
fails only leg 8. Writing the leg exposed a second mechanism, now asserted
too: by call 20 the object has already moved the full three units
(`x` 20 -> 23) while the three-unit row has not run, because `obj_walk`
adds the same `dist` to `facc` and to `xa` (:1103-1104) and only `xa` has
its whole part taken away (:1105-1107), and that subtraction re-draws the
rounding at every pixel boundary - on call 20 the position residual hit
1.0000000000000002 and paid its third whole pixel (leaving
2.2204460492503131e-16) in the step where `facc` came out at
2.9999999999999991. Which of those two accumulators the engine keeps for
a walking state is unread, so this is recorded as PR-119 open item 1 and
the leg makes no engine claim. No behaviour changed: the leg and the
summary clause that names it are the only lines added under ported/.
PR-119 record in NEXT_STEPS.md.
PR-120 added 2026-11-18: a byte-level reading of the engine's own row
feed, no code change. It answers PR-119 open item 1 (whether the dword
the row gate's accumulator consumes is the pixel counter or a second
one): NEITHER. `[obj+0xa1]` is a field whose only consumer anywhere in
the object code is the row gate at 0x52323, the load behind the compare
at 0x52329; its writer set is the four feed sites PR-116 listed, the set
at 0x52118 and the three zeroes, and the mode-2/mode-3 branches write it
through a register offset (`0x5440c/0x54414`, 0x54435/0x5443d), which a
scan for the text `+0xa1]` misses. What the feed adds is `abs()` of
`[obj+0xca]` (`cdq`/`xor`/`sub` at 0x543cc..0x543cf), and that dword is
made by an `idiv` by 100 a few instructions earlier (0x5434d..0x54365,
y twin 0x5436b..0x54372) - NEW here is that the remainder is dropped AT
THE DIVIDE (0x5435f overwrites `edx`; the y remainder dies at the
epilogue), so the engine counts a sum of truncated steps, never a
truncated total. Two consequences recorded in the PR-120 record: the
engine-side set of reachable distance-fed thresholds is the multiples of
that per-step integer (so PR-118's 713/689 figures are the PORT's
boundary count, and its own law is quoted there), and the `F=` to v_x
relation stays the missing input for any engine-side count. PR-99 item 9
said the same quantization "is worth a test of its own": it has one now,
as the QUANT mutation (`o->facc += (double)(int)dist`, transcript
/tmp/opencode/pr120_quant.txt) - it breaks exactly one leg in either
chain, PR-119's leg 9, at its first check, because 0.15 a call truncates
to 0; leg 8 passes it because that fixture's step is a whole 50. So the
port's fractional feed, which the code comment at :1073-1077 calls a
deliberate difference, is pinned by a leg. Unchanged: PR-99 item 10's
"the position writer is still unread", now narrowed to the three unread
readers of `[obj+0xca]` at 0x54810/0x54830/0x54836. PR-120 record in
NEXT_STEPS.md.
PR-121 added 2026-11-18: the position writer, found at byte
level, no code change. It closes what PR-120 left open, in its own words,
quoted:
"now narrowed to the three unread readers of `[obj+0xca]` at
0x54810/0x54830/0x54836",
Those readers are one operation: the coordinate advance at
0x54804..0x5484a. It reads `[obj+0x0]`/`[obj+0x4]`, adds the
SAME truncated dwords PR-120 described (0x54827 `add edi,edx`, 0x54829
`add ebp,eax`), stores them back (0x5482b/0x5482d), then clears the delta
fields by self-subtract through a register offset (0x54830/0x54836/0x5483c
and the y twin at 0x5483e/0x54844/0x5484a) - which is the proof that no
sub-pixel remainder is carried: the engine quantises an object's POSITION
exactly as its row gate quantises the feed. The row gate takes `abs()` of
that dword; this writer takes it signed. So the port, which accumulates
the fraction in `xa` and pays whole pixels out of it (:1104-1106), moves
at the exact velocity while the engine moves at that velocity truncated
every tick: they can only agree at a velocity that is a whole multiple of
100 hundredths per tick, and turning that into pixels still needs the
`.MOV` `F=` to v_x relation PR-118 item 2 holds open. New with it:
`[obj+0xea]` is a one-shot y amount added into the y delta and zeroed in
the same breath (0x5480a/0x5481b), and the only computed store to it in
the image is 0x454a8, two instructions after that block links an object
into `[obj+0xe6]` (0x454a2) - the very field this function copies to
`[obj+0xa9]` at 0x547bc and calls through (0x547c2/0x547c6). Within that
tail the coordinate write is unconditional (four transfer targets); the
flag bytes and the linked-object handoff are the conditional parts. Not
settled: what the `[obj+0x0]`/`[obj+0x4]` pair IS. The method-table
alternative is ruled out for this class, whose table is `[obj+0x8]`
(0x547eb then 0x547fa calls slot +0xb8 through it), but offset 0 is a
table in the linked object's class and the note at NEXT_STEPS.md:6871
shows a ctor writing a pair at `[obj+0x0]` in another one. No behaviour
changed and no port comment touched; the coordinate half of the
difference is not modelled and not stated anywhere in the code. PR-121
record in NEXT_STEPS.md.
PR-122 added 2026-11-18: where the one-shot vertical amount comes from,
a reading, no code change. It takes PR-121's own closing words here,
quoted:
"the `[obj+0xea]` one-shot vertical amount and the `[obj+0xe6]` link as its
open items",
Those two fields are now read. 0x455a0 is 21 instructions of one
formula: `P.y + trunc((R.x - P.x) * (Q.y - P.y) / (Q.x - P.x)) - R.y`,
the signed vertical gap from a point R up to the line through P and Q,
and like PR-120's divide its remainder is dropped (0x455c3 `idiv`,
overwritten at 0x455c5). Its caller at 0x45400 builds P and Q as
adjacent records 13 bytes apart - 0x4543c/0x45440/0x45481/0x45485 are
four `imul ...,0xd`, and 0x4540c..0x45419 read a count byte at +0xc of
the same table - indexes them through slot +0x6c of the second argument's
table (0x45434/0x45439), and stores the result only past two guards
(0x45496..0x454a0: the gap must be <= 0 and <= what is already there),
0x454a2 `mov [edi+0xe6],esi` and 0x454a8 `mov [edi+0xea],eax` in the
same breath. So the link and the correction are one transaction: a
surface provider is attached, a non-positive gap to a two-point segment is
computed against it, and PR-121's tail applies that gap to the y delta
once and clears it. Not claimed: that the records are terrain. The shape
fits and PR-99 item 10 guessed terrain, but 0x45380 (0x45423) and the
+0x6c provider are unread, and there is no zero-denominator guard in
those 21 instructions. The port-side contrast is stated: it has no
geometric y path at all - y comes from the row's authored `dy=`
(:1238) and is reset to the base y (:831, :927, :1001, :1214) - so no
shipped leg could detect a missing slope push. PR-122 record in
NEXT_STEPS.md, with reading 0x45380 and the +0x6c provider as its first
open item.
PR-123 added 2026-11-18: the segment lookup that goes with PR-122's gap
function, and the port's own copy of the same query. It takes PR-122's
closing words here, quoted:
"with reading 0x45380 and the +0x6c provider as its first open item."
It answers the first half. 0x45380 is a 43-byte lookup over the table
PR-122 described: `eax` = the owner (its base pointer at +0x4, its record
count as a DWORD at +0x8, 13 bytes each), `edx` = a pointer to the x to
find. It returns 0 from three bail-outs - an empty table (0x45388), x
below `[base]` (0x45393/0x45395), x above the dword at `base + count*13`
(0x4539f/0x453a1) - and otherwise scans the records in order
(0x453a3..0x453bb, `cmp x,[base + k*13]`/`jg`) and returns the index of
the first record whose x is >= x. So the answer is always in 1..count-1
and index 0 is the not-found value, which is how both listed callers
treat it (0x4542c/0x4542e and 0x453db/0x453dd) and why 0x45400's pair
`i` and `i-1` is always two real records. The bounds only make sense if
the table is x-sorted, and they do not exclude two records sharing an x,
which would make 0x455a0's divide by zero: assumed ordering, unproven
either way. Sixteen bytes later 0x453d0 is a sibling client of the same
table - lookup, bail to `mov eax,0x7fffffff` at 0x453f5, else build
base+i*13 and base+(i-1)*13 (0x453df..0x453eb), `call 0x455a0` and hand
the gap back to its caller; the index labels that region as still inside
FUN_00045380, which is a hint. 21 sites in 0x40000..0x46000 scale an
index by 13 (first 0x45012), so this lookup is module-wide. The port
already asks the same question, in the open: `seg_y_at` (`ll_play.c:123`)
is 0x455a0's first term in C, its divide truncating towards zero the same
way and its vertical-segment case at :126 existing precisely because a
zero denominator is not allowed; `ll_plt_surface_at` (:140) and
`ll_plt_first_below` (:164) scan every polyline for a covering segment
and report not-found with `best = 0x7fffffff` - the sibling's sentinel.
The `.PLT` ground they read is documented as x right, y DOWN, feet on top
(`ll_play.h:54`) and parsed from the HD ETAPY block (`ll_play.h:10`).
Who gets the answer: the player, every step (`ll_player_step` :905, the
slope-following call at :1149, the fall check at :1173); a scene object
once, at placement (`obj_settle`, `ll_level.c:1693`, snap at :1700/
:1705), after which its y is only `dy=` (:1238) and base-y resets. So
what PR-122 wrote here - that the port "models no geometric y at all" -
holds only of the per-tick object path, and that correction is recorded
in PR-123 rather than in an edited record. Still unproven: that these
records are `.PLT` ground data, which class answers slot +0x6c with an
index, and what writes a 13-byte record. Shape agreed, identity not.
PR-123 record in NEXT_STEPS.md, with the +0x6c provider and the record
writer as its open items.









## 7. Open questions - NOW AT THE REGISTER (PR-202, doc step 4)

Every open thread that matters across PRs is a row in
docs/OPEN_ITEMS.md (stable OI-nnn ids; statuses; origins). This
section is no longer the ledger: the ledger is FROZEN verbatim
at docs/archive/HANDOFF_SECTION7_OPEN_QUESTIONS.md (the file
states its source commit), and nothing is restated in two
places: a new open thread arrives as a row in the SAME commit
as the record that opened it, and the record cites the id.
The G-nn audit gaps of the PR-131 era live in register rows
with their content; the frozen file carries the originals.
Citations written as "section 7 item N", "HANDOFF 7.2", "Q0" or
"G-nn" - still present in older text below and in records -
resolve in that frozen file, never by re-opening this section.

## 8. Plan: tasks, in order. One task = one or a few PRs.

Each task: goal / where / how / done when. Do not start a task whose "how"
needs a number you cannot source - do the RE step first.

> **PLAN ONE-HOME (PR-203, doc step 5: moved here from the
> journal's `## Next` - one plan, one place).** The "IS SHIPPED"
> summaries that paragraph carried are deliberately NOT re-copied:
> a record is the one home for what its PR did (the journal is
> that history). The living half, restated here because it is the
> plan's frame: the OWNER'S RULING (2026-11-19, second) made
> documentation the current focus - that line ran PR-200..203 and
> is COMPLETE; engine reading and porting stay where PR-199 left
> them until the owner re-opens, and the T-tasks below are then
> the ordered plan. Decisions and deferrals that already have a
> number on them are REGISTER rows, not list items: OI-006 and
> OI-009 (row-firing strictness; the truncated-step fidelity
> asks), OI-015/OI-016 (the PR-135 not-modelled list; the T=130
> cap), OI-095 (the palette freeze - FROZEN by ruling), OI-096
> (the Windows end pass: `make -k verify-windows` + one
> ci-windows dispatch + `make win`, ONCE at the end, never per
> PR - section 2 rule 8). The old journal `## Deferred big items`
> list is rows OI-093..OI-101 (its live parts; done-halves are
> history in their records). Picking a task means reading its
> rows: docs/OPEN_ITEMS.md is the open-thread map.

**T1. Close the jump (open question 1).**
Goal: running jumps cross the first chasm of ZARIA.01. How: (a) finish tracing
how the engine consumes the `F`/`FC` accumulators (parser and accumulation are
documented; interpretation still open), (b) parse the directives into
`ll_mov_fr`, (c) apply only the RE-confirmed effect, (d) check against the video: ep.1
~316-317 s and ep.2 ~465 s contain running jumps; ZARIA_A.PLT gives the gap
widths (`ll_plt` polylines). Done when: a hand-derived data-free test pins the
horizontal range for one state, and a video measurement agrees within ~15%.
STATUS after PR-98: (a) is CLOSED for the velocity chain - the engine's
whole F=/FC=/I/IC path is now read, at instruction level and with its data
links read out of the image (merge at 0x526df..0x5273d: the current row's
`FC`/`IC` pairs fold ONTO the state's `F=` pair, which then enters a
per-tick velocity increment scaled by a terrain scalar /20; `T=` is a
per-tick PERCENT drag taken from the pre-add velocity; frame `F`/`I` pairs
are a velocity that 0x54170 SETS, never past the target, and it runs after
the drag; the PR-79 frame-advance-by-distance mechanism is confirmed at
0x543a0 + 0x52317..0x52338; and the header keys map to rec+0x50/+0x54
(`F=`, +0x58/+0x5c (`T=`, clamped to 130 by the parser) and +0x48/+0x4c
(`G=`)). (b)/(c) still cannot be applied: that loop's steady state couples
walking speed to F/T while the measured walk law is F/4 px/s - but the
two coincide at T=40, which is where every object sample in this repo
sits, so the video does not separate them. PR-99 corrects PR-98's
"the measured walks refute it" wording on exactly that point (see §7
item 13). The header-`F`-as-velocity escape stays closed: 0x54170's
target comes only from the row nodes. The units by contrast look close
to pinned: v is hundredths of a px per tick and 25 ticks/s reproduces
this port's own two fitted /4 constants. What is left of (a) is now
narrow and explicit: PR-135 REPLACED the port's fitted target-velocity
law with the engine's own F/T register, so the two sides no longer
disagree in code - the open question is purely which T dependence the
video shows, and only a BY_X object at T other than 40 can say. Next steps, in order: (1) that video test, which
the authored data supplies in quantity (F=100 against 11 different T
pairs); PR-125 ran the first two probes on R3 footage and both referee
objects (RYBKA A_RUN F=100 T=20, PINGWIN A_RUN F=75 T=50) animated
without translating until the lion was adjacent - §7 item 13's
PR-125 bullet names the next fixture; (2)
the two cheap tests PR-102 points at: the first is DONE (PR-107 applies the
authored start row on both sides; measured through the port's own parser,
of 2572 numeric transition lines 2299 ask for row 0 and 273 do not, 172
of them cross-state and only 25 in a scene-object script), and what the
data still owes is FOOTAGE of one of those 25 to say whether the port's
row is the engine's. The other test is CLOSED, by PR-109: the lion climb
states (BY_TY: 12 blocks in 7 files, 3 state names) were read end to end
by PR-108 - the mode-4 feed, its `[obj+0xbe]` guard, the contact record
that writes that register, and the fact that no BY_TY state declares
`time_scale=`, so the engine's row thresholds ARE the authored numbers -
and because that left the data unable to choose, PR-108 asked the owner
for one clip; PR-109 measured it and the answer is the ITERATION model:
an authored threshold T is displayed for T/100 frame-loop iterations, not
T milliseconds, so the port's row clock is 3.0-3.5x too slow and PR-110
is the behaviour change, with its plan named in the PR-109 record. Item
(1) above is now THREE negative probes and three facts, recorded in §7
item 13's PR-125 bullet, and PR-126's join: the fixture named
there, KADMAR.07's NINJA1-6, is WITHDRAWN - no `.des` places it
and no `.LVM` or `.MOV` references it - and the named referee is
now the KADMAR.07 BOSS A_RUNA/A_RUNB (BY_X, F=75 T=20: A 18.8
vs B 37.5), one instance at x=1328. PR-128 closes THIS file as a source:
its whole 1451.7 s is swept at the widest setting (93 camera-still
windows, 3.0-17.5 s) and a frame from the middle of every one has been
looked at - no window with the HUD drawn contains the boss, and the boss
figure that repeats through the cut is a scripted no-HUD scene (a black
room between two white-outlined wardrobe doors), which is also what the
763-810 s windows PR-125 and PR-126 measured are. The code side is now
read end to end (PR-129/PR-130): the integrator at 0x54250 - the
standard class-table slot +0xc8 - settles at (5*F*s/T)/100 px/tick,
where F and T ARE the authored `F=`/`T=` clauses (0x52680 copies the
object fields at +0x50/+0x58 into the command block verbatim; the
parser 0x52590 is the class's own vtable slot +0x00) and s =
[obj+0xba]/[obj+0xbe] is seeded from the record's own default factors
by 0x549f0 [s CLOSED IN PR-132: that "record" is a caller-built stack
pair, every producer is read, |s| <= 10 and s = +/-10 exactly at full
one-axis intent; no authored field can carry s]. What is open is the
PAIR (rate, s) [RATE CLOSED IN PR-133, CLOCK MEASURED IN PR-134]:
the only clock read in this repo is the ~28 ms iteration clock
(PR-109/110), while the published reproduces split between 20 Hz
(T=40 walks, s=10) and 25
ticks/s (the jump /100 reading), and the record default factors were
never censused. The video referee - LONA.05's YETI (F=100 T=50, one
instance at x=2100) - can only decide once rate AND s are fixed; an
iteration-driver read plus an s census settle it from code and data
first [s census DONE IN PR-132; the iteration-driver read is
PR-133, and it CLOSED the rate side: no separate object tick -
rate = iterations/s x passes-per-record-per-state-step, at most
1 pass for the moving families, so "20 Hz" is the iteration
clock at 50 ms, the top edge of PR-109's bracket]. The two-factor
fit is MEASURED IN PR-134 and its clock factor is settled: inside a
four-ways-verified camera lock the lion's ground run at a held key
is 38-76 units/s and the ZARIA worm's rate is <= 30.3 units/s (its
head eye tracked at 60 fps), so the pass rate comes out 15-24/s
from both families - 20/s, the 50 ms edge at one pass per state
step, inside both; 35.7/s, the 28 ms design point LL_ITER_MS,
outside both (it predicts 44.6 and 89 u/s against those bounds).
The pass factor's mechanism is read and its bound is 1. The clock and
the port's own speed constants are APPLIED IN PR-135: LL_ITER_MS = 50,
and the port's lion and objects now step the engine's own velocity
register once per iteration (`v -= clamp(T,.,130)*v/100`, then
`v += F*s/100` at the key intent s = ±10) with AT MOST ONE row admit
per iteration, so the port's ground run is that arithmetic rather than
a fitted target velocity. What is still open, all named in the PR-135
record: the vertical constants are not re-fitted (the authored jump
apex is now 20 px against the ~24 px measured), the engine's
conditional raise of T to >= 20, the partial-intent values of s below
±10, the object's own velocity register, whether a state change
consumes its step's feed, the force/gate-fed row lists inside A_JUMP
and A_DOSTAL, the velocity carried across a state change, PR-83's
pending-AC_TIME lifetime, the object `nfr == 0` guard, and the one-
point-per-iteration terrain test. Also still open: the rhino
referee - now placed,
`NOSOROG,dummy,nos.mov,1700,600`, bounds 1680,1950, but never on
camera in a sampled window - the state that plays the airborne leap
poses. The G-2 camera position IS MEASURED IN PR-136 - the landmark
anchor PR-134 asked for is built from the level's own static .SEQ
placements, and the whole 460 s clip now reads a camera (x, y) in world
units: the right-facing lead 95-108 confirms CAM_LEAD_R=100, the
grounded vertical anchor is ~128 plus the existing world clamp (not
178) - AND IT IS APPLIED IN PR-137, with the whole camera law pinned
hermetically by --follow-selftest; the catch-up ease stays a model
slower than the port's in the data, and the view does not hold rigidly
through jumps (both counted open). PR-136 opened G-13 and PR-138
CLOSED it: the "7-51 above" offset was a measurement artifact
(screen-x from false-matched single-landmark cameras); the engine's
ground records ARE the authored .PLT text, and every verified frame
stands the lion on a .PLT line at d+0. What remains ground-side is
the ANIPL item named in the PR-138 record (ani_plat= local lines
never parsed; the entity-table registration chain unread). See the
PR-130, PR-132, PR-133, PR-134, PR-135, PR-136, PR-137 and PR-138
bullets in §7 item 13.
tools/re/des_census.py prints the whole joined census. The
table
itself is now read end to end:
PR-102 found its writer (the base record parse method 0x51910 ->
0x51e07, entries of {key, target state index, start row} pushed into
the vector at `[rec+0x1a]`, and the derived record parser 0x52590 that
reads `F=`/`T=`/`G=` and delegates every other line to 0x51910) and
its authored form (`TARGET(N)=<key expression>` inside a state
section), and 0x47e40's four key-match cases. This port parses that
row number (PR-86) and ignores it, and PR-103 corrects the row number
itself: the engine stores `( N )` as N-1, or -1 when the part is
absent, and resolves the TARGET name to a state index at PARSE time
through the case-insensitive 0x53440. PR-103 then read the class map
PR-100 item 5 asked for: it is a name factory at 0x4df10 keyed on the
first field of the placement lines (case-insensitive), 163 class-table
pairs, 107 named classes including a TLew<level> lion class per stage,
and it shows the `[[obj+8]+off]` hook offsets to be per-class rather
than universal (0x543a0 sits at +0xa0 in 118 classes, 0x536f0 at +0x14
in 24). What is left of that line of work is now narrower: who
APPENDS to `[obj+0x3c]` and fills `[obj+0x50]` (the ctor chain only
initialises them; PR-103 open item 2), what the per-class methods do
- PR-104 read the first whole method of the TLew* lion classes (a
death-theme hook that plays a hard-coded `.MOD` through the music
singleton when the current state's row index is 0; the themes are
named NOWHERE in the authored data) and the rest of their bodies is
still open - PR-105 corrected where that hook lives (the +0x08 key
builder of the RECORD sub-tables, not a lion object slot) and closed
the pacing chain end to end: the step 0x521f0 has ONE call site
(0x5240c, in the record-step driver 0x523f0), which has one call
site (0x5371c, inside 0x536f0, class slot +0x14), the feed
dispatcher 0x543a0 has one call site inside 0x521f0 itself, so ONE
+0x14 CALL = one key composition + one feed update + at most one
row, and no other scheduler exists; PR-107 read what that one row
advance touches: the live row counter is the state record's own
`rec+0x4` (0x520c0 reads, wraps and mirrors it; the row-setter 0x52360
is the state record's vtable slot +0x1c in the table every record stores
at rec+0x44 - 0xa25d4, written by its constructors 0x524cf/0x52508 - and
the transition step calls it on the NEW record at 0x535ea), so the
engine keeps one row counter per state record while the port has one
`o->fi` per object shared by all its states;
"100 units" of the level global's +0x388 feed is half-closed
(PR-100: the single writer is `mov [eax+0x388],0x64` at 0x4c8b7) and
PR-102 found the gate; PR-108 corrects its wording and reads its
producer: that feed runs unguarded for mode 2/BY_TIME (0x543fb), only
mode 3 is behind `[obj+0xba] != 0` (0x543f2) and mode 4 is a separate
body behind `[obj+0xbe] != 0` (0x5441b), and both registers are written
by the input-apply method 0x549f0 - the frame loop's slot +0xcc - from
the input block and the contact record at `[obj+0xd2]`. And PR-102 found the
concrete mechanism the pacing tension was missing - the row's threshold
is stored pre-scaled at parse time, `[node+0x10] = T * time_scale /
1000` (0x51660), and 0x52320 compares that against the accumulator, so
a threshold of 200 with the 1000 default is 2 feed steps and the same
row under `time_scale=2000` is 4. What a row's duration is, in
seconds, used to be exactly one unread number: how often the +0x14
slot (0x536f0 and its wrappers) runs during play. PR-106 read that
caller - the level frame loop 0x4abd0 (one caller, 0x5d251 in the
level-run wrapper 0x5d170) calls the think pass 0x4ba90, which steps
an object through 0x52b80 -> +0x14 only when its slot +0x60 answers
non-zero, and 132 of 162 measured class tables have +0x60 = `return
[obj+0x2c]`, a flag written by the drawing pass at 0x4b970 through
slot +0x64 (a rect-in-view test). PR-105's "the pump 0x4bb30 steps
every object in the level's four lists" is corrected: it is a
filtered message broadcast. What is left is the loop's own period,
and that is now a measurement question, not an RE one: the pacer
0x4a120 (called from the loop at 0x4aef5 via 0x4b0e0) waits for a
setpoint at [0xcef38] that startup code 0x4a190 authors as 2 x
measured-retrace-ms - 1 with a 27 ms fallback (= 2*14-1) and then
syncs to one VGA retrace (0x1dba0: port 3DAh bit 3); the ms clock it
spins on moves in 55-unit BIOS ticks (the INT 8 chain 0x1c6d0 ->
0x1c680 -> 0x1c5b0, the clock started by 0x5b860 from the level
wrapper at 0x5d18b); the video mode is 320x200 via INT 10h AX=0x13
(0x4a230) and the sound init sets PIT counter 0 to 70.0 Hz
(0x18870). So the row-to-seconds factor depends on which of those
rates a real machine actually ran at (NEXT_STEPS PR-106 open item 2)
- the port's 20 ms tick is not re-pinned here.
(3) what writes
[obj+0xba]/[obj+0xbe] (0x54a00..0x54ab0) - they are now known to be
the gate of the time feed, but their meaning is unread - and where the
full-precision velocity moves the coordinates (PR-99 item 10; only
`v/100` is stored);
(4) the other video tests PR-98 lists (the jump's x track against the
port's one-shot `ix/4`; an `FC` window in A_FIGHT), then a PR that puts
both sides on one measured law with every play pin re-pinned from it.
STATUS PR-205: the ordered re-watch is DONE, with its own status
block below (the "STATUS after PR-205" paragraph under the crossing
line, which closes the "until re-watched" condition and corrects
PR-131's "down at t=25-28" wording).
STATUS PR-206 (the register's video test, RUN, and it REFUTES the
register model): LONA.05's YETI, the census's only clean referee
left standing (BY_X, A_RUN its only F-nonzero state, `F=100,0`
`T=50,5`, bounds 2000..2350), is on film in levels/LONA.05.mkv
t=437-484: `video_stills.py` finds four camera-still windows,
`video_lane_shift.py` confirms the lock on decor-only bands (best
shift 0, sharp minimum, three windows), and the object-mask
centroid track at 30 fps shows GROUNDED walks (feet row constant,
blob height 55-60, no jump arc) in three sustained bursts: 24.9,
24.9 and 22.0 world units/s over 2.4-2.6 s spans. The shipped
register law predicts 19.8-20.0. Two of three bursts agree with
|F|/4 = 25.0 within 0.4%. That is register row OI-107. The unit
bridge is pinned too: PR-125's LONA.05 blobs of "55-65 game px/s"
are 640x400-frame px, i.e. 27.5-32.5 in my 320x200 world units,
and my YETI per-frame spread runs 27.3-30.3 in those same units -
the scales reconcile; and those blobs were NOT penguins (FOKA and
LONASUR1 are BY_TIME, `F=0,0`), exactly the caution PR-125's own
bullet named. What the test leaves is the 1.25 factor: the un-read
SECOND value of each pair (`T=50,5`, `F=100,0`) is the only
candidate home inside the authored row.
STATUS PR-207 (step 5a RUN; the port mechanism is FOUND, the film
answer is NOT, and the PR-206 numbers are RETRACTED): the probe
`LEO_OBJ_POS` prints what the port does - every ZARIA `animacje=`
ANI2 object (POCISK x16 fruit, BAL x6 balloons, the DRABINKA
ladder, LINA ropes) reaches obj_draw at `layer=1` through the 85/66
draw-cam the PR-61 entry-index rule computes; ANI1's PICT statics
reach the reference plane. That rule has NEVER been read from the
machine code. The re-measurement at native resolution (the level
clip is 1280x720 = exactly 4x the view; the old 576x360
intermediates are gone) locks the t=22.0 camera y=150 sharply but
x only to 28..37, and t=31.25 y=207 sharply with x 240..260,
against controls 6-9 MAD worse. Two traps showed: the ZARIA
recording carries a screen-fixed red graphics column (x 289..320
y 53..128) exactly where fruit falls in view, and POCISK/kokos.mov
IS the bullet chain - the film's fruit moves. After the overlay
diff, t=22.0 shows exactly ONE KOKOS-sized disc and t=31.25 two;
they match neither the port's 85/66 boxes nor the 1:1 boxes
consistently (t=31.25 second disc ~1 px from 1:1 x of (322,298)
where y cannot discriminate; t=22's disc 16 px above every 1:1
box), and the (95,320)/(115,320) pair is absent from the film
entirely at that moment. So: the port's fruit/balloon placement set
is NOT the film's - the user's complaint stands - but the plane
law cannot be pinned from moving fruit; the machine code owns that
question. (5b) is now: PR-208 [PORT] reads the engine's object-draw
camera first (leads: FUN_0005b040 case 0x10 writes [rec+0x24] 0/1
on token 0x5e; FUN_0005da60 tests it), states the law with a
failing pin test, then fixes it; the film cross-check must use
native /4 extraction, the overlay diff, and the static (non-red,
stateless) ANI1 PICT art as the template, not the bullet-chain
fruit. The PR-205/206 x-locks (x=1, x=231) and the "within 2-4 px"
fruit matches did not reproduce and are retracted in the register.
STATUS PR-208 (step 5b RUN, AND THE PLANE LAW IS FOUND - in the
LEVEL'S OWN DATA, not in the film and not by guessing): every
shipped level .des carries `plany=` with exactly one '|'-separated
spec per tlo= layer, letters only {T,A,L,B}: T once per spec = that
layer's own art, L once = the `lew=` lion and it always sits on the
main_back= spec (which is the 100,100 pair on all 12 levels), B
once = the `belka=` belt, also on a 100,100 spec, and A appears
exactly as often as the `animacje=` list has cells - 3=3, 3=3, 3=3,
2=2, 3=3, 3=3, 5=5, 4=4, 4=4, 4=4, 3=3, 0=0. The k-th A in reading
order IS the k-th cell, and it rides the layer whose spec carries
it. A bare `none` inside a list is a cell and consumes a letter
(RAN.01's none,wulkany,dymy has 3 A's); a list that is only `none`
names no cell (JUNCTION "TL"). ZARIA.01's planies T|TA|TALAB with
speed 39,40;85,66;100,100 main_back 2 therefore puts ANI3.DES on
layer 1 and ANI2/ANI1.DES - the 16-fruit POCISK chain, the 6 BAL
balloons, the DRABINKA ladder, both LINA ropes, the user's exact
set - on layer 2 = main_back = 1:1, which is also what the film
said at t=31.25 (~1 px from the 1:1 x of the (322,298) entry: the
entry-index rule could not produce that). Engine side at raw
machine code: 0x57500/0x57530 take the layer in EDX and index
[scene+0x20] by it, dividing by the row [scene+0x1c] selects =
main_back; 0x52d67/0x52d72 load it from [obj+0x24]; 0x23b90
(`mov [eax+0x24],edx; ret`) is verb +0x04 of 211 class tables, and
the pool ids at [level+0x40]++/[level+0x58] are key-occurrence
ids, not cells (PR-207's FUN_0005b040/FUN_0005da60 leads are NOT
this field). The port now implements the law (plan_t, plany_parse,
des_list_cells, the `ll_level: planies: ...` report, one mapping in
the placement loop), pinned by plany_selftest in the always-on
--mov-selftest bundle and by a two-run verify-level leg
(lv_plany/lv_plany2) that pins the corrected cameras AND the
unchanged `zwierzaki`/`ani_plat` controls; verify-all 65 ok (the
+1 is that leg), fixture-free gate unchanged at 13 ok + 21 named
SKIPs, 0 warnings. STILL OPEN: which plane the two UN-lettered keys
use in the engine - new register row OI-109 (the port keeps
PR-61's entry-index model there; in ZARIA it coincides with the
lion's own plane for ZWIERZ1, which is what the collision gate
needs, but that is not proof); and the film re-check of the
corrected scene, because the PR-207 film boxes were measured
against pre-PR-208 renders and the POCISK chain moves - the
re-check must template-match the stateless ANI1 PICT art.

STATUS PR-209 (OI-109 follow-up READ, no behaviour change): the
engine's planies consumer is 0x4af10 (exact bytes; the string it
runs is [level+0x30], parsed by the loader at 0x4d230 with '
%s' from the "plany" literal 0x91eb4). It is a PROGRAM, not a
plane table: 'T' drives the tlo layer objects (the T ordinal =
the layer index; the first 'T' also skips one character the
shipped strings never carry), 'L' activates the lion
[level+0x394] and walks a second chain at [level+0x64]
(0x4b2f0), 'B' runs the belt pair ([level+0x38c] -> 0x448c0,
0x4a170/0x49bd0), 'A' k calls 0x4b360, which walks cell k of
EACH of the three placement-key pools (one pool per key,
[level+0x58 + keyordinal*4] chain-heads arrays, counts at
[level+0x44 + keyordinal*4], cell parser 0x4c720 with ' ' "%[^,
" 0x91d6c - our des_list_cells' grammar) running verb +0x60
then +0x74; any other letter prints "Bledny opis poziomu -
plany" (0x91d2a). The pool layout confirms PR-60/PR-170 exactly,
and the new data law is that planies A counts are sized to the
authored lists. WHAT DID NOT CHANGE: PR-208's plane law and the
shipped mapping - no handler writes [obj+0x24]; 0x4aaa0/0x4dd50
build and chain placements without it. WHAT DID CHANGE:
PR-208's "0x23b90 = verb +0x04 of 211 class tables" is
WITHDRAWN (the slot +0x04 census: 158 ctor tables hold only
0x23850/0x23ea0/0x44ec0; 0x23b90 is an accessor thunk inside
FUN_00023850, the pair 0x23b80/0x23e00 reads +0x2c/+0x24) - the
211-value scan stands, its reading is corrected. OI-109's leads
are now concrete: the writer of [obj+0x24] (the +0x24 accessor
thunks, 146 slot +0x04 call sites, or the entry-ctor family
behind 0x4df10), the meaning of the +0x60/+0x74 pair, and the
unnamed feeder of the [level+0x64] chain (PR-105 item 7). The
TUNGA.11 bound check (5 A's vs a 1-cell zwierzaki pool) is
stated as measured. Docs + comments only: 65 ok / 13 ok + 21
SKIPs unchanged, 0 warnings.

STATUS PR-210 (OI-108 remainder READ - the film re-check of the
corrected scene - and it CLOSES the row): PR-207's ZARIA camera
numbers were a PIPELINE artifact - their scans resized the full
1280x720 frame straight to 320x200, 0.9y squash. With the
correct /4 + 20-black-row-pad pipeline (tools/re/video_camera_read
.py --now carries --native for it) the t=22.0 lock is (30,162),
sharp, and law-free: no-obj and obj-on full-frame curves agree
within 0.1 MAD across a y sweep of 140..172 (a placed object is
~0.05 MAD on a full frame). The t=22.0 red disc PR-207 measured
(279..287.8,131.5..141.8, valid coordinates) then sits ON the
shipped law's design point POCISK (310,298) -> (280..292,
136..148): 1 px x, 4.5 px y, and the chain moves; the "absent
pair" at (95,320)/(115,320) is chain travel. t=31.25 -> (235..
242,214..216) shallow, discs within chain travel. The ANI3
waterfall window (t=70.5/72.5/75) adds a second, independent
plane hit: ANI3's WAZ column measures at film x 257/185 =
exactly the shipped L1 law (0.85 rate) with the 1:1 palms
adjacent, while PR-61's entry-index (ANI3 -> layer 0) could
never show that water at all (on-screen only for camx
3315..4190). Measured tool lesson: landmark-template camera reads
are sharp on port frames (self-tests rank-0/1px) but false-
positive on this film - a film camera needs the full-frame MAD
or colour-anchored art edges, never single-template NCC. Leads
kept inside the closed row: the camy~520-vs-YMAX500 + lisc2 at
y=1546 geometry puzzle, and the unmined ZARIA.02/03 clips.
LEO_OBJ_ART probe was reverted unused; only --native ships.
Gates unchanged: 65 ok / 13 ok + 21 SKIPs, 0 warnings.

STATUS PR-211 (OI-107 READ - the YETI physics chain is now
read end to end, and 1.25 has NO home in the register): the
axis law is machine-code - the parser 0x52590 (a slot of the
object's own class table, +0x1c8 in the [obj+8] view 0x9f298)
stores F/T/G as (X,Y) pairs, the merge 0x52680 (+0x1f4) feeds
the ctx, and the integrator 0x54250 (+0xc8) reads firsts for
x and seconds for y: lead "the second of each pair" is DEAD
for the ground walk. The intent s ([obj+0xba]/[obj+0xbe]) has
exactly one writer, the input-apply 0x549f0 (+0xcc), and
every IN path into it is multiple-of-5, |s| <= 10 on x (the
0x27d60 chase normalizes by 10.0/(|dx|+|dy|); the 0x34495
terrain path is 10*[obj+0x6d]; the dead-band stores 10/-10/0)
- lead "partial intent above 10" is DEAD, and 22.0-24.9 px/s
would need s = 11.0..12.5. The law matches the shipped port
(steady x = 5*F_x*s/T_x per step); the whole contradiction
moved to ONE named lead: the per-frame CALL RATE of the step
wrapper 0x52750 (integrator dispatch at 0x52774, exactly one
in the image) at its ten class-driver call sites, against the
PR-134 display cadence. Row stays OPEN. The YETI class graph
('YETI' 0x91247, factory 0x38498, ctor 0x73e50, views 0x9f298
/0x9f290) and the .elf-vs-.img table-read caveat are recorded
in the PR. Docs only: 65 ok / 13 ok + 21 SKIPs unchanged,
0 warnings.

STATUS PR-212 (OI-109 READ - THE PLANE LAW IS FOUND): the
plane of EVERY placement entry - all three keys, un-lettered
included - is set at level load by ONE pass, 0x4b1d0 (its
only call site 0x4d764): list entry i chains at cell i raw
('none' occupies a head), and cell c of any pool rides the
planies segment that carries its 'A' - the count of '|'
before the (c+1)-th A of the level's planies program, last
segment if the A's run out - dispatched through class-table
slot +0x5c (0x23b90 plain store in 150/177 tables; the
image's ten +0x5c dispatches are all accounted for: the
pass, the lion := main_back at 0x4d68b, four creation-time
child inheritances, and the propagation verbs 0x1ecb0/
0x332f0/0x554f0/0x531b0). The letters EXECUTE, the PIPES
place: that is why no spec names those keys, and why ZARIA's
animals riding the lion's plane 2 is a consequence of the
law. Measured divergences (LEO_OBJ_POS): port raw-index
model vs engine - ZARIA KAMIEN 1 vs 2, LONA.05 PINGWIN/YETI
2 vs 1 (engine puts them on the lion's plane 1), KADMAR2 0 =
0. The row's animacje map equals the engine's law verbatim
(TUNGA.11 1,2,2,2,2; ENA2 2,2,3). Next is a PORT PR,
failing test first, re-deriving the lv_plany2 controls.
Docs only here: 65 ok / 13 ok + 21 SKIPs unchanged,
0 warnings.

STATUS PR-213 (THE PLANE LAW SHIPS - OI-109 CLOSED, OI-110
OPENED): plany_parse now fills cell_plane[] = the engine's
raw segment law (PR-212's pass 0x4b1d0: '|' count before the
(c+1)-th 'A', last segment once the A's run out, no
validation gate) and des_placement takes the plane from it
for ALL THREE keys; the PR-61 entry-index model is retired
(except for a no-plany= fallback no shipped level uses).
Failing test first: the lv_plany2 KAMIEN pin moved to the
engine value (layer 2, draw-cam (1200,280)), verify-level
ran RED, then green after the change; the selftest grew
cell_plane expectations for all eight strings. Measured
after: ZARIA KAMIEN 1->2 (stones now on the lion's plane),
LONA.05 PINGWIN/IGLO 2->1 = the reference plane, KADMAR2
and every animacje pin unmoved, verify-play's ZARIA animal
legs unmoved. Collision-equality effects are now the
engine's, by law. 65 ok / 13 ok + 21 SKIPs, 0 warnings.

STATUS PR-214 (READ, OI-078 LEAD ANSWERED NEGATIVE): nothing
sets the lion's contact target [obj+0xd2] per frame. The only
appends are PR-146's asker-append (0x3d618, inside the +0x78
entry 0x3d440 held by two non-lion tables) and 0x1ef71, in
the handler referenced exactly once in the image - 0x97050 =
LION base 0x96874 + 0x7dc - gated on the counterpart's
[obj+0]==2; heads are only zeroed (0x5415b/0x54e88) or
popped (0x551f0). New [obj+0x24] consumer named for OI-110:
TLewZaria's +0x7dc 0x531b0 answers pair-asks by plane
matching, and 0x4b140 derives the lion's plane from the
planies string ('A' before the first 'L' = 2 for ZARIA),
resolving cells through 0x4b0f0/0x4aa70/0x52c10. Docs only:
65 ok / 13 ok + 21 SKIPs, 0 warnings.

STATUS PR-215 (PORT, OI-092 ROW-SEEK HALF): the lion's own
states now honour the `(N)` their scripts author. A
self-target transition line in player_iterate writes its
authored start row into the running state's row counter
before the cross-state arm, exactly where the engine's
applier does it (0x5360f-0x53616), keeping the PR-172 rule
that the arm inlines ONLY that write (no ENTER, the row
feed/carry survives). An absent/non-numeric token writes
nothing: the engine's -1 is refused by its row-setter
(0x52381) and the census (217 self-target lines, 22 on held
K_FWD/K_BKWD, 19 of those tokenless, all walk states) rules
out "write the top" - it would pin every walk to row 0,
against PR-79/PR-134's video gait. Failing test first: the
new ll_play_selftest leg measured red at five steps without
the arm, green with it. No shipped fixture pin moved: 65 ok
/ 13 ok + 21 SKIPs, 0 warnings. Open half registered on
OI-092 (raw -1 on the same-state cursor 0x53616 vs 0x52381;
held-arrow edge-vs-level).

STATUS PR-216 (PORT, OI-078 GATE HALF CLOSED): the ladder's
contact answer is gated on the engine's STATE index, not the
row cursor PR-150 shipped, and the loader names that number
space: the state list a script declares BEFORE its blocks,
ONE-BASED (0x53905 fills the vector [obj+0x3c], 0x53440
returns position+1 or 0 for an undeclared name, 0x534b6 stores
the record at that slot of [obj+0x50], the record-ctor loop
starts at slot 1, and the object ctor writes [obj+0x5c] = 1 at
0x538ab). ll_mov_parse now keeps that list (m->decl) and gives
every block st[].decl_idx; ladder_answer_core gates on it
(`state != 2` answers nothing). ZARIA.01's own DRAB.MOV
declares A_UP,A_DOWN,A_DROP but writes the blocks
A_UP,A_DROP,A_DOWN, so a placed ladder sits at index 1 = A_UP =
the child-ANY branch (0x3f1e4/0x44ec0, signals nothing), index
2 = A_DOWN is the ONLY state that answers the lion, and A_DROP,
the unrolling rope, is 3 = inert. That corrects PR-158/159's
value mapping and closes their "binding order / encrypted pool"
item. Failing test first: the L2 leg measured red at `state 1
got 1400 want 0`; a second red was my own wrong expectation (a
lone declared state IS index 1 and answers nothing) - the leg
was wrong, not the model. Census: 172 of 777 texts differ
between the two orders, 54 sites compare [obj+0x5c] with a
literal (21 of them `== 1` = the initial state), one file
matches only case-insensitively so the compare folds case. The
ZARIA.01 walk probe is unchanged (no answer line; x=180 at
tick 91, x=685 at 300) - the placed ladder answers in neither
the old nor the new model. OI-078 now has ONE named blocker:
what raises K_GEN1 on a DRABINKA. 0 warnings; 65 ok with
fixtures, 13 ok + 21 named SKIPs without.

STATUS PR-252 (READ): the integration surface is complete
and the `I`-consumer hunt closes negative. 0x54250 has
exactly three call sites: the direct physics slot of 127
class tables, two wrapped slots (0x989b4 -> 0x24700, which
adds +60/+30 to the T pair when the class word is >0x640
and the state decl is 1/3/4/0x10; 0x9d528 -> 0x33310,
which shifts G_X by +/-5*[obj+0xf2] at decl 3/5), and one
code site 0x321fe inside a class frame driver (its 0x321c0
adds class-derived terms to G_Y and G_X at decls 3/4 and
0xb/0x11/0x12). None reads block+0x20/+0x24, so the id-2
`I` pairs have NO consumer in the engine as read - PR-98's
account that the I sum joins the pull pair is the only
behaviour-making reading. The integrator itself is now
confirmed slot-exact: T pair +0x8/+0xc, F pair +0x0/+0x4
(over [obj+0xba]/[obj+0xbe] /20), G pair +0x10/+0x14 with
only the X leg scaled by [obj+0x6d], pull pair
+0x18/+0x1c, velocities [obj+0xc2]/[obj+0xc6], per-iter
deltas [obj+0xca]/[obj+0xce] = v/100. And OI-015's
conditional T>=20 raise is measured: 0x54266 calls the
class verb at table+0xbc; when it returns true, block+0x8
< 20 and [obj+0xb6] bit 1 is clear, 0x54275 sets T_X = 20
- the Y term has no clamp. NEXT (PR-253, test first): the
ask-channel port - drive the ask per step from the
requester's CHECK_ANI/CHECK_ZW bit matched with o->src.

STATUS PR-251 (READ): the `I` channel has no consumer, and
the ZARIA.01 jump arc corrected. The record-class verb
tables (15, base e.g. 0xa25d4) carry +0x18 = 0x520c0 (the
row-advance, the handler the state step calls at 0x52338),
+0x2c = 0x52680 (the block merge), +0x34 = 0x52790 (the
ask dispatcher); the +0x30 post-integrate verb of the base
class is 0x2b4d0 = `ret`, and no provider 0x4ff80/0x61250/
0x617d0/0x61830 reads block+0x20/+0x24, where the id-2 'I'
query writes (0x52731/0x5273d; the integrator reads only
+0x0..+0x1c). So `I` pairs land in an unconsumed slot while
`F` reaches the pull pair +0x18/+0x1c and `FC` reaches it
ungated - PR-98's account that 'I' joins ctx+0x18/0x1c is
the other reading; the bytes stand either way. Taking PR-98
with the one-shot law and running the port's EXACT order
(v += G; v -= v*T/100; integrate), the authored A_JUMP arc
is 109 px rise, apex iteration 19, air 39, land 40 - the
PR-250 "50 px" was an arithmetic artifact, withdrawn in the
PR-251 record. From the ground contact 649 the peak is 540;
the authored ladder point is 507, 34 px above it: the climb
needs the A_PRE_JUMP/A_JUMP_S route, not a re-fit. NEXT:
0x33310 (the only integrator variant that edits the ctx G
term) as the 'I'-consumer lead, then the ask-channel port.

STATUS PR-250 (READ): the row-node channels and the
jump's row-entry one-shot. The node factory 0x46878 builds
one node class 0xa1090 for every pair tail and stores the
CHANNEL ID at [node+0x10]: 'I' 2, 'F' 0, 'IC' 3, 'FC' 1,
'M' 4 (token strings 0x91966..; pair-verifies the middle
band PR-224 left open). The merge 0x52680 queries id 1
('FC') into block+0x18/0x1c - the pair the integrator's
velocity-SET leg 0x5433a/0x54348 feeds to the soft pull
0x54170 - id 3 ('IC') into the F= force pair, and id 2
('I') into block+0x20/0x24, whose consumer is still
unfound. The gate is the [obj+0x95] flag: the state-record
step 0x521f0 clears it at its head (0x52217) and sets it
only in the row-admission tail (0x5232b compare, 0x52338
row handler, 0x5234a set), so 'F'/'I' pulls are ONE-SHOT at
row entry; the sums are of the CURRENT row (chain-head
table at [rec+0x2e] indexed by the row cursor [rec+4],
0x5268e/0x52221) - PR-98's item 3 and PR-100's caveat are
closed. Arithmetic on ZARIA.01's authored A_JUMP (T=20,1,
G=0,55, F 0,350 x2, I 150,-1200): one-shot v_y=-1200
(-12 px/iter), apex 50 px at iteration 15 on the 20 Hz
clock - CORRECTED BY PR-251: run in the port's exact
order that arc is 109 px, and the 50 px figure was an
arithmetic artifact. NEXT (as PR-251 later shipped): the
'I' consumer lead 0x33310, then the ask channel.

STATUS PR-249 (READ): who asks whom. The control word has
THREE channels in the dispatcher 0x52790: CHECK_LEW (0x200)
-> the walk 0x4bd30; CHECK_ANI (0x40) -> 0x4b890; CHECK_ZW
(0x80) -> 0x4b870, the last two walking the class groups at
[ds:0xa94ec]/[ds:0xcef44] through the shared walk 0x4b500
(instance count [class+0x44], array [class+0x58], chained by
[obj+0x34]), with the same law as 0x4bd30 and NO shape test
anywhere: [class+0x60] candidate filter, plane test with the
CHECK_ALL_PLANE exception, skip self, then call the
candidate's +0x78 with ebx = the requester. The keyword
masks are measured one instruction each: CHECK_ALL = 0x1c0 =
ANI|ZW|LN (0x51c73) and CHECK_LEW is a separate keyword
(0x51c8f); CHECK_BASE_LN = 0x4000 (0x51ca9, not 0x400000),
CHECK_ALL_PLANE = 0x8000 (0x51d2b). Authored census:
CHECK_LEW in 535 control lines / 205 files, CHECK_ALL 360.
The port's ctrl_flag already matches 0x1c0 - OI-113's
CHECK_LEW addition is withdrawn, half one closed. Why the
climb is stuck: ZARIA.01's DRAB.MOV authors `control=BY_TIME`
in all three ladder states, so the port's solid loop skips
the ladder before the shape test (a temporary probe print at
the ask call: zero evaluations across a 6 s walk and an 8 s
walk+jump run, including tick 91 at the ladder column), while
the engine reaches it from the lion's CHECK_ANI channel.
NEXT: PR-250 - drive the ask per step from the requester's
CHECK_ANI/CHECK_ZW bit matched with o->src.

STATUS PR-248 (PORT): the ask is not gated against the
authored rectangle - PR-246's filter is deleted. The walk's
candidate filter (0x4bd81) calls class verb +0x60, which for
the ladder family is 0x23b80, the plain [obj+0x2c] getter 156
classes use, so the port's shape meet stands for it as before;
the rectangle test 0x23140 is class slot +0x64, called by the
broad phase 0x4b970 against a LAYER'S ACTIVE WINDOW (0x4b940:
the layer origin from 0x57500/0x57530 expanded by
-0xa0,-0x78,+0x1e0,+0x140), and its answer is one of the
writers of that contact node. TEST FIRST: the re-pinned L8
case - a lion box covering the authored point (213,507) but
missing the authored rectangle (y 540..670), which IS the
authored geometry since the point sits above the rect - fails
on the filtered build (exit 1) and passes after the deletion;
the other seven L8 cases and the L9 grammar legs are
untouched. The rectangle parse, its absolute x0,y0,x1,y1
storage (0x3f0f0 + 0x3f124 confirm both) and the report line
stay - the line's wording now names the broad phase.
Measured: the walking probe's log is identical to the
filtered build's apart from that wording, still no K_GEN1
(the jump half is the real gate); make 0 warnings, verify-all
65 ok, fixture-free 13 + 22 named. NEXT: the jump half, then
the AC_DRAB/AC_DRABK answer branch (0x3f214); the active-
window culling model waits on the layer transform.

STATUS PR-247 (READ): the ask's filters, measured. The
authored rectangle is class slot +0x64 (NOT +0x60, which is
the plain [obj+0x2c] getter for the ladder like for 156
tables), and its caller is the broad phase 0x4b970: for
every layer it builds that layer's screen window (0x4b940:
origin from 0x57500/0x57530, expanded -0xa0,-0x78,+0x1e0,
+0x140 = 640x440) and asks every instance of the object's
class chain and of [obj+0x64] through [class+0x64]; 0x23140
answers authored-rect-in-window and writes [obj+0x2c].
Census of +0x64: an empty ret (always yes) in 109 tables,
0x53490/0x23ba0/0x23bd0/0x37380 in 21/16/4/2, and 0x23140
in exactly two (0x97cdc, 0xa1dfc). So PR-246's asker-box
filter is a test the engine does not run - the ask is not
gated by the authored rect; the rect gates whether the
object counts as in-play. Confirmed instead: the record
parser 0x3f0f0 reads list -> rect -> third block in that
order and 0x3f124-0x3f14b makes the rect ABSOLUTE with
layout x0,y0,x1,y1 (the port's place_rect matches); the
third block is [obj+0xc5]=(v,y-0x50,v,y) plus a
(width+1)-element container, counted not modelled. The
point gate's asker rect is the live box [obj+0x75] via
class verb +0x4c = 0x23a20 for lion and ladder alike -
runtime state (0x534e0 from the +0x68 verb: the lion's is
`add eax,0xd6`, the current frame), which is what the
port's player_shape_box stands in for. The walk 0x4bd30
walks [[0xcef40]+0x64], the global scene list, with the
layer test [cand+0x24] vs [requester+0x24] unless
CHECK_ALL_PLANE is set. NEXT: remove the asker-box filter
(test first), then the jump half.

STATUS PR-246 (PORT): the record's extra lines are now all
read and the authored rect gates the ladder. The placement
loop follows an entry through its continuation lines in the
engine's order - `N dx,dy` list, `x0,y0;x1,y1` rect, bare
`x,y` pair - consuming and counting all three (the PR-243
one-line version never saw a rect); the rect is stored
ABSOLUTE on the placement. Measured on shipped data:
ZARIA.01 reports DRABINKA's rect absolute (203,540)-
(233,670) (= 180,510 + 23,30;53,160, PR-242's exact
rect) and the (125,292) LINA's (148,322)-(156,492); the
full census is read - 11 entries with lists, 21 records
with any extra lines, across 14 .DES files; TUNGA.11
ANI2.DES's six rect-only entries are XCHG x3, KLINA x2,
GUARD x1. ladder_ask_raise now applies the walk's first
gate first (0x23140: authored rect must intersect the
asker's live box; entries with no rect keep the generic
contact-node path, already stood in by the shape meet).
TEST FIRST: the L8 case "box covers the point but misses
the rect" fails without the filter (exit 1) and passes
with it; L9 pins the grammars (reversed rect sorts -
counted model choice; `30,50` is a pair not a rect).
Nothing else moved: walking probe unchanged (no K_GEN1 -
the authored point is above a walking lion), verify-all 65
ok, fixture-free 13 + 22 named, make 0 warnings. NEXT:
use the parsed rect for physics collision, then the jump
half so the point gate can pass, then the pair set's
consumers.

STATUS PR-245 (READ): the ask channel is the CONTROL WORD.
The per-step key builder 0x523f0 calls the record class's
+0x10 verb with the live set (0x5242e-0x52435); for the
placed rope that verb is 0x52790, which switches on the
control word [rec+0x12..] that the PR-230 keyword table
fills: CHECK_LEW (engine 0x200, port 0x8000) -> the ask
walk 0x4bd30 (the CHECK_ALL_PLANE bit passed as its layer
argument, 0x527c0-0x527c5), CHECK_ZW (0x8000) -> 0x4b890,
CHECK_ALL_PLANE (0x8000000) -> 0x4b870. CHECK_ALL ORs all
four check bits (0x51c73, 0x1c0 = ANI+ZW+LN+LEW) - the
port's ctrl_flag CHECK_ALL lacks its CHECK_LEW bit (OI-113
half one grows that entry; no port code tests the bit yet).
Measured in the shipped data: ZARIA.01's DRABINKA/LINA
states are bare control=BY_TIME (records never ask), the
lion's states carry CHECK_ALL / CHECK_BASE_LN - so the ask
runs from the LION per step, exactly PR-150's shape with
its enabling bit now named. The walk's candidate filter is
the kind verb +0x60: 156 of 186 tables answer 0x23b80 =
`mov eax,[obj+0x2c]; ret` - the contact-node field, set to
1 at 9 sites - so only a TOUCHED object is asked; then the
layer must match unless the CHECK_ALL_PLANE bit is clear.
The rope's own +0x60 (0x23140) is a rectangle-intersect of
the AUTHORED rect [obj+0xb1..0xbd] against the requester's
live box, writing the result to [obj+0x2c] - a second live
use of the authored rectangle (with PR-244's state-2
answer): the engine collides with the authored rect, not
the art bbox. Corrections: the ask bit is 0x200 not
0x20000; 0x52790 sits at 0x97cd0, 0xc below the kind
table, not at slot +0x14; and the control chain starts at
BY_X = default/sets nothing, BY_Y = 0x1, BY_TIME = 0x2
(PR-239(e) shifted it). NEXT: [[obj+0x64]] and the
contact-node writers, 0x4b890/0x4b870, the authored rect as
the port's collision box, then the AC_DRAB answer.

STATUS PR-244 (READ): the ask itself, end to end. An ask is
a record-class row-bit channel: 0x52790 is a method of the
placed rope's own class table (slot +0x14 at 0x97cd0); bit
0x20000 of its row-step word calls the delivery walk
0x4bd30 (out-packet, level singleton 0xcef40, the row's
OBJECT word, that word masked 0x8000 as a layer filter);
bits 0x40/0x80 call sibling branches 0x4b890/0x52805 - all
answers OR-merge into the set the transition key is built
from (0x523f0's +0x10 verb at 0x52435). 0x4bd30 walks
[[requester+0x64]], keeps candidates passing their kind
+0x60 verb and matching layers, and calls the candidate's
+0x78 with ebx = the requester; with no candidate it falls
back to the level member [[0xcef40+0x394]]'s +0x78. The
parent gate PR-242 censused is called at 0x3f1d1 - on the
REQUESTER, at the entry of 0x3f1c0 - not at 0x3f1e9, which
is the rectangle getter (PR-240 citation corrected;
re-census confirms 171 fail/15 pass, the lion among the 15
at 0x9699c; the rope's own +0x3c = 0x23270 returns the
never-null global 0xceeac, unique to that table). The
rope's STATE-2 branch (its A_DOWN, 0x3f214) ANSWERS:
requester position in its first authored element -> bit 8 =
AC_DRABK; in the rectangle built from [self+0xb1] (0x49fb0,
descriptor 0x98274) -> bit 12 = AC_TIME; else bit 10 =
AC_DRAB. Signal names are shipped in table order at
0x919ec and match the port's enum bit-for-bit (AC_DRAB=10,
AC_DRABK=8, AC_TIME=12). The lion half is authored:
ZARIA.01 LEW.MOV `A_WSP(1)=AC_BREAK_NEXT,AC_DRAB` (328),
`A_FALL_DOWN(1)=!AC_DRAB` (363); LONA.05 lines 58/59 climb
ropes with AC_DRAB/AC_DRABK. No code shipped: the port's
ask sites are lion-contact-only, and the lion class passes
the 0x3f1d1 gate by construction. NEXT: the control-name
bit map (the three row bits), [[obj+0x64]] and the +0x60
filter, the authored rect as a live box (then a hanging
rope can answer AC_DRAB), the other 18 list-bearing
entries, the jump half.

STATUS PR-243 (PORT): that authored list is now read and it
gates the raise. The placement parser takes the `N dx,dy ...`
line as part of the entry (place_points) and stores the
points ABSOLUTE on the placement, the way the engine's
0x44e90 does at load; the line is consumed, so it is no
longer classified as a group-header line. ladder_ask_raise
now requires the 0x44ec0 test first: some authored point
inside the asker's live rectangle, else nothing happens (an
empty vector answers FALSE, so 171 of the 186 classes never
raise this signal - PR-241's unconditional raise was too
broad). The asker's rectangle is the field [obj+0x75..0x84]
(+0x4c = 0x23a20 in every kind table), refreshed by the
kind-table +0x94 verb (0x53db0 at 0x97d70/0x969f4) through
+0x98 (0x23a50 -> 0x534e0): position from the +0x68 getter
(the lion's 0x23900 = `add eax,0xd6`; the rope's 0x23b60 is
a bare `ret`, returning the object), extents from the state
record's +0x48 (0x23aa0) and +0x50 (0x53e40) verbs. The port
stands in its own frame SEQ shape box for those extents
(same as its balloon hit test); counted model choices: art
based, drawn mirror not applied. TEST FIRST: leg L8 - no
points, a walking lion's box, a 1 px miss, an exact inclusive
edge touch, a covering box - fails on the ungated build
("no authored points: the empty vector answers false") and
passes with the gate; place_points_selftest pins the grammar
(`1 33,-3`, `1 0,200`, `2 -4,0 4,314` parse; `30,50`,
`23,30;53,160`, trailing junk and a zero count are refused).
Measured on the shipped data: the port's ZARIA.01 run prints
the DRABINKA's point at (213,507) = 180+33, 510-3 and the
two LINA points at (125,492) and (525,525); 3 list lines per
placement pass. No play-probe or fixture moves (65 ok,
fixture-free 13 + 22 named): the ladder's only point is 3 px
above its placement, so a lion walking the ground never
covers it. NEXT: use the authored `x0,y0;x1,y1` rect as the
collision box (this ladder spans x 203..233, y 540..670 down
to the ground), and the other 18 list-bearing entries.

STATUS PR-242 (READ): the K_GEN1 gate is authored data the
port never reads. An object's placement record CONTINUES past
its five fields: ZARIA.01's ladder carries `1 33,-3` (one
contact point, relative to the placement), `23,30;53,160` (a
rectangle) and `30,50` - measured verbatim in
hd_files/ETAPY after 0x2fed. Census of that file: 2076 entry
lines, 21 followed by those lines (11 points+rect+pair, 10
rect only), SIX classes only: DRABINKA 4, LINA 7, KLINA 3,
XCHG 3, PILAGEN 2, GUARD 2. The kind-table +0x18 verb is the
reader (the placed rope's = 0x3f0f0, 2 of 186 tables; the
common values are 0x23c60/0x40eb0): 0x44f10 parses the COUNT
into [obj+0xa9] and allocates 12-byte elements at
[obj+0xa5] - the header is itself an object whose method
table is 0x97f50, the block PR-239 read as a value, which
also holds 0x44ec0 and 0x44f10 - and 0x44e90 then ADDS the
placement to every point, so the authored numbers are
relative and become absolute once, at build time. The
element class 0x98274 / descriptor 0x98278 has its +0x8 verb
at 0x23e00 = POINT-IN-RECTANGLE, so the 0x44ec0 ANY-test =
"is any of my authored points inside the asker's rectangle",
and that rectangle is the plain field [obj+0x75..0x84]
(+0x4c = 0x23a20 `add eax,0x75` in all 186 kind tables). The
gate PR-240 called a parent is a CLASS gate: +0x3c = 0x23c10
(`xor eax,eax`) in 171 tables, 0x230a0 (`mov eax,1`) in 15 -
and the ZARIA lion is one of the 15 (the factory branch
0x1fef4/0x1ff15 for TLewZaria allocates 0x12e bytes, calls
the ctor 0x1e0a0 and writes [obj+8] = 0x96960). Authored
chain, complete: the rope at A_UP with one point at (213,507)
asked by the lion raises K_GEN1 on itself iff the lion's box
covers that point - 3 px ABOVE the placement, so never a lion
walking the ground at y about 680: that is the
jump-onto-a-ladder half of OI-078. The second line becomes an
absolute RECT at [obj+0xb1..]: this ladder's x 203..233, y
540..670, down to the authored ground line - the port
collides with the art bbox 42x48 at (180,510) instead, and
its placement loop counts those three lines as poly/shortline
noise (ll_level.c 8135-8141). NEXT: who writes [obj+0x75];
parse the point list and the authored rect in the port.

STATUS PR-241 (PORT): that raise is now in the port, test
first. New selftest leg L7 in the ladder answer selftest: the
authored `[A_UP]` pair (`A_DROP(1)=K_GEN1` and `1,0,WAIT
K_GEN1`), a ladder-class object at the ask pixel at its FIRST
declared state - it answers NOTHING (0x3f20f -> 0x3f364), it
raises K_GEN1 ON ITSELF (0x3f203-0x3f20c), and the following
record steps take the A_DROP block; the same script unasked
never moves nor arms a bit. The leg fails on the stubbed
helper (selftest exit 1) and passes with the raise (exit 0).
Two helpers: ladder_flag_raise (the branch test, decl_idx 1
only) and ladder_ask_raise (arm, test that object's own lines
at this beat, clear - the port's existing shape for object-
side raises), called from the two PR-150 ladder ask sites.
Counted model choices: no [obj+0xa5] vector (the pair contact
stands for the 0x44ec0 ANY-test), no parent gate, and the
engine's one-record-step delay applied at the ask beat. The
pinned ZARIA.01 walk is UNCHANGED and prints no K_GEN1 line:
the lion never reaches (180,510) - the authored ground he
follows is at y=680 and ends at x=408 - so the climb still
needs the lion-side half (OI-072/081/082/083) and the read of
who writes [obj+0xa5] / whose ask channel reaches a placed
DRABINKA. GATE: make 0 warnings; verify-all 65 (ok);
fixture-free 13 (ok) + 22 named SKIP.

STATUS PR-240 (READ): OI-078's headline is ANSWERED. The
ladder raises K_GEN1 on ITSELF while answering an ask: 0x3f1c0
at the asked's first declared state ([asked+0x5c] == 1,
A_UP) gates on the requester having a PARENT ([[kind]+0x3c]
= 0x23c10) and 0x44ec0 - an ANY-test walking the 12-byte-
element vector [[asked+0xa5]] (count [asked+0xa9]) calling
each element's class verb +0x8 with the requester's
[[kind]+0x4c] object (0x23a20) - then 0x3f203-0x3f20c does
`[asked+0x66] |= 0x40` = bit 54 = K_GEN1 on the asked's own
live set (the store the port counts as "[self+0x66] bit 6")
and answers nothing. Next step 0x53725 composes it into
[obj+0x60] and the authored `A_UP` `A_DROP(1)=K_GEN1` line
matches: A_UP -> A_DROP -> A_DOWN is reachable with no
external sender. The same store sits in the KAMIEN answer
0x3d440 at 0x3d4e5 (also K_GEN1, not a "child-ANY bit").
Ruled out: record-class +0x30 = 0x2b4d0 (bare `ret`), the
rope's +0xc8 = 0x54560 (a message handler over
[obj+0x6d]/[0x71]/[0xc2]/[0xc6]/[0xb6], never [obj+0x60]).
Base message handler 0x52bb0 (the receiver's fallback at
0x537dd): type 1 = position deltas, 0xe = `[obj+0x30] =
[msg+8]` (the parent; 0x52bef is an ARM, not a +0xc8 slot -
corrects PR-236), 0x21 = `[obj+0x28] = 1` (PR-225's
frame-step flag has a second feed). NEXT: port the
self-raise (test first) and read the ask that triggers it.

STATUS PR-239 (READ): the per-step key composition, end to
end. 0x523f0 (called at 0x5371c) = record-class +0x8 0x52750
+ row step 0x521f0 + the owner kind-table +0xa8/+0xac + a
getter; for the placed rope those owner verbs are bare
`ret`s (nothing). 0x52750 builds a context struct from the
owner's state fields ([obj+0x50]/0x54/0x58/0x5c, +0x48/0x4c)
and only its LAST verb writes the out: +0x34 = 0x52790, the
ASK CHANNEL - 0x4bd30/0x4b890/0x4b870 gated on the record's
`control=` bits ([rec+0x12] 0x40/0x80, [rec+0x13] bit 2;
the parse arms 0x51b6f-0x51d2b). ZARIA's rope = BY_TIME, so
its asks are OFF. The row step splits on the row's TEST
answer: non-empty (a Wait clause, 0x45972) -> execute the
chain (a Wait node's execute 0x45870 answers EMPTY - so the
row key NEVER feeds the transition key: refuted) and gate
the advance on [row+0x10] < [obj+0xa1] with the difference
as budget; empty -> match the empty clause (always true) and
advance with a zero budget; both end in 0x520c0 (its budget
spending stays open, OI-111 tail). GEN confirmed as the
port models it (0x45920 ORs [node+0x8]). K_GEN1 census: no
bit-54 write, no signal message with payload 54 (builders
0x287a0 AC_DRAB-to-parent, 0x2c4a0 0x21/0x1c/0x1b, 0x3e980
K_GEN4) though 0x53770 would OR a payload straight into
[obj+0x60]. Next named: 0x2b4d0, 0x54560, the control-name
bit map, 0x4b890/0x4b870.

STATUS PR-238 (READ): class identity from the engine's own
name factory 0x1f870 (strcmp chain on the class-name block
0x906fa-0x9078b: SEP2, DRABINKA, RYBA, ..., ZTABL,
TLewZaria). The placed DRABINKA's kind word is 0x97cdc -
the factory OVERWRITES the ctor default 0xa2498 (0x1f947) -
so PR-150/216's answer verb 0x3f1c0 is the placed rope's
own, and its +0x8c provider slot is the NULL 0x23a60: the
PR-236 attach-merge cannot be the rope's K_GEN1 feed
(ruled out by measurement). 0x96a88 = KAMIEN, 0x9d8a8 =
KDRABINA (kind-word overrides 0x1fb16/0x34e62); 0x9a44c's
name stays open. 0x1eba0 = the 0x96874 family's ask-all
method (vtable slot 0x97954 = +0x10e0): a union containing
K_GEN1 sets [self+0x28], PR-225's frame-step gate. The
+0x78 verb's edx block is the answer OUT buffer. 0x53ae0 =
the block-response gate: {AC_BLOK}/{AC_BLOK_Y} answers -
the port's blocking-signal raiser is named at last.
Next reads for the rope's WAIT K_GEN1: 0x536f0's ORs into
the rope row, and which class answers WITH K_GEN1.

STATUS PR-237 (READ): the ASK verb is kind-table slot
+0x78. Class-table anchor: slot +0x6c holds the record
accessor 0x23a10 - exactly 186 tables. +0x78 census:
0x552e0 x78 (base: 0x53ae0 gate, verb +0x9c, 0x55230),
0x3df00 x23, 0x50f80 x15, 0x53c90 x14 incl. DRABINKA
0xa2510, 0x3d990 x5, 0x3d440 x2 (0x96a88/0x99e64, PR-214
CONFIRMED), 0x3f1c0 x1 - PR-150/216's ladder answer is
this verb, owner class 0x97cdc; LION answers with 0x27f30.
Ask loops: 0x553e0 (array [obj+0x40]/[obj+0x3c]) and
0x4b500 (global [obj+0x34] list, class-id filter
[obj+0x24]); answers are OR-ed TEventSets. 0x3d440 whole:
asked-state-1 gate, requester parent [edi+0x30] gate,
asked verbs +0xdc/+0xf0/+0xd8 (bit [asked+0x66] 0x40;
[player+0xee] equality; the {AC_STONE} answer), box test
vs 0x98274 via 0x4a050/0x4a080, requester K_FIRE gate,
then the two attach entries + [asked+0x106] facing.
0x28590 = class 0x9a44c's +0x78 override (corrects
PR-214's 0x9a390+0x134). Next named read: 0x53c90, the
DRABINKA's own answer verb.

STATUS PR-236 (READ): the K_GEN1 feed machinery is mapped.
Refresh 0x536ff calls the +0x8c provider slot; the rope
tables carry the NULL 0x23a60, other classes 0x549f0,
which evaluates the [obj+0xd2] attachment-entry queue
(kind 1/2/4; entries 0x1d bytes: next/kind/param/payload/
12B-key/Y). kind 2 ORs the ENTRY's own key (copied at
enqueue by 0x53f50) into [obj+0x60] when entry+8 is 0 or
the object's state ID, then pops (0x551f0 = head pop,
PR-227's "related object" phrasing corrected). Enqueuer
0x3d440 requires player state 1, [obj+0x30] parent (set
by type-0xe messages at 0x52bef) and the receiver K_FIRE
bit, which provider 0x54910 synthesizes from [obj+0xa5]
bit 1 (also K_ALT/K_JUMP/K_BKWD/K_FWD). Rope tables'
+0x10 = 0x53770: signal bits reach the rope like the
base handler. PR-225(c)'s names for raised indexes
33/28/27 corrected to AC_KOREK/AC_BECZKA/AC_BUTLA. Open:
who runs 0x28590 (0x9a390+0x134), the type-0xe sender,
[obj+0xa5] bit 1, any K_GEN1-synthesizing provider.

STATUS PR-235 (PORT): the AC_DRAB contact raise (0x287a0) is
in the model. The state RECORD ID word = the authored block
position: the block parser zeroes ds:0xceff8 (0x5150b) and
every record create receives it (0x51a2a); it sits at record
+0x4, is copied to object +0x8d with the record (0x5386c) and
read back by the +0x6c accessor 0x23a10. ll_mov_st gained
block_pos; acdrab_law + wiring at both ladder-contact sites
raise LL_SIG_AC_DRAB on the lion when its current-state ID is
6 or 0x12 and the other's ID is 0..0x19 - census-confirmed:
LEW.MOV block order puts A_DEAD at 6, A_DRAP at 18, exactly
the states whose rows consume AC_DRAB. Test first: L6 failed
at HEAD (block_pos nst 2 got 0/0), passes now; no existing leg
moves (no probe reaches state 6/18); gate: 0 warnings, 65 ok
rc=0, 13 ok + 23 SKIP. Open: the [[other]+0x3c] probe and
0x28820 clause, K_GEN1 delivery to the drabinka, the
AC_DRAB_CATCH raiser.

STATUS PR-234 (READ): the raiser census, closed and re-opened
in one place. (a) The engine name block 0x91b40-0x91c68 read
raw matches the port's LL_SIG_* order 68/68 - PR-225's
"shipped words map to other indexes" alternative is ruled out;
bit 54 = K_GEN1 stands. (b) FOUND the ladder's signal on the
LION: 0x287a0 (its only call site = the collision loop
0x3eac6/[obj+0x30] neighbour walk) raises AC_DRAB (10) on the
lion by a synthetic type-0x10 tag-0x8138764 message, gated on
[[other+8]+0x3c] plus the current STATE record's authored +0x4
word == 6 or 0x12 - the `A_TURN_BACK(1)=K_UP,AC_DRAB` entry's
missing raiser, PR-147's gap now named code. (c) K_NEVER is
not bit 57 (0x47d60 parser special) - the port's park model is
consistent, no fix needed. (d) Raised indexes anywhere: 33,28,
27,21,30,10,57 - nothing raises 54; with the drabinka's null
provider (PR-233) its own A_DROP(1)=K_GEN1 waits on either
the GEN-tail admission effect (unread: 0x45920 delegates to
the Wait execute 0x45870 and copies the node set) or the
PR-227 attach-merge at [rel+0x10]. PR-235: port the AC_DRAB
contact raise (state-ID gate to pin against LEW.ZAR). Light
gate (docs-only).

STATUS PR-233 (READ): OI-078's last named question ANSWERED,
NEGATIVE. ZARIA.01's DRABINKA is built by the generic factory
chain (0x4df10 <- level loader 0x4aac3; the class token is
authored level text, 'Nie znaleziono obiektu na liscie!'):
'DRABINKA' matches at 0x1f8ac, alloc 0xd9, ctor 0x531f0 whose
tail 0x532a6 stores kind [obj+8]=0xa2498 (+0xc=0xa2490) - low
byte 0x98 = bit 0x10 only. KDRABINA (0x34e39, factory 0x34c20)
overrides to 0x9d8a8 - no bit 0x20 either. The drabinka kind
table's provider slot +0x8c is the NULL provider 0x23a60 (empty
TEventSet; the real provider 0x54910 sits at +0x8c of 70 other
tables incl. 0x9a778/0x96874). The 0x9a754/0x9a778 stores are
MID-CHAIN ctor stores in the KADMAR factory (0x29a51 then
0x29a69 -> 0x99828 at 0x2a18d...) - PR-229's tie-out proposal
is corrected: the overlap-kind law cannot feed the ladder's
`WAIT K_GEN1`; the signal must arrive via [drab+0x60], and
PR-226's "who sets bit 0x20 of [obj+0xa5]" stands as the next
named census (PR-234). Light gate per PR-232 (docs-only).

STATUS PR-232 (GATE): the gate now FOLLOWS THE RECORD KIND
(owner ruling 2026-11-20). A read-/docs-kind PR whose diff is
.md-only runs ONLY `make verify-index verify-docs` and records
the single line `GATE: light: verify-index verify-docs
rc=0 (ok);`; port/gate PRs (or any diff touching code,
Makefile, tools/, .github/, .gitignore) keep the full two-line
gate. doc_census P4 enforces it from the record's own KIND
line: light accepted only under read|docs, full pair still
accepted there (history), port|gate demand full. Test first:
probe battery C1..C8 against HEAD failed C1 (read+light:
missing GATE,GATE-FREE); after the change C1/C5/C6/C7 pass and
C2/C3/C4/C8 fail as designed. Rule 4 also drops WAITING for CI
after push. Gate: 0 warnings, fixtures 65 ok rc=0,
fixture-free 13 ok + 23 measured SKIP lines.

STATUS PR-231 (PORT): CHECK_ALL now implies the CHECK_LN rule
bit, exactly as the engine's keyword 0x921c6 maps to 0x1c0
(PR-230). Test first: a --play-selftest CHECK on
`control=BY_TIME|CHECK_ALL` failing at line 3037, then
ctrl_flag's CHECK_ALL entry sets CHECK_LN too; ll_play.h's
PR-60 exemption comment replaced. Consequences on ZARIA.01
(all authored-data, no video number moved): the lion's
A_WALK follows ZARIA_A.PLT's first polyline -50,680!408,684
(grounded y 648->652 across the first screen), stands grounded
to tick 189 AT the polyline end x=408, walks off the authored
pit edge at tick 190, crosses the level bounds death line
('below lew_rect at (480,1007) -> respawn') and respawns on
tick 218; jump apexes shift +1 from the sloped take-off. The
traverse leg's x=685-at-tick-300 pins are re-pinned to that
story (they described hovering over the pit - a port artifact,
not authored walkability); a PR-231 RE-PIN NOTE at the top of
verify-play scopes the +1..+4 y shifts; RAN.01 legs unchanged
(RAN spells CHECK_LN explicitly). Gate: 0 warnings, fixtures
65 ok rc=0, fixture-free 13 ok + 23 measured SKIP lines.
OI-113 half closed: keyword entries for CHECK_BASE_LN/INV_V/
BY_TX/SEQ_REV and the row-order word/apply verbs stay open.

STATUS PR-230 (READ): the complete control keyword-to-bit table
and two corrections. The control= parser 0x51b0d-0x51dd8 is the
authoritative map (all names above 0x9216e-0x9222f): BY_X=0
default, BY_Y 0x1, BY_TIME 0x2, BY_TX 0x3, BY_TY 0x4,
FRAME_REV 0x8, SEQ_REV_X 0x10, SEQ_REV_Y 0x20, CHECK_ANI 0x40,
CHECK_ZW 0x80, CHECK_LN 0x100, CHECK_ALL 0x1c0 (IMPLIES
CHECK_LN), CHECK_LEW 0x200, SOLID 0x400, CH_DIR 0x800,
INV_V 0x1000, DANGER 0x2000, CHECK_ALL_PLANE 0x8000,
CHECK_BASE_LN 0x4000, ANTI_SOLID 0x10000, SOLID2=0x400|0x20000,
SEMI_SOLID=0x400|0x40000, ANTI_POCISK 0x80000; unknown names
format 'Bledny control %s w pliku %s.' behind a null stream
gate. The port's ctrl_flag lacks CHECK_BASE_LN/INV_V/BY_TX/
SEQ_REV bits and its CHECK_ALL omits the CHECK_LN rule bit -
ZARIA.01 A_WALK is BY_X|FRAME_REV|CHECK_ALL|CH_DIR (OI-113).
[obj+0x99] = ROW-ORDER +/-1: reset 1 next to [obj+0xa1] at
0x5236b-0x52377, neg by rev's apply verb 0x459b0, cursor walk
0x52150-0x521da decrements [rec+4] when it says backward.
The row-directive keyword->table map 0x45e00-0x46750: rev/
zero_v/zero/zero_t/smpauto/smp_unloop/smp_stop/chdir/ldir/
rdir -> 0xa0f28/0xa0fa0/0xa0f88/0xa0f88/0xa0fe8/0xa0fb8/
0xa0fd0/0xa0f40/0xa0f58/0xa0f70; slot +0x04 is APPLY (not
clone): ldir -1 / rdir +1 into [obj+0x6d] (the +/-1 DIRECTION
word, not a speed - corrects PR-226), chdir calls the
CH_DIR-gated flip 0x53670 (bit 0x800 = CH_DIR, not mirror),
rev negs [obj+0x99]. Authored: rdir 7 lines incl. ZARIA.01
A_DEAD row 181, rev 21 lines incl. A_LOOKUP row 629. No
behaviour changed.

STATUS PR-229 (READ): the AC_RECT chain end to end, and the bit
law that answers OI-078. Five sites enqueue type 0xf =
LL_SIG_AC_RECT right after the rectangle-intersect test
(0x28654/0x28b9b/0x3740c/0x374c5/0x3dc7b), tag = THE OTHER
OBJECT'S POINTER (mov ebx, obj; edx=0xf; ecx=2/3; call 0x52b10).
The +0xfc arm 0x50dc7 passes edx=[[msg+8]] - that pointer IS the
block's argument - and the raw bytes of 0x549f0 store
[other+0]->[obj+0xba], [other+4]->[obj+0xbe], and at
0x54be6-0x54be9 [other+8] -> [obj+0xa5], the byte the provider
(PR-226) tests. [other+8] is the KIND METHOD TABLE pointer -
0x9a754 holds the TEventSet/interval verbs 0x45120/0x45250/...
(that is the "class id" PR-221 compared against ds:0xceba4 =
0x96fd8). Measured low bytes: 0x97970/0x978dc/0x96874 -> no
bits; 0x96fd8 -> 0x18 (41 only); the 0x9a74c/0x9a754/0x9a778
family -> 0x54 = bits 0x10+0x20 -> provider raises 41 AND
K_GEN1. RULE (FOCUS 1): overlapping an object of a 0x9a754-
family kind makes the provider raise K_GEN1 every frame - the
drabinka's WAIT K_GEN1 needs no GEN row and no signal message.
Left: which authored kind the ZARIA.01 drabinka carries, then
implement the overlap rule. Kind tables are stored into class
data at 0x298d7-0x298e8/0x2a18d/0x2b312 (kind at class+0x178,
0x9a74c at +0xE6, method table 0x9a778 at +0x94). No behaviour
changed.

STATUS PR-228 (READ): the +0xfc family and the entry of PR-227's
"unreferenced" block. It starts at 0x549f0 (push ebx/ecx/esi;
the index boundary hid the first 5 bytes) and is vtable slot
+0xfc of 15 class tables via 0x50340 (zeroes the caller's
triple if [obj+0x106]==0, negates it if [obj+0x11a]==5, calls
the block). Two switches call [reg+0xfc]: 0x41ae2 (switch on
byte [obj+0x160]) whose arms for +0xf4/+0xf8/+0xfc OR the
RETURNED TEventSet into [obj+0x60] (0x49580) - signal
providers like +0x8c - and 0x50d70, a message switch over types
9..0x15 (table 0x50d20 misaligned; best read: type 0xf -> the
+0xfc arm with pair (msg+8,msg+0xc); type 0xa -> vtable+0xd8),
matching the key-send second checks 0xa/0x1 at 0x2c611/0x2c651.
Inside the block [obj+0xa5]=[[second]] = the CLASS ID of the
carried object -> K_GEN1 = "attached to a class whose vtable
low byte has bit 0x20" (true for 0x9a778-style tables), re-
tested every frame by the provider - OI-078's mechanism, with
"which sender delivers the drabinka's attachment" left open.
Sibling 0x54c00: sound-effect law - signed byte [obj+0xb5] ->
name via vtable+0x50 -> [[0xcef40]+0x390] + 0x57530 (PR-222's
settings lookup); gate [obj+0xee]<3 (0x1f100) - PR-225's
field-fed sender objects are this player class. No behaviour
changed.

STATUS PR-227 (READ): both sides of the provider, and a SECOND
row gate. TEventSet::match 0x47e40 has exactly two callers: the
PR-217 row-step clause gate (0x522e6) and 0x52033 inside 0x52000
- a FIRST-PITCH row scan reached only from the refresh: 0x536f0
(0x5373c) -> 0x53520, which copies the object's OWN signal set
[obj+0x60]+mode (the copy PR-226(e) misread as [state+0x60] -
corrected) and calls 0x52000 directly with the CURRENT STATE
S=[[obj+0x50]+[obj+0x5c]*4]: walk S's row table [S+0x1a] (count
[S+0x22], stride 0x15), test entry+0's clause set against the
copy, FIRST match wins (0x5203c-0x5204f: copy entry+0xd/+0x11
to the caller's two out pointers and break), then call the state
class's verb [[S+0x44]+0x04]; the refresh feeds the pair to
0x535a0. The provider's field writer is also found: the block
0x549f2-0x54bf2 (entry unreferenced statically - the one thing
left for OI-078) stores [obj+0xa5] = [[arg+0x8]] = the CLASS ID
of the related object from the argument's third slot - K_GEN1
means "attached to a class whose vtable word has bit 0x20" -
and ORs the related object's [[...+0x10]+0x60] record set into
[obj+0x60] (0x54b78-0x54b84). Corrects PR-226's (c)+(e). The
port models ONE row gate; the engine has two in series (pitch-
scan before the step) - register for FOCUS 1. No behaviour
changed.

STATUS PR-226 (READ): the real signal raiser is the object
itself. 102 class vtables implement slot +0x8c as 0x54910 - the
frame refresh 0x536f0 calls it on every object every frame and
merges the result into [obj+0x60] (this corrects PR-225's (e):
0x536f0 is the CALLER of +0x8c, not its base). The provider's
law, machine code: [obj+0xa5] bit 0x1 raises 33=K_FWD, bit 0x10
raises 41, BIT 0x20 RAISES 54=K_GEN1; when [obj+0xba]!=0 the
[obj+0xb6] bit 0x10 (maintained as the sign of the speed word
[obj+0x6d], 0x54bb0-0x54bd4) picks 36=K_FWD vs 35=K_BKWD;
[obj+0xbe]<0 raises 29, >0 raises 30. The drabinka's WAIT
K_GEN1 therefore needs no message and no GEN row - only bit
0x20 of the object dword +0xa5; who sets THAT bit is OI-078's
one remaining question (ctor zeroes it 0x2b6a0, clone copies it
0x2b840, other code indexes [obj+0xa9] with it). Also: state
flag word +0x12 bit 0x800 = state MIRROR - entering such a
state does `neg DWORD PTR [obj+0x6d]` and calls vtable+0x54
(0x53670), and the row word `rev` (node [node+0x0] = the row's
object) calls the SAME flip - the execute PR-224 found for the
`rev` class. And 0x53520 proves the state record carries a
12-byte row-chain TEventSet at +0x60 (+mode byte at +0x6c),
handed to vtable+0x90 with the state class's own chain
(vtable+0x20) - the same structure PR-225 saw merged at the
collision (0x3ee04). Two portable FOCUS 1 rules identified:
mirror-state entry negates the speed word; `rev` negates it.
No behaviour changed.

STATUS PR-225 (READ): the message system, end to end, and the
closed set of signal raisers. Enqueuer 0x52b10 (receiver in eax,
TYPE in edx, TAG in ebx, payload in ecx) appends a 0x10-byte
node {link, type, tag, payload} to the queue at [obj+0x10];
pump 0x52b50 calls the receiver's vtable+0x10 per message;
0x52b80 = vtable+0x14, then the pump, then vtable+0x00 when
[obj+0x28] is set. Base handler 0x52bb0 dispatches on the TAG at
[msg+4]: tag 1 = position delta from [msg+8]/[msg+0xc], tag 0xe
= store into the object named by [msg+0xc], tag 0x21 = set
[obj+0x28] - the tag numbers ARE this port's signal numbers
(0x21 = LL_SIG_K_FWD). The signal family: type 0x10 + tag
0x8138764, payload = signal index; the magic occurs at EXACTLY
six producer sites (0x2882d, 0x2c5e1, 0x2c613, 0x2c653, 0x2c99a,
0x3eacf) plus the check at 0x53780, so the raiser set is CLOSED:
0x2c5cd/0x2c611/0x2c651 broadcast 33/28/27 = K_FWD/K_UP/K_DOWN
(confirming PR-88's ordinals from the raising side) through the
broadcast helper 0x4bc40 (scene, type in edx, tag in ebx,
selector in ecx - 0 = every object, else match vtable+0xc =
class id - and the SIGNAL INDEX as the pushed extra), 0x2c99f
raises 21 on [scene+0x394], 0x3a1dc raises 30 on self, 0x2882d-
0x2885a calls vtable+0x10 on a stack-built signal message
(index 10), and the collision loop 0x3eac6-0x3eaf4 raises index
57 on every object of the neighbour list. Field-fed sites like
0x1f454/0x34a7f put a SUBSYSTEM constant in the TYPE slot with
tag 0 - they never raise (corrects PR-224's (e)). Non-message
writers: a collision ORs 4 plus the set at [[obj+0x3c]+0x60]
(THE CURRENT STATE OBJECT CARRIES A TEventSet at +0x60,
0x3edff-0x3ee11), and vtable+0x8c (base 0x536f0) merges a
per-frame set at 0x5370c. No sender passes 53/54/58 =
K_JUMP/K_GEN1/K_NEVER, so OI-078 now asks: the state-carried
set, or the shipped `.MOV` name->index lookup (0x57d30 against
0x961fc/0x96200) mapping the WORD K_GEN1 to a signal the engine
really raises. No behaviour changed.

STATUS PR-224 (READ): how a class table is addressed, and
what each row directive is. A node's class word IS its create
verb and the 0x18-byte table there is create/+0x04 clone/+0x08
test/+0x0c execute/+0x10 chain/+0x14 a sixth verb - verified
against the two tables whose numbering was already pinned from
use (record 0xa260c, row 0xa2638), which makes PR-220's "node
base 0xa1000 ... the `Wait` word 0xa11c0 ... the `GEN` word
0xa11d8" wrong: those two are not class words, the clause words
are 0xa11c8 and 0xa11e0 (PR-217 was right) and their +0x14 is a
FACTORY (0x47c00 clones a clause into the clause-with-operand
class 0xa11b0). Reading the builder 0x45df0-0x46d80 as
(compare target, class store) pairs gives the keyword-to-class
map: `rev` 0xa0f28, `zero_v` 0xa0fa0, `zero`/`zero_t` 0xa0f88,
`smpauto`/`atc_on`/`atc_off` 0xa10f0 (stores 1 at [node+0x8]),
`lock`/`lock2`/`unlock`/`unlock2`/`ground_on`/`ground_off`
0xa10a8 with 0xa10d8/0xa10c0 variants for the `2` forms,
`chdir` 0xa0f40, `ldir` 0xa0f58, `rdir` 0xa0f70, `F` 0xa1090,
`MOVE` 0xa1060, `Wait` 0xa11c8, `GEN` 0xa11e0, and the `%i`
node 0xa1048 = the `end_level`
node of PR-194/PR-199; the middle band (`smp`/`vol`/`pos`/
`goto`/`I`/`M`...) is NOT pair-verified yet. So the
answer-6 verbs are the SIXTH verbs of directive classes -
0xa1048 +0x14 = 0x45d90 (`end_level`: *[ebx] =
[node+0x8] sets the ROW CURSOR to the authored number and the
answer becomes 6, so the row is re-run, not advanced - see
TUNGA.11's `10,6,end_level 1`), 0xa1030 +0x14 = 0x45dc0,
0xa1090 +0x14 = 0x45d50 (`F`,
gated on [node+0x10]) - and clause nodes cannot hold a row:
their chain verb is the base walker and they have NO operand
(the node ctor 0x45780 writes 0 at [node+0]; the clause parser
fills the set inline at [node+0x8]). A clause's test 0x45970
answers with its authored set, its execute is the base delegate
(EMPTY); the `GEN` execute 0x45920 ORs [node+0x8] in via 0x49580
- the raise path, OR-ed into the caller's set at 0x5230c. THE
RAISER, named: handler 0x53772 tests message type 0x10 + tag
0x8138764 and ORs `1 << (idx % 32)` into [obj+0x60 + 4*(idx >>
5)] from the payload byte at [msg+0xc]; the engine's own key
sends 0x21/0x1c/0x1b at 0x2c5cd/0x2c611/0x2c651 are EXACTLY
this port's LL_SIG_K_FWD/K_UP/K_DOWN, confirming PR-88's
ordinals from the raising side. No sender passes 54 = K_GEN1:
all 22 sites of 0x4bc40 use small AC_* indexes or values read
from object fields ([obj+0xee], [obj+0xf2], [obj+0x122]) and the
broadcast loop 0x3eac6 uses 0x39. OI-078 now asks exactly that.
Also: the record factory 0x518f0 (class +0x28) writes the owner
object at [rec+0x0], which the row step reads at
0x52214/0x52311. No behaviour changed.

STATUS PR-223 (DOCS): the register was carrying mangled rows.
PR-220/PR-221 inserted their OI-078 notes through a Python
line-list splice of a STRING, which writes one character per
line: 1293 one-or-two-column lines in 21 runs (566-1898) and,
inside one run, the two OI-078 HEADER LINES - so OI-078 had no
title for three commits while verify-docs, re_index and the
width ratchet stayed green. Repaired from 44e29de plus only the
legitimate additions (now +113 -0 against it, 0 short lines, no
line over 70), and tools/doc_census.py P6 now FAILS on a run of
4+ one-or-two-column register lines: measured red on the
committed register (21 runs) and green on the repaired one.
Rule for the next writer: use text.replace(anchor, note+anchor).
No engine read, no behaviour.

STATUS PR-222 (READ): a row's tail is a keyword language. The
engine's directive table is one string block at 0x91847-0x91990,
scanned with ` %[^,| \r\n\t]` (0x91844) and matched by
0x76a69: rev, zero_v, zero, zero_t, vol_music, smp (` %i`),
smp2/smp3, smpauto, smpauto2, smpprior (` %d`), vol, smp_loop,
smp_unloop, smp_stop, atc_on, atc_off, lock, lock2, unlock,
unlock2, ground_on, ground_off, chdir, ldir, rdir, pos
(` %i , %i`), goto (`%d`), gotorel, goto_rel, I (` %i,%i`), F,
IC, FC, M (` %i,%i`), MOVE, GEN, Wait. Arguments are read by a
scan with THAT format into the node (0x46beb-0x46bf9: an
0xc-byte node, class 0xa1048, `%d` at 0x91998); an unmatched
word ENDS the tail - 0x46c1c reports it (0x496b0, format
0x9199d, the global 0xa92d4) and returns 0, so the chain build
stops at 0x516e8. Counted on the shipped texts: field 3 is
`smpauto` 370, `GEN K_GEN1` 288, `ATC_OFF` 242, `ATC_ON` 235,
`GEN AC_PLAT_OFF` 181, `M`+0 172, `WAIT K_NEVER` 157, `F`+0 99,
lock/unlock 92/66, `GEN AC_TIME` 60, `F` -300/-330/-250
36/36/36, zero 30, `WAIT K_GEN1` 25 - and ZERO row lines carry
a bare number there, which is the negative datum OI-112 needed:
nothing in the engine could read a row's second authored
number. Clause nodes hold only the set: 0x46c70 (`GEN`,
0xa11e0) and 0x46cf0 (`Wait`, 0xa11c8) allocate 0x8+0xc and
parse through 0x47d60 - so PR-217/218's "same ctor as GEN" is
only true of the clause parser, not the create. The answer-6
holders are the sibling sub-tables of the node base: 0xa1038 +
0x10 = 0x45dc0 (*[ebx] += [node+0x8], answer 6) and 0xa1040 +
0x10 = 0x45d90 (*[ebx] = [node+0x8], answer 6) - jumps and
deltas on the row cursor EBX = &rec+0x4 - and 0xa1098 + 0x10 =
0x45d50, gated on the node's own trigger byte [node+0x10],
applying [node+0x8]/[node+0xc]. So a row is re-run exactly
while one of its OWN directive nodes answers 6. Unread and
first in line: 0x45df0's first step is NOT a token scan but the
settings lookup 0x57d30 (ECX = 0x91844, EDX = 1, EBX = the
text) and 0x45e20 treats an empty result as end-of-text - how a
row tail reaches the keyword match is still unnamed. The named
lead: 0x49920/0x498f0 are BARE scans, while 0x49950/0x499b0
first strip separators (`%c` 0x91cda then 0x91ce1/0x91cf1,
0x4996d-0x49980) and only then apply the format - a leading
` %[^...]` only behaves as "skip the separator, take the token"
through THAT kind of helper. Two more rows read cleanly: the
unknown-word report is the engine's own diagnostic string at
0x9199d (`Bledna komenda %s w`), and the record's row-reset
verb 0x52360 (+0x1c) rolls the `time=` default through 0x52070
and PLACES THE ROW CURSOR at it, [rec+0x4] = k or count-1-k by
the state's 0x10/0x20 bits (0x523bc-0x523cb) - the `time=`
number is a row index on state entry, not a duration. No
behaviour changed.

STATUS PR-221 (READ): how much of a row line the engine
actually reads. The row-line parser 0x51660 pushes exactly
three arguments to the scan helper (push [row+0x10], push
0x92115 `%hi`, push the line; one `add esp,0xc`), so the ctor
parses ONE number from a row line - the first - and scales it
into [row+0x10]. What PR-220 read as "the second value" is not
one: 0x51692 tests the scan's own result and 0x5169d-0x516d2
store the constants +2 / -2 / -1 with a flag, while [row+0x8]
takes the CALLER's value in EBX - ds:0xceff4, the sequence's
default registered on `seq=` (0x51f48), or a settings-block
name lookup (0x51a76-0x51a92) at the other row-ctor site
(0x51ac1). Counted on the shipped texts: all 18617 row lines
have exactly two numbers, none has one, the first column never
exceeds |82| and walks up and down (ENA.03_ROZGW `-5,4` ...
`-8,4` ... `-6,4`) - so the port's `%d,%d` parse
(ported/ll_play.c:521) is its own grammar, not the engine's
read of those fields. Second number: no named engine consumer
(OI-112). The row's LENGTH has a separate mechanism that does
not need one: the row-run verb zeroes a local answer (0x52120),
calls the row's chain verb with mode 6 (0x52138-0x5213d) and at
0x52140 `cmp [esp+0xc], 6; je 0x521da` SKIPS the whole
list-walk when the answer is 6 - the cursor [rec+0x4] stays put
and the row runs again; the PR-135 walk (OR bit 8 / bit 4 into
the caller's set byte, 0x521a1/0x521c1) is the else branch.
That puts PR-218's "the clause selects the path, it never
refuses the row" back on the table, together with the row CLONE
made at 0x520e1 as the other place per-row state could live.
No behaviour changed; the port's .thresh model is unpinned by
the engine, not refuted.

STATUS PR-220 (READ): who builds a row and what scales it. The
state record is 0x48 bytes with class word 0xa260c; its ctor
0x517e0 allocates one and calls the row-list builder 0x51eb0
with eax = the record (0x517b9-0x517c9). That builder dispatches
each block line, and the settings branch (0x5264f -> 0x51910)
writes `time=` into [rec+0x8]/[rec+0xa] (the `%hi` / `,%hi`
scans at 0x5194e/0x51960 - PR-163's bounds pair, now with its
store sites), `sample` at [rec+0x16], `time_scale` at
[rec+0xe] (0x519c3), and on `seq=` registers the sequence name
and stores its looked-up default in the global 0xceff4
(0x51a35). The row path 0x51f37-0x51f54 then passes the record
field itself to the row ctor: `mov edx,[rec+0xe]`, `push edx`
as the scale factor, the duplicated line text as edx and the
sequence default as ebx. So time_scale multiplies the FIRST
authored number (`[row+0x10] = first * [rec+0xe] / 1000`),
never the second, and the row step's gate is a compare against
the monotonic object clock, run with the overshoot as the
budget and with no decrement (0x52320-0x52338) - not the port's
`clock >= thresh; clock -= thresh`. That CORRECTS PR-110's
three-part model (scale_at position rule, second-number
scaling, subtract gate). The `control=` parser 0x51b0d names
the bits 0x543a0 masks with 7: BY_X=1, BY_Y=2, BY_TIME=4,
BY_TX=8, BY_TY=0x10, FRAME_REV=0x20, SEQ_REV_X=0x40,
SEQ_REV_Y=0x80 (more at [rec+0x13]) - the port's LL_CTRL_*
values are its own vocabulary, not these. KADMAR.07's
`[A_DEAD]` (`control=BY_TIME|FRAME_REV`, `time_scale=500`,
rows `0,100`, `1,1000`, `2,400`) is the divergence: engine
gate 0,0,1,2..., port gate 50,500,200. OI-112 stays open on
the second number: it lands at [row+0xc] with a sign flag at
[row+0x14] and a sequence-default substitution at [row+0x8],
and PR-220 found no reader of those fields anywhere on the step
path - the class tables (node base 0xa1000, row class 0xa2638,
PR-221: those two fields are not the second number at all), the
record class 0xa260c, dumped here) hold no verb that reads
them, and PR-218's "0xa11c8 = 0xa11c0+8" description of the
clause tables is corrected: the class words are the bases and
their +0x10 slot is the test verb. One bonus answer: 0x52072
is the `time=` bounds consumer PR-163 left open - max != 0
stores min + rand * (max - min) / 0x7fff at [rec+0xc], max = 0
(the shipped `time=220` form) stores min. No behaviour changed.

STATUS PR-219 (READ): the row object and its clock, field by
field. The row ctor entry is 0x515e0 (0x515e2, as PR-217 had
it, is a 2-byte dump artifact): it zeroes +0x8/+0xc, writes
+0x10 = 1 and +0x14 = 0 and the class word 0xa2638, then hands
the text to the line parser 0x51660. That parser scans the
FIRST number of the line with the format at 0x92115 (`%hi`) into
[row+0x10] and then scales it - `imul` by a factor from the
caller's frame, `idiv 0x3e8` - i.e. `* time_scale / 1000`, the
only such site in the image; the SECOND value goes to
[row+0xc] (magnitude) with a sign flag at [row+0x14], and when
it is 0 the engine substitutes a registry value looked up
through 0x44110 by a key at [scene+0x398] and stores that at
[row+0x8]. The clock the row step compares [row+0x10] against
(0x52320) is the OBJECT's [obj+0xa1], not [rec+0xa1]: the row
reset 0x52360 writes 0 there (0x5236b) - PR-217's "fed 100" is
wrong - and its feeder is 0x543a0, which switches on the state's
control bits [rec+0x12] & 7 (table 0x54380) and adds
|[obj+0xca]|, |[obj+0xce]|, or the scene frame delta
[scene+0x388] gated by [obj+0xba]/[obj+0xbe] - the port's
distance-fed vs wall-time-fed split, now named. The row-run verb
is the record class 0xa260c's +0x18 = 0x520c0 (called as
`call [esi+0x18]` with esi = [rec+0x44], the record's own class
word): it clones the row, composes the key with 0x98278, SETS
[obj+0xa1] to the budget handed over, runs the row through its
class's +0x10 with edx = 6, and does the list-end/wrap by OR-s
bit 4/8 into the caller's set byte 0 (0x521a1/0x521c1) -
PR-135's bit, named. OI-112 is opened on the one thing not
settled: the engine scales and gates on the FIRST authored
number while the port scales and gates on the SECOND
(ll_mov_fr.thresh, ported/ll_level.c:1506-1514) - shipped rows
put an index first (`-1,200,I 100,0` in KADMAR.07, ZARIA.01's
`1,0` and `11,0`) and a time second, so either [row+0x10] is a
delay and the real gate is the row-class +0x10 verb this PR did
not read, or the port's mapping is mirrored. No behaviour
changed; the gate numbers are unchanged by construction.

STATUS PR-218 (READ + ONE PARSER FIX): the row clause's MODE
byte and the two paths its test result selects. The clause
parser 0x47d60 clears the mode byte at [set+0xc] (0x47d73) and
0x47db2-0x47dd0 write 1 for `!`, 2 for `*`, 3 for `|`; a comma
writes nothing. TEventSet::match 0x47e40 (its own error string
0x919ca) switches on that byte through 0x47e30: mode 0 (0x47e63)
is true when the AND of the two sets equals the clause byte for
byte - the clause is a SUBSET of the set handed to it; mode 1
(0x47edc) when that AND is EMPTY; mode 2 (0x47f42) when it is
NOT; mode 3 (0x47fb2) when the sets are byte-identical. The row
step 0x521f0 asks the ROW object (class 0xa2638 = the chain
delegate 0x45800), directive nodes are a chain (ctor 0x45780
puts the operand at [node+0x0] under the base class word
0xa1000), and a clause node composes: 0x45970/0x45920 call the
chain getter 0x45870 and OR their own [node+0x8] in. 0x5223c
then tests the answer with 0x495b0 (empty only when all three
dwords are zero) and `je 0x522fc` sends the NON-EMPTY one to the
execute slot, ORs its result into the caller's set (0x5230c) and
gates the row on its authored frame [row+0x10] against the
object's clock [obj+0xa1] (0x52320); only an EMPTY answer builds
key | [obj+0x60] (0x52251-0x522bc) and calls match (0x522e6),
whose 0 answer skips the row. On shipped data the match test is
INERT: 230 clauses are mode 0, 21 mode 1, `*`/`|` appear in zero
rows, and every clause leaves a non-empty set - K_NEVER OR-s
0x60 into data byte 4 (0x4806f-0x4807d), K_ANY OR-s 0x2001f8
into dword 1 (0x48037-0x48048) - so no shipped row reaches match
with a clause. A clause therefore changes the PATH (clock-gated
vs immediate), not the admission: OI-111 rewritten to that one
question. Code: the `Wait` arm of ll_mov_parse never wrote the
terminator where it trimmed the clause, so `WAIT K_NEVER
;,smpauto` was captured as the token `K_NEVER ` and counted as an
unknown name - 2 rows (SADES.06 BUTLA_BL/BK `[A_RUN]` row 10);
fixed and legged, and ll_mov_fr/ll_mov_gate gain `gop`, which
names WHICH no-bit operand a clause holds (K_NEVER 161 / K_ANY 0
/ unknown 0 through the port's parser, pinned by verify-endlevel)
- still no model acts on the clause, so no frame moves. Also
measured for FOCUS 1: `GEN K_GEN1` is authored ONLY in the
lion's own scripts (20 texts, ZARIA.01's LEW.MOV 36 rows), ZERO
in any DRABINKA, and the lion's answer slot 0x96948+0xd4 =
0x544d0 is an empty override, so 0x54482's state-index case is a
no-op for the lion.

STATUS PR-217 (PORT, CAPTURE ONLY): a row's `Wait <signal>` tail
is the engine's own signal clause, not a delay. The directive
chain is 0x45df0; its `GEN` arm 0x46af4-0x46b34 and its `Wait`
arm 0x46b62-0x46ba2 allocate the same 0x15 bytes, call the same
node ctor 0x45770 and parse the operand with the same clause
parser 0x47d60 into the 12-byte TEventSet at [node+0x8]; they
differ only in the class word they write at [node+0x4] - 0xa11e0
for `GEN`, 0xa11c8 for `Wait`. Measured on the shipped set: 255
Wait tails in 133 of the 777 .MOV texts, 254 exactly one token,
22 `!`-negated, operands K_NEVER 159 / K_GEN1 25 / AC_DUP 20 /
AC_TIME 18. The engine's name table gives 68 ordinals and the
port's LL_SIG_* enum matches them name for name (K_GEN1 = bit
54); K_ANY/K_NEVER are non-bit specials (0x4803d, 0x48075). The
row step 0x521f0 tests a clause twice, both times for being
EMPTY (0x52235 -> 0x495b2 -> 0x5224b, then the union with the
caller's key and [obj+0x60] at 0x522e6/0x47e40) before 0x52303
may execute the row and 0x5230c OR into [obj+0x60]; the `Wait`
vtable answers the test slot with 0x45970, which hands back the
node's own set. That reading is unfinished (it would make
ZARIA.01's rope never unroll) and is registered as OI-111, so
THIS PR acts on nothing: ll_mov_fr gains gsig_id/gneg, the
parser fills them through the same parse_cond the transition
lines use, and obj_advance is untouched. Failing test first:
obj_rowgate_selftest measured red at `clause -1 want clause 54`
before the arm went in. The rows leave the "directive the model
does not act on" count and enter a named captured count. 0
warnings; 65 ok with fixtures, 13 ok + 21 named SKIPs without.

PLAN STEP PR-206 (object placement fidelity, the user-visible
thread; register row OI-108): two PRs after this step, neither part
of closing T1. (5a) [READ] measure where the ORIGINAL draws `.MOV`
object art against the authored placement point, using all THREE
ZARIA I references at verified cameras (levels/ZARIA.01.mkv + _b,
the ladder clip, the ep.1/zax37 captures): POCISK fruit, BAL
balloons, the DRABINKA ladder, LINA, KAMIEN. The port draws at the
authored point; at the verified t=22.0/31.25s cameras the video's
fruit sit tens of units off and some clusters the port never
places. (5b) [PORT] the anchor fix, failing pin test first.
DONE since PR-98: PR-99 read the `control=` dispatch and the BY_* mode
enum (its entry address: that record printed 0x543a1, and PR-100
corrects it to 0x543a0 from the aligned prologue and 155 method-table
entries at slot +0xa0; the jump table at 0x54380 stands), closed
PR-98's item 2, and showed the BY_X branch feeding the row distance
accumulator from 0x54250's own `v/100`. PR-100 then found that
dispatcher's caller - the state record's step at 0x521f0, reached as
slot +0xa0 of the owner object's method table - and with it the whole
gate: the current row's threshold field is compared against [obj+0xa1]
once per record step, at most one row is admitted per step, and the
remainder is handed to the row handler at `[[rec+0x44]+0x18]`. PR-100
then went one level deeper: `[rec+0x44]` is a SECOND method table
inside the record object itself (96-byte records, installed by the
constructors, 16 copies with one shared slot map), the row handler is
its slot +0x18 = 0x520c0 which writes the leftover straight back into
[obj+0xa1] at 0x52118, the step is slot +0xc and is called from exactly
one place, 0x5240c inside the key builder 0x523f0 (a 13-byte key made
from the record class, the owner and the frame array), and slot +0x1c
is a row-set method, 0x52360 in four classes, which reverses the row
order from movrec control bits 0x10/0x20 - the same two bits 0x520c0
tests, and the strongest lead yet that SEQ_REV_X / SEQ_REV_Y are
row-order flags rather than motion. Following that chain one step
further, PR-100 also settled the object side of the same struct:
[obj+0x50]/[obj+0x5c] is the object's RECORD ARRAY plus a current
index - PR-77's "frame array" - so each frame carries its own motion
record, and the method that switches it (0x535a0, the third
[obj+0x95] setter) calls the old record's +0x20 with the object's key,
sets [obj+0x95], runs the 0x239c0 hook, calls the new record's +0x1c
row-set method and writes the new index. PR-98's item 6 is closed too
([obj+0x95] means "the row advanced on this record step"), and the row
thresholds are now censused per mode by `tools/re/mov_channels.py`: 94%
of the BY_TIME values are exact multiples of that 100 feed, the BY_X
values are small counts of px.
PR-101 then read the layer under the rows (RE only, no pin): the row
entries are nodes with a 6-slot class table at `[node+4]` (31 of them,
`python3 tools/re/node_classes.py`), slot +0x10 of that table is a
query-by-id that every node on the chain answers by adding its own pair
- which is how authored row directives are consumed, and an id-6 answer
adds straight into the row index through `&[rec+4]` - and the key is
the OR of the nodes on that chain, computed on demand and never stored.
0x520c0's sequential half is complete now: the row list wraps and the
wrap is published as key bits, and the reversal is the XOR of the two
SEQ_REV control bits with their owner-side guards, which settles those
bits as row order. Two things PR-101 corrects in PR-100's own text: the
two branches of 0x521f0 are swapped there (the empty-key row is the one
that runs the feed and the threshold compare), and the `+0x1c` argument
it flagged as inconsistent is a row index after all - the index comes
from the transition table at `[rec+0x1a]` (key, record index, row
index), which is the engine's own version of `obj_transition` and the
new head of this list.
PR-102 then wrote that table's story (RE only, no pin): its writer is
the base record class's own parse method (the class table 0xa260c
holds 0x51910 at its +0x00 parse slot, and inside it 0x51e07 builds
the entries - the entry ctor 0x49610 has exactly one caller), it
matches the authored form `TARGET(N)=<key expression>` (format string
at 0x9225c) and pushes {key, target-state index resolved from
`[obj+0x3c]` by name, row index N} into the vector at `[rec+0x1a]`.
The record class itself is now mapped: a base of 0x48 bytes (factory
0x518f0, copy-ctor 0x517e0, which writes `[rec+0x44] = 0xa260c`) and
a derived class of 0x60 (0x52570, ctor 0x52500 writing `[rec+0x44] =
0xa25d4`) whose parser reads `F=`/`T=`/`G=` into `[rec+0x50]` and
beyond, with the 130 clamp on `T=`, and delegates every other line to
the base parser. So a shipped line like `A_WAIT(1)=AC_BREAK_NEXT`
means "go to state A_WAIT starting at row 1 when AC_BREAK_NEXT is
raised" - the engine's transition picks the START ROW, this port
parses it (PR-86) and then sets fi = 0, and the
data asks for other rows thousands of times (2572 lines, 37 states
entered at several rows, up to 97). 0x47e40 is the match and its four
length-byte cases are exact / subset-by-AND / any-common-bit /
degenerate; the control-token table re-derives to the port's own
PR-59/60/90 bit values exactly; and the mode field turns out to have
five values, with BY_Y feeding `|v_y|` (0x543dc) and BY_TY being a
separate guarded feed (0x5441b, gate `[obj+0xbe] != 0`) - so BY_TY is
not a typo alias of BY_TIME, and those two gates
([obj+0xba]/[obj+0xbe]) turn out to be *assigned*, not derived:
0x549f9/0x54a08 copy them from a caller-supplied 2-dword structure and
the same method then rewrites `[obj+0xba]` to +10/-10/0 from a
distance comparison with +/-5 margins - the best lead yet on the
fields PR-99 left unread. PR-108 then closed that lead's remaining half:
the method IS the frame loop's slot +0xcc (0x549f0, 102 of 158 class
tables; the lion family overrides it at 0x50340), `[obj+0xbe]` is the
contact record's vertical force (kind 4 at 0x54aa2, built from the
authored array at `[obj+0x136]`, 0x24681-0x24688), and BY_TIME is NOT
gated at all - 0x543fb is entered straight from the jump table.
STATUS after PR-131 (footage audit of THIS stage): the first-chasm
crossing is STILL not observed in any sampled window, and the
leading reading changed - the player most likely BYPASSED the gap
above the water (up the ladder at x=180, along the canopy, down at
t=25-28 of levels/ZARIA.01.mkv), so do not treat a running jump at
that chasm as the reference until re-watched; T1's zax37 ~315 s
running jump is a separate jump (and the zax37 player does walk the
water surface at 305-307). The measurement windows are named in
§7 item 13's PR-131 bullet. The port's own probe walks die to LICHA
contact damage long before x=408 (G-5, CLOSED by PR-139 - the
corrected contact model is what lets him reach x=180). The probe is run
again as PR-140.
STATUS after PR-205 (the ordered re-watch of this crossing: DONE;
it closes the "until re-watched" condition above and narrows that
line's "down at t=25-28" wording per the PR-205 CORRECTS line):
the crossing is STILL not observed, and this clip can never supply
it as a measurement - the last verified frame is t=31.25s, camera
(231,210), the lion's feet near x=306 on static branch art, 102
units WEST of the chasm's west lip (x=408) and far above the
canopy polyline, that frame's view covering the west lip itself;
at t=37.5s and 38.0s no camera in x -900..1761 matches the port's
statics renders at all - a floor of 24.3-28.9 MAD against verified
readings of 9.9/10.4/15.4 and shifted-camera controls of
25.7-43.4, because from t about 32s the frames are dominated by
object-class content and tiled background that the statics render
excludes. The verified approach CONFIRMS the BYPASS reading
approach-side (ladder at x=180, then along the canopy toward the
gap) and leaves it unmeasured AT the gap; a running jump at this
chasm is not on film and is not filmable in this clip. T1's video
fixtures for the jump range are UNCONDITIONALLY the zax37 jumps
(ep.1 ~316-317 s, ep.2 ~465 s), not "until re-watched". The full
measure, byte citations and the instrument bands: the PR-205
record.

**T2. The lion's attack and enemy energy.**
Goal: kill the first LICHA. Where: `LEW.MOV` states `A_FIGHT*`, `A_FG_PACKA*`
(conditions `K_FIRE`+`K_F1`/`K_F2`, `K_ALT`); enemy `.MOV` death states;
`energy=` from the placement. How: add the fire keys (Ctrl=fire per the
walkthrough, F1..F4 weapons; map as viewer choices — LANDED in PR-87:
K_FIRE/K_F1..F4/K_ALT on all three input surfaces, comma groups as AND,
A_FIGHT reachable and pinned on the real ZARIA script), find where the lion's hit
box (`AC_RECT`) and the damage to enemies are decided (enemy hit method slot
+0xd0, class 0x41180 / 0x3dae0), apply, play the enemy's dead state, remove
the contact damage. Done when: a scripted run kills one LICHA and a pin shows it.
(Partially met: the keys and the A_FIGHT entry exist since PR-87; the hit,
the energy decrement and the death state do not. MET IN PR-89: the ATC
window (ATC_ON/ATC_OFF frame flags), the hit through the SAME shape
contact as the push-out, damage 30 (the 0x34103 default), the
[obj+0x102]=14 hit countdown, the engine's hp<0 -> A_DEAD-by-name gate,
and dead objects stop hurting the lion - ZARIA.01's first LICHA dies at
tick 184 in a pinned scripted run. PR-89 also root-caused the
lion-vs-object shape reads (each element's margin lists come from its
OWN .SEQ payload - the old single-payload read used wrong bytes, so the
PR-59/PR-68 contact pins were re-pinned). MET IN PR-90/91 (T3 st. 1/2):
the enemies' OWN attacks through their ATC windows (their A_ATAK ATC_ON
frames vs the lion) and the lion-side AC_ENEMY_ATC raise with his
authored A_DOSTAL reaction. PR-94 UPDATE to the `AC_RECT` plan: the
census over the shipped .MOV corpus shows AC_RECT is authored ONLY in
lion-class scripts (ZARIA A_KILOF(25)=AC_RECT,K_GEN1, SADES A_HOLE,
LONA A_WALKINTOIGLO, TUNGA/TUNGA2 A_EXIT=AC_RECT,AC_BLOK, KADMAR
A_LISC_FALL/END, ZOLW A_JUMP) and NONE of the contact/answer handlers
raises it - it is a lion-side state-entry signal (stone-push/hole/
igloo/exit), not the object hit box; the raiser code and any object-
side consumer stay open. The hit-method slot +0xd0 is family-specific
(111 factory-installed vtable bases, no shared layout) - full protocol
in the PR-94 NEXT_STEPS record. Still open for T2: the proportional-hit
template identity (the energy*30/100+10 id array, PR-96) - see open question 9.)

**T3. Enemy behaviour.**
Patrol between `bounds=` (LANDED in PR-79: BY_X states walk at F/4 px/s and
flip at the bounds range; PR-135 restates that rate as the engine's own law -
`F*s*LL_ITER_MS/(1000*T)` px per iteration at s = ±10, the SAME number at
T=40, which is where every shipped patrol sits, and 25% slower at T=50);
CH_DIR landed in PR-81 as the facing flip - the
census pairs bounds= with CH_DIR and the video's winning art polarity flips
with the travel direction), detect via the class AI's compiled box
(PR-90 pinned LICHA's; `scope=`'s consumer still open - measurably NOT
that box), `AC_ENEMY_ATC` attack, the
`ATC_ON/OFF` directives and the `F` motions in their scripts. Needs T1's `F`
semantics and open question 3. Done when: ZARIA.01's LICHA/PAJAK/NOSOROG move
and attack in a headless run (log pins), and the contact damage uses the state.
(Partially met: they MOVE and TURN - the verify-play contact leg now hits a
WALKING LICHA (the worm is in its BY_X A_RUN state at the first touch, tick
170 since PR-97 settled both shape boxes on the ground), and PR-82 generalized
facing to follow the travel
direction of every CH_DIR state for objects AND the lion, so a future
charge/push source turns the sprite with it automatically. PR-89 adds
the missing attack MECHANICS: the ATC window is parsed for every script
(the LICHA's A_ATAK opens it at frame -4, closes at -8) and obj_hit is
symmetric. PR-90 lands T3 STAGE 1 - the ENEMY ATTACKS THE LION: the
engine's enemy AI is compiled per class and its class is the placement
token ("LICHA" at 0x9321c vs the .des field 1, 0x616d0), so A_ATAK is
entered by CODE, never by .MOV data; the port's stand-in (OFF unless
LEO_PLAY_DANGER=1) arms the script's OWN A_ATAK by name from a
DANGER-flagged state when the lion enters the pinned LICHA MAIN box
(abs(dx) < 40 && abs(dy) < 50, 0x61b6e), and the enemy lands one hit
through its OWN ATC window (verify-play pins: arm 171, window hit 265,
energy 55). PR-91 lands T3 STAGE 2 - the lion ANSWERS: the window hit
raises AC_ENEMY_ATC on the lion's own set, and ZARIA's authored
A_DOSTAL reaction runs with zero new mechanism (entry row 1 at tick
266, seq=hit art, the `I -320,-250` pullback impulse, wrap exit
A_DRAP(1)=AC_BREAK_NEXT at tick 356; selftest + play legs pin the
whole chain; contact hits deliberately do NOT raise it - open question
10). PR-92 lands the OBJECT-SIDE half of the contact protocol: the
touch raises AC_ENEMY (bit 7) on the touched object - the engine
packet bit (0x51050 `or dh,0x80`) - and the object's OWN A_ATAK(1)/
A_DEAD(1)=AC_ENEMY lines react (obj_touch_enemy; a second non-solid
shape pass outside the SOLID loop, because no consumer state carries
SOLID; the selftest pins the shipped RBLOK/GPTAK chains, the two
contact legs pin the raise at 170 in both (PR-97 re-pinned these; every
older pin in the same legs is
byte-identical). What remains for T3: the engine's own driver and ATACK-side test
(open question 10), the other classes' gates, live object-side
consumption (swimming/flight locomotion - see open question 10), and
`scope=`'s real consumer (measurably NOT the attack box); the attack
itself has NO authored motion - the worm stands and swings, matching
the frames. PR-97 UPDATE to those tick numbers: every grounded y in the
play legs is now the shown animation's shape-box bottom standing ON the
.plt line, so the ZARIA.01 chain re-pinned to first touch 170, arm 171,
window hit 264, A_DOSTAL 265, A_DRAP 355, the kill at 183; the per-pin
history in the Makefile carries each number and its reason.)

Scene placement (M4-C line, closed further by PR-93): the user's
ZARIA.01 report led to the audit - placement coordinates WERE already
read; what was missing were the two grammars the census proved (§5):
PR-93 places and draws the static `.SEQ` images (the stone shelves,
14/14 in ZARIA.01, 69 join game-wide, the LONA.05 WODOSPAD2 holes
named) behind the actors, and stops instantiating the 0,0 attached
children (the corner fruits) while joining their scripts unchanged. The
before/after probe method (byte-deterministic pinned camera,
LEO_LEVEL_CAM/CAMY/PPM_TICK) is recorded in the PR-93 NEXT_STEPS
record. Open follow-ups are §7 item 11 (statics animation, the child
spawn trigger, panc.seq's coordinate reading, ZAR.BLK/GRP formats).

**T4. Signal bus, pickups, inventory.**
Resolve open question 2 (RESOLVED in PR-88: the full name->bit table decoded
from 0x48000/0x919ec, K_GEN1=54 confirmed), then: the `GEN`/signal delivery
between objects (OPEN: who routes a signal raised on one object's set into
another's conditions), the
`AC_*` actions (`PLAT_OFF/OUT`, `XCHG_LEVEL`, `SHOW_CROWN`, `MSG_PUSH`, `LEAVE`),
pickups (`GRABOBJ` handler at slot 0x78 = 0x3db90, `KORONA`, `SURPRISE`, stars
`GWIAZDKI`), the item box (`TOOLS.SEQ` icons; the video shows the held item),
tools use (`K_F3/F4`: stone/pickaxe/stick). Done when: ZARIA.01 can be played to
the stone-corking puzzle (walkthrough in NEXT_STEPS PR record / the Polish
walkthrough page) and each step has a log pin.

**T5. Ladders, vines, climbing.** States `A_WSP`, `A_WSP_LIN`, `A_PODCIAG`,
`A_SCIAG`, objects `DRABINKA`, `LINA`; conditions `AC_DRAB_CATCH`,
`AC_LINA_CATCH`, `AC_EDGE`. Depends on T4's conditions.
STATUS after PR-109: the engine's side is read - all 12 authored BY_TY
states (A_WSP `climb`/`climb_r`, A_WSP_LIN, TUNGA2's A_DRAB, in
ZARIA.01/ZARIA.02 LEW.ZAR + LEW.ZARPL + LEW2.ZAR + LEW2.ZARPL, KARA.07,
TUNGA.11, TUNGA2.12), the mode-4 feed 0x5441b with its `[obj+0xbe]`
guard, the input-apply 0x549f0 that writes that register from a contact
record, and the fact that the entry/exit conditions are
AC_DRAB/AC_DRAB_CATCH/AC_LINA/AC_EDGE, which no port code raises. The
port's model AT THAT POINT (plain ms clock, no vertical feed) was pinned
by a `--play-selftest` leg.
DECIDED, by the owner's clip (PR-109): the row clock is the frame-loop
ITERATION count, NOT milliseconds - an authored threshold T is displayed
for T/100 iterations (measured 66-100 ms on screen for the authored
200/300 of `[A_WSP]`, and no pose held 5 video frames or more in 5 s),
so the port's ms row clock is 3.0-3.5x too slow and PR-110 is the
change that makes it count iterations, with its test plan named in that
record. The clip also fixes what these states are: rung-ladder /
DRABINKA climbs (`seq=climb`/`climb_r`), not vine or rope hangs; ENA2's
rope state A_CLIMB is BY_Y (mode 1) with a |v_y| feed and thresholds of
4, a velocity-coupled animation and a different mechanism.
PR-110 SHIPPED THAT CHANGE: the port's row clock now counts
1/100-iteration units with one iteration = `LL_ITER_MS` = 28 ms, fed
through a whole-millisecond carry, and the mode split of 0x543a2 is in
both the player and the object path - a BY_X state is fed by travelled
distance alone, every time mode (BY_TY included) by 100 per iteration.
The BY_TY selftest leg was re-derived on that grid (a 1700-unit list =
17 feeds), and the legs of every other task that quoted a row duration
moved with it, each with its derivation in the Makefile. The port still
does NOT model the `[obj+0xbe]` guard of mode 4, nor any vertical force.
STILL OPEN: the vertical DISTANCE law (measured ~100 source px/s, and
nothing in the repo turns `F=0,100 T=160,50` into it), which conditions
the port should raise to enter and leave a climb, and PR-108's question
1. **THE CLIP STILL WANTED: a hang with no vertical travel - a hold at a
rung, or the pause at the top before the step off** - which says directly
whether the animation freezes without vertical force. The clip that
answered question 2 shows continuous climbing, so it could not answer
that one, and it could not be tied to a level (the `[A_WSP]` block is
identical in ZARIA.01/ZARIA.02/TUNGA.11, so the timing conclusion does
not depend on which). Full wording in the PR-109 record.
STATUS after PR-131 (superseded on the port side by PR-139/PR-140 -
read the PR-140 record before quoting the old probe wording): the
ZARIA.01 ladder AT 180 is confirmed usable in the original (ridden
down at levels/ZARIA.01.mkv t=13.3-15.7 and mid-climb at
ZARIA.01_b.mkv t=200.0-200.7). The old claim that the port's probe
"dies at the LICHA worms around x~153" is NOT what the port does: on
the pre-PR-139 source the same walk reached the SAME positions
(tick 91 x=180, tick 300 x=685) while PAYING 15 per touch (three
contact hits, energy 100 -> 55) - the old model blocked the traverse
by energy cost, not by a position stop, and no probe configuration
here reproduced a death before x=180. PR-140 runs the probe under the
corrected model and pins it as a verify-play leg: a 12 s FWD walk
crosses both worms and stands x=685 at tick 300 with ZERO damage,
zero state changes outside A_WALK, and the worm answer being the SEMI
push-out alone (cost: ~40 lost iterations, 0.8 vs 2.5 units/tick).
The CLIMB at x=180 still does not work, and the blocker list has
narrowed repeatedly (PR-141, PR-142, PR-144/145; PR-146 closed
the last reading gap - the contact delivery is read end to end -
and PR-147 closed the last reading law: the A_WSP feed law is
now read at byte level, so ONLY the implementation decision is
left; PR-148 shipped its first half - the guard and the intent
word are CODE, and what blocks the climb now is solely the
contact-answer route). CLOSED by PR-141: the
AC_DRAB/AC_DRAB_CATCH answer bits themselves - DRABINKA's own
answer entry 0x3f1c0 raises exactly those into the toucher, gated
on its unroll rows, and the ladder's +0x40 getter is constant-zero
(no ATC window, ever). CLOSED by PR-142: the EVALUATOR/KEY
mechanism - the 13-byte field a transition ANDs against IS the
live signal set at [obj+0x60] (PR-88's TEventSet reading stands,
PR-101's identity-key wording is superseded: the object family's
identity providers are `ret` stubs), 0x47e40 is a set-operation
dispatcher not a lookup, and an authored condition matches by
(authored node mask AND live) != 0 inside the ordered row scan
0x52000 - first match wins - which is EXACTLY the port's shipped
condition model (a confirmation, no port change). Signals enter the
live set through the class +0x10 entry 0x53770 (signal record, bit
index at its byte +0xc), so K_GEN1's MECHANISM is found and only
its POSTER is open. PR-145 read the KEY FEED and PR-146 CLOSED
the CONTACT DELIVERY end to end, so the climb line now has one
open law and one implementation decision - no unread mechanism
left (PR-146 also corrected three readings, including two of this
series' own):
THE CONTACT LOOP, READ: object A's motion pipeline (the [obj+0x44]
interior vtable: 0x52750 -> slot +0x34 = the merger 0x52790 ->
the Lew / animacje / zwierzaki list paths 0x4bd30 / 0x4b890 /
0x4b870, gated on the object's own flag bytes) gives the other
side's +0x78 contact entry a turn; that entry answers with a
13-byte set AND appends a node (alloc 0x1d = a 16-byte header +
the answer set at node+0x10) onto THE ASKER's contact list
[A+0xd2]. That append inside a contact entry (0x3d440, held at
+0x78 by the tables 0x96a88/0x99e64) is the ONLY non-zero writer
of [obj+0xd2] (0x5415b/0x54e88 write zero, 0x551fc pops). A's
contact method 0x549f0 then ORs the head node's STORED answer
set into [A+0x60] under a node-kind + record-index guard (0x54b65
..0x54b84; PR-145's "[partner+0x60]" expression corrected), and
the lion's own key provider pops the head (0x504e0 -> 0x551f0).
That is how DRABINKA's 0x3f1c0 answer (AC_DRAB 10, AC_DRAB_CATCH
12, PR-141) reaches the lion's live set - no signal record.
PR-144's "zero-reference contact fragments" were a mis-anchor (the
entry is 0x4bd30, called once at 0x527cb from 0x52790, which
PR-130 already named); the queue drain 0x52b50/0x52b80 was
already PR-106's.
THE SIGNAL POSTERS ARE A CLOSED SET: the tag immediate 0x8138764
appears SEVEN times image-wide - six builders plus the receiver
test at 0x53780. AC_DRAB is posted by ONE direct receive call
(0x28835 builds bit 10, 0x2885a calls the target's +0x10 entry)
into [[obj+0x30]+0x40][1] - that index is NOT proven to be the
lion; AC_KOREK(33)/AC_BECZKA(28)/AC_SPOKUJ(27) ride the three
0x4bc40 broadcasts; AC_LINA(21) is pushed straight into the
lion's queue (0x2c9a4); K_GEN4(57) is posted in a loop over that
array. Bits 12/22 have NO signal poster - a counted negative, and
consistent with the answer-set route above. PR-143's broadcast
"bit census" was the RECORD-TYPE argument (the type census is in
the PR-146 record); its conclusions - no broadcast posts key bits
or 10/12 - stand on the stronger basis.
THE KEY FEED (PR-145, two sentences corrected by PR-146): the
generic standing-bits provider 0x54910 (class slot +0x8c;
DRABINKA overrides it with a zero stub - PR-142 measured the
override) builds K_UP/K_DOWN from the signed intent fields
[obj+0xbe], K_BKWD/K_FWD/K_JUMP from the intent + the open-window
flag [obj+0xb6] bit 4, K_FIRE/K_ALT from the authored state flags
[obj+0xa5..]. The intents come straight from the state record
(record word 0 -> [obj+0xba] at 0x549f9, word +4 -> [obj+0xbe]
at 0x54a08), with the engine's own +/-10 overrides written by
0x549f0 when the head contact node's KIND is 1 (target-relative
through the [vt+0x68] getter, +-8 dead band) or 4 (raw x); the
selector is the node's kind word [[obj+0xd2]+4], not a record
mode. The 0xcf400 "input singleton" is itself an object (records
via 0x58690). K_GEN1's posters: an HP<100-gated message type (the
0x50d20 jump-table entry 0x50df4), 0x244a5/0x24552, and a
PRNG-random K_GEN1/2/3 (0x76bcc LCG -> 0x26670 -> the raise at
0x26738). PR-144's 14-site live-set write census is resolved
except two mask windows (0x28bc7, 0x50ab7); 0x2451e is K_FIRE
(corrected).
WHAT REMAINS FOR THE CLIMB: ZERO open laws (PR-147 closed the
last one at byte level, and corrected one of PR-146's labels -
the kind-4 contact chain is the VERTICAL intent arm, not a
raw-x variant). THE LAW, END TO END: the authored A_WSP/
A_WSP_LIN blocks (12 in ANIMACJE, BY_TX zero - a byte census)
carry wire control word 0x205 = BY_TY mode 4 + CHECK_ALL +
SEQ_REV_Y, F=0,100, T=160,50, rows 300/200.../300. A BY_TY row
is fed EXACTLY 100 per frame while [obj+0xbe] != 0 (0x5441b;
the 100 has exactly one writer image-wide, 0x4c8b7 in an init
entry), then the compare against the row threshold [rec+0x10]
runs AFTER the feed with one admit and the remainder carried
(0x52320..0x52338 -> the row advance 0x52470 writes it back at
0x52118) - the shape the port already ships. THE GUARD WORD IS
THE PLAYER'S OWN VERTICAL INTENT: the frame loop calls the
LION's class-table slot +0xcc with its 12-byte input block every
frame before stepping it (0x4aeca; that slot has exactly 7 call
sites; the loop is gated on ds:0xa94f0 - zero makes it return
2), 0x549f0 stores the block's words into [obj+0xba]/[obj+0xbe]
unconditionally (0x549f9/0x54a08), and the KEYBOARD puts +/-20
on the two axes (Up 0x248 -> -20, Down 0x250 -> +20, Home/Left
0x24b -> -20, Right 0x24d -> +20; reader 0x582f0; words 0/1 are
only ever 0/+/-20). THEN the contact chain overrides, in the
same call: a head-node of kind 4 moves the vertical intent to
+/-10 toward the node's y ([head+0x1d], a +-8 dead band,
0x54b16/0x54b3b/0x54b47); kind 1 does the same on x (PR-108's
arm, +-8 confirmed); kind 2 merges the stored answer set (PR-
146). [obj+0xbe] has exactly four writers and five readers (the
guard 0x5441b, the integrator's force 0x542cd, K_UP/K_DOWN at
0x5499d) - so while a contact node exists the intent is the
engine's +/-10 toward the partner, otherwise the player's
+/-20; PR-145's "s = 10, never any other magnitude" NARROWS to
the contact path. The keyword encoding itself is now byte-exact
(0x51b35..0x51da0: BY_X 1, BY_Y 2, BY_TIME 3, BY_TX 4, BY_TY 5,
FRAME_REV 8, SEQ_REV_X 0x10, SEQ_REV_Y 0x20, ...) confirming
PR-108's five-value mode item for item; the demo record format
is a 12-byte input block + a length (0x58690 playback,
PR-145 item 6 CLOSED). THE IMPLEMENTATION DECISION IS HALF
SHIPPED (PR-148): the port now HAS the guard - a BY_TY row is
fed the 100 only while the player's vertical-intent word
(p->vert_intent, the engine's [obj+0xbe] shape with its sign
semantics) is non-zero, the compare still runs, and the partial
accumulator carries across the gap (0x5441b's order); the held-
intent spans are UNCHANGED from PR-108's, so the guard only
subtracts feeds. What the port still lacks: the contact-answer
route (the node payload -> the live set - the blocker of the
ZARIA.01 climb, NOT the guard), the word's use as a v_y force
(0x542cd reads [param+0x4], field map unread - PR-148 open
item 1), and any vertical MOTION: the session's claimed rise
law at 0x50252 is DEAD CODE (zero references by both scan
kinds; the lion's frame-loop +0x74 is 0x50320 -> 0x54c00, a
friction/ground gate), so the climb's rise waits on the y-force
path, not on this PR. OPEN ODDITY COUNTED: the kind-4 chain
compares node+0x1d, the 30th byte, while the node builder's
allocation is 0x1d bytes (whose answer set ends at +0x1c) -
who builds kinds 1/2/4 (PR-146 item 1) has teeth now; node+0xc
IS the ID compared at 0x54a24 (PR-108), which half-resolves it.
CLOSED by PR-143 additionally: the answer-
chain entry gate (row flags 0x4000 = the port's
LL_CTRL_CHECK_ANI; ZARIA.01's drab/lina rows carry none, so the
ladder answering nothing in PR-140's run matches the engine's
law), the row-switch executor 0x535a0 (exit/entry/index-store),
and the scan's caller key = a verbatim copy of [obj+0x60]
evaluated in the same pass as the signal raise. CLOSED by
PR-144 additionally: the LION's own side - class tables
0x96960/0x96958 (token LewZaria 0x9078b, factory 0x1fef2), its
receive front door 0x1e130 with generic tail 0x50d60 (PR-143's
item 4: the +0x10 slot DOES differ per family), its contact
entry +0x78 = 0x50f80 (15 tables carry it; the ladder's 0x3f1c0
is the same slot on the other side), the pair-probe 0x4bd32
that gives BOTH members their contact turn and unions their
outputs (the "collision caller's buffer" of PR-142/143 IS this
union), and the exhaustive live-set-write census above - the
AC_DRAB instance hunt is down to one link, not a search. The
player also rewrites its class-table pair at runtime
(0x1ec36..0x1ec3f) - named, not modelled, not needed. Reading
order in
the PR-144 Next entry; nothing was invented for any of it.

STATUS after PR-147 (the feed law, CLOSED): the 12 authored
BY_TY blocks run mode 4 - guard `[obj+0xbe] != 0`, feed exactly
100 per frame, threshold = the row word [rec+0x10] (the port
reads it from the 300/200 row lists), compare AFTER the feed,
one admit, remainder carried - so PR-109's operational rule
(T/100 iterations) is now RE-DERIVED from the engine with its
word identity pinned. The guard word is the PLAYER'S VERTICAL
INTENT: the frame loop hands the LION the 12-byte keyboard block
through its +0xcc contact method every frame (0x4aeca), the
keyboard writes the axes at +/-20 (Up -20 / Down +20, 0x584a9/
0x584c3), and a kind-4 contact node overrides it to +/-10
toward the node's y with a +-8 dead band (0x54b16..0x54b47) -
the same word the integrator uses as the v_y force (0x542cd).
A BY_TY row therefore advances only while the vertical intent
is held, and stops the instant it is not; the port's row model
matches the shape and lacks the guard and the magnitudes. What
remains is the implementation decision (PR-148: the input/guard
law with the engine's numbers, then the contact-answer route),
plus one counted oddity (node+0x1d vs the 0x1d allocation -
CLOSED by PR-149: kind-4 nodes are allocated 0x21, one 0x21
site image-wide, in the kind-4 chain itself).
STATUS after PR-148 (the guard, SHIPPED): that decision's first
half is CODE now - LL_CTRL_BY_TY + the parser mapping, the
player's vert_intent word (Up negative, y+ down, at the port's
validated intent scale; the 20-vs-10 keyboard constant tension
is NAMED at LL_PLAY_INTENT_FULL), and section 7's mode-4 feed
guard in 0x5441b's exact order (feed gated, compare not, carry
kept). The re-pinned PR-108 leg runs all three behaviours and
the 8/10 mutation battery pins the scope (extending the guard
to every non-walk mode starves the BY_TIME legs across the
whole selftest, first at the pre-existing walk/jump legs; the
BY_TIME guard 0x543f2 itself is a counted study, not copied).
NO vertical
motion was added: the 0x50252 "rise law" in the working notes
is dead code (zero references, both scan kinds; the lion's
frame-loop +0x74 is 0x50320 -> 0x54c00), and the y-force path
(0x542cd, [param+0x4] field map unread) is open item 1 - the
climb's rise is a contact-era question, not this PR's. M9 (the
intent sign) and M10 (keys-not-word) survive by construction
until the y-force path and the kind-4 override exist - the
reason the port carries a WORD. The ZARIA.01 climb blocker is
unchanged: the contact-answer route, PR-149.
STATUS after PR-149 (the blocker's spec, CLOSED AS A READING
GAP): the contact-answer route's every input is now pinned at
byte level - the node builders and their allocation sizes
(kind 1 at 0x53eb0/0x53f00, the answer-COPYING kind-2 builder
0x53f50, the kind-4 patch chain allocating 0x21), and the
ladder's answer 0x3f1c0 as an operand-pinned predicate tree
(the toucher's block vs the ladder's +0xc5 block, then its
70%-inset rect, then its full [self+0xb1..0xbd] rect - all
through the shared rect-intersection slot 0x23e00; the row-1
branch only FLAGS, row 0 never answers). [Corrections shipped
by PR-150: the order is FULL extent FIRST, then the 70%
probe; the inset is a SYMMETRIC central-70% cut; and the
row values are 1-BASED POSITIONS - position 1 FLAGS (a fresh
ladder's child list is empty, so it is inert in practice),
position 2 - the drop's `3,400` line - is the ONLY answer
row, positions >= 3 inert.] So PR-150 was a CODE
question, not a reading one - and it is now shipped, below.
The remaining NAMED in-game
blocker moved one link down the chain: the ladder's own live
set never receives K_GEN1 (bit 54) to admit its A_UP out of
`1,0,WAIT K_GEN1` - PR-149 adds two negatives there (the
+0xd0 receiver and the signal-record census) and the probe
leg must raise the signal directly until it resolves.
STATUS after PR-150 (the answer, SHIPPED): the ladder's +0x78
answer is port behaviour now - ladder_class / pt_in_blk /
blk_probe70 / ladder_answer_core / blk_of_obj /
ladder_answer_for in ll_level.c, called from BOTH play_collide
passes (solid and PR-92 non-solid) after the shape meet and
the push, merged straight into the lion's pulse set (no node
list, no merge-delay: counted open item) and read by the NEXT
iteration's transitions. The row gate is the 1-BASED row
position (the port's fi + 1, clamped like obj_shape_elem):
only the drop's SECOND line answers. The RUNG block is sourced
from the object's own current art bbox through the same
obj_shape_elem path the contact passes use; with no art/shape
it is the ctor-shaped 1x1 at the object, and the EDGE block is
modelled as that same 1x1 (the ctor never stores +0xc5 at all
- a named model choice, pinned by legs). The three PR-149
mis-reads (row index space, symmetric inset, P2/P3 order) were
corrected in place, not hidden. ZARIA.01 output is UNCHANGED
and pinned by legs: no CHECK bit gates the walk's list, and no
K_GEN1 raiser means the port's ladder never leaves A_UP - the
K_GEN1/WAIT arm remains THE named in-game blocker (fresh
leads: the 0x238e4 row-2 base method, the 0x52607/0x52610
clamp family). Test: ladder_answer_selftest wired into the mov
chain (probe law at every hand-pinned truncation edge, the
decision table, class scope, the ZARIA-shaped idle-row and
clamp legs); 10 mutations, ALL DETECTED, none survive. Next
code PR: the kind-4 contact node's +/-10 y-override - the
answer path can now PLANT a node.
STATUS after PR-151 (docs-only, and it corrects that sentence
before a line of kind-4 code was written): the premise is
WRONG - answers plant kind-2 nodes, and kind-4 nodes come
only from the per-class post-transition hook +0x90 (the
pipeline 0x53520 calls it right after the 0x52000 scan and
before the live-set clear - the port's evaluate-then-clear
ordering CONFIRMED; the +0x90 call sites are exactly two,
0x5357d/0x3edaf). ZARIA's hook (0x1e250) is a transition
REDIRECTOR (facing flip, a 30-tick [obj+0xad] bite, a
one-shot rewrite to row 9 when [obj+0x12a] arms - no live
ZARIA-path armer found, counted - transition CANCELs under
globals ds:0xcef64/0xcef68, then the base hook); the only
kind-4 supplier image-wide is the ENA hook 0x245b0, building
zones on the LION from the runtime array [obj+0x136] whose
only writer is the SADES hook 0x2e6a0 (so PR-108's "authored
attach data?" is ANSWERED NO). ZARIA.01 supplies no kind-4
contact - the y-override CODE pass waits for an ENA/SADES/
TUNGA-era controller; producer AND consumer are now fully
read, so that future PR is implementation. For the named
K_GEN1 blocker this pass found the strongest lead yet:
ZARIA's ctor links a COMPANION [obj+0x112] through
0x4dcc0(<level>, &0x497b0 query, seed ptr [ds:0x961fc] ->
0x1034c), and the +0x100 hand-off method's ENA variant
0x24440 raises K_GEN1 into the RESOLVED COMPANION's live
set - the first bit-54 raiser outside GEN row tails and
kind-9 messages (ZARIA's tables hold the BASE +0x100 with no
visible raise; the two call sites 0x50c2a/0x41b6a are the
named unread bodies). PR-129's "0x52cc0 ... all edx=9" was
also corrected (0x3f77e passes [self+0x4c]; its method is a
type-0x1c message receiver at a TUNGA-region table dword).
ZARIA.01 output unchanged (no code). Next: the K_GEN1 arm
reading pass with the companion-query grammar FIRST, then the
+0x100 bodies, then the 0x52cc0 sites; then the y-force/
0xcf400 block path, then the WAIT law. 9 open items counted.
STATUS after PR-152 (docs-only; the K_GEN1 chain now has SIX
NAMED LINKS): the hand-off that runs +0x100 is gated per
class by TWO getters now read - a +0x104 row-cursor window
(TLewZaria: true only at row POSITIONS 7-8; ENA 5-6; SADES
29-30; TUNGA2 7/9; the BASE class getter is `xor eax,eax` -
plain base classes NEVER hand off) - and a +0x108 constant
(ZARIA -6, SADES -4, base/ENA2 +1 via [obj+0x9d], TUNGA2
-14): the current row's transition-record array is scanned
for a record whose +0x4 word equals the constant 6 [VERIFIED
by PR-153 F1: row records are 72 bytes and field 2 IS at
record +0x4 - the layout question PR-152 named is CLOSED] -
and in
LEW.MOV the row lines AT POSITIONS 7-8 of A_WALK/A_ZABLOK
are exactly `4,6,GEN AC_TIME`/`5,6,GEN AC_TIME` and `4,6`/
`5,6` - the SAME field-2 = 6 gate PR-100 measured for every
A_WALK pose (the unmodelled WAIT row-gate field, PR-150 item
6, IS the hand-off match constant). The chain: those rows ->
scan fires -> +0x100 resolves the companion (a KEY-REGISTRY
resolution, [obj+0x112], no authored name - the 0x497b0/
0x4dcc0 "search" was mis-framed, corrected; 0x4dcc0 is the
sub-line factory reader, cf. ZWIERZ1's POCISK/PICT children)
-> ZARIA's +0x118 enqueues to the global message queue
[CORRECTED by PR-153 F2: per-object at [companion+0x10],
drained by 0x52b50 through the owner's +0x10 RECEIVER slot]
with the controller's facing -> the receiver 0x50d60's
type-9 arm 0x50df4 is PR-70's bit-54 (K_GEN1) setter [BYTE-
VERIFIED by PR-153 F5 - but the node is TYPE 5, ZARIA's own
receiver 0x1e130 (not 0x50d60) owns the type-5 arm, and the
66-site enqueue census shows NO static type-9: the LINK IS
VERIFIED, THE BRIDGE IS STILL OPEN - PR-153 F6] -> the
ladder's `A_UP(1)=K_GEN1` and the player's climb rows. THE
TWO DECIDING GAPS: the record layout (is [record+4] the row
line's second field?) and the queue consumer + the posted
node's type [BOTH CLOSED by PR-153 F1/F2: yes, and the con-
sumer is 0x52b50 -> the receiver slot, node type 5].
ZARIA.01 output unchanged (no code). Next: the
chain trace (record layout first, then the queue consumer),
then the WAIT law DOUBLY load-bearing now, then the y-force
path. 8 open items counted.

STATUS after PR-153 (docs-only; the chain trace ran and both
deciding gaps CLOSED, with the pass's biggest finding being a
CORRECTION): ZARIA's message receiver 0x1e130 IS the live
+0x10 slot of TLewZaria/TLewZariaB (PR-129's "UNREFERENCED -
dead override" was a listing BOUNDARY ARTIFACT - the entry
sits 2 bytes before the address the objdump listing shows; a
+0x10-slot census found the refs 0x96970/0x97980/0x97aa8).
THE RECORD LAYOUT: CLOSED - row records are 72 bytes (0x518f0
alloc, 0x517e0 grammar): +0x0 = field 1 = the frame selector,
+0x4 = field 2 = THE gate value, vtable at +0x44, row enter/-
exit = slots +0x1c/+0x20 called by 0x535a0 - so the hand-off
scan's [record+4] IS the authored second field (ZARIA matches
the 6 of the A_WALK/A_ZABLOK rows at positions 7-8 and noth-
ing else in the +0x104 window). THE QUEUE CONSUMER: CLOSED -
0x52b10 appends 16-byte nodes {+4 type, +8 payload, +c aux}
to a PER-OBJECT list at [obj+0x10] (the companion's, not a
global queue); 0x52b50 drains each node through [[obj+8]+0x10]
= the owner's receiver; the pump 0x52b80 = drain + tick +0x14
+ conditional +0x0(edx=2) on [obj+0x28], run by the container
loop 0x55380; the hand-off node is TYPE 5 with the facing
sign as payload. THE TYPE-5 RECEIVER ARM (live in ZARIA's own
0x1e130, siblings 0x27470/0x33edb in 2 other eras): payload
.+0 call with edx=2, [ds:0xa94f4]++ - the dead-pairing is
resolved, both sides LIVE and the counter is the one-shot
hand-off latch - then a 2-instruction stamp 0x44900 onto the
main-.des-built singleton [[ds:0xcef40]+0x38c] ([t+0x5c]=55,
[t+0x6c]=count; ctor 0x443b0 stores the class-table pointer
0x98278, in PR-150's counted 0x98274-gap family, referenced
from ZARIA lion code), then 0x52cc0(self,9) - which is NOT a
queue post but a HIT pipeline (slot +0x30 getter, +0x24 =
0x52a50 asking THE PLAYER at [[0xcef40]+0x394]+0x68, +0x28 =
0x52a90, 0x56600 -> [[self+8]+0x18] = bare `ret` in every
lion table, then [self+0x20]=1). FURTHER: the transition
pipeline has ONE entry - 0x536f0, the ladder-table tick - and
the LION tables' +0x14 0x550a0 calls it too (0x53520/0x535a0/
0x523f0 all single-callers inside it), slot order +0x8c feed
(the ladder's is the EMPTY-set stub 0x23a60) -> 0x523f0 row-
key OR -> +0x54 threshold nodes -> 0x53520 scan (context =
OWN live set) -> +0x90 hook -> 0x749a0 fill-0 clear; and
PR-70's bit-54 arm 0x50df4 is now BYTE-VERIFIED ([obj+0xad]
< 100 guard, fill 0, buffer[6] |= 0x40, 0x49580-OR, [obj+
0xad] += 32 - the same countdown field as ZARIA's hook -30
bite). BUT the bridge stays OPEN: drab.MOV/lina.MOV read
WHOLE carry ZERO GEN tokens, the ladder's +0x8c feed is the
empty stub, and the 66-site 0x52b10 census has NO static
type-9 - so nothing static raises bit 54 on the ladder; the
named candidates are the 4 dynamic-edx enqueue sites, the
0x38c stamp-object path and its class table, the ds:0xcf0ac
kind-table builders, and the WAIT-node getters (0xa11c8).
The gap is now exactly: who writes bit 54 (or the row-55
stamp) into the right object. ZARIA.01 output unchanged (no
code). Next: that bridge, in the PR-154 order named in NEXT_
STEPS; the WAIT row-gate CODE pass is next after it - field
2 = the hand-off constant CONFIRMED, doubly load-bearing.
8 new open items counted.

STATUS after PR-154 (docs-only; two bridge hypotheses CLOSED
NEGATIVE, the named K_GEN1 blocker is narrower and still open):
the row-55-stamp alternative is dead - the [[ds:0xcef40]+0x38c]
stamp object is an unclassed LIST of 8-byte two-dword records
(class 0x98278: free-path receiver + a plain copy verb; ctor
0x443b0 never gives it a class pair; built in the main-.des
'demolist' region, and a 4th stamp site sits inside the lion
base's hit-countdown method 0x50720) - bit 54 must come from
a live-set writer, and PR-70's type-9 arm 0x50df4 still has
NO static feeder: the 66-site enqueue census is now raw-
corrected (no dynamic types; one NEW static type-5 site at
0x330a7 onto a fresh 0x9d088 clone, and the [obj+0x12a] type-
0xc chain CLOSED at 0x1ef22). The hand-off's only functional
arm is now exactly: the kind-9 OBJECT (ds:0xcf0ac[9], a run-
time table) passed into the companion's +0x18 - the ladder's
0x3f0f0 consumes one for geometry; finding its builder or a
direct receiver call with edx=9 is THE named next step (the
PR-154 Next list, PR-155). And the WAIT row-gate CODE pass is
fully unblocked: node getters compose the authored row key
through the chain (0x457e0/0x45870/0x45970), so the engine
law is complete and only the port-side implementation is
missing. ZARIA.01 output unchanged (no code). 7 new open items
counted.

STATUS after PR-155 (docs-only; the named K_GEN1 blocker is
now a BOUNDED question, not a search): the engine's generic
cross-object signal law is the kind-0x10 magic-record ({0,
0x8138764, bit-index}) handled at 0x53772 - any reachable re-
ceiver gets ANY signal bit, no HP guard (so the ladder's A_UP
K_GEN1 row and the player's climb rows CAN be gated by design,
and PR-144's full-HP question is closed YES). The hand-off's
type-5 node is INERT on the base receiver chain; its only ac-
tion happens when the companion's receiver is a ZARIA-family
0x1e130 (kind-5..11 arm -> 0x52cc0 hit pipeline -> kind-9 ->
the LADDER's +0x18 = RUNG-RECT GEOMETRY + pump, never a live
set). The only static type-9 poster is the BAL touch (0x3d3fb,
PR-70's port path confirmed). The static K_GEN1 raiser set is
enumerated and era-classified; AC_DRAB (bit 10) gained its
first static poster (0x287a0, with the engine's OWN [[assoc]+
0x50][i]+4 == 6 field-gate - a direct cross-check of PR-152's
row-gate model, which strengthens the WAIT row-gate CODE pass
now). The ONE deciding question for PR-156: whether ds:0xcf0ac/
0xcf0b4 (kind tables) are ever written - if not, kind-dispatch
is dead code and kind-0x10 is the only signal path. ZARIA.01
output unchanged (no code). 10 new open items counted.

STATUS after PR-156 (docs-only; the K_GEN1 search is now a
CONSTRUCTION question, not a signal question): kind-dispatch
is DEAD CODE - nothing ever writes ds:0xcf0ac/0xcf0b4, so
every 0x56600 kind arm (including both hand-off kind-9 arms
and the ladder +0x18 rung-geometry call) never runs; the only
cross-object signal paths alive are the kind-0x10 magic-record
law, queue-typed messages, self-raisers, and the pair share.
For ZARIA.01 that means the authored K_GEN1 (the ladder's
`WAIT K_GEN1` row) must come from one of: a kind-0x10 record,
a queue message, the object's OWN self-raiser, or the pair
share - the DRABINKA/LINA ladder machinery (the only roster
entries with param lines, the only classes embedding the lad-
der pair) is the prime self-raiser candidate, exactly like the
MOVEPLAT 0x28af2 law PR-156 F2 read (state-4 + lion-rect-in-
minus-20 -> bit 54 into self). The ROW-GATE spec is now un-
blocked: the engine indexes [[obj+0x50] by [[obj]+0x5c] and
tests field +4 vs per-consumer constants (2/6/0x19) in FOUR
independent methods - PR-150 item 6's UNMODELLED gate field
becomes a parseable, feedable field once the row-builder and
ANI-STATE reader (PR-157) names the format. ZARIA.01 output
unchanged (no code). 7 new open items counted.

STATUS after PR-157 (docs-only; the row-gate is now a COM-
PLETE ENGINE SPEC, not a question): the tested word is the
current state's ROW CURSOR [record+4], written only by the
transition applier 0x535a2 (same-state: cursor := rowB; new
state: EXIT/ENTER verbs + [obj+0x5c] := recA); rows [[obj+
0x50] are per-state authored RECORDS bound BY NAME at place-
ment (slot +0x7C, 0x537f0) from the object's own parameter
entries through the ds:0xceff8 registry, which is fed by au-
thored 'seq NAME,...' lines - present in NO extracted file,
so ZARIA's controller rows exist only inside the encrypted
main data. The K_GEN1 bridge keeps its one unbound link for
the honest reason: the controller object whose A_WALK cursor
the hand-off machinery would test is never instantiated in
anything readable. For T5 the practical consequence: the WAIT
row-gate CODE pass can be written NOW (cursor + row-targeted
transitions + the (key,recA,rowB) transition-table law ground-
ed at 0x52000/0x535a2) with ZARIA.01 output required un-
changed - a pure model-correctness pass, no visible behavior
risk. PR-152 open item 7 and the [rec+4] identity are
CLOSED (cursor; the "authored 6" coincidence resolves to row
INDEX 6 = the `4,6,GEN AC_TIME` pose). 8 new open items.
[PR-158 F4: the cursor's 0/1-based nature is OPEN - if 1-
based (the (N)-verbatim + "0 = before any admit" + 0x300dd
zero-reset readings), consumer 6 names the `3,6` line and the
row-identity sentence shifts; the numeric coincidence stands
either way.] [RESOLVED by PR-159 F1/F3: the base IS the au-
thored 1-based (N) - the only non-zero writer copies it ver-
batim, rows[] binding scans names with inc-before-compare,
and 0 is exactly the clone-init value; consumer 6 = the `3,6`
line.]

STATUS after PR-158 (ledger recovered; the row-gate CODE
question CLOSED as a question, by inventory): PR-150's and
PR-151's full records are back in NEXT_STEPS.md (they had
been swallowed by old Next-swaps and survived only inside
their own commit blobs - the census recipe in the record is
the audit rule for every future swap). The ladder's answer
gate was re-read: 0x3f1dc reads [ecx+0x5c] with ecx = SELF,
so by PR-157's law it tests the STATE slot - 1 = the initial
state (FLAG branch, answers nothing), the K_GEN1-transition
target = the only answering state, >= 3 inert - and PR-150's
"1-based row position" attribution is corrected in the ledger,
the HANDOFF and the port's own comment, while EVERY ZARIA.01
leg and the port's fi-model survive both readings (the one
authored transition never fires). No new cursor machinery is
needed or shipped: o->fi IS the modelled cursor (set via
ll_mov_row_start, ll_level.c:835) and every cross-object
cursor reader belongs to a class the port never instantiates
at T5. One honest crack stays open and is now PR-159's first
read: WHO advances [[rec]+4] during ordinary row playback
(the applier only SETS it; 0x300dd resets it) - until that
owner is known the cursor's 0/1-based nature is unresolved,
PR-157 F4's row-identity arithmetic is flagged, and the lad-
der-gate fi-vs-state decision waits on the record-pool de-
code. [CLOSED by PR-159 F1/F3: no incrementer exists - the
writer law is a transition match (0x53616) or the clone/ctor
initialization (0x300e6, the 0x300dd window CORRECTED from a
"restart" to object creation); the base = the authored 1-
based (N), and rows[] binding is name-position-based and 1-
based; only the param-token mapping stays data-blocked.]
ZARIA.01 output: UNCHANGED (the ll_level.c edits are
comment/diagnostic-string only; full gates pass - 56 with
fixtures, 10 + the same 23 SKIPs without). 3 new open items
counted + 1 process rule; PR-150 item (6) closed.

STATUS after PR-159 (the row-cursor question CLOSED as a law,
and a new engine law counted): [[rec]+4] has NO incrementer -
its complete writer set is a transition match (0x53616, ver-
batim from the authored (N), skipped-when-equal at 0x53612)
or the CLONE/ctor initialization at 0x300d7-0x300e6 (the
"state restart" reading was wrong; it is object creation,
class pair 0x9c05c). Row progress lives in the threshold/
accumulator walk (PR-100/101) and never reads the cursor, so
the cursor's base IS the authored 1-based (N): PR-157 F4's
consumer-6 row resolves to the `3,6` line and PR-158 item 2
closes. The transition pipeline itself raises AC_TIME: self-
transition matches counted in [obj+0x9d] against WORD [state-
record+0xc] set live-set bit 9 (0x53626-0x5365c) - the port's
frame-list approximation was directionally right; the exact
gate waits on the [rec+0xc] authoring decode. [DECODED by
PR-160 F2/F3: the time= directive pair + a rand() roll at
ENTER; zero time= lines in ZARIA.01 keep every leg un-
changed; the exact roll-and-count law is fully specced for
a CODE pass.] The rows[] bind-
ing helper (0x53480/0x53440) PROVES the state-index space
1-based (rows[0]=NULL, init=first param entry): the ladder
gate's unit reading stands, and the fi-vs-state decision is
now purely data-blocked on the param-token map. The PR-152
hand-off scan is a ONE-SHOT cursor compare (0x50be2), which
corroborates the whole law; its base-class gate stays FALSE
at T5 so no port action is due. One tension stays honestly
open: the walk's threshold [entry+0x10] vs the transition-
entry layout - the [cls+0x18] body is PR-160's first read.
ZARIA.01 output: UNCHANGED (comment-only ll_level.c edits;
full gates pass - 56 with fixtures, 10 + the same 23 SKIPs
without). 5 new open items counted + 1 narrowed; PR-158 item
2 and the F4 base question closed.

STATUS after PR-160 (the AC_TIME question fully CLOSED with the
directive parser decode): the engine's state-block DIRECTIVE
layer is read end to end - time= is the authored min/max pair
that ENTER turns into a random AC_TIME match-threshold via the
Borland rand() LCG; seq= registers a block into the global
name pool and s= inserts a named-sequence row; control= is
enumerated verbatim (21 flags) and the parser cites source
file names. The record model is decoded: class tables (base
0xa25d4 / LADDER 0x97c9c), per-class record factories via the
+0x88 verb, ctor defaults (time_scale 1000 = 1x), and the
rows vector indexed BY THE ROW CURSOR - the walk verb, the
[entry+0x10] threshold gate on the [obj+0xa1] accumulator
(the pre-scale call is the frame-advance dispatcher that
FEEDS it - PR-100's mechanism, now named), and the PR-142
latch writes all sit in named verbs. ZARIA.01: authored texts
carry zero time= lines, so AC_TIME never self-raises there -
every ZARIA.01 leg stands; the port's frame-list approxima-
tion is now replaceable by the exact roll-and-count law (Next
names the fully-specced CODE pass). Honest gaps counted: the
row-line parser for plain rows (what fills rows[] from au-
thored `N,T` lines - the last piece of the row model and the
decider for the 21-byte layout and the WAIT-key question),
the walk-arm attribution, and derived ENTER overrides. ZARIA.
01 output: UNCHANGED (docs-only PR, nothing compiled). PR-159
items 2/3 close (half of 3), F4 closes; 3 new items counted.

STATUS after PR-161 (the row model is CLOSED end to end): the
row-line parser is found (param-entry verb +0x14 = 0x51eb0 via
driver 0x52b80) - row lines are ID,THRESHOLD pairs resolved
through the sequence pool into 0x18-byte wrapper entries appen-
ded to [rec+0x2e]; the walk verb is SINGLE-SHOT per tick (via
object-verb +0x44 at 0x53b18) and the image has NO row-cursor
incrementer - rows advance exactly and only by authored transi-
tion row-outputs (PR-159's writer law with a negative proof);
an unresolvable row ID means an EMPTY KEY that always passes
the intersection test - the WAIT law - so its threshold gates
the accumulator every tick and matches keep counting toward the
AC_TIME roll (PR-159 item-3 closed). The transition-vs-row la-
yout tension dissolved (two distinct arrays) and the two-VIEW
class tables (0xa25d4/0xa260c) explain every earlier slot dis-
crepancy. The placement ctor ENTERs the initial record at
0x539fb - the AC_TIME roll fires at spawn, the last detail the
specced CODE pass needed. ZARIA.01: rows register no pool names,
so the port's threshold-gated row model stands corroborated,
pending the pool-contents read (now the NAMED edge of the K_
GEN1 data blocker). ZARIA.01 output: UNCHANGED (docs-only PR,
nothing compiled). PR-159 item-(3) + PR-160 items (1)/(2)
closed; 4 new items counted; one earlier phrasing corrected.

STATUS after PR-162 (the row model is fully corrected and the
control= flag word is fully decoded): PR-161's "no incrementer
exists" was the wrong grep idiom - the helper verb 0x520c0
continues at 0x5211e into a row-step tail (step 0x521d4/0x521
d7, wrap+|=4 at 0x521c1-0x521c5, reverse 0x521a1-0x521b1 via
SEQ_REV bits paired with position sentinels, mode-6 skip), so
ORDINARY row progression is advance-on-threshold-pass through
the helper the walk calls on every paid row: the port's model
(obj_row_tick advancing fi on threshold passes; PR-100/101's
measured law) is now CORROBORATED end to end, while the tran-
sition applier's authored-row-output SETS stand unchanged.
Both walk arms reach the gate (conditions are key-merge book-
keeping only at this level) - every row's threshold gates pay-
ment+stepping in practice, so the practical WAIT law stands.
The COMPLETE 21-name flag map is verbatim now (window [rec+
0x12..+0x15]; BY_X has NO bit; SEQ_REV_X/Y = 0x10/0x20 con-
firming PR-95 exactly; composites CHECK_ALL=0x1c0, SOLID2=
0x20400, SEMI_SOLID=0x40400; the +0x15 bit-0x8 that PR-160
called face-flip is the ANTI_POCISK bit - PR-108's flip beha-
vior stands, its bit attribution reopened). The AC_TIME CODE
pass spec is UNCHANGED and is PR-163. 5 new open items, most
importantly the mechanism boundary: where the transition ap-
plier is invoked from (the port's per-row transition model de-
pends on it). ZARIA.01 output: UNCHANGED (docs-only, nothing
compiled). Baselines 71/72 held; ledger census run.

STATUS after PR-163 (THE AC_TIME LAW IS NOW PORT CODE, and
ZARIA.01 output is UNCHANGED): the time= pair lives as
time_lo/time_hi (parse_pair pairs single values exactly like
the engine scanf leaves max unset=min); obj_enter zeroes the
match counter and rolls gate = lo + (hi-lo)*rand/32767 with
the Borland LCG verbatim; obj_transition's SAME-STATE arm -
the port's copy of the engine's match arm at 0x53626-0x5365c -
counts matches and raises LL_SIG_AC_TIME (bit 9) exactly when
gate && count >= gate; no time= = gate 0 = never. ENTER runs
at all four state-change sites mirroring the engine: a LAZY
placement ENTER at the first scripted tick (the 0x539e6/0x539fb
ctor ENTER), obj_transition's change arm, the death force, and
the A_ATAK force. The unified ENTER is now stated IN THE CODE:
record-table slot +0x1c (0x52360) is ONE body - PR-107's row-
set half and PR-160's gate half - and the port splits it the
same way the applier reaches it (ll_mov_row_start + obj_enter).
CARRIED CODE COMMENTS FIXED: the row step 0x521d4, wrap+OR-4
at 0x521c1-0x521c5, reverse via SEQ_REV_X/Y 0x10/0x20, and
the brk || approximation is now labeled as exactly that (the
raise is additive). obj_actime_selftest adds FIVE hand-derived
legs (fresh-object LCG stream 0, 21468 pins the roll: gate
(2,50)=2, the single-value deterministic gate, lerp(1,10) at
r=21468 = exactly 6; the state change itself proves the raise,
since the consuming line is tested at brk=0; the death force
clears a live gate+count). MUTATION BATTERY 11 mutants: 9
killed; 2 SURVIVORS STATED - M10 cross-match counting (unob-
servable by construction: the change ENTER zeroes) and M11
(A_ATAK-force ENTER unreachable with current content). DIVER-
GENCE DOCUMENTED: per-object LCG state vs the engine's single
0x76bc6. 6 open items counted, the in-walk-visibility one
merged with PR-162's mechanism boundary = PR-164's read. Gates
ALL RUN: make + make win zero warnings, ll_level relinked,
mov/play selftests rc=0, verify-all BOTH modes unchanged (56
/ 10+23), no over-79 added, baselines 71/72 held. CI note:
PR-160/161's cancelled runs re-issued via gh run rerun; PR-
162's run confirmed green.

STATUS after PR-164 (the mechanism boundary is CLOSED, with a
ledger correction to PR-162): PR-105/106 ALREADY had the whole
chain - slot +0x14 (two views) -> wrappers (LION BASE 0x550a0:
zero [obj+0xe6]/[obj+0xea], call 0x536f0) -> pump 0x536f0 =
key getter/merge, WALK 0x523f0 at 0x5371c on the current re-
cord of [[obj+0x50]+[obj+0x5c]*4] (the object's live state
vector+index), LOOKUP 0x53520 (live-key copy + 0x52000 walk),
APPLY at 0x5374b, hooks around - and PR-162 item 5 re-opened
what was closed. Tonight's independent byte reads CONFIRM it
and add two facts. (1) The applier's ENTRY is 0x535a0 (four-
push prologue 51 56 57 55 at 0x535a0, own four-pop returns) -
"0x535a2" is the objdump misalignment, the one-byte trap class
again; three port comments cite it and ride the next code PR.
(2) The applier's arms CONFIRM PR-163's shipped law VERBATIM:
cross path zeroes [obj+0x9d] WITHOUT counting (M10 is engine-
wrong), runs the new record's +0x1c (unified ENTER+row-set -
re-confirming PR-163 F1) and writes [obj+0x5c]=idx (the state
change); same-index path counts even when the authored row is
already current (0x53614 je into the count arm - the WAIT-law
count confirmed), and the arm is EXACTLY [obj+0x9d]+=1; gate
= WORD [rec+0xc]; skip iff gate==0 or gate>count; else [obj+
0x61] |= 2 = live-set bit 9 = LL_SIG_AC_TIME. The port's ob-
ject-state model (o->sti/o->act_*) is now structurally GROUN-
DED ([obj+0x50]+[obj+0x5c] = the engine's state vector+index).
apply-before-walk vs walk-before-apply reduces to TWO unob-
servable-now differences (the vtable+0-word OR 4/8 consumer;
in-call raise visibility) = PR-163 item (1) narrowed; the port
shape stands. 4 new items (word consumer, 0x53520 arg plumbing,
[obj+0x95] reader, 31-vs-24 slot delta); PR-162 item 5 CLOSED
(bracketed in its record). ZARIA.01 output: UNCHANGED (docs-
only). Baselines 71/72 held; census run; PR-160's rerun green.

STATUS after PR-165 (the flag bits get their CONSUMERS, and
the largest one was hidden behind a WORD read): the exhaustive
byte-test census of [rec+0x12..+0x15] finds ONLY the SEQ_REV
pair (0x52158/0x52171 + inside ENTER at 0x52385/0x523a0 -
third corroboration of PR-95/162) and CHECK_LEW/ANI/ZW inside
the key builder - but the BY_* bits' real consumer is the
FEED DISPATCHER the record step calls at 0x52317: owner slot
+0xa0 (0x543a0) re-derives the current record, loads DWORD
[rec+0x12], and,7, five-way jump table 0x54380 - mode 0 = NO
FLAG = BY_X feeds [obj+0xa1] += abs([obj+0xca]) (PR-98's
|v_x| law VERBATIM), mode 1 = BY_Y feeds abs([obj+0xce]) (a
VERTICAL twin the port does not model), mode 2 = BY_TIME runs
a [[obj+0xa1]+0xc] deref + jle full-feed (open semantics -
the headline new item, touching the ladder-gate block), mode
3 = BY_TX is the guarded composite ([obj+0xba]) exactly as
PR-162's map read it, mode 4 = BY_TY adds a POOL-RUNTIME
constant rate [ds:0xcef40+0x388] (static 0, set at startup),
mode >=5 incl. FRAME_REV bit 3 = NO feed. FRAME_REV has zero
consumers anywhere; DANGER/CH_DIR still none - the port's
labels for those stay PORT-DERIVED semantics, now labeled as
such. NO port behaviour change indicated: the BY_* modes work
exactly as the port's feed models (the enum stays port-
internal). SECOND: THE KEY BUILDER IS RECORD SLOT +0x08
(0x52750 -> verbs +0x2c/+0x30 + owner getter +0xc8 -> flag
merge +0x34 = 0x52790, PR-162 item 4 CLOSED): CHECK_LEW/ANI/
ZW run POOL QUERIES (0x4bd30/0x4b890/0x4b870 over pools ds:
0xcef40/0xa94ec/0xcef44, 0x8000 CHECK_ALL_PLANE sub-mode)
ORed into the 13-byte key + count byte - CHECK_* rows ADD
KEY BITS, they do not gate. THIRD: DERIVED ENTER/LEAVE
OVERRIDES ARE WRAPPERS (0x4fef0/0x50040 tail-call the base
0x52360; 0x50060 wraps 0x52660) - the port's single obj_enter
core stands for derived classes. FOURTH: THE POOLS' STARTUP
REGISTRY (0x4fe02: 0x5c320/0x51120 with class ids + obfus-
cated names at 0x9209a.. - decode open) feeds the very glo-
bals the queries use - the pool-contents read is PR-166's
first target. One-byte listing trap found again (0x4b890 ->
0x4b88f). 6 new open items (BY_TIME deref semantics, pool
name decode, 0x4b500 body, +0x2c/+0x30 bodies, the 0x4fef0
pool side-effects, the second-view slots); PR-162 items 1
and 4 CLOSED with brackets. ZARIA.01 output: UNCHANGED
(docs-only). Baselines 71/72 held; census run; no code built.

STATUS after PR-166 (the pool subsystem is READ THROUGH and it
is the level's own placed objects, not an abstract sequence
store): the CHECK_* key queries hit CONTAINER-ATTACHED POOLS.
The loader names them at load by the PR-60 source keys - 'zwy
erzaki'/'animacje'/'ani_plat' comparisons at 0x4cd6c-0x4cdca
store the container's pool INDEX [container+0x40] into ds:0x
cef44/0xa94ec/0xa94e8, and the container object IS the pool
array base (its own cleanup uses the identical layout at 0x4c
e0a-0x4ce24; ds:0xcef40 at query time = the current contain-
er). QUERY BODIES CLOSED: 0x4b500 = chain-head walk ([poolOb
j+0x44] count, +0x58 heads, [entry+0x34] next), predicate v
erb +0x60, CLASS FILTER [entry+0x24]==[owner+0x24] with the
pushed 0x8000 CHECK_ALL_PLANE cross-class switch (confirmed
meaning), key-provider verb +0x78, OR-merge via 0x49580 into
the 13-byte key; 0x4bd30 (CHECK_LEW) = the OWNER's own list
at [obj+0x64], INTERSECT (0x495b0 idiom) + copy-out, no OR.
Class id field CONFIRMED at [obj+0x24]. Wrappers 0x4b870/0x4
b88f = stack-arg adapters (0x4b890 listing misaligned again)
. The STARTUP REGISTRY is fully transcribed - 14 type-0x14 p
ools, class ids 6,6,11,(11),7,3,5,12,2,9,8,10,11,(11), name
s 0x9201c-0x920e4, handles ds:0xcefbc..0xceff0 - and NONE o
f those handles is read by any code (negative): the registra
tion is startup plumbing; the QUERYED pools are the load-tim
e ones. Pool names: decode still open, negatives banked (no
 single-byte XOR works; pairs position-independent; block si
ts right after the readable LEVELS.DES keys + 'ANITAB'..'BE
RLO' factory-name table). NAME-KEYED ACCESS REAL: 0x4dcc0(ds
:0xcef40,name) -> 0x4df10 ('ANITAB' special case creates ty
pe 0xa5) -> factory clone path stores the result at [obj+0x1
20] (PR-104's prototype cache); the row resolver's result sl
ot ds:0xceff4 abuts the registry globals - ds:0xcef40..0xcef
f4 is ONE pool-singleton block, so PR-161's sequence pool an
d the container pools are ONE subsystem. ZARIA.01 rows still
 resolve NULL - the K_GEN1 data block STANDS but is precisely
 narrowed (the key-path pools need no names). SIDE FACTS: po
ol manager singleton at [container+0x394] (verb +0xd8 regist
ers, +0xe4); BY_TY feed rate = [container+0x388] (setter un
read - narrows PR-165 item 1); 0x4b600/0x4b6ba = the ani_pl
at pool position/velocity scan (platform-ground lead, caller
s 0x546d7/0x54719). 6 new open items (headline: WHO inserts
 into the container pools + the entry class carrying verbs +
0x60/+0x78 = PR-167; name decode; 0x4dcc0 contract; 0x4b600
; ds:0xcef58/0xcef40 writers; manager slots). PR-165 item 3
CLOSED, its F3/F5 corrected with brackets. ZARIA.01 output:
UNCHANGED (docs-only). Baselines 71/72 held; census run; no
code built.

STATUS after PR-167 (the pool subsystem's last leg closes at the
class-verb level: the pool entries ARE the game objects): the
walk's verbs are the OBJECT CLASS TABLE's own slots - 130/155
class tables carry 0x23b80 at +0x60 (`return [entry+0x2c]`:
membership = the word at [obj+0x2c]) and +0x78 is a three-
variant key-provider family (default 0x552e0 = clear a 13-byte
local, gate via 0x53ae0, gate via class verb +0x9c, build via
0x55230, emit EXACTLY 12 bytes + count byte). THE BASE OBJECT
CLASS TABLE IS AT 0xa2550 (71 slots): every generic verb the
PR-162..166 call-site readings predicted sits at its predicted
offset (class-id getter 0x23e00 at +0x08, the walk at +0x14,
ENTER 0x52360 at +0xa0/+0xd8, base LEAVE 0x52470 at +0xe4);
the 0x23bxx bodies are one-instruction accessors. THE ADDER IS
THE PER-CLASS VERB +0xd8 CALLED FROM FACTORY CHAINS AT LOAD -
no-op (0x40f90) for most classes, real bodies (0x3def0 family,
which PROPAGATES registration to related objects) for the
pooling ones: PR-60's placement-category model exists at the
VERB level. FIELD IDENTITIES PINNED from the ENTER body (ver-
batim 0x52361-0x523d0): [obj+0xa1] = the feed accumulator and
ENTER ZEROES IT (PR-163's gate/counter clear gains its engine
twin for the feed side); [obj+0x99] := 1 at every ENTER (the
writer of the long-carried sentinel); the SEQ_REV pair plus
BOTH row-cursor stores (reversed (row+8)-edx at 0x523c5, plain
edx at 0x523cb) - PR-162's row-cursor law CONFIRMED inside
ENTER. The +0x1c=0x52360 census = exactly 4 tables: PR-100's
two named views + 0xa2e9c/0xa2ed4, already in PR-100's 16-
table census - what remains is the view-SELECTION code, not a
missing class. One open half stays honest: the chain-link/
head STORE is still uncensored (constructors init +0x34=3; no
direct chain store found) - the real +0xd8 bodies are the
lead. 5 new items (chain store; 0x53ae0/+0x9c key internals -
where the 0x600000/0x800000 CHECK_LN constants MUST appear, or
PR-161's constants get re-derived; the base table's own +0x1c/
+0x20 overrides 0x52c70/0x52a00; the 25-class +0x60 minority;
[obj+0x99] reader semantics). PR-166 item 1 CLOSED with a
bracket; its F3 bracket strengthened. ZARIA.01 output: UN-
CHANGED (docs-only). Baselines 71/72 held; census run; no
code built.

STATUS after PR-168 (the feed law is CLOSED end to end, and a
treasure of misalignments falls): the three "distinct" BY_*
feed arms are ONE arms - modes 2/3/4 are byte-identical feeds
of [container+0x388] into the accumulator [obj+0xa1] (BY_TX/
BY_TY pre-guarded by cmp [obj+0xba]/[obj+0xbe],0;je), and
the long-quoted "[obj+0xa1]+0xc dereference, cmp +0xc, jle
full-feed" NEVER EXISTED: one-byte objdump split of `mov eax,
[eax+0x388]` at 0x543fb (seventh listing-trap member). THE
RATE IS A LITERAL 100/TICK (sole writer 0x4c8b7 `mov [x+
0x388],0x64` at container init). THE GUARDS ARE THE STATE'S
OWN MOTION PARAMETERS: [obj+0xba]/[obj+0xbe] = PR-98's physi-
cs fields, loaded by the gate 0x549f0 from the param block
[obj+0xd2] words 0/1, clamped by the 0x54a57-0x54b47 family
(literals 10/-10/0 against param-block compares), zeroed at
0x54e2e/0x54e38 - so BY_TX/BY_TY = the constant feed while
authored motion is live. The consumer side re-confirms PR-
98/162's accumulator model as the engine's (read at 0x52323,
written at 0x52118, zeroed by ENTER, fed by the dispatcher).
KEY-BIT MECHANISM FOUND: the second main key-provider family
(+0x78 = 0x54860 in 99 tables, paired with +0x9c = 0x549f0)
turns the PARAM SIGNS into key bits (`cmp [obj+0xbe],0; jge
-> or BYTE [key+4],0x20`, jle -> 0x40, plus a 32-bucket bit-
map loop - full map open, and STILL the last place the
0x600000/0x800000 CHECK_LN constants can live); 0x54a24 com-
pares [paramblk+0xc] against [obj+0x6d] the FACING - the
flip-bit question finally has a named lead, and the port's
DANGER/CH_DIR behaviours now have a named engine counterpart
(param-sign key bits, not flag bits). The ladder-gate block
NARROWS again: with the deref hypothesis dead, the gate is
PURELY fi-vs-state parameter selection (still data-blocked -
do NOT touch the fi gate). One narrow audit opened: whether
the port's BY_TIME row-gating is observationally equivalent
to a constant-100/tick feed (its fixtures have '1,100/2,100'
rows). 4 new items (the audit; the 0x54860 full map; the
clamp family; +0x9c variants). PR-165 item (1) CLOSED with
its F2 text corrected in place; PR-166's carried mention
closed. ZARIA.01 output: UNCHANGED (docs-only). Baselines
71/72 held; census run; no code built.

STATUS after PR-169 (the predicate gets its producer; the
chain hunt turns out to be a VERB-DISPATCH hunt): the member-
ship field [obj+0x2c] is written by the generic accessor
0x23b70 - `call the class verb +0x08; mov [obj+0x2c],eax` -
so the 130-copy predicate 0x23b80 is a NULL-TEST of a COMPU-
TED class-verb product, not a state flag (PR-167 F2a bracket-
corrected). The base +0x08 verb (0x23e00) is a bounds-contain
ment compare of the object's [0]/+4/+8 rect - PR-162's rect-
angle identity re-confirmed as a live verb. THE CHAIN-INSERT
CENSUS IS EXHAUSTIVE AND NEGATIVE for displacement stores (a
full-pattern sweep of +0x34/+0x58 direct/indexed/lea forms):
the container's heads-array alloc is the only +0x58 store;
+0x34 stores are constructors, non-object structs, and class-
table installs at [obj+0x64]. Yet chains are walked and prov-
ably allocated - because THIS ENGINE LINKS THROUGH VERBS:
0x53db0 calls a neighbour's verb +0x14 with edx=2 (its LEAVE)
then relinks [x+0x54]. PR-170 = the pool manager's method
table ([[ds:0xcef40]+0x394]+0x8) and the 0x5c320 pool-object
class - where list maintenance actually lives. THE +0x60 MI-
NORITY IS MAPPED (PR-167 item 4 closed): 12 of the 25 are
constant-true `return 1` class families (always matchable,
never pooled - the 0x26xxx ENA/KADMAR cluster among them);
the rest conditional (carried). The 0x507e0 +0xd8 body is a
13-way jump managing a global counter ds:0xa94f0 clamped to
1..9 - registration verbs count, they do not chain: the two
mechanisms are separate (the 14 startup type-0x14 pools are
name-registered data pools = PR-161's family). 3 new items
(manager verbs; the [obj+0x2c] producer contract - 0x23b70 is
itself only reachable as a verb body; the conditional mino-
rity) + PR-167 item (1) re-scoped with a bracket. ZARIA.01
output: UNCHANGED (docs-only). Baselines 71/72 held; census
run; no code built.

STATUS after PR-170 (the pool "manager" loses its mystery - it
IS the level controller): [container+0x394] is written by the
container's own construction, nine sequential level-ctor at-
tempts at 0x4de22-0x4dea4 (one per PR-104 level family). The
TLewZaria ctor 0x1fef0 compares the name, builds via the sha-
red 0x5c320(0x12e)+0x1e0a0 and installs TWO class tables
(0x96960/0x96958; the two-table layout of PR-95) - the pool
manager is an ordinary game object, the level controller. The
mythical "+0x68 pool-handle getter" = 0x23900 `add eax,0xd6`
- a pointer to the object's own +0xd6 pair, which the ctor's
reset block (0x54e02) fills as a POSITION SNAPSHOT ([obj+0]/
+4 copy) and initializes the whole motion state (+0xad := 100,
+b1 := 22 = PR-98's defaults!). Two generics pinned in pass-
ing: 0x23920 = current-record flag getter (the [[obj+0x50]+
[obj+0x5c]*4]+0x12 idiom as a REUSABLE VERB), and 0x238f0 =
the boolean membership writer for [obj+0x2c] - PR-169's mem-
bership field has two producer shapes (computed handle via
0x23b70, boolean via 0x238f0). THE JOIN IS A HANDSHAKE: con-
troller second-table +0xd8 = 0x50ed0 (guard: [obj+0x11a] not
6/7, [obj+0x126] clear, both verbs +0x40 pass; captures the
participant's +0xc id and +0x70 position into [ctl+0x10a]/+
0x10e) then calls the game-class +0xe4 (0x50300: sets the 30-
tick busy field [obj+0x102], read by 0x502f0) - registration
counts/times out, it does NOT chain, exactly as PR-169 fore-
cast. The walk loop is now verbatim-pinned (count +0x44, head
slot [[x]+0x58], next [entry+0x34], predicate/class-filter/
self-skip/order as PR-166 said) - the law stands; the chain-
STORE hunt is re-pointed, not closed: the verb +0x10 family
(the 0x54ed6 call with the 4-dword 0/0x1b arg block) and the
shared ctor 0x1e0a0 are the remaining candidates, and they
sit INSIDE the key-builder code - so PR-171 merges the key-
bit map (PR-168 item 2, the last place for the 0x600000/
0x800000 CHECK_LN constants) with the +0x10 census. New li-
fetime items: the +0xd6 snapshot readers (0x54efa delta math
vs [obj+0x89]/[obj+0xda]) and the +0x102 30-tick timeout (a
CONTACT-work lead). 1 bracket: PR-169's manager-sentence cor-
rected. ZARIA.01 output: UNCHANGED (docs-only). Baselines
71/72 held; census run; no code built.

STATUS after PR-171 (the key law is complete at the bit level,
and three roles fall): THE MAIN KEY BUILDER IS AT 0x54912
(ninth one-byte-trap member - PR-168's "0x54900" was inte-
rior; byte-level proof: padding+ret at 0x54905, fresh pro-
logue at 0x54912). COMPLETE BIT MAP: [obj+0xa5] state-flag
bits -> key[+4]|0x80, key[+5]|0x01, key[+6]|0x20; nonzero
motion param [obj+0xba] -> key[+4]|0x08 or |0x10 selected by
status bit [obj+0xb6]&0x10 (the `sete; add al,0x23; idiv 32`
one-index "bitmap" idiom); param-sign bits key+4|0x20/|0x40
ONLY when [obj+0xba]==0 (key+4 = 0x60 exactly at zero). THE
99-TABLE +0x78 VALUE 0x54860 IS A KEY CONSUMER, NOT A PRO-
VIDER: it decays [obj+0xc6] by -2/3 (idiv) when the walked
key bytes say so - and those counters [obj+0xc2]/[obj+0xc6]
are SET/ADDED/ZEROED by the CONTROL-MESSAGE verb +0x10 =
0x54560 (87/155 tables; switch on [block+4] id: facing
writes id<=5, counter arms, status-bit ORs [obj+0xb6]|4/8):
messages load counters, the key walk decays them - one closed
loop. PR-170's +0x10 chain-store lead is DEAD but paid: its
0x54eb2 caller sends msg {0,0x1b,0,0} then RE-ANCHORS posi-
tion from the [obj+0xd6] snapshot pair (pos := arg - snapshot
+ [obj+0x89]/[obj+0xda] - the ride/re-anchor law; PR-170
item (2) first half ANSWERED). THE "CLAMP" FAMILY IS A DEAD-
ZONE POSITION CONTROLLER (PR-168 item 3 CLOSED): param mode
[pb+4] (1=x/4=y), threshold [pb+0xc] vs facing setne*15 -
the long-carried flip-bit test 0x54a24 is now NAMED - and a
+-5 window vs the snapshot forces the motion params to 10/-
10/0. DECISIVE NEGATIVE: 0x600000/0x800000 exist ONLY as
byte-table misalignment artifacts - no code immediate uses
them: PR-161's CHECK_LN/BASE_LN "constants" MUST be re-de-
rived (labeled hypothesis: key-BYTE sequences 0x60/0x80 in a
hex-dump reading = exactly F1's bit map). NEW MYSTERY, TOP
LEAD: the 99 family-B classes have NO statically-referenced
key builder - 0x54912 has zero table slots, zero calls, zero
branches (triple census). POOL LOAD PATH READ TO ITS END AND
ENDS EMPTY: heads array allocated+zeroed at pool-slot assign-
ment ([poolObj+0x58] = 0x76a64 buffer, the 0x4cdf8 store
CONFIRMED as alloc - an awk range had hidden it), [poolObj+
0x44] := count, [c+0x40] increments per name group (SLOT
COUNTER - PR-166 F2 refined); runtime insert remains the one
open chain-store item (third re-point: +0x10 minority bodies
0x3dcb0 x27, unread msg arms 0x5460e/0x5463a, the family-B
producer hunt). 0x52070 RULED OUT (ENTER preamble). 2 new
items (family-B reachability; msg-id map + [obj+0x102] read-
er); CLOSED with brackets: PR-168 items (2) and (3); PR-170
item (2) half; 5 brackets total. ZARIA.01 output: UNCHANGED
(docs-only). Baselines 71/72 held; census run; no code built.

STATUS after PR-172 (a real BY_TIME divergence found and fixed
- the audit PR became a code PR): THE FEED AND GATE LAWS WERE
ALREADY RIGHT (100/iteration, hold-while-below, subtract-carry);
what the port lacked is the engine's ZEROING of its one feed
register [obj+0xa1] at EVERY state change - the applier (entry
0x535a0) calls the new record's ENTER at [[rec+0x44]+0x1c]
(the class tables install the 0x4fef0 wrapper there; its body
calls the row-set method 0x52360 at 0x4ff6b - a call, not the
tail call PR-170 said - and 0x5236b writes [obj+0xa1] := 0),
while the same-state match arm writes rec+0x4 inline (0x53616)
and calls no ENTER, so a match CARRIES the register. Row-write
and zeroing are ONE method; registers are fresh at every state
entry. OBSERVABILITY PROVEN ON AUTHORED DATA: seven of the
twelve distinct plain-BY_TIME gates in ZARIA.01 (1/40/75/80/
120/150/225 across 1128 rows) are not multiples of 100 - mid-
row exits leave non-zero remainders. The port zeroed facc but
not row_t at its applier arms; `o->row_t = 0;` now ships at
BOTH (keyed transition + forced A_DEAD), and the row_t/facc
split remains observationally identical to the engine's single
register (mode-fixed feed within a state + fresh at entry).
NEW TEST obj_rowzero_selftest (hand-derived, gates 70/240/120/
150): five targeted mutants killed at their exact legs (no-
zero-at-transition, zero-at-match, no-zero-at-death, no-sub-
tract-remainder, zero-at-wrap); one honest survivor (M6, zero
row_rem too - the ms carry has no engine counterpart and is
always 0 at an applier in this fixture). THE PR-164 RIDE IS
PAID (0x535a2 -> 0x535a0 at the three comment sites). CLOSED:
PR-168 item (1) (the BY_TIME audit). NEW ITEM: the BY_TY mode
feed is unmodelled (4 shipped states; the 0x543a0 mode arm is
the lead). UPDATED: the self-target row item (engine writes
the authored row on every same-state match - the object path
counts but never re-seeks; candidate code PR after the entry-
token census). ZARIA.01 output: the pinned frame comparisons
ALL pass UNCHANGED with the fix in. Gates: zero warnings both
platforms, verify-all 56 (ok) / 10 (ok)+23 SKIP rc=0 both
modes; baselines 71/72 held.

STATUS after PR-173 (the KEY-LITERAL re-derivation CLOSED the
literal hunt and UNIFIED the key with the signal set): the
0x919ec block is fully dumped — 73 NUL names ending at 0x91c68
(PR-88's 0x91c40 was short): 4 non-bit node-op specials, then
EXACTLY 68 signal names in the port LL_SIG_* enum's exact
order, then the 'AC_EV_MAX' sentinel; bit N = block index N+4,
machine-verified 68/68 against the port enum AND the port's
name->enum token table (complete, zero mismatches — PR-88's
table was sampled, now total). KEY = TEventSet (3 dwords +
count at +0xc; 0x5533c stores it, 0x49580 ORs the dwords):
row keys, condition masks, the live set and built keys are ONE
vocabulary. The pump 0x536f0 verbatim: class verb +0x8c = the
self key GETTER, its key ORed into the live set [obj+0x60]
before the 0x523f0 chain-walk merge — transition matching
tests live ∪ getter ∪ chain-key. Slot censuses (marker
+0x60=0x23b80, 178 tables): +0x8c holds 0x54910 x115 — PR-
171's "0x54912, zero table slots" was the listing trap plus a
search for the mis-parsed value (bytes at 0x54910 = 53 51 56
57, the prologue itself; tables store 0x54910) — plus the
base 0x52750 (PR-165's CHECK_LEW/ANI/ZW pool-query merge,
slot-identified), 0x23a60 x38, 0x23a20 = &[obj+0x75] (a
SECOND 13-byte set beside the live set), TLewZaria's 0x504e0;
the 99 family-B tables pair 0x54860/+0x9c=0x549f0 EXACTLY
99/99 and carry 0x23870 = bool [obj+0xa9]!=0 at +0x8c — a
SECOND contract, call-sites open. 0x54910 DECODED through the
block: [obj+0xa5] bits -> K_FIRE/K_ALT/K_JUMP; [obj+0xba] ->
K_BKWD/K_FWD; [obj+0xbe] sign -> K_UP/K_DOWN (0x60 = idle
both) — THE ENGINE SYNTHESIZES ITS INPUT-KEY VOCABULARY FROM
MOTION STATE: K_* tokens are AI-addressable motion tests. The
strict authored-data census: ZARIA.01's K-input-family lines
(118) live ONLY in the two LEW (lion) scripts — the port's
player path feeds those from real input, and no ZARIA.01
object path reaches any synthesized bit: NO port change (the
obj_cond_true default-false comment stays honest). The
0x600000/0x800000 "CHECK_LN constants" are CLOSED as pure
ledger artifacts: git-proven zero occurrences in PR-161, first
appearance PR-166's Next unrecorded; the REAL flag->key
bridge is named (CHECK_LEW/ANI/ZW pool queries at 0x52750;
ANTI_POCISK 0x80000 / DANGER 0x2000 tests in 0x55230 — the
entry-key language is relations: contact via 0x53ae0, push-
resistance, danger, [x+0xad] ordinals, always ORing AC_ENEMY).
5 new counted items (+0x8c contracts' call-sites; [obj+0xa9]/
[obj+0xad]/[obj+0x75] readers; per-class +0x78 singleton
family incl. slot-identified 0x28af0 = the bit-54 raiser;
KADMAR camel K_* reachability at its stage; 0x504e0/0x551f0).
ZARIA.01 output: nothing executed differently (comment-only
edit in ll_play.h). Gates: zero warnings both platforms,
ll_level re-linked, mov-selftest rc=0; baselines 71/72 held.

STATUS after PR-174 (the SAME-STATE ROW SEEK shipped for the
object path): obj_transition's same-state arm now opens as the
engine's does - it writes the authored start row into the cur-
rent state's row counter BEFORE the PR-163 count legs (0x5360f-
0x53616: store only when it differs; the guard's other half is
the +0xac verb call, unread, counted), calls no ENTER, and the
feed register stays carried (PR-172's law, mutant M7 pins it
again). The mapping is the shared ll_mov_row_start - the fourth
call site of the one rule (N>=2 -> N-1; "(1)"/absent/over-long
-> the top, all already settled for this path by PR-107). OBSER-
VABILITY (census, C tool linking ported/ll_play.c - PR-107's
method, numbers reproduce PR-107 EXACTLY: 2767 lines, 92 absent
+ 24 "(1)" + 101 seeks, 12 over-range): the 101 split LION 77 /
SCENE 24 (KADMAR.07 19, KADMAR2 3, LONA.05 2); ZARIA.01 has
ALL 20 in the lion's ZAR_LEW/ZAR_LEWPL and NOT ONE self-target
line in any ZARIA.01 scene script - shipped ZARIA.01 object out-
put is provably unchanged, and the verify-all fixtures (56 ok,
byte-identical) confirm it. The fix is law-completion: it is
what makes the 24 KADMAR/LONA-class lines behave at their T6+
stages. TEST: obj_rowseek_selftest, 7 hand-derived legs (seek
lands and is the LIVE row; empty token re-seeks to the top from
row 3; "(1)" = top; over-long clamp; row-write-only with the
carried 150 opening row 2's gate from 250; cross-state PR-107
regression through A_TWO's (4) line). MUTATIONS: 6/7 killed -
no-write, raw-store, skip-empty, no-clamp, gate-on-AC_TIME,
zero-register (M2/M7 also die in PR-172's obj_rowzero_selftest
- cross-coverage); the SURVIVOR is the engine's own equality
guard (observationally equivalent; the unmodelled half is +0xac
- counted item 1). THE PLAYER SIDE STAYS OPEN with its full
data (item 2): the 20 lion lines (A_BORRING(10), A_DEAD(33) x2,
A_FALL_DOWN(3), A_FG_PACKA(17), A_FIGHT(20), A_JUMP(11),
A_JUMP_S(9), A_KILOF(13)/(25)) key on AC_TIME/AC_DUP/|K_ANY/
K_FIRE,K_GEN2/AC_RECT,K_GEN1 - the player path models none of
those signal legs, and lion row changes re-open T2/T3 animation
claims, so PR-86's keep-running stays shipped until the signal-
side model + fresh frames run (the applier is class-independent
- PR-107's fixed record table 0xa25d4 - so the engine DOES re-
seek the lion's rows: this is a real divergence, bounded and
listed, not hand-waved). ZARIA.01 output: nothing executed dif-
ferently. Gates: zero warnings both platforms, ll_level re-
linked, mov-selftest rc=0 with the new test in the chain, ver-
ify-all rc=0 both ways (56 ok; 10 ok + the same 23 named SKIPs);
baselines 71/72 held.

STATUS after PR-175 (the KEY-FOLLOW-UP INTERLOCK PASS; docs
reading - ZERO port files touched, so T5's shipped behaviour
is EXACTLY PR-174's, and ZARIA.01 output is unchanged by
construction): what the pass settles FOR the port's model:
(1) the engine's per-object state-step verb is slot +0x14,
and the key pump 0x536f0 runs for exactly 105 of the 178
marker-anchored classes through it (0x550a0 x79 wrapper + the
26 direct + nothing else; 0x550b4 is the only direct pump
call in the image) - the port's KEY/AC_* vocabulary has a
named, counted producer set, and LION (0x96960, +0x14 =
0x550a0) is pumped every tick exactly as PR-153 assumed;
(2) the 99 "family-B" tables are a SEPARATE, passive lineage
(+0x14 = 0x23920 returns the record's control-flags word;
their +0x60 is never the 0x23b80 marker) - PR-173's joint
presentation corrected, so no port reading of the pump/boolean
"two contracts" was ever at stake; (3) the +0xac verb the
PR-174 equality guard skips is a live-KEY-SET extension hook
(edx = &[obj+0x60], never the row) - PR-174's shipped row-
seek model stands unchanged, its one unmodelled half re-
scoped and partially read (0x546a0 head: pool-query + live-
set merge + [obj+0xa9] RMW); (4) [obj+0xad] IS the energy
counter PR-89 named (0x5516a compares self vs other; the
winning branch builds the AC_ENEMY_ATC key, the losing one
AC_ENEMY) - the port's energy field and the engine's key-
building relation verb are the SAME field, which grounds the
port's atc windows (PR-89/obj_rowzero law) on real engine
semantics; (5) [obj+0xa9]'s site map is complete (msg-id
write 0x54624, pool-query feed 0x4b64f, animal family, zero-
ers; semantics open), [obj+0x75] is pointer-only through the
0x23a20 getter - and 0x23a20 is slot +0x4c's default (the
carried FLAG-getter item, slot-identified). Twelve LION-
shape controller classes (the 0x504e0 getter) are census-
listed for PR-176. Nothing claimed here changes a shipped
behaviour: all three carried items it closes were model-
blocking questions, and every closure says so. Gates: docs-
only (PR-138..171 precedent); added lines <= 79; baselines
71/72 held; PR-174's binary+verify state stands (56 ok / 10
ok + 23 named SKIPs).

STATUS after PR-176 (the CONTROLLER-GETTER pass; docs
reading - ZERO port lines changed, the ZARIA.01 output image
is byte-identical to PR-175's, and the build state remains
PR-174's green binary with verify-all rc=0 both ways).

What this pass GROUNDS for T5 (no shipped behaviour changed,
so no new test was required - the reading's payoffs are
confirmations and exclusions):
(1) The controller getter CONFIRMS the key model end-to-end:
    its jump table builds bits 41..52 and those are EXACTLY
    K_F1..K_F12 in PR-173 F5's name block and in the port's
    LL_SIG_* enum - a second, independent confirmation of
    the bit->name law (on top of the 68/68 row census).
(2) The getter = signal-ordinal key OR the 0x54910 motion
    key, merged via 0x49580 into the standard +0x8c buffer
    the pump 0x536f0 already reads - the port's pump-side
    model needs no change for controllers.
(3) THE TWELVE 0x504e0-carriers are IDENTIFIED: the six TLew
    stage-lion classes (TLewZaria -> 0x96960 - THE ZARIA.01
    lion's class), five per-stage death-module classes, and
    the shared parent 0x97970. The lion object the port
    models belongs to a named, enumerated family - no unknown
    class remains among the controller-getter tables.
(4) The KADMAR camel rows are EXCLUDED from the controller
    lineage (CAMEL -> 0x98278, a decor class in a third
    table format with no +0x14 pump and no +0x8c getter in
    any read map) - the camel K_* row mystery narrows to one
    decor-class format question, still T6+ data-blocked; the
    port's ZARIA.01 output is unaffected either way.
Five new open items are counted in the record (five unnamed
TLew classes; [obj+0xee] >= 10; the queue producers/verbs;
the button-bit map; the switch-array source + decor format);
two closed (PR-175 F6 twelve-identity; PR-173 item (e)).
Gates: docs-only (PR-138..171, PR-175 precedent); added lines
<= 79; baselines 71/72 held; PR-174's binary+verify state
stands (56 ok / 10 ok + 23 named SKIPs).

STATUS after PR-177 (the RELATIONSHIP-KEY PROVIDER pass;
docs reading - ZERO port lines changed, the ZARIA.01 output
image is byte-identical to PR-176's, and the build state is
the OWNER GATE of 2026-11-19: make clean + parallel
verify-all rc=0 both ways, 52 (ok) fixtures / 10 (ok) +
18 named SKIPs fixture-free).

What this pass GROUNDS for T5 (nothing shipped changed, so
no new test was required; the payoff is that every shipped
raise rule now has its engine counterpart named):
(1) THE GENERATED SIGNALS ZARIA.01 ACTUALLY USES - K_GEN1
    (KAMIEN1-3, BALONIK, DRABINKA, KATAPULT, KORONA),
    AC_ENEMY_ATC/AC_ENEMY, AC_BLOK (ZAR_LEW) - all flow
    through the class +0x78 provider slots, driven by the
    base +0x8c getter 0x52750 (gated by [rec+0x12]&0x8000,
    new fact tying the record-flag law of PR-159/160 to
    signal raising), the pump's +0xac hook (LION's 0x546a0
    calls the bounds test 0x4b430 and the pool query
    0x4b600), and child aggregation 0x553e0.
(2) PR-175's energy/AC_ENEMY reading is REPRODUCED inside
    the providers themselves (AC_ENEMY always ORed,
    AC_ENEMY_ATC on the contact arm) - the key law holds
    end to end, now with the raising code named.
(3) The census is CORRECTED (9 shared x122 + 56 singletons
    inside the 178, disjoint from the 99 passive lineage -
    the THIRD conflation of those two populations closed by
    bracket), so any future claim about "+0x78 families"
    has exact numbers.
(4) The decor tables from the camel item (0x98278/0x98274)
    are REFERENCED INSIDE the provider code (0x3d49d,
    0x28b47, 0x4b441) - the camel reachability item
    updates again, still data-blocked, now with concrete
    code contact points.
Five new open items are counted in the record (relation-
helper bodies; unread provider tails; the other 55
singletons; the array/field producers; entry-boundary
honesty); one closed (PR-173 item (3)).
Gates: docs-only (PR-138..171, PR-175, PR-176 precedent);
added lines <= 79; baselines 71/71 held; the Windows half
stays in the end pass (rule 8).

STATUS after PR-178 (the BY_TY FEED pass; docs reading -
ZERO port lines changed, the ZARIA.01 output image is byte-
identical to PR-177's, and the build state is the OWNER
GATE of 2026-11-19 measured on 40cf915: make clean +
parallel verify-all rc=0 both ways, 52 (ok) fixtures /
10 (ok) + 18 named SKIPs fixture-free; only docs have
moved since).

What this pass GROUNDS for T5 (nothing shipped changed):
(1) THE HIGHEST-CARRIED PORT-SIDE QUESTION IN ZARIA.01 IS
    CLOSED AS NO GAP: the 4 BY_TY states ([A_WSP]/
    [A_WSP_LIN] of ZAR_LEW/ZAR_LEWPL) are fed by the SAME
    constant 100 the port feeds (byte-exact at 0x5441b/
    0x543fb), and the port's player path has modelled the
    guard since PR-148. The object path needs no BY_TY arm:
    no authored ZARIA.01 non-lion state uses the mode.
    The PR-148 intent-proxy guard STANDS unchanged - this
    pass found no data contradicting it; what would pin
    the real gate is the buffer filler (counted item 1).
(2) THE FEED UNITS ARE PINNED: walking states are fed by
    |[obj+0xc2/c6]|/100 - 1/100-pixel units, exactly the
    port's facc law; time modes by the 100-per-iteration
    constant, exactly the port's row_t law. The PR-109
    unit measurement (the on-screen 200/300 ladder-clip
    rows) now has its engine mechanism named end to end.
(3) THE ZARIA.01 LION CLASS IS TABLE 0x96960 (of the 15,
    all sharing +0x14 = 0x550a0, +0xac = 0x546a0, +0xa0 =
    0x543a0, +0x78 = 0x50f80) - every future ZARIA.01
    claim about "the lion's code" now has an address.
(4) The climb gate's real meaning is a VERTICAL WINDOW
    against the attach object ([obj+0xd2] kind-4, [obj+
    0xbe] = +10/-10/0 vs [d2]+0x1d) - if a future pass
    pins the buffer filler, the object-path guard becomes
    a code-PR candidate with a hand-derived test.
Five new open items are counted (buffer filler + [obj+
0x106]; the [obj+0x11a] mode field; 0x549f0 tails + [obj+
0x71] readers; the +0xa0 census oddities; 0x55070's chain);
one closed (PR-172 item (1)).
Gates: docs-only (PR-138..177 precedent); added lines <= 79;
baselines 71/71 held; the Windows half stays in the end
pass (rule 8).

STATUS after PR-179 (the LION +0xac / msg-map / identity
pass; docs reading - ZERO port lines changed, the ZARIA.01
output image is byte-identical to PR-178's, and the build
state is the OWNER GATE of 2026-11-19 measured on
40cf915: 52 (ok) fixtures / 10 (ok) + 18 named SKIPs
fixture-free; only docs have moved since).

What this pass GROUNDS for T5:
(1) PR-150'S AC_DUP LADDER-OVERLAP MODEL (green on frames
    since PR-150) NOW HAS ITS ENGINE MECHANISM NAMED: the
    pump's +0xac verb runs an attach state machine that
    raises AC_DUP (bit 4) on candidate transitions and
    AC_DUP_CD (bit 13) when the re-attach lands with
    [obj+0xe2] = 0 - and the candidate field is cleared
    EVERY STEP by the LION pump itself, so the engine
    recomputes overlap continuously, exactly what the
    port's per-step overlap model does.
(2) THE AC_BLOK RAISER (PR-177 F1's bounds test 0x4b430)
    is CONFIRMED to be MERGED INTO THE SAME KEY BUFFER by
    +0xac - the collision-signal path is now complete:
    bounds test -> merge -> row gating.
(3) THE MSG SURFACE IS PINNED: velocity SET (id 2) and
    IMPULSE (id 13) and facing SET (id 6) are the channels
    the game uses to move objects from the OUTSIDE - the
    future input-side work (PR-174 item (2), the msg
    senders) now has its protocol; and the lion's velocity
    feed (PR-178) has its external writer named.
(4) The energy field [obj+0xad] has its DEFAULT (100) and
    its reset site (0x54e00) named - the PR-175 energy
    law's population story gains its starting value.
No shipped behaviour is contradicted; the +0xac body's
only unresolved shape is a 38-byte CFG window in the
re-attach path (counted). Five new items are counted, one
lead NAMED for the next pass (what fills [obj+0xe6]);
three carried items closed or updated; 0x55070's slot
corrected to +0x54.
Gates: docs-only (PR-138..178 precedent); added lines <=
79; baselines 71/71 held; the Windows half stays in the
end pass (rule 8).

STATUS after PR-180 (the producer pass for the feed-gate
words; docs reading - ZERO port lines changed, the ZARIA.01
output image is byte-identical to PR-179's; build state =
the OWNER GATE (rule 8) measured on 40cf915, only docs
moved since).

What this pass GROUNDS for T5:
(1) THE FEED-GATE STORY IS NOW MECHANICALLY COMPLETE ON
    THE ENGINE SIDE except one dispatch: sensor word ->
    +0xcc (0x50340: zero-guard + [obj+0x11a]==5 negate) ->
    0x549f0 (assign + attach-window re-decide) -> mode-2/
    4 gate -> row feed. For the LION the sensor word is
    an ANCHOR-DELTA ratio (PR-180 F1) - the exact formula
    is in the record; its behavioural reading (does the
    port's intent-proxy ever diverge?) is video-gated and
    counted, like the standing ladder-gate item.
(2) PR-178 ITEM (1) CLOSED - the buffer filler is named
    (the class step driver pattern), so the carried
    "feed gate unmodelled" question is no longer a port
    gap; it is a data-blocked behaviour study.
(3) THE CANDIDATE MECHANISM IS WIRED-BY-ADDRESS (PR-180
    F3) - PR-150's ladder-overlap AC_DUP law (engine half
    named by PR-179) now has its last open half pinned
    down to ONE specific function (the 0x2c6d3 level-init
    wiring) that PR-181 will read, together with the
    carried pool-name decode. Same pass, two carried items
    converge - that is why PR-181 is specced as the pool-
    wiring pass.
Four new items are counted (the +0x240 dispatch; the
ratio-law behaviour study; the other drivers' bodies; the
state-copy helper + the -0xd6 structure family); one
closed (PR-178 item (1)); one re-scoped (PR-179 item (1)
-> the pool-wiring lead).
Gates: docs-only (PR-138..179 precedent); added lines <=
79; baselines 71/71 held; the Windows half stays in the
end pass (rule 8).

STATUS after PR-181 (the pool-wiring pass; docs reading -
ZERO port lines changed, the ZARIA.01 output image is
byte-identical to PR-180's; build state = the OWNER GATE
(rule 8) measured on 40cf915, only docs moved since).

What this pass GROUNDS for T5:
(1) PR-150's PER-STEP CANDIDATE MODEL NOW HAS ITS ENGINE
    COUNTERPART NAMED: the relationship-nearest selector
    (0x45400 via wrapper 0x45320, called by the provider
    bodies) picks the best candidate from the rel-list by
    distance and stores it into [obj+0xe6]+[obj+0xea]+
    [obj+0xe2] EVERY step after the pump's clear - i.e.
    the engine re-selects nearest-overlap continuously,
    exactly what the port's PR-150 model (green on frames)
    does. The attach/AC_DUP gates are CLASS-BYTES from
    the descriptor (+0xc), the PR-172 layout - the value
    map (which byte = ladders/ropes) is counted open.
(2) THE POOL-NAME DECODE CLOSED: the level-file grammar
    block (0x91e01) carries the 31 object-family names -
    THEY ARE EXACTLY the authored .MOV stems - so "pool
    X" = the class-table set for family X, and the
    membership/energy rules key on class-ID ranges. This
    directly connects the level-data family names to the
    class-table families the whole ledger names.
(3) THE CAMEL-FAMILY placement method (0x2c670) explains
    PR-180's false lead: the candidate is NOT wired by
    address; PR-180's F3 is corrected with data (the
    0x2c6f0 +0xe6 is arithmetic, the globals private to
    that one function).
Three carried items closed or closed-in-part (pool-name
decode; the [obj+0xe6] writer for the relationship
family; PR-177 provider items in part - the chain is
supply-tracked); PR-180 F3 corrected; 5 new items
counted (slot-owner/dispatch ambiguity incl. the +0x240
family; the class-byte value map; the list-builder +
distance internals; the energy class-range rule; the
camel placement model).
Gates: docs-only (PR-138..180 precedent); added lines <=
79; baselines 71/71 held; the Windows half stays in the
end pass (rule 8).

STATUS after PR-182 (the msg-sender/event pass; docs
reading - ZERO port lines changed, the ZARIA.01 output
image is byte-identical to PR-181's; build state = the
OWNER GATE (rule 8) measured on 40cf915, only docs moved
since).

What this pass GROUNDS for T5:
(1) THE SIGNAL-RAISING LAW IS NOW ENGINE-VERIFIED:
    raising an AC_* signal = the broadcast (0x4bc40)
    enqueuing an id-16 event whose arg is the KEY BIT
    NUMBER, and the consumer (0x53770) setting that bit
    (word N>>5, bit N&31 - the port's PR-88 numbering
    EXACTLY) into the live set [obj+0x60] + the merge
    helper. The port's key model (ll_play.c raising
    AC_BUTLA/AC_BECZKA etc., PR-88/PR-9x) is the same
    mechanism under a different transport.
(2) THE PER-STEP ORDER IS CONFIRMED: events are drained
    through class slot +0x10 BEFORE the step verb +0x14
    (the LION pump of PR-179) - the port's order (state/
    signals -> collide/select -> move) is frame-aligned
    with this; PR-179's per-step candidate clear now has
    its full frame.
(3) AUTHORED-LEVEL EVIDENCE: the camel-family classes
    RAISE AC_BUTLA (27) / AC_BECZKA (28) at their record
    arms - the exact signals the authored ZARIA .MOV
    state lists listen for - closing a class of "where
    do the authored signals come from" questions.
No port code change (the model already matches); PR-179
item (4) CLOSED (msg senders + the 87-slot census); the
msg-sender half of PR-181 item (1) resolved; PR-179 F2's
'generic 0x53770' UPDATED with data. 5 new items counted
(the 66-site id/arg values; the LION consumer 0x1e130 +
0x52bb0 bodies; the identity-getter semantics of the
0x8138764 magic; the [obj+0x28] gate + the global
registry; the [rec+4] guards).
Gates: docs-only (PR-138..181 precedent); added lines <=
79; baselines 71/71 held; the Windows half stays in the
end pass (rule 8).

STATUS after PR-183 (the event-consumer body pass; docs
reading - ZERO port lines changed, the ZARIA.01 output
image is byte-identical to PR-182's; build state = the
OWNER GATE (rule 8) measured on 40cf915, only docs moved
since).

What this pass GROUNDS for T5:
(1) THE AUTHORED SIGNAL SOURCES ARE NOW ENGINE-FOUND:
    the camel-family classes raise AC_KOREK (33, one-
    shot), AC_BECZKA (28) and AC_BUTLA (27) at their
    mode/record arms via the token-filtered broadcast -
    CORK/BARREL/BOTTLE, the exact authored props of the
    camel scenes; the raise law (bit N into the live set,
    PR-88 numbering) is fully engine-verified end to end
    (sender method -> 0x4bc40 -> 0x52b10 node -> class
    slot +0x10 -> 0x53770 bit-set + merge). The port's
    direct bit-raising is the same law minus the token
    indirection - no port change.
(2) THE ENERGY FIELD [obj+0xad] HAS ITS COMPLETE WRITER
    SET: default 100 (PR-179), the 0x54458 class-range
    -1 rule (PR-181), and NOW the FEED EVENT (id 9):
    +32 clamped to 100 after a live-key-set merge. The
    port's feed=constant stands: no literal id-9 sender
    exists (item (4) counts whether any ZARIA.01 event
    could carry one).
(3) THE PER-STEP INTERACTION FRAME IS COMPLETE: anchored
    objects ignore move-by (id 1) events; impulses/
    move-by events fire the +0x54 hook family; the pump
    gate [obj+0x28] is event-id-14-set; the attach law
    (event 27 = LEAVE/ENTER pair over [obj+0xa9]) is the
    PR-179 attach machine's event half. The PR-150-era
    interaction model (green on frames) matches all of
    it; PR-174's row-seek model precondition is now
    complete (frames stay the only blocker).
PR-179 item (2) PART-closed (bit setters = ids 3/4);
PR-182 items (1)/(2)/(3) closed; PR-177 item (1) helper
0x52cc0 closed; two label CORRECTIONS shipped as data
brackets (PR-179 F2 map, PR-182 F2 raise map). 5 new
items counted (id-17/18/20/25/29/30 consumer families;
the pair semantics; the [rec+4] guards + token-owner
family; the id-9 sender; the 0x116/0x108 setters).
Gates: docs-only (PR-138..182 precedent); added lines <=
79; baselines 71/71 held; the Windows half stays in the
end pass (rule 8).

STATUS after PR-184 (the consumer-census/token correction
pass; docs reading - ZERO port lines changed, the ZARIA.01
output image is byte-identical to PR-183's; build state =
the OWNER GATE (rule 8) measured on 40cf915, only docs
moved since).

What this pass GROUNDS for T5:
(1) THE SIGNAL-DELIVERY REACH IS ENGINE-SETTLED: named
    raises (the camel trio) are UNFILTERED broadcasts -
    every object in both level chains ORs the bit into its
    OWN live key set; the port's per-object signal model
    (PR-88 bits, PR-15x listening states) matches exactly;
    TOKEN filters exist but are used by targeted family
    messages (the id-7 send/consume pair found end to end:
    0x1e4xx/0x1e570-cluster providers -> 0x1ec90/0x1f110
    consumer families).
(2) GROUP SEMANTICS NOW HAVE THEIR ENGINE MECHANISM: the
    0x55720 family forwards events to CHILDREN ([obj+0x3c
    /0x40]) and the 0x3dcb0 family forwards to a LINKED
    object ([obj+0x112]) - the ZONE/GRUPA-style authored
    group objects propagate state through the event bus,
    not through shared state; counted for per-level use.
(3) THE CENSUS IS A STABLE BASIS: 41 +0x10 implementations
    over 178 tables with the two base vocabularies (PR-183
    F2 maps) + per-family intercepts - future family work
    (the PR-177 item (3) singletons, the class-byte map)
    now keys off this census.
SELF-CORRECTION SHIPPED WITH DATA: three brackets replace
PR-183 F3's token story (tokens are 2..136 small ints from
the static base 1; 0x8138764 is a constant STAMP; the
filter argument is EDX). No port change (the corrected
model still matches the port's direct bit-raising). 5 new
items counted; PR-183 items (1)/(3), PR-177 item (3),
PR-176 items (2)/(3) part-closed/extended.
Gates: docs-only (PR-138..183 precedent); added lines <=
79; baselines 71/71 held; the Windows half stays in the
end pass (rule 8).

STATUS after PR-185 (the controller-is-the-LION pass; docs
reading - ZERO port lines changed, the ZARIA.01 output image
is byte-identical to PR-184's; build state = the OWNER GATE
(rule 8) measured on 40cf915, only docs moved since).

What this pass GROUNDS for T5:
(1) THE INPUT PATH IS ENGINE-SETTLED END TO END: hardware
    state dword -> [obj+0xa5..a8] (the 0x54910 tail copy,
    per motion-synth call) -> the 11-bit getter chain +
    queue gate -> merged synthesized+signal key set for the
    LION's state machine. The port's ll_play_keys channel
    corresponds to the synthesized-keys half EXACTLY (PR-
    173 F4 legs); the signal-ordinal half (K_F*) is level-
    authored by gate words the port need not model (ZARIA's
    are authored ON; no ZARIA.01 row consumes an ordinal-
    only signal). NO port change.
(2) THE CONTROLLER = THE STAGE LION is now the grounded
    model for the per-level 'director': the 12 tables are
    PR-176 F3's twelve LION classes; their vtables carry
    the relation/selector machinery (PR-181), the sensors
    (mgr+0x394 tracked-object distance), and the event-
    queue block (PR-176 F2). T5's LION work (feed, energy,
    modes) now has its full class map.
(3) THE EVENT ALPHABET IS CLOSED: every id ever observed
    sent now has a NAMED consumer or is dissolved (id 30;
    ids 17/29 -> the 0x988ec/0x9d338 controller tables;
    id 5 is the ONE LION controller send, sender := its
    [obj+0x6d] link). PR-183 item (1) CLOSED wholly.
SELF-CORRECTION SHIPPED WITH DATA: 1 bracket extends PR-
184's id-25 census (8th arm 0x2e8e0 via the deeper form;
the 83-form count was form-partial - law unchanged). 5 new
items counted (LION flag family/state block; input-source
identity; per-level gate values; the 2 wrapped classes'
names; the id-13/15/21/23 arms); PR-176 items (2)/(4) and
PR-184 item (1) closed; PR-176 (5), PR-177 (4), PR-184 (2)
part-closed.
Gates: docs-only (PR-138..184 precedent); added lines <=
79; baselines 71/71 held; the Windows half stays in the end
pass (rule 8).

STATUS after PR-186 (the LION-flag/input-bottom pass; docs
reading - ZERO port lines changed, the ZARIA.01 output image
is byte-identical to PR-185's; build state = the OWNER GATE
(rule 8) measured on 40cf915, only docs moved since).

What this pass GROUNDS for T5:
(1) THE INPUT MODEL IS ENGINE-SETTLED AT THE BOTTOM NOW:
    device-manager sample rings (0xcf400/0x58690) -> frame
    block -> the +0xcc INPUT-APPLY chain, with the LION
    wrapper 0x50340 gating input OFF per object
    ([obj+0x106] == 0, the PR-104/106 re-arm field) and
    mirroring controls in mode 5 ([obj+0x11a] == 5). The
    port's keyboard->ll_play_keys channel remains equivalent
    at the key-bit layer. NO port change.
(2) THE LION STATE BLOCK (mode [obj+0x11a] + flags [obj+
    0x12a/0x12e/0x132] + queue flag [obj+0x106] +
    [obj+0x116] + authored signal list [obj+0x136]) IS NOW
    A CONNECTED MACHINE: state rows raise signals via the
    +0x90 verb on the LINKED object; signals 9/10/0xb/0xc
    and modes 9/0xa have read semantics; the authored list
    pushes kind-4 nodes onto the PLAYER's queue (the PR-147
    kind-4 contact node now has a PRODUCER) and zeroes the
    player's input via [tracked+0x106]. T5's LION rows can
    now be read against this machine.
(3) T5 LEADS FOR THE ENERGY LAW (both video-gated, nothing
    changed): signal 10/mode 0xa drains energy by 30, and
    the lion FACTORY authors initial energy 500 while
    0x54e00 resets to 100 (third writer; PR-179 F4
    extended). The start-of-level HUD number decides.
(4) LIFE/DEATH CLASS PAIRS share one object (flag block
    survives the 0x988ec -> 0x9d338 class switch) - the
    death-double law now has STATE semantics, not just
    table pairing.
SELF-CORRECTION SHIPPED WITH DATA: 1 bracket fixes PR-185
F2's function attribution (input-write tail = the 0x549f0
input-apply verb + the 0x50340 wrapper, as PR-106 already
knew; 0x54910 ends at 0x549e6) - the data-flow conclusion
and the port verdict STAND. Closures: PR-185 items (1)/
(4); PR-176 item (5) as a law; parts of PR-185 item (3),
PR-106 item (1), PR-179 F4. 11 new items counted (the
non-controller wrapper class 0x9a54c; the class-record
header pair law; the capture hook; ...).
Gates: docs-only (PR-138..185 precedent); added lines <=
79; baselines 71/71 held; the Windows half stays in the end
pass (rule 8).

STATUS after PR-187 (the hardware-bottom/controller-family
pass; docs reading - ZERO port lines changed, the ZARIA.01
output image is byte-identical to PR-186's; build state =
the OWNER GATE (rule 8) measured on 40cf915, only docs
moved since).

What this pass GROUNDS for T5:
(1) THE INPUT STACK IS NOW TRACED END TO END FROM SILICON:
    port 0x201 PPI one-shot axis timings (against the 8253
    counter, `in eax,0x50`) + the 0xcde70 scancode layer ->
    0x57fe0 sample pulls -> per-driver rings -> 0x58690
    frame read -> the +0xcc INPUT-APPLY chain with the
    0x50340 gate. The port's keyboard channel remains
    equivalent at the key-bit layer. The engine's own
    player input is JOYSTICK-shaped (analogue axis
    timings + button byte) - a T5 reading lead, not a port
    change (ZARIA playback uses the synthesized keys).
(2) FLAG/ORDINAL DOUBLING: at the Kadmar class the getter
    rewrites the key ordinal from the signal flag ([ee] :=
    flag+2 when ee >= 3) - T5's K_F11/K_F12 leg reading
    now has a second, flag-driven producer. Which flags
    raise it to >= 8 is a counted item.
(3) THE +0x84 OWNER HOOKS ARE STATE-NAME RECORD CTORS
    (A_DEAD/A_LOOKUP/A_LOOKDOWN -> sub-table stores):
    T5's state-name rows now have their construction law
    read, including the 0x60 record-type gate.
(4) CORRECTION THAT TOUCHES T5's DEATH READING: the
    flag/state block is PER-OBJECT; the PR-186
    life/death-class-pair sharing claim is wrong as
    written (0x988ec=ZARIA-death vs 0x9d338=RAN-death).
    Where T5 needs state survival across a class switch,
    the mechanism is RECLASS-ON-OBJECT (0x2bfc2-style
    [obj+0x8/0xc] rewrites) or a NEW controller OBJECT
    (the Kadmar factory form) - both with their own flag
    initializations (-1/1 vs 0). Death at ZARIA.01 is
    unaffected (no reclass sites read there yet).
SELF-CORRECTION SHIPPED WITH DATA: 1 bracket corrects
PR-186 F1's pairing claim; extensions at PR-176 items
(1)/(2), the +0x84 owner-hook paragraph, PR-106 item (1)
and PR-186 items (4)/(7)/(9). Closures: PR-186 items (7)
(TLewKadmar1's class), (9) (alias, no header), (4) to the
HW bottom. 9 new items counted (the 0x585e2 entry +
[dev+0xb] state law is FIRST - it decides whether the
sample rings are live input or recordings, PR-106's old
question).
Gates: docs-only (PR-138..186 precedent); added lines <=
79; baselines 71/71 held; the Windows half stays in the
end pass (rule 8).

STATUS after PR-188 (the input-stack completion pass;
docs reading - ZERO port lines changed, the ZARIA.01
output image is byte-identical to PR-187's; build state =
the OWNER GATE (rule 8) measured on 40cf915, only docs
moved since).

What this pass GROUNDS for T5:
(1) THE INPUT LAWS ARE CLOSED AS A SYSTEM (PR-106 item (1)
    CLOSED): 0x58690 runs capture (live port read with
    the +/-20 calibration steps), run-length recording and
    demo playback off [dev+0xb]; input RECORDS are 16-byte
    RLE records ([rec+0xc] = repeat count). For T5 this
    means: the demo legs of the level flow are FILE
    PLAYBACK of authored records (names built with
    '%s\%s'+'%d'), and live play is the state-0/1 chain -
    the port's key-bit channel stays the correct shim
    either way.
(2) THE KEY->FLAG LAW FEEDS THE ORDINAL QUESTION (PR-187
    item (1) CLOSED): the signal flag is fed by LIVE KEY
    WORDS [obj+0x60..0x67] bits (a Kadmar arm) and by a
    verb-3 THROW COUNTER - not by a button combo; the
    31-store census shows no flag >= 8, so the decorated
    getter's >=10 arm has no static flag producer: T5's
    K_F11/K_F12 leg keeps ONLY the PR-185 wrapper path.
    T5's key-bit reading gains the word layout lead
    (0x64.02/0x66.40/0x67.08 named at the consumer).
(3) THE 'ANY KEY DOWN' BIT: [block+8] bit 8 comes from
    the 0xcde70 key-state table via 0x1c530(9); the
    0x808 sibling leg is DEAD CODE (stub 0x58550) - a
    future port must NOT emulate it.
(4) ds:0xa94f0 IS THE BOUNDED STAGE/PLAYER INDEX (its
    own writers clamp 1..9): the frame loop's input-push
    gate and the getter index read it. T5's stage rows
    get a named global; the death path's dec is named.
SELF-CORRECTION SHIPPED WITH DATA: 4 brackets - 3 fix
the PR-186 F3 / PR-187 F3+F4 ring-fill attribution (the
legs are in 0x58690, not 0x585e2; capture AND playback
are both real), 1 fixes PR-147's "replays the PREVIOUS
record / merges via 0x225a0" sentence (it is DEDUP +
capacity GROWTH; [rec+0xc] is the run count).
Closures: PR-106 item (1), PR-147 item (2)
(setter census), PR-187 items (1) (key->flag), (9) (the
316-store class census: 14 allocator-coupled stores, the
rest spawner-driven constructors), (6) half (0x1c530 read;
0x1c550 stays). 9 new items counted (the +0x100
pair/carry-verb field law and the 0xcf068 subsystem-
registry law are FIRST).
Gates: docs-only (PR-138..187 precedent); added lines <=
79; baselines 71/71 held; the Windows half stays in the
end pass (rule 8).

STATUS after PR-189 (the interaction-verb + subsystem-
registry pass; docs reading - ZERO port lines changed,
the ZARIA.01 output image is byte-identical to PR-188's;
build state = the OWNER GATE (rule 8) measured on
40cf915, only docs moved since).

What this pass GROUNDS for T5:
(1) THE INTERACTION VERB IS FULLY LAID OUT (PR-188 item
    (2) CLOSED): two dispatch sites only; the 0x41a50
    dispatcher is an INSTALLED message handler (0x9803c);
    partner selection runs through the [obj+0x104]
    target-lookup verb and a per-class AUTHORED fallback
    (16 ctor stores, never runtime); the default-partner
    global ds:0xa94ec is fed from the CONFIG PARSER
    ('animacje' gate). For T5: interactions are data-
    driven per class; the energy-copy trigger IS the 0x14
    init the port already models - no port change.
(2) THE SUBSYSTEM REGISTRY IS NAMED (item (3) CLOSED):
    UTF-16 entry names, the 'none'/'['/']' config-
    grammar registration set, the event post with
    EXACTLY 2 senders, the default handler pair 0x96200/
    0x961fc. The pump-vector CORRECTION (+0x44/+0x4c)
    fixes the ledger's frame-loop prose at 3 mentions -
    the demo pump law stands.
(3) THE FRAME GATE IS READ (item (4) CLOSED): [0x3a0]
    != 0 makes the frame loop SKIP world construction
    and go straight to the input push; [0x3b0] is a one-
    shot deferred request; [0x3a8]/[0x3a0] are an on/off
    command pair set by ONE command. T5's level flow
    gains the exact skip/defer semantics the demo legs
    hinge on, and the 'ANITAB' level chunk lands at
    [level+0x3b8].
(4) THE LIVE-KEY REGION'S WRITE LAW: NO direct stores -
    writes are pointer-mediated plus exactly TWO
    synthetic bit injections (the 0x41a50 distance leg
    ORs key word 1 bit 1; the id-17 record arm ORs 0x20
    into word 3). The port needs no extra synthetic-key
    channel: injections come from VERBS, not devices.
SELF-CORRECTION SHIPPED WITH DATA: 1 correction (the
'+0x48' registry-pump prose -> +0x44/count +0x4c,
bracketed at all three ledger mentions). Closures:
PR-188 items (2)(3)(4), item (5) PART (the final
pointer-mediated writer stays counted), PR-186 items
(2)(3)(5), PR-187 item (2)'s carried remainders, and 5
entries of the PR-188 carried list. 7 new items counted
(the [obj+0x160] selector author and the final key-byte
writer are first).
Gates: docs-only (PR-138..188 precedent); added lines <=
79; baselines 71/71 held; the Windows half stays in the
end pass (rule 8).

STATUS after PR-190 (the frame-loop + record-chain +
handler-vector pass; docs reading - ZERO port lines
changed, the ZARIA.01 output image is byte-identical to
PR-189's; build state = the OWNER GATE (rule 8) measured
on 40cf915, only docs moved since).

What this pass GROUNDS for T5:
(1) THE LEVEL-CHANGE CONTRACT IS EXACT (PR-147 item (2)
    CLOSED): the level loop is ONE function (0x4abd0)
    called once; it returns WHY the level ended (player
    flag / quit request / stage-zeroed / hotkey) and the
    world leaves an EXIT WORD in [mgr+0x4]; the caller
    stores both into its result block before teardown.
    For T5: level transitions are a CODE+WORD handshake,
    not a callback - the port's level flow stands; the
    loop SAMPLES INPUT EVERY FRAME and the demo pump
    runs ONCE at level end - the demo legs stand.
(2) THE LEVEL-PLAN SCRIPT IS A REAL ARTIFACT (new law):
    [mgr+0x30] is a character script run ONE CHAR PER
    FRAME ('A' spawn-request, 'L' entity tick, 'T'
    camera scroll, 'B' data leg; any other char prints
    'Bledny opis poziomu - plany' - a NAMED error). The
    camera law is exact (SET-SCROLL to player minus
    160/100 EVERY iteration). T5's flow gates and
    camera moves now have their engine counterparts;
    plan authoring (the file that fills [mgr+0x30]) is
    PR-191 item (1) - the last unknown of the level
    story.
(3) THE LIVE-KEY PRODUCER IS SETTLED (PR-189 item (1)
    CLOSED): verb results are OR-MERGED into the live
    words by 0x49580 (31 of its 69 callers target that
    region); the interaction SELECTOR [obj+0x160] is
    authored ONLY by its own class arm (0x41480). The
    port's key-bit channel remains the right shim; the
    interaction-dispatch class (0x97f50) is identified
    structurally, name pending.
(4) THE 'ANIMATION-TABLE' READING WAS WRONG AND IS
    CORRECTED WITH DATA: 0x98278 = the default bounds-
    method class (85 installs) and the 0x27170 leg is
    an OUT-OF-BOUNDS REQUEST MESSAGE against extent
    constants 0x143/0x18b - no table-index law to port.
SELF-CORRECTION SHIPPED WITH DATA: 3 correction
categories at 9 sites (the 'handler vector / installed
message handler' x2, the '0x49580 record append'
phrasing x5 - restoring the PR-88/100/101 OR-INTO law,
the 'animation-table' x2). Closures: PR-147 item (2)-
remainder, PR-189 items (1) and (2). 8 new items
counted (file-hotkey semantics + plan 'B'-leg first).
Gates: docs-only (PR-138..189 precedent); added lines <=
79; baselines 71/71 held; the Windows half stays in the
end pass (rule 8).

PR-191 (level-plan authoring + file hotkeys + class
identities) SHIPPED - DOCS ONLY, NO T5 BEHAVIOUR CHANGED:
(1) THE CLIMBING/INTERACTION CHUNK VOCABULARY IS NOW THE
    ENGINE'S OWN WORDS: the level-file tag set (31 NUL-
    terminated tags at 0x91dc0-0x92120 plus the referenced
    copies 'PLANT' 0x90755, 'ANITAB' 0x91f3d) includes
    LINA (rope), TABLICA (board/sign), KATPLAT (tilting
    platform), DZWIGNIA (lever), WODOSPAD (waterfall),
    ZONE, KORONA, TRASA/TRASA2 (routes), GRABOBJ/GRABG
    (grabbables), GWIAZDKI (stars), SURPRISE, NIESP -
    THE AUTHORED NAMES every deferred T5 chunk-format
    item must key off. THE TAG FACTORY'S SHAPE IS
    SETTLED: an strcmp chain (`mov edx,<tag>; call
    0x76a69`) + a size alloc (0x5c320) + a per-type ctor
    ('TRASA' -> 0x54 + 0x52860 called at 0x4e744; 'PLANT'
    -> 0xa5 + 0x531f0 at 0x1fca7), and TWO such chains
    exist (the 0x1fxxx/0x20xxx family + the 0x4e710 chain
    inside the class table 0x97f50).
(2) THE AUTHORED-OBJECT FAMILY IS STRUCTURALLY IDENTIFIED:
    class table 0x97f50 is installed by EXACTLY FOUR sites
    (0x3f570/0x44e5d/0x4573d/0x4e85b) that have ZERO E8
    and ZERO immediate references whole-image - the same
    computed-dispatch shape as the open 302-spawner split;
    the level manager (0xcef40) is an INSTANCE of this
    class (its +0xcc gate = 0x498f0, its pair test =
    0x4b120 = the table's own slots), so the LINA class
    chain PR-147/149 read sits in this family. Class NAMES
    stay counted; the tag->ctor map is PR-192's target and
    feeds the spawner-role split the T5 chunk work needs.
(3) THE PLAN MODEL CORRECTION CHANGES NO T5 OBSERVABLE:
    0x4af10 replays the whole plan script per call and
    the 'A' leg is an indexed authored-object tick (not a
    spawn), but the port's level logic (ladder/vine gates,
    BY_TY, the energy ladders) is untouched and its
    verdicts stand as PR-190 left them. The hotkey legs
    are .mnu menu scripts (the port's menu engine is out
    of stage scope); no input-model change was made.

PR-192 (tag-factory map + file model + .des/plan format)
SHIPPED - DOCS ONLY, NO T5 BEHAVIOUR CHANGED:
(1) THE AUTHORED-TYPE NAME SPACE IS NOW THE ENGINE'S OWN,
    MAPPED WHOLE: 16 factory chains / 361 tag arms ->
    three universal initializers (geometry 0x52860 /
    static props 0x531f0 / movers 0x53ff0) + per-type
    ctors with per-type class tables. THE LINA/LADDER
    FAMILY SITS EXACTLY WHERE THE T5 CHUNK WORK NEEDS IT:
    'LINA' 0xd9 + 'DRABINKA' (ladder) 0xd9 -> the SAME
    0x531f0 initializer; 'KDRABINA' 0xa5, 'WSPRECT' (the
    wall-support rect) 0x50 -> 0x52860; the zone classes
    (GRABOBJ/SPAD/TABLICZKALONA) share table 0x9815c.
    THE 302-SPAWNER ROLE SPLIT (the item every T5
    chunk-format PR carried) IS RESOLVED.
(2) THE DEFERRED CHUNK-FORMAT WORK HAS ITS SOURCE FILES
    PINNED: the authored-object descriptions live in the
    ANIMACJE archive keyed BY THE 31 TAGS (20/31 present
    as plaintext in the ISO member - LINA x10, BERLO x15,
    KATPLAT, DZWIGNIA, WODOSPAD...), while level DATA is
    the binary ETAPY\*.IS stream (ZARIA1.IS: zero
    plaintext tags). PR-193 reads both formats byte-wise;
    the 'levels'/.des chain ('plany=T|TALA|ATAB' = three
    plan slots; 'lew=TLew*' lion types) is the authoring
    contract the port's loader mirrors - no discrepancy
    found against the port's level model.
(3) NO T5 OBSERVABLE MOVED: no port file touched; the
    ladder/vine/BY_TY/energy verdicts stand as PR-191 left
    them; the exit-word writer stays counted (re-measured
    across the whole manager verb set).

(4) THE LAYER ABOVE THE LEVEL LOOP IS READ, AND THREE LABELS FALL
    (PR-193): the 13 per-stage wrappers give the engine's OWN stage
    order (zaria1, zaria2, ena1, ena2, lona, sades, kadmar1,
    kadmar2, ran, kara, junction, tunga1, tunga2 - ZARIA.01 first,
    the port roadmap); the driver 0x5d170 holds the ONLY call site
    of the level loop 0x4abd0 and stores the pair (loop return,
    [level+0x4]) into its caller's buffer; and the caller's split
    is now read byte for byte: 3 = call the reset pair (0x5da60 +
    0x5d050) and return 1 = run the same stage again, 2 = call the
    queue runner 0x5d2f0, which opens status.leo and runs ONE
    ds:0xcf464 queue entry through the stream run verb (returns 2
    if one ran, 0 if the queue is empty), 4 = return 0 = stop the
    chain, anything else = return 1 with no reset. Retracted by
    re-measurement on this
    machine: '0x5f310 = chunk interpreter' (it is the input flush +
    int-0x0b pump), '0x4be20 = the level loop' (it is the class
    +0x20 close verb, zero callers), '0xa2dd8 = the base stream
    class' (a verb vector). The 'levels' config chain is CLOSED as
    a named-entry lookup with the `Bledny wpis ` not-found leg.
    T6's engine-side contract is therefore WRITTEN (PR-193 F2/F3)
    and is PR-194's subject - a [PORT] change with its own test.
    No T5 observable moved here either: no port file was touched,
    so the ladder/vine/BY_TY/energy verdicts stand as (3) left them.

(5) THE LADDER THREAD, WHERE IT STANDS AND WHAT IS LEFT, IN
    ORDER (PR-310, after PR-308 and PR-309). The two routes this
    task named for years are now CLOSED as authored routes, both
    by measurement, so they are not steps: the ask path is right
    in this port (PR-308: the +0xb0 point verb hands the asker
    the live position), and no row of A_WSP / A_WSP_LIN /
    A_PODCIAG / A_DRAP authorises a rise while A_SCIAG authors
    its descent as six `m 0,1` steps (PR-309), and ZARIA.01's
    own DRAB.MOV asks for NO solidity (257 other scripts do).
    "Playable ZARIA.01" also has an authored definition now: the
    level ends through A_WPAD -> A_WYJSCIE, whose rows carry the
    only two `pos` directives in the shipped set (1020,692), and
    ZAR_LEW.MOV has no `end_level`. The y 489 of PR-286/PR-294
    stays a measured standing height, not a target. In order:

    F1. GROUND TRUTH FIRST [measure, no code]. Read the ladder
        out of the captures already on disk - R2
        `reference/video/ladder-DomCTOtx2rg.webm` t=46.0-49.0
        (the climb PR-109 timed), `reference/video/levels/
        ZARIA.01.mkv` t=13.3-15.7 (PR-131's "ridden down") and
        `reference/video/levels/ZARIA.01_b.mkv` t=200.0-200.7
        (mid-climb). Three questions: does the lion's feet line
        ever rise above the ladder's shape top (512), does the
        drawn ladder extend while he is on it, and what does the
        player press. Done when: a per-frame trace, or the plain
        statement that this ladder is a route DOWN, not up.
    F2. [read] the apply of the six directives that exit needs -
        `pos`, `lock`/`unlock`, `ldir`/`rdir`, `goto`/`GOTO_REL`
        - through the engine's own table 0x917c0 node classes
        (PR-198/199 read its `end_level` sibling 0xa1048). This
        is also the only mechanism left that could move a rider
        whose state authors no motion.
    F3. CLOSED AS A MIS-SCOPE (PR-316). Who writes
        [level+0x4] is OI-046, closed by PR-198/PR-199 - the
        end_level node's 0x45750 writes that exit word and the
        port implements the contract. The open half, kept as
        F3b: what that exit word drives, and what ends a level
        whose lion script has no `end_level` row at all -
        ZARIA.01 is one.
    F4. RAISERS NAMED AND COUNTED (PR-318); THE GATE'S INPUT STILL
        OPEN. Both halves of this step's question - who raises
        AC_ONAIR and AC_DRECT - are now tool output: AC_DRECT has
        exactly two constant-mask raisers in the image, 0x40d99
        (the `DRECT` class's ask verb 0x40d50, table 0xa1638 the
        placement factory installs by name) and 0x286ec (the
        `BICZRECT` verb 0x286a0), and both answer only when the
        asker's own geometry overlaps the instance's AUTHORED
        rectangle, the box the class's +0x18 verb parses from the
        placement line with the format at 0x91d00. ZARIA.01 is
        the ONE level in the shipped set that places a `DRECT`,
        and its rectangle (988..1340, 645..750) is the exit
        region of PR-317. What the step could not close is the
        gate's second condition, `[asker+0xc6] > 0` - OI-119 -
        and what the asker's +0x68 verb returns.
    F5. [read] the water as a floor (OI-082): A_WPAD and
        A_WYJSCIE choose `seq=wet`/`seq=dry`, and PR-317 read the
        ground itself: `zaria_a.plt` runs (953,654)(994,644)
        (1007,646)(1051,633) and then NOTHING until (1115,662), so
        the authored exit point 1020,692 sits in a 64 px HOLE,
        39-59 px under the terrain, with only the catch-all line
        (-100,1500)-(3000,1500) below it. ZARIA.01 DOES place
        WODA - ANI2.DES line 29, `WODA,woda.ani,woda.mov,955,651`,
        and the HD install carries ZARIA.01/WODA/WODA.MOV
        (PR-318, correcting PR-314's inventory) - so this is now a
        read of an object the level really places, and the port
        already places it; read the seven ZARIA_A.PLT polylines
        this port counts as moving ground too.
    F6. DONE FOR THE FIRST HALF (PR-317): the authored `pos
        1020,692` is parsed and applied as the engine applies it -
        an absolute set at the row admit, red test first, with the
        full no-Windows suite and its fixture-free half both rc=0
        (the last full run is PR-317's). The ly_exit leg itself is
        still open and waits on F4 and on the `lock`/`unlock` pair
        PR-316 read, since both exit states open with `lock`.
    F7. [gate] the standing end pass only (OI-096): the Windows
        legs once, at the end, never per PR.

**T6. Level flow.** `end_level` directive, `AC_LEAVE`, the vulture ride of
ZARIA I, the map -> level -> map loop, TUNGA I <-> II exchange
(`AC_XCHG_LEVEL`), lives/energy persistence, game over. STATUS.LEO progress is
blocked by open question 5 - ask the owner for a paired capture (menu before
and after finishing a level) or leave progress unimplemented and say so.
STATUS after PR-131: the ZARIA I end is now filmed in detail (the
level-end interaction scene with a purple-robed NPC + the grey-blue
bird, ball tosses included, levels/ZARIA.01.mkv t=440.3-457.3, the
ZARIA.02 cut at 458) and the vulture's mid-level sequences are
(zax37 t=306-315 and 385-413, on close view swoop-and-dodge, no
frame clearly shows the lion carried). Neither the NPC nor the bird
has a placement line, and the ZARIA.01 ANIMACJE tree carries NO bird
leaf (PTAK.MOV exists only in KADMAR.07/PTAK, TUNGA2.12/PTAKPLAT,
RAN.01/GPTAK_RA): the scenes are a scripted chain reaching into
other levels' leaves, or engine code - find which (G-10/G-11).

STATUS after PR-194: the CONTRACT half of T6 is now in port code, and
    the data names the producer half of it. (1) The split itself (3 = the
    reset pair then run again, 2 = the queue runner, 4 = stop, anything
    else = continue with no reset) lives in ported/ll_stage.[ch] as a
    pure table plus the counted unknowns, and the play leg answers it
    under LEO_LEVEL_EXIT; verify-stage pins all four outcomes at BYTE
    level (a stop at tick 50 gives the 50-tick frame, the default leg
    changes no byte at all, and 50 ticks after a reset at tick 50 are
    byte-identical to 50 ticks with no reset - that is what "the reset
    restored the authored state" means when it is proved). (2) The
    authored producer: `end_level` exists EXACTLY ONCE in the image
    (0x9198e, in the .MOV directive vocabulary) and is parsed at EXACTLY
    one site (0x46bbd-0x46c01, a node of class 0xa1048 carrying the
    number); in the fixtures all 26 `end_level <n>` rows are in the
    LION's own LEW.MOV, in 3 of its 12 members - two carry 12 rows of
    `end_level 1` inside state `A_EXIT`, one carries `A_TUNGA1` -> 1 and
    `A_TUNGA2` -> 2. The level ends from the lion's walk-off script, and
    the TUNGA pair asks with two different values; the port knows A_EXIT
    only as a name in a comment today. (3) What still blocks T6's
    behaviour half, all counted: the runtime that EXECUTES a parsed
    directive node (0xa1048 and its siblings 0xa1030/0xa1060/0xa11c8),
    whether that number is the loop's return, the exit word [level+0x4]
    or a level index, and who writes [level+0x4] - the easy shape was
    measured and ruled out (no ds:0xcef40 load pairs with a [reg+0x4]
    store nearby). PR-195 is that read.

**T7. RAN's balloon mode.** Episode 9 of the video shows it (you fly right,
birds attack the canopy, vulture boss throws bombs). Lion states with scripted
gravity (`G=400`), the `BAL_RAN`/`GPTAK_RA`/`SEP_RAN`/`PIOR_RAN` objects. Until
done the contact pass is OFF for RAN.01.

**T8. Sound effects.** `sample=` lines and `smpauto` frame words -> the `SOUND`
container (WAV samples); the audio path exists for music (`ll_audio.c`).

**T9. Menu/map polish.** Progress arrows, the "start" variant (element 1 of
`LEVELnn.SEQ`), the grey element 2, the mouse pointer (`MOUSE.SEQ`).

**T10. Fidelity harness.** A script that renders a level at a given state and
lays it next to a reference frame (the owner's timestamps) - the manual
procedure in section 9 automated. Never commit frames.

**T11. Cleanup (low risk, do when blocked).** Independent MOD mixer oracle;
the duplicated sparse-table scans (verify_pcm/verify_help); hard-coded
`/tmp/opencode` paths in tools; four tools without a Makefile target; the flaky
Wine restart-confirm leg; stale README/PLAN statements; NEXT_STEPS.md is 3500
lines - add a table of contents or split.

## 9. Method notes

Reading the machine code (how every engine fact above was found):
1. `tools/re/make_text_asm.sh`; find a string address in `leo.elf`
   (python `re.finditer`), grep the asm for `0x<addr>` to find the code using it.
1b. For anything beyond one literal grep, build the index first:
   `python3 tools/re/leodb.py build` then `callers`/`refs`/`slots`/`tables`/
   `sql` (see tools/re/README.md). It indexes the disassembly, every dword of
   the image (with an `aligned` column, so instruction bytes masquerading as a
   pointer are visible as such), branch xrefs, instruction immediates and the
   class tables. It is for FINDING, never for READING: the exact bytes at a VA
   still come from objdump (`leodb.py dis`, identical to `dat.py`), and sweep
   rows that another instruction overlaps are flagged `[SUSPECT]`. PR-106's
   method (call graph, not grep) is what this tool automates.
2. Class methods are virtual: find the class by its factory (0x4df10 maps the
   first field of a placement line to a vtable), read the vtable (dwords at
   the vtable address in `leo.elf`, VA - 0xf000) and the slot you need.
3. Decompiler output (`build/decompiled/all`) loses register arguments; trust
   the disassembly, state the operand identities in the NEXT_STEPS record.
4. Prove it: a hand-derived data-free test + a mutation check.

Measuring on the reference video (episodes 1-11 of the playlist
`PL6B7604DEF5B643F9`, ids: 1 xPiXBmPEdXg, 2 ti6IYfhh1bI, 3 uSZ9HekkBiw,
4 HJLaOqiLeys, 5 fxJrvBwtL-g, 6 v2LElzvk_pQ, 7 v-cJ0HcyHfg, 8 af0puAqzcMA,
9 HGmAqEa5FpQ, 10 O6-GbC1ZVmk, 11 p8TWx_PXanM):
- crop the game picture `crop=640:400:0:40`; it is the 320x200 view at 2x.
  Measured again in PR-111 (HANDOFF section 3, R1): for GAMEPLAY that is
  exact - rows 40..439 of a 640x480 frame at 2x, full width (motion
  profile over a 6 s window at t=314-320). It is NOT the right box for
  the rest of the file: in the window t=118-124, which shows one of the
  game's own `.IS` cutscenes (an animated cave interior with the lion, a
  dog and a spotted big cat at a table), the black bands reach to column
  36 and row 65; the level title cards are black-background text and
  their band is not pinned. Never reuse one crop across segment kinds -
  measure the band per segment (section 3).
- The full playthrough (section 3, R3) carries the game's own title cards,
  "ENA I / underwater journey", "KADMAR I / ancient mysteries" and the
  rest, and the owner has since supplied its level-by-level timestamps:
  those cards are the cleanest level-start markers the project has.
- That same file has the uploader's decoration baked into the picture: a
  static 128 px column at x 1152..1279, which the owner asked to keep in
  mind (section 3, R3, "THE BAR ON THE RIGHT"). Always crop
  `crop=1152:720:0:0` first; it is opaque art, so it is found by
  per-column TEMPORAL std (0.0-0.1 there, 3-24 inside the game window), not
  by looking for black.
- 480p compression and a scrolling camera limit precision; prefer segments the
  owner points out ("jump in place", camera not scrolling). Scripts: tools/re.
- The owner's timestamps so far: ep.1 5:16-5:19 (vine jump, used), ep.2
  0:53-0:55, 0:56-0:58, 7:13-7:16, 7:44-7:47 (all with a scrolling camera).

## 10. Checklist for every PR

- [ ] one logical step, evidence recorded (machine code address / data file /
      timestamp), estimates named in code AND in NEXT_STEPS
- [ ] data-free hand-derived test + mutation check
- [ ] `make` warning-free (`make win` and the wine legs belong to the
      end-of-implementation Windows pass, section 2 rule 8)
- [ ] `make -k -j"$(nproc)" verify-all` EXIT=0 with fixtures and
      fixture-free (serial is fine too; the Windows/wine legs stay serial) -
      not run for a pure-docs commit (section 2 rule 4's exception,
      2026-11-17)
- [ ] end-of-implementation only: `make win` warning-free,
      `make -k verify-windows`, and ci-windows.yml green
- [ ] README hook list updated if a `LEO_*` variable was added
- [ ] `make verify-docs` EXIT=0 (the docs structure pin: record
      headings, the journal-only rule (no plan/status/rule section
      in NEXT_STEPS; the one-homes in HANDOFF), the over-79
      ratchet, the field block of every record from PR-196 on, the
      .md inventory (P5) and the open-items register (P6)). This
      replaces the two hand greps the audit measured: the width-set
      check, and the heading census, a PREFIX PROXY that saw 107 of
      the 184 records existing at the PR-195 measurement.
- [ ] `make verify-index` EXIT=0 (docs/RE_INDEX.md is GENERATED by
      tools/re_index.py; the leg regenerates and `cmp`s it, so the index
      cannot drift from the ledger. If it fails, run `python3
      tools/re_index.py docs/RE_INDEX.md` and commit the regenerated
      file. It reads only tracked markdown, so it runs in both fixture
      modes; never hand-edit the index.)
- [ ] NEXT_STEPS.md record, carrying the field block of section 10a
      (required from PR-196 on; `make verify-docs` P4 enforces it);
      this file's sections 1, 6, 8 updated - never section 7 (it is
      a pointer; the ledger is frozen in the archive); an opened or
      closed cross-PR thread means a row (or status flip) in
      docs/OPEN_ITEMS.md IN THE SAME COMMIT, cited by the record
- [ ] commit "PR-NN: ..." ; `git push`; `gh run list --workflow=ci.yml` green
- [ ] nothing copyrighted staged (`git status`)

STOP CONDITIONS (joined from the journal's `## Next` by PR-203,
doc step 5 - one home for rules): every claim byte-backed or
fixture-byte-backed (cite ISO offsets); formats cross-checked
against the real ZARIA1.IS/ANIMACJE bytes, never inferred from
names alone; unproven halves stay counted; corrections get
brackets AND the `CORRECTS:` line; no port behaviour changes
without a failing test; Linux-first gates (make zero-warning +
verify-all both fixture modes) apply to any code PR, and a record
states its gate state ONCE, in its two GATE lines. THE DOC
INVARIANTS ARE NOT HAND-CHECKED: `make verify-docs` measures the
record headings (floor + accepted-gap list), the journal-only rule
and the HANDOFF one-homes, the over-79 ratchet, the record
template, the .md inventory and the register; a raised ratchet
needs a commit that says why; current floors are in
`tools/doc_census.py`'s own comments, not quoted here. Since
PR-197 `make verify-index` pins the generated index by
regenerate + cmp: if it fails, run
`python3 tools/re_index.py docs/RE_INDEX.md` and commit the
result - the index is generated, never hand-edited.

## 10a. The record template (PR-196, doc-audit step 1)

The owner's audit measured what the fields are worth when they are
optional: `- FINDING` in 29% of records, `- WHY` 39%, `OPEN ITEMS` 43%,
a gate marker in 41%, and 3,022 of 3,371 `PR-nn` citations carrying no
address/field/name token within 200 characters. The fields below are
therefore REQUIRED, from PR-196 on, by `tools/doc_census.py` P4 - not
advice. History is not rewritten: P4 tests records whose number is
`TEMPLATE_FROM` (196) or higher.

The block is the eleven lines below, in this order, immediately after
the record's title lines. Field names are exact - the audit found 86
prose gate markers in four spellings (`- GATES.`, `- GATES:`,
`- GATES (`, `- TESTS AND GATES`), so a stem is not enough. Every line
stays within 79 columns; a field continues on an INDENTED continuation
line, and an indented copy of a field name is prose, never a field.
The indented lines below give each field name and, after it, what to
write. P4 enforces PRESENCE, ORDER and EXACTLY-ONE-EACH; it does not
test position inside the record, and "immediately after the title" is
the style that keeps a record readable.

  KIND: read | port | gate | docs
      What the record ESTABLISHES, not which files it touches: a
      reading PR that adds a tool to prove it is still `read`; a gate
      or tooling PR whose product is the check itself is `gate`. The
      commit subject's kind tag matches this value (section 2 rule 9).
  SUBSYSTEM: <one lowercase phrase>
      The subsystem the finding lands in (`level exit contract`,
      `.MOV` action runtime, `.mnu` interpreter, docs structure). It
      is what groups the index by area.
  ADDRESSES: <tokens>, or `none`
      EVERY engine address the record establishes as current truth OR
      retracts, exact `0xNNNNN` tokens only, prose elsewhere. A
      retracted address belongs here - that is what stops the reader
      having to read 51 records to learn whether a claim still stands
      (0xcef40 measured 113 lines across 51 records).
  NAMES: <tokens>, or `none`
      The authored vocabulary the record touches: `.des` keys
      (`control=`), state and action names (`A_EXIT`, `AC_TIME`),
      `.MOV`/`.SEQ` member names, `[field+0xNN]` words, file names.
  CODE: no: <one line> | yes: <one line>
      What the code change does; `no: none` for a reading or docs PR.
  HOOKS: <LEO_* names>, or `none`
      Environment hooks this record adds or changes. README's hook
      list stays the authority for what exists.
  OPENS: <ids or phrases> | CLOSES: <ids or phrases> | `none`
      Open items moved by the record. Stable `OI-` ids arrive with
      step 3; until then, name the item in one phrase so the register
      can be seeded from these lines instead of re-reading 24 records.
  CORRECTS: none | PR-nn[, PR-nn]: <the claim that was wrong>
      The mechanical correction mark. The bracket form appears 7 times
      in the ledger while the prose words appear 112 + 21 + 12 + 6
      times, and 352 distinct addresses sit inside correction
      sentences - `grep 0x4be20` finds the mentions, never the
      sentence that killed it. This field fixes that going forward:
      the index marks the record where a claim turned current or
      retracted.
  GATE: make 0 warnings; verify-all fixtures rc=0 <N> (ok);
  GATE-FIXTURE-FREE: rc=0 <M> (ok) + <K> named verify-SKIPs
      The two rule-4 halves, in exactly this wording, counts filled
      in. Prose may continue either line, INDENTED. Do not restate
      the gate state anywhere else in the record: this is the one
      place it is written, because restating it is measured drift -
      the audit found 12 different count claims around those 86
      markers, which is where the 18/19/20 SKIP confusion came from.
  GATE (light profile, PR-232): `GATE: light: verify-index
      verify-docs rc=0 (ok);`
      Only for read-/docs-kind records (section 2 rule-4 GATE
      PROFILE): their PR ran the docs checks alone, so the record
      carries this ONE line and no GATE-FIXTURE-FREE line. A
      read/docs record that did run the full gate keeps the
      two-line form; a record never carries both forms. The tool
      (P4) accepts light only under `KIND: read|docs`; port|gate
      records must carry the full pair.

ONE exemption, frozen in the tool as FIELDS_EXEMPT: PR-196, the record
that writes this rule. It cannot carry the contract it is introducing,
so it carries a self-exemption note instead. The exemption skips the
BLOCK requirement; the tool still reads that record and reports how
many field lines it carries. PR-196 carries exactly two - its own
gate lines, in the format defined here - and a PARTIAL block is not
accepted for any other record, so quoting these names is only safe
inside an exempt record.

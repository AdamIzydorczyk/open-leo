/* ll_stage.h - THE STAGE-DRIVER EXIT CONTRACT (PR-194).
 *
 * What the original does between "this level is over" and "run the next
 * thing". Every address below is from PR-193's read of the engine
 * (NEXT_STEPS.md, "PR-193 - THE STAGE-DRIVER LAYER AND THE EXIT-CODE
 * CONTRACT ARE READ END TO END"); the bytes were re-measured there and
 * this file only states the contract, it adds no new reading.
 *
 * The engine, per stage, has a wrapper (13 of them; the zaria1 one calls
 * the driver at 0x5d3b0 and splits at 0x5d3b5, the zaria2 one calls it
 * at 0x5d420 and splits at 0x5d425), which
 *   1. calls the driver 0x5d170 with the stage name (0x5d170 holds the
 *      ONLY call site of the level loop 0x4abd0, at 0x5d251),
 *   2. the driver stores a PAIR into the caller's buffer at
 *      0x5d2d1-0x5d2dc: the level loop's return value and the exit word
 *      at [level-object + 0x4] (the level object is the global
 *      ds:0xcef40),
 *   3. the wrapper reads the FIRST of those back (`mov edx,[esp]`) and
 *      splits on it (0x5d3b5-0x5d409, repeated per wrapper at 0x5d425...).
 *
 * The split, value by value:
 *   3  the stage reset pair runs first (0x5da60 then 0x5d050, with the
 *      stage object in eax; wrapper 1 passes edx=0 at 0x5d3c8/0x5d3d5,
 *      wrapper 2 passes edx=1 at 0x5d436/0x5d446 - what that flag picks
 *      is not read), and the wrapper THEN returns 1 = run the same stage
 *      again.
 *   2  the queue runner 0x5d2f0 runs and ITS value is returned: it
 *      FIRST resets the life counter ds:0xa94f0 to 5 (0x5d2f9, read by
 *      PR-530), then opens status.leo (the save writer 0x5dcc0 called
 *      at 0x5d303), reads the queue count at
 *      ds:0xcf464 (0x5d308); with the count 0 it returns 0 (`xor
 *      ebx,ebx` at 0x5d38e), with a non-empty queue it processes EXACTLY
 *      ONE entry (0x5e180/0x5df50 build the strings, 0x60a30 constructs,
 *      0x603b0 is the stream run verb at 0x5d35f) and returns 2 after
 *      popping it (`dec ecx` + `mov ds:0xcf464,ecx` at 0x5d375-0x5d378,
 *      `mov ebx,0x2` at 0x5d368).
 *   4  returns 0: the caller chain stops (PR-190's exit-4 leg).
 *   any other value  returns 1, WITHOUT any reset. Note that 3 and the
 *      default therefore BOTH return 1; the whole difference is the
 *      reset. A model that collapses them loses the reset.
 *
 * What is NOT known, and stays counted (do not fill it in here):
 *   - who WRITES the exit word [level+0x4], and who returns 1..4 out of
 *     the level loop. The authored `.MOV` directive `end_level <n>` (26
 *     lines in the HD ANIMACJE members, values 1 and 2, parsed by the
 *     engine at 0x46bbd-0x46c01 into a node of class 0xa1048) is the
 *     producer CANDIDATE named by the data; its runtime executor was not
 *     read, so this module never invents a producer.
 *   - what the reset pair 0x5da60 / 0x5d050 does inside, and what the
 *     per-stage edx 0/1 selects.
 *   - who FILLS the queue at ds:0xcf464 and what one entry looks like.
 *   - whether the port will ever have a queue: ll_stage_queue_len()
 *     returns 0 because nothing produces entries here, which is also
 *     what the engine's own empty-queue leg answers.
 */
#ifndef LL_STAGE_H
#define LL_STAGE_H

/* The wrapper's return value, i.e. what the engine's caller acts on:
 * 1 = run the stage again, 2 = the queue ran one item (come back),
 * 0 = stop the caller chain. */
#define LL_STAGE_AGAIN 1
#define LL_STAGE_QUEUED 2
#define LL_STAGE_STOP 0

/* the port's own queue length: no producer is implemented (see above) */
int ll_stage_queue_len(void);

/* THE CONTRACT. exit_code is the value the wrapper read out of its
 * result pair, queue_len is ll_stage_queue_len() (or a caller-supplied
 * length for a test), *did_reset is set to 1 exactly when the reset
 * pair would run. Returns LL_STAGE_AGAIN / LL_STAGE_QUEUED /
 * LL_STAGE_STOP. Pure function: no globals, no logging, so a test can
 * pin it without a fixture. */
int ll_stage_outcome(int exit_code, int queue_len, int *did_reset);

#endif /* LL_STAGE_H */

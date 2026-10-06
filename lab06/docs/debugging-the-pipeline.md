# Debugging a pipeline

A pipeline fails differently from a single-cycle machine. There, a wrong answer
came from one wrong stage and the trace pointed straight at it. Here the
instruction that produces the wrong answer is usually not the one at fault: it
read a register too early, or it should not have been executed at all.

So read the *cycle*, not just the instruction.

## The one command worth knowing

    printf '[Logging]\nlog_level = DEBUG\n' > debug.ini
    build/sim --proc=pipelined --config=debug.ini tests/images/p4-branch.r5ob

At `DEBUG` the simulator prints one line per cycle showing what is in each
pipeline register:

    cycle 6      IF/ID 80000018  ID/EX 80000014  EX/MEM 80000010  MEM/WB 8000000c
    cycle 7      IF/ID 8000001c  ID/EX 80000018  EX/MEM 80000014  MEM/WB 80000010
    cycle 8      IF/ID 8000001c  ID/EX --------  EX/MEM 80000018  MEM/WB 80000014

Each column is an address, or `--------` for an empty stage. Read it downwards:
an instruction should appear one column to the right on each successive line.

That is the whole diagnostic vocabulary:

| What you see | What it means |
| --- | --- |
| the same address in IF/ID on two lines, and a `--------` moving right | a stall: the instruction was held in ID and a bubble was sent on |
| an address that disappears without reaching MEM/WB | it was flushed -- something ahead of it redirected the PC |
| every column `--------` after the first few cycles | nothing is being fetched, or the latches are not being copied |
| an address appearing twice in the same column | the PC did not advance |
| addresses advancing by 4 forever past the end of the program | the store to `tohost` was not noticed |

## Reading a trace failure

The tests compare your `[OUT]` trace against ours and name the first line that
differs. In a pipeline, look at the *previous* few lines before the named one:

    trace differs at retired instruction 41
          yours: [OUT] 800020a4 | next_pc = 800020a8 | x15 = 00000002 | mem[?] = ...
          ours:  [OUT] 800020a4 | next_pc = 800020a8 | x15 = 00000003 | mem[?] = ...

Same address, same `next_pc`, different value. The instruction went to the right
place and computed the wrong number, which in a pipeline almost always means it
read a stale register -- a dependency that should have stalled and did not.

If instead the *addresses* diverge, an instruction retired that should have been
thrown away, or one was thrown away that should not have been. Look at the
branch a few lines earlier and count the instructions between it and the
divergence.

## Three mistakes that produce plausible-looking output

**Writing the current latch instead of the next one.** The pipeline then runs
several stages of one instruction in a single cycle, so the program is *correct*
and far too fast. If your cycle count is close to your instruction count, this
is why.

**Counting a squashed bubble as a flushed instruction.** `flushed` ends up
larger than the number of instructions ever fetched. Only count a latch that
held something.

**Fetching while stalled.** The instruction count stays right, because the
duplicate is squashed, but `fetches` climbs and every cycle count is a little
too high once a cache is involved.

## The identity to check first

With an ideal memory:

    cycles = instructions + 4 + stall_cycles + flushed_instructions

The `+ 4` is filling the pipeline. If this does not balance, one of the four
numbers is being counted in the wrong place, and that is a much smaller thing to
look for than a wrong trace.

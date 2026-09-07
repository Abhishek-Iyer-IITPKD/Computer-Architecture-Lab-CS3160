# Running your simulator

    make                # build build/sim
    make test           # the tests, all three tiers
    make unit           # just the unit tests, quieter while you work on one stage

Once it builds, run it on a program yourself:

    build/sim --proc=single tests/images/1-even.r5ob

which writes two files into the current directory:

| File         | What it holds                                                    |
|--------------|------------------------------------------------------------------|
| `sim.log`    | one `[OUT]` line per retired instruction, and more at `DEBUG`    |
| `stats.json` | the counts: instructions, cycles, loads, stores, branches, jumps |

Useful options:

| Option                        | Effect                                                     |
|-------------------------------|------------------------------------------------------------|
| `--log=FILE` / `--stats=FILE` | write them somewhere else                                  |
| `--num_insts=N`               | stop after N instructions, for a program that will not end |
| `--disasm=LO:HI`              | disassemble a range before running, using week 4's code    |
| `--norun`                     | load and inspect only                                      |

## Reading a failure

The tests compare your `[OUT]` trace against ours line for line, and tell you
the first line that differs:

    trace differs at retired instruction 41
          yours: [OUT] 800020a4 | next_pc = 800020a8 | x15 = 00000003 | mem[?] = 00000000
          ours:  [OUT] 800020a4 | next_pc = 800020b8 | x15 = 00000003 | mem[?] = 00000000

Read the columns. Same address, same register, same value -- only `next_pc`
differs, so the instruction computed the right answer and control went to the
wrong place. That is `next_pc_of()` or the branch verdict, not the ALU.

If the register value differs instead, it is the ALU or the writeback. If the
address differs, everything before this instruction was already wrong and this
is only where it became visible -- look at an earlier failure first.

To see what your simulator was doing at that point, turn the log up:

    printf '[Logging]\nlog_level = DEBUG\n' > debug.ini
    build/sim --proc=single --config=debug.ini tests/images/1-even.r5ob

which prints every stage of every instruction into `sim.log`.

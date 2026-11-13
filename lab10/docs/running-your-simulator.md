# Running the simulator

    make                # build build/sim
    make test           # all three test tiers
    make unit           # unit tests only

`tests/images/` holds the programs the tests use. `--proc` selects the
processor model; only the models built so far are available:

    build/sim --proc=single     tests/images/<name>.r5ob   # lab 5 onwards
    build/sim --proc=pipelined  tests/images/<name>.r5ob   # lab 6 onwards
    build/sim --proc=forwarding tests/images/<name>.r5ob   # lab 8 onwards

Requesting a model not yet built is reported as an error; no other model is
substituted.

Each run writes two files to the current directory:

| File         | Contents                                                         |
|--------------|------------------------------------------------------------------|
| `sim.log`    | one `[OUT]` line per retired instruction, and more at `DEBUG`    |
| `stats.json` | the counters: instructions, cycles, loads, stores, branches, jumps |

Options:

| Option                        | Effect                                                     |
|-------------------------------|------------------------------------------------------------|
| `--log=FILE` / `--stats=FILE` | write the log or statistics to FILE                        |
| `--num_insts=N`               | stop after N instructions                                  |
| `--disasm=LO:HI`              | disassemble an address range before running (lab 4 code)   |
| `--norun`                     | load and inspect only; do not run                          |

## Interpreting a trace mismatch

The tests compare the `[OUT]` trace with the reference line by line and report
the first difference:

    trace differs at retired instruction 41
          yours: [OUT] 800020a4 | next_pc = 800020a8 | x15 = 00000003 | mem[?] = 00000000
          ours:  [OUT] 800020a4 | next_pc = 800020b8 | x15 = 00000003 | mem[?] = 00000000

Compare the fields. Here the address, register and value agree and only
`next_pc` differs: the instruction computed the correct result but control
went to the wrong address. The fault lies in `next_pc_of()` or the branch
decision, not the ALU.

A differing register value indicates the ALU or writeback. A differing address
means that execution had already diverged before this instruction; examine an
earlier failure first.

For detail at that point, raise the log level:

    printf '[Logging]\nlog_level = DEBUG\n' > debug.ini
    build/sim --proc=single --config=debug.ini tests/images/1-even.r5ob

This writes every stage of every instruction to `sim.log`.

# The memory hierarchy

Before lab 9 every model assumed an *ideal* memory, which responds within the
cycle of the request; a program's cycle count then depends only on the
pipeline. No real machine has such a memory.

From lab 9 onwards the machine is described by a configuration file:

    build/sim --proc=forwarding --config=configs/icache-direct.ini prog.r5ob

Without `--config` the memory is ideal, which remains appropriate when
debugging the processor rather than the memory.

## Configuration format

    [RAM]
    latency = 100          # cycles to answer anything

    [I1_Cache]             # the instruction side
    valid        = true    # false means this level is not there at all
    latency      = 2       # cycles to look it up, hit or miss
    size         = 128     # bytes of storage
    block_size   = 16      # bytes moved between levels, in one piece
    assoc        = 2       # ways per set -- only read when mapping = set
    mapping      = direct  # direct | set | full
    replacement  = FIFO    # FIFO | LRU
    write_policy = write-back    # write-back | write-through
    write_miss   = allocate      # allocate | no-allocate

    [L1_Cache]             # the data side
    [L2_Cache]             # shared by both, when it is there

A misspelt key or an unknown value terminates the run; neither is ignored.

The three levels are connected as follows; any of them may be absent:

    processor --fetch--> I1 --+
                              +--> L2 --> RAM
    processor --data---> L1 --+

With no cache present, both sides access the RAM directly: an ideal memory
with a latency.

## Reported statistics

The summary gains one line per cache:

      I1   accesses      923   hits      828   misses       95   hit rate  89.71%

`stats.json` gains a `memory` array with the same figures for each level, and
additionally `read_hits`, `read_misses`, `write_hits`, `write_misses`,
`evictions` and `writebacks`. The tests read this array.

| Counter      | Incremented when                                                 |
|--------------|------------------------------------------------------------------|
| `accesses`   | any request reaches this level (reads and writes)                |
| `misses`     | the requested block is not present                               |
| `evictions`  | a *valid* block is removed to make room for another              |
| `writebacks` | a removed block is dirty and must first be written to the level below |

The first miss into an empty set is not an eviction, since nothing is removed.
Every writeback accompanies an eviction, but not conversely: a clean block is
identical to the copy below and is discarded without a write.

Two operations are deliberately excluded from the counts. The flush at the end
of a run, which writes dirty blocks down so that the RAM holds the final state,
makes that state visible and is not an action of the program. `--dump` reads
the RAM directly, bypassing the caches, for the same reason.

## Cycle cost

A hit costs the latency of its level. A miss costs that latency *plus* the cost
at the level below, recursively, until a level holds the block:

    hit in I1                        2
    miss in I1, hit in L2            2 + 10
    miss in both, RAM answers        2 + 10 + 100

`memory_stall_cycles` in `stats.json` is the total cost of the run beyond one
cycle per access; with a cache present, it accounts for most of the difference
between `cycles` and `instructions`.

The average memory access time (AMAT) is printed under each cache:

    AMAT = hit time + miss rate x miss penalty

where the miss penalty is the AMAT of the level below. AMAT is *derived from
the counters*, not measured, and the two can differ: a write-through cache pays
the cost of the level below on every write, hit or miss, which the formula does
not capture.

## Method

Labs 9 and 10 compare one program on two machines. The program is held fixed
and one property of the machine is varied, so the difference in the statistics
is the cost of that property. The configurations in `configs/` form families
for this purpose: within a family, exactly one line differs.

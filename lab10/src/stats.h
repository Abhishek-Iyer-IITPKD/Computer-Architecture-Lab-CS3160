// Counters, and the stats.json report built from them.
//
// Every measurement is recorded here, so that all processor models report
// through the same interface. Each memory level has its own LevelStats, which it
// registers with Stats when the hierarchy is built; stats_write() reports the
// levels in hierarchical order.

#ifndef CS3160_STATS_H
#define CS3160_STATS_H

#include "common.h"

// Counters for one level of the memory hierarchy.
//
// A cache sets all of these; RAM has no misses. `is_cache` tells the reporter
// which fields to print.
struct LevelStats {
    const char *name;      // "I1", "L1", "L2", "RAM"
    bool is_cache;

    // The latency of this level and the level that serves its misses: not
    // measurements but the structure of the hierarchy, recorded here for
    // level_amat().
    int         latency;   // cycles for a lookup here, hit or miss
    LevelStats *below;     // the level a miss is handed to; NULL for the last

    long reads;            // requests arriving from the level above
    long writes;
    long read_hits;
    long read_misses;
    long write_hits;
    long write_misses;

    long evictions;        // valid blocks displaced to make room
    long writebacks;       // dirty blocks handed to the next level down
};

void level_stats_init(LevelStats &s, const char *name, bool is_cache,
                      int latency);

inline long level_accesses(const LevelStats &s) { return s.reads + s.writes; }
inline long level_hits(const LevelStats &s)     { return s.read_hits + s.write_hits; }
inline long level_misses(const LevelStats &s)   { return s.read_misses + s.write_misses; }

// Average memory access time, in cycles, of the accesses reaching this level:
//
//     AMAT = hit time + miss rate x miss penalty
//
// The hit time is this level's latency, the miss rate comes from the counters,
// and the miss penalty is the AMAT of the level below. The last level never
// misses, so its AMAT is its latency, which ends the recursion.
//
// AMAT is *derived*, and differs from the measured memory_stall_cycles /
// accesses: the formula charges the level below only on misses, whereas a
// write-through hit also pays it.
double level_amat(const LevelStats &s);

struct Stats {
    const char *processor;   // the model that produced these figures

    // The two [Timing] settings in force, recording the timing model of the
    // run.
    const char *resolve_stage;  // "ID", "EX" or "MEM"
    const char *overlap;        // "max" or "sum"

    long instructions;       // instructions that retired
    long cycles;             // clock cycles, including stalls and memory delay
    long stall_cycles;       // cycles lost to hazard stalls
    long mem_stall_cycles;   // cycles lost waiting for the memory hierarchy
    long flushed;            // wrong-path instructions squashed

    long fetches;            // instruction-side memory accesses
    long data_accesses;      // data-side memory accesses (loads + stores)
    long loads;
    long stores;
    long branches;
    long taken_branches;     // inferred: next_pc was not pc + 4
    long jumps;

    // Redirects as decided by the pipeline (measured)
    long pc_redirects;

    bool exited;             // whether the program reached _exit and stored to tohost
    i32  exit_status;

    LevelStats *levels[8];
    int n_levels;
};

void stats_init(Stats &st);

// Register a memory level for the report. Levels are reported in registration
// order, so register them from the processor downwards.
void stats_add_level(Stats &st, LevelStats *level);

// Cycles per instruction; 0 when nothing retired.
double stats_cpi(const Stats &st);

// Write the machine-readable report.
void stats_write(const Stats &st, const char *path);

// Write a summary to stdout, one figure per line.
void stats_print_summary(const Stats &st);

#endif  // CS3160_STATS_H

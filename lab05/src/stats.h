// Counters, and the stats.json report built from them.
//
// Every measurement should come from here so that different processor 
// models use a similar interface.
// Each memory level has its own LevelStats and registers it with the Stats
// object at set-up time; stats_write() then reports them in hierarchical order.

#ifndef CS3160_STATS_H
#define CS3160_STATS_H

#include "common.h"

// Counters for one level of the memory hierarchy.
//
// A cache fills in all of these. RAM has no notion of a miss. 
// The `is_cache` field tells the reporter which fields to print.
struct LevelStats {
    const char *name;      // "I1", "L1", "L2", "RAM"
    bool is_cache;

    long reads;            // requests arriving from the level above
    long writes;
    long read_hits;
    long read_misses;
    long write_hits;
    long write_misses;

    long evictions;        // valid blocks displaced to make room
    long writebacks;       // dirty blocks handed to the next level down
};

void level_stats_init(LevelStats &s, const char *name, bool is_cache);

inline long level_accesses(const LevelStats &s) { return s.reads + s.writes; }
inline long level_hits(const LevelStats &s)     { return s.read_hits + s.write_hits; }
inline long level_misses(const LevelStats &s)   { return s.read_misses + s.write_misses; }

struct Stats {
    const char *processor;   // which model produced these numbers

    // The two [Timing] decisions in force so that a run says what timing model
    // produced it.
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

    // Redirects as the pipeline actually decided them
    long pc_redirects;

    bool exited;             // did the program reach _exit and store to tohost?
    i32  exit_status;

    LevelStats *levels[8];
    int n_levels;
};

void stats_init(Stats &st);

// Register a memory level so it appears in the report. Levels are reported in
// registration order, so register from the processor side downwards.
void stats_add_level(Stats &st, LevelStats *level);

// Cycles per instruction: 0 when nothing retired, so callers need no guard.
double stats_cpi(const Stats &st);

// Write the machine-readable report. 
void stats_write(const Stats &st, const char *path);

// One-line-per-figure summary on stdout.
void stats_print_summary(const Stats &st);

#endif  // CS3160_STATS_H

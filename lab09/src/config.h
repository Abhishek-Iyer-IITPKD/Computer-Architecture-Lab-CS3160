// Configuration file parsing.
//
// Every variable parameter of the machine (the processor model, the depth of
// the memory hierarchy, the latency of each level, the stage in which branches
// resolve) is a configuration value, which simplifies "what-if" studies.
//
// Two deliberate rules:
//   * every value is converted here to an enum, a bool or an int, so that no
//     configuration string is compared during a run;
//   * an unknown section or key is a fatal error.

#ifndef CS3160_CONFIG_H
#define CS3160_CONFIG_H

#include <string>

#include "common.h"

enum ProcKind {
    PROC_SINGLE,      // one instruction per iteration, no pipeline
    PROC_PIPELINED,   // 5 stages, stall on every hazard
    PROC_FORWARDING,  // 5 stages, forwarding paths, branch resolved early
};

enum Mapping {
    MAP_DIRECT,  // one block per set (associativity 1)
    MAP_SET,     // `assoc` blocks per set
    MAP_FULL,    // one set holding every block
};

enum Replacement { REPL_FIFO, REPL_LRU };

enum WritePolicy {
    WP_WRITEBACK,     // a write marks the block dirty; memory is updated on eviction
    WP_WRITETHROUGH,  // a write goes to this level and to the next level immediately
};

// What happens on a write that misses.
enum WriteMiss {
    WM_ALLOCATE,    // fetch the block, then write into it
    WM_NOALLOCATE,  // write straight past this level, leaving it unchanged
};

// The pipeline stage in which a branch or jump outcome becomes known. Every
// instruction fetched after the branch until then is on the wrong path and is
// flushed, so this value fixes the flush count: 1 for ID, 2 for EX, 3 for MEM.
enum ResolveStage { RESOLVE_ID = 2, RESOLVE_EX = 3, RESOLVE_MEM = 4 };

// Whether a cycle in which both an instruction fetch and a data access take
// longer than a cycle costs both latencies, or only the larger.
enum Overlap {
    OVERLAP_MAX,  // the two accesses proceed in parallel; pay the larger
    OVERLAP_SUM,  // single-ported memory, one access at a time; pay both
};

struct CacheConfig {
    bool        valid;        // whether this level is present
    const char *name;         // "I1", "L1", "L2"
    int         latency;      // cycles for a hit at this level
    int         size;         // bytes of data storage
    int         block_size;   // bytes per block
    int         assoc;        // blocks per set (ignored unless mapping = set)
    Mapping     mapping;
    Replacement replacement;
    WritePolicy write_policy;
    WriteMiss   write_miss;
};

struct Config {
    // [General]
    std::string stats_file;
    std::string log_file;
    long        num_insts;    // instruction budget; a correct program exits first
    u32         start;        // initial PC

    // [Logging]
    int log_level;

    // [Processor]
    ProcKind proc;

    // [Timing]
    ResolveStage pipelined_resolve;
    ResolveStage forwarding_resolve;
    Overlap      latency_overlap;

    // [RAM]
    u32 ram_base;
    u32 ram_size;
    int ram_latency;

    // [Termination]
    u32 tohost;               // a store to this address ends the simulation

    CacheConfig i1, l1, l2;
};

// Fill `cfg` with the default configuration.
void config_defaults(Config &cfg);

// Read `path` on top of the defaults. Fatal on a missing file, an unknown
// section or key, or a value that does not parse.
void config_read(Config &cfg, const char *path);

// Reject combinations that cannot be built: a cache whose size is not a
// multiple of its block size, direct mapping with associativity 2, an L1 above
// an L2 with a different block size, and so on.
void config_validate(const Config &cfg);

// Names, for the log and the report.
const char *proc_kind_name(ProcKind k);
const char *mapping_name(Mapping m);
const char *replacement_name(Replacement r);
const char *write_policy_name(WritePolicy w);
const char *write_miss_name(WriteMiss w);
const char *resolve_stage_name(ResolveStage s);

// Parse a processor name from the command line or the configuration file:
// the short names (single, pipelined, forwarding) or the class names, in any
// case.
ProcKind proc_kind_from_name(const char *name);

// The stage in which `kind` resolves a branch.
ResolveStage config_resolve_stage(const Config &cfg, ProcKind kind);

// Whether `kind` forwards results between stages.
bool config_has_forwarding(ProcKind kind);

// Write the configuration in force to the log, so that a run can be reproduced.
void config_log(const Config &cfg);

#endif  // CS3160_CONFIG_H

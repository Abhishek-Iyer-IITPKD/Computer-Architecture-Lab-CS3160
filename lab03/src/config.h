// Configuration file parsing.
//
// Every parameter of a processor that can be varied such as the processor model
// to simulate, depth of the memory hierarchy, how many cycles each level costs, 
// at which pipeline stage is a branch is resolved, etc is a configuration 
// value. This helps in simplifying "what-if" studies.
//
// Two rules the parser follows on purpose:
//   * every value is converted to an enum, a bool or an int here. No part of 
//     the simulator should compare configuration strings during the run.
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

// The pipeline stage in which a branch or jump target becomes known. Every
// instruction fetched after the branch and before the end of this stage is on
// the wrong path and gets squashed/flushed, so this value fixes the flush count:
// resolving in ID costs 1 wrong-path instruction, in EX 2, in MEM 3.
enum ResolveStage { RESOLVE_ID = 2, RESOLVE_EX = 3, RESOLVE_MEM = 4 };

// When an instruction fetch and a data access both take longer than a cycle in
// the same clock cycle, does the pipeline pay for both or only for the slower?
enum Overlap {
    OVERLAP_MAX,  // the two accesses proceed in parallel; pay the larger
    OVERLAP_SUM,  // single ported memory, one access at a time; pay both
};

struct CacheConfig {
    bool        valid;        // is this level present at all?
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

// Fill `cfg` with defaults (as in timing handout). 
void config_defaults(Config &cfg);

// Read `path` on top of the defaults. Fatal on a missing file, an unknown
// section or key, or a value that does not parse.
void config_read(Config &cfg, const char *path);

// Reject combinations that cannot be built (a cache whose size is not a
// multiple of its block size, direct mapping with associativity 2, an L1
// without an L2 below it that has a different block size, ...).
void config_validate(const Config &cfg);

// Names, for the log and the report.
const char *proc_kind_name(ProcKind k);
const char *mapping_name(Mapping m);
const char *replacement_name(Replacement r);
const char *write_policy_name(WritePolicy w);
const char *write_miss_name(WriteMiss w);
const char *resolve_stage_name(ResolveStage s);

// Parse a processor name from the command line or the config file. Accepts 
// the short names (single, pipelined, forwarding) and the class names 
// case-insensitively.
ProcKind proc_kind_from_name(const char *name);

// The stage in which `kind` resolves a branch.
ResolveStage config_resolve_stage(const Config &cfg, ProcKind kind);

// Does `kind` forward results between stages?
bool config_has_forwarding(ProcKind kind);

// Write the configuration in force to the log (for reproducible runs)
void config_log(const Config &cfg);

#endif  // CS3160_CONFIG_H

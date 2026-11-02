// A cache: one level of the memory hierarchy.
//
// One structure serves every level and every geometry. Direct-mapped,
// set-associative and fully associative caches are one implementation with
// different numbers of blocks per set, into which `mapping` is translated when
// the cache is built:
//
//     direct   assoc = 1                    one block per set
//     set      assoc = as configured        n blocks per set
//     full     assoc = every block          one set
//
// An address is divided into three fields:
//
//     +---------------------+-----------+--------+
//     |         tag         |    set    | offset |
//     +---------------------+-----------+--------+
//
// offset selects the byte within a block and set the set; tag is the remaining
// upper bits, stored with the data to identify which block is present. A fully
// associative cache has one set and no set bits, so its tag is the whole block
// address.
//
// Every level counts its own hits, misses, evictions and writebacks.

#ifndef CS3160_CACHE_H
#define CS3160_CACHE_H

#include <vector>

#include "config.h"
#include "memory.h"
#include "stats.h"

struct CacheBlock {
    bool valid;
    bool dirty;  // written since it was fetched; the level below is stale
    u32  tag;
};

// The fields of an address for a particular cache.
struct CacheAddr {
    u32 offset;
    u32 set;
    u32 tag;
};

struct Cache : MemoryLevel {
    const char *level_name;
    int         latency;      // cycles for a lookup at this level
    int         block_size;
    int         assoc;        // blocks per set, after `mapping` is applied
    int         n_sets;
    Mapping     mapping;
    Replacement replacement;
    WritePolicy write_policy;
    WriteMiss   write_miss;

    MemoryLevel *next;  // the level below; never NULL for a cache

    std::vector<CacheBlock> blocks;  // n_sets * assoc, indexed set * assoc + way
    std::vector<u8>         data;    // the same, block_size bytes each

    // Replacement order: one list of way numbers per set, the front being the
    // next to be replaced. FIFO appends a way on insertion only; LRU also moves
    // a way to the back when it is used. The policies differ only in this.
    std::vector<std::vector<int> > order;

    LevelStats stats;

    u32 read(u32 addr, int size, int *latency) override;
    void write(u32 addr, u32 data, int size, int *latency) override;
    void read_block(u32 addr, u8 *dst, int block_size, int *latency) override;
    void write_block(u32 addr, const u8 *src, int block_size, int *latency) override;
    const char *name() const override { return level_name; }
    LevelStats *level_stats() override { return &stats; }
};

// Build a cache from its configuration. `next` is the level below, which every
// cache has. config_validate() has already rejected invalid geometries.
void cache_init(Cache &c, const CacheConfig &cfg, MemoryLevel *next);

// Divide an address into tag, set and offset for this cache.
CacheAddr cache_split(const Cache &c, u32 addr);

// The address of the first byte of the block with the given set and tag: the
// inverse of cache_split(). A dirty block being evicted records only its tag,
// so its address must be reconstructed from the tag and the set.
u32 cache_block_addr(const Cache &c, u32 set, u32 tag);

// The way of `set` holding `tag`, or -1: the lookup that distinguishes a hit
// from a miss.
int cache_find(const Cache &c, u32 set, u32 tag);

// --- The steps of an access -------------------------------------------------
//
// Declared here, rather than private to cache.cpp, so that each step can be
// unit-tested in isolation; an error is far easier to locate in one small
// function than in an overall hit rate.

// The data of one block, and its tag/valid/dirty record, each indexed by a set
// and a way within it.
u8         *cache_block_data(Cache &c, u32 set, int way);
CacheBlock &cache_block_at(Cache &c, u32 set, int way);

// Record that a block has just been placed in `way` of `set`: it is now the
// furthest from replacement, under either policy.
void cache_order_insert(Cache &c, u32 set, int way);

// Record that `way` has just been *used*: the only point at which the two
// policies differ. LRU updates the order; FIFO does not.
void cache_order_touch(Cache &c, u32 set, int way);

// The way of `set` to receive the next block: an invalid way if there is one,
// otherwise the way the replacement policy selects.
int cache_choose_way(Cache &c, u32 set);

// Make room in `set` and return the way to use. Counts the eviction and, if
// the displaced block holds the only current copy, writes it back first.
int cache_evict_for(Cache &c, u32 set, int *latency);

// If the block is dirty, write it to the level below and mark it clean.
// Returns whether it wrote.
//
// It does not count the writeback, because its two callers differ: a block
// written back on eviction is traffic the program caused, which
// cache_evict_for() counts, whereas cache_flush() at the end of a run only
// makes the final state visible and must not alter any reported figure.
bool cache_writeback_if_dirty(Cache &c, u32 set, int way, int *latency);

// Fetch the block containing `addr` from the level below and install it,
// evicting as necessary. Returns the way filled.
int cache_fill(Cache &c, u32 addr, int *latency);

// Two restrictions, enforced rather than modelled. An access must lie within
// one block (one spanning two would need two lookups), and the whole hierarchy
// uses one block size, so that no level splits or merges blocks. A violation
// stops the run.
void cache_check_within_block(const Cache &c, u32 addr, int size);
void cache_check_block_size(const Cache &c, int block_size);

// Write every dirty block to the level below, so that after a run the RAM
// holds the program's final state and a memory dump is current.
//
// Deliberately leaves the statistics and the cycle count unchanged: this
// traffic makes the final state visible and is not an action of the program.
void cache_flush(Cache &c);

// The total number of blocks.
inline int cache_n_blocks(const Cache &c) { return c.n_sets * c.assoc; }

#endif  // CS3160_CACHE_H

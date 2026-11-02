// The memory interface.
//
// The processor does not know whether it accesses a cache or the RAM, and a
// cache does not know whether the level below it is another cache or the RAM.
// These four functions, reading and writing bytes and blocks, are all that the
// levels of a hierarchy share.
//
//     processor --fetch--> I1 --+
//                               +--> L2 --> RAM
//     processor --data---> L1 --+
//
// read() and write() serve the processor: an access of 1, 2 or 4 bytes at a
// byte address. read_block() and write_block() serve the level above, since
// caches transfer whole blocks, never single bytes.
//
// `latency` is an out-parameter, keeping the data separate from its cost. Every
// implementation sets it to the cycles the access took, including all accesses
// it caused further down the hierarchy.

#ifndef CS3160_MEMORY_H
#define CS3160_MEMORY_H

#include "common.h"
#include "stats.h"

struct MemoryLevel {
    virtual ~MemoryLevel() {}

    // Read `size` bytes at `addr`, little-endian, zero-extended into the
    // returned word. Sign extension, where the instruction requires it, is
    // performed by the processor.
    virtual u32 read(u32 addr, int size, int *latency) = 0;

    // Write the low `size` bytes of `data` at `addr`, little-endian.
    virtual void write(u32 addr, u32 data, int size, int *latency) = 0;

    // Copy the `block_size`-byte block containing `addr` into `dst`. The block
    // is the one starting at addr & ~(block_size - 1).
    virtual void read_block(u32 addr, u8 *dst, int block_size, int *latency) = 0;

    // Store `block_size` bytes from `src` as the block containing `addr`.
    virtual void write_block(u32 addr, const u8 *src, int block_size, int *latency) = 0;

    virtual const char *name() const = 0;

    // This level's counters. A cache obtains those of the level below when the
    // hierarchy is built, so that level_amat() can traverse the chain.
    virtual LevelStats *level_stats() = 0;
};

#endif  // CS3160_MEMORY_H

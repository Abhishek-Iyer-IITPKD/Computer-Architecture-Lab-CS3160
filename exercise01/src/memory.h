// The memory interface in the simulator.
//
// The processor does not know whether it is reading from (or writing to) 
// a cache or the RAM. Similarly, a cache does not know what is below it
// (another cache or RAM). So the four functions (reading/writing bytes and 
// reading/writing blocks) are essential to build a memory hierarchy.
//
//     processor --fetch--> I1 --+
//                               +--> L2 --> RAM
//     processor --data---> L1 --+
//
// The top two calls (read/write) are what the processor uses: an access of 1, 2
// or 4 bytes at a byte address. The bottom two (read_block/write_block) are
// what one level uses to talk to the level below it, because caches move whole
// blocks, never single bytes.
//
// `latency` is an out-parameter rather so that the data and the cost of getting 
// the data stay separate. Every implementation should set it to the
// number of cycles the access took, including everything it triggered
// further down the hierarchy.

#ifndef CS3160_MEMORY_H
#define CS3160_MEMORY_H

#include "common.h"

struct MemoryLevel {
    virtual ~MemoryLevel() {}

    // Read `size` bytes at `addr`, little-endian, zero-extended
    // into the returned word. Sign extension (if required by the 
    // instruction is handled within the processor).
    virtual u32 read(u32 addr, int size, int *latency) = 0;

    // Write the low `size` bytes of `data` at `addr`, little-endian.
    virtual void write(u32 addr, u32 data, int size, int *latency) = 0;

    // Copy the `block_size`-byte block containing `addr` into `dst`. The block
    // is the one starting at addr & ~(block_size - 1).
    virtual void read_block(u32 addr, u8 *dst, int block_size, int *latency) = 0;

    // Store `block_size` bytes from `src` as the block containing `addr`.
    virtual void write_block(u32 addr, const u8 *src, int block_size, int *latency) = 0;

    virtual const char *name() const = 0;
};

#endif  // CS3160_MEMORY_H

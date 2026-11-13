// The simulated RAM: a flat array of bytes covering a fixed address range.
//
// The linker script (custom/test.ld) places .text at 0x80000000 and .data at
// 0x80008000, and crt.S places a 128 KiB stack above the data. One megabyte
// from 0x80000000 covers all of these. Since the range is known before the
// program starts, a flat array suffices.
//
// Two deliberate behaviours:
//   * a byte in range that has never been written reads as 0, since the array
//     starts zeroed; an uninitialised global therefore reads as 0, as it does
//     on hardware where .bss is zeroed.
//   * an access out of range stops the simulation. No correct program makes
//     one, and returning 0 would hide the error.

#ifndef CS3160_RAM_H
#define CS3160_RAM_H

#include <vector>

#include "memory.h"
#include "stats.h"

struct Ram : MemoryLevel {
    std::vector<u8> bytes;  // bytes[i] is the byte at address base + i
    u32             base;
    u32             size;
    int             latency;  // cycles to answer any access
    LevelStats      stats;

    u32 read(u32 addr, int size, int *latency) override;
    void write(u32 addr, u32 data, int size, int *latency) override;
    void read_block(u32 addr, u8 *dst, int block_size, int *latency) override;
    void write_block(u32 addr, const u8 *src, int block_size, int *latency) override;
    const char *name() const override { return "RAM"; }
    LevelStats *level_stats() override { return &stats; }
};

// Create a zeroed RAM covering [base, base + size).
void ram_init(Ram &ram, u32 base, u32 size, int latency);

// Whether every byte of a `size`-byte access at `addr` lies inside the RAM.
bool ram_in_range(const Ram &ram, u32 addr, int size);

// Single-byte access. Out of range is fatal.
u8   ram_read_byte(Ram &ram, u32 addr);
void ram_write_byte(Ram &ram, u32 addr, u8 value);

// Multi-byte access, little-endian: the byte at the lowest address holds the
// least significant bits. `size` must be 1, 2 or 4.
u32  ram_read(Ram &ram, u32 addr, int size);
void ram_write(Ram &ram, u32 addr, u32 data, int size);

// Print the bytes of [start, end) as words, one address per line, to `out`.
// Used by the --dump option.
void ram_dump(Ram &ram, u32 start, u32 end, FILE *out);

#endif  // CS3160_RAM_H

// The simulated RAM: one flat array of bytes covering a fixed address range.
//
// The range comes from the linker script (custom/test.ld), which puts .text at
// 0x80000000 and .data at 0x80008000, and from crt.S, which puts a 128 KiB
// stack above the end of the data. One megabyte from 0x80000000 covers all of
// it, which is why the array is flat rather than a map of touched addresses:
// the whole range is known before the program starts, so there is nothing to
// discover at run time.
//
// Two behaviours are chosen deliberately:
//   * a byte inside the range that no one has written reads as 0. The array
//     starts zeroed, so an uninitialised global reads as 0 as it does
//     on real hardware where the loader zeroes .bss.
//   * an access outside the range stops the simulation. It cannot be a correct
//     program doing it, and returning 0 instead would fail silently.

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
};

// Create a zeroed RAM covering [base, base + size).
void ram_init(Ram &ram, u32 base, u32 size, int latency);

// Is every byte of a `size`-byte access at `addr` inside the RAM?
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

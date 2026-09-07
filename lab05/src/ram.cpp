#include "ram.h"

#include "log.h"

void ram_init(Ram &ram, u32 base, u32 size, int latency) {
    ram.base    = base;
    ram.size    = size;
    ram.latency = latency;
    ram.bytes.assign(size, 0);
    level_stats_init(ram.stats, "RAM", false);
}

bool ram_in_range(const Ram &ram, u32 addr, int size) {
    if (addr < ram.base) {
        return false;
    }
    u32 offset = addr - ram.base;
    // Compare against the size rather than computing offset + size, which
    // could wrap for an address near the top of the address space.
    return offset < ram.size && (u32)size <= ram.size - offset;
}

// The one place an out-of-range address is reported, so the message is the same
// whichever way the access arrived.
static void check_range(const Ram &ram, u32 addr, int size, const char *what) {
    if (!ram_in_range(ram, addr, size)) {
        fatal("%s of %d byte(s) at 0x%08x is outside RAM [0x%08x, 0x%08x)",
              what, size, addr, ram.base, ram.base + ram.size);
    }
}

u8 ram_read_byte(Ram &ram, u32 addr) {
    check_range(ram, addr, 1, "read");
    return ram.bytes[addr - ram.base];
}

void ram_write_byte(Ram &ram, u32 addr, u8 value) {
    check_range(ram, addr, 1, "write");
    ram.bytes[addr - ram.base] = value;
}

u32 ram_read(Ram &ram, u32 addr, int size) {
    if (size != 1 && size != 2 && size != 4) {
        fatal("read of %d bytes at 0x%08x: only 1, 2 and 4 byte accesses exist", size, addr);
    }
    check_range(ram, addr, size, "read");

    u32 offset = addr - ram.base;
    u32 value  = 0;
    for (int i = 0; i < size; i++) {
        value |= (u32)ram.bytes[offset + i] << (8 * i);
    }
    return value;
}

void ram_write(Ram &ram, u32 addr, u32 data, int size) {
    if (size != 1 && size != 2 && size != 4) {
        fatal("write of %d bytes at 0x%08x: only 1, 2 and 4 byte accesses exist", size, addr);
    }
    check_range(ram, addr, size, "write");

    u32 offset = addr - ram.base;
    for (int i = 0; i < size; i++) {
        ram.bytes[offset + i] = (u8)((data >> (8 * i)) & 0xFF);
    }
}

void ram_dump(Ram &ram, u32 start, u32 end, FILE *out) {
    // Round the window out to word boundaries so the columns line up.
    start &= ~3u;
    for (u32 addr = start; addr < end; addr += 4) {
        u32 word = ram_read(ram, addr, 4);
        fprintf(out, "0x%08x: %08x\n", addr, word);
    }
}

// --- MemoryLevel interface -------------------------------------------------
//
// RAM is the bottom of the hierarchy: it always has the data, so its latency is
// the whole cost of the access and there is nothing below it to ask.

u32 Ram::read(u32 addr, int size, int *latency) {
    stats.reads++;
    *latency = this->latency;
    return ram_read(*this, addr, size);
}

void Ram::write(u32 addr, u32 data, int size, int *latency) {
    stats.writes++;
    *latency = this->latency;
    ram_write(*this, addr, data, size);
}

void Ram::read_block(u32 addr, u8 *dst, int block_size, int *latency) {
    stats.reads++;
    *latency = this->latency;

    u32 start = addr & ~((u32)block_size - 1);
    check_range(*this, start, block_size, "block read");
    for (int i = 0; i < block_size; i++) {
        dst[i] = bytes[start - base + i];
    }
}

void Ram::write_block(u32 addr, const u8 *src, int block_size, int *latency) {
    stats.writes++;
    *latency = this->latency;

    u32 start = addr & ~((u32)block_size - 1);
    check_range(*this, start, block_size, "block write");
    for (int i = 0; i < block_size; i++) {
        bytes[start - base + i] = src[i];
    }
}

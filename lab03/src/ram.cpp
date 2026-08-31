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
    // TODO(week 3): read `size` bytes (1, 2 or 4) starting at `addr` and
    // return them as one word.
    //
    // Memory here is a flat array of bytes -- ram.bytes -- and `addr` is an
    // address in the machine's address space, so the index into that array is
    // addr - ram.base. ram_read_byte() above already does that arithmetic.
    //
    // RISC-V is **little-endian**: the byte at the lowest address is the
    // least significant one. So reading four bytes at 0x1000 gives
    //     bytes[0] | bytes[1] << 8 | bytes[2] << 16 | bytes[3] << 24
    // and getting this backwards is a bug.
    //
    // Anything narrower than a word is zero-extended into the returned u32.
    //
    // Call check_range() first, so an address outside the RAM can be named 
    // in the output message (rather than a segfault).
    check_range(ram, addr, size, "read");
    int nread = 0;
    u32 val = 0;
    for(nread=0;nread<size;nread++){
        u8 read = ram_read_byte(ram, addr + nread);
        val = val | (read << nread*8);
    }
    return val;
}

void ram_write(Ram &ram, u32 addr, u32 data, int size) {
    // TODO(week 3): write the low `size` bytes of `data` at `addr`.
    //
    // The mirror of ram_read(): least significant byte at the lowest
    // address. Only `size` bytes are written -- a `sb` to the middle of a
    // word must leave the other three bytes alone, which is what
    // test_ram.cpp's sub-word tests are checking.
    check_range(ram, addr, size, "write");
    int nwritten = 0;
    u8 val;
    for(nwritten=0;nwritten<size;nwritten++){
        val = (u8) (data >> nwritten*8);
        ram_write_byte(ram, addr + nwritten, val);
    }
    // while(nwritten<4){
    //     ram_write_byte(ram, addr + nwritten, 0xff);
    //     nwritten++;
    // }
}

void ram_dump(Ram &ram, u32 start, u32 end, FILE *out) {
    // TODO(week 3): print the words from `start` up to (but not including)
    // `end`, one per line:
    //
    //     0x80000000: 00000297
    //     0x80000004: 01028293
    //
    // The address, a colon and a space, then the word in eight hex digits
    // with leading zeros.
    //
    // Round `start` down to a word boundary first, so the addresses stay
    // aligned however the caller asked. The tests compare your output
    // character for character, so match the format exactly -- lower-case
    // hex, "0x%08x: %08x" and a newline.
    start = start - start%4;
    while(start < end){
        u32 word = ram_read(ram, start, 4);
        fprintf(out, "0x%08x: %08x\n", start, word);
        start += 4;
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

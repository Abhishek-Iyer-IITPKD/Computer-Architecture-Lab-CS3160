// The parts of week 3 the shared RAM tests do not reach: the dump format, the
// loader's return value and its failure cases, and reading back every width at
// every alignment.
//
// test_ram.cpp beside this file is the reference simulator's own suite, kept as
// it is. This file exists because that suite was written to catch regressions
// in working code, and a suite that only does that is a poor way to *grade*
// one: several of its tests are satisfied by a memory that does nothing at all.
// Everything here fails against an empty implementation.

#include "check.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "loader.h"
#include "ram.h"

static const u32 BASE = 0x80000000u;
static const u32 SIZE = 0x10000u;

static void fresh(Ram &ram) { ram_init(ram, BASE, SIZE, 0); }

// Capture what ram_dump() prints. A temporary file rather than a pipe: the
// dump is small and this keeps the test to one screen.
static std::string dumped(Ram &ram, u32 start, u32 end) {
    FILE *f = tmpfile();
    if (f == NULL) {
        return "(could not open a temporary file)";
    }
    ram_dump(ram, start, end, f);
    rewind(f);

    std::string out;
    char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        out.append(buf, n);
    }
    fclose(f);
    return out;
}

// --- reading back what was written ----------------------------------------

TEST(ram_round_trips_a_word) {
    Ram ram;
    fresh(ram);
    ram_write(ram, BASE + 0x40, 0x12345678u, 4);
    CHECK_EQ(ram_read(ram, BASE + 0x40, 4), 0x12345678u);
}

TEST(ram_round_trips_every_width) {
    Ram ram;
    fresh(ram);
    ram_write(ram, BASE + 0x10, 0xAAu, 1);
    ram_write(ram, BASE + 0x20, 0xBEEFu, 2);
    ram_write(ram, BASE + 0x30, 0xDEADBEEFu, 4);

    CHECK_EQ(ram_read(ram, BASE + 0x10, 1), 0xAAu);
    CHECK_EQ(ram_read(ram, BASE + 0x20, 2), 0xBEEFu);
    CHECK_EQ(ram_read(ram, BASE + 0x30, 4), 0xDEADBEEFu);
}

TEST(ram_narrow_reads_are_zero_extended_not_sign_extended) {
    // Whether the sign should be filled in is the *instruction's* business,
    // decided in week 5. Memory hands back the bytes and nothing more.
    Ram ram;
    fresh(ram);
    ram_write(ram, BASE + 0x40, 0xFFFFFFFFu, 4);
    CHECK_EQ(ram_read(ram, BASE + 0x40, 1), 0x000000FFu);
    CHECK_EQ(ram_read(ram, BASE + 0x40, 2), 0x0000FFFFu);
}

TEST(ram_works_at_both_ends_of_its_range) {
    // Off-by-one errors in the base arithmetic hide in the middle of the array
    // and show up here.
    Ram ram;
    fresh(ram);
    ram_write(ram, BASE, 0x11111111u, 4);
    ram_write(ram, BASE + SIZE - 4, 0x22222222u, 4);
    CHECK_EQ(ram_read(ram, BASE, 4), 0x11111111u);
    CHECK_EQ(ram_read(ram, BASE + SIZE - 4, 4), 0x22222222u);
}

TEST(ram_reads_a_word_from_four_separate_byte_writes) {
    // The other direction of the endianness question: bytes written one at a
    // time must read back as the word they spell.
    Ram ram;
    fresh(ram);
    ram_write_byte(ram, BASE + 0x50, 0x78);
    ram_write_byte(ram, BASE + 0x51, 0x56);
    ram_write_byte(ram, BASE + 0x52, 0x34);
    ram_write_byte(ram, BASE + 0x53, 0x12);
    CHECK_EQ(ram_read(ram, BASE + 0x50, 4), 0x12345678u);
}

// --- the dump format -------------------------------------------------------

TEST(dump_prints_one_word_per_line_in_the_documented_format) {
    Ram ram;
    fresh(ram);
    ram_write(ram, BASE, 0x00000093u, 4);
    ram_write(ram, BASE + 4, 0x000A0113u, 4);

    CHECK_STR_EQ(dumped(ram, BASE, BASE + 8).c_str(),
                 "0x80000000: 00000093\n"
                 "0x80000004: 000a0113\n");
}

TEST(dump_pads_to_eight_hex_digits) {
    // "%x" instead of "%08x" is the usual slip, and it only shows on a small
    // value -- every instruction word has a high bit set somewhere.
    Ram ram;
    fresh(ram);
    ram_write(ram, BASE, 1u, 4);
    CHECK_STR_EQ(dumped(ram, BASE, BASE + 4).c_str(), "0x80000000: 00000001\n");
}

TEST(dump_stops_before_the_end_address) {
    // The window is half-open: [start, end), like every other range here.
    Ram ram;
    fresh(ram);
    CHECK_STR_EQ(dumped(ram, BASE, BASE + 4).c_str(), "0x80000000: 00000000\n");
}

TEST(dump_rounds_the_start_down_to_a_word) {
    // Asked for an unaligned address, the columns still line up.
    Ram ram;
    fresh(ram);
    ram_write(ram, BASE, 0xAABBCCDDu, 4);
    CHECK_STR_EQ(dumped(ram, BASE + 2, BASE + 4).c_str(),
                 "0x80000000: aabbccdd\n");
}

// --- the loader ------------------------------------------------------------

// Write `n` bytes to a temporary file and return its path.
static std::string image_of(const u8 *bytes, size_t n, char *path) {
    strcpy(path, "/tmp/cs3160-image-XXXXXX");
    int fd = mkstemp(path);
    if (fd < 0) {
        return "";
    }
    FILE *f = fdopen(fd, "wb");
    fwrite(bytes, 1, n, f);
    fclose(f);
    return std::string(path);
}

TEST(loader_returns_the_number_of_bytes_it_copied) {
    Ram ram;
    fresh(ram);
    const u8 bytes[] = {0x93, 0x00, 0x00, 0x00, 0x13, 0x01, 0x0A, 0x00};
    char path[64];
    std::string p = image_of(bytes, sizeof(bytes), path);

    CHECK_EQ(loader_load(ram, p.c_str(), BASE), (u32)sizeof(bytes));
    remove(path);
}

TEST(loader_puts_byte_n_of_the_file_at_load_addr_plus_n) {
    Ram ram;
    fresh(ram);
    const u8 bytes[] = {0x93, 0x00, 0x00, 0x00, 0x13, 0x01, 0x0A, 0x00};
    char path[64];
    std::string p = image_of(bytes, sizeof(bytes), path);
    loader_load(ram, p.c_str(), BASE);
    remove(path);

    for (size_t i = 0; i < sizeof(bytes); i++) {
        CHECK_EQ(ram_read_byte(ram, BASE + (u32)i), bytes[i]);
    }
    // And read back as words, which is how the rest of the course sees them.
    CHECK_EQ(ram_read(ram, BASE, 4), 0x00000093u);
    CHECK_EQ(ram_read(ram, BASE + 4, 4), 0x000A0113u);
}

TEST(loader_honours_a_load_address_that_is_not_the_base) {
    Ram ram;
    fresh(ram);
    const u8 bytes[] = {0xDE, 0xAD, 0xBE, 0xEF};
    char path[64];
    std::string p = image_of(bytes, sizeof(bytes), path);
    loader_load(ram, p.c_str(), BASE + 0x100);
    remove(path);

    CHECK_EQ(ram_read(ram, BASE + 0x100, 4), 0xEFBEADDEu);
    CHECK_EQ(ram_read(ram, BASE, 4), 0u);   // nothing landed at the base
}

// Not tested here: an empty or unreadable image. Both are meant to call
// fatal(), which ends the process, so a test for them would take the rest of
// the suite with it -- as one written here did, and the run then looked like a
// clean pass because the tests that never ran also never failed. The harness
// checks that behaviour instead, by running your simulator on a real file.

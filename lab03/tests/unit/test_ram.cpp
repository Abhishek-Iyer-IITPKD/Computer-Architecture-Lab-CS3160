// RAM and loader: endianness, sub-word access, range checking, block traffic.

#include "check.h"

#include "loader.h"
#include "ram.h"

static const u32 BASE = 0x80000000u;
static const u32 SIZE = 0x10000u;

static void fresh(Ram &ram) { ram_init(ram, BASE, SIZE, 0); }

TEST(ram_starts_zeroed) {
    Ram ram;
    fresh(ram);
    CHECK_EQ(ram_read_byte(ram, BASE), 0);
    CHECK_EQ(ram_read(ram, BASE + 0x1000, 4), 0u);
}

TEST(ram_is_little_endian) {
    Ram ram;
    fresh(ram);
    ram_write(ram, BASE + 4, 0xDEADBEEFu, 4);

    // Least significant byte at the lowest address.
    CHECK_EQ(ram_read_byte(ram, BASE + 4), 0xEF);
    CHECK_EQ(ram_read_byte(ram, BASE + 5), 0xBE);
    CHECK_EQ(ram_read_byte(ram, BASE + 6), 0xAD);
    CHECK_EQ(ram_read_byte(ram, BASE + 7), 0xDE);

    CHECK_EQ(ram_read(ram, BASE + 4, 4), 0xDEADBEEFu);
    CHECK_EQ(ram_read(ram, BASE + 4, 2), 0xBEEFu);
    CHECK_EQ(ram_read(ram, BASE + 6, 2), 0xDEADu);
    CHECK_EQ(ram_read(ram, BASE + 4, 1), 0xEFu);
}

TEST(ram_sub_word_write_leaves_neighbours_alone) {
    Ram ram;
    fresh(ram);
    ram_write(ram, BASE + 8, 0xFFFFFFFFu, 4);
    ram_write(ram, BASE + 9, 0x00u, 1);
    CHECK_EQ(ram_read(ram, BASE + 8, 4), 0xFFFF00FFu);

    ram_write(ram, BASE + 10, 0x1234u, 2);
    CHECK_EQ(ram_read(ram, BASE + 8, 4), 0x123400FFu);
}

TEST(ram_write_keeps_only_the_requested_bytes) {
    Ram ram;
    fresh(ram);
    // A byte store of a word-sized value must write one byte, not four.
    ram_write(ram, BASE + 0x20, 0xAABBCCDDu, 1);
    CHECK_EQ(ram_read(ram, BASE + 0x20, 4), 0x000000DDu);
}

TEST(ram_range_check) {
    Ram ram;
    fresh(ram);
    CHECK(ram_in_range(ram, BASE, 4));
    CHECK(ram_in_range(ram, BASE + SIZE - 4, 4));
    CHECK(!ram_in_range(ram, BASE + SIZE - 3, 4));  // last word straddles the end
    CHECK(!ram_in_range(ram, BASE + SIZE, 1));
    CHECK(!ram_in_range(ram, BASE - 1, 1));
    CHECK(!ram_in_range(ram, 0, 4));
    CHECK(!ram_in_range(ram, 0xFFFFFFFFu, 4));      // must not wrap around
}

TEST(ram_block_access_uses_the_containing_block) {
    Ram ram;
    fresh(ram);
    for (int i = 0; i < 64; i++) {
        ram_write_byte(ram, BASE + 0x100 + i, (u8)i);
    }

    u8  block[64];
    int latency = -1;
    // An address in the middle of the block must fetch the whole block.
    ram.read_block(BASE + 0x100 + 37, block, 64, &latency);
    CHECK_EQ(latency, 0);
    for (int i = 0; i < 64; i++) {
        CHECK_EQ(block[i], i);
    }

    for (int i = 0; i < 64; i++) {
        block[i] = (u8)(0x80 + i);
    }
    ram.write_block(BASE + 0x100 + 5, block, 64, &latency);
    CHECK_EQ(ram_read_byte(ram, BASE + 0x100), 0x80);
    CHECK_EQ(ram_read_byte(ram, BASE + 0x13F), 0xBF);
}

TEST(ram_reports_its_latency) {
    Ram ram;
    ram_init(ram, BASE, SIZE, 100);
    int latency = -1;
    ram.read(BASE, 4, &latency);
    CHECK_EQ(latency, 100);
    ram.write(BASE, 0, 4, &latency);
    CHECK_EQ(latency, 100);
    CHECK_EQ(ram.stats.reads, 1);
    CHECK_EQ(ram.stats.writes, 1);
}

TEST(loader_copies_the_image_verbatim) {
    // Write a small image, load it, and check every byte landed where the
    // linker script says it should.
    const char *path = "build/test_image.r5ob";
    FILE       *f    = fopen(path, "wb");
    CHECK(f != NULL);
    if (f == NULL) {
        return;
    }
    const u8 image[] = {0x93, 0x00, 0x10, 0x00, 0xEF, 0xBE, 0xAD, 0xDE};
    fwrite(image, 1, sizeof(image), f);
    fclose(f);

    Ram ram;
    fresh(ram);
    u32 loaded = loader_load(ram, path, BASE);
    CHECK_EQ(loaded, sizeof(image));
    CHECK_EQ(ram_read(ram, BASE, 4), 0x00100093u);      // addi x1, x0, 1
    CHECK_EQ(ram_read(ram, BASE + 4, 4), 0xDEADBEEFu);
    CHECK_EQ(ram_read(ram, BASE + 8, 4), 0u);           // nothing beyond the image

    remove(path);
}

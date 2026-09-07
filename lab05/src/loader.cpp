#include "loader.h"

#include "log.h"

u32 loader_load(Ram &ram, const char *path, u32 load_addr) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fatal("cannot open program image '%s'", path);
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fatal("cannot measure program image '%s'", path);
    }
    long length = ftell(f);
    if (length < 0) {
        fatal("cannot measure program image '%s'", path);
    }
    rewind(f);

    if (length == 0) {
        fatal("program image '%s' is empty", path);
    }
    if (!ram_in_range(ram, load_addr, (int)length)) {
        fatal("program image '%s' is %ld bytes at 0x%08x, which does not fit in "
              "RAM [0x%08x, 0x%08x)",
              path, length, load_addr, ram.base, ram.base + ram.size);
    }

    u32 offset = load_addr - ram.base;
    size_t got = fread(&ram.bytes[offset], 1, (size_t)length, f);
    if (got != (size_t)length) {
        fatal("read only %zu of %ld bytes from '%s'", got, length, path);
    }
    fclose(f);

    log_info("loaded %ld bytes of '%s' at 0x%08x", length, path, load_addr);
    return (u32)length;
}

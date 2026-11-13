// Loading a program image into RAM.
//
// The simulator reads .r5ob files, the flat binary produced from the ELF by
//
//     riscv-none-elf-objcopy -O binary prog.r5o prog.r5ob
//
// A flat image is exactly the bytes of the loadable sections, with any gaps
// between them zero-filled, and no header. Loading is therefore a copy to the
// lowest load address, 0x80000000 (fixed by custom/test.ld), and no ELF parser
// is needed.

#ifndef CS3160_LOADER_H
#define CS3160_LOADER_H

#include "ram.h"

// Copy the whole of `path` into `ram` from `load_addr`; return the number of
// bytes loaded. A missing or unreadable file, or an image too large for the
// RAM, is fatal.
u32 loader_load(Ram &ram, const char *path, u32 load_addr);

#endif  // CS3160_LOADER_H

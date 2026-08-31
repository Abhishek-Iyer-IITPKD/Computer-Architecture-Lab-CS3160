// Loading a program image into RAM.
//
// The simulator reads .r5ob files: the flat binary that
//
//     riscv-none-elf-objcopy -O binary prog.r5o prog.r5ob
//
// produces from the ELF. A flat image is exactly the bytes of the loadable
// sections, with the gaps between them filled with zeroes, and no header of any
// kind -- so loading it is a straight copy to the lowest load address, and the
// simulator needs no ELF parser. That address is 0x80000000, fixed by
// custom/test.ld. Hence `load_addr` has a default value.

#ifndef CS3160_LOADER_H
#define CS3160_LOADER_H

#include "ram.h"

// Copy the whole of `path` into `ram` starting at `load_addr`. Returns the
// number of bytes loaded. A missing file, an unreadable file, or an image too
// large for the RAM is fatal.
u32 loader_load(Ram &ram, const char *path, u32 load_addr);

#endif  // CS3160_LOADER_H

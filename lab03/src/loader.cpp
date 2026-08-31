#include "loader.h"

#include "log.h"

u32 loader_load(Ram &ram, const char *path, u32 load_addr) {
    // TODO(week 3): copy a program image into memory.
    //
    // A .r5ob file is not an ELF. It is exactly the bytes that 
    // belong in memory (in the same order, with no prefix or suffix).
    // It is a dump of the memory regions just before the program 
    // starts executing on the machine. 

    // Use ram_write_byte() rather than touching ram.bytes directly, so that
    // an image too big for the configured RAM is caught here and reported
    // instead of running off the end of the array.
    //
    // A missing or unreadable file should call fatal() with the path in the
    // message.

    FILE* f = fopen(path, "rb");
    if(f==NULL) fatal("could not open file at path %s", path);
    char byte;
    int n = 0;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    u8* buf = (u8*) malloc(size);
    size_t read_size = fread(buf, 1, size, f);
    for(u32 i=0;i<read_size;i++){
        ram_write_byte(ram, load_addr+i, buf[i]);
    }

    free(buf);
    fclose(f);

    return size;
}

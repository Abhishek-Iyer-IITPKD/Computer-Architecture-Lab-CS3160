#include "cache.h"

#include "log.h"

void cache_init(Cache &c, const CacheConfig &cfg, MemoryLevel *next) {
    if (next == NULL) {
        fatal("%s cache has nothing below it", cfg.name);
    }

    c.level_name   = cfg.name;
    c.latency      = cfg.latency;
    c.block_size   = cfg.block_size;
    c.mapping      = cfg.mapping;
    c.replacement  = cfg.replacement;
    c.write_policy = cfg.write_policy;
    c.write_miss   = cfg.write_miss;
    c.next         = next;

    int n_blocks = cfg.size / cfg.block_size;

    // TODO(lab 9): turn the mapping into a geometry.
    //
    // Direct-mapped, set-associative and fully associative caches are one
    // implementation with different numbers of blocks per set, decided
    // only here:
    //
    //     MAP_DIRECT   one block per set
    //     MAP_SET      cfg.assoc blocks per set
    //     MAP_FULL     one set, holding every block
    //
    // Set c.assoc, then c.n_sets = n_blocks / c.assoc. All other code
    // (decomposition, lookup, replacement) reads only these two values.
    //
    // The direct-mapped case is provided so that the starter runs.
    if(cfg.mapping == MAP_DIRECT) c.assoc  = 1;
    else if(cfg.mapping == MAP_SET) c.assoc = cfg.assoc;
    else if(cfg.mapping == MAP_FULL) c.assoc = n_blocks;
    c.n_sets = n_blocks / c.assoc;

    CacheBlock empty;
    empty.valid = false;
    empty.dirty = false;
    empty.tag   = 0;
    c.blocks.assign((size_t)n_blocks, empty);
    c.data.assign((size_t)n_blocks * (size_t)cfg.block_size, 0);
    c.order.assign((size_t)c.n_sets, std::vector<int>());

    level_stats_init(c.stats, cfg.name, true, cfg.latency);
    c.stats.below = next->level_stats();

    log_debug("%s cache: %d B in %d blocks of %d B, %d set(s) of %d way(s), "
              "%s, %s, %s, latency %d",
              c.level_name, cfg.size, n_blocks, c.block_size, c.n_sets, c.assoc,
              mapping_name(c.mapping), replacement_name(c.replacement),
              write_policy_name(c.write_policy), c.latency);
}

CacheAddr cache_split(const Cache &c, u32 addr) {
    // TODO(lab 9): cut an address into tag, set and offset.
    //
    //     +---------------------+-----------+--------+
    //     |         tag         |    set    | offset |
    //     +---------------------+-----------+--------+
    //
    // The offset selects a byte within the block: the low
    // log2(block_size) bits. The set is the next log2(n_sets) bits, and the
    // tag the remaining upper bits.
    //
    // log2_exact() (common.h) gives the bit counts; the configuration has
    // already been checked to contain only powers of two.
    //
    // A fully associative cache has one set, so log2_exact(1) is 0: no set
    // bits. Shifting a u32 by 32 is undefined in C++, so handle the case of
    // zero set bits explicitly.
    CacheAddr a;
    int offset_a = log2_exact(c.block_size);
    int offset_b = offset_a + log2_exact(c.n_sets);
    a.offset = bits(addr, offset_a-1, 0);
    a.set    = c.n_sets == 1 ? 0 : bits(addr, offset_b-1, offset_a);
    a.tag    = bits(addr, 31, offset_b);
    return a;
}

u32 cache_block_addr(const Cache &c, u32 set, u32 tag) {
    // TODO(lab 9): the inverse of cache_split().
    //
    // Place the tag above the set bits and the set above the offset bits,
    // with a zero offset: the result is the address of the first byte of
    // the block.
    //
    // A dirty block being evicted records only its tag and set, from which
    // this reconstructs the address to which it is written back.
    u32 addr = tag;
    addr = addr << log2_exact(c.n_sets);
    addr = addr | set;
    addr = addr << log2_exact(c.block_size);
    return addr;
}

int cache_find(const Cache &c, u32 set, u32 tag) {
    // TODO(lab 9): the lookup, which distinguishes a hit from a miss.
    //
    // Search every way of the set. A way matches if it is valid *and* its
    // tag is equal; an invalid way matches nothing, whatever its tag.
    // Return the way, or -1.
    //
    // The blocks of set s are c.blocks[s * c.assoc + 0 .. c.assoc - 1]. A
    // direct-mapped cache searches one way; a fully associative cache has a
    // single set and searches every block.
    for(int i=0; i<c.assoc; i++){
        if(c.blocks[set * c.assoc + i].valid && (c.blocks[set * c.assoc + i].tag == tag)) return i;
    }
    return -1;
}

u8 *cache_block_data(Cache &c, u32 set, int way) {
    return &c.data[((size_t)set * c.assoc + way) * c.block_size];
}

CacheBlock &cache_block_at(Cache &c, u32 set, int way) {
    return c.blocks[(size_t)set * c.assoc + way];
}

// TODO(lab 9): maintain the replacement order.
//
// c.order[set] is a list of way numbers; the way at the *front* is the
// next to be replaced.
//
// cache_order_insert: a block has just been placed in `way`, so move it
// to the back, removing any existing entry first; a way must appear only
// once.
//
// cache_order_touch: `way` has just been read or written. FIFO depends
// only on arrival order, so does nothing; LRU does as insert does.
//
// That condition is the entire difference between the two policies.
void cache_order_insert(Cache &c, u32 set, int way) {
    std::vector<int> new_order;
    for(u32 i=0; i < c.order[set].size(); i++){
        if(c.order[set][i] != way) new_order.push_back(c.order[set][i]);
    }
    new_order.push_back(way);
    c.order[set] = new_order;
}

void cache_order_touch(Cache &c, u32 set, int way) {
    if(c.replacement == REPL_LRU) cache_order_insert(c, set, way);
}

// TODO(lab 9): choose a way, and clear it.
//
// cache_choose_way: prefer an invalid way, which can be filled at no
// cost. When every way is valid, return c.order[set].front(), the next
// to be replaced under either policy. A full set with an empty order
// list means the order is not being maintained: call fatal().
//
// cache_evict_for: take that way. If it holds a valid block, count an
// eviction, pass the block to cache_writeback_if_dirty() and count a
// writeback if it wrote, then mark the way invalid. Return the way.
//
// Taking a way that was already invalid is not an eviction: nothing is
// removed.
int cache_choose_way(Cache &c, u32 set) {
    for(int i=0; i<c.assoc; i++) if(!c.blocks[set * c.assoc + i].valid) return i;
    if(c.order[set].empty()) fatal("Set %d is full but order list is empty", set);
    return c.order[set].front();
}

int cache_evict_for(Cache &c, u32 set, int *latency) {
    int way = cache_choose_way(c, set);
    CacheBlock b = cache_block_at(c, set, way);
    if(b.valid){
        int down = 0;
        c.stats.writebacks += cache_writeback_if_dirty(c, set, way, latency);
        *latency += down;
        b.valid = false;
        c.blocks[set*c.assoc + way] = b;
        c.stats.evictions += 1;
    }
    return way;
}

bool cache_writeback_if_dirty(Cache &c, u32 set, int way, int *latency) {
    // TODO(lab 10): write a dirty block back before it is lost.
    //
    // Unless the block is valid *and* dirty, return false: a clean block is
    // identical to the copy below.
    //
    // Otherwise reconstruct its address with cache_block_addr(), write it
    // with c.next->write_block(), add the cost to *latency, mark the block
    // clean, and return true.
    //
    // Marking it clean matters: cache_flush() also calls this function, and
    // a block written back twice reaches the level below twice.
    //
    // Do not update the statistics here; whether the writeback counts as
    // program traffic depends on the caller, which counts it.
    if(!c.blocks[set * c.assoc + way].valid || !c.blocks[set * c.assoc + way].dirty) return false;
    c.next->write_block(cache_block_addr(c, set, c.blocks[set * c.assoc + way].tag), cache_block_data(c, set, way), c.block_size, latency);
    c.blocks[set * c.assoc + way].valid = false;
    c.blocks[set * c.assoc + way].dirty = false;
    return true;
}

int cache_fill(Cache &c, u32 addr, int *latency) {
    // TODO(lab 9): fetch a block from the level below.
    //
    // Decompose the address, make room in its set with cache_evict_for(),
    // and read the whole block with c.next->read_block() into
    // cache_block_data() for that way. Add the cost to *latency; the
    // caller has already charged this level's lookup.
    //
    // Then set the block record: valid, clean (a fresh copy of the level
    // below), with this address's tag. Insert the way into the replacement
    // order and return it.
    CacheAddr a = cache_split(c, addr);
    int way = cache_evict_for(c, a.set, &c.latency);
    int down = 0;
    c.next->read_block(addr, cache_block_data(c, a.set, way), c.block_size, &down);
    *latency += down;
    CacheBlock b = cache_block_at(c, a.set, way);
    b.valid = true;
    b.dirty = false;
    b.tag = a.tag;
    c.blocks[a.set * c.assoc + way] = b;
    cache_order_insert(c, a.set, way);
    return way;
}

void cache_check_within_block(const Cache &c, u32 addr, int size) {
    u32 offset = cache_split(c, addr).offset;
    if (offset + (u32)size > (u32)c.block_size) {
        fatal("%s cache: a %d-byte access at 0x%08x crosses a block boundary "
              "(offset %u in a %d-byte block). Misaligned accesses are not "
              "supported.",
              c.level_name, size, addr, offset, c.block_size);
    }
}

void cache_check_block_size(const Cache &c, int block_size) {
    if (block_size != c.block_size) {
        fatal("%s cache: asked for a %d-byte block but its blocks are %d bytes",
              c.level_name, block_size, c.block_size);
    }
}

u32 Cache::read(u32 addr, int size, int *latency) {
    // TODO(lab 9): a read.
    //
    // Count the read and charge this level's latency (`this->latency`, not
    // the `latency` out-parameter), which a lookup costs whether it hits
    // or misses.
    //
    // Decompose the address and look it up. On a hit, count it and record
    // the use in the replacement order. On a miss, count it and call
    // cache_fill(), which returns the way filled.
    //
    // Return `size` bytes from that way, starting at a.offset,
    // little-endian. The bytes come from the block, not from the level
    // below, which after a fill this level holds.
    //
    // The placeholder below passes every access to the level below. It is
    // correct but caches nothing, so every counter stays at zero.
    cache_check_within_block(*this, addr, size);
    *latency = this->latency;
    CacheAddr a = cache_split(*this, addr);
    int way = cache_find(*this, a.set, a.tag);
    if(way == -1){
        this->stats.read_misses += 1;
        int down = 0;
        cache_fill(*this, addr, &down);
        *latency += down;
    } else{
        this->stats.read_hits += 1;
        cache_order_touch(*this, a.set, way);
    }
    this->stats.reads += 1;
    u32 value = 0;
    u8 *data_ptr = cache_block_data(*this, a.set, way) + a.offset + size - 1;
    for(int i=0; i<size; i++){
        value = (value << 8) | (u32) *data_ptr;
        data_ptr--;
    }
    return value;
}

void Cache::write(u32 addr, u32 value, int size, int *latency) {
    // TODO(lab 10): a write, under the two write policies.
    //
    // As for a read: count it, charge this level's latency, decompose the
    // address and look it up. A hit is recorded as a use of the block.
    //
    // On a miss, `write_miss` decides:
    //   WM_NOALLOCATE  pass the write down with next->write() and return
    //                  once its cost is added; nothing here becomes dirty.
    //   WM_ALLOCATE    cache_fill() the block first, since a partial write
    //                  must preserve the rest of the block.
    //
    // Then write the low `size` bytes of `value` into the block at
    // a.offset, little-endian, mirroring the read.
    //
    // `write_policy` decides what the level below receives:
    //   WP_WRITEBACK      nothing yet; mark the block dirty, since this
    //                     level now holds the only current copy.
    //   WP_WRITETHROUGH   the write as well, every time; no block is ever
    //                     dirty.
    //
    // The placeholder below invalidates any copy at this level (which would
    // otherwise become stale) and passes the write down. It is correct but
    // caches nothing, so no write hits and no block becomes dirty.
    cache_check_within_block(*this, addr, size);
    *latency = this->latency;
    CacheAddr a = cache_split(*this, addr);
    int way = cache_find(*this, a.set, a.tag);
    if (way >= 0) {
        cache_block_at(*this, a.set, way).valid = false;
    }
    int down = 0;
    next->write(addr, value, size, &down);
    *latency += down;
}

// The block interface between levels. Caches transfer whole blocks, and the
// hierarchy is checked at start-up to use a single block size, so these never
// split or merge a block.

void Cache::read_block(u32 addr, u8 *dst, int bsize, int *latency) {
    // TODO(lab 10): a block read requested by the level above.
    //
    // As Cache::read(), with two differences: there is no offset, since a
    // whole block is requested, and the bytes are copied into `dst` rather
    // than assembled into a word.
    //
    // This is called only when a cache above this one misses.
    //
    // The placeholder below passes the request on. It is correct, but this
    // level keeps no copy.
    cache_check_block_size(*this, bsize);
    *latency = this->latency;
    int down = 0;
    next->read_block(addr, dst, bsize, &down);
    *latency += down;
}

void Cache::write_block(u32 addr, const u8 *src, int bsize, int *latency) {
    // TODO(lab 10): a block written back by the level above on evicting a
    // dirty block.
    //
    // As Cache::write(), with one difference: on a miss under WM_ALLOCATE
    // the whole block is being written, so there is nothing to preserve.
    // Take a way with cache_evict_for() and set the record directly
    // (valid, clean, this tag, inserted into the replacement order) rather
    // than calling cache_fill(), which would read a block about to be
    // overwritten entirely.
    //
    // Otherwise as Cache::write(): WM_NOALLOCATE passes the block down,
    // WP_WRITEBACK marks it dirty, WP_WRITETHROUGH also passes it down.
    //
    // The placeholder below, as in Cache::write(), invalidates any copy at
    // this level and passes the block on.
    cache_check_block_size(*this, bsize);
    *latency = this->latency;
    CacheAddr a = cache_split(*this, addr);
    int way = cache_find(*this, a.set, a.tag);
    if (way >= 0) {
        cache_block_at(*this, a.set, way).valid = false;
    }
    int down = 0;
    next->write_block(addr, src, bsize, &down);
    *latency += down;
}

void cache_flush(Cache &c) {
    // TODO(lab 10): write every dirty block down, so that at the end of a
    // run the RAM holds the program's final state and --dump is current.
    //
    // Apply cache_writeback_if_dirty() to every way of every set. Discard
    // the latency it reports and leave the statistics unchanged: this
    // traffic makes the final state visible and is not an action of the
    // program.
    (void)c;
}

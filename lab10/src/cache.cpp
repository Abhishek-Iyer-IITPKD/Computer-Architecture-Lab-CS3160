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

    // The mapping determines the geometry here, and nowhere else.
    switch (cfg.mapping) {
        case MAP_DIRECT: c.assoc = 1;        break;
        case MAP_FULL:   c.assoc = n_blocks; break;
        case MAP_SET:    c.assoc = cfg.assoc; break;
    }
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
    int offset_bits = log2_exact((u32)c.block_size);
    int set_bits    = log2_exact((u32)c.n_sets);

    CacheAddr a;
    a.offset = addr & ((1u << offset_bits) - 1u);
    // A fully associative cache has one set and no set bits, so its set is 0.
    // The case is explicit because the general mask would shift by 32.
    a.set = (set_bits == 0) ? 0u : ((addr >> offset_bits) & ((1u << set_bits) - 1u));
    a.tag = addr >> (offset_bits + set_bits);
    return a;
}

u32 cache_block_addr(const Cache &c, u32 set, u32 tag) {
    int offset_bits = log2_exact((u32)c.block_size);
    int set_bits    = log2_exact((u32)c.n_sets);
    return (tag << (offset_bits + set_bits)) | (set << offset_bits);
}

int cache_find(const Cache &c, u32 set, u32 tag) {
    for (int way = 0; way < c.assoc; way++) {
        const CacheBlock &b = c.blocks[(size_t)set * c.assoc + way];
        if (b.valid && b.tag == tag) {
            return way;
        }
    }
    return -1;
}

u8 *cache_block_data(Cache &c, u32 set, int way) {
    return &c.data[((size_t)set * c.assoc + way) * c.block_size];
}

CacheBlock &cache_block_at(Cache &c, u32 set, int way) {
    return c.blocks[(size_t)set * c.assoc + way];
}

void cache_order_insert(Cache &c, u32 set, int way) {
    std::vector<int> &o = c.order[set];
    for (size_t i = 0; i < o.size(); i++) {
        if (o[i] == way) {
            o.erase(o.begin() + i);
            break;
        }
    }
    o.push_back(way);
}

void cache_order_touch(Cache &c, u32 set, int way) {
    if (c.replacement == REPL_LRU) {
        cache_order_insert(c, set, way);
    }
}

int cache_choose_way(Cache &c, u32 set) {
    for (int way = 0; way < c.assoc; way++) {
        if (!cache_block_at(c, set, way).valid) {
            return way;
        }
    }
    std::vector<int> &o = c.order[set];
    if (o.empty()) {
        fatal("%s cache: set %u is full but its replacement order is empty",
              c.level_name, set);
    }
    return o.front();
}

int cache_evict_for(Cache &c, u32 set, int *latency) {
    int         way = cache_choose_way(c, set);
    CacheBlock &b   = cache_block_at(c, set, way);

    if (b.valid) {
        c.stats.evictions++;
        if (cache_writeback_if_dirty(c, set, way, latency)) {
            c.stats.writebacks++;
        }
        b.valid = false;
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
    CacheBlock &b = cache_block_at(c, set, way);
    if(b.valid==false || b.dirty==false) return false;
    int down = 0;
    c.next->write_block(cache_block_addr(c, set, b.tag), cache_block_data(c, set, way), c.block_size, &down);
    *latency += down;
    b.dirty = false;
    return true;
}

int cache_fill(Cache &c, u32 addr, int *latency) {
    CacheAddr a   = cache_split(c, addr);
    int       way = cache_evict_for(c, a.set, latency);

    int down = 0;
    c.next->read_block(addr, cache_block_data(c, a.set, way), c.block_size, &down);
    *latency += down;

    CacheBlock &b = cache_block_at(c, a.set, way);
    b.valid = true;
    b.dirty = false;
    b.tag   = a.tag;
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
    cache_check_within_block(*this, addr, size);
    stats.reads++;
    *latency = this->latency;  // a lookup costs this, hit or miss

    CacheAddr a   = cache_split(*this, addr);
    int       way = cache_find(*this, a.set, a.tag);

    if (way >= 0) {
        stats.read_hits++;
        cache_order_touch(*this, a.set, way);
    } else {
        stats.read_misses++;
        way = cache_fill(*this, addr, latency);
    }

    const u8 *bytes = cache_block_data(*this, a.set, way);
    u32       value = 0;
    for (int i = 0; i < size; i++) {
        value |= (u32)bytes[a.offset + i] << (8 * i);
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
    stats.writes++;
    *latency = this->latency;

    CacheAddr a = cache_split(*this, addr);
    int way = cache_find(*this, a.set, a.tag);

    if (way >= 0) {
        stats.write_hits++;
        cache_order_touch(*this, a.set, way);
    } else {
        stats.write_misses++;
        if(this->write_miss == WM_ALLOCATE){
            int down = 0;
            way = cache_fill(*this, addr, &down);
            *latency += down;
        }
        else if(this->write_miss == WM_NOALLOCATE){
            int down = 0;
            next->write(addr, value, size, &down);
            *latency += down;
            return;
        }
    }

    u8* dst = cache_block_data(*this, a.set, way) + a.offset;
    for (int i = 0; i < size; i++) {
        dst[i] = (value>>(8*i)) & 0xFF;
    }

    if(this->write_policy == WP_WRITEBACK) cache_block_at(*this, a.set, way).dirty = true;
    else if(this->write_policy == WP_WRITETHROUGH){
        cache_block_at(*this, a.set, way).dirty = false;
        int down = 0;
        this->next->write(addr, value, size, &down);
        *latency += down;
    }
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
    stats.reads++;
    *latency = this->latency;

    CacheAddr a   = cache_split(*this, addr);
    int way = cache_find(*this, a.set, a.tag);
    
    if (way >= 0) {
        stats.read_hits++;
        cache_order_touch(*this, a.set, way);
    } else {
        stats.read_misses++;
        int down = 0;
        way = cache_fill(*this, addr, &down);
        *latency += down;
    }

    u8* bytes = cache_block_data(*this, a.set, way);
    for(int i=0;i<bsize;i++) dst[i] = bytes[i];
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
    stats.writes++;
    *latency = this->latency;

    CacheAddr a = cache_split(*this, addr);
    int way = cache_find(*this, a.set, a.tag);

    if (way >= 0) {
        stats.write_hits++;
        cache_order_touch(*this, a.set, way);
    } else {
        stats.write_misses++;
        if(this->write_miss == WM_ALLOCATE){
            way = cache_evict_for(*this, a.set, latency);
            CacheBlock &b = cache_block_at(*this, a.set, way);
            b.valid = true;
            b.dirty = false;
            b.tag   = a.tag;
            cache_order_insert(*this, a.set, way);
        } else if(this->write_miss == WM_NOALLOCATE){
            int down = 0;
            next->write_block(addr, src, bsize, &down);
            *latency += down;
            return;
        }
    }

    u8* dst = cache_block_data(*this, a.set, way);
    for (int i = 0; i < bsize; i++) {
        dst[i] = src[i];
    }

    if(this->write_policy == WP_WRITEBACK) cache_block_at(*this, a.set, way).dirty = true;
    else if(this->write_policy == WP_WRITETHROUGH){
        int down = 0;
        next->write_block(addr, src, bsize, &down);
        *latency += down;
    }
}

void cache_flush(Cache &c) {
    // TODO(lab 10): write every dirty block down, so that at the end of a
    // run the RAM holds the program's final state and --dump is current.
    //
    // Apply cache_writeback_if_dirty() to every way of every set. Discard
    // the latency it reports and leave the statistics unchanged: this
    // traffic makes the final state visible and is not an action of the
    // program.
    int latency;
    for(int s=0;s<c.n_sets;s++){
        for(int w=0;w<c.assoc;w++) 
            cache_writeback_if_dirty(c, s, w, &latency);
    }
}

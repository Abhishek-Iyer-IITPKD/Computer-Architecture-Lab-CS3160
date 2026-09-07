#include "config.h"

#include <cstring>
#include <string>

#include "log.h"

// ---------------------------------------------------------------------------
// Names
// ---------------------------------------------------------------------------

const char *proc_kind_name(ProcKind k) {
    switch (k) {
        case PROC_SINGLE:     return "single";
        case PROC_PIPELINED:  return "pipelined";
        case PROC_FORWARDING: return "forwarding";
    }
    return "?";
}

const char *mapping_name(Mapping m) {
    switch (m) {
        case MAP_DIRECT: return "direct";
        case MAP_SET:    return "set";
        case MAP_FULL:   return "full";
    }
    return "?";
}

const char *replacement_name(Replacement r) {
    return (r == REPL_FIFO) ? "FIFO" : "LRU";
}

const char *write_policy_name(WritePolicy w) {
    return (w == WP_WRITEBACK) ? "write-back" : "write-through";
}

const char *write_miss_name(WriteMiss w) {
    return (w == WM_ALLOCATE) ? "write-allocate" : "no-write-allocate";
}

const char *resolve_stage_name(ResolveStage s) {
    switch (s) {
        case RESOLVE_ID:  return "ID";
        case RESOLVE_EX:  return "EX";
        case RESOLVE_MEM: return "MEM";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Defaults
// ---------------------------------------------------------------------------

static void cache_defaults(CacheConfig &c, const char *name, bool valid, int latency,
                           int size, int block_size, int assoc, Mapping mapping,
                           Replacement repl) {
    c.name         = name;
    c.valid        = valid;
    c.latency      = latency;
    c.size         = size;
    c.block_size   = block_size;
    c.assoc        = assoc;
    c.mapping      = mapping;
    c.replacement  = repl;
    c.write_policy = WP_WRITEBACK;
    c.write_miss   = WM_ALLOCATE;
}

void config_defaults(Config &cfg) {
    cfg.stats_file = "stats.json";
    cfg.log_file   = "sim.log";

    // A bound on a runaway program. A correct program returns from main and
    // exits through tohost long before this.
    cfg.num_insts = 1000000;

    // _start, the first instruction of custom/crt.S. Starting anywhere else
    // skips the register and stack-pointer set-up that crt.S does.
    cfg.start = 0x80000000u;

    cfg.log_level = LOG_INFO;
    cfg.proc      = PROC_SINGLE;

    // Timing contract. Both decisions were frozen in Phase E; the handout is
    // written against these values. config.ini carries the full argument for
    // each, and configs/ keeps every alternative executed -- freezing a value
    // is not the same as retiring the ones it was chosen over.
    //
    // A branch resolved in MEM leaves three wrong-path instructions in the
    // pipeline; resolved in ID, one. 
    cfg.pipelined_resolve  = RESOLVE_MEM;
    cfg.forwarding_resolve = RESOLVE_ID;
    // [I1_Cache] and [L1_Cache] are separate arrays, and serialising a fetch 
    // against a data access is the cost that splitting the L1 exists to 
    // remove. See config.ini for the full argument and for what max 
    // knowingly ignores (contention at the shared L2 on a coincident
    // miss).
    cfg.latency_overlap = OVERLAP_MAX;

    // Built-in memory defaults describe an *ideal* memory: no caches, and RAM
    // answering within the cycle. A run with no --config therefore produces
    // cycle counts that depend only on the pipeline. The shipped config.ini 
    // turns the hierarchy on for later weeks.
    cfg.ram_base    = 0x80000000u;
    cfg.ram_size    = 0x00100000u;  // 1 MiB: .text, .data and crt.S's 128 KiB stack
    cfg.ram_latency = 0;

    // custom/test.ld places the .tohost section one page above .text.init.
    cfg.tohost = 0x80001000u;

    cache_defaults(cfg.i1, "I1", false, 2, 1024, 64, 1, MAP_DIRECT, REPL_FIFO);
    cache_defaults(cfg.l1, "L1", false, 2, 1024, 64, 1, MAP_DIRECT, REPL_FIFO);
    cache_defaults(cfg.l2, "L2", false, 10, 4096, 64, 4, MAP_SET, REPL_LRU);
}

// ---------------------------------------------------------------------------
// Value parsing
// ---------------------------------------------------------------------------

static bool iequal(const std::string &a, const char *b) {
    size_t i = 0;
    for (; i < a.size() && b[i] != '\0'; i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) {
            return false;
        }
    }
    return i == a.size() && b[i] == '\0';
}

// Where a bad value is reported from, so the message can name the key.
static const char *g_where = "";

static long parse_long(const std::string &v) {
    const char *s = v.c_str();
    char *end = NULL;
    long n = strtol(s, &end, 0);  // base 0: 0x... is hex, 0... is octal, else decimal
    if (end == s || *end != '\0') {
        fatal("%s: '%s' is not a number", g_where, s);
    }
    return n;
}

static u32 parse_u32(const std::string &v) {
    const char *s = v.c_str();
    char *end = NULL;
    unsigned long n = strtoul(s, &end, 0);
    if (end == s || *end != '\0') {
        fatal("%s: '%s' is not a number", g_where, s);
    }
    return (u32)n;
}

static int parse_int(const std::string &v) {
    return (int)parse_long(v);
}

static bool parse_bool(const std::string &v) {
    if (iequal(v, "true") || iequal(v, "yes") || iequal(v, "1")) return true;
    if (iequal(v, "false") || iequal(v, "no") || iequal(v, "0")) return false;
    fatal("%s: '%s' is not a boolean (expected true or false)", g_where, v.c_str());
}

static Mapping parse_mapping(const std::string &v) {
    if (iequal(v, "direct")) return MAP_DIRECT;
    if (iequal(v, "set") || iequal(v, "set-associative")) return MAP_SET;
    if (iequal(v, "full") || iequal(v, "fully-associative")) return MAP_FULL;
    fatal("%s: '%s' is not a mapping (expected direct, set or full)", g_where, v.c_str());
}

static Replacement parse_replacement(const std::string &v) {
    if (iequal(v, "FIFO")) return REPL_FIFO;
    if (iequal(v, "LRU")) return REPL_LRU;
    fatal("%s: '%s' is not a replacement policy (expected FIFO or LRU)", g_where, v.c_str());
}

static WritePolicy parse_write_policy(const std::string &v) {
    if (iequal(v, "write-back") || iequal(v, "writeback")) return WP_WRITEBACK;
    if (iequal(v, "write-through") || iequal(v, "writethrough")) return WP_WRITETHROUGH;
    fatal("%s: '%s' is not a write policy (expected write-back or write-through)",
          g_where, v.c_str());
}

static WriteMiss parse_write_miss(const std::string &v) {
    if (iequal(v, "allocate") || iequal(v, "write-allocate")) return WM_ALLOCATE;
    if (iequal(v, "no-allocate") || iequal(v, "no-write-allocate")) return WM_NOALLOCATE;
    fatal("%s: '%s' is not a write-miss policy (expected allocate or no-allocate)",
          g_where, v.c_str());
}

static ResolveStage parse_resolve(const std::string &v) {
    if (iequal(v, "ID")) return RESOLVE_ID;
    if (iequal(v, "EX")) return RESOLVE_EX;
    if (iequal(v, "MEM")) return RESOLVE_MEM;
    fatal("%s: '%s' is not a stage (expected ID, EX or MEM)", g_where, v.c_str());
}

static Overlap parse_overlap(const std::string &v) {
    if (iequal(v, "max") || iequal(v, "overlap")) return OVERLAP_MAX;
    if (iequal(v, "sum") || iequal(v, "serial")) return OVERLAP_SUM;
    fatal("%s: '%s' is not an overlap rule (expected max or sum)", g_where, v.c_str());
}

ProcKind proc_kind_from_name(const char *name) {
    std::string v(name);
    if (iequal(v, "single") || iequal(v, "singlecycle") ||
        iequal(v, "SingleCycleProcessor")) {
        return PROC_SINGLE;
    }
    if (iequal(v, "pipelined") || iequal(v, "PipelinedProcessor")) {
        return PROC_PIPELINED;
    }
    if (iequal(v, "forwarding") || iequal(v, "fpipelined") ||
        iequal(v, "FPipelinedProcessor")) {
        return PROC_FORWARDING;
    }
    fatal("'%s' is not a processor (expected single, pipelined or forwarding)", name);
}

ResolveStage config_resolve_stage(const Config &cfg, ProcKind kind) {
    switch (kind) {
        case PROC_PIPELINED:  return cfg.pipelined_resolve;
        case PROC_FORWARDING: return cfg.forwarding_resolve;
        case PROC_SINGLE:     break;
    }
    // The single-cycle model has no pipeline to flush: the whole instruction,
    // target PC included, completes before the next fetch starts.
    return RESOLVE_ID;
}

bool config_has_forwarding(ProcKind kind) {
    return kind == PROC_FORWARDING;
}

// ---------------------------------------------------------------------------
// The file itself
// ---------------------------------------------------------------------------

static std::string trim(const std::string &s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) b++;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) e--;
    return s.substr(b, e - b);
}

// Apply one key/value pair belonging to a cache section. Returns false if the
// key is not one a cache understands, so the caller can report it.
static bool cache_set(CacheConfig &c, const std::string &key, const std::string &val) {
    if (key == "valid")        { c.valid = parse_bool(val); return true; }
    if (key == "latency")      { c.latency = parse_int(val); return true; }
    if (key == "size")         { c.size = parse_int(val); return true; }
    if (key == "block_size")   { c.block_size = parse_int(val); return true; }
    if (key == "assoc")        { c.assoc = parse_int(val); return true; }
    if (key == "mapping")      { c.mapping = parse_mapping(val); return true; }
    if (key == "replacement")  { c.replacement = parse_replacement(val); return true; }
    if (key == "write_policy") { c.write_policy = parse_write_policy(val); return true; }
    if (key == "write_miss")   { c.write_miss = parse_write_miss(val); return true; }
    return false;
}

void config_read(Config &cfg, const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fatal("configuration file '%s' not found", path);
    }

    char        line[512];
    std::string section;
    int         lineno = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        lineno++;
        std::string text(line);

        // Strip comments. Both '#' and ';' start one, anywhere on the line.
        size_t hash = text.find_first_of("#;");
        if (hash != std::string::npos) {
            text = text.substr(0, hash);
        }
        // Drop the newline and surrounding space.
        size_t nl = text.find('\n');
        if (nl != std::string::npos) {
            text = text.substr(0, nl);
        }
        text = trim(text);
        if (text.empty()) {
            continue;
        }

        if (text[0] == '[') {
            if (text[text.size() - 1] != ']') {
                fatal("%s:%d: malformed section header '%s'", path, lineno, text.c_str());
            }
            section = text.substr(1, text.size() - 2);
            continue;
        }

        size_t eq = text.find('=');
        if (eq == std::string::npos) {
            fatal("%s:%d: '%s' is neither a section header nor a key = value pair",
                  path, lineno, text.c_str());
        }
        std::string key = trim(text.substr(0, eq));
        std::string val = trim(text.substr(eq + 1));

        // Give the value parsers something useful to name in an error.
        static char where[256];
        snprintf(where, sizeof(where), "%s:%d: [%s] %s", path, lineno,
                 section.c_str(), key.c_str());
        g_where = where;

        bool known = true;
        if (section == "General") {
            if (key == "stats_file")     cfg.stats_file = val;
            else if (key == "log_file")  cfg.log_file = val;
            else if (key == "num_insts") cfg.num_insts = parse_long(val);
            else if (key == "start")     cfg.start = parse_u32(val);
            else known = false;
        } else if (section == "Logging") {
            if (key == "log_level") cfg.log_level = log_level_from_name(val.c_str());
            else known = false;
        } else if (section == "Processor") {
            if (key == "type") cfg.proc = proc_kind_from_name(val.c_str());
            else known = false;
        } else if (section == "Timing") {
            if (key == "pipelined_branch_resolve") {
                cfg.pipelined_resolve = parse_resolve(val);
            } else if (key == "forwarding_branch_resolve") {
                cfg.forwarding_resolve = parse_resolve(val);
            } else if (key == "latency_overlap") {
                cfg.latency_overlap = parse_overlap(val);
            } else {
                known = false;
            }
        } else if (section == "RAM") {
            if (key == "latency")   cfg.ram_latency = parse_int(val);
            else if (key == "base") cfg.ram_base = parse_u32(val);
            else if (key == "size") cfg.ram_size = parse_u32(val);
            else known = false;
        } else if (section == "Termination") {
            if (key == "tohost") cfg.tohost = parse_u32(val);
            else known = false;
        } else if (section == "I1_Cache") {
            known = cache_set(cfg.i1, key, val);
        } else if (section == "L1_Cache") {
            known = cache_set(cfg.l1, key, val);
        } else if (section == "L2_Cache") {
            known = cache_set(cfg.l2, key, val);
        } else {
            fatal("%s:%d: unknown section [%s]", path, lineno, section.c_str());
        }

        if (!known) {
            fatal("%s:%d: unknown key '%s' in section [%s]",
                  path, lineno, key.c_str(), section.c_str());
        }
    }

    fclose(f);
    g_where = "";
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

static void validate_cache(const CacheConfig &c) {
    if (!c.valid) {
        return;
    }
    if (c.size <= 0 || c.block_size <= 0) {
        fatal("%s cache: size and block_size must be positive", c.name);
    }
    if (!is_power_of_two((u32)c.block_size)) {
        fatal("%s cache: block_size %d is not a power of two", c.name, c.block_size);
    }
    if (c.block_size < 4) {
        fatal("%s cache: block_size %d is smaller than a word", c.name, c.block_size);
    }
    if (c.size % c.block_size != 0) {
        fatal("%s cache: size %d is not a multiple of block_size %d",
              c.name, c.size, c.block_size);
    }
    int n_blocks = c.size / c.block_size;
    if (!is_power_of_two((u32)n_blocks)) {
        fatal("%s cache: %d blocks is not a power of two", c.name, n_blocks);
    }
    // Associativity comes from the mapping for direct and full; only a
    // set-associative cache reads `assoc`, and then it must divide the cache.
    if (c.mapping == MAP_SET) {
        if (c.assoc <= 0 || !is_power_of_two((u32)c.assoc)) {
            fatal("%s cache: assoc %d is not a power of two", c.name, c.assoc);
        }
        if (n_blocks % c.assoc != 0) {
            fatal("%s cache: assoc %d does not divide the %d blocks of the cache",
                  c.name, c.assoc, n_blocks);
        }
    }
    if (c.latency < 0) {
        fatal("%s cache: latency %d is negative", c.name, c.latency);
    }
}

void config_validate(const Config &cfg) {
    validate_cache(cfg.i1);
    validate_cache(cfg.l1);
    validate_cache(cfg.l2);

    // The whole hierarchy shares one block size. Chaining levels with
    // different block sizes would mean one fill turning into several, which is
    // more machinery than this course needs -- so it is rejected, not guessed at.
    const CacheConfig *first = NULL;
    const CacheConfig *all[3] = {&cfg.i1, &cfg.l1, &cfg.l2};
    for (int i = 0; i < 3; i++) {
        if (!all[i]->valid) {
            continue;
        }
        if (first == NULL) {
            first = all[i];
        } else if (all[i]->block_size != first->block_size) {
            fatal("%s cache block_size %d differs from %s cache block_size %d: "
                  "every cache in the hierarchy must use the same block size",
                  all[i]->name, all[i]->block_size, first->name, first->block_size);
        }
    }

    if (cfg.ram_latency < 0) {
        fatal("RAM latency %d is negative", cfg.ram_latency);
    }
    if (cfg.ram_size == 0) {
        fatal("RAM size is zero");
    }
    if (cfg.start < cfg.ram_base || cfg.start >= cfg.ram_base + cfg.ram_size) {
        fatal("start address 0x%08x is outside RAM [0x%08x, 0x%08x)",
              cfg.start, cfg.ram_base, cfg.ram_base + cfg.ram_size);
    }
    if (cfg.tohost < cfg.ram_base || cfg.tohost >= cfg.ram_base + cfg.ram_size) {
        fatal("tohost address 0x%08x is outside RAM [0x%08x, 0x%08x)",
              cfg.tohost, cfg.ram_base, cfg.ram_base + cfg.ram_size);
    }
    if (cfg.num_insts <= 0) {
        fatal("num_insts %ld must be positive", cfg.num_insts);
    }
}

// ---------------------------------------------------------------------------
// Reporting
// ---------------------------------------------------------------------------

static void log_cache(const CacheConfig &c) {
    if (!c.valid) {
        log_debug("  %s cache: absent", c.name);
        return;
    }
    log_debug("  %s cache: %d B, %d B blocks, %s, %s, %s, %s, latency %d",
              c.name, c.size, c.block_size, mapping_name(c.mapping),
              replacement_name(c.replacement), write_policy_name(c.write_policy),
              write_miss_name(c.write_miss), c.latency);
}

void config_log(const Config &cfg) {
    log_debug("configuration in force:");
    log_debug("  processor: %s", proc_kind_name(cfg.proc));
    log_debug("  start: 0x%08x, instruction budget: %ld", cfg.start, cfg.num_insts);
    log_debug("  branch resolved in: pipelined %s, forwarding %s",
              resolve_stage_name(cfg.pipelined_resolve),
              resolve_stage_name(cfg.forwarding_resolve));
    log_debug("  memory latency overlap: %s",
              cfg.latency_overlap == OVERLAP_MAX ? "max" : "sum");
    log_debug("  RAM: [0x%08x, 0x%08x), latency %d",
              cfg.ram_base, cfg.ram_base + cfg.ram_size, cfg.ram_latency);
    log_debug("  tohost: 0x%08x", cfg.tohost);
    log_cache(cfg.i1);
    log_cache(cfg.l1);
    log_cache(cfg.l2);
}

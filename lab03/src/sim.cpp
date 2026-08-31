// Entry point: read the options, build the machine, run the program, report.
//
//   build/sim [options] program.r5ob
//
//     --config=FILE      configuration file; without one, built-in defaults
//                        describing an ideal memory (no caches, no delay)
//     --proc=KIND        single | pipelined | forwarding
//     --start=ADDR       initial PC (default 0x80000000, _start in crt.S)
//     --num_insts=N      stop after N instructions, whatever the program does
//     --stats=FILE       where to write the statistics (default stats.json)
//     --log=FILE         where to write the log (default sim.log)
//     --dump=LO:HI       after the run, print RAM [LO, HI) as words
//     --disasm=LO:HI     before the run, disassemble RAM [LO, HI)
//     --norun            load and inspect only; do not execute anything
//
// Options given on the command line takes precedence over the configuration 
// file, which takes precedence over the built-in defaults.

#include <cstring>
#include <string>

#include "config.h"
#include "loader.h"
#include "log.h"
#include "ram.h"
#include "stats.h"

// Everything the machine is made of is tied together in a way that one function
// can wire it up and the pointers stay valid for the whole run.
//
//     processor --fetch--> I1 --+
//                               +--> L2 --> RAM
//     processor --data---> L1 --+
//
// Any of the three caches can be absent, and what a level talks to is then
// simply whatever is below it. With none of them present both sides address the
// RAM directly, which is the ideal-memory model the built-in defaults to.
struct Machine {
    Ram   ram;

    MemoryLevel *imem;  // what instruction fetch reads from
    MemoryLevel *dmem;  // what loads and stores go to
};

static void build_machine(Machine &m, const Config &cfg, Stats &st) {
    ram_init(m.ram, cfg.ram_base, cfg.ram_size, cfg.ram_latency);

    // Caches will be built in a future lab. Until then both sides of the
    // machine read straight from the RAM.
    if (cfg.i1.valid || cfg.l1.valid || cfg.l2.valid) {
        fatal("this configuration asks for a cache, and this simulator has "
              "none.");
    }
    m.imem = &m.ram;
    m.dmem = &m.ram;
    stats_add_level(st, &m.ram.stats);
}

// Push dirty blocks down so that the RAM holds the program's final state.
// Done after the statistics have been written, so that this tidying-up traffic
// is not mistaken for something the program did.
static void flush_machine(Machine &m, const Config &cfg) {
    // Nothing to flush: there is no cache to hold a dirty block yet.
    (void)m;
    (void)cfg;
}

// The address the linker script loads the program image at. Not configurable.
static const u32 LOAD_ADDR = 0x80000000u;

struct Options {
    const char *image;
    const char *config_path;
    const char *proc;
    const char *start;
    const char *num_insts;
    const char *stats_path;
    const char *log_path;
    const char *dump;
    const char *disasm;
    bool        norun;
};

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s [options] program.r5ob\n"
            "\n"
            "  --config=FILE     configuration file\n"
            "  --proc=KIND       single | pipelined | forwarding\n"
            "  --start=ADDR      initial PC, e.g. 0x80000000\n"
            "  --num_insts=N     instruction budget\n"
            "  --stats=FILE      statistics output (default stats.json)\n"
            "  --log=FILE        log output (default sim.log)\n"
            "  --dump=LO:HI      dump RAM [LO, HI) as words after the run\n"
            "  --disasm=LO:HI    disassemble RAM [LO, HI) before the run\n"
            "  --norun           load and inspect only; execute nothing\n",
            argv0);
    exit(2);
}

// Match "--name=value" and hand back a pointer to the value.
static bool option(const char *arg, const char *name, const char **value) {
    size_t n = strlen(name);
    if (strncmp(arg, name, n) == 0 && arg[n] == '=') {
        *value = arg + n + 1;
        return true;
    }
    return false;
}

static Options parse_args(int argc, char **argv) {
    Options opt;
    memset(&opt, 0, sizeof(opt));

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (a[0] != '-') {
            if (opt.image != NULL) {
                fprintf(stderr, "more than one program image given: '%s' and '%s'\n",
                        opt.image, a);
                usage(argv[0]);
            }
            opt.image = a;
        } else if (option(a, "--config", &opt.config_path) ||
                   option(a, "--proc", &opt.proc) ||
                   option(a, "--start", &opt.start) ||
                   option(a, "--num_insts", &opt.num_insts) ||
                   option(a, "--stats", &opt.stats_path) ||
                   option(a, "--log", &opt.log_path) ||
                   option(a, "--dump", &opt.dump) ||
                   option(a, "--disasm", &opt.disasm)) {
            // handled
        } else if (strcmp(a, "--norun") == 0) {
            opt.norun = true;
        } else {
            fprintf(stderr, "unknown option '%s'\n", a);
            usage(argv[0]);
        }
    }

    if (opt.image == NULL) {
        fprintf(stderr, "no program image given\n");
        usage(argv[0]);
    }
    return opt;
}

// Parse "LO:HI", both in any base strtoul understands.
static void parse_range(const char *text, const char *what, u32 *lo, u32 *hi) {
    const char *colon = strchr(text, ':');
    if (colon == NULL) {
        fatal("%s: '%s' is not a range (expected LOW:HIGH)", what, text);
    }
    char *end = NULL;
    *lo = (u32)strtoul(text, &end, 0);
    if (end != colon) {
        fatal("%s: '%s' has a malformed low address", what, text);
    }
    *hi = (u32)strtoul(colon + 1, &end, 0);
    if (*end != '\0') {
        fatal("%s: '%s' has a malformed high address", what, text);
    }
    if (*hi <= *lo) {
        fatal("%s: '%s' is empty (high address is not above the low one)", what, text);
    }
}

int main(int argc, char **argv) {
    Options opt = parse_args(argc, argv);

    Config cfg;
    config_defaults(cfg);
    if (opt.config_path != NULL) {
        config_read(cfg, opt.config_path);
    }

    // Command-line overrides, applied after the file so they win.
    if (opt.log_path != NULL)   cfg.log_file = opt.log_path;
    if (opt.stats_path != NULL) cfg.stats_file = opt.stats_path;

    log_open(cfg.log_file.c_str(), (LogLevel)cfg.log_level);

    if (opt.proc != NULL)      cfg.proc = proc_kind_from_name(opt.proc);
    if (opt.start != NULL)     cfg.start = (u32)strtoul(opt.start, NULL, 0);
    if (opt.num_insts != NULL) cfg.num_insts = strtol(opt.num_insts, NULL, 0);

    config_validate(cfg);

    if (opt.config_path == NULL) {
        log_info("no --config given: using built-in defaults "
                 "(ideal memory, no caches)");
    }
    config_log(cfg);

    Stats st;
    stats_init(st);
    st.processor = proc_kind_name(cfg.proc);
    // Timing models that are irrelevant for a single-cycle processor.
    if (cfg.proc == PROC_SINGLE) {
        st.resolve_stage = "n/a";
        st.overlap       = "n/a";
    } else {
        st.resolve_stage = resolve_stage_name(config_resolve_stage(cfg, cfg.proc));
        st.overlap       = (cfg.latency_overlap == OVERLAP_MAX) ? "max" : "sum";
    }

    Machine m;
    build_machine(m, cfg, st);

    // The image goes straight into the RAM and the caches start empty (as
    // in real hardware).
    loader_load(m.ram, opt.image, LOAD_ADDR);

    if (opt.disasm != NULL) {
        fatal("--disasm needs a decoder and a disassembler."
              "This simulator can load a program and show you "
              "the bytes (--dump).");
    }

    // There is no processor yet. Everything up to here still happens: the 
    // configuration is read, the machine is built and the program image is 
    // loaded into RAM, which is what this week is about. --dump will 
    // show you it landed.
    if (!opt.norun) {
        log_info("nothing to run yet. The program has been loaded.");
    }

    stats_write(st, cfg.stats_file.c_str());

    if (opt.dump != NULL) {
        flush_machine(m, cfg);
        u32 lo, hi;
        parse_range(opt.dump, "--dump", &lo, &hi);
        ram_dump(m.ram, lo, hi, stdout);
    }

    stats_print_summary(st);
    log_close();

    // A non-zero exit status of the *simulated* program is not a failure of the
    // simulator, so it is reported rather than passed on. The harness reads the
    // status out of stats.json.
    return 0;
}

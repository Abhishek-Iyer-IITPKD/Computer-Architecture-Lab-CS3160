#include "stats.h"

#include <cstring>

#include "log.h"

void level_stats_init(LevelStats &s, const char *name, bool is_cache) {
    memset(&s, 0, sizeof(s));
    s.name = name;
    s.is_cache = is_cache;
}

void stats_init(Stats &st) {
    memset(&st, 0, sizeof(st));
    st.processor     = "?";
    st.resolve_stage = "?";
    st.overlap       = "?";
}

void stats_add_level(Stats &st, LevelStats *level) {
    if (st.n_levels >= (int)(sizeof(st.levels) / sizeof(st.levels[0]))) {
        fatal("too many memory levels registered with the statistics object");
    }
    st.levels[st.n_levels++] = level;
}

double stats_cpi(const Stats &st) {
    if (st.instructions == 0) {
        return 0.0;
    }
    return (double)st.cycles / (double)st.instructions;
}

static double rate(long part, long total) {
    if (total == 0) {
        return 0.0;
    }
    return (double)part / (double)total;
}

void stats_write(const Stats &st, const char *path) {
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        fatal("cannot open statistics file '%s' for writing", path);
    }

    fprintf(f, "{\n");
    fprintf(f, "    \"processor\": \"%s\",\n", st.processor);
    fprintf(f, "    \"branch_resolved_in\": \"%s\",\n", st.resolve_stage);
    fprintf(f, "    \"latency_overlap\": \"%s\",\n", st.overlap);
    fprintf(f, "    \"instructions\": %ld,\n", st.instructions);
    fprintf(f, "    \"cycles\": %ld,\n", st.cycles);
    fprintf(f, "    \"cpi\": %.4f,\n", stats_cpi(st));
    fprintf(f, "    \"stall_cycles\": %ld,\n", st.stall_cycles);
    fprintf(f, "    \"memory_stall_cycles\": %ld,\n", st.mem_stall_cycles);
    fprintf(f, "    \"flushed_instructions\": %ld,\n", st.flushed);
    fprintf(f, "    \"instruction_fetches\": %ld,\n", st.fetches);
    fprintf(f, "    \"data_accesses\": %ld,\n", st.data_accesses);
    fprintf(f, "    \"loads\": %ld,\n", st.loads);
    fprintf(f, "    \"stores\": %ld,\n", st.stores);
    fprintf(f, "    \"branches\": %ld,\n", st.branches);
    fprintf(f, "    \"taken_branches\": %ld,\n", st.taken_branches);
    fprintf(f, "    \"jumps\": %ld,\n", st.jumps);
    fprintf(f, "    \"pc_redirects\": %ld,\n", st.pc_redirects);
    fprintf(f, "    \"exited\": %s,\n", st.exited ? "true" : "false");
    if (st.exited) {
        fprintf(f, "    \"exit_status\": %d,\n", st.exit_status);
    } else {
        fprintf(f, "    \"exit_status\": null,\n");
    }

    fprintf(f, "    \"memory\": [\n");
    for (int i = 0; i < st.n_levels; i++) {
        const LevelStats &l = *st.levels[i];
        fprintf(f, "        {\n");
        fprintf(f, "            \"level\": \"%s\",\n", l.name);
        fprintf(f, "            \"accesses\": %ld,\n", level_accesses(l));
        fprintf(f, "            \"reads\": %ld,\n", l.reads);
        fprintf(f, "            \"writes\": %ld", l.writes);
        if (l.is_cache) {
            fprintf(f, ",\n");
            fprintf(f, "            \"hits\": %ld,\n", level_hits(l));
            fprintf(f, "            \"misses\": %ld,\n", level_misses(l));
            fprintf(f, "            \"hit_rate\": %.4f,\n",
                    rate(level_hits(l), level_accesses(l)));
            fprintf(f, "            \"read_hits\": %ld,\n", l.read_hits);
            fprintf(f, "            \"read_misses\": %ld,\n", l.read_misses);
            fprintf(f, "            \"write_hits\": %ld,\n", l.write_hits);
            fprintf(f, "            \"write_misses\": %ld,\n", l.write_misses);
            fprintf(f, "            \"evictions\": %ld,\n", l.evictions);
            fprintf(f, "            \"writebacks\": %ld\n", l.writebacks);
        } else {
            fprintf(f, "\n");
        }
        fprintf(f, "        }%s\n", (i + 1 < st.n_levels) ? "," : "");
    }
    fprintf(f, "    ]\n");
    fprintf(f, "}\n");

    fclose(f);
    log_info("statistics written to %s", path);
}

void stats_print_summary(const Stats &st) {
    printf("\n");
    printf("  processor        %s\n", st.processor);
    printf("  instructions     %ld\n", st.instructions);
    printf("  cycles           %ld\n", st.cycles);
    printf("  CPI              %.4f\n", stats_cpi(st));
    if (st.stall_cycles != 0 || st.flushed != 0) {
        printf("  stall cycles     %ld\n", st.stall_cycles);
        printf("  flushed insts    %ld\n", st.flushed);
    }
    if (st.mem_stall_cycles != 0) {
        printf("  memory cycles    %ld\n", st.mem_stall_cycles);
    }
    for (int i = 0; i < st.n_levels; i++) {
        const LevelStats &l = *st.levels[i];
        if (l.is_cache) {
            printf("  %-4s accesses %8ld   hits %8ld   misses %8ld   hit rate %6.2f%%\n",
                   l.name, level_accesses(l), level_hits(l), level_misses(l),
                   100.0 * rate(level_hits(l), level_accesses(l)));
        } else {
            printf("  %-4s accesses %8ld\n", l.name, level_accesses(l));
        }
    }
    if (st.exited) {
        printf("  exit status      %d\n", st.exit_status);
    } else {
        printf("  exit status      program did not reach _exit\n");
    }
    printf("\n");
}

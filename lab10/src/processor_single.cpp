// The single-cycle model: each iteration executes one instruction completely.
//
// Cycles: one per instruction, plus memory stall time. The fetch and the data
// access occur in sequence within the same cycle, so their latencies add. A
// latency of n means the access occupies n cycles, including the cycle the
// instruction takes in any case.

#include "processor.h"

#include "log.h"

void run_single(Processor &cpu) {
    const Config &cfg = *cpu.cfg;
    Stats        &st  = *cpu.stats;

    while (st.instructions < cfg.num_insts) {
        Fetched f = stage_fetch(cpu, cpu.pc);

        Decoded d = decode(f.raw, f.pc);
        if (d.op == OP_INVALID) {
            processor_illegal(cpu, d);
        }

        u32 v_rs1, v_rs2;
        stage_operands(cpu, d, &v_rs1, &v_rs2);

        Executed e = stage_execute(d, v_rs1, v_rs2);
        u32 next   = next_pc_of(d, e);
        if (redirects_pc(d, e)) {
            // Nothing to flush, but counting redirects keeps the statistic
            // comparable across models.
            st.pc_redirects++;
        }

        MemAccess m = stage_memory(cpu, d, e.result, v_rs2);

        stage_writeback(cpu, d, e.result, m, next);

        int cost = cycle_cost_serial(f.latency, m.latency);
        st.cycles += cost;
        st.mem_stall_cycles += cost - 1;
        cpu.pc = next;

        if (cpu.halted) {
            break;
        }
    }
    processor_dump(cpu);
}

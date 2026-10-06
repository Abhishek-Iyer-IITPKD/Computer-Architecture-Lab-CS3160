// The single-cycle model: one instruction, start to finish, every iteration.
//
// Cycles: one per instruction, plus whatever the memory made it wait for. Both
// the fetch and the data access happen inside the same cycle, one after the
// other, so their latencies add up. A latency of n means the access occupies 
// n cycles including the one the instruction would have taken anyway. 

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

        u32       v_rs1, v_rs2;
        CsrAccess c;
        stage_operands(cpu, d, &v_rs1, &v_rs2, &c);

        Executed e = stage_execute(d, v_rs1, v_rs2);
        u32 next   = next_pc_of(d, e);
        if (redirects_pc(d, e)) {
            // No pipeline to flush, but the question still has an answer, and
            // counting it here keeps the statistic comparable across models.
            st.pc_redirects++;
        }

        MemAccess m = stage_memory(cpu, d, e.result, v_rs2);

        stage_writeback(cpu, d, e.result, m, c, next);

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

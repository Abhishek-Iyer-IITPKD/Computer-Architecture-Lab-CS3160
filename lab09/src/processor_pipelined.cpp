// The five-stage pipeline, with no forwarding: every hazard is a stall.
//
// Each iteration of the loop is one clock cycle, in which all five stages run.
// Each stage reads the latch in front of it (its state at the start of the
// cycle) and writes a *next* latch; at the end of the cycle the four next
// latches replace the current ones. This models pipeline registers, and makes
// the result independent of the order in which the stages appear.
//
// One exception is deliberate: WB writes the register file in the first half
// of the cycle and ID reads it in the second, so an instruction in ID sees the
// value being written by the instruction in WB. A back-to-back dependency
// therefore costs two stall cycles rather than three, as in the textbook.
//
// This model differs from the forwarding model in two respects: it has no
// forwarding paths (hazard_in_id() is told so), and it resolves branches in
// the stage the configuration names.

#include "hazard.h"
#include "log.h"
#include "processor.h"

void run_pipelined(Processor &cpu) {
    const Config &cfg     = *cpu.cfg;
    Stats        &st      = *cpu.stats;
    const ResolveStage resolve = config_resolve_stage(cfg, PROC_PIPELINED);

    Latch if_id  = latch_bubble();
    Latch id_ex  = latch_bubble();
    Latch ex_mem = latch_bubble();
    Latch mem_wb = latch_bubble();

    bool done = false;  // the store to tohost has retired; the program is over

    // A pipeline that retires nothing is making no progress, the usual state of
    // an incomplete implementation. Without this check the loop would run to
    // the instruction budget, indistinguishable from a hang. The limit far
    // exceeds the longest legitimate gap between retirements: four cycles to
    // fill the pipeline, plus a stall, plus a flush.
    const long IDLE_LIMIT = 256;
    long idle = 0, retired_before = -1;

    while (!done && st.instructions < cfg.num_insts) {
        if (st.instructions == retired_before) {
            if (++idle >= IDLE_LIMIT) {
                log_error("the pipeline has run %ld cycles without retiring an "
                          "instruction, so it is not moving. Look at what fills "
                          "IF/ID and what copies each next_* latch over the one "
                          "in front of it.", IDLE_LIMIT);
                break;
            }
        } else {
            idle           = 0;
            retired_before = st.instructions;
        }

        int fetch_latency = 0;
        int mem_latency   = 0;

        Latch next_if_id  = latch_bubble();
        Latch next_id_ex  = latch_bubble();
        Latch next_ex_mem = latch_bubble();
        Latch next_mem_wb = latch_bubble();

        pipeline_dump(st.cycles, if_id, id_ex, ex_mem, mem_wb);

        // --- WB: write the result back and retire ---
        if (mem_wb.valid) {
            // An instruction is illegal only if it reaches the end of the
            // pipeline. An invalid word fetched on a wrong path is flushed
            // before reaching WB and must not be reported.
            if (mem_wb.d.op == OP_INVALID) {
                processor_illegal(cpu, mem_wb.d);
            }
            stage_writeback(cpu, mem_wb.d, mem_wb.e.result, mem_wb.m, mem_wb.next_pc);
            if (mem_wb.m.did_write && mem_wb.m.addr == cfg.tohost) {
                done = true;
            }
        }

        // --- MEM: load or store ---
        if (ex_mem.valid) {
            Latch l     = ex_mem;
            l.m         = stage_memory(cpu, l.d, l.e.result, l.v_rs2);
            mem_latency = l.m.latency;
            next_mem_wb = l;
        }

        // --- EX: the ALU, and the target of a branch or jump ---
        if (id_ex.valid) {
            Latch l     = id_ex;
            l.e         = stage_execute(l.d, l.v_rs1, l.v_rs2);
            l.next_pc   = next_pc_of(l.d, l.e);
            next_ex_mem = l;
        }

        // --- ID: decode, check for hazards, read the registers ---
        Hazard hz;
        hz.stall   = false;
        hz.fwd_rs1 = FWD_NONE;
        hz.fwd_rs2 = FWD_NONE;

        if (if_id.valid) {
            Decoded d = decode(if_id.d.raw, if_id.d.pc);
            hz = hazard_in_id(d, true, id_ex, ex_mem, mem_wb,
                              /*forwarding=*/false,
                              /*resolves_in_id=*/resolve == RESOLVE_ID);
            if (!hz.stall) {
                u32 v_rs1, v_rs2;
                stage_operands(cpu, d, &v_rs1, &v_rs2);
                Latch l = latch_from_id(d, v_rs1, v_rs2);

                if (resolve == RESOLVE_ID && (is_branch(d.op) || is_jump(d.op))) {
                    // When resolving in ID, the redirect below reads the
                    // outcome from this latch, so it must be computed here.
                    // No forwarding is needed: the stall above holds the
                    // instruction until every register it reads has been
                    // written back. EX recomputes the same result a cycle later.
                    l.e       = stage_execute(l.d, l.v_rs1, l.v_rs2);
                    l.next_pc = next_pc_of(l.d, l.e);
                }
                next_id_ex = l;
            } else {
                log_debug("ID  0x%08x: stalled on a data hazard", d.pc);
            }
        }

        // --- IF: fetch, unless the pipeline is held or the program is over ---
        if (hz.stall) {
            // The stalled instruction remains in ID: IF/ID keeps it and the PC
            // does not advance. No fetch occurs, so a stall costs no memory
            // access.
            next_if_id = if_id;
            st.stall_cycles++;
        } else if (cpu.halted) {
            // The program has stored to tohost: stop fetching and let the
            // pipeline drain.
        } else {
            Fetched f     = stage_fetch(cpu, cpu.pc);
            fetch_latency = f.latency;
            next_if_id    = latch_from_fetch(f);
            cpu.pc        = cpu.pc + 4;
        }

        // --- Control flow: redirect the PC, and throw away the wrong path ---
        //
        // The deciding instruction is the one leaving the resolving stage.
        // Everything fetched after it, one instruction per stage from fetch to
        // that stage, is on the wrong path: exactly the younger latches.
        const Latch *resolved = (resolve == RESOLVE_MEM)  ? &next_mem_wb
                                : (resolve == RESOLVE_EX) ? &next_ex_mem
                                                          : &next_id_ex;
        if (redirects_pc(*resolved)) {
            cpu.pc = resolved->next_pc;
            st.pc_redirects++;

            Latch *younger[3] = {&next_if_id, &next_id_ex, &next_ex_mem};
            int    depth      = wrong_path_instructions(resolve);
            for (int i = 0; i < depth; i++) {
                if (younger[i]->valid) {
                    st.flushed++;
                }
                *younger[i] = latch_bubble();
            }
            log_debug("0x%08x redirects to 0x%08x: %d wrong-path instruction(s) "
                      "discarded", resolved->d.pc, cpu.pc, depth);
        }

        if_id  = next_if_id;
        id_ex  = next_id_ex;
        ex_mem = next_ex_mem;
        mem_wb = next_mem_wb;

        // A memory access longer than a cycle stalls the whole pipeline, so the
        // extra time is charged to the cycle rather than to a stage.
        int cost = cycle_cost(cpu, fetch_latency, mem_latency);
        st.cycles += cost;
        st.mem_stall_cycles += cost - 1;
    }
    processor_dump(cpu);
}

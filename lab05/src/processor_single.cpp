// The single-cycle model: one instruction, start to finish, every iteration.
//
// Cycles: one per instruction, plus whatever the memory made it wait for. Both
// the fetch and the data access happen inside the same cycle, one after the
// other, so their latencies add up. A latency of n means the access occupies 
// n cycles including the one the instruction would have taken anyway. 

#include "processor.h"

#include "log.h"

void run_single(Processor &cpu) {
    // TODO(week 5): the run loop.
    //
    // One instruction per iteration, start to finish, until the program
    // stops. The stages are all declared in processor.h and you are writing
    // most of them this week; this loop is what puts them in order:
    //
    //   fetch -> decode -> operands -> execute -> next_pc -> memory ->
    //   writeback
    //
    // Then move the PC to where next_pc_of() said, and stop when cpu.halted
    // goes true. Guard the whole loop with cfg.num_insts so that a program
    // that never terminates stops being your problem after a million
    // instructions.
    //
    // Three important details. An instruction that does not decode is a 
    // bug in the program, not in your loop -- call processor_illegal() on
    // such cases. The cycle count: this machine takes one cycle per 
    // instruction plus whatever the memory made it wait for,
    // which is cycle_cost_serial(fetch latency, data latency);
    // everything over the first cycle is memory stall.
    // One counter belongs here rather than in a stage: redirects_pc()
    // (given to you, in processor.cpp) says whether this instruction sent
    // the PC somewhere other than straight-line, and cpu.stats->pc_redirects
    // counts the times it did. It is not the same number as
    // stage_writeback()'s taken_branches; the handout says why.
    //
    // Finish with processor_dump(), which prints the final register state at
    // DEBUG level.
    const Config &cfg = *cpu.cfg;
    for(long long i=0;i<cfg.num_insts;i++){
        Fetched f = stage_fetch(cpu, cpu.pc);
        Decoded d = decode(f.raw, f.pc);
        u32 v_rs1, v_rs2;
        stage_operands(cpu, d, &v_rs1, &v_rs2);
        Executed e = stage_execute(d, v_rs1, v_rs2);
        u32 next_pc = next_pc_of(d, e);
        MemAccess m = stage_memory(cpu, d, e.result, v_rs2);
        stage_writeback(cpu, d, e.result, m, next_pc);
        cpu.pc = next_pc;
        if(redirects_pc(d,e)) cpu.stats->pc_redirects += 1;
        cpu.stats->cycles += 1 + (cycle_cost_serial(f.latency, m.latency));
        if(cpu.halted) break;
    }
    cpu.stats->cycles /= 2;
    processor_dump(cpu);
}

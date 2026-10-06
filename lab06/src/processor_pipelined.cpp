// The five-stage pipeline, with no forwarding: every hazard is a stall.
//
// One iteration of the loop is one clock cycle, and in that cycle all five
// stages run. Each stage reads the latch in front of it -- the state as it was
// at the start of the cycle -- and writes a *next* latch; the four next latches
// are copied over the current ones at the end. That is what a pipeline register
// is, and doing it this way means the order the stages appear in the code cannot
// affect the result.
//
// With one deliberate exception, which is a real design decision and not an
// accident of ordering: the register file is written by WB in the first half of
// the cycle and read by ID in the second. So an instruction in ID sees what the
// instruction in WB is writing, and a dependency on it costs nothing. This is
// what makes a back-to-back dependency cost two stall cycles here rather than
// three, and it is the convention the textbook uses.
//
// The three ways this model differs from the forwarding one are all in one place
// each: no forwarding paths (hazard_in_id is told so), branches resolved in the
// stage the configuration names, and nothing else.

#include "config.h"
#include "decode.h"
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

    // A pipeline that retires nothing is not making progress, and while it is
    // being built that is the normal state of it. Without this the loop would
    // spin to the instruction budget -- a million cycles of nothing, which from
    // the outside is indistinguishable from a hang. The limit is far above any
    // real gap between retirements: the deepest is the four cycles it takes to
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

        // TODO(lab 6): the writeback stage.
        //
        // If MEM/WB holds an instruction, retire it: stage_writeback() (yours,
        // from lab 5) does the work, and the latch carries every argument it
        // needs. Set `done` when the instruction that just retired was the store
        // to cfg.tohost -- that is how the loop learns the program is over.
        //
        // One check belongs here. An instruction that does not
        // decode is only really illegal if it reaches the end of the pipeline:
        // rubbish fetched on the wrong path is squashed before it gets here, and
        // reporting it would fail programs that are perfectly correct.
        if(mem_wb.valid){
            stage_writeback(cpu, mem_wb.d, mem_wb.e.result, mem_wb.m, mem_wb.next_pc);
            if(cpu.halted) done = true;
        }


        // TODO(lab 6): the memory stage.
        //
        // Copy EX/MEM into a local, fill in its `m` with stage_memory(), and make
        // that the next MEM/WB. Record the access's latency in mem_latency: the
        // cycle cost at the bottom of the loop needs it.
        //
        // Notice the shape, because all four stages share it: read the latch in
        // front of you, add what this stage computes, write the *next* latch.
        // Never write the current one -- another stage is still reading it.
        next_mem_wb = ex_mem;
        next_mem_wb.m = stage_memory(cpu, next_mem_wb.d, next_mem_wb.e.result, next_mem_wb.v_rs2);
        mem_latency = next_mem_wb.m.latency;



        // TODO(lab 6): the execute stage.
        //
        // Same shape again: take ID/EX, fill in `e` with stage_execute() and
        // `next_pc` with next_pc_of(), and make it the next EX/MEM. Both are
        // yours from lab 5.
        next_ex_mem = id_ex;
        next_ex_mem.e = stage_execute(next_ex_mem.d, next_ex_mem.v_rs1, next_ex_mem.v_rs2);
        next_ex_mem.next_pc = next_pc_of(next_ex_mem.d, next_ex_mem.e);
        


        // --- ID: decode, check for hazards, read the registers ---
        Hazard hz;
        hz.stall   = false;
        hz.fwd_rs1 = FWD_NONE;
        hz.fwd_rs2 = FWD_NONE;

        // TODO(lab 6): the decode stage.
        //
        // If IF/ID holds anything, decode it -- IF/ID carries bits, not a
        // Decoded, because decoding them is this stage's job -- then ask
        // hazard_in_id() whether it may proceed. Hazard detection is given to
        // you this week and always answers "no hazard"; you write it in lab 7,
        // and this call is where it plugs in. Pass forwarding=false and
        // resolves_in_id = (resolve == RESOLVE_ID).
        //
        // When it may proceed: read its operands with stage_operands() and build
        // the next ID/EX with latch_from_id().
        //
        // When it may not, do nothing at all. Leaving next_id_ex as the bubble it
        // starts as is what puts a hole in the pipeline, and the IF stage below
        // is what keeps the stalled instruction where it is.
        //
        // One more case, and only if resolve == RESOLVE_ID: a model that decides
        // branches here has to compute the outcome into the latch before the
        // redirect below can read it. The default configuration resolves in MEM,
        // so you can leave this until the rest works.
        if(if_id.valid){
            Decoded d_id = decode(if_id.d.raw, if_id.d.pc);
            hz = hazard_in_id(d_id, if_id.valid, id_ex, ex_mem, mem_wb, false, (resolve == RESOLVE_ID));
            if(!hz.stall){
                u32 v_rs1, v_rs2;
                stage_operands(cpu, d_id, &v_rs1, &v_rs2);
                next_id_ex = latch_from_id(d_id, v_rs1, v_rs2);
            }
        }



        // TODO(lab 6): the fetch stage, and what a stall does to it.
        //
        // Three cases, in this order:
        //
        //   the pipeline is stalled  IF/ID must keep what it is holding and the
        //                            PC must not move. No fetch happens -- which
        //                            is why a stall costs no memory access -- and
        //                            the cycle is counted in st.stall_cycles.
        //   the program has halted   stop feeding the pipeline and let what is in
        //                            it drain. Do nothing; the bubbles do it.
        //   otherwise                fetch at cpu.pc, put it in the next IF/ID
        //                            with latch_from_fetch(), record the latency
        //                            in fetch_latency, and advance cpu.pc by 4.
        //
        // Fetching pc + 4 unconditionally is right even for a branch: whether it
        // was right is not known for another three stages, and the flush below is
        // how the machine changes its mind.
        if(cpu.halted);



        // TODO(lab 6): control hazards.
        //
        // The instruction that decides where control goes is the one leaving the
        // stage this model resolves in -- `resolved` below points at the right
        // next_* latch for each setting, and is given to you. If that latch
        // redirects the PC (redirects_pc(), yours to write in hazard.cpp):
        //
        //   * send cpu.pc to where it says, and count it in st.pc_redirects
        //   * throw away everything fetched behind it. Those are the younger
        //     latches -- next_if_id, next_id_ex, next_ex_mem, in that order --
        //     and wrong_path_instructions() says how many of them to empty.
        //     Emptying one means replacing it with latch_bubble().
        //
        // Count only the latches that actually held an instruction. Squashing a
        // bubble costs nothing and must not appear in st.flushed: a stalled cycle
        // sends a bubble down the pipe, and it may be sitting in one of these.

        const Latch *resolved = (resolve == RESOLVE_MEM)  ? &next_mem_wb
                                : (resolve == RESOLVE_EX) ? &next_ex_mem
                                                          : &next_id_ex;
        (void)resolved;



        // TODO(lab 6): end the cycle.
        //
        // Copy each next_* latch over the one in front of it. This is the moment
        // the pipeline registers clock, and doing it here -- rather than as each
        // stage finishes -- is what makes the order the stages appear in above
        // unable to affect the result.
        //
        // Then charge the cycle: cycle_cost(cpu, fetch_latency, mem_latency) is
        // what it cost, everything over the first cycle is memory stall, and both
        // go in st. A slow memory freezes the whole pipeline, which is why the
        // cost is charged to the cycle and not to any one stage.
        (void)next_if_id;
        (void)next_id_ex;
        (void)next_ex_mem;
        (void)next_mem_wb;

    }
    processor_dump(cpu);
}

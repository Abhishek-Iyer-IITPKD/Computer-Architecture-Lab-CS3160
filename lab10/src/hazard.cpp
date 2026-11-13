#include "hazard.h"

#include "log.h"

// Whether the instruction in `l` writes register `n`.
//
// Two conditions that every comparison needs are included. An empty latch
// writes nothing, so a bubble is never a producer. Register 0 never counts: it
// is hardwired to zero, and treating it as a dependency would stall on every
// nop (`addi zero, zero, 0`).
static bool writes(const Latch &l, int n) {
    return l.valid && n != 0 && writes_rd(l.d.op) && l.d.rd == n;
}

// The registers the instruction reads, or 0. An instruction whose format has no
// rs2 still has bits in that position, belonging to the immediate; consulting
// the format rather than the encoding prevents false dependencies, such as a
// jal depending on a register number formed by its immediate bits.
static void sources(const Decoded &d, int *rs1, int *rs2) {
    *rs1 = reads_rs1(d.op) ? d.rs1 : 0;
    *rs2 = reads_rs2(d.op) ? d.rs2 : 0;
}

// --- Without forwarding: the only remedy is to stall -----------------------

Hazard hazard_stalling(const Decoded &id, const Latch &ex, const Latch &mem) {
    Hazard h;
    h.stall   = false;
    h.fwd_rs1 = FWD_NONE;
    h.fwd_rs2 = FWD_NONE;

    int rs1, rs2;
    sources(id, &rs1, &rs2);

    // An instruction in EX or MEM has not yet written the register file, so a
    // dependent instruction must wait.
    if (writes(ex, rs1) || writes(ex, rs2) ||
        writes(mem, rs1) || writes(mem, rs2)) {
        h.stall = true;
    }
    return h;
}

// --- With forwarding: the two dependencies it cannot resolve --------------

Hazard hazard_forwarding(const Decoded &id, const Latch &ex, const Latch &mem,
                         bool resolves_in_id) {
    Hazard h;
    h.stall   = false;
    h.fwd_rs1 = FWD_NONE;
    h.fwd_rs2 = FWD_NONE;

    int rs1, rs2;
    sources(id, &rs1, &rs2);

    // With forwarding, an instruction obtains its operands in EX. One
    // dependency remains for ID: a value not yet produced, because the load
    // producing it has not completed its memory access.
    if (is_load(ex.d.op) && (writes(ex, rs1) || writes(ex, rs2))) {
        h.stall = true;
        return h;
    }

    if (!resolves_in_id) {
        return h;
    }

    // Branch and jump targets are computed here, so a branch needs its operands
    // one stage earlier than other instructions, leaving its producer one
    // stage less in which to complete.
    if (!is_branch(id.op) && !is_jump(id.op)) {
        return h;
    }

    // A producer still in EX has no result to forward.
    if (writes(ex, rs1) || writes(ex, rs2)) {
        h.stall = true;
        return h;
    }
    // A load in MEM produces its value at the end of this cycle: too late for
    // the comparison here, in time for the register file in the next cycle.
    if (is_load(mem.d.op) && (writes(mem, rs1) || writes(mem, rs2))) {
        h.stall = true;
        return h;
    }

    // Any other producer in MEM can be forwarded from there.
    if (writes(mem, rs1)) {
        h.fwd_rs1 = FWD_EX_MEM;
    }
    if (writes(mem, rs2)) {
        h.fwd_rs2 = FWD_EX_MEM;
    }
    return h;
}

// The decision ID makes, by whichever of the two functions above belongs to the
// model being run.
Hazard hazard_in_id(const Decoded &id, bool id_valid, const Latch &ex,
                    const Latch &mem, const Latch &wb,
                    bool forwarding, bool resolves_in_id) {
    Hazard h;
    h.stall   = false;
    h.fwd_rs1 = FWD_NONE;
    h.fwd_rs2 = FWD_NONE;

    if (!id_valid) {
        return h;
    }

    // WB writes the register file in the first half of the cycle and ID reads
    // it in the second, so a dependency on the instruction in WB needs neither
    // a stall nor forwarding; hence neither function below takes it.
    (void)wb;

    return forwarding ? hazard_forwarding(id, ex, mem, resolves_in_id)
                      : hazard_stalling(id, ex, mem);
}

Forwarding forward_in_ex(const Latch &ex, const Latch &mem, const Latch &wb) {
    Forwarding f;
    f.rs1 = FWD_NONE;
    f.rs2 = FWD_NONE;

    if (!ex.valid) {
        return f;
    }

    int rs1, rs2;
    sources(ex.d, &rs1, &rs2);

    // MEM before WB: if both write this register, the instruction in MEM is the
    // more recent, and its value is the one the program intends.
    if (writes(mem, rs1)) {
        f.rs1 = FWD_EX_MEM;
    } else if (writes(wb, rs1)) {
        f.rs1 = FWD_MEM_WB;
    }
    if (writes(mem, rs2)) {
        f.rs2 = FWD_EX_MEM;
    } else if (writes(wb, rs2)) {
        f.rs2 = FWD_MEM_WB;
    }
    return f;
}

u32 forwarded_value(FwdSrc src, u32 from_regfile, const Latch &mem, const Latch &wb) {
    // A forwarded value is the value its producer will write to the register
    // file, so both cases below use writeback_value(). For jal this is the
    // return address, not the ALU result (the target).
    switch (src) {
        case FWD_NONE:
            return from_regfile;
        case FWD_EX_MEM:
            // This instruction has not yet accessed memory, so a load has no
            // data to forward. The load-use stall in hazard_in_id() prevents
            // this case; should it occur, report it rather than forward zero.
            if (is_load(mem.d.op)) {
                fatal("forwarding out of EX/MEM asked for the value of the load "
                      "at 0x%08x, which has not read memory yet. The load-use "
                      "stall in hazard_in_id() should have prevented this.",
                      mem.d.pc);
            }
            return writeback_value(mem.d, mem.e.result, mem.m);
        case FWD_MEM_WB:
            // The instruction in WB has completed its memory access, so every
            // field of its latch is set.
            return writeback_value(wb.d, wb.e.result, wb.m);
    }
    return from_regfile;
}

bool redirects_pc(const Latch &l) {
    return l.valid && redirects_pc(l.d, l.e);
}

int wrong_path_instructions(ResolveStage stage) {
    switch (stage) {
        case RESOLVE_ID:  return 1;
        case RESOLVE_EX:  return 2;
        case RESOLVE_MEM: return 3;
    }
    fatal("unknown branch resolution stage");
}

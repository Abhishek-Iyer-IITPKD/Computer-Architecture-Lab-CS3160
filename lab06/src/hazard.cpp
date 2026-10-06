#include "hazard.h"

#include "log.h"

// Does the instruction in `l` write register `n`?
//
// Two conditions are folded in here that every one of these comparisons needs.
// A latch that holds no instruction writes nothing -- a bubble must not look
// like a producer. And register 0 never counts: it is hardwired to zero, so a
// dependency on it is not a dependency at all, and treating it as one would
// stall the pipeline on every `addi zero, zero, 0` the assembler emits as a nop.
static bool writes(const Latch &l, int n) {
    return l.valid && n != 0 && writes_rd(l.d.op) && l.d.rd == n;
}

// The registers this instruction actually reads. An instruction whose format
// has no rs2 still has bits in that position -- they belong to the immediate --
// so asking the format, not the encoding, is what keeps a jal from appearing to
// depend on whatever register number its immediate happens to look like.
static void sources(const Decoded &d, int *rs1, int *rs2) {
    *rs1 = reads_rs1(d.op) ? d.rs1 : 0;
    *rs2 = reads_rs2(d.op) ? d.rs2 : 0;
}

// --- Lab 7: no forwarding paths, so the only remedy is to wait ------------

Hazard hazard_stalling(const Decoded &id, const Latch &ex, const Latch &mem) {
    Hazard h;
    h.stall   = false;
    h.fwd_rs1 = FWD_NONE;
    h.fwd_rs2 = FWD_NONE;

// TODO(lab 7): detect a read-after-write hazard.
//
// `id` is the instruction in the decode stage. `ex` and `mem` are the
// latches holding the two instructions ahead of it. There are no
// forwarding paths in this model -- that is lab 8 -- so the only thing
// this stage can do about a dependency is refuse to let the instruction
// past, which is what setting h.stall does.
//
// Set h.stall when `id` reads a register that either of those two is
// going to write. Two helpers just above do the fiddly parts:
//
//   sources()  which registers this instruction actually reads. Ask it
//              rather than reading d.rs1 and d.rs2 yourself: an
//              instruction whose format has no rs2 still has bits in
//              that position, and they belong to the immediate.
//   writes()   does the instruction in this latch write that register?
//              It already knows that a bubble writes nothing and that
//              x0 never counts.
//
// Note which latch is NOT in the list. The instruction in WB writes the
// register file in the first half of the cycle and this stage reads it
// in the second, so a dependency on it is already satisfied. That
// convention is why a back-to-back dependency costs two stall cycles
// here and not three.
(void)id;
(void)ex;
(void)mem;
(void)writes;
(void)sources;
    return h;
}

// --- Lab 8: forwarding, and the two dependencies it cannot fix ------------

Hazard hazard_forwarding(const Decoded &id, const Latch &ex, const Latch &mem,
                         bool resolves_in_id) {
    Hazard h;
    h.stall   = false;
    h.fwd_rs1 = FWD_NONE;
    h.fwd_rs2 = FWD_NONE;

// TODO(lab 8): what forwarding cannot fix in time.
//
// Most dependencies are satisfied one stage later, in EX, by
// forward_in_ex() below -- so this stage stalls for far less than it did
// last week. Exactly two cases are left, and both cost one cycle.
//
// The first exists in every forwarding pipeline: a load's value does not
// exist until its memory access finishes, so an instruction directly
// behind a load that reads what it loads cannot be given the value at
// any price. is_load() and the `ex` latch are what you need.
//
// The second exists only because this model decides branches here rather
// than in EX, which is what makes a mispredicted branch cost one
// instruction instead of three. A branch needs its operands one stage
// earlier than everybody else, so it has one stage less in which a
// producer can have finished. Guard this half with `resolves_in_id` and
// apply it only to branches and jumps:
//
//   * a producer still in EX has computed nothing yet     -> stall
//   * a load in MEM finishes at the end of this cycle,
//     too late to compare against                         -> stall
//   * anything else in MEM has its value                  -> forward it,
//     by setting h.fwd_rs1 / h.fwd_rs2 to FWD_EX_MEM
//
// The run loop applies those two fields through forwarded_value().
(void)id;
(void)ex;
(void)mem;
(void)resolves_in_id;
(void)writes;
(void)sources;
    return h;
}

// The question the ID stage actually asks, answered by whichever of the two
// above belongs to the model being run.
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

    // The instruction in WB writes the register file in the first half of the
    // cycle and ID reads it in the second, so a dependency on it is already
    // satisfied and needs neither a stall nor a forwarding path. Neither
    // function below is given it, and that is the reason.
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

// TODO(lab 8): where should this instruction's operands come from?
//
// The instruction in `ex` is about to enter the ALU. For each register it
// reads, decide whether the value it collected from the register file is
// still current, or whether a later instruction is about to overwrite it
// and the fresher value should be taken from where that instruction has
// got to. sources() and writes() above answer both halves.
//
// Set f.rs1 / f.rs2 to FWD_EX_MEM to take the value from the instruction
// now in MEM, FWD_MEM_WB to take it from the one in WB, and leave them
// FWD_NONE when the register file's value is already right.
//
// Order matters when both have written the same register: one of them is
// the later instruction, and its value is the one the program means.
//
// This function never stalls. The one dependency forwarding cannot
// satisfy -- a use of a value a load has not read yet -- was caught a
// stage earlier, in hazard_forwarding().
(void)mem;
(void)wb;
    return f;
}

u32 forwarded_value(FwdSrc src, u32 from_regfile, const Latch &mem, const Latch &wb) {
// TODO(lab 8): fetch the value forward_in_ex() decided to take.
//
// FWD_NONE means the register file was right: return from_regfile.
// Otherwise the value wanted is the one that instruction is going to
// write to the register file -- which is exactly what writeback_value()
// computes, so ask it rather than reaching for mem.e.result. They are not
// the same thing for a jal, whose ALU result is the address it jumped to
// and not the return address it writes.
//
// One case must not happen. An instruction in MEM has not performed its
// memory access yet, so if it is a load there is no value to hand over.
// The load-use stall in hazard_forwarding() is what guarantees you are
// never asked. If you are asked anyway, call fatal() and say so -- a
// forwarded zero would be a wrong answer that looks like a right one.
(void)src;
(void)mem;
(void)wb;
return from_regfile;
}

bool redirects_pc(const Latch &l) {
// TODO(lab 6): does this latch redirect the PC?
//
// The same question redirects_pc(Decoded, Executed) in processor.h
// answers -- which you wrote in lab 5 and is given to you here -- asked
// about a pipeline latch instead of a bare instruction.
//
// One thing to get right, and it is the whole reason this is a separate
// function: an empty latch redirects nothing. A bubble must not be
// mistaken for the instruction that was squashed out of it, or the
// pipeline will flush on a stage that is holding nothing at all.
(void)l;
return false;
}

int wrong_path_instructions(ResolveStage stage) {
// TODO(lab 6): how many instructions were fetched on the wrong path?
//
// A redirect is not acted on until the deciding instruction reaches the
// stage this model resolves control flow in. Everything fetched behind it
// in the meantime is on the wrong path and has to be thrown away.
//
// Count one per stage between fetch and resolution, inclusive. Work it
// out for each of the three rather than memorising the answers: it is the
// same argument three times, and it is what the flush in your run loop
// uses to decide how many latches to empty.
(void)stage;
return 0;
}

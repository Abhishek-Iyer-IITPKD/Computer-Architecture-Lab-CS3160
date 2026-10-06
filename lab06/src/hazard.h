// Hazard detection and forwarding, as functions of the pipeline state.
//
// This is the piece the 2025 Python simulator did not have. There, the
// detection was a handful of ad-hoc booleans -- rs1_ex_mem_fwd, id_stalling,
// three separate latch-freeze flags -- interleaved with stage execution inside
// one 223-line run(). Nothing about it could be tested except by running a
// whole program and looking at the cycle count.
//
// Here the decisions are pure functions of the latch contents, so a test can
// build a pipeline state by hand and ask what should happen. That is what makes
// the lab 7 and lab 8 labs gradable at the component level rather than only
// end to end.
//
// Two functions, because a pipeline asks two different questions in two
// different places:
//
//   hazard_in_id()   asked by the ID stage, before an instruction is allowed to
//                    move on: must the pipeline stall, and (in a model that
//                    resolves branches in ID) where do the operands come from?
//   forward_in_ex()  asked by the EX stage, at the moment the ALU needs its
//                    inputs: is a fresher value available from a later stage
//                    than the one the register file gave us?
//
// Control hazards are deliberately not answered here. Whether a flush is needed
// is not a property of the latches alone: in a model that resolves branches in
// ID, it is not known until that same ID stage has done its comparison. So the
// run loop decides it, using the two predicates at the bottom of this file, and
// the flush *cost* stays a consequence of one configuration value rather than a
// number written down twice.

#ifndef CS3160_HAZARD_H
#define CS3160_HAZARD_H

#include "processor.h"

enum FwdSrc {
    FWD_NONE,    // the value read from the register file is current
    FWD_EX_MEM,  // take the ALU result of the instruction now in MEM
    FWD_MEM_WB,  // take the value the instruction now in WB is writing back
};

struct Hazard {
    bool   stall;
    FwdSrc fwd_rs1;
    FwdSrc fwd_rs2;
};

struct Forwarding {
    FwdSrc rs1;
    FwdSrc rs2;
};

// What the ID stage must do about the instruction it holds.
//
//   id, id_valid   the instruction in ID
//   ex, mem, wb    the latches holding the instructions one, two and three
//                  stages ahead of it (ID/EX, EX/MEM, MEM/WB)
//   forwarding     does this model have forwarding paths?
//   resolves_in_id does this model compute branch and jump targets here, so
//                  that a branch needs its operands *now* rather than in EX?
//
// Without forwarding, an instruction waits in ID until every register it reads
// has been written back. A dependency on the instruction in WB needs no stall:
// the register file is written in the first half of the cycle and read in the
// second, so ID already sees it.
//
// With forwarding, most dependencies are satisfied later, in EX, and this
// function stalls only for what forwarding cannot fix in time -- and, when
// branches resolve here, for the operands this stage itself needs.
Hazard hazard_in_id(const Decoded &id, bool id_valid, const Latch &ex,
                    const Latch &mem, const Latch &wb,
                    bool forwarding, bool resolves_in_id);

// The two halves of that answer, one per model, split out because they are
// built in different weeks and can therefore be tested -- and stubbed --
// separately. Neither is given the WB latch: a dependency on the instruction in
// WB is already satisfied by the time this stage reads the register file.
//
// hazard_stalling() is the whole of a no-forwarding model's answer: wait until
// the producer has written back. hazard_forwarding() is what is left once
// values can be taken from where they are.
Hazard hazard_stalling(const Decoded &id, const Latch &ex, const Latch &mem);
Hazard hazard_forwarding(const Decoded &id, const Latch &ex, const Latch &mem,
                         bool resolves_in_id);

// Where the operands of the instruction in EX should come from.
//
// EX/MEM wins over MEM/WB when both have written the same register: it holds
// the more recent instruction, and its value is the one the program means.
//
// This function never needs to stall. The one dependency forwarding cannot
// satisfy -- a use of a value a load has not read yet -- was already caught one
// stage earlier by hazard_in_id(), which is where the load-use stall lives, as
// in the textbook's design.
Forwarding forward_in_ex(const Latch &ex, const Latch &mem, const Latch &wb);

// Apply what forward_in_ex() decided, returning the operand value to use.
u32 forwarded_value(FwdSrc src, u32 from_regfile, const Latch &mem, const Latch &wb);

// --- Control hazards -------------------------------------------------------

// Does a pipeline latch redirect the PC? The instruction-level question is
// redirects_pc(Decoded, Executed) in processor.h; this is that question asked
// about a latch. An empty latch redirects nothing:
// a bubble must not be mistaken for the instruction that was squashed out of it.
bool redirects_pc(const Latch &l);

// How many instructions are fetched on the wrong path before a redirect takes
// effect, when control resolves in `stage`: one per stage between fetch and
// resolution inclusive. ID gives 1, EX 2, MEM 3.
int wrong_path_instructions(ResolveStage stage);

#endif  // CS3160_HAZARD_H

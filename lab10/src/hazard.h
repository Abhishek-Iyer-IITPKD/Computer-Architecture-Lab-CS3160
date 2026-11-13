// Hazard detection and forwarding, as functions of the pipeline state.
//
// The decisions are pure functions of the latch contents, so that a test can
// construct a pipeline state and check the decision directly.
//
// A pipeline asks two questions, in two places:
//
//   hazard_in_id()   in ID, before an instruction proceeds: must the pipeline
//                    stall, and (when branches resolve in ID) where do the
//                    operands come from?
//   forward_in_ex()  in EX, when the ALU needs its inputs: does a later stage
//                    hold a more recent value than the register file?
//
// Control hazards are decided by the run loop, not here: when branches resolve
// in ID, the need for a flush is not known until ID has compared the operands.
// The loop uses the two predicates at the end of this file, so that the cost
// of a flush follows from a single configuration value.

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
//   forwarding     whether the model has forwarding paths
//   resolves_in_id whether branch and jump targets are computed in ID, so that
//                  a branch needs its operands here rather than in EX
//
// Without forwarding, an instruction waits in ID until every register it reads
// has been written back. A dependency on the instruction in WB needs no stall:
// the register file is written in the first half of the cycle and read in the
// second.
//
// With forwarding, most dependencies are satisfied in EX, and this function
// stalls only for those that forwarding cannot satisfy in time and, when
// branches resolve in ID, for the operands ID itself needs.
Hazard hazard_in_id(const Decoded &id, bool id_valid, const Latch &ex,
                    const Latch &mem, const Latch &wb,
                    bool forwarding, bool resolves_in_id);

// The two cases of hazard_in_id(), one per model, separately testable. Neither
// takes the WB latch, since a dependency on the instruction in WB is already
// satisfied when ID reads the register file.
//
// hazard_stalling() is the complete rule without forwarding: wait until the
// producer has written back. hazard_forwarding() covers the hazards that
// remain once values can be forwarded.
Hazard hazard_stalling(const Decoded &id, const Latch &ex, const Latch &mem);
Hazard hazard_forwarding(const Decoded &id, const Latch &ex, const Latch &mem,
                         bool resolves_in_id);

// Where the operands of the instruction in EX should come from.
//
// EX/MEM takes precedence over MEM/WB when both write the same register: it
// holds the more recent instruction, whose value the program intends.
//
// This function never stalls. The dependency forwarding cannot satisfy, the
// use of a value a load has not yet read, is detected one stage earlier by
// hazard_in_id(), as in the textbook design.
Forwarding forward_in_ex(const Latch &ex, const Latch &mem, const Latch &wb);

// The operand value at the source chosen by forward_in_ex().
u32 forwarded_value(FwdSrc src, u32 from_regfile, const Latch &mem, const Latch &wb);

// --- Control hazards -------------------------------------------------------

// Whether a latch redirects the PC: redirects_pc(Decoded, Executed), in
// processor.h, applied to a latch. An empty latch never redirects; a bubble
// must not be mistaken for the instruction flushed from it.
bool redirects_pc(const Latch &l);

// The number of instructions fetched on the wrong path before a redirect takes
// effect, when control resolves in `stage`: one per stage from fetch to the
// resolving stage, inclusive. ID gives 1, EX 2, MEM 3.
int wrong_path_instructions(ResolveStage stage);

#endif  // CS3160_HAZARD_H

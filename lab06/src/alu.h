// The execute stage's arithmetic, in two halves that mirror the datapath.
//
//   alu_inputs()  the operand multiplexers: which two values reach the ALU for
//                 this instruction -- registers, the immediate, the PC, or zero
//   alu_apply()   the ALU itself: one operation on two 32-bit values
//
// Splitting them this way is not decoration. Almost every instruction reaches
// the same adder; what differs is what is fed into it. Keeping the choice of
// operands separate from the operation is what makes an address calculation, an
// addi and a jump target visibly the same piece of hardware.
//
// Every value here is a 32-bit pattern (u32). Where an operation is signed --
// slt, sra, div, the signed branches -- it reinterprets its inputs, does the
// arithmetic, and hands back a pattern again. Nothing carries a sign around in
// its type, because the register file does not either.

#ifndef CS3160_ALU_H
#define CS3160_ALU_H

#include "decode.h"

struct AluInputs {
    u32 a;
    u32 b;
};

// Which two values this instruction's operands are, given what the register
// file returned for rs1 and rs2.
AluInputs alu_inputs(const Decoded &d, u32 v_rs1, u32 v_rs2);

// Apply the operation. For a load or a store this is the address calculation;
// for jal and jalr it is the target calculation (jalr's low bit cleared, as the
// specification requires); for a branch the result is unused -- the answer to a
// branch is a yes or no, which is branch_taken()'s job.
u32 alu_apply(Op op, u32 a, u32 b);

// Does this branch redirect the PC? Only meaningful when is_branch(op).
bool branch_taken(Op op, u32 a, u32 b);

#endif  // CS3160_ALU_H

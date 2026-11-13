// The execute stage's arithmetic, in two parts mirroring the datapath:
//
//   alu_inputs()  the operand multiplexers: the two values (registers, the
//                 immediate, the PC, or zero) that reach the ALU
//   alu_apply()   the ALU: one operation on two 32-bit values
//
// Almost every instruction uses the same adder and differs only in its inputs.
// Separating operand selection from the operation shows that an address
// calculation, an addi and a jump target use the same hardware.
//
// Every value is a 32-bit pattern (u32). A signed operation (slt, sra, div, the
// signed branches) reinterprets its inputs, computes, and returns a pattern. As
// in the register file, no value carries a sign in its type.

#ifndef CS3160_ALU_H
#define CS3160_ALU_H

#include "decode.h"

struct AluInputs {
    u32 a;
    u32 b;
};

// The two ALU operands of `d`, given the register values of rs1 and rs2.
AluInputs alu_inputs(const Decoded &d, u32 v_rs1, u32 v_rs2);

// Apply the operation. For a load or store this computes the address; for jal
// and jalr the target (with jalr's low bit cleared, as the specification
// requires). For a branch the result is unused: the outcome is given by
// branch_taken().
u32 alu_apply(Op op, u32 a, u32 b);

// Whether the branch is taken. Meaningful only when is_branch(op).
bool branch_taken(Op op, u32 a, u32 b);

#endif  // CS3160_ALU_H

// The ALU, and especially the four cases RISC-V defines that C++ does not:
// division by zero, remainder by zero, and the -2^31 / -1 overflow for both.
//
// tests/arith_edges.s runs the same cases under Spike, which is the real check.
// These tests state them in one screen, so that a change to alu.cpp that breaks
// one is reported as a named failure rather than as a mismatch 300 instructions
// into a trace.

#include "check.h"

#include "alu.h"

static const u32 MIN  = 0x80000000u;  // -2^31
static const u32 MAX  = 0x7FFFFFFFu;  //  2^31 - 1
static const u32 NEG1 = 0xFFFFFFFFu;  // -1, and also the largest unsigned

static u32 apply(Op op, i32 a, i32 b) {
    return alu_apply(op, as_unsigned(a), as_unsigned(b));
}

TEST(alu_add_and_sub_wrap) {
    CHECK_EQ(apply(OP_ADD, 2, 3), 5u);
    CHECK_EQ(apply(OP_SUB, 2, 3), as_unsigned(-1));
    CHECK_EQ(alu_apply(OP_ADD, MAX, 1), MIN);         // wraps, does not saturate
    CHECK_EQ(alu_apply(OP_SUB, MIN, 1), MAX);
    CHECK_EQ(alu_apply(OP_ADD, NEG1, 1), 0u);
}

TEST(alu_shifts_use_five_bits_of_the_amount) {
    CHECK_EQ(alu_apply(OP_SLL, 1, 0), 1u);
    CHECK_EQ(alu_apply(OP_SLL, 1, 31), MIN);
    CHECK_EQ(alu_apply(OP_SLL, 1, 32), 1u);   // 32 & 31 == 0: no shift at all
    CHECK_EQ(alu_apply(OP_SLL, 1, 33), 2u);
    CHECK_EQ(alu_apply(OP_SRL, MIN, 31), 1u);
    CHECK_EQ(alu_apply(OP_SRL, NEG1, 28), 0xFu);
}

TEST(alu_sra_shifts_the_sign_in) {
    CHECK_EQ(alu_apply(OP_SRA, MIN, 31), NEG1);
    CHECK_EQ(alu_apply(OP_SRA, MIN, 1), 0xC0000000u);
    CHECK_EQ(alu_apply(OP_SRA, NEG1, 31), NEG1);   // stays -1 however far it goes
    CHECK_EQ(alu_apply(OP_SRA, MAX, 31), 0u);      // positive: zeroes shift in
    CHECK_EQ(alu_apply(OP_SRA, 0x00000010u, 2), 4u);
}

TEST(alu_comparisons_differ_by_signedness) {
    // The same two patterns, compared both ways.
    CHECK_EQ(alu_apply(OP_SLT, NEG1, 1), 1u);    // -1 < 1
    CHECK_EQ(alu_apply(OP_SLTU, NEG1, 1), 0u);   // 0xffffffff > 1
    CHECK_EQ(alu_apply(OP_SLT, MIN, MAX), 1u);
    CHECK_EQ(alu_apply(OP_SLTU, MIN, MAX), 0u);
    CHECK_EQ(alu_apply(OP_SLT, 1, 1), 0u);       // strictly less than
    CHECK_EQ(alu_apply(OP_SLTU, 0, NEG1), 1u);
}

TEST(alu_signed_division_truncates_toward_zero) {
    // Not floor division: -7 / 2 is -3, not -4. The Python simulator used
    // Python's // operator and got -4.
    CHECK_EQ(apply(OP_DIV, 7, 2), as_unsigned(3));
    CHECK_EQ(apply(OP_DIV, -7, 2), as_unsigned(-3));
    CHECK_EQ(apply(OP_DIV, 7, -2), as_unsigned(-3));
    CHECK_EQ(apply(OP_DIV, -7, -2), as_unsigned(3));

    // The remainder takes the sign of the dividend, which is what makes
    // (a / b) * b + (a % b) == a hold.
    CHECK_EQ(apply(OP_REM, 7, 2), as_unsigned(1));
    CHECK_EQ(apply(OP_REM, -7, 2), as_unsigned(-1));
    CHECK_EQ(apply(OP_REM, 7, -2), as_unsigned(1));
    CHECK_EQ(apply(OP_REM, -7, -2), as_unsigned(-1));
}

TEST(alu_division_by_zero_is_defined) {
    // RISC-V has no divide-by-zero trap: the result is fixed instead.
    CHECK_EQ(apply(OP_DIV, 5, 0), NEG1);          // -1
    CHECK_EQ(apply(OP_DIV, -5, 0), NEG1);
    CHECK_EQ(apply(OP_DIV, 0, 0), NEG1);
    CHECK_EQ(alu_apply(OP_DIVU, 5, 0), NEG1);     // all ones

    // Remainder by zero yields the dividend, so the identity above still holds.
    CHECK_EQ(apply(OP_REM, 5, 0), 5u);
    CHECK_EQ(apply(OP_REM, -5, 0), as_unsigned(-5));
    CHECK_EQ(alu_apply(OP_REMU, 5, 0), 5u);
    CHECK_EQ(alu_apply(OP_REMU, MIN, 0), MIN);
}

TEST(alu_the_one_division_overflow) {
    // -2^31 / -1 is 2^31, which does not fit. RISC-V wraps to -2^31.
    CHECK_EQ(alu_apply(OP_DIV, MIN, NEG1), MIN);
    CHECK_EQ(alu_apply(OP_REM, MIN, NEG1), 0u);
    // Unsigned, the same patterns are 2^31 and 2^32 - 1, and nothing overflows.
    CHECK_EQ(alu_apply(OP_DIVU, MIN, NEG1), 0u);
    CHECK_EQ(alu_apply(OP_REMU, MIN, NEG1), MIN);
}

TEST(alu_unsigned_division_of_negative_looking_patterns) {
    CHECK_EQ(alu_apply(OP_DIVU, NEG1, 2), MAX);
    CHECK_EQ(alu_apply(OP_REMU, NEG1, 2), 1u);
    CHECK_EQ(apply(OP_DIV, -1, 2), 0u);            // truncates to zero
    CHECK_EQ(apply(OP_REM, -1, 2), NEG1);
}

TEST(alu_multiply_high_halves) {
    // -3 * 5 == -15: the low half is the pattern of -15, the signed high half
    // is its sign extension.
    CHECK_EQ(apply(OP_MUL, -3, 5), as_unsigned(-15));
    CHECK_EQ(apply(OP_MULH, -3, 5), NEG1);
    // Unsigned, -3 is 2^32 - 3, and (2^32 - 3) * 5 has 4 in its high half.
    CHECK_EQ(alu_apply(OP_MULHU, as_unsigned(-3), 5), 4u);
    // One of each: signed times unsigned.
    CHECK_EQ(alu_apply(OP_MULHSU, as_unsigned(-3), 5), NEG1);

    // 2^31 * 2^31 == 2^62, whose high half is 2^30. Read as unsigned the
    // operands are the same, so mulhu agrees; mulhsu has one of each, giving
    // -2^31 * 2^31 == -2^62.
    CHECK_EQ(alu_apply(OP_MUL, MIN, MIN), 0u);
    CHECK_EQ(alu_apply(OP_MULH, MIN, MIN), 0x40000000u);
    CHECK_EQ(alu_apply(OP_MULHU, MIN, MIN), 0x40000000u);
    CHECK_EQ(alu_apply(OP_MULHSU, MIN, MIN), 0xC0000000u);
}

TEST(alu_branch_conditions) {
    CHECK(branch_taken(OP_BEQ, 5, 5));
    CHECK(!branch_taken(OP_BEQ, 5, 6));
    CHECK(branch_taken(OP_BNE, 5, 6));
    CHECK(branch_taken(OP_BLT, NEG1, 1));      // -1 < 1
    CHECK(!branch_taken(OP_BLTU, NEG1, 1));    // 0xffffffff > 1
    CHECK(branch_taken(OP_BGEU, NEG1, 1));
    CHECK(branch_taken(OP_BGE, 1, 1));         // greater *or equal*
    CHECK(branch_taken(OP_BGE, MAX, MIN));
    CHECK(!branch_taken(OP_BGEU, MAX, MIN));
    // An operation that is not a branch never claims to be taken.
    CHECK(!branch_taken(OP_ADD, 1, 1));
}

TEST(alu_operand_selection) {
    // addi takes the immediate as its second operand, not rs2.
    Decoded d = decode(0x00558513u, 0x80002000u);  // addi a0, a1, 5
    AluInputs in = alu_inputs(d, 100, 200);
    CHECK_EQ(in.a, 100u);
    CHECK_EQ(in.b, 5u);

    // add takes both from the register file.
    d = decode(0x00c58533u, 0x80002000u);  // add a0, a1, a2
    in = alu_inputs(d, 100, 200);
    CHECK_EQ(in.a, 100u);
    CHECK_EQ(in.b, 200u);

    // A store's operands are the address calculation, not the value stored:
    // rs2 goes to memory, not to the ALU.
    d = decode(0x00a12223u, 0x80002000u);  // sw a0, 4(sp)
    in = alu_inputs(d, 0x80008000u, 0xDEADBEEFu);
    CHECK_EQ(in.a, 0x80008000u);
    CHECK_EQ(in.b, 4u);
    CHECK_EQ(alu_apply(d.op, in.a, in.b), 0x80008004u);

    // lui adds its immediate to zero; the immediate is already shifted.
    d = decode(0x80008537u, 0x80002000u);  // lui a0, 0x80008
    in = alu_inputs(d, 999, 999);
    CHECK_EQ(in.a, 0u);
    CHECK_EQ(in.b, 0x80008000u);
    CHECK_EQ(alu_apply(d.op, in.a, in.b), 0x80008000u);

    // auipc and jal both work from the instruction's own address.
    d = decode(0x00001517u, 0x80000094u);  // auipc a0, 0x1
    in = alu_inputs(d, 999, 999);
    CHECK_EQ(in.a, 0x80000094u);
    CHECK_EQ(alu_apply(d.op, in.a, in.b), 0x80001094u);

    d = decode(0x010000efu, 0x80002000u);  // jal ra, .+16
    in = alu_inputs(d, 999, 999);
    CHECK_EQ(alu_apply(d.op, in.a, in.b), 0x80002010u);
}

TEST(alu_jalr_clears_the_low_bit_of_its_target) {
    // jalr a0, 1(a1): the specification requires the target to be even.
    Decoded d = decode(0x00158567u, 0x80002000u);
    CHECK_EQ(d.op, OP_JALR);
    CHECK_EQ(d.imm, 1u);
    AluInputs in = alu_inputs(d, 0x80002100u, 0);
    CHECK_EQ(alu_apply(d.op, in.a, in.b), 0x80002100u);

    // And an odd base address is rounded down the same way.
    CHECK_EQ(alu_apply(OP_JALR, 0x80002101u, 0), 0x80002100u);
}

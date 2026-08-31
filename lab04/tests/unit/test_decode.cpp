// Decode: the fields, and especially the immediates.
//
// tests/check_disasm.py already compares every instruction in the corpus
// against objdump, which catches an immediate gathered from the wrong bits.
// What it cannot check is the parts of the Decoded struct that never reach the
// text: which registers the instruction is considered to read and write, and
// what the immediate's numeric value is. That is what these tests are for.

#include "check.h"

#include "decode.h"
#include "disasm.h"

TEST(decode_r_type_fields) {
    // add a0, a1, a2  ->  0x00c58533
    Decoded d = decode(0x00c58533u, 0x80000000u);
    CHECK_EQ(d.op, OP_ADD);
    CHECK_EQ(d.fmt, FMT_R);
    CHECK_EQ(d.rd, 10);
    CHECK_EQ(d.rs1, 11);
    CHECK_EQ(d.rs2, 12);
    CHECK_EQ(d.imm, 0u);
    CHECK(reads_rs1(d.op));
    CHECK(reads_rs2(d.op));
    CHECK(writes_rd(d.op));
}

TEST(decode_distinguishes_add_sub_and_mul) {
    // Same opcode and funct3; only funct7 differs.
    CHECK_EQ(decode(0x00c58533u, 0).op, OP_ADD);   // funct7 0x00
    CHECK_EQ(decode(0x40c58533u, 0).op, OP_SUB);   // funct7 0x20
    CHECK_EQ(decode(0x02c58533u, 0).op, OP_MUL);   // funct7 0x01
    CHECK_EQ(decode(0x00c5d533u, 0).op, OP_SRL);
    CHECK_EQ(decode(0x40c5d533u, 0).op, OP_SRA);
    CHECK_EQ(decode(0x02c5d533u, 0).op, OP_DIVU);
}

TEST(decode_i_immediate_is_sign_extended) {
    // addi a0, a1, -1  ->  0xfff58513
    Decoded d = decode(0xfff58513u, 0);
    CHECK_EQ(d.op, OP_ADDI);
    CHECK_EQ(as_signed(d.imm), -1);

    // addi a0, a1, -2048 and +2047, the ends of the field
    CHECK_EQ(as_signed(decode(0x80058513u, 0).imm), -2048);
    CHECK_EQ(as_signed(decode(0x7ff58513u, 0).imm), 2047);
    CHECK(!reads_rs2(OP_ADDI));
}

TEST(decode_shift_immediate_is_the_shift_amount) {
    // srai a0, a1, 31  ->  0x41f5d513. The high bits of the immediate field
    // select arithmetic-vs-logical and must not end up in the shift amount.
    Decoded d = decode(0x41f5d513u, 0);
    CHECK_EQ(d.op, OP_SRAI);
    CHECK_EQ(d.imm, 31u);

    // slli a0, a1, 0 -- a shift of zero is still a shift
    d = decode(0x00059513u, 0);
    CHECK_EQ(d.op, OP_SLLI);
    CHECK_EQ(d.imm, 0u);
}

TEST(decode_store_immediate_is_split_across_two_fields) {
    // sw a0, -4(sp)  ->  0xfea12e23
    Decoded d = decode(0xfea12e23u, 0);
    CHECK_EQ(d.op, OP_SW);
    CHECK_EQ(d.fmt, FMT_S);
    CHECK_EQ(d.rs1, 2);
    CHECK_EQ(d.rs2, 10);
    CHECK_EQ(d.rd, 0);
    CHECK_EQ(as_signed(d.imm), -4);
    CHECK(!writes_rd(d.op));
    CHECK(reads_rs2(d.op));

    // sw a0, 4(sp)  ->  0x00a12223
    CHECK_EQ(as_signed(decode(0x00a12223u, 0).imm), 4);
}

TEST(decode_branch_immediate_is_a_byte_offset) {
    // beq a0, a1, .+8  ->  0x00b50463
    Decoded d = decode(0x00b50463u, 0x80002000u);
    CHECK_EQ(d.op, OP_BEQ);
    CHECK_EQ(d.fmt, FMT_B);
    CHECK_EQ(as_signed(d.imm), 8);
    CHECK_EQ(d.pc + d.imm, 0x80002008u);
    CHECK(!writes_rd(d.op));

    // bne a0, a1, .-8  ->  0xfeb51ce3
    d = decode(0xfeb51ce3u, 0x80002000u);
    CHECK_EQ(d.op, OP_BNE);
    CHECK_EQ(as_signed(d.imm), -8);
    CHECK_EQ(d.pc + d.imm, 0x80001ff8u);
}

TEST(decode_jal_immediate_reaches_both_ways) {
    // jal ra, .+16  ->  0x010000ef
    Decoded d = decode(0x010000efu, 0x80002000u);
    CHECK_EQ(d.op, OP_JAL);
    CHECK_EQ(d.fmt, FMT_J);
    CHECK_EQ(d.rd, 1);
    CHECK_EQ(as_signed(d.imm), 16);
    CHECK(!reads_rs1(d.op));

    // jal ra, .-16  ->  0xff1ff0ef
    d = decode(0xff1ff0efu, 0x80002000u);
    CHECK_EQ(as_signed(d.imm), -16);
}

TEST(decode_upper_immediate_is_already_shifted) {
    // lui a0, 0x80008  ->  0x80008537. The value the instruction produces is
    // the immediate itself, so decode leaves it shifted into place.
    Decoded d = decode(0x80008537u, 0);
    CHECK_EQ(d.op, OP_LUI);
    CHECK_EQ(d.imm, 0x80008000u);
    CHECK_EQ(d.rd, 10);
    CHECK(!reads_rs1(d.op));

    // auipc a0, 0x1  ->  0x00001517
    d = decode(0x00001517u, 0x80000094u);
    CHECK_EQ(d.op, OP_AUIPC);
    CHECK_EQ(d.imm, 0x1000u);
    CHECK_EQ(d.pc + d.imm, 0x80001094u);
}

TEST(decode_loads_and_stores_report_their_width) {
    CHECK_EQ(mem_size(OP_LB), 1);
    CHECK_EQ(mem_size(OP_LBU), 1);
    CHECK_EQ(mem_size(OP_LH), 2);
    CHECK_EQ(mem_size(OP_LHU), 2);
    CHECK_EQ(mem_size(OP_LW), 4);
    CHECK_EQ(mem_size(OP_SB), 1);
    CHECK_EQ(mem_size(OP_SH), 2);
    CHECK_EQ(mem_size(OP_SW), 4);
    CHECK_EQ(mem_size(OP_ADD), 0);

    // Only the signed loads sign-extend.
    CHECK(mem_signed(OP_LB));
    CHECK(mem_signed(OP_LH));
    CHECK(!mem_signed(OP_LBU));
    CHECK(!mem_signed(OP_LHU));
    CHECK(!mem_signed(OP_LW));
}

TEST(decode_system_instructions) {
    CHECK_EQ(decode(0x00000073u, 0).op, OP_ECALL);
    CHECK_EQ(decode(0x00100073u, 0).op, OP_EBREAK);
    CHECK_EQ(decode(0x30200073u, 0).op, OP_MRET);
    CHECK_EQ(decode(0x0000100fu, 0).op, OP_FENCE_I);
    CHECK_EQ(decode(0x0ff0000fu, 0).op, OP_FENCE);

    // csrrs a0, mhartid, zero -- the read of the hart id in crt.S
    Decoded d = decode(0xf1402573u, 0);
    CHECK_EQ(d.op, OP_CSRRS);
    CHECK_EQ(d.csr, 0xf14u);
    CHECK_EQ(d.rd, 10);
    CHECK_EQ(d.rs1, 0);
    CHECK(writes_rd(d.op));

    // csrrs zero, mstatus, t0 -- the mstatus set-up in crt.S
    d = decode(0x3002a073u, 0);
    CHECK_EQ(d.op, OP_CSRRS);
    CHECK_EQ(d.csr, 0x300u);
    CHECK_EQ(d.rd, 0);
    CHECK_EQ(d.rs1, 5);

    // The immediate CSR forms put a constant where rs1 would be.
    d = decode(0x3001d073u, 0);  // csrrwi zero, mstatus, 3
    CHECK_EQ(d.op, OP_CSRRWI);
    CHECK_EQ(d.imm, 3u);
    CHECK(!reads_rs1(d.op));
}

TEST(decode_rejects_what_it_does_not_know) {
    // All zeroes: what a fetch off the end of the program returns.
    Decoded d = decode(0x00000000u, 0x80001000u);
    CHECK_EQ(d.op, OP_INVALID);
    CHECK_EQ(d.fmt, FMT_NONE);
    CHECK(!writes_rd(d.op));
    CHECK(!reads_rs1(d.op));
    CHECK_EQ(d.raw, 0u);
    CHECK_EQ(d.pc, 0x80001000u);

    CHECK_EQ(decode(0xffffffffu, 0).op, OP_INVALID);
    // A compressed instruction: the low two bits are never 0b11.
    CHECK_EQ(decode(0x00004501u, 0).op, OP_INVALID);
    // An M-extension funct7 on a funct3 the I extension does not define here.
    CHECK_EQ(decode(0x0ac58533u, 0).op, OP_INVALID);  // funct7 0x05: nothing
}

TEST(disasm_matches_objdump_conventions) {
    // Spot checks of the format; the real comparison is check_disasm.py.
    CHECK_STR_EQ(disasm(0x00c58533u, 0), "add a0, a1, a2");
    CHECK_STR_EQ(disasm(0xfff58513u, 0), "addi a0, a1, -1");
    CHECK_STR_EQ(disasm(0x41f5d513u, 0), "srai a0, a1, 0x1f");
    CHECK_STR_EQ(disasm(0xfea12e23u, 0), "sw a0, -4(sp)");
    CHECK_STR_EQ(disasm(0x00412503u, 0), "lw a0, 4(sp)");
    CHECK_STR_EQ(disasm(0x00b50463u, 0x80002000u), "beq a0, a1, 0x80002008");
    CHECK_STR_EQ(disasm(0x010000efu, 0x80002000u), "jal ra, 0x80002010");
    CHECK_STR_EQ(disasm(0x00008067u, 0), "jalr zero, 0(ra)");
    CHECK_STR_EQ(disasm(0x80008537u, 0), "lui a0, 0x80008");
    CHECK_STR_EQ(disasm(0xf1402573u, 0), "csrrs a0, mhartid, zero");
    CHECK_STR_EQ(disasm(0x00000000u, 0), ".word 0x00000000");
}

TEST(register_names_are_the_abi_names) {
    CHECK_STR_EQ(reg_name(0), "zero");
    CHECK_STR_EQ(reg_name(1), "ra");
    CHECK_STR_EQ(reg_name(2), "sp");
    CHECK_STR_EQ(reg_name(8), "s0");
    CHECK_STR_EQ(reg_name(10), "a0");
    CHECK_STR_EQ(reg_name(17), "a7");
    CHECK_STR_EQ(reg_name(27), "s11");
    CHECK_STR_EQ(reg_name(31), "t6");
}

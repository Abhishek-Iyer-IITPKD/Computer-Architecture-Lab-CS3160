#include "alu.h"

#include "log.h"

// The most negative 32-bit number. Negating it overflows, which is the one case
// signed division has to be told about.
static const u32 INT32_MIN_BITS = 0x80000000u;

AluInputs alu_inputs(const Decoded &d, u32 v_rs1, u32 v_rs2) {
    AluInputs in;
    in.a = 0;
    in.b = 0;

    switch (d.op) {
        // Register-register: both operands come from the register file.
        case OP_ADD: case OP_SUB: case OP_SLL: case OP_SLT: case OP_SLTU:
        case OP_XOR: case OP_SRL: case OP_SRA: case OP_OR: case OP_AND:
        case OP_MUL: case OP_MULH: case OP_MULHSU: case OP_MULHU:
        case OP_DIV: case OP_DIVU: case OP_REM: case OP_REMU:
        // Branches compare two registers.
        case OP_BEQ: case OP_BNE: case OP_BLT:
        case OP_BGE: case OP_BLTU: case OP_BGEU:
            in.a = v_rs1;
            in.b = v_rs2;
            break;

        // Register-immediate, and the address calculations, which are the same
        // addition: base register plus a signed offset.
        case OP_ADDI: case OP_SLTI: case OP_SLTIU: case OP_XORI:
        case OP_ORI: case OP_ANDI: case OP_SLLI: case OP_SRLI: case OP_SRAI:
        case OP_LB: case OP_LH: case OP_LW: case OP_LBU: case OP_LHU:
        case OP_SB: case OP_SH: case OP_SW:
        case OP_JALR:
            in.a = v_rs1;
            in.b = d.imm;
            break;

        // lui resuces its immediate and nothing else, so it adds it to zero.
        case OP_LUI:
            in.a = 0;
            in.b = d.imm;
            break;

        // auipc and jal are both "this instruction's address plus a constant".
        case OP_AUIPC:
        case OP_JAL:
            in.a = d.pc;
            in.b = d.imm;
            break;

        // Nothing to compute: the CSR instructions read a register we do not
        // model, and the fences and environment calls resuce no value.
        case OP_CSRRW: case OP_CSRRS: case OP_CSRRC:
        case OP_CSRRWI: case OP_CSRRSI: case OP_CSRRCI:
        case OP_FENCE: case OP_FENCE_I:
        case OP_ECALL: case OP_EBREAK: case OP_MRET:
        case OP_INVALID: case OP_COUNT:
            break;
    }
    return in;
}

// Shifts use only the low five bits of the amount: a 32-bit register can only
// be shifted 0..31 places, and the specification says the rest are ignored
// rather than making the instruction illegal.
static int shift_amount(u32 b) { return (int)(b & 0x1Fu); }

u32 alu_apply(Op op, u32 a, u32 b) {
// TODO(week 5): the ALU.
//
// Return the result of applying `op` to `a` and `b`. The two operands have
// already been chosen for you by alu_inputs() above: for an `add` they are
// two registers, for an `addi` a register and the immediate, and for a
// load, a store or a jump they are the base and the offset. That is the
// point of doing it there -- every arithmetic instruction, every address
// calculation and every jump target ends up as the same switch here.
//
// Write OP_ADD and OP_SUB first and run `make test`. Each unit test names
// the operation it was checking, so the failures tell you what to write
// next. Four of them are worth reading the notes above for before you
// attempt them -- division and remainder by zero, and the -2^31 / -1
// overflow of both -- because RISC-V defines all four and C++ does not.
//
// shift_amount() just above is here for when you get to the shifts: a
// 32-bit register can only move 0 to 31 places, so only the low five bits
// of the amount count and the rest are ignored rather than illegal.
u32 out;
i64 res;
switch(op){
    case OP_ADD: case OP_ADDI: 
    case OP_LB: case OP_LH: case OP_LW: case OP_LBU: case OP_LHU:
    case OP_SB: case OP_SH: case OP_SW:
    case OP_AUIPC: case OP_LUI:
        return a+b;
    case OP_SUB:
        return a-b;
    case OP_MUL:
        return a*b;
    case OP_MULH:
        res = (i64) as_signed(a) * (i64) as_signed(b);
        return res >> 32;
    case OP_MULHSU: 
        res = (i64) as_signed(a) * (u64) as_unsigned(b);
        return res >> 32;
    case OP_MULHU:
        res = (u64) as_unsigned(a) * (u64) as_unsigned(b);
        return res >> 32;
    case OP_DIV:
        if(a == 0x80000000 && b == -1) return 0x80000000;
        if(b == 0) return -1;
        return as_signed(a) / as_signed(b);
    case OP_DIVU:
        if(b == 0) return -1;
        return as_unsigned(a) / as_unsigned(b);
    case OP_REM:
        if(a == 0x80000000 && b == -1) return 0;
        if(b == 0) return a;
        return as_signed(a) % as_signed(b);
    case OP_REMU:
        if(b == 0) return a;
        return as_unsigned(a) % as_unsigned(b);
    case OP_SLL: case OP_SLLI: 
        return a << shift_amount(b);
    case OP_SRL: case OP_SRLI:
        return a >> shift_amount(b);
    case OP_SRA: case OP_SRAI:
        return sign_extend(a >> shift_amount(b), 32-shift_amount(b));
    case OP_SLT: case OP_SLTI:
        return as_signed(a) < as_signed(b);
    case OP_SLTU: case OP_SLTIU:
        return a < b;
    case OP_JALR: case OP_JAL:
        return (a + b) >> 1 << 1;
}
return 0;
}

bool branch_taken(Op op, u32 a, u32 b) {
// TODO(week 5): is this branch taken?
//
// `a` and `b` are the two registers the branch compares. Return true if
// control should go to the branch target rather than to the next
// instruction. Mind the signed and unsigned pairs: blt/bge compare as
// signed numbers, bltu/bgeu as unsigned, and as_signed() in common.h is
// how you say which you mean.
switch(op){
    case OP_BEQ:
        return a == b;
    case OP_BNE:
        return a != b;
    case OP_BLT:
        return as_signed(a) < as_signed(b);
    case OP_BLTU:
        return as_unsigned(a) < as_unsigned(b);
    case OP_BGE:
        return as_signed(a) >= as_signed(b);
    case OP_BGEU:
        return as_unsigned(a) >= as_unsigned(b);
}
return false;
}

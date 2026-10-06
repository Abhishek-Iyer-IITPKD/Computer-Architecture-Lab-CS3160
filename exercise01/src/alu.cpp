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

        // lui produces its immediate and nothing else, so it adds it to zero.
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
        // model, and the fences and environment calls produce no value.
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
    switch (op) {
        case OP_ADD: case OP_ADDI:
        // Address and target calculations are additions.
        case OP_LB: case OP_LH: case OP_LW: case OP_LBU: case OP_LHU:
        case OP_SB: case OP_SH: case OP_SW:
        case OP_LUI: case OP_AUIPC: case OP_JAL:
            return a + b;  // u32 wraps around, which is what the hardware does

        case OP_JALR:
            // The specification requires the low bit of a jalr target to be
            // cleared, so that jalr rd, 1(rs1) still lands on an instruction.
            return (a + b) & ~1u;

        case OP_SUB:
            return a - b;

        case OP_XOR: case OP_XORI: return a ^ b;
        case OP_OR:  case OP_ORI:  return a | b;
        case OP_AND: case OP_ANDI: return a & b;

        case OP_SLL: case OP_SLLI:
            return a << shift_amount(b);
        case OP_SRL: case OP_SRLI:
            return a >> shift_amount(b);  // a is unsigned: this shifts in zeroes
        case OP_SRA: case OP_SRAI:
            // Arithmetic: the sign bit is copied into the vacated positions.
            return (u32)(as_signed(a) >> shift_amount(b));

        case OP_SLT: case OP_SLTI:
            return (as_signed(a) < as_signed(b)) ? 1u : 0u;
        case OP_SLTU: case OP_SLTIU:
            return (a < b) ? 1u : 0u;

        case OP_MUL:
            // The low 32 bits of the product are the same whether the operands
            // are read as signed or unsigned, so one case covers both.
            return (u32)((u64)a * (u64)b);
        case OP_MULH:
            return (u32)(((i64)as_signed(a) * (i64)as_signed(b)) >> 32);
        case OP_MULHU:
            return (u32)(((u64)a * (u64)b) >> 32);
        case OP_MULHSU:
            // One signed, one unsigned. The product of a value in
            // [-2^31, 2^31) and one in [0, 2^32) fits in a signed 64-bit
            // number, so this needs no wider arithmetic.
            return (u32)(((i64)as_signed(a) * (i64)(u64)b) >> 32);

        // Division has three cases the hardware must answer for, and RISC-V
        // fixes all three rather than trapping. The Python simulator
        // implemented none of them, and used Python's floor division, which
        // rounds -7 / 2 to -4 where RISC-V truncates it to -3.
        case OP_DIV:
            if (b == 0) {
                return 0xFFFFFFFFu;  // divide by zero yields -1
            }
            if (a == INT32_MIN_BITS && b == 0xFFFFFFFFu) {
                return INT32_MIN_BITS;  // the one overflow: -2^31 / -1
            }
            return (u32)(as_signed(a) / as_signed(b));  // C++ truncates toward zero
        case OP_DIVU:
            if (b == 0) {
                return 0xFFFFFFFFu;
            }
            return a / b;
        case OP_REM:
            if (b == 0) {
                return a;  // remainder by zero yields the dividend
            }
            if (a == INT32_MIN_BITS && b == 0xFFFFFFFFu) {
                return 0;
            }
            return (u32)(as_signed(a) % as_signed(b));
        case OP_REMU:
            if (b == 0) {
                return a;
            }
            return a % b;

        // A branch's answer is a direction, not a value. Ask branch_taken().
        case OP_BEQ: case OP_BNE: case OP_BLT:
        case OP_BGE: case OP_BLTU: case OP_BGEU:
            return 0;

        // No arithmetic. A CSR read yields 0: the simulator models no control
        // and status registers, and the only reads in the shipped runtime are
        // crt.S asking for its hart id, which is 0 on a single-core machine.
        case OP_CSRRW: case OP_CSRRS: case OP_CSRRC:
        case OP_CSRRWI: case OP_CSRRSI: case OP_CSRRCI:
        case OP_FENCE: case OP_FENCE_I:
        case OP_ECALL: case OP_EBREAK: case OP_MRET:
        case OP_INVALID: case OP_COUNT:
            return 0;
    }
    return 0;
}

bool branch_taken(Op op, u32 a, u32 b) {
    switch (op) {
        case OP_BEQ:  return a == b;
        case OP_BNE:  return a != b;
        case OP_BLT:  return as_signed(a) < as_signed(b);
        case OP_BGE:  return as_signed(a) >= as_signed(b);
        case OP_BLTU: return a < b;
        case OP_BGEU: return a >= b;
        default:      return false;
    }
}

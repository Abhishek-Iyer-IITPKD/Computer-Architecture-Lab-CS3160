#include "decode.h"

#include "log.h"

// Opcodes, in the order they appear in the RISC-V manual's opcode map.
enum {
    OPC_LOAD   = 0x03,
    OPC_FENCE  = 0x0F,
    OPC_OPIMM  = 0x13,
    OPC_AUIPC  = 0x17,
    OPC_STORE  = 0x23,
    OPC_OP     = 0x33,
    OPC_LUI    = 0x37,
    OPC_BRANCH = 0x63,
    OPC_JALR   = 0x67,
    OPC_JAL    = 0x6F,
    OPC_SYSTEM = 0x73,
};

// --- Immediate extraction --------------------------------------------------
//
// One function per format. Each one gathers the immediate's bits from wherever
// that format scattered them and sign-extends the result. The scattering looks
// arbitrary but is not: it keeps the sign bit at bit 31 and the register fields
// at fixed positions in every format, so the hardware can start reading
// registers before it has finished working out which instruction this is.

static u32 imm_i(u32 raw) {
    return sign_extend(bits(raw, 31, 20), 12);
}

static u32 imm_s(u32 raw) {
    u32 v = (bits(raw, 31, 25) << 5) | bits(raw, 11, 7);
    return sign_extend(v, 12);
}

static u32 imm_b(u32 raw) {
    u32 v = (bits(raw, 31, 31) << 12) | (bits(raw, 7, 7) << 11) |
            (bits(raw, 30, 25) << 5) | (bits(raw, 11, 8) << 1);
    return sign_extend(v, 13);  // bit 0 is not encoded: offsets are even
}

static u32 imm_u(u32 raw) {
    return raw & 0xFFFFF000u;  // already in place; the low 12 bits are zero
}

static u32 imm_j(u32 raw) {
    u32 v = (bits(raw, 31, 31) << 20) | (bits(raw, 19, 12) << 12) |
            (bits(raw, 20, 20) << 11) | (bits(raw, 30, 21) << 1);
    return sign_extend(v, 21);
}

// --- decode ----------------------------------------------------------------

Decoded decode_none(void) {
    Decoded d;
    d.raw = 0;
    d.pc  = 0;
    d.op  = OP_INVALID;
    d.fmt = FMT_NONE;
    d.rs1 = 0;
    d.rs2 = 0;
    d.rd  = 0;
    d.imm = 0;
    d.csr = 0;
    return d;
}

Decoded decode(u32 raw, u32 pc) {
    Decoded d = decode_none();
    d.raw = raw;
    d.pc  = pc;

    u32 opcode = bits(raw, 6, 0);
    u32 funct3 = bits(raw, 14, 12);
    u32 funct7 = bits(raw, 31, 25);

    // Register fields sit in the same place in every format that has them; the
    // format then decides which of them mean anything.
    int rd  = (int)bits(raw, 11, 7);
    int rs1 = (int)bits(raw, 19, 15);
    int rs2 = (int)bits(raw, 24, 20);

    switch (opcode) {
        case OPC_OP:
            d.fmt = FMT_R;
            d.rd = rd; d.rs1 = rs1; d.rs2 = rs2;
            if (funct7 == 0x00) {
                switch (funct3) {
                    case 0x0: d.op = OP_ADD;  break;
                    case 0x1: d.op = OP_SLL;  break;
                    case 0x2: d.op = OP_SLT;  break;
                    case 0x3: d.op = OP_SLTU; break;
                    case 0x4: d.op = OP_XOR;  break;
                    case 0x5: d.op = OP_SRL;  break;
                    case 0x6: d.op = OP_OR;   break;
                    case 0x7: d.op = OP_AND;  break;
                }
            } else if (funct7 == 0x20) {
                if (funct3 == 0x0)      d.op = OP_SUB;
                else if (funct3 == 0x5) d.op = OP_SRA;
            } else if (funct7 == 0x01) {
                // The M extension shares the OP opcode and is selected by funct7.
                switch (funct3) {
                    case 0x0: d.op = OP_MUL;    break;
                    case 0x1: d.op = OP_MULH;   break;
                    case 0x2: d.op = OP_MULHSU; break;
                    case 0x3: d.op = OP_MULHU;  break;
                    case 0x4: d.op = OP_DIV;    break;
                    case 0x5: d.op = OP_DIVU;   break;
                    case 0x6: d.op = OP_REM;    break;
                    case 0x7: d.op = OP_REMU;   break;
                }
            }
            break;

        case OPC_OPIMM:
            d.fmt = FMT_I;
            d.rd = rd; d.rs1 = rs1;
            d.imm = imm_i(raw);
            switch (funct3) {
                case 0x0: d.op = OP_ADDI;  break;
                case 0x2: d.op = OP_SLTI;  break;
                case 0x3: d.op = OP_SLTIU; break;
                case 0x4: d.op = OP_XORI;  break;
                case 0x6: d.op = OP_ORI;   break;
                case 0x7: d.op = OP_ANDI;  break;
                case 0x1:
                    // Shifts are not really I-format: the immediate is a 5-bit
                    // shift amount and the bits above it select the operation.
                    if (funct7 == 0x00) {
                        d.op  = OP_SLLI;
                        d.imm = bits(raw, 24, 20);
                    }
                    break;
                case 0x5:
                    if (funct7 == 0x00) {
                        d.op  = OP_SRLI;
                        d.imm = bits(raw, 24, 20);
                    } else if (funct7 == 0x20) {
                        d.op  = OP_SRAI;
                        d.imm = bits(raw, 24, 20);
                    }
                    break;
            }
            break;

        case OPC_LOAD:
            d.fmt = FMT_I;
            d.rd = rd; d.rs1 = rs1;
            d.imm = imm_i(raw);
            switch (funct3) {
                case 0x0: d.op = OP_LB;  break;
                case 0x1: d.op = OP_LH;  break;
                case 0x2: d.op = OP_LW;  break;
                case 0x4: d.op = OP_LBU; break;
                case 0x5: d.op = OP_LHU; break;
            }
            break;

        case OPC_STORE:
            d.fmt = FMT_S;
            d.rs1 = rs1; d.rs2 = rs2;
            d.imm = imm_s(raw);
            switch (funct3) {
                case 0x0: d.op = OP_SB; break;
                case 0x1: d.op = OP_SH; break;
                case 0x2: d.op = OP_SW; break;
            }
            break;

        case OPC_BRANCH:
            d.fmt = FMT_B;
            d.rs1 = rs1; d.rs2 = rs2;
            d.imm = imm_b(raw);
            switch (funct3) {
                case 0x0: d.op = OP_BEQ;  break;
                case 0x1: d.op = OP_BNE;  break;
                case 0x4: d.op = OP_BLT;  break;
                case 0x5: d.op = OP_BGE;  break;
                case 0x6: d.op = OP_BLTU; break;
                case 0x7: d.op = OP_BGEU; break;
            }
            break;

        case OPC_LUI:
            d.fmt = FMT_U;
            d.rd = rd;
            d.imm = imm_u(raw);
            d.op = OP_LUI;
            break;

        case OPC_AUIPC:
            d.fmt = FMT_U;
            d.rd = rd;
            d.imm = imm_u(raw);
            d.op = OP_AUIPC;
            break;

        case OPC_JAL:
            d.fmt = FMT_J;
            d.rd = rd;
            d.imm = imm_j(raw);
            d.op = OP_JAL;
            break;

        case OPC_JALR:
            d.fmt = FMT_I;
            d.rd = rd; d.rs1 = rs1;
            d.imm = imm_i(raw);
            if (funct3 == 0x0) {
                d.op = OP_JALR;
            }
            break;

        case OPC_FENCE:
            d.fmt = FMT_I;
            d.imm = imm_i(raw);
            if (funct3 == 0x0)      d.op = OP_FENCE;
            else if (funct3 == 0x1) d.op = OP_FENCE_I;
            break;

        case OPC_SYSTEM:
            d.fmt = FMT_I;
            d.csr = bits(raw, 31, 20);
            if (funct3 == 0x0) {
                // ecall, ebreak and mret are distinguished by the whole
                // immediate field rather than by funct7 alone.
                if (raw == 0x00000073u)      d.op = OP_ECALL;
                else if (raw == 0x00100073u) d.op = OP_EBREAK;
                else if (raw == 0x30200073u) d.op = OP_MRET;
            } else {
                d.rd = rd;
                switch (funct3) {
                    // The register forms read rs1; the immediate forms carry a
                    // 5-bit zero-extended constant in the same field.
                    case 0x1: d.op = OP_CSRRW;  d.rs1 = rs1; break;
                    case 0x2: d.op = OP_CSRRS;  d.rs1 = rs1; break;
                    case 0x3: d.op = OP_CSRRC;  d.rs1 = rs1; break;
                    case 0x5: d.op = OP_CSRRWI; d.imm = (u32)rs1; break;
                    case 0x6: d.op = OP_CSRRSI; d.imm = (u32)rs1; break;
                    case 0x7: d.op = OP_CSRRCI; d.imm = (u32)rs1; break;
                }
            }
            break;

        default:
            break;
    }

    if (d.op == OP_INVALID) {
        // Not fatal here: fetching rubbish is a symptom, and the stage that
        // tries to execute it is where the useful error message can be given.
        d.fmt = FMT_NONE;
        d.rd = 0; d.rs1 = 0; d.rs2 = 0; d.imm = 0;
    }
    return d;
}

// --- Questions about an operation ------------------------------------------

bool is_branch(Op op) {
    switch (op) {
        case OP_BEQ: case OP_BNE: case OP_BLT:
        case OP_BGE: case OP_BLTU: case OP_BGEU:
            return true;
        default:
            return false;
    }
}

bool is_jump(Op op) {
    return op == OP_JAL || op == OP_JALR;
}

bool is_load(Op op) {
    switch (op) {
        case OP_LB: case OP_LH: case OP_LW: case OP_LBU: case OP_LHU:
            return true;
        default:
            return false;
    }
}

bool is_store(Op op) {
    return op == OP_SB || op == OP_SH || op == OP_SW;
}

bool is_csr(Op op) {
    switch (op) {
        case OP_CSRRW: case OP_CSRRS: case OP_CSRRC:
        case OP_CSRRWI: case OP_CSRRSI: case OP_CSRRCI:
            return true;
        default:
            return false;
    }
}

bool writes_rd(Op op) {
    if (op == OP_INVALID) {
        return false;
    }
    if (is_branch(op) || is_store(op)) {
        return false;
    }
    switch (op) {
        case OP_FENCE: case OP_FENCE_I:
        case OP_ECALL: case OP_EBREAK: case OP_MRET:
            return false;
        default:
            return true;
    }
}

bool reads_rs1(Op op) {
    if (op == OP_INVALID) {
        return false;
    }
    switch (op) {
        // Nothing else: U-format, J-format, the fences, the environment calls
        // and the immediate CSR forms take no register operand.
        case OP_LUI: case OP_AUIPC: case OP_JAL:
        case OP_FENCE: case OP_FENCE_I:
        case OP_ECALL: case OP_EBREAK: case OP_MRET:
        case OP_CSRRWI: case OP_CSRRSI: case OP_CSRRCI:
            return false;
        default:
            return true;
    }
}

bool reads_rs2(Op op) {
    // Only the R format and the two formats built around a second source --
    // stores and branches -- read rs2.
    if (is_branch(op) || is_store(op)) {
        return true;
    }
    switch (op) {
        case OP_ADD: case OP_SUB: case OP_SLL: case OP_SLT: case OP_SLTU:
        case OP_XOR: case OP_SRL: case OP_SRA: case OP_OR: case OP_AND:
        case OP_MUL: case OP_MULH: case OP_MULHSU: case OP_MULHU:
        case OP_DIV: case OP_DIVU: case OP_REM: case OP_REMU:
            return true;
        default:
            return false;
    }
}

int mem_size(Op op) {
    switch (op) {
        case OP_LB: case OP_LBU: case OP_SB: return 1;
        case OP_LH: case OP_LHU: case OP_SH: return 2;
        case OP_LW: case OP_SW:              return 4;
        default:                             return 0;
    }
}

bool mem_signed(Op op) {
    return op == OP_LB || op == OP_LH;
}

const char *op_name(Op op) {
    switch (op) {
        case OP_INVALID: return "?";
        case OP_ADD:     return "add";
        case OP_SUB:     return "sub";
        case OP_SLL:     return "sll";
        case OP_SLT:     return "slt";
        case OP_SLTU:    return "sltu";
        case OP_XOR:     return "xor";
        case OP_SRL:     return "srl";
        case OP_SRA:     return "sra";
        case OP_OR:      return "or";
        case OP_AND:     return "and";
        case OP_MUL:     return "mul";
        case OP_MULH:    return "mulh";
        case OP_MULHSU:  return "mulhsu";
        case OP_MULHU:   return "mulhu";
        case OP_DIV:     return "div";
        case OP_DIVU:    return "divu";
        case OP_REM:     return "rem";
        case OP_REMU:    return "remu";
        case OP_ADDI:    return "addi";
        case OP_SLTI:    return "slti";
        case OP_SLTIU:   return "sltiu";
        case OP_XORI:    return "xori";
        case OP_ORI:     return "ori";
        case OP_ANDI:    return "andi";
        case OP_SLLI:    return "slli";
        case OP_SRLI:    return "srli";
        case OP_SRAI:    return "srai";
        case OP_LUI:     return "lui";
        case OP_AUIPC:   return "auipc";
        case OP_JAL:     return "jal";
        case OP_JALR:    return "jalr";
        case OP_BEQ:     return "beq";
        case OP_BNE:     return "bne";
        case OP_BLT:     return "blt";
        case OP_BGE:     return "bge";
        case OP_BLTU:    return "bltu";
        case OP_BGEU:    return "bgeu";
        case OP_LB:      return "lb";
        case OP_LH:      return "lh";
        case OP_LW:      return "lw";
        case OP_LBU:     return "lbu";
        case OP_LHU:     return "lhu";
        case OP_SB:      return "sb";
        case OP_SH:      return "sh";
        case OP_SW:      return "sw";
        case OP_FENCE:   return "fence";
        case OP_FENCE_I: return "fence.i";
        case OP_ECALL:   return "ecall";
        case OP_EBREAK:  return "ebreak";
        case OP_MRET:    return "mret";
        case OP_CSRRW:   return "csrrw";
        case OP_CSRRS:   return "csrrs";
        case OP_CSRRC:   return "csrrc";
        case OP_CSRRWI:  return "csrrwi";
        case OP_CSRRSI:  return "csrrsi";
        case OP_CSRRCI:  return "csrrci";
        case OP_COUNT:   break;
    }
    return "?";
}

#include "decode.h"

#include "log.h"

// Opcodes, in the order they appear in the RISC-V manual's opcode map.
enum {
    OPC_LOAD   = 0x03, // I
    OPC_FENCE  = 0x0F, // 
    OPC_OPIMM  = 0x13, // I
    OPC_AUIPC  = 0x17, // U
    OPC_STORE  = 0x23, // S
    OPC_OP     = 0x33, // R
    OPC_LUI    = 0x37, // U
    OPC_BRANCH = 0x63, // B
    OPC_JALR   = 0x67, // I
    OPC_JAL    = 0x6F, // J
    OPC_SYSTEM = 0x73, // I
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
// TODO(week 4): turn 32 bits into a Decoded.
//
// Every RISC-V instruction carries its opcode in the low 7 bits. That
// tells you the *format* -- R, I, S, B, U or J -- and the format tells
// you which other fields exist and where the immediate is scattered.
// funct3 and funct7 then separate instructions that share an opcode.
//
// The immediate helpers are already written for you at the top of this
// file: imm_i, imm_s, imm_b, imm_u, imm_j. They exist because the
// immediate is the one field RISC-V genuinely scrambles, and they all
// return it **sign-extended into a u32**, ready to use.
//
// Fill in d.op, d.fmt, d.rd, d.rs1, d.rs2 and d.imm, and leave d.pc as
// the pc you were given. An encoding you do not recognise is OP_INVALID
// with FMT_NONE, not a crash -- whoever tries to execute it is where the
// useful error message belongs.
//
// Work opcode by opcode and run `make test` after each: the unit tests
// are named for the instruction they decode. When those pass, the
// harness also checks your decoder against objdump over a program
// holding one of every instruction, which is a much harsher test than
// any you would write by hand.
Decoded d = decode_none();

u32 opcode = bits(raw, 6, 0);
u32 rd = bits(raw, 11, 7);
u32 funct3 = bits(raw, 14, 12);
u32 rs1 = bits(raw, 19, 15);
u32 rs2 = bits(raw, 24, 20);
u32 funct7 = bits(raw, 31, 25);

switch(opcode){
    case OPC_LOAD:
        d.fmt = FMT_I;
        break;
    case OPC_FENCE:
        // d.fmt = FMT_;
        break;
    case OPC_OPIMM:
        d.fmt = FMT_I;
        break;
    case OPC_AUIPC:
        d.fmt = FMT_U;
        break;
    case OPC_STORE:
        d.fmt = FMT_S;
        break;
    case OPC_OP:
        d.fmt = FMT_R;
        break;
    case OPC_LUI:
        d.fmt = FMT_U;
        break;
    case OPC_BRANCH:
        d.fmt = FMT_B;
        break;
    case OPC_JALR:
        d.fmt = FMT_I;
        break;
    case OPC_JAL:
        d.fmt = FMT_J;
        break;
    case OPC_SYSTEM:
        d.fmt = FMT_I;
        break;
};

switch(d.fmt){
    case FMT_R:
        switch(funct3){
            case 0x0:
                if(funct7 == 0x00) d.op = OP_ADD;
                else if(funct7 == 0x20) d.op = OP_SUB;
                else if(funct7 == 0x01) d.op = OP_MUL;
                break;
            case 0x4:
                if(funct7 == 0x00) d.op = OP_XOR;
                else if(funct7 == 0x01) d.op = OP_DIV;
                break;
            case 0x6:
                if(funct7 == 0x00) d.op = OP_OR;
                else if(funct7 == 0x01) d.op = OP_REM;
                break;
            case 0x7:
                if(funct7 == 0x00) d.op = OP_AND;
                else if(funct7 == 0x01) d.op = OP_REMU;
                break;
            case 0x1:
                if(funct7 == 0x00) d.op = OP_SLL;
                else if(funct7 == 0x01) d.op = OP_MULH;
                break;
            case 0x5:
                if(funct7 == 0x00) d.op = OP_SRL;
                else if(funct7 == 0x20) d.op = OP_SRA;
                else if(funct7 == 0x01) d.op = OP_DIVU;
                break;
            case 0x2:
                if(funct7 == 0x00) d.op = OP_SLT;
                else if(funct7 == 0x01) d.op = OP_MULHSU;
                break;
            case 0x3:
                if(funct7 == 0x00) d.op = OP_SLTU;
                else if(funct7 == 0x01) d.op = OP_MULHU;
                break;
        }
        break;
    case FMT_I:
        if(opcode == OPC_OPIMM){
            switch(funct3){
                case 0x0:
                    d.op = OP_ADDI;
                    break;
                case 0x1:
                    if(funct7 == 0x00) d.op = OP_SLLI;
                    break;
                case 0x2:
                    d.op = OP_SLTI;
                    break;
                case 0x3:
                    d.op = OP_SLTIU;
                    break;
                case 0x4:
                    d.op = OP_XORI;
                    break;
                case 0x5:
                    if(funct7 == 0x00) d.op = OP_SRLI;
                    else if(funct7 == 0x20) d.op = OP_SRAI;
                    break;
                case 0x6:
                    d.op = OP_ORI;
                    break;
                case 0x7:
                    d.op = OP_ANDI;
                    break;
            }
        } else if(opcode == OPC_LOAD){
            if(funct3 == 0x0) d.op = OP_LB;
            else if(funct3 == 0x1) d.op = OP_LH;
            else if(funct3 == 0x2) d.op = OP_LW;
            else if(funct3 == 0x4) d.op = OP_LBU;
            else if(funct3 == 0x5) d.op = OP_LHU;
        } else if(opcode == OPC_SYSTEM){
            if(funct7 == 0 && funct3 == 0 && rs2 == 0) d.op = OP_ECALL;
            else if(funct7 == 0 && funct3 == 0 && rs2 == 1) d.op = OP_EBREAK;
            
        } else if(opcode == OPC_JALR) d.op = OP_JALR;
        break;
    case FMT_S:
        if(funct3 == 0x0) d.op = OP_SB;
        else if(funct3 == 0x1) d.op = OP_SH;
        else if(funct3 == 0x2) d.op = OP_SW;
        break;
    case FMT_B:
        if(funct3 == 0x0) d.op = OP_BEQ;
        else if(funct3 == 0x1) d.op = OP_BNE;
        else if(funct3 == 0x4) d.op = OP_BLT;
        else if(funct3 == 0x5) d.op = OP_BGE;
        else if(funct3 == 0x6) d.op = OP_BLTU;
        else if(funct3 == 0x7) d.op = OP_BGEU;
        break;
    case FMT_U:
        if(opcode == OPC_LUI) d.op = OP_LUI;
        else if(opcode == OPC_AUIPC) d.op = OP_AUIPC;
        break;
    case FMT_J:
        d.op = OP_JAL;
        break;
};

if(writes_rd(d.op)) d.rd = rd;
if(reads_rs1(d.op)) d.rs1 = rs1;
if(reads_rs2(d.op)) d.rs2 = rs2;

switch(d.fmt){
    case FMT_I:
        if(d.op == OP_SLLI || d.op == OP_SRLI || d.op == OP_SRAI) d.imm = rs2;
        else if(opcode == OPC_SYSTEM) d.imm = 0;
        else d.imm = imm_i(raw);
        break;
    case FMT_S:
        d.imm = imm_s(raw);
        break;
    case FMT_B:
        d.imm = imm_b(raw);
        break;
    case FMT_U:
        d.imm = imm_u(raw);
        break;
    case FMT_J:
        d.imm = imm_j(raw);
        break;
};

d.pc = pc;
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

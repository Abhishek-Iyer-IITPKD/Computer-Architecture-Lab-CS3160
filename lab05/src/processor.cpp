#include "processor.h"

#include <cstring>

#include "alu.h"
#include "disasm.h"
#include "log.h"

void processor_init(Processor &cpu, const Config &cfg, Stats &st,
                    MemoryLevel &imem, MemoryLevel &dmem) {
    cpu.cfg   = &cfg;
    cpu.stats = &st;
    cpu.imem  = &imem;
    cpu.dmem  = &dmem;
    cpu.pc    = cfg.start;
    memset(cpu.regs, 0, sizeof(cpu.regs));
    cpu.halted      = false;
    cpu.exit_status = 0;
}

void processor_run(Processor &cpu) {
    switch (cpu.cfg->proc) {
        case PROC_SINGLE:     run_single(cpu); break;
        // The pipelined models arrive in weeks 7-9. Until then this
        // simulator has one machine in it, and asking for another should
        // say so rather than quietly running the wrong one.
        case PROC_PIPELINED:
        case PROC_FORWARDING:
            fatal("--proc=%s: this simulator has no pipeline yet; that is "
                  "weeks 7 to 9. Use --proc=single.",
                  proc_kind_name(cpu.cfg->proc));
    }

    Stats &st = *cpu.stats;
    st.exited      = cpu.halted;
    st.exit_status = cpu.exit_status;

    if (cpu.halted) {
        log_info("program exited with status %d after %ld instructions, %ld cycles",
                 cpu.exit_status, st.instructions, st.cycles);
    } else {
        log_error("instruction budget of %ld exhausted without the program "
                  "reaching _exit; the trace is incomplete",
                  cpu.cfg->num_insts);
    }
}

// --- Register file ---------------------------------------------------------

u32 reg_read(const Processor &cpu, int n) {
    if (n == 0) {
        return 0;
    }
    return cpu.regs[n];
}

void reg_write(Processor &cpu, int n, u32 value) {
    if (n == 0) {
        return;  // x0 is hardwired to zero
    }
    cpu.regs[n] = value;
}

// --- Stages ----------------------------------------------------------------

Fetched stage_fetch(Processor &cpu, u32 pc) {
    Fetched f;
    f.pc      = pc;
    f.latency = 0;
    f.raw     = cpu.imem->read(pc, 4, &f.latency);
    cpu.stats->fetches++;

    if (log_debug_enabled()) {
        log_debug("IF  0x%08x: %08x  %s", pc, f.raw, disasm(f.raw, pc).c_str());
    }
    return f;
}

void stage_operands(const Processor &cpu, const Decoded &d, u32 *v_rs1, u32 *v_rs2) {
    *v_rs1 = reads_rs1(d.op) ? reg_read(cpu, d.rs1) : 0;
    *v_rs2 = reads_rs2(d.op) ? reg_read(cpu, d.rs2) : 0;
}

Executed stage_execute(const Decoded &d, u32 v_rs1, u32 v_rs2) {
    // TODO(week 5): the execute stage.
    //
    // Build an Executed for this instruction: `result` is what the ALU
    // produced, and `taken` says whether a branch is going to its target.
    // alu_inputs() picks the two operands, alu_apply() combines them, and
    // branch_taken() answers the second question -- all three are in alu.h,
    // and only branches have an opinion about `taken`.
    AluInputs in;
    in = alu_inputs(d, v_rs1, v_rs2);
    Executed e;
    e.result = alu_apply(d.op, in.a, in.b);
    e.taken  = branch_taken(d.op, in.a, in.b);
    return e;
}

u32 next_pc_of(const Decoded &d, const Executed &e) {
    // TODO(week 5): where does control go next?
    //
    // Three answers, in order: a jump goes wherever the ALU computed (the
    // target is `e.result`, which is why jal and jalr are additions); a
    // branch that is taken goes to its own pc plus its immediate; everything
    // else goes to the next instruction. is_jump() and is_branch() are in
    // decode.h.
    if(is_jump(d.op)) return e.result;
    if(is_branch(d.op) && e.taken) return d.pc + d.imm;
    return d.pc + 4;
}

bool redirects_pc(const Decoded &d, const Executed &e) {
    if (is_jump(d.op)) {
        return true;  // unconditional: a jump goes somewhere by definition
    }
    return is_branch(d.op) && e.taken;
}

MemAccess stage_memory(Processor &cpu, const Decoded &d, u32 addr, u32 v_rs2) {
    // TODO(week 5): the memory stage.
    //
    // Loads and stores, and nothing else, touch memory -- for every other
    // instruction this stage does nothing and returns a MemAccess with both
    // flags false. `addr` has already been computed for you by the ALU.
    //
    // Three things to get right:
    //   - the width. mem_size() gives 1, 2 or 4 bytes; cpu.dmem->read() and
    //     ->write() take it, and also fill in the latency for you.
    //   - the sign. A load of less than a word arrives in the low bits, and
    //     lb/lh fill the rest with the sign while lbu/lhu fill it with
    //     zeroes. mem_signed() says which, sign_extend() does it.
    //   - how the program stops. crt.S ends by storing to the address in
    //     cpu.cfg->tohost. When that store happens, set cpu.halted and put
    //     the status in cpu.exit_status -- see the note in crt.S for the
    //     shift. Miss this and your simulator never terminates.
    //
    // Count each load and each store in cpu.stats->data_accesses.
    MemAccess m;
    m.did_read  = false;
    m.did_write = false;
    m.addr      = addr;
    m.value     = 0;
    m.latency   = 0;
    if(is_load(d.op)){
        m.did_read = true;
        switch(d.op){
            case OP_LB: case OP_LH:
                m.value = sign_extend(cpu.dmem->read(addr, mem_size(d.op), &m.latency), mem_size(d.op)*8);
                break;
            case OP_LBU: case OP_LHU: case OP_LW:
                m.value = cpu.dmem->read(addr, mem_size(d.op), &m.latency);
                break;
        }
        cpu.stats->data_accesses += 1;
    }
    if(is_store(d.op)){
        m.did_write = true;
        u32 size = mem_size(d.op);
        u32 mask = (size == 1) ? 0xffu :
                   (size == 2) ? 0xffffu :
                                  0xffffffffu;
        m.value = v_rs2 & mask;
        cpu.dmem->write(addr, v_rs2, size, &m.latency);
        cpu.stats->data_accesses += 1;
        if(addr == cpu.cfg->tohost){
            cpu.halted = true;
            cpu.exit_status = v_rs2 >> 1;
        }
    }
    return m;
}

// One line per retired instruction, in the format the pr5 simulator used and
// the lab harnesses parse. Four fields, always in the same order, so that two
// traces can be compared line by line:
//
//   <pc> | next_pc = <pc> | x<rd> = <value> | mem[<addr>] <op> <value>
//
// A field an instruction has nothing to say about is filled with `?`, which
// keeps every line the same shape. `=>` marks a load, `<=` a store.
static void trace_retire(const Processor &cpu, const Decoded &d, u32 next_pc,
                         const MemAccess &m) {
    char reg_field[32];
    if (writes_rd(d.op)) {
        // The value read back from the register file, so that a write to x0
        // shows the zero that actually landed there.
        snprintf(reg_field, sizeof(reg_field), "x%d = %08x", d.rd,
                 reg_read(cpu, d.rd));
    } else {
        snprintf(reg_field, sizeof(reg_field), "x? = %08x", 0);
    }

    char mem_field[40];
    if (m.did_read) {
        snprintf(mem_field, sizeof(mem_field), "mem[%08x] => %08x", m.addr, m.value);
    } else if (m.did_write) {
        snprintf(mem_field, sizeof(mem_field), "mem[%08x] <= %08x", m.addr, m.value);
    } else {
        snprintf(mem_field, sizeof(mem_field), "mem[?] = %08x", 0);
    }

    log_out("%08x | next_pc = %08x | %s | %s", d.pc, next_pc, reg_field, mem_field);
}

u32 writeback_value(const Decoded &d, u32 result, const MemAccess &m) {
    if (is_load(d.op)) {
        return m.value;  // the data it read, not the address it computed
    }
    if (is_jump(d.op)) {
        // jal and jalr write the return address, not the target: the ALU
        // result went to the PC instead.
        return d.pc + 4;
    }
    return result;
}

void stage_writeback(Processor &cpu, const Decoded &d, u32 result,
                     const MemAccess &m, u32 next_pc) {
// TODO(week 5): the writeback stage, and the statistics.
//
// Two jobs. First, if this instruction writes a register, write it:
// writes_rd() says whether it does, writeback_value() picks between the
// ALU result and what a load brought back, and reg_write() is the one
// that knows x0 stays zero.
//
// Second, count it. Every instruction that gets here has retired, so
// cpu.stats->instructions goes up; loads, stores, jumps and branches
// each have their own counter. `taken_branches` is the interesting one:
// this stage cannot see the branch's verdict, so infer it from where
// control actually went -- next_pc != pc + 4. That inference is wrong
// for exactly one encoding, which is the subject of the handout on
// reading stats.json, and it is left wrong on purpose.
//
// Finish with trace_retire(), which writes the [OUT] line the tests
// compare against.
if(writes_rd(d.op)) reg_write(cpu, d.rd, writeback_value(d, result, m));
cpu.stats->instructions += 1;
if(is_load(d.op)) cpu.stats->loads += 1;
if(is_store(d.op)) cpu.stats->stores += 1;
if(is_branch(d.op)) cpu.stats->branches += 1;
if(next_pc != d.pc + 4 && is_branch(d.op)) cpu.stats->taken_branches += 1;
if(is_jump(d.op)) cpu.stats->jumps += 1;
trace_retire(cpu, d, next_pc, m);
}

// --- Shared bits of the run loops -----------------------------------------

void processor_illegal(const Processor &cpu, const Decoded &d) {
    fatal("illegal instruction 0x%08x at 0x%08x, after %ld instructions. "
          "Either the program jumped somewhere that is not code, or it uses an "
          "instruction this simulator does not implement.",
          d.raw, d.pc, cpu.stats->instructions);
}

int cycle_cost_serial(int fetch_latency, int mem_latency) {
    int cost = fetch_latency + mem_latency;
    return (cost < 1) ? 1 : cost;
}

int cycle_cost(const Processor &cpu, int fetch_latency, int mem_latency) {
    if (cpu.cfg->latency_overlap != OVERLAP_MAX) {
        return cycle_cost_serial(fetch_latency, mem_latency);
    }
    int cost = (fetch_latency > mem_latency) ? fetch_latency : mem_latency;
    return (cost < 1) ? 1 : cost;
}

Latch latch_bubble(void) {
    Latch l;
    memset(&l, 0, sizeof(l));
    l.valid = false;
    l.d     = decode_none();
    return l;
}

Latch latch_from_fetch(const Fetched &f) {
    Latch l = latch_bubble();
    l.valid = true;
    l.d.raw = f.raw;
    l.d.pc  = f.pc;
    return l;
}

Latch latch_from_id(const Decoded &d, u32 v_rs1, u32 v_rs2) {
    Latch l = latch_bubble();
    l.valid = true;
    l.d     = d;
    l.v_rs1 = v_rs1;
    l.v_rs2 = v_rs2;
    return l;
}

// One stage's worth of the pipeline dump: the PC in it, or a dash.
static void stage_field(char *buf, size_t n, const Latch &l) {
    if (l.valid) {
        snprintf(buf, n, "%08x", l.d.pc);
    } else {
        snprintf(buf, n, "--------");
    }
}

void pipeline_dump(long cycle, const Latch &if_id, const Latch &id_ex,
                   const Latch &ex_mem, const Latch &mem_wb) {
    if (!log_debug_enabled()) {
        return;
    }
    char a[16], b[16], c[16], e[16];
    stage_field(a, sizeof(a), if_id);
    stage_field(b, sizeof(b), id_ex);
    stage_field(c, sizeof(c), ex_mem);
    stage_field(e, sizeof(e), mem_wb);
    log_debug("cycle %-6ld IF/ID %s  ID/EX %s  EX/MEM %s  MEM/WB %s",
              cycle, a, b, c, e);
}

void processor_dump(const Processor &cpu) {
    if (!log_debug_enabled()) {
        return;
    }
    log_debug("pc = 0x%08x", cpu.pc);
    for (int i = 0; i < 32; i += 4) {
        log_debug("  %-4s %08x  %-4s %08x  %-4s %08x  %-4s %08x",
                  reg_name(i), cpu.regs[i], reg_name(i + 1), cpu.regs[i + 1],
                  reg_name(i + 2), cpu.regs[i + 2], reg_name(i + 3), cpu.regs[i + 3]);
    }
}

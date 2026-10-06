#include "processor.h"

#include <cstring>

#include "alu.h"
#include "csr.h"
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
    csr_init(cpu.csrs);
}

// Which models this build actually has.
// One function per model rather than two cases in the switch below.
// Asking for a model that is not built yet has to say so. 
static void dispatch_pipelined(Processor &cpu) {
    (void)cpu;
    fatal("--proc=pipelined: this simulator has no pipeline yet.");
}

static void dispatch_forwarding(Processor &cpu) {
    (void)cpu;
    fatal("--proc=forwarding: this simulator has no forwarding pipeline "
          "yet.");
}

void processor_run(Processor &cpu) {
    switch (cpu.cfg->proc) {
        case PROC_SINGLE:     run_single(cpu); break;
        case PROC_PIPELINED:  dispatch_pipelined(cpu); break;
        case PROC_FORWARDING: dispatch_forwarding(cpu); break;
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

// --- The control and status registers --------------------------------------

CsrAccess csr_access_none(void) {
    CsrAccess c;
    c.did_read  = false;
    c.did_write = false;
    c.num       = 0;
    c.value     = 0;
    c.next      = 0;
    c.written   = 0;
    return c;
}

CsrAccess csr_read(Processor &cpu, const Decoded &d, u32 v_rs1) {
    CsrAccess c = csr_access_none();
    if (!is_csr(d.op)) {
        return c;   // nearly every instruction: there is no CSR to read
    }
    // TODO(midsem, part 2): the read half of a CSR instruction.
    //
    // This runs beside the read of x[rs1], in the same stage, and fills in:
    //
    //   num       which CSR this instruction names
    //   value     what it holds now -- and this is what x[rd] receives
    //   next      what it is to be given, which is where the three
    //             instructions differ:
    //                 csrrw   v_rs1
    //                 csrrs   the old value with v_rs1's 1 bits set
    //                 csrrc   the old value with v_rs1's 1 bits cleared
    //   written   what it will hold afterwards, as far as this half knows,
    //             which is the value it just read
    //   did_read  the read happened
    //
    // csr_index() says which slot of cpu.csrs holds it, and -1 if this
    // simulator has not got that register -- which is an error naming the
    // instruction that asked for it, not a quiet zero.
    //
    // Count the read in cpu.stats->csr_reads.
    c.num = d.csr;
    c.value = cpu.csrs.word[csr_index(c.num)];
    if(d.op == OP_CSRRW) c.next = v_rs1;
    else if(d.op == OP_CSRRS) c.next = c.value | v_rs1;
    else if(d.op == OP_CSRRC) c.next = c.value & ~v_rs1;
    c.written = c.value;
    c.did_read = true;
    if(c.did_read) cpu.stats->csr_reads += 1;
    return c;
}

void csr_write(Processor &cpu, const Decoded &d, CsrAccess &c) {
    if (!is_csr(d.op)) {
        return;   // nearly every instruction: there is no CSR to write
    }
    // TODO(midsem, part 2): the write half of a CSR instruction.
    //
    // This runs beside the write of x[rd], in the same stage, and stores the
    // value the read half worked out -- c.next.
    //
    // Two rules, and the first is the one that is easy to miss: not every
    // CSR instruction writes at all. The handout says which do not, and it
    // is a rule about rs1 rather than about rd.
    //
    // The second is the read-only register, which csr_writable() knows
    // about. Its write is discarded rather than refused: the instruction
    // still counts as having written, and the register goes on reading 0.
    //
    // Set c.did_write when the write half happened, leave c.written holding
    // what the register says afterwards -- read it back rather than assuming
    // it took c.next -- and count it in cpu.stats->csr_writes.
    if((d.op == OP_CSRRS || d.op == OP_CSRRC) && d.rs1 == 0) c.did_write = false;
    else c.did_write = true;
    if(c.num == CSR_MHARTID) c.next = c.value;
    if(c.did_write) cpu.csrs.word[csr_index(c.num)] = c.next;
    c.written = cpu.csrs.word[csr_index(c.num)];
    if(c.did_write) cpu.stats->csr_writes += 1;
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

void stage_operands(Processor &cpu, const Decoded &d, u32 *v_rs1, u32 *v_rs2,
                    CsrAccess *c) {
    *v_rs1 = reads_rs1(d.op) ? reg_read(cpu, d.rs1) : 0;
    *v_rs2 = reads_rs2(d.op) ? reg_read(cpu, d.rs2) : 0;

    // The second register file, read in the same stage as the first. It does
    // nothing at all for the instructions that name no CSR, which is nearly
    // all of them.
    *c = csr_read(cpu, d, *v_rs1);
}

Executed stage_execute(const Decoded &d, u32 v_rs1, u32 v_rs2) {
    AluInputs in = alu_inputs(d, v_rs1, v_rs2);
    Executed  e;
    e.result = alu_apply(d.op, in.a, in.b);
    e.taken  = is_branch(d.op) && branch_taken(d.op, in.a, in.b);

    if (log_debug_enabled()) {
        log_debug("EX  0x%08x: %-24s a=0x%08x b=0x%08x -> 0x%08x%s",
                  d.pc, disasm(d).c_str(), in.a, in.b, e.result,
                  is_branch(d.op) ? (e.taken ? " taken" : " not taken") : "");
    }
    return e;
}

u32 next_pc_of(const Decoded &d, const Executed &e) {
    if (is_jump(d.op)) {
        return e.result;  // the ALU computed the target
    }
    if (is_branch(d.op) && e.taken) {
        return d.pc + d.imm;
    }
    return d.pc + 4;
}

bool redirects_pc(const Decoded &d, const Executed &e) {
    if (is_jump(d.op)) {
        return true;  // unconditional: a jump goes somewhere by definition
    }
    return is_branch(d.op) && e.taken;
}

MemAccess stage_memory(Processor &cpu, const Decoded &d, u32 addr, u32 v_rs2) {
    MemAccess m;
    m.did_read  = false;
    m.did_write = false;
    m.addr      = addr;
    m.value     = 0;
    m.latency   = 0;

    int size = mem_size(d.op);

    if (is_load(d.op)) {
        m.did_read = true;
        u32 raw = cpu.dmem->read(addr, size, &m.latency);
        // A load of less than a word arrives in the low bits; the instruction
        // decides whether the rest is filled with the sign or with zeroes.
        m.value = mem_signed(d.op) ? sign_extend(raw, size * 8) : raw;
        cpu.stats->data_accesses++;
        log_debug("MEM 0x%08x: load  %d byte(s) from 0x%08x -> 0x%08x",
                  d.pc, size, addr, m.value);
    } else if (is_store(d.op)) {
        m.did_write = true;
        // Only the low bytes are written, so only they are reported.
        m.value = (size == 4) ? v_rs2 : (v_rs2 & ((1u << (size * 8)) - 1u));
        cpu.dmem->write(addr, v_rs2, size, &m.latency);
        cpu.stats->data_accesses++;
        log_debug("MEM 0x%08x: store %d byte(s) of 0x%08x to 0x%08x",
                  d.pc, size, m.value, addr);

        // How the program ends. crt.S's _exit stores (status << 1) | 1 to the
        // tohost word; the shift is there so that a status of 0 is still a
        // non-zero word, which is how the host tells "exited with 0" from
        // "has not exited". Recovering the status is that shift undone --
        // arithmetically, so that a negative status survives.
        if (addr == cpu.cfg->tohost) {
            cpu.halted      = true;
            cpu.exit_status = as_signed(v_rs2) >> 1;
            log_debug("MEM 0x%08x: store to tohost: exit status %d",
                      d.pc, cpu.exit_status);
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
                         const MemAccess &m, const CsrAccess &c) {
    char reg_field[32];
    if (writes_rd(d.op)) {
        // The value read back from the register file, so that a write to x0
        // shows the zero that actually landed there.
        snprintf(reg_field, sizeof(reg_field), "x%d = %08x", d.rd,
                 reg_read(cpu, d.rd));
    } else {
        snprintf(reg_field, sizeof(reg_field), "x? = %08x", 0);
    }

    char mem_field[48];
    if (is_csr(d.op)) {
        // No memory was touched, so neither `mem[a] => v` nor `mem[a] <= v`
        // says anything true here and a CSR gets a field of its own. The two
        // shapes are the whole point of it: an instruction that only read the
        // register, and one that read it and then wrote it. `csrrs rd, csr, x0`
        // must produce the first.
        if (c.did_write) {
            snprintf(mem_field, sizeof(mem_field), "csr[%03x] %08x <= %08x",
                     c.num, c.value, c.written);
        } else {
            snprintf(mem_field, sizeof(mem_field), "csr[%03x] %08x (read only)",
                     c.num, c.value);
        }
    } else if (m.did_read) {
        snprintf(mem_field, sizeof(mem_field), "mem[%08x] => %08x", m.addr, m.value);
    } else if (m.did_write) {
        snprintf(mem_field, sizeof(mem_field), "mem[%08x] <= %08x", m.addr, m.value);
    } else {
        snprintf(mem_field, sizeof(mem_field), "mem[?] = %08x", 0);
    }

    log_out("%08x | next_pc = %08x | %s | %s", d.pc, next_pc, reg_field, mem_field);
}

u32 writeback_value(const Decoded &d, u32 result, const MemAccess &m,
                    const CsrAccess &c) {
    // TODO(midsem, part 2): what does a CSR instruction write to rd?
    //
    // All three are read-then-modify, and the answer is not the value the
    // register ends up holding. Both are in the CsrAccess that the read half
    // filled in; the handout says which field is which.
    if (is_csr(d.op)) return c.value;
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
                     const MemAccess &m, CsrAccess &c, u32 next_pc) {
    if (writes_rd(d.op)) {
        reg_write(cpu, d.rd, writeback_value(d, result, m, c));
    }

    // The second register file, written in the same stage as the first. It
    // fills in what the CSR ended up holding, which the trace line below
    // reports -- so it has to happen before that line is emitted.
    csr_write(cpu, d, c);

    Stats &st = *cpu.stats;
    st.instructions++;
    if (is_load(d.op))  st.loads++;
    if (is_store(d.op)) st.stores++;
    if (is_jump(d.op))  st.jumps++;
    if (is_branch(d.op)) {
        
        st.branches++;

        // Inferred, not measured: this stage has no Executed to consult, so it
        // works out "taken" from where the next instruction came from. That is
        // right for every branch a compiler emits and wrong for exactly one --
        // a branch whose offset is +4, whose target therefore *is* pc + 4. The
        // pipeline redirects and flushes for it; this counts it as not taken.
        if (next_pc != d.pc + 4) {
            st.taken_branches++;
        }
    }

    trace_retire(cpu, d, next_pc, m, c);
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

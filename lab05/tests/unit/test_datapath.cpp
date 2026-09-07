// The five stages, each called on its own.
//
// This is what the stage functions being free functions buys: a test can hand
// stage_execute() a Decoded it made up and look at what comes back, without
// building a machine or running a program. When one of these fails it names the
// stage, which a trace mismatch three hundred instructions into a program does
// not.
//
// Run them with `make test`. Work down the file in order -- it is the order the
// stages run in, and the order the handout introduces them.

#include "check.h"

#include "alu.h"
#include "processor.h"
#include "ram.h"

static const u32 BASE   = 0x80000000u;
static const u32 SIZE   = 0x10000u;
static const u32 TOHOST = 0x80001000u;

// A machine with nothing but RAM under it, which is what week 5 runs on.
struct Bench {
    Config    cfg;
    Stats     st;
    Ram       ram;
    Processor cpu;

    Bench() {
        config_defaults(cfg);
        cfg.tohost = TOHOST;
        stats_init(st);
        ram_init(ram, BASE, SIZE, 0);
        processor_init(cpu, cfg, st, ram, ram);
    }
};

// A Decoded built by hand, so a stage can be tested on an instruction that no
// program has to contain.
static Decoded inst(Op op, int rd, int rs1, int rs2, u32 imm, u32 pc) {
    Decoded d = decode_none();
    d.op = op; d.rd = rd; d.rs1 = rs1; d.rs2 = rs2; d.imm = imm; d.pc = pc;
    return d;
}

static MemAccess no_access(void) {
    MemAccess m;
    m.did_read = false;
    m.did_write = false;
    m.addr = 0;
    m.value = 0;
    m.latency = 0;
    return m;
}

// --- stage_execute ---------------------------------------------------------

TEST(execute_computes_an_alu_result) {
    Executed e = stage_execute(inst(OP_ADD, 5, 1, 2, 0, BASE), 7, 35);
    CHECK_EQ(e.result, 42u);
}

TEST(execute_uses_the_immediate_for_an_i_type) {
    // The second operand comes from the instruction, not from rs2 -- so the
    // 99 below has to be ignored.
    Executed e = stage_execute(inst(OP_ADDI, 5, 1, 0, 10, BASE), 32, 99);
    CHECK_EQ(e.result, 42u);
}

TEST(execute_reports_whether_a_branch_is_taken) {
    CHECK(stage_execute(inst(OP_BEQ, 0, 1, 2, 16, BASE), 5, 5).taken);
    CHECK(!stage_execute(inst(OP_BEQ, 0, 1, 2, 16, BASE), 5, 6).taken);
    CHECK(stage_execute(inst(OP_BNE, 0, 1, 2, 16, BASE), 5, 6).taken);
}

TEST(execute_leaves_taken_false_for_things_that_are_not_branches) {
    // An add whose operands happen to be equal is not a taken beq.
    CHECK(!stage_execute(inst(OP_ADD, 5, 1, 2, 0, BASE), 5, 5).taken);
    CHECK(!stage_execute(inst(OP_JAL, 1, 0, 0, 16, BASE), 0, 0).taken);
}

TEST(execute_computes_an_address_for_a_load_or_store) {
    // Address arithmetic is the same addition as everything else: base + offset.
    CHECK_EQ(stage_execute(inst(OP_LW, 5, 1, 0, 12, BASE), BASE, 0).result,
             BASE + 12);
    CHECK_EQ(stage_execute(inst(OP_SW, 0, 1, 2, 12, BASE), BASE, 0).result,
             BASE + 12);
    // A negative offset arrives already sign-extended from decode.
    CHECK_EQ(stage_execute(inst(OP_LW, 5, 1, 0, as_unsigned(-8), BASE),
                           BASE + 16, 0).result, BASE + 8);
}

// --- next_pc_of, and redirects_pc ------------------------------------------

TEST(next_pc_is_the_following_instruction_by_default) {
    Decoded  d = inst(OP_ADD, 5, 1, 2, 0, BASE);
    Executed e = stage_execute(d, 1, 2);
    CHECK_EQ(next_pc_of(d, e), BASE + 4);
}

TEST(next_pc_follows_a_taken_branch_and_not_an_untaken_one) {
    Decoded d = inst(OP_BEQ, 0, 1, 2, 16, BASE);
    CHECK_EQ(next_pc_of(d, stage_execute(d, 5, 5)), BASE + 16);
    CHECK_EQ(next_pc_of(d, stage_execute(d, 5, 6)), BASE + 4);
}

TEST(a_branch_target_is_relative_to_the_branch) {
    // pc + imm, not next_pc + imm: the offset is measured from the branch
    // itself, which is why a backward branch has a negative immediate.
    Decoded d = inst(OP_BNE, 0, 1, 2, as_unsigned(-8), BASE + 32);
    CHECK_EQ(next_pc_of(d, stage_execute(d, 1, 2)), BASE + 24);
}

TEST(a_jump_goes_where_the_alu_computed) {
    Decoded  d = inst(OP_JALR, 1, 1, 0, 4, BASE);
    Executed e = stage_execute(d, BASE + 64, 0);
    CHECK_EQ(next_pc_of(d, e), BASE + 68);
}

TEST(redirects_pc_agrees_with_next_pc_of) {
    Decoded taken = inst(OP_BEQ, 0, 1, 2, 16, BASE);
    CHECK(redirects_pc(taken, stage_execute(taken, 5, 5)));
    CHECK(!redirects_pc(taken, stage_execute(taken, 5, 6)));

    Decoded jump = inst(OP_JAL, 1, 0, 0, 16, BASE);
    CHECK(redirects_pc(jump, stage_execute(jump, 0, 0)));

    Decoded add = inst(OP_ADD, 5, 1, 2, 0, BASE);
    CHECK(!redirects_pc(add, stage_execute(add, 1, 2)));
}

// --- stage_memory ----------------------------------------------------------

TEST(memory_does_nothing_for_an_instruction_that_touches_no_memory) {
    Bench b;
    MemAccess m = stage_memory(b.cpu, inst(OP_ADD, 5, 1, 2, 0, BASE), BASE, 0);
    CHECK(!m.did_read);
    CHECK(!m.did_write);
    CHECK_EQ(b.st.data_accesses, 0l);
}

TEST(memory_stores_and_loads_a_word) {
    Bench b;
    stage_memory(b.cpu, inst(OP_SW, 0, 1, 2, 0, BASE), BASE + 8, 0xDEADBEEFu);
    MemAccess m = stage_memory(b.cpu, inst(OP_LW, 5, 1, 0, 0, BASE), BASE + 8, 0);
    CHECK(m.did_read);
    CHECK_EQ(m.value, 0xDEADBEEFu);
    CHECK_EQ(b.st.data_accesses, 2l);
}

TEST(memory_writes_only_as_many_bytes_as_the_instruction_says) {
    Bench b;
    stage_memory(b.cpu, inst(OP_SW, 0, 1, 2, 0, BASE), BASE + 8, 0xFFFFFFFFu);
    stage_memory(b.cpu, inst(OP_SB, 0, 1, 2, 0, BASE), BASE + 8, 0x00u);
    // Only the byte at BASE+8 was cleared; the three above it are untouched.
    CHECK_EQ(ram_read(b.ram, BASE + 8, 4), 0xFFFFFF00u);
}

TEST(memory_sign_extends_lb_and_lh_but_not_lbu_and_lhu) {
    Bench b;
    stage_memory(b.cpu, inst(OP_SW, 0, 1, 2, 0, BASE), BASE + 8, 0x000080FFu);

    CHECK_EQ(stage_memory(b.cpu, inst(OP_LB, 5, 1, 0, 0, BASE), BASE + 8, 0).value,
             as_unsigned(-1));          // 0xFF read as a signed byte
    CHECK_EQ(stage_memory(b.cpu, inst(OP_LBU, 5, 1, 0, 0, BASE), BASE + 8, 0).value,
             0x000000FFu);
    CHECK_EQ(stage_memory(b.cpu, inst(OP_LH, 5, 1, 0, 0, BASE), BASE + 8, 0).value,
             as_unsigned(-32513));      // 0x80FF read as a signed half
    CHECK_EQ(stage_memory(b.cpu, inst(OP_LHU, 5, 1, 0, 0, BASE), BASE + 8, 0).value,
             0x000080FFu);
}

TEST(memory_halts_the_machine_on_a_store_to_tohost) {
    Bench b;
    CHECK(!b.cpu.halted);
    // crt.S writes (status << 1) | 1, so 0x09 means an exit status of 4.
    stage_memory(b.cpu, inst(OP_SW, 0, 1, 2, 0, BASE), TOHOST, 0x09u);
    CHECK(b.cpu.halted);
    CHECK_EQ(b.cpu.exit_status, 4);
}

// --- stage_writeback -------------------------------------------------------

TEST(writeback_writes_the_alu_result_to_rd) {
    Bench b;
    stage_writeback(b.cpu, inst(OP_ADD, 5, 1, 2, 0, BASE), 42, no_access(),
                    BASE + 4);
    CHECK_EQ(reg_read(b.cpu, 5), 42u);
    CHECK_EQ(b.st.instructions, 1l);
}

TEST(writeback_writes_what_a_load_brought_back_not_the_address) {
    Bench b;
    MemAccess m = no_access();
    m.did_read = true;
    m.addr     = BASE + 8;
    m.value    = 0x1234u;

    stage_writeback(b.cpu, inst(OP_LW, 5, 1, 0, 0, BASE), BASE + 8, m, BASE + 4);
    CHECK_EQ(reg_read(b.cpu, 5), 0x1234u);
}

TEST(writeback_never_changes_x0) {
    Bench b;
    // addi zero, zero, 7 -- which is how the assembler spells some nops.
    stage_writeback(b.cpu, inst(OP_ADDI, 0, 0, 0, 7, BASE), 7, no_access(),
                    BASE + 4);
    CHECK_EQ(reg_read(b.cpu, 0), 0u);
}

TEST(writeback_counts_each_kind_of_instruction) {
    Bench b;
    stage_writeback(b.cpu, inst(OP_ADD, 5, 1, 2, 0, BASE), 0, no_access(), BASE + 4);
    stage_writeback(b.cpu, inst(OP_LW,  5, 1, 0, 0, BASE), 0, no_access(), BASE + 4);
    stage_writeback(b.cpu, inst(OP_SW,  0, 1, 2, 0, BASE), 0, no_access(), BASE + 4);
    stage_writeback(b.cpu, inst(OP_JAL, 1, 0, 0, 8, BASE), 0, no_access(), BASE + 8);

    CHECK_EQ(b.st.instructions, 4l);
    CHECK_EQ(b.st.loads, 1l);
    CHECK_EQ(b.st.stores, 1l);
    CHECK_EQ(b.st.jumps, 1l);
}

TEST(writeback_infers_a_taken_branch_from_where_control_went) {
    Bench    b;
    Decoded br = inst(OP_BEQ, 0, 1, 2, 16, BASE);

    stage_writeback(b.cpu, br, 0, no_access(), BASE + 16);  // went to the target
    stage_writeback(b.cpu, br, 0, no_access(), BASE + 4);   // fell through

    CHECK_EQ(b.st.branches, 2l);
    CHECK_EQ(b.st.taken_branches, 1l);
}

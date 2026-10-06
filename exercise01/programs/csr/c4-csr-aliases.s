# The names the assembler actually accepts.
#
# Almost nobody writes `csrrs t0, mepc, zero`. The assembler provides shorter
# spellings for the common cases, and they assemble to exactly the three
# instructions being implemented -- so a simulator that gets those three right
# gets all of these right, and one that special-cases the spelling does not.
#
#   csrr  rd, csr        =  csrrs rd, csr, x0      read
#   csrw  csr, rs        =  csrrw x0, csr, rs      write
#   csrs  csr, rs        =  csrrs x0, csr, rs      set bits
#   csrc  csr, rs        =  csrrc x0, csr, rs      clear bits
#
# Disassemble this file with `-M no-aliases` to see the three underneath.

    .section .text
    .globl main
main:
    la   s0, out

    li   t0, 0x01020304
    csrw mepc, t0               # csrrw x0, mepc, t0
    csrr t1, mepc               # csrrs t1, mepc, x0
    sw   t1, 0(s0)              # 0x01020304

    li   t0, 0x00000F00
    csrs mepc, t0               # csrrs x0, mepc, t0
    csrr t1, mepc
    sw   t1, 4(s0)              # 0x01020F04

    li   t0, 0x01000000
    csrc mepc, t0               # csrrc x0, mepc, t0
    csrr t1, mepc
    sw   t1, 8(s0)              # 0x00020F04

    csrr t1, mhartid
    sw   t1, 12(s0)             # 0

    # Two registers at once, to show they are separate storage.
    li   t0, 0x55555555
    csrw mcause, t0
    csrr t1, mepc
    sw   t1, 16(s0)             # 0x00020F04, unchanged
    csrr t1, mcause
    sw   t1, 20(s0)             # 0x55555555

    li   a0, 0
    ret

    .section .data
    .align 2
    .globl out
out:
    .word 0, 0, 0, 0, 0, 0

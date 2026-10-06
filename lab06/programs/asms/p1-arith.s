# Lab 6: straight-line arithmetic, hazard-free.
#
# No branches, no memory -- the smallest program that still drives every stage
# of the pipeline from IF to WB, and drains it at the end.
#
# Every result is used at least three instructions after it is produced. That
# is the rule this week's programs are written to: without forwarding, an
# instruction waits in ID until its operands have been written back, so a
# producer one or two instructions ahead would stall the pipeline. Three ahead
# is in WB, and WB writes the register file in the first half of the cycle
# while ID reads it in the second, so that one costs nothing.

    .section ".text.init"
    .globl _start
_start:
    li   t0, 5
    li   t1, 7
    li   t2, 11
    li   t3, 3
    nop

    add  t4, t0, t1          # t1 produced 4 back
    sub  t5, t2, t3          # t3 produced 3 back
    xor  t6, t0, t1
    and  a1, t2, t3
    or   a2, t4, t5          # t5 produced 3 back
    sll  a3, t6, t3
    srl  a4, a1, t3
    add  a6, t0, t2
    add  a5, a2, a3
    mul  s2, t0, t1
    nop
    slt  a7, a4, a6          # a6 produced 4 back
    nop
    nop
    add  s3, a5, a7          # a7 produced 3 back
    nop
    nop
    add  s4, s2, s3

# --- exit with status 0 ----------------------------------------------------
    li   a0, 1               # (status << 1) | 1, with status 0
    nop
    nop
    lui  t5, %hi(tohost)
    nop
    nop
    sw   a0, %lo(tohost)(t5)
    nop
1:  j    1b

# The host interface. Both symbols have to exist and be page-aligned this way:
# a store to `tohost` is what ends the program, and Spike -- which the reference
# answers are checked against -- only watches for it when `fromhost` is there
# too. Defining just the one leaves Spike running until it is killed.
    .section ".tohost","aw",@progbits
    .align 6
    .globl tohost
tohost:   .dword 0
    .align 6
    .globl fromhost
fromhost: .dword 0

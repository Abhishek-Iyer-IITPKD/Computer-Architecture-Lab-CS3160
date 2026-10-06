# Lab 6: a dependent chain, spaced out by hand.
#
# Every instruction here depends on the one three before it, which is the
# closest a dependency can be without stalling a pipeline that has no
# forwarding. The nops are not padding for its own sake: they are what a
# compiler scheduling for this machine would have to insert if it had nothing
# useful to put there.
#
# Lab 7 removes the need for them by stalling, and lab 8 removes most of the
# cost by forwarding. Keep this program: running it again then, and watching
# the cycle count not change, is the point.

    .section ".text.init"
    .globl _start
_start:
    li   t0, 1

    nop
    nop
    addi t1, t0, 1           # t0 produced 3 back
    nop
    nop
    addi t2, t1, 1
    nop
    nop
    addi t3, t2, 1
    nop
    nop
    addi t4, t3, 1
    nop
    nop
    addi t5, t4, 1
    nop
    nop
    add  t6, t5, t0

# --- exit with status 0 ----------------------------------------------------
    li   a0, 1
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

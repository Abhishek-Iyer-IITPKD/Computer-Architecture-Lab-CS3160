# The three CSR instructions, with an explicit destination register.
#
# Every one of them is read-then-modify: the destination gets what the register
# held *before* the instruction ran, and the register is left holding something
# computed from that old value. The read happens in the stage that reads the
# registers and the write in the stage that writes them -- a CSR is a register
# file, and no part of this goes near memory.
#
# mcause carries almost all of this, and mepc appears only with word-aligned
# values. Both are registers nothing else in the runtime touches -- mstatus and
# mtvec belong to crt.S, and writing them from a test program would be changing
# the machine out from under the startup code.
#
# The restriction on mepc is not arbitrary. On real hardware it holds the
# address of an interrupted instruction, so its low bits are forced to zero
# whatever you write. This simulator stores the five registers flat and does not
# model that, which is a simplification worth knowing about rather than one to
# discover through a test that disagrees with the hardware.

    .section .text
    .globl main
main:
    la   s0, out

    # --- csrrw: replace ---------------------------------------------------
    li   t0, 0x11112220
    csrrw t1, mepc, t0          # t1 = old mepc (0), mepc = 0x11112220
    sw   t1, 0(s0)

    li   t0, 0x33334440
    csrrw t1, mepc, t0          # t1 = 0x11112220, mepc = 0x33334440
    sw   t1, 4(s0)

    csrrs t1, mepc, zero        # read it back: 0x33334440
    sw   t1, 8(s0)

    # --- csrrs: set the bits of the mask ----------------------------------
    li   t0, 0x0F000000
    csrrw zero, mcause, t0      # mcause = 0x0F000000

    li   t0, 0x000000F0
    csrrs t1, mcause, t0        # t1 = 0x0F000000, mcause = 0x0F0000F0
    sw   t1, 12(s0)

    csrrs t1, mcause, zero
    sw   t1, 16(s0)             # 0x0F0000F0

    # Setting bits that are already set changes nothing.
    li   t0, 0x0F000000
    csrrs t1, mcause, t0
    sw   t1, 20(s0)             # 0x0F0000F0
    csrrs t1, mcause, zero
    sw   t1, 24(s0)             # still 0x0F0000F0

    # --- csrrc: clear the bits of the mask --------------------------------
    li   t0, 0x000000F0
    csrrc t1, mcause, t0        # t1 = 0x0F0000F0, mcause = 0x0F000000
    sw   t1, 28(s0)

    csrrs t1, mcause, zero
    sw   t1, 32(s0)             # 0x0F000000

    # Clearing bits that are already clear changes nothing.
    li   t0, 0x000000FF
    csrrc t1, mcause, t0
    sw   t1, 36(s0)             # 0x0F000000
    csrrs t1, mcause, zero
    sw   t1, 40(s0)             # 0x0F000000

    # --- a mask of all ones ------------------------------------------------
    li   t0, 0xFFFFFFFF
    csrrs t1, mcause, t0        # every bit set
    sw   t1, 44(s0)
    csrrs t1, mcause, zero
    sw   t1, 48(s0)             # 0xFFFFFFFF

    li   t0, 0xFFFFFFFF
    csrrc t1, mcause, t0        # every bit cleared
    sw   t1, 52(s0)
    csrrs t1, mcause, zero
    sw   t1, 56(s0)             # 0

    li   a0, 0
    ret

    .section .data
    .align 2
    .globl out
out:
    .word 0, 0, 0, 0, 0, 0, 0, 0
    .word 0, 0, 0, 0, 0, 0, 0

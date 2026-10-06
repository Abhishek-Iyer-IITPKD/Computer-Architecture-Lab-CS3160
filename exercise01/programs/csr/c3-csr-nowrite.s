# When a CSR instruction does *not* write.
#
# The rule is about the source register, not the destination:
#
#   csrrs / csrrc with rs1 = x0   read only. They must not write at all --
#                                 which is not the same as writing the old
#                                 value back, because on a real machine writing
#                                 has side effects that reading must not cause.
#   csrrw                         always writes, even with rd = x0. Writing is
#                                 the whole purpose of it.
#
# The values below cannot tell the two apart -- writing the old value back
# leaves the same number behind. What tells them apart is the trace, which says
# whether a write happened, and the csr_writes counter in stats.json.

    .section .text
    .globl main
main:
    la   s0, out

    li    t0, 0x0000AAAA
    csrrw zero, mcause, t0        # rd = x0, and it still writes

    csrrs t1, mcause, zero        # read only
    sw    t1, 0(s0)             # 0x0000AAAA
    csrrc t1, mcause, zero        # read only, again
    sw    t1, 4(s0)             # 0x0000AAAA
    csrrs t1, mcause, zero
    sw    t1, 8(s0)             # unchanged: 0x0000AAAA

    # The same three with a real source register do write.
    li    t0, 0x00005555
    csrrs t1, mcause, t0          # mcause = 0xAAAA | 0x5555 = 0xFFFF
    sw    t1, 12(s0)            # 0x0000AAAA
    csrrs t1, mcause, zero
    sw    t1, 16(s0)            # 0x0000FFFF

    li    t0, 0x0000000F
    csrrc t1, mcause, t0          # mcause = 0xFFFF & ~0xF = 0xFFF0
    sw    t1, 20(s0)            # 0x0000FFFF
    csrrs t1, mcause, zero
    sw    t1, 24(s0)            # 0x0000FFF0

    # csrrw with rd = x0 and a source of zero still writes -- zero.
    csrrw zero, mcause, zero
    csrrs t1, mcause, zero
    sw    t1, 28(s0)            # 0

    li   a0, 0
    ret

    .section .data
    .align 2
    .globl out
out:
    .word 0, 0, 0, 0, 0, 0, 0, 0

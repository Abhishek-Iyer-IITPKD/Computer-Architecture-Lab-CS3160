# Arithmetic edge cases, for checking the ALU against Spike.
#
# This program computes each of them and stores the result. It checks nothing
# itself: tests/check_vs_spike.py compares every register and memory write
# against Spike, so simply executing the operation is the test.

.section .data
	.align 2
results:
	.space 512
scratch:
	.word 0
	.word 0
bytes:
	.byte 0x80, 0xff, 0x7f, 0x01
halves:
	.align 2
	.half 0x8000, 0xffff, 0x7fff, 0x0001

.section .text

# Store one result and advance the cursor kept in t0.
.macro SAVE reg
	sw	\reg, 0(t0)
	addi	t0, t0, 4
.endm

	.globl main
main:
	addi	sp, sp, -16
	sw	ra, 12(sp)
	la	t0, results

	# --- Signed division: the direction it rounds in ---
	li	a1, 7
	li	a2, 2
	div	a3, a1, a2		# 3
	rem	a4, a1, a2		# 1
	SAVE	a3
	SAVE	a4

	li	a1, -7
	div	a3, a1, a2		# -3, truncated toward zero (not -4)
	rem	a4, a1, a2		# -1, taking the sign of the dividend
	SAVE	a3
	SAVE	a4

	li	a1, 7
	li	a2, -2
	div	a3, a1, a2		# -3
	rem	a4, a1, a2		# 1
	SAVE	a3
	SAVE	a4

	li	a1, -7
	div	a3, a1, a2		# 3
	rem	a4, a1, a2		# -1
	SAVE	a3
	SAVE	a4

	# --- Division by zero: defined, not a trap ---
	li	a1, 5
	li	a2, 0
	div	a3, a1, a2		# -1
	rem	a4, a1, a2		# the dividend, 5
	divu	a5, a1, a2		# 0xffffffff
	remu	a6, a1, a2		# 5
	SAVE	a3
	SAVE	a4
	SAVE	a5
	SAVE	a6

	li	a1, -5
	div	a3, a1, a2		# -1
	rem	a4, a1, a2		# -5
	SAVE	a3
	SAVE	a4

	# --- The one signed overflow: -2^31 / -1 ---
	li	a1, 0x80000000
	li	a2, -1
	div	a3, a1, a2		# -2^31 again; wraps rather than trapping
	rem	a4, a1, a2		# 0
	divu	a5, a1, a2		# 0: 2^31 / (2^32 - 1) unsigned
	remu	a6, a1, a2		# 2^31
	SAVE	a3
	SAVE	a4
	SAVE	a5
	SAVE	a6

	# --- Unsigned division of patterns that look negative ---
	li	a1, -1			# 0xffffffff
	li	a2, 2
	divu	a3, a1, a2		# 0x7fffffff
	remu	a4, a1, a2		# 1
	div	a5, a1, a2		# 0: -1 / 2 truncates to 0
	rem	a6, a1, a2		# -1
	SAVE	a3
	SAVE	a4
	SAVE	a5
	SAVE	a6

	# --- Multiplication: the high half, and the three signednesses ---
	li	a1, 0x12345678
	li	a2, 0x10000000
	mul	a3, a1, a2		# low half wraps
	mulh	a4, a1, a2
	mulhu	a5, a1, a2
	mulhsu	a6, a1, a2
	SAVE	a3
	SAVE	a4
	SAVE	a5
	SAVE	a6

	li	a1, -3
	li	a2, 5
	mul	a3, a1, a2		# -15
	mulh	a4, a1, a2		# -1: the sign extension of -15
	mulhu	a5, a1, a2		# 4: (2^32 - 3) * 5 >> 32
	mulhsu	a6, a1, a2		# -1
	SAVE	a3
	SAVE	a4
	SAVE	a5
	SAVE	a6

	li	a1, 0x80000000
	li	a2, 0x80000000
	mul	a3, a1, a2		# 0
	mulh	a4, a1, a2		# 2^30
	mulhu	a5, a1, a2		# 2^30
	mulhsu	a6, a1, a2		# -2^30
	SAVE	a3
	SAVE	a4
	SAVE	a5
	SAVE	a6

	# --- Shifts: both ends of the amount, and the sign on sra ---
	li	a1, 0x80000001
	li	a2, 0
	sll	a3, a1, a2		# unchanged
	srl	a4, a1, a2
	sra	a5, a1, a2
	SAVE	a3
	SAVE	a4
	SAVE	a5

	li	a2, 31
	sll	a3, a1, a2
	srl	a4, a1, a2		# 1: zeroes shifted in
	sra	a5, a1, a2		# -1: the sign shifted in
	SAVE	a3
	SAVE	a4
	SAVE	a5

	slli	a3, a1, 1
	srli	a4, a1, 1
	srai	a5, a1, 1
	SAVE	a3
	SAVE	a4
	SAVE	a5

	# Only the low five bits of the amount are used.
	li	a2, 33
	sll	a3, a1, a2		# a shift of 1, not of 33
	srl	a4, a1, a2
	sra	a5, a1, a2
	SAVE	a3
	SAVE	a4
	SAVE	a5

	# --- Comparisons: the same patterns, signed and unsigned ---
	li	a1, -1
	li	a2, 1
	slt	a3, a1, a2		# 1: -1 < 1
	sltu	a4, a1, a2		# 0: 0xffffffff > 1
	slti	a5, a1, 0		# 1
	sltiu	a6, a1, 0		# 0: nothing is below zero unsigned
	SAVE	a3
	SAVE	a4
	SAVE	a5
	SAVE	a6

	li	a1, 0x80000000
	li	a2, 0x7fffffff
	slt	a3, a1, a2		# 1: the most negative is below the most positive
	sltu	a4, a1, a2		# 0: 2^31 is above 2^31 - 1
	SAVE	a3
	SAVE	a4

	# sltiu against the sign-extended immediate -1, i.e. 0xffffffff
	li	a1, 1
	sltiu	a3, a1, -1		# 1
	slti	a4, a1, -1		# 0
	SAVE	a3
	SAVE	a4

	# --- Branches: signed and unsigned on the same pair ---
	li	a1, -1
	li	a2, 1
	li	a3, 0
	bltu	a2, a1, 1f		# taken: 1 < 0xffffffff
	addi	a3, a3, 1
1:	blt	a1, a2, 2f		# taken: -1 < 1
	addi	a3, a3, 2
2:	bge	a1, a2, 3f		# not taken
	addi	a3, a3, 4
3:	bgeu	a1, a2, 4f		# taken: 0xffffffff >= 1
	addi	a3, a3, 8
4:	beq	a1, a1, 5f		# taken
	addi	a3, a3, 16
5:	bne	a1, a1, 6f		# not taken
	addi	a3, a3, 32
6:	SAVE	a3

	# --- Immediates at the ends of the 12-bit field ---
	li	a1, 0
	addi	a3, a1, 2047
	addi	a4, a1, -2048
	xori	a5, a1, -1		# 0xffffffff
	andi	a6, a5, -1366		# sign-extended mask
	SAVE	a3
	SAVE	a4
	SAVE	a5
	SAVE	a6

	# Wrapping around: addition on a full register is modulo 2^32.
	li	a1, 0x7fffffff
	addi	a3, a1, 1		# 0x80000000
	li	a2, 1
	add	a4, a1, a2
	sub	a5, a1, a2
	li	a1, 0x80000000
	sub	a6, a1, a2		# 0x7fffffff
	SAVE	a3
	SAVE	a4
	SAVE	a5
	SAVE	a6

	# --- lui and auipc ---
	lui	a3, 0xfffff		# 0xfffff000
	lui	a4, 0x80008
	auipc	a5, 0			# this instruction's own address
	SAVE	a3
	SAVE	a4
	SAVE	a5

	# --- Sub-word loads: which ones sign-extend ---
	la	a1, bytes
	lb	a3, 0(a1)		# 0x80 -> 0xffffff80
	lbu	a4, 0(a1)		# 0x00000080
	lb	a5, 1(a1)		# 0xff -> 0xffffffff
	lbu	a6, 1(a1)		# 0x000000ff
	SAVE	a3
	SAVE	a4
	SAVE	a5
	SAVE	a6

	lb	a3, 2(a1)		# 0x7f, positive either way
	lbu	a4, 2(a1)
	SAVE	a3
	SAVE	a4

	la	a1, halves
	lh	a3, 0(a1)		# 0x8000 -> 0xffff8000
	lhu	a4, 0(a1)		# 0x00008000
	lh	a5, 2(a1)		# 0xffff -> 0xffffffff
	lhu	a6, 2(a1)		# 0x0000ffff
	SAVE	a3
	SAVE	a4
	SAVE	a5
	SAVE	a6

	# --- Sub-word stores touch only their own bytes ---
	la	a1, scratch
	li	a2, -1
	sw	a2, 0(a1)		# fill the word
	li	a2, 0
	sb	a2, 1(a1)		# clear one byte in the middle
	lw	a3, 0(a1)		# 0xffff00ff
	SAVE	a3

	li	a2, 0x1234
	sh	a2, 2(a1)
	lw	a3, 0(a1)		# 0x123400ff
	SAVE	a3

	# A store of a wide value through a narrow store writes the low bytes only.
	li	a2, 0xaabbccdd
	sb	a2, 0(a1)
	lw	a3, 0(a1)		# 0x123400dd
	SAVE	a3

	# --- jalr ignores the low bit of its target ---
	la	a1, after_jalr
	jalr	a2, 1(a1)		# (address + 1) & ~1, so still after_jalr
after_jalr:
	SAVE	a2			# the return address the jump saved

	lw	ra, 12(sp)
	addi	sp, sp, 16
	li	a0, 0
	ret

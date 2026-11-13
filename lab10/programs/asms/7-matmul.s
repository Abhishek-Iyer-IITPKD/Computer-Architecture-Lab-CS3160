# 3x3 integer matrix multiply: C = A * B.
#
# The program of Lab 2, used as a test program. Three pointers and three loop
# counters exceed the convenient number of t-registers, so they are kept in
# s-registers, saved on the stack; the stack, and so the data cache, is
# therefore heavily used.
#
# Reads A and B, writes C, and returns 1 if every element of C is non-zero.

.section .data
	.align 2
A:	.word 1, 2, 3,  4, 5, 6,  7, 8, 9
B:	.word 1, 0, 0,  0, 1, 0,  0, 0, 1
C:	.fill 9, 4, 0
answer:	.word 0

.section .text

	.globl matmul
matmul:
	addi sp, sp, -32
	sw   ra, 28(sp)
	sw   s0, 24(sp)			# A
	sw   s1, 20(sp)			# B
	sw   s2, 16(sp)			# C
	sw   s3, 12(sp)			# i
	sw   s4, 8(sp)			# j
	sw   s5, 4(sp)			# "all non-zero" flag

	mv   s0, a0
	mv   s1, a1
	mv   s2, a2
	li   s5, 1
	li   s3, 0			# i = 0

row:
	li   t0, 3
	beq  s3, t0, finished
	li   s4, 0			# j = 0

col:
	li   t0, 3
	beq  s4, t0, next_row

	li   t0, 0			# accumulator for C[i][j]
	li   t1, 0			# k = 0
dot:
	li   t2, 3
	beq  t1, t2, store

	# A[i][k] is at A + 4*(3i + k)
	li   t2, 3
	mul  t3, s3, t2
	add  t3, t3, t1
	slli t3, t3, 2
	add  t3, s0, t3
	lw   t3, 0(t3)

	# B[k][j] is at B + 4*(3k + j)
	li   t2, 3
	mul  t4, t1, t2
	add  t4, t4, s4
	slli t4, t4, 2
	add  t4, s1, t4
	lw   t4, 0(t4)

	mul  t3, t3, t4
	add  t0, t0, t3
	addi t1, t1, 1
	j    dot

store:
	# C[i][j] is at C + 4*(3i + j)
	li   t2, 3
	mul  t3, s3, t2
	add  t3, t3, s4
	slli t3, t3, 2
	add  t3, s2, t3
	sw   t0, 0(t3)

	bnez t0, not_zero
	li   s5, 0			# found a zero element
not_zero:
	addi s4, s4, 1
	j    col

next_row:
	addi s3, s3, 1
	j    row

finished:
	mv   a0, s5

	lw   s5, 4(sp)
	lw   s4, 8(sp)
	lw   s3, 12(sp)
	lw   s2, 16(sp)
	lw   s1, 20(sp)
	lw   s0, 24(sp)
	lw   ra, 28(sp)
	addi sp, sp, 32
	ret

	.globl main
main:
	addi sp, sp, -16
	sw   ra, 12(sp)

	la   a0, A
	la   a1, B
	la   a2, C
	call matmul
	la   t0, answer
	sw   a0, 0(t0)

	lw   ra, 12(sp)
	addi sp, sp, 16
	li   a0, 0
	ret

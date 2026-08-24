# Problem 3 -- int matmul(int *A, int *B, int *C)
#
# Write a function `matmul` that multiplies two 3x3 matrices: C = A * B.
#
#   argument 1 (a0) : address of A
#   argument 2 (a1) : address of B
#   argument 3 (a2) : address of C, where the result goes
#   returns    (a0) : 1 if every element of C is non-zero, otherwise 0
#
# Matrices are stored row by row, so element [i][j] of a 3x3 matrix is the
# word at  base + 4*(3*i + j).
#
# C must be fully overwritten -- do not assume it arrives full of zeros.
#
# Three pointers and three loop counters is more than the t-registers hold
# comfortably, so you will probably want s-registers. You may use them, but
# you must put them back before returning.
#
# The tests call `matmul` directly. The main below is for your own use.

.section .data
	.align 2
A:	.word 1, 2, 3,  4, 5, 6,  7, 8, 9
B:	.word 1, 0, 0,  0, 1, 0,  0, 0, 1
C:	.fill 9, 4, 0
answer:	.word 0

.section .text

	.globl matmul
matmul:
	li a3, 1			# a3 = 1
	li t1, 2			# t1 = i = 2
iloop:
	blt t1, zero, iend
	li t2, 2			# t2 = j = 2
jloop:
	blt t2, zero, jend
	li t3, 2			# t3 = k = 2
	li t0, 0			# t0 = 0
kloop:
	blt t3, zero, kend
	li t4, 3				# t4 = 3
	mul t4, t4, t1			# t4 = 3*i
	add t4, t4, t3			# t4 = 3*i + k
	slli t4, t4, 2			# t4 = t4 * 4
	add t4, a0, t4			# t4 = addr(A_ik)
	lw t5, 0(t4)			# t5 = A_ik
	li t4, 3				# t4 = 3
	mul t4, t4, t3			# t4 = 3*k
	add t4, t4, t2			# t4 = 3*k + j
	slli t4, t4, 2			# t4 = t4 * 4
	add t4, a1, t4			# t4 = addr(B_kj)
	lw t6, 0(t4)			# t6 = B_kj
	mul t5, t5, t6			# t5 = A_ik * B_kj
	add t0, t0, t5			# t0 = t0 + A_ik * B_kj
	addi t3, t3, -1			# k--
	j kloop
kend:
	li t4, 3				# t4 = 3
	mul t4, t4, t1			# t4 = 3*i
	add t4, t4, t2			# t4 = 3*i + j
	slli t4, t4, 2			# t4 = t4 * 4
	add t4, a2, t4			# t4 = addr(C_ij)
	sw t0, 0(t4)			# C_ij = t0
	beqz t0, iszero
	j isnotzero
iszero:
	li a3, 0
isnotzero:
	addi t2, t2, -1			# j--
	j jloop
jend:
	addi t1, t1, -1			# i--
	j iloop
iend:
	mv a0, a3
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

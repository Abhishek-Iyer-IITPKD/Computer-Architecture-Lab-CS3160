.section .data
	.align 2
a:	.word 10
result:	.word 0

.section .text
	.globl main
main:
	lw   x5, a
	li   x6, -1		# assume not prime
	li   x7, 2
	blt  x5, x7, store	# a < 2 -> not prime

	li   x28, 2		# trial divisor
loop:
	mul  x29, x28, x28
	bgt  x29, x5, is_prime	# d*d > a: no factor found
	rem  x30, x5, x28
	beqz x30, store		# divides exactly -> composite
	addi x28, x28, 1
	j    loop

is_prime:
	li   x6, 1

store:
	la   x31, result
	sw   x6, 0(x31)
	li   x10, 0
	ret

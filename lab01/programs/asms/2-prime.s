# Problem 2 -- primality test.
#
# Decide whether the value in `a` is a prime number.
# Store 1 in `result` if it is prime, and -1 if it is not.
#
# By definition nothing below 2 is prime, so a = 1, a = 0 and negative values
# must all give -1.
#
# DO NOT rename or reorder the globals below. The tests replace the contents
# of `a` with their own value and then read `result`.

.section .data
	.align 2
a:	.word 10
result:	.word 0

.section .text
	.globl main
main:
	# your code here
	lw x5, a			# x5 = num
	li x6, 2			# x6 = current divisor
	li x7, 1			# x7 = result
	blt x5, x6, no
	j loop
no:
	li x7, -1
	j done
loop:
	beq x5, x6, done
	rem x28, x5, x6
	beqz x28, no
	addi x6, x6, 1
	j loop
done:
	la x28, result
	sw x7, 0(x28)
	li   x10, 0		# exit status 0
	ret			# main must return -- do not loop forever

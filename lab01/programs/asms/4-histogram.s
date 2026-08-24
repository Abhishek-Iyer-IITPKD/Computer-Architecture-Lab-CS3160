# Problem 4 -- histogram of marks.
#
# Each of the first `n` entries of `marks` is a mark between 0 and 10
# inclusive. Count how many students scored each mark: when you are done,
# count[m] must hold the number of marks equal to m, for every m in 0..10.
#
# `count` starts out as all zeros. Entries of `marks` beyond the first `n`
# must not be counted.
#
# Hint: count[m] lives at address (address of count) + 4*m, because each entry
# is a 4-byte word. You do not need a comparison per bucket.
#
# DO NOT rename, reorder or resize the globals below. The tests replace the
# contents of `n`, `marks` and `count` with their own data and then read
# `count` back, so `marks` must keep room for all 32 words.

.section .data
	.align 2
count:	.fill 11, 4, 0		# one bucket per mark 0..10
marks:	.word 2, 3, 0, 5, 10, 7, 1, 10, 10, 8
	.word 8, 9, 6, 7, 8, 2, 4, 5, 0, 9
	.fill 12, 4, 0		# marks holds 32 words in total
n:	.word 20

.section .text
	.globl main
main:
	# your code here
	la x5, marks			# x5 = address of marks
	lw x6, n
	li x7, 4
	mul x6, x6, x7		# x6 = 4*n
	la x7, count			# x7 = address of count
	li x11, 0				# x11 = offset
loop:
	bge x11, x6, done
	add x17, x11, x5
	lw x12, 0(x17)
	li x17, 4
	mul x12, x12, x17
	add x17, x12, x7
	lw x13, 0(x17)
	addi x13, x13, 1
	sw x13, 0(x17)
	addi x11, x11, 4
	j loop
done:	
	li   x10, 0		# exit status 0
	ret			# main must return -- do not loop forever

# data_vars.s -- worked example, given to you. Read it, build it, run it.
#
# Shows how to declare variables and an array in the .data section, and how to
# load their addresses and values into registers.
#
# Two things to notice:
#
# 1. Which registers this uses. x5, x6 and x7 hold addresses; x11..x14 hold
#    values. All of these are yours to overwrite. x1, x2, x3 and x4 are NOT --
#    they hold the return address, the stack pointer and two pointers the
#    start-up code set up. Overwrite x1 and the `ret` below will jump to
#    whatever you left there instead of returning. See docs/registers.md.
#
# 2. How main ENDS: `ret`. Every program you write in this course must return
#    from main. Returning hands control back to the start-up code, which stops
#    the machine cleanly. A program that instead spins in a loop forever never
#    stops, and the test harness cannot read your results.

.section .data
	.align 2
myvar1:
	.word 0xc001

myvar2:
	.word 0xc0de

myarr:
	.word 0xfeed
	.word 0xdeed
	.word 0xdeaf
	.word 0xd00d

.section .text
	.globl main
main:
	la   x5, myvar1		# x5 = address of myvar1
	lw   x11, 0(x5)		# x11 = contents of myvar1

	la   x6, myvar2
	lw   x12, 0(x6)

	la   x7, myarr		# x7 = address of the first element
	lw   x13, 0(x7)		# x13 = myarr[0]
	lw   x14, 4(x7)		# x14 = myarr[1]  (words are 4 bytes apart)

	li   x10, 0		# exit status 0
	ret

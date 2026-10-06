# Part 1 -- a circular buffer of 8 words.
#
# You write three functions. The loop that drives them is
# already below: it walks the script of operations the tests supply, calls the
# right function for each one, and records what it returned.
#
#   enqueue       a0 holds the value to add. Returns 0, or -1 if the buffer
#                 was already full and the oldest element was overwritten.
#   dequeue       returns the oldest element in a0, or -1 if the buffer is
#                 empty.
#   get_vacancy   returns how many words of the buffer are free, 0 to 8 (CAP).
#
# The state of the buffer is three globals:
#
#   buf     the storage, 8 words (CAP)
#   head    the index into buf of the oldest element
#   count   how many elements are in the buffer, 0 to 8 (CAP)
#
# There is no tail. The next free slot is always (head + count) mod CAP. 
# Deriving it removes the ambiguity a head-and-tail pair has when the two meet, 
# where nothing distinguishes a full buffer from an empty one.
#
# DO NOT rename, reorder or resize the globals below. The tests replace the
# contents of `n_ops`, `op` and `arg` with their own data and then read `out`,
# so each of those arrays must keep room for all 24 words.

.equ CAP,  8
.equ NOPS, 24

.section .data
	.align 2
n_ops:	.word 0
op:	.fill NOPS, 4, 0
arg:	.fill NOPS, 4, 0
out:	.fill NOPS, 4, 0

# The buffer, and where it starts and ends. Both start at zero: an empty buffer.
buf:	.fill CAP, 4, 0
head:	.word 0
count:	.word 0

# The driver's loop index. Nothing you write needs to touch it.
idx:	.word 0

.section .text

# ============================================================================
# enqueue -- add a0 to the buffer. Returns 0, or -1 if something was lost.
#
#   * the next free slot is (head + count) mod CAP; store a0 there
#   * if the buffer was not full, one more element is in it now, and the answer
#     is 0
#   * if it was full, the slot you just wrote held the oldest element -- so the
#     oldest is now the one after it, the number of elements is unchanged, and
#     the answer is -1, because an element was silently dropped
#
# Overwriting is still not an error: -1 reports it, it does not stop anything.
#
# "mod CAP" is never a division here. An index that has just been stepped on by
# one can only ever be one too large, so a compare and a subtract is enough.
#
# a0 may be destroyed. If you call another function from here, save `ra` first.
# ============================================================================
	.globl enqueue
enqueue:
	# TODO(part 1): your code here.

	li   a0, 0
	ret

# ============================================================================
# dequeue -- take the oldest element out and return it in a0.
#
#   * if the buffer is empty, return -1 and change nothing. That is not an
#     error and must not stop the program
#   * otherwise the answer is the element at head, and afterwards head steps on
#     by one (mod CAP) and one fewer element is in the buffer
# ============================================================================
	.globl dequeue
dequeue:
	# TODO(part 1): your code here.

	li   a0, 0
	ret

# ============================================================================
# get_vacancy -- how many words of the buffer are free, returned in a0.
#
# CAP when it is empty, 0 when it is full, and it changes nothing. Note that an
# enqueue into a full buffer does not change it either: something was lost, but
# nothing was freed.
# ============================================================================
	.globl get_vacancy
get_vacancy:
	# TODO(part 1): your code here.

	li   a0, 0
	ret

# ============================================================================
# The driver. This is written for you; you should not need to change it.
#
# For each i from 0 to n_ops-1 it reads op[i] -- 1 for enqueue, 2 for dequeue,
# 3 for get_vacancy -- calls that function, and writes what it returned into
# out[i]. Every operation records its own return value, which is why one array
# of answers is enough for three different questions.
#
# It keeps nothing in a register across a call, so the three functions are free
# to use any register they like except the stack pointer.
# ============================================================================
	.globl main
main:
	addi sp, sp, -16
	sw   ra, 12(sp)

	la   t0, idx
	sw   zero, 0(t0)	# i = 0

loop:
	la   t0, idx
	lw   t0, 0(t0)		# i
	lw   t1, n_ops
	bge  t0, t1, done

	slli t2, t0, 2		# byte offset of entry i
	la   t3, op
	add  t3, t3, t2
	lw   t3, 0(t3)		# op[i]

	li   t4, 1
	beq  t3, t4, call_enqueue
	li   t4, 2
	beq  t3, t4, call_dequeue

	jal  ra, get_vacancy
	j    record

call_enqueue:
	la   t3, arg
	add  t3, t3, t2
	lw   a0, 0(t3)		# arg[i]
	jal  ra, enqueue
	j    record

call_dequeue:
	jal  ra, dequeue

record:
	la   t0, idx
	lw   t0, 0(t0)		# reload: the call may have used t0
	slli t2, t0, 2
	la   t4, out
	add  t4, t4, t2
	sw   a0, 0(t4)		# out[i] = what the function returned

	addi t0, t0, 1
	la   t4, idx
	sw   t0, 0(t4)
	j    loop

done:
	lw   ra, 12(sp)
	addi sp, sp, 16
	li   a0, 0		# exit status 0
	ret			# main must return -- do not loop forever

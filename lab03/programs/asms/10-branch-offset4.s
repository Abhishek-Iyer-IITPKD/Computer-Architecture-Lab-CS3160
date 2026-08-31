# A branch whose target is the instruction after it.
#
# `beq zero, zero, .+4` is always taken, and it is taken *to where it would have
# fallen through to anyway*. The assembler will produce it from that source but
# no compiler ever emits it: it is a no-op with a pipeline flush attached.
#
# It exists here because it is the one encoding that separates the simulator's
# two counts of the same thing:
#
#   taken_branches   inferred by stage_writeback() from next_pc != pc + 4, which
#                    this branch defeats -- its next_pc *is* pc + 4
#   pc_redirects     measured by redirects_pc() from the branch's own verdict,
#                    which is what the run loop flushes on
#
# So four of these add 4 to `branches`, 0 to `taken_branches`, 4 to
# `pc_redirects`, and 4 x depth to `flushed_instructions`. Before pc_redirects
# existed, tests/check_models.py reconstructed the redirect count as
# taken_branches + jumps and so computed a flush budget too small to hold the
# flushes that actually happened -- it reported a failure on this program, which
# is correct. That is the regression this file guards.
#
# handouts/derived-statistics.tex walks a student through this program and
# quotes its counter values, so changing it means changing that document.
#
# Also worth having for its own sake: it is the only program in the corpus where
# a redirect target equals the fall-through address, so it is the only one that
# proves the run loop redirects on the branch's verdict rather than on whether
# the PC would have changed.

.section .text
	.globl main
main:
	beq	zero, zero, .+4
	beq	zero, zero, .+4
	beq	zero, zero, .+4
	beq	zero, zero, .+4

	# And the not-taken counterpart of the same encoding, which agrees with
	# both counters: bne can never be true with two zero operands, so control
	# falls through and nothing is flushed.
	bne	zero, zero, .+4
	bne	zero, zero, .+4

	li	a0, 0
	ret

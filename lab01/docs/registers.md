# RISC-V registers and instructions -- quick reference

There is no internet in the lab, so this is your reference. `RISCV_CARD.pdf`
in this folder has the full instruction list; this file covers the registers,
a worked example, and the things looked up most often.

## The 32 registers

There are 32 registers, each 32 bits wide, named `x0` to `x31`.

**In this lab, use register numbers: `x5`, `x11`, `x28`.** You may see
alternative names like `t0`, `a0` and `sp` in the disassembly output. Those are
*ABI names* -- a convention about what each register is used for when functions
call each other. They mean nothing to the hardware: `x10` and `a0` assemble to
exactly the same bits. We will introduce them in Lab 2, along with the calling
convention that gives them their meaning. Until then, stick to using register
numbers.

| Register    | Reserved for                         | Necessary for Lab 1? |
|-------------|--------------------------------------|----------------------|
| `x0`        | Always reads 0; writes are discarded | yes, see below       |
| `x1`        | Return address                       | **no**               |
| `x2`        | Stack pointer                        | **no**               |
| `x3`        | Global pointer                       | **no**               |
| `x4`        | Thread pointer                       | **no**               |
| `x5`-`x7`   | --                                   | yes                  |
| `x8`-`x9`   | (callee-saved; Lab 2)                | avoid for now        |
| `x10`       | Exit status when `main` returns      | yes, at the end      |
| `x11`-`x17` | --                                   | yes                  |
| `x18`-`x27` | (callee-saved; Lab 2)                | avoid for now        |
| `x28`-`x31` | --                                   | yes                  |

So: **`x5`-`x7`, `x11`-`x17` and `x28`-`x31` are yours.** That is fourteen
registers, which is plenty for these problems.

### Why `x1` to `x4` are off limits

The start-up code sets these up before it calls your `main`, and something will
break if you change them. `x1` is the worst: it holds the address that `ret`
jumps back to. Your program does not return properly if this is overwritten. It
may jump into your data section (or elsewhere in RAM) and start executing the
contents present there, as if they were instructions. The tests will report this
as *"program crashed"*.

### `x0` is genuinely useful

Because `x0` always reads as zero and throws away anything written to it,
several common operations are expressed through it:

```asm
    add  x5, x6, x0     # copy x6 into x5
    beq  x5, x0, label  # branch if x5 is zero
    sw   x0, 0(x6)      # store zero to memory
```

## A worked example: summing an array

This demonstrates how you can write a loop in this lab -- walk a pointer along
an array, one element at a time, until a counter runs out.

```asm
.section .data
        .align 2
n:      .word 5                 # how many elements
list:   .word 4, 8, 15, 16, 23  # the elements themselves
total:  .word 0                 # where the answer goes

.section .text
        .globl main
main:
        la   x5, list           # x5 = ADDRESS of the first element
        lw   x6, n              # x6 = how many are left to visit
        li   x7, 0              # x7 = running total, starts at zero

loop:
        beqz x6, done           # nothing left? then we are finished
        lw   x28, 0(x5)         # x28 = the element x5 currently points at
        add  x7, x7, x28        # add it to the running total

        addi x5, x5, 4          # advance to the next element: +4 BYTES
        addi x6, x6, -1         # one fewer left to visit
        j    loop

done:
        la   x28, total         # x28 = address of `total`
        sw   x7, 0(x28)         # write the answer into memory
        li   x10, 0             # exit status 0
        ret                     # return -- this stops the machine
```


**`la` versus `lw`.** `la x5, list` puts the *address* of `list` into `x5`.
`lw x6, n` puts the *contents* of `n` into `x6`. They look similar and do
completely different things.

```asm
    la   x5, n          # x5 = the ADDRESS of n
    lw   x6, 0(x5)      # x6 = the CONTENTS at that address
    lw   x6, n          # shorthand for both lines above
```

**`0(x5)` means "the memory at the address in `x5`".** The number in front is
an offset in bytes, so `4(x5)` is the next word along and `0(x5)` is the one
you are pointing at. This is the only way to reach memory: RISC-V arithmetic
instructions work on registers only, never directly on memory.

**Stepping by 4, not by 1.** Each `.word` is four bytes, so element `i` of an
array lives at `base + 4*i`. Adding 1 to your pointer moves you a quarter of
the way into the current element, which is a mess. Either advance by 4 as
above, or compute the offset with a shift:

```asm
    slli x29, x28, 2    # x29 = x28 * 4   (shift left 2 = multiply by 4)
    add  x29, x5, x29   # x29 = &list[x28]
```

**Writing the answer back.** Computing the right value in a register is only
half the job -- the tests read your answer out of *memory*, so you must store
it. Forgetting the `sw` at the end can also lead to a test case failing.

## Pseudo-instructions

The assembler accepts these and expands each into one or two real
instructions. Look at `programs/dumps/*.dump` to see what yours became.

| You write        | What it does                             |
| ---------------- | ---------------------------------------- |
| `li rd, imm`     | Load a constant into `rd`                |
| `la rd, symbol`  | Load the **address** of a variable       |
| `mv rd, rs`      | Copy `rs` into `rd`                      |
| `ret`            | Return from a function                   |
| `j label`        | Jump                                     |
| `beqz rs, label` | Branch if `rs` is zero                   |
| `bnez rs, label` | Branch if `rs` is not zero               |
| `bltz rs, label` | Branch if `rs` is negative               |
| `bgt rs, rt, l`  | Branch if `rs > rt`                      |
| `ble rs, rt, l`  | Branch if `rs <= rt`                     |

## Declaring data

```asm
.section .data
        .align 2                # align to a 4-byte boundary
count:  .word 0                 # one 32-bit variable, initially 0
list:   .word 4, 8, 15, 16      # four consecutive words
        .fill 12, 4, 0          # 12 more words, each 4 bytes, each 0
```

`.fill count, size, value` is how the starter files reserve room for an array
without writing out every entry. Do not change these declarations: the tests
overwrite them with their own data and rely on the sizes staying put.

## Arithmetic and comparison notes

- `rem` gives a remainder, `div` a quotient. 
  `rem x28, x5, x6` computes `x5 % x6`.
- `slli rd, rs, 2` multiplies by 4 -- the usual way to turn an array index
  into a byte offset.
- Comparisons come in signed and unsigned forms: `blt` is signed, `bltu` is
  unsigned. Your arrays contain negative numbers, so you want the signed
  forms. `bltu` would treat -1 as a very large positive number.
- There is no "branch if greater than" instruction. `bgt` is a
  pseudo-instruction that swaps the operands and uses `blt`.

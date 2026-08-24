# The RISC-V calling convention

## Why a convention is needed at all

The hardware has no idea what a "function" (in some software programming 
language) is. There is no call instruction that saves your variables, 
no notion of a local variable, and nothing stopping one piece of code 
from overwriting registers another piece was relying on.

All of that is handled by a *convention*: a set of agreements about who puts
what where. If your code and my code, both follow the agreement, my function can
call yours without either of us reading the other's source. That is the whole
point of having calling conventions, and is also the reason why your functions
are tested by *our* driver code this week. Note how either of our code never has
to see each other's implementation to be able to work seamlessly.

## Register names, finally

In this lab, the registers mean something. Each register has an ABI name saying
what it is used for; `x10` and `a0` are the same register, and you should now
write `a0`.

| Register    | ABI name  | Role                          | Who preserves it |
| ----------- | --------- | ----------------------------- | ---------------- |
| `x0`        | `zero`    | Always zero                   | --               |
| `x1`        | `ra`      | Return address                | caller           |
| `x2`        | `sp`      | Stack pointer                 | **callee**       |
| `x3`        | `gp`      | Global pointer                | never touch      |
| `x4`        | `tp`      | Thread pointer                | never touch      |
| `x5`-`x7`   | `t0`-`t2` | Temporary                     | caller           |
| `x8`        | `s0`/`fp` | Saved / frame pointer         | **callee**       |
| `x9`        | `s1`      | Saved                         | **callee**       |
| `x10`-`x11` | `a0`-`a1` | Arguments 1-2, return value   | caller           |
| `x12`-`x17` | `a2`-`a7` | Arguments 3-8                 | caller           |
| `x18`-`x27` | `s2`-`s11`| Saved                         | **callee**       |
| `x28`-`x31` | `t3`-`t6` | Temporary                     | caller           |

## The one rule that matters

**Caller-saved (`t0`-`t6`, `a0`-`a7`, `ra`):** a function you call may destroy
these. If you need a value afterwards, save it yourself before calling.

**Callee-saved (`s0`-`s11`, `sp`):** a function you call must give these back
unchanged. If *you* want to use `s0`, you must save the old value first and
restore it before you return.

Read that second one carefully. `s0`-`s11` are convenient -- they survive calls,
so they are the natural home for a loop counter in a function that calls
something. But they are convenient *because everybody restores them*, and your
code is expected to do the same.

The tests check this directly. They fill every `s` register with a known value,
call your function, and check the values afterwards. Getting the right answer
while clobbering `s0` is reported as `abi FAIL`: it would corrupt whatever your
caller was doing.

## Leaf functions

A function that calls nothing is a *leaf*. It needs no stack frame at all,
provided it sticks to `t` and `a` registers:

```asm
        .globl double_it
double_it:
        add  a0, a0, a0         # argument in a0, result in a0
        ret                     # nothing to save, nothing to restore
```

## Non-leaf functions and the stack

The moment your function calls another function, you have a problem: `call`
overwrites `ra` with the return address of *your* call, destroying the one you
needed to get back to your own caller. So you must save it.

The stack is the place to put it. `sp` points at the lowest in-use word; you
make room by *subtracting* from it, and hand the room back before returning.

```asm
        .globl f
f:
        addi sp, sp, -16        # make a 16-byte frame
        sw   ra, 12(sp)         # save the return address
        sw   s0, 8(sp)          # save s0, because we are about to use it

        mv   s0, a0             # keep our argument somewhere call-proof
        call g                  # this overwrites ra, a0-a7, t0-t6
        add  a0, a0, s0         # a0 is g's result. s0 still holds our argument

        lw   s0, 8(sp)          # put the caller's s0 back
        lw   ra, 12(sp)         # and the return address
        addi sp, sp, 16         # release the frame
        ret
```

Here is what those four instructions actually do to memory. The stack grows
*downwards*, so higher addresses are drawn at the top and `addi sp, sp, -16`
moves `sp` down the page:

```
          higher addresses
        +---------------------+
        |                     |
        |   caller's frame    |
        |                     |
        +=====================+  <--- sp when f was entered
        |   saved ra          |  12(sp)     sw ra, 12(sp)
        +---------------------+
        |   saved s0          |   8(sp)     sw s0, 8(sp)
        +---------------------+
        |   unused            |   4(sp)
        +---------------------+
        |   unused            |   0(sp)
        +=====================+  <--- sp inside f, after addi sp, sp, -16
        |                     |
        |   free stack        |
        v                     v
          lower addresses
```

Two things to read off the picture.

**The offsets are measured upwards from the new `sp`.** `12(sp)` is the word
twelve bytes above where `sp` now points -- the top slot of the frame. That is
why `sw ra, 12(sp)` comes *after* `addi sp, sp, -16`, never before.

**The frame is handed back by putting `sp` where it was.** `addi sp, sp, 16`
at the end does that. Everything below `sp` is free for anyone to use, so a
function that returns without restoring `sp` leaves its caller pointing into
the middle of nowhere -- which is why `sp` is callee-saved, and why the tests
check it.

Keep the frame size a multiple of 16. That is part of the convention too, and
while nothing here will crash if you ignore it, do it anyway.

### The three-line checklist

Whenever you write a function, ask:

1. Do I call anything? If yes, save and restore `ra`.
2. Do I use any of `s0`-`s11`? If yes, save and restore each one I use.
3. Does `sp` end where it started?

Almost every bug in this lab is one of those three.

## Recursion

Recursion needs nothing new -- each call simply gets its own frame, further
down the stack. What makes it feel harder is that the mistake is invisible
until you unwind:

```asm
        .globl countdown
countdown:
        addi sp, sp, -16
        sw   ra, 12(sp)
        sw   s0, 8(sp)

        beqz a0, base           # base case: stop recursing
        mv   s0, a0             # remember n
        addi a0, a0, -1
        call countdown          # recurse
        mv   a0, s0             # n is still here, one frame down
base:
        lw   s0, 8(sp)
        lw   ra, 12(sp)
        addi sp, sp, 16
        ret
```

Nothing in that code says "recursion". It is the same three-line checklist as
before -- save `ra`, save `s0`, put `sp` back. What recursion adds is that
several frames exist at once, one per call still in progress. Calling
`countdown(2)` builds this:

```
          higher addresses
        +=====================+  <--- sp before the first call
        |  ra -> our caller   |     frame for countdown(2)
        |  s0 = 2             |
        +=====================+
        |  ra -> inside cd(2) |     frame for countdown(1)
        |  s0 = 1             |
        +=====================+
        |  ra -> inside cd(1) |     frame for countdown(0)
        |  s0 = (unused)      |     base case: does not recurse
        +=====================+  <--- sp at the deepest point
          lower addresses
```

Each call has its own `s0` slot, so each call's `n` is safe from the calls
below it. Returning pops the frames back off in reverse order, and `sp` climbs
back to where it started.

If you forget to save `ra`, the innermost call returns correctly, and then
every outer return jumps to the same wrong place. The tests report this as
*"crashed"* or as running past the instruction limit.

Make sure your base case actually triggers. `fact(0)` must return without
recursing, or you will recurse until the stack runs into your data.

## Calling a function

```asm
        la   a0, myarray        # first argument
        li   a1, 10             # second argument
        call myfunc             # jump, and put the return address in ra
        # result is now in a0
        # t0-t6 and a1-a7 may have been destroyed
```

`call` is a pseudo-instruction; the disassembly will show it as `jal` or as an
`auipc`/`jalr` pair. `ret` is likewise `jalr zero, 0(ra)`.

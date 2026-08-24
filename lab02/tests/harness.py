"""Shared test-harness machinery for the assembly labs.

The approach, in one paragraph: we assemble the student's .s file, look up the
named globals in the resulting ELF's symbol table, overwrite the *input*
globals with test data, run the patched program on Spike, replay Spike's
commit log to reconstruct the final contents of memory, and compare the
*output* globals against the expected answer. Patching inputs is what lets one
student program be tested against many cases -- a hardcoded answer passes the
first case and fails the rest.

Nothing here depends on how the student solved the problem: only on where the
answer ends up. Two different sorting algorithms both pass.
"""

import os
import re
import struct
import subprocess

ARCH = "rv32ima_zicsr"
MAX_STEPS = 200000
SPIKE_TIMEOUT = 30

# ---------------------------------------------------------------- ELF reading

class Elf:
    """Just enough ELF32-little-endian to find symbols and read/write memory
    images. Parsing directly avoids depending on the exact objcopy/nm version
    installed on the lab machines."""

    def __init__(self, path):
        with open(path, "rb") as f:
            self.raw = bytearray(f.read())
        if self.raw[:4] != b"\x7fELF":
            raise ValueError(f"{path} is not an ELF file")
        if self.raw[4] != 1:
            raise ValueError(f"{path} is not 32-bit")

        e_phoff, e_shoff = struct.unpack_from("<II", self.raw, 0x1C)
        e_phentsize, e_phnum = struct.unpack_from("<HH", self.raw, 0x2A)
        e_shentsize, e_shnum = struct.unpack_from("<HH", self.raw, 0x2E)

        # Loadable segments: (vaddr, file offset, filesz, memsz)
        self.segments = []
        for i in range(e_phnum):
            off = e_phoff + i * e_phentsize
            p_type, p_offset, p_vaddr = struct.unpack_from("<III", self.raw, off)
            p_filesz, p_memsz = struct.unpack_from("<II", self.raw, off + 16)
            if p_type == 1:  # PT_LOAD
                self.segments.append((p_vaddr, p_offset, p_filesz, p_memsz))

        # Symbol table
        self.symbols = {}
        sections = []
        for i in range(e_shnum):
            off = e_shoff + i * e_shentsize
            sh_type, = struct.unpack_from("<I", self.raw, off + 4)
            sh_offset, sh_size = struct.unpack_from("<II", self.raw, off + 16)
            sh_link, = struct.unpack_from("<I", self.raw, off + 24)
            sh_entsize, = struct.unpack_from("<I", self.raw, off + 36)
            sections.append((sh_type, sh_offset, sh_size, sh_link, sh_entsize))

        for sh_type, sh_offset, sh_size, sh_link, sh_entsize in sections:
            if sh_type != 2 or sh_entsize == 0:  # SHT_SYMTAB
                continue
            strtab_off = sections[sh_link][1]
            for j in range(sh_size // sh_entsize):
                e = sh_offset + j * sh_entsize
                st_name, st_value, st_size = struct.unpack_from("<III", self.raw, e)
                if st_name == 0:
                    continue
                end = self.raw.index(b"\0", strtab_off + st_name)
                name = self.raw[strtab_off + st_name:end].decode("ascii", "replace")
                self.symbols[name] = (st_value, st_size)

    def addr_of(self, name):
        if name not in self.symbols:
            raise KeyError(name)
        return self.symbols[name][0]

    def _file_offset(self, vaddr, nbytes):
        for seg_vaddr, seg_off, filesz, _ in self.segments:
            if seg_vaddr <= vaddr and vaddr + nbytes <= seg_vaddr + filesz:
                return seg_off + (vaddr - seg_vaddr)
        return None

    def read(self, vaddr, nbytes):
        """Initial contents of memory, straight from the loadable image.
        Addresses in .bss (memsz beyond filesz) read as zero."""
        off = self._file_offset(vaddr, nbytes)
        if off is None:
            return bytes(nbytes)
        return bytes(self.raw[off:off + nbytes])

    def write(self, vaddr, data):
        off = self._file_offset(vaddr, len(data))
        if off is None:
            raise ValueError(f"{vaddr:#x} is not inside a loadable segment")
        self.raw[off:off + len(data)] = data

    def save(self, path):
        with open(path, "wb") as f:
            f.write(self.raw)


def words(values):
    """Encode a list of signed 32-bit ints as little-endian bytes."""
    return b"".join(struct.pack("<i", v) for v in values)


def unwords(data):
    return list(struct.unpack("<%di" % (len(data) // 4), data))


# ------------------------------------------------------------ building/running

class BuildError(Exception):
    pass


LINK_FLAGS = [
    "-T./custom/test.ld", "-static", "-mcmodel=medany",
    "-fvisibility=hidden", "-nostdlib", "-nostartfiles", "-g",
    "./custom/crt.S", "-I./custom",
    f"-march={ARCH}", "-mabi=ilp32",
]


def _run(cmd, cwd):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if p.returncode != 0:
        raise BuildError(p.stderr.strip() or p.stdout.strip())


def assemble(src, out_elf, programs_dir):
    """Assemble one .s the same way the student's Makefile does, producing a
    standalone program that runs the student's own main."""
    _run(["riscv-none-elf-gcc", os.path.abspath(src)] + LINK_FLAGS +
         ["-o", os.path.abspath(out_elf)], programs_dir)


def link_with_driver(src, driver, out_elf, programs_dir, drivers_dir):
    """Build a program in which OUR driver is main and the student's file
    supplies only the function under test.

    This is what lets us test the calling convention rather than the student's
    own main: the driver chooses the arguments, checks the return value, and
    checks that callee-saved registers survived the call.

    The student's main is renamed out of the way rather than rejected -- they
    need a main of their own so that `make run` works while they develop.
    """
    obj = os.path.splitext(os.path.abspath(out_elf))[0] + ".o"
    _run(["riscv-none-elf-gcc", "-c", os.path.abspath(src),
          f"-march={ARCH}", "-mabi=ilp32", "-g", "-o", obj], programs_dir)
    _run(["riscv-none-elf-objcopy", "--redefine-sym", "main=student_main", obj],
         programs_dir)
    _run(["riscv-none-elf-gcc", os.path.abspath(driver), obj] + LINK_FLAGS +
         ["-I" + os.path.abspath(drivers_dir),
          "-o", os.path.abspath(out_elf)], programs_dir)


# Spike commit lines, e.g.
#   core   0: 3 0x80002008 (0x0002a303) x6  0x0000000b mem 0x80008000
#   core   0: 3 0x8000200c (0x0062a423) mem 0x80008008 0x0000000b
# A trailing "mem <addr>" with no value is a load (address only, no write).
_STORE = re.compile(r"\bmem\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\b")
_TOHOST_OK = None  # set per-run


def run_spike(elf_path):
    """Returns (trace_text, timed_out). Spike halts by itself when the program
    stores to tohost; MAX_STEPS only bounds a runaway program."""
    cmd = ["spike", f"--isa={ARCH}", "--priv=msu",
           f"--steps={MAX_STEPS}", "--log-commits", "-l", elf_path]
    try:
        p = subprocess.run(cmd, capture_output=True, text=True,
                           timeout=SPIKE_TIMEOUT)
    except subprocess.TimeoutExpired:
        return "", True
    trace = p.stderr
    overran = "Max steps exceeded" in trace or "Max steps exceeded" in p.stdout
    return trace, overran


def final_memory(elf, trace, regions):
    """Reconstruct the final contents of the given (addr, nbytes) regions.

    Start from the program's initial image, then apply every store the trace
    reports. A location the program never wrote keeps its initial value, which
    is what makes in-place algorithms (e.g. sorting) work.
    """
    mem = {}
    for addr, nbytes in regions:
        initial = elf.read(addr, nbytes)
        for i, b in enumerate(initial):
            mem[addr + i] = b

    for m in _STORE.finditer(trace):
        addr = int(m.group(1), 16)
        raw = m.group(2)
        value = int(raw, 16)
        # Spike prints as many hex digits as the store was wide:
        # 0x21 is a byte, 0x0021 a halfword, 0x00000021 a word.
        nbytes = max(1, (len(raw) - 2 + 1) // 2)
        for i in range(nbytes):
            if addr + i in mem:
                mem[addr + i] = (value >> (8 * i)) & 0xFF
    return mem


def read_region(mem, addr, nbytes):
    return bytes(mem.get(addr + i, 0) for i in range(nbytes))


def exit_status(elf, trace):
    """The status the program finished with, or None if it never finished.

    crt.S ends by storing (status << 1) | 1 to the `tohost` word. A program
    that traps -- because it jumped somewhere silly, most often by overwriting
    the return address in x1 -- lands in the default trap handler, which exits
    with -32. That is worth reporting differently from a wrong answer.
    """
    if "tohost" not in elf.symbols:
        return None
    tohost = elf.addr_of("tohost")
    last = None
    for m in _STORE.finditer(trace):
        if int(m.group(1), 16) == tohost:
            last = int(m.group(2), 16)
    if last is None:
        return None
    if last & 0xFFFFFFFF > 0x7FFFFFFF:
        last -= 1 << 32
    return last >> 1


TRAP_STATUS = -32

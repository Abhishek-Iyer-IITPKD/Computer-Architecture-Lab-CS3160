#include "csr.h"

#include <cstring>

// The five, in slot order. One table rather than a switch in each function, so
// that adding a sixth register is one line and cannot be added to the read path
// and forgotten in the write path.
static const u32 NUMBERS[CSR_COUNT] = {
    CSR_MSTATUS, CSR_MTVEC, CSR_MEPC, CSR_MCAUSE, CSR_MHARTID,
};

static const char *NAMES[CSR_COUNT] = {
    "mstatus", "mtvec", "mepc", "mcause", "mhartid",
};

void csr_init(Csrs &c) {
    memset(c.word, 0, sizeof(c.word));
}

int csr_index(u32 num) {
    // TODO(midsem): which slot holds this CSR?
    //
    // Return the index into Csrs::word for `num`, or -1 if this simulator
    // does not model that register. NUMBERS above lists the five it has, in
    // slot order.
    for(int i=0;i<5;i++){
        if(NUMBERS[i] == num) return i;
    }
    return -1;
}

bool csr_writable(u32 num) {
    // TODO(midsem): may a program write this register?
    //
    // Four of the five are ordinary read/write registers. One is not: see
    // the table in csr.h, and the note about what happens to a write that
    // is not allowed.
    if(num == CSR_MHARTID) return false;
    return true;
}

const char *csr_mnemonic(u32 num) {
    int i = csr_index(num);
    return (i < 0) ? "?" : NAMES[i];
}

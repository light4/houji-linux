// houji EL2 probe stub, runs at EL1 at kernel entry.
// Appended after Image; entry patched to branch here.
// Writes only non-standard SMCCC responses into linux_banner (printed by kernel),
// then either enters EL2 (if takeover worked) or jumps back to kernel entry unchanged.
//
// STUB_AT and BANNER are injected by patch.py via -defsym.

    .equ VEC_BASE, 0x0

// ---- vector table (VBAR_EL2 = _base + VEC_BASE), 16 slots x 0x80 ----
_base:
    .rept 16
    b   _exc
    .space 0x7c
    .endr

    .equ CODE, 0x800

// ---- code ----
    .org CODE
_code:
    adr  x9,  _base
    msr  vbar_el1, x9              // own vectors: faults reset instead of hanging
    isb
    // x0 = dtb
    adr  x9,  _save
    stp  x0,  x1,  [x9, #0x00]
    stp  x2,  x3,  [x9, #0x10]
    stp  x4,  x5,  [x9, #0x20]
    stp  x6,  x7,  [x9, #0x30]
    stp  x8,  x9,  [x9, #0x40]
    stp  x10, x11, [x9, #0x50]
    stp  x12, x13, [x9, #0x60]
    stp  x14, x15, [x9, #0x70]
    stp  x16, x17, [x9, #0x80]
    stp  x18, x19, [x9, #0x90]
    stp  x20, x21, [x9, #0xa0]
    stp  x22, x23, [x9, #0xb0]
    stp  x24, x25, [x9, #0xc0]
    stp  x26, x27, [x9, #0xd0]
    stp  x28, x29, [x9, #0xe0]
    str  x30,      [x9, #0xf0]

    // x21 = runtime address of linux_banner
    adr  x9,  _base
    ldr  x10, l_banner_off
    add  x21, x9, x10

    // banner = "PRB "
    ldr  w10, =0x20504252          // "RPB "
    str  w10, [x21]
    add  x22, x21, #4              // write cursor
    mov  w23, wzr                  // count of interesting hits
    mov  w24, wzr                  // index
    mov  w27, wzr                  // EL2 flag
    adr  x25, _fids

_loop:
    hvc  #0x6000
    mov  x23, x0
    mov  x24, x1
    mov  x25, x2
    mov  x26, x3
    mov  w0,  w23
    mov  x1,  x22
    bl   _hex8
    mov  x22, x1
    mov  w0,  #':'
    strb w0,  [x22], #1
    mov  w0,  w24
    mov  x1,  x22
    bl   _hex8
    mov  x22, x1
    mov  w0,  #':'
    strb w0,  [x22], #1
    mov  w0,  w25
    mov  x1,  x22
    bl   _hex8
    mov  x22, x1
    mov  w0,  #':'
    strb w0,  [x22], #1
    mov  w0,  w26
    mov  x1,  x22
    bl   _hex8
    mov  x22, x1
    mov  w0,  #' '
    strb w0,  [x22], #1
    strb w0,  [x22], #1
    strb w0,  [x22], #1
    b    _after_probe

_after_probe:
    // terminator + EL2 flag
    strb wzr, [x22]                // x22 is 4-aligned here
    cbz  w27, _back_to_el1

// ---------------- takeover path: minimal EL2 state ----------------
    adr  x9,  _base
    msr  vbar_el2, x9              // vectors first, before anything can fault
    isb
    mov  x9,  #1
    msr  hcr_el2, x9               // RW=1
    msr  cptr_el2, xzr
    msr  cnthctl_el2, x9

    adr  x9,  _stack_top
    msr  sp_el2, x9
    adr  x9,  _stack_top
    mov  sp,  x9

    adr  x9,  _exit
    msr  elr_el1, x9               // resume: trampoline direct-branches to ENTRY+4
    mov  x9,  #0x3c5               // EL1h, DAIF masked
    msr  spsr_el1, x9
    mov  w9,  #'2'
    strb w9,  [x22]
    b    _restore

// ---------------- unchanged path: back to kernel at EL1 ----------------
_back_to_el1:
    adr  x9,  _exit
    msr  elr_el1, x9
    mov  x9,  #0x3c5               // EL1h, DAIF masked
    msr  spsr_el1, x9
    b    _restore

// direct-branch back into the kernel (BTI: indirect branches must not target
// a plain instruction, so land on this slot with br and let it `b` directly)
.align 4
_exit:
    .word 0xB0000000               // patched: b ENTRY+4

// ---------------- restore + return ----------------
_restore:
    adr  x2,  _save
    ldp  x0,  x1,  [x2, #0x00]
    ldr  x3,       [x2, #0x18]
    ldr  x4,       [x2, #0x20]
    ldp  x5,  x6,  [x2, #0x28]
    ldp  x7,  x8,  [x2, #0x38]
    ldp  x9,  x10, [x2, #0x48]
    ldp  x11, x12, [x2, #0x58]
    ldp  x13, x14, [x2, #0x68]
    ldp  x15, x16, [x2, #0x78]
    ldp  x17, x18, [x2, #0x88]
    ldp  x19, x20, [x2, #0x98]
    ldp  x21, x22, [x2, #0xa8]
    ldp  x23, x24, [x2, #0xb8]
    ldp  x25, x26, [x2, #0xc8]
    ldp  x27, x28, [x2, #0xd8]
    ldp  x29, x30, [x2, #0xe8]
    ldr  x2,       [x2, #0x10]
    eret

// ---------------- helpers ----------------
// x0 = value, x1 = dest ; writes 8 lowercase hex chars
_hex8:
    mov  w2,  #8
    mov  w3,  #28
2:  lsr  w4,  w0, w3
    and  w4,  w4, #0xF
    cmp  w4,  #10
    b.lo 3f
    add  w4,  w4, #('a' - 10)
    b    4f
3:  add  w4,  w4, #'0'
4:  strb w4,  [x1]
    add  x1,  x1, #1
    subs w3,  w3, #4
    b.ge 2b
    ret

_exc:
    adr  x11, _base
    ldr  x12, l_banner_off
    add  x11, x11, x12
    mov  w13, #'!'
    strb w13, [x11]
    mrs  x0,  esr_el1
    add  x1,  x11, #1
    adr  x30, _exc1
    b    _hex8
_exc1:
    mov  w13, #'#'
    add  x1,  x11, #18
    strb w13, [x1]
    mrs  x0,  elr_el1
    add  x1,  x11, #19
    b    _hex8
_exc2:
    mrs  x9,  elr_el1              // resume after the faulting instruction
    add  x9,  x9, #4
    msr  elr_el1, x9
    eret

.align 3
l_banner_off:  .quad (BANNER - STUB_AT)
.align 3
l_entry_off:   .quad (ENTRY + 4 - STUB_AT)
_fids:
    .word 0x02000121
    .word 0x82000121
    .word 0xC2000121
    .word 0x82000100
    .word 0x82000101
    .word 0xC6000000
    .word 0xC6000001
    .word 0xC6000002
    .word 0x82000000
    .word 0xC2000000
    .word 0xC2000001
    .word 0x82000001
    .word 0x86000000
    .word 0
    .align 4
.align 3
_save:  .space 0x100
_stack: .space 0x2000
_stack_top:

_base:
    .rept 16
    b   _exc
    .space 0x7c
    .endr
    .org 0x800
    adr  x9,  _base
    ldr  x10, l_banner_off
    add  x21, x9, x10
    ldr  w10, =0x31505242          // "BRP1" marker
    str  w10, [x21]
    adr  x9,  _exit
    br   x9
_exit:
    .word 0xB0000000
.align 3
l_banner_off: .quad (BANNER - STUB_AT)
_exc:
    b   _exc

# Harness internals and the four pitfalls that cost cycles

## How the stub is spliced

`patch.py` rewrites the arm64 `Image` in place:

1. the entry word (`0x1000`) becomes `b <stub code>` (the hijacked instruction
   is not replayed; the stub returns to `0x1004`, right after the header's
   two-instruction boot stub)
2. the stub is written into a long zero run inside the Image (`0x1024000`) —
   *not* after `image_size`, the bootloader will not have copied that
3. `kernel_size`/`image_size` are raised if needed (never lowered)
4. `kvm-arm.mode=protected` in the embedded cmdline is switched to `nvhe`

Output goes into `linux_banner` (offset `0x12b8660` — the live copy; the one
at `0x1031108` is not printed). The stub recognises a
`<fid>:<result>` record layout and pads every record to 20 bytes.

## Pitfall 1 — MMU is off, so no unaligned access

With `SCTLR_EL1.M == 0` all data accesses are Device memory and **may not be
unaligned**. Two bugs came from this: `.quad` literals at 4-byte alignment
(a 4-byte code shift is enough), and a string cursor that advanced by odd
record lengths. Every `.quad` now carries `.align 3`; records are 20 bytes.

## Pitfall 2 — BTI

`br` back to `0x1004` aborts: a plain instruction is not a landing pad. The
stub ends with a *direct* `b` at a 4-aligned slot, and `br`/`eret` only target
that slot.

## Pitfall 3 — `x0` is the DTB pointer

A probe loop that overwrote `x0` and returned without restoring it made
`start_kernel` explode before any console existed, i.e. silently, with an
empty pstore. The stub saves `x0..x30` up front and restores all of them on
**both** exit paths.

## Pitfall 4 — clinic on the way down

Useful things learned while bisecting (they also generalise):

* `fastboot boot <img>` is a no-op on this device; the phone boots `boot_a`
  anyway, so the harness has to flash
* ramoops/pstore only covers faults after the console registers (~60 ms);
  everything earlier is invisible, hence the banner as the reporting channel
* an EL2-register read (`mrs esr_el2`) inside an EL1 fault handler traps
  again → endless loop. Use `esr_el1`
* a long `fastboot getvar all` over a monitor USB hub wedges the bootloader;
  plug into the machine directly

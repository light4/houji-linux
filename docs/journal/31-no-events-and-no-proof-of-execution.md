# No backend events, and still no proof that the guest executes

Following `journal/30`: the MMIO writes of a stub guest produce nothing, so the
event path itself was instrumented.

## How to watch the backend properly

The two eventfds are the real signal path: `irqfd` (driver → guest interrupt) and
`ioeventfd` (guest kick → driver). Reading them is what `qcrosvm` does instead of
waiting on any ioctl. arm64 has **no `poll(2)`** — use `ppoll` or non-blocking
eventfd reads (`eventfd2` with `EFD_NONBLOCK`); an `EFAULT`/`EINVAL` from a
mishandled `poll` wastes a cycle.

Result: **both eventfds stay empty** (`EAGAIN` on every read) throughout a run,
and `GH_WAIT_FOR_EVENT` never returns either. The driver never sees the guest.

## And still no proof the guest executes

A stub that reads an *unbacked* address (`0xbeef0000`) before looping survives as
long as one that reads the real window (~8–10 s), so the read does not fault.
That is consistent with the guest never running a single instruction: neither a
valid nor an invalid access leaves a trace.

Setting the kernel segment's `p_vaddr`/`p_paddr` to the guest memory base
(`0x28800000`, matching the DT's `memory` node) changed nothing measurable
(~0.5 s, as before), although that is still the most plausible mapping
expectation.

## Where this points

Everything is now consistent with: the image is loaded and authenticated, vCPUs
are created and the loader reports `started running`, but **no vCPU ever
executes**; an idle VM is dropped after ~10 s and the vendor kernel's extra work
makes it die faster. The next step has to be reading the loader's own start path
(`gunyah_loader`, the code that emits `VM:%d started running`) to see what it
does with the segments, the entry point and `VCPU_RUN` — the hypothesis space on
the userspace side is exhausted.

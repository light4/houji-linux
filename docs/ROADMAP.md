# Roadmap

Host side is done (see [STATE](STATE.md)). Everything below is guest side or
verification.

## Open questions, in order

1. **Why does the guest die at entry?** No fault is logged, the reset comes
   <0.5 s after `VCPU_RUN` and no device is ever touched. Candidates:
   the bundle's guest does not survive this hyp/kernel combination; the `b02`
   template needs generation into a real DT; the image expects a boot mode we
   never set.
2. **Host-side verification of the last unknowns** — nothing left that userspace
   can set (no boot-context ioctl, DT overlays unused).
3. **Console** — needed for every further step; today the only channel is the
   loader's own log.

## Concrete next moves

* ~~`reg`/`interrupts` in `b02`~~ — done, no effect (`journal/25`).
* ~~drop `crash-fatal`~~ — no effect, the reset is a hard reset from the hyp/SCM
  side, not a panic (`journal/26`, `journal/27`).
* watch `/proc/vmkmsg` as soon as a guest survives long enough to write to it
  (`journal/28`).
* stub guest already proves the point (`journal/29`): make it touch the virtio
  window (`ldr` of the magic at `0xda6f8000`), then loop — if that keeps the VM
  alive the device is reachable and only the handshake is missing.
* try arming **one** label only, to separate "any backend at all" from the setup
  order.
* bring our own guest image: a minimal aarch64 stub is enough to prove execution
  (it can write to `vmkmsg`, or just busy-loop past the current ~0.5 s), and it
  is on the way to the real goal anyway.
* finish the `GH_GET_QUEUE_INFO` layout — it is the only non-blocking progress
  signal, and `EINVAL` may just mean "not ready yet".
* alternatively replace the bundle's kernel with our own and use an initramfs
  that prints over `hvc0` regardless of what else is missing.
* keep `qcrosvm` as the oracle: if a newer ROM ships, its ABI (`0xBCxx`,
  `0x4020B245`) is what the bundle was built against — re-check the version skew
  first after any update.
* only after a console exists: rootfs, then the `gh_virt_wdt` keep-alive.

## Parked

* crosvm / pVM route: needs a vendor-signed pvmfw; parked until one appears
  (`journal/11`, `journal/13`).
* `/dev/kvm`: closed on this firmware (`journal/01`, `journal/03`,
  `journal/06`). Re-check with the tripwire in the root README.

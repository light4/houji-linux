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

* patch `trustedvm.b02` into a static guest DT: give both `virtio-mmio` nodes
  real `reg` and `interrupts`, and drop `console=hvc0` from `chosen.bootargs`
  while at it. The bundle container and re-sign flow are in
  [reference/bundle-format](reference/bundle-format.md).
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

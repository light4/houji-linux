# Mimicking the working VM's config: still 0.2 s

Applied every difference to `cpusys_vm` that can be expressed in the guest DT and
the bundle, in one shot:

* `vdevices` stripped of `virtio-mmio@0/1` and `swiotlb-shm` (no virtio at all);
* `vm-attrs = "no-shutdown", "crash-fatal", "ras-error-handler"`;
* `chosen.bootargs = "nokaslr"` (no console);
* kernel segment `p_vaddr`/`p_paddr = 0x28800000` (the DT's memory base, as
  `cpusys_vm` does).

Result: guest still gone in ~0.2 s (2 samples from an internal 100 ms logger), no
backend events. Relative to all previous variants this changes nothing.

So the difference to a booting VM is not in the guest DT, not in the bundle's
segment addressing, and not in the attributes we can set — at least not in any
combination we can reach. That leaves three things we cannot influence from
userspace:

1. the host-side VM template the hyp builds the VM from,
2. the loader's start path after `VCPU_RUN`,
3. something the vendor's own launcher does that our tool does not, in a way that
   never shows up as an ioctl the driver implements.

## Realistic next steps

* run `qcrosvm` again now that we know more (its `-l` path needs
  `/sys/kernel/load_guestvm_<vm>`, which the loader only creates in that mode) and
  try to get *it* to boot `trustedvm` — if the vendor launcher also fails, the
  ROM/hyp combination is the limit, not our code;
* or accept the vendor path being closed on this build and revisit after a
  firmware update, watching for `/sys/kernel/load_guestvm_*` and for
  `0x4020B245`/`0xBCxx` support (the newer ABI the launcher expects).

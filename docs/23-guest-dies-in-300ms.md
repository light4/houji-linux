# The guest dies in about 300 ms — and the ROM may be older than the bundle

Measurements this round:

* the tool now samples every 200 ms after `VCPU_RUN`; only the first sample
  (~200 ms) is ever written, so the VM is gone in under half a second.
* the log still ends at `started running` — **no kernel panic output at all**.
  A `panic()` from the VM notifier would print a stack; instead we get a hard
  reset, which is what a guest **triple fault** looks like from the host side.
* `GH_CREATE_VM` with argument `1` instead of `0` changes nothing (both create
  the VM fine), so the create argument is not the missing piece.
* `set_fw_name("cpusys_vm")` fails: only `trustedvm` resolves
  (`/product/vm-system/trustedvm/boot/trustedvm`); `cpusys_vm` is not under
  `/product/vm-system` at all, so it cannot be used as a control.

## New hypothesis: driver older than the bundle

`qcrosvm` — the launcher the ROM ships — uses `0x4020B245` right after
`GH_CREATE_VM` and the whole `0xBCxx` command family. On this driver both answer
`EINVAL`; only the legacy `0xB2xx` numbers work. That means the shipped
launcher was built for a **newer** driver ABI than this kernel has.

`trustedvm` also has no entry in `/vendor/etc/qvirtmgr-vndr.json`, i.e. nothing
on this ROM ever boots it. Taken together: the SVM path may be *not wired up*
on this build, and the guest dying at entry would then be a version/entry-state
mismatch rather than our sequence.

## Where the entry state could come from

An AArch64 Gunyah guest is started by the hyp with a defined boot context
(PC from the image, `x0` = FDT, `x1`..`x3` zeroed). If the DT handoff is missing
the guest faults on the first `x0` dereference — ~300 ms is exactly that scale,
and it explains why no device is ever touched. Candidates we have not yet
checked:

* `0xB243` is "get vcpu count", but there may be a *set* counterpart we never
  found (0xB244?) carrying pc/fdt;
* the DT the guest should receive may have to be registered through
  `GH_SET_DEVICE_CONFIG_DATA` on a *dtb* device, or through the `0xB26C` slot we
  never used.

## Practical next step

Enumerate the remaining `0xB2xx` numbers that exist in the driver: send
`0xB241`..`0xB27F` with a scratch buffer and log which ones answer something
other than `EINVAL`/`ENOTTY`. That gives the real list of implemented ioctls,
which is the only reliable way to find a missing "boot context" call.

# Roadmap

Three options for running Arch/NixOS are compared in [OPTIONS](OPTIONS.md). This
page tracks option 3 (hardware virtualization); options 1 and 2 have their plans
there.

## The one open question

**Why does the RM refuse `VM_START` (`NORESOURCE`) for an unauthenticated crosvm
VM on `/dev/gunyah`?** That path is the only one that can run our own kernel
(`journal/42`). Configure and init succeed; start fails, with or without pVM
options and vm-config overlays.

The vendor-loader (`/dev/qgunyah`) path is closed for custom code: bundles are
authenticated against Xiaomi's root, `qtestsign` is rejected (`journal/42`).

## Next experiments on this ROM

* bisect crosvm's generated `gunyah-vm-config` against `cpusys_vm`'s working one
  (vcpus `affinity`/`affinity-map`, memory base, vdevices) via
  `--device-tree-overlay`, since the RM parses that node;
* try `--cpus` / memory sizes / no virtio devices to see whether `NORESOURCE`
  tracks a quota;
* get an RM log: `/sys/kernel/debug/gunyah/hyp_uart_ctrl` and `/proc/tzdbg/hyp_log`
  are present but silent on this production hyp.

## If kernel RE continues

* read the loader's start path in `gunyah_loader` (around `gh_vcpu_ioctl`'s
  `VCPU_RUN` branch, `.text+0x14e8`): what it does with the segments, the entry
  offset and the run hypercall (`journal/32`).
* diff against `cpusys_vm`'s start, the only known-good VM on this device
  (`journal/35`).

## If a newer ROM arrives

Work the tripwires first (root README): `mssecapp` in `hyp_a`, Gunyah in the AVF
APEX, `/sys/kernel/load_guestvm_*`. Then, in order:

1. re-check the ABI: `0x4020B245` and the `0xBCxx` family should stop answering
   `EINVAL` (`journal/21`, `journal/23`);
2. re-run the stock `qcrosvm` on `trustedvm` (`journal/41`) — it is the
   reference, and it already works on this ROM for `cpusys_vm`;
3. `GH_GET_QUEUE_INFO` and `ACK_DRIVER_OK` should exist — both are the observables
   we lacked (`journal/21`, `journal/24`);
4. only then: guest console, rootfs, `gh_virt_wdt` keep-alive.

## Build conventions from the working bundle

`cpusys_vm.mdt` shows what the loader expects (`journal/38`): `p_paddr` = the
guest memory base, 2 MB-aligned segment offsets, `PT_LOAD` flags `0x6`. Keep that
in any future bundle, even though it is not sufficient on its own.

## Parked routes

* crosvm / pVM on `/dev/gunyah`: needs a vendor-signed pvmfw; `RM start` →
  `ENODEV` (`journal/11`, `journal/13`).
* `/dev/kvm`: closed on this firmware — EL2 belongs to Gunyah (`journal/01`,
  `journal/03`, `journal/06`).

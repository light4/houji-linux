# Roadmap

Parked on this firmware — see [FINDINGS](FINDINGS.md). Everything here needs
either a newer ROM or kernel RE beyond the reach of userspace.

## The one open question

**Why does `trustedvm` die right after `started running`?** It does so with our
probe *and* with the vendor launcher (`journal/41`), while `cpusys_vm` runs on the
same driver. Leading hypothesis: it needs the Trusted UI hand-off of display/touch
(`iomemory-ranges`) before start.

Ruled out, each with a recorded experiment: guest kernel content (`journal/29`),
`reg`/`interrupts` in the DT (`journal/25`), `crash-fatal` (`journal/26`),
affinity proxy/pinned/none (`journal/33`, `journal/34`), a full mimicry of the
working VM's configuration (`journal/39`), adding/removing devices in the bundle
(`journal/36`), kernel `p_paddr` (`journal/38`), the overlay (`journal/19`).

## Next experiments on this ROM

* start `trustedvm` the way the product does — through the Trusted UI flow
  (`vendor.xiaomi.hardware.mitrustedui-service` → TUI HAL) — with
  `/data/misc/vm-system/system.img` provided, and see whether the hand-off changes
  the outcome;
* strip `iomemory-ranges`/`gic-irq-ranges` from the guest config (re-signed
  `b02`) so the VM no longer claims host devices;
* or build a VM shaped like `cpusys_vm` (no lent devices) under a name the loader
  accepts.

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

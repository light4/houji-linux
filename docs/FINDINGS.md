# Findings — one page

Everything below was verified on device (houji / 23127PN0CC, HyperOS 3.0.7.0,
KernelSU). The device matches the stock `OS3.0.7.0.WNCCNXM` fastboot ROM
partition-for-partition, except KernelSU's `init_boot`
([journal/41](journal/41-stock-launcher-same-failure.md)). Numbers, commands and raw logs are in the journal; where an early
journal entry was later corrected, the correction is linked too.

## 1. `/dev/kvm` cannot be had on this firmware

KVM needs EL2; EL2 belongs to Qualcomm's Gunyah hypervisor from XBL onwards.
Both hand-back routes are closed:

* Secure Launch (the `slbounce` approach) is not implemented — no `mssecapp`, no
  `hyp_manager_launch` in `hyp_a`/`xbl_a`/`tz_a`/`devcfg_a`; the config field
  `0x221194` has zero readers ([journal/01](journal/01-secure-launch-absent.md)).
* Gunyah's host ABI (`hvc 0x6000+`) is VM management only — no "give EL2 back"
  call ([journal/03](journal/03-gunyah-hvc-abi.md)).

AVF is additionally blocked because this build ships **no Gunyah backend**; it is
crosvm-on-KVM, so flipping `protected_vm.supported` only moves the failure to the
missing `/dev/kvm` ([journal/06](journal/06-avf-is-kvm-only.md)).

## 2. The vendor loader route works up to the guest starting

`/dev/qgunyah` drives a real Gunyah VM ([journal/07](journal/07-gunyah-direct.md)):

* `GH_CREATE_VM`, `GH_VM_SET_FW_NAME("trustedvm")`, `GH_CREATE_VCPU`, backend arming
  and `GH_VCPU_RUN` all work; the loader logs `VM:45 started running` and
  `VCPU_RUN` blocks, so the vCPU really is entered ([journal/37](journal/37-vcpu-run-blocks.md)).
* the SVM images are **test**-signed, and re-signing with `qtestsign` is accepted
  — including structurally modified bundles ([journal/09](journal/09-custom-system.md), [journal/25](journal/25-bundle-patched-and-resigned.md)).
  Signing is not the obstacle.
* full backend ABI and its traps (labels are hex, `flags` must be 1, one `mmap`
  per label, `set_fw_name` before `0x8008B261`) are in
  [reference/qgunyah-abi](reference/qgunyah-abi.md).

## 3. …and there `trustedvm` stops

The vCPU is entered but **never makes progress**: no backend event ever fires, no
fault is reported, `GH_GET_QUEUE_INFO` and `ACK_DRIVER_OK` are unimplemented, there
is no vCPU state/register ioctl, `/proc/vmkmsg` stays empty, and `mtdoops` records
no panic — the reset comes from the hyp/RM, not a host panic ([journal/22](journal/22-guest-never-touches-the-window.md),
[journal/24](journal/24-no-boot-context-ioctl.md), [journal/27](journal/27-the-reset-leaves-no-host-trace.md), [journal/31](journal/31-no-events-and-no-proof-of-execution.md), [journal/32](journal/32-where-started-running-comes-from.md)).

The same happens with a guest kernel that is a single `b .` instruction
([journal/29](journal/29-stub-guest-exonerates-the-kernel.md), corrected in [journal/34](journal/34-proxy-does-not-matter.md)), so it is not the vendor kernel.

## 4. What was tried and did not help

`reg`/`interrupts` in the guest DT, removing `crash-fatal`, proxy vs pinned vs no
CPU affinity, `nokaslr`, kernel `p_paddr` set to the memory base, a single-entry
DT table, a full mimicry of the working VM's configuration ([journal/25](journal/25-bundle-patched-and-resigned.md), [journal/29](journal/29-stub-guest-exonerates-the-kernel.md), [journal/38](journal/38-segments-are-placed-by-paddr.md), [journal/39](journal/39-mimicking-the-working-vm.md)).
The device list does not even come from the bundle — it is decided by the hyp/RM
([journal/36](journal/36-backends-do-not-come-from-b02.md)).

## 5. The vendor launcher fails the same way

`/dev/qgunyah` is fine: the stock stack runs `cpusys_vm` from boot via
`/system_ext/bin/qcrosvm --vm=cpusys_vm` (vCPU thread blocked in
`gh_vcpu_ioctl`). For `trustedvm`, the stock on-demand start (Trusted UI →
`vendor.qti.qvirt.IVirtualizationService`) fails earlier, because its system disk
`/data/misc/vm-system/system.img` is absent on this ROM. Running `qcrosvm` by hand
with the untouched bundle and both disks supplied reaches `started running` and the
phone resets — **the same signature as our probe**
([journal/41](journal/41-stock-launcher-same-failure.md), correcting
[journal/40](journal/40-why-qcrosvm-cannot-run-here.md)).

## 6. Honest boundary

Established: the route, the ABI, the bundle formats, the signing story, the DT
layout, the failure signature, and the fact that the failure is host/hyp-side of
anything userspace can reach.

**Not** established: *why* `trustedvm` dies. It is not our tooling (the vendor
launcher fails identically) and not the `/dev/qgunyah` path (`cpusys_vm` runs).
The most plausible cause is that `trustedvm` is a Trusted UI VM: it is lent real
display/touch device ranges (`iomemory-ranges`, `gic-irq-ranges`) and is meant to
start only after the TUI HAL has handed those over. Unproven.

## Tripwires for a firmware update

```bash
adb shell su -c 'strings /dev/block/by-name/hyp_a | grep -E "mssecapp|hyp_manager_launch"'
adb shell su -c 'strings /apex/com.android.virt/bin/virtualizationservice | grep -c gunyah'
adb shell su -c 'ls /sys/kernel/load_guestvm_*'          # newer ABI
```

Any hit re-opens the work; the harness, tools and ABI notes in this repo are the
starting point.

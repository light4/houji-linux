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

## 2. The vendor loader route cannot run custom code

`/dev/qgunyah` drives real Gunyah VMs ([journal/07](journal/07-gunyah-direct.md)),
and the stock stack uses it for `cpusys_vm`. But every VM on that path is a
`secured` VM: its bundle is authenticated against Xiaomi's signing root.
**`qtestsign` re-signing is rejected** (`VM_AUTH_IMAGE` -22) once the patched
bundle is really loaded — earlier "accepted" results came from a silent fallback
to the stock bundle under SELinux enforcing
([journal/42](journal/42-patched-bundles-never-loaded.md), retracting
[journal/09](journal/09-custom-system.md)).

## 3. …and there `trustedvm` stops

The vCPU is entered but **never makes progress**: no backend event ever fires, no
fault is reported, `GH_GET_QUEUE_INFO` and `ACK_DRIVER_OK` are unimplemented, there
is no vCPU state/register ioctl, `/proc/vmkmsg` stays empty, and `mtdoops` records
no panic — the reset comes from the hyp/RM, not a host panic ([journal/22](journal/22-guest-never-touches-the-window.md),
[journal/24](journal/24-no-boot-context-ioctl.md), [journal/27](journal/27-the-reset-leaves-no-host-trace.md), [journal/31](journal/31-no-events-and-no-proof-of-execution.md), [journal/32](journal/32-where-started-running-comes-from.md)).

The same happens with a guest kernel that is a single `b .` instruction
([journal/29](journal/29-stub-guest-exonerates-the-kernel.md), corrected in [journal/34](journal/34-proxy-does-not-matter.md)), so it is not the vendor kernel.

> Note: the experiments in §4 ran the stock bundle without our knowledge
> ([journal/42](journal/42-patched-bundles-never-loaded.md)); they say nothing
> about the patches themselves.

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

## 6. Where a custom OS could still come from

Only an **unauthenticated VM on `/dev/gunyah`** (mainline ABI, crosvm, own kernel)
avoids the signature. On this ROM the Resource Manager rejects its `VM_START` with
`NOMEM` (configure and init succeed). Memory size, IPA base, vCPU count, VM name,
`vm-attrs`, optional devices, doorbell count and stopping the only other VM all
leave it unchanged ([journal/45](journal/45-nomem-sweep.md),
[journal/42](journal/42-patched-bundles-never-loaded.md)).

**Not a dead end:** it is a kernel bug, not the firmware
([journal/46](journal/46-nomem-is-a-known-kernel-bug.md)) — on SM8650 the RM's VMID
is above `QCOM_SCM_MAX_MANAGED_VMID` and `gunyah_qcom.c` hardcodes the HLOS VMID
for every lend, so SCM rejects them all. A 38-line fix and a houji fork with ready
builds exist. Running a patched kernel (an unlocked bootloader allows it) reopens
the path — and the same kernel is what option 1 needs for native Linux.

So on stock firmware: no `/dev/kvm`, no custom code in a secured VM, and the
unauthenticated path needs a kernel patch. Without flashing a kernel, proot-style
containers are the only way to run Arch/NixOS userspace here.

## What next

The three remaining options — native Linux as the primary OS, a real chroot, and
the unauthenticated VM — are compared in [OPTIONS](OPTIONS.md).

## Tripwires for a firmware update

```bash
adb shell su -c 'strings /dev/block/by-name/hyp_a | grep -E "mssecapp|hyp_manager_launch"'
adb shell su -c 'strings /apex/com.android.virt/bin/virtualizationservice | grep -c gunyah'
adb shell su -c 'ls /sys/kernel/load_guestvm_*'          # newer ABI
```

Any hit re-opens the work; the harness, tools and ABI notes in this repo are the
starting point.

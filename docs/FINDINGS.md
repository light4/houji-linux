[English](FINDINGS.md) | [中文](FINDINGS.zh.md)

# Findings — one page

Everything below was verified on device (houji / 23127PN0CC, HyperOS 3.0.7.0,
KernelSU). The device matches the stock `OS3.0.7.0.WNCCNXM` fastboot ROM
partition-for-partition, except KernelSU's `init_boot`
([journal/41](journal/41-stock-launcher-same-failure.md)). Numbers, commands and raw logs are in the journal; where an early
journal entry was later corrected, the correction is linked too.

Since [journal/59](journal/59-global-rom-and-debian-sid.md) the phone runs the Global ROM
`OS3.0.304.0.WNCMIXM` (host kernel 6.1.138, same hypervisor `gunyah-6de79709c`).
The tripwires below stay negative and the `/dev/gunyah` route (section 6)
works unchanged there.

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

## 6. The unauthenticated VM on `/dev/gunyah` works

Only an **unauthenticated VM on `/dev/gunyah`** (mainline ABI, crosvm, own kernel)
avoids the signature, and it runs on the **stock** kernel:

* **The error was misread.** `VM_START` answers `Error: 2`, which is
  `NORESOURCE` in both the kernel's `rsc_mgr.c` and the RM source — not `NOMEM`
  as decoded from journal/45 on ([journal/51](journal/51-first-linux-guest-boots.md)).
* **The VMID kernel bug does not apply here.** Tracing shows every
  `qcom_scm_assign_mem` succeeding; the vendor `gunyah_qcom` already maps guest
  VMIDs above 0x3F, and the host's own VMID is 3 (= HLOS), so the 38-line patch is a
  no-op. The published "fix" kernels do not even build it
  ([journal/48](journal/48-releases-do-not-contain-the-fix.md),
  [journal/50](journal/50-vmid-patch-is-a-noop.md)).
* **The real cause**: the Resource Manager (open source,
  `quic/gunyah-resource-manager`, same strings as `hypvm*.mbn`) requires the
  guest's main memory parcel to be **lent**. `GH_VM_SET_USER_MEM_REGION` *shares*
  it; `process_image_memparcel()` denies that, and `VM_START` reports any such
  failure as `NORESOURCE`. DroidVM's crosvm lends
  (`--protected-vm-without-firmware`) → `VM_START` succeeds and a Linux kernel boots
  ([journal/51](journal/51-first-linux-guest-boots.md)).
* **Memory ceiling = fragmentation.** Beyond ~256 MiB the lend fails after some
  uptime (tens of thousands of 16 KiB fragments). `gh-hugepage-reserve` holds
  2 MiB pages from early boot; with it the guest gets 1.5 GiB
  ([journal/54](journal/54-droidvm-fork-or-not.md), [journal/57](journal/57-hugepage-reserve-1536m.md)).
* **Result**: Arch Linux ARM with systemd, network and `pacman -Syu`, managed by
  the DroidVM app ([journal/53](journal/53-arch-linux-arm-boots.md)–[58](journal/58-pseudo-unprotected-nokaslr.md)).

So on stock firmware: no `/dev/kvm`, no custom code in a secured VM — but an
unauthenticated Gunyah VM with our own kernel works without flashing anything
beyond root.

## What next

Current state: [STATE](STATE.md). The three routes — native Linux, a real chroot,
and the Gunyah VM — are compared in [OPTIONS](OPTIONS.md); remaining work is in
[ROADMAP](ROADMAP.md).

## Tripwires for a firmware update

```bash
adb shell su -c 'strings /dev/block/by-name/hyp_a | grep -E "mssecapp|hyp_manager_launch"'
adb shell su -c 'strings /apex/com.android.virt/bin/virtualizationservice | grep -c gunyah'
adb shell su -c 'ls /sys/kernel/load_guestvm_*'          # newer ABI
```

Any hit makes the `/dev/kvm` and vendor-loader routes worth another look; the
harness, tools and ABI notes in this repo are the starting point.

Last run: Global `OS3.0.304.0.WNCMIXM` (journal/59): no hit on any of the three.

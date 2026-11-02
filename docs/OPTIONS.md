[English](OPTIONS.md) | [中文](OPTIONS.zh.md)

# Options for running Arch Linux / NixOS on houji

Three ways forward after [FINDINGS](FINDINGS.md). Only option 3 is hardware
virtualization; options 1 and 2 follow the route LineageOS and Droidian take.
**Option 3 works today** on the stock kernel: Arch Linux ARM runs as a Gunyah
guest under DroidVM ([journal/51](journal/51-first-linux-guest-boots.md)–[58](journal/58-pseudo-unprotected-nokaslr.md)).
Background on the kernel and Droidian is in
[journal/43](journal/43-kernel-config-and-droidian.md).

## Why LineageOS-style works and our VMs did not

Everything on this phone runs under Gunyah; there are two verification chains:

| | primary VM (HLOS = Android) | secondary VMs (`trustedvm`, `cpusys_vm`) |
|---|---|---|
| loaded by | XBL → ABL from `boot`/`vendor_boot`/`super` | `gunyah_loader` at runtime |
| verified by | AVB | hyp/TZ PIL, Xiaomi signing root |
| after bootloader unlock | not enforced (orange state) — any kernel boots | **still enforced** |

LineageOS replaces the left column and reuses all signed vendor firmware
unchanged. It does not get `/dev/kvm` or custom secondary VMs either.

## Comparison

| | 1. Native Linux as the primary OS | 2. Android kernel + real chroot/container | 3. Gunyah VM (`/dev/gunyah`, crosvm/DroidVM) |
|---|---|---|---|
| hardware virtualization | no (still Gunyah's primary VM, owns EL1) | no (shares Android's kernel) | **yes** |
| independent system | yes, own kernel and init | no, isolated userspace only | yes, own kernel and systemd |
| state | not started | feasible, not set up | **working**: Arch, 4 vCPU, 1.4 GiB, network, `pacman -Syu` |
| host changes | custom `boot` (+`vendor_boot`) | root | root (KernelSU), `gh-hugepage-reserve` + DroidVM runtime modules; stock kernel |
| performance | native | native | near-native CPU; virtio I/O |
| hardware | phone hardware, see "drivers" below | Android's, via its kernel; no display of its own | virtio only: disk, net, serial, (gpu/input/sound need guest kernel v3) |
| impact on Android | replaces it (or dual boot) | none | none; Android runs alongside |
| risk / rollback | medium — flash `boot`(+`vendor_boot`); stock backups restore | low — files in `/data` | low — userspace + KSU modules; disable module, reboot |
| effort | weeks–months | hours | done (days, incl. the dead ends) |

### Pros and cons

**1. Native Linux as the primary OS**

* \+ full native performance and the phone's own hardware: display, GPU (turnip on
  KGSL), touch, battery, USB, Wi-Fi;
* \+ a real phone OS, not a guest inside Android;
* − Android is gone (or dual boot), and with it modem, camera, audio, fingerprint
  unless Halium/libhybris is added;
* − kernel must be rebuilt (`DEVTMPFS`, `FHANDLE`, namespaces) and stay CRC-compatible
  with 300+ vendor modules (`MODVERSIONS`, `TRIM_UNUSED_KSYMS`);
* − no Linux port for houji yet (a native XNU port exists, see below); weeks to
  months of bring-up; every flash risks a non-booting phone (recoverable via
  fastboot backups).

**2. Android kernel + real chroot/container**

* \+ fastest to get: hours, files in `/data`, nothing flashed;
* \+ native speed (no ptrace like proot), direct access to Android's network and storage;
* − not an independent system: Android's kernel, no own init — systemd cannot be
  PID 1;
* − the stock kernel lacks `PID_NS`/`USER_NS`: no systemd-nspawn/LXC, no Nix sandbox;
* − no display of its own (VNC/X server app only), shares Android's lifecycle.

**3. Gunyah VM**

* \+ real hardware virtualization with our own kernel and a full systemd Arch,
  isolated from Android, on the **stock** host kernel;
* \+ Android stays fully usable; start/stop/console in the DroidVM app; ssh from a PC;
* \+ rollback is trivial (stop the VM, disable the KSU modules);
* − virtio devices only: no direct phone hardware (camera, modem, sensors); GPU
  acceleration is DroidVM's virgl/gfxstream path, not native;
* − guest RAM comes from a boot-time 2 MiB page pool (`gh-hugepage-reserve`, 2 GiB
  held from Android); without it the ceiling is ~256 MiB after uptime;
* − depends on DroidVM's crosvm fork and test builds, and on root;
* − after a reboot the phone must be unlocked once before a VM can start (CE storage);
* − quirks found: `nokaslr` needed in pseudo-unprotected mode, custom guest kernel
  (no modules) maintained by us.

## Option 1 — native Linux with the existing kernel and drivers

Reuse the GKI kernel line (`6.1.118-android14`) and Xiaomi's vendor modules (319
in `vendor_dlkm`, plus `vendor_boot`), replace Android's userspace.

**Kernel gaps** (from `/proc/config.gz`), all blocking systemd and therefore both
Arch and NixOS:

| option | stock | needed for |
|---|---|---|
| `DEVTMPFS` | off | systemd/udev (hard requirement) |
| `FHANDLE` | off | systemd (hard requirement) |
| `PID_NS`, `USER_NS` | off | nspawn, containers, Nix sandboxed builds |
| `SYSVIPC` | off | various programs |
| `VT`, `AUTOFS_FS` | off | optional |

So the kernel must be rebuilt from ACK `android14-6.1` with those options while
keeping the vendor modules loadable. The constraint: `MODVERSIONS` and
`TRIM_UNUSED_KSYMS` are on, modules check symbol CRCs, `MODULE_SIG_FORCE` is off
(signatures are not the issue, CRCs are). `DEVTMPFS`/`FHANDLE`/`PID_NS` are
likely KMI-neutral; `SYSVIPC`/`USER_NS` are riskier.

**Drivers** — kernel side mostly present, userspace is the problem:

| hardware | kernel | plain Linux userspace |
|---|---|---|
| display | `/dev/dri/card0`, DSI-1, SDE atomic KMS | DRM/KMS compositors (weston, sway); downstream quirks possible |
| GPU | `/dev/kgsl-3d0` (Qualcomm KGSL, not freedreno) | Mesa turnip has a KGSL backend (Vulkan, GL via zink), or libhybris |
| touch, keys | evdev | works |
| battery/charging | `power_supply` sysfs | works |
| USB network/serial | configfs gadget | works — first target for SSH |
| Wi-Fi | `kiwi_v2`, cfg80211/nl80211 | probably, may need `cnss-daemon` |
| audio, modem, camera, fingerprint, sensors | present | need Android HALs → Halium/libhybris |

**Droidian** already maintains this route: `linux-android-common-6.1-android14`
(ACK 6.1.174 + fragments enabling `DEVTMPFS`, `PID_NS`, `USER_NS`, `SYSVIPC`,
`VT`, `CGROUP_PIDS`, …; `FHANDLE` not seen in the fragments). It has no houji or
SM8650 port, and it is Debian, not Arch/NixOS — reusable are the kernel recipe
and the Halium/libhybris stack.

**Plan**

1. offline: compare Droidian's 6.1 kernel exports (`Module.symvers`) with the CRCs
   the stock vendor modules require; confirm `FHANDLE`;
2. build an ACK `android14-6.1` kernel at the stock config first and check that all
   vendor modules load, then add options one at a time;
3. minimal initramfs: load vendor modules, mount an Arch Linux ARM root, USB
   network + SSH only;
4. then display + GPU (turnip on KGSL), then Wi-Fi; Halium only if audio/modem are
   wanted.

Only `boot_a` (and maybe `vendor_boot_a`) is flashed; see [RESTORE](RESTORE.md).

**New prior art (2026-11):** [UniXNU-houji](https://github.com/lonzzi/UniXNU-houji)
boots a native iOS 16 / XNU on houji through a **Mu-Silicium UEFI houji package**
(RAM-only, packed as a boot image — testable with `fastboot boot`, nothing
flashed), at EL1 like this option. It also points to **Xiaomi's official kernel
and DTS sources** (MiCode `shennong-u-oss`). Two consequences for option 1:
(a) rebuild kernel *and* vendor modules from MiCode's source, so no CRC matching
is needed; (b) or boot a Linux kernel through that UEFI, with a houji device tree
derived from MiCode's DTS. See [reference/links](reference/links.md).

## Option 2 — Android kernel + real chroot/container

KernelSU root allows a real chroot of Arch Linux ARM or a Nix install, native
speed (no ptrace like proot). Limits come from the same kernel config: without
`PID_NS`/`USER_NS` there is no systemd-nspawn/LXC and no Nix sandbox; systemd
cannot be PID 1. Good for quick, practical use; not an independent system and not
virtualization.

## Option 3 — Gunyah VM on `/dev/gunyah`

The only route that is hardware virtualization with our own kernel. History in
[journal/42–50](journal/): the vendor loader only boots signed VMs; the
unauthenticated path failed at `VM_START`, first misread as `NOMEM` and blamed on
a VMID kernel bug whose patch turned out to be a no-op here.

What actually works ([journal/51](journal/51-first-linux-guest-boots.md) onwards):

* **cause**: the RM requires the guest's main memory parcel to be **lent**, not
  shared; `GH_VM_SET_USER_MEM_REGION` shares it, so `VM_START` returned
  `NORESOURCE`. DroidVM's crosvm lends (`--protected-vm-without-firmware`) or uses
  its pseudo-unprotected shim;
* **memory**: beyond ~256 MiB the lend fails on fragmentation; `gh-hugepage-reserve`
  supplies 2 MiB pages from a boot-time pool ([journal/57](journal/57-hugepage-reserve-1536m.md));
* **guest**: ACK 6.1 `defconfig` kernel + virtio, Landlock, PL030 RTC
  (`guest/build_guest.sh`), Arch Linux ARM ext4 disk (`guest/mk_alarm.sh`);
* **management**: DroidVM app, NAT bridge over the phone's Wi-Fi.

Two VM modes, both working:

| mode | pros | cons |
|---|---|---|
| `pseudo_unprotected` (DroidVM default, in use) | no swiotlb: +58 MiB RAM, no bounce for virtio I/O; stock distro kernels possible | needs `nokaslr` with our kernel ([journal/58](journal/58-pseudo-unprotected-nokaslr.md)); relies on the DroidVM shim |
| `protected_without_firmware` | simplest path, no shim | guest kernel needs `DMA_RESTRICTED_POOL`; swiotlb (64 MiB) taken from guest RAM, virtio I/O bounces |

## Recommendation

* **use option 3** — it is working: Arch Linux ARM in a Gunyah guest next to
  Android, managed in DroidVM. Next steps in [ROADMAP](ROADMAP.md);
* option 2 remains a quick fallback (hours to set up) when no VM is wanted;
* option 1 only if the goal becomes replacing Android, starting with the offline
  CRC check of the vendor modules.

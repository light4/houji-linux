# Options for running Arch Linux / NixOS on houji

Three ways forward after [FINDINGS](FINDINGS.md). Only option 3 is hardware
virtualization; options 1 and 2 follow the route LineageOS and Droidian take.
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

| | 1. Native Linux as the primary OS | 2. Android kernel + real chroot/container | 3. `/dev/gunyah` + crosvm unauthenticated VM |
|---|---|---|---|
| hardware virtualization | no (still Gunyah's primary VM, owns EL1) | no (shares Android's kernel) | **yes** |
| independent system | yes, own kernel and init | no, isolated userspace only | yes, own kernel |
| doable today | yes, proven route | yes | **no** — RM rejects `VM_START` with `NORESOURCE` |
| main difficulty | kernel config + hardware enablement | almost none | unknown cause, no RM log |
| performance | native | native | near-native (virtio IO) |
| hardware | see "drivers" below | no display/GPU of its own | virtio only: disk, net, serial; no GPU |
| impact on Android | replaces it (or dual boot) | none | none |
| risk / rollback | medium — flash `boot`(+`vendor_boot`); stock backups restore | low — files in `/data` | low — userspace only |
| effort | weeks–months | hours | unbounded: days, or never |

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

## Option 2 — Android kernel + real chroot/container

KernelSU root allows a real chroot of Arch Linux ARM or a Nix install, native
speed (no ptrace like proot). Limits come from the same kernel config: without
`PID_NS`/`USER_NS` there is no systemd-nspawn/LXC and no Nix sandbox; systemd
cannot be PID 1. Good for quick, practical use; not an independent system and not
virtualization.

## Option 3 — unauthenticated VM on `/dev/gunyah`

The only route that is hardware virtualization with our own kernel. Status
([journal/42](journal/42-patched-bundles-never-loaded.md)): configure and init are
accepted, `VM_START` returns `NORESOURCE`; the RM does parse crosvm's
`gunyah-vm-config` (`pinned`/`static` affinity fail init), but proxy affinity,
affinity maps and dropping optional devices do not change the start failure.
Next suspect: memory lending (crosvm lends host user pages; the working VMs use
carve-outs). The hyp contains "PVM only variant" strings, so it may simply be
forbidden on this firmware. Worth a bounded effort; otherwise wait for a firmware
update.

## Recommendation

* usable Arch/NixOS soon → option 2 now;
* a real independent system → option 1, starting with the offline CRC check;
* option 3 → time-boxed experiments only.

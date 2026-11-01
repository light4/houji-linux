# Kernel config, existing drivers, and Droidian

Groundwork for running Arch/NixOS as the primary OS ([OPTIONS](../OPTIONS.md)).

## Stock kernel config (`/proc/config.gz`)

`6.1.118-android14-11-gca0ef6d17716-ab13624819` (Google-built GKI).

* on: `CGROUPS`, `MEMCG`, `CGROUP_FREEZER`, `CGROUP_BPF`, `BPF_SYSCALL`,
  `SECCOMP_FILTER`, `NET_NS`, `UTS_NS`, `EXT4_FS`, `OVERLAY_FS`, `FUSE_FS`,
  `DRM`, `INPUT_EVDEV`, `USB_CONFIGFS*`, `IPV6`
* **off**: `DEVTMPFS`, `FHANDLE`, `PID_NS`, `USER_NS`, `SYSVIPC`, `VT`,
  `AUTOFS_FS`, `CGROUP_PIDS`, `BTRFS_FS`
* modules: `MODVERSIONS=y`, `TRIM_UNUSED_KSYMS=y`, `MODULE_SIG=y`,
  `MODULE_SIG_FORCE` off; 319 vendor modules in `/vendor_dlkm/lib/modules`

Device nodes present: `/dev/dri/card0` + `renderD128` (connectors DSI-1, DP-1,
Virtual-1/2, SDE CRTCs), `/dev/kgsl-3d0`.

## Droidian

* Debian + Phosh, Halium/libhybris, Android 9+ devices; active (releases
  `101.20251130`, `101.20261023`, nightlies).
* device list (devices.droidian.org): 33 devices; no Xiaomi 14 / SM8650; newest
  Qualcomm are Lenovo ThinkPhone (`bronco`, SD 8+ Gen 1) and Xperia 5 II.
* `droidian-devices/linux-android-common-6.1-android14`: packaging for
  `droidian-devices/android_kernel_common` (ACK 6.1.174, `gki_defconfig` +
  `droidian-devices/common_fragments`: `droidian.config`, `halium.config`,
  `container.config`; `6.1-android14.config` is empty). Fragments enable
  `DEVTMPFS`, `PID_NS`, `USER_NS`, `SYSVIPC`, `VT`, `CGROUP_PIDS`,
  `POSIX_MQUEUE`, `SQUASHFS`, `VETH`, …, cmdline includes `selinux=0`;
  `FHANDLE` does not appear.
* porting guide: `droidian/porting-guide` (`kernel-compilation.md`; boot image
  header v4 for GKI devices).

## Open

* whether the stock vendor modules' symbol CRCs match a kernel built with those
  fragments (`MODVERSIONS` + `TRIM_UNUSED_KSYMS`);
* how Droidian gets `FHANDLE`;
* which shipping device actually uses the common 6.1 kernel.

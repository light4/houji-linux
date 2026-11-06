[English](RESTORE.md) | [中文](RESTORE.zh.md)

# Restore / rollback

Nothing except `boot_a` was ever flashed during the harness work, and the run
that installed the dtbo overlay only ever touched `dtbo_a`. Both stock images
are byte-identical backups.

```bash
fastboot flash dtbo_a ~/Downloads/houji_backup/dtbo_a.img
fastboot flash boot   ~/playground/houji-linux/baseline/boot_a_OS3.0.7.0_orig.img
```

| partition | stock sha256 |
|---|---|
| `dtbo_a` | `6264b8e66e2e7720e9f593f93f9b7390ccbe5675c04d15c0c13abf1ff327456b` |
| `boot_a` | `c88537b854911c142a920989865b663a5b7e0b3faab7025a7858b7a7adddafa9` |

## Current device: Global ROM (journal/59)

Since journal/59 the phone runs the **Global** ROM `OS3.0.304.0.WNCMIXM`, flashed
with its `flash_all.sh`. The China `OS3.0.7.0` stock images and backups in the
sections below belong to the old ROM; do **not** flash them onto the Global
system piecemeal. To go back, reflash a whole fastboot ROM.

| partition | on device | sha256 |
|---|---|---|
| `boot_a` | Global stock `boot.img` | `c1df90a5915dad7044eed6b3ac897ba4898f040f15e9cd1670d74b0def89d1ff` |
| `init_boot_a` | Global `init_boot.img` + KernelSU 3.3.1 LKM | `6cc8f04b6f6469f573815700815db15a0142cb86ea802fc59b6f74c9362f352f` (stock: `2c7888fe…cbd064`) |
| `dtbo_a` | Global stock `dtbo.img` (first 20 MiB of the partition) | `a5a54a5aaafbf240dffd2fe16b61846b3ef770447f6b23f4de08717268a4b843` |

Files on the build host, `~/Downloads/houji_rom/`:

| file | what |
|---|---|
| `houji_global_images_OS3.0.304.0.WNCMIXM_20260908.0000.00_16.0_global_8f1c952b2d.tgz` | Global fastboot ROM (current), md5 `8f1c952b2d74e9ed33e41a95aff60943` |
| `houji_eea_global_images_OS3.0.304.0.WNCEUXM_…_414730a1d8.tgz` | EEA fastboot ROM (spare) |
| `rom.tgz` | China fastboot ROM `OS3.0.7.0.WNCCNXM` (the pre-journal/59 system) |
| `ksu/kernelsu_patched_20261106_060206.img` | the KernelSU `init_boot` above |
| `backup/20261106_OS3.0.304.0.WNCMIXM/` | EFS/modem and calibration partitions (`modemst1/2`, `fsg`, `fsc`, `persist`, …), sha256 in `SHA256SUMS` |

```bash
fastboot flash init_boot_ab ~/Downloads/houji_rom/houji_global_images_OS3.0.304.0.WNCMIXM_16.0/images/init_boot.img   # drop KernelSU
fastboot flash init_boot_ab ~/Downloads/houji_rom/ksu/kernelsu_patched_20261106_060206.img                           # KernelSU again
```

Never relock the bootloader: China hardware on a Global ROM plus a patched
`init_boot` will not boot locked.

## Kernel / root (China 3.0.7.0, until journal/58)

State after `journal/48`: **stock kernel** (`boot_a` = the stock image,
sha256 `c88537b8…`) and **KernelSU v3.3.1 LKM** in `init_boot`, Manager v3.3.1.
Backups in `~/Downloads/houji_backup/`:

| file | what |
|---|---|
| `boot_a_pre_kpatch_20261102.img` | stock `boot_a` (identical to the ROM's `boot.img`) |
| `init_boot_ksu_v3.3.1_lkm.img` | ROM `init_boot` patched with `ksud boot-patch` (KernelSU 3.3.1 LKM) |
| `boot_a_kernel_v6.1.138_GunyahFix17.img` | the custom Gunyah-fix kernel tried in `journal/47` |

```bash
fastboot flash boot      ~/Downloads/houji_backup/boot_a_pre_kpatch_20261102.img
fastboot flash init_boot ~/Downloads/houji_backup/init_boot_ksu_v3.3.1_lkm.img
```

Recreate the KernelSU `init_boot` from scratch (as `shell` on the device, with the
v3.3.1 release assets):

```bash
ksud-aarch64-linux-android boot-patch -b init_boot.img \
  -m lkm-aarch64-android14-6.1_kernelsu.ko -i ksuinit-aarch64 \
  --kmi android14-6.1 --partition init_boot -o . --out-name init_boot_ksu.img
```

Note: the GunyahFix kernels carry a built-in KernelSU (driver 32644) that does
not match the 3.3.1 manager; with them, use a matching CI manager build or Magisk.

## KernelSU modules (journal/57)

* `gh-hugepage-reserve` v12 — boot-time 2 MiB page pool, `pool_want=1024` (2 GiB).
  Disable: `touch /data/adb/modules/gh-hugepage-reserve/disable` (or KernelSU
  safe mode / Module tab), reboot. It self-disables after a crashed boot.
* `gunyah-kvcalloc-fix` — disabled (superseded by DroidVM's runtime module).
* DroidVM runtime modules (`*_gki_6.1`) auto-load with the DroidVM daemon only.

## Guest / DroidVM

The VM lives entirely in userspace; removing it touches nothing flashed.

| what | where |
|---|---|
| guest kernel | `/data/local/tmp/guest.Image` (build: `guest/build_guest.sh`) |
| guest disk | `/data/local/tmp/debian-sid.img` (build: `guest/mk_debian_sid.sh`); the Arch `alarm.img` (`guest/mk_alarm.sh`) was wiped with `/data` |
| VM / network config | `/data/data/cn.classfun.droidvm/files/{vms,networks,disks}.json` |
| app | `cn.classfun.droidvm` — uninstall removes config and runtime modules |

Back up the guest disk with the VM stopped (`adb pull`); it is the only stateful part.

## Firmware bundle

The device is on the **stock** `trustedvm` bundle (nothing patched is installed).
If something was left behind from an experiment, drop it with:

```bash
adb shell su -c 'mount -o rw,remount /vendor/firmware_mnt; rm /vendor/firmware_mnt/image/trustedvm.*'
```

The loader then falls back to `/product/vm-system/trustedvm/boot/trustedvm`.

Current device state (Global ROM, journal/59): **stock `dtbo_a`, stock `trustedvm` bundle**, stock `boot_a`.
(`/vendor/firmware_mnt` survives reboots; remove files there by hand.)

## If the phone dead-hangs

A bad stub hangs before the bootloader watchdog; the bootloader watchdog brings
fastboot back after ~2 min. Otherwise long-press power ~15 s, then
Vol-Down+Power for fastboot.

## Full partition backup

Recipe: `journal/13` (AVF-through-vendor-path) — the same procedure used to
produce the two stock images above.

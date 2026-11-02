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

## Kernel / root

Current state (after `journal/48`): **stock kernel** (`boot_a` = the stock image,
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

## Firmware bundle

The device is on the **stock** `trustedvm` bundle (nothing patched is installed).
If something was left behind from an experiment, drop it with:

```bash
adb shell su -c 'mount -o rw,remount /vendor/firmware_mnt; rm /vendor/firmware_mnt/image/trustedvm.*'
```

The loader then falls back to `/product/vm-system/trustedvm/boot/trustedvm`.

Current device state: **stock `dtbo_a`, stock `trustedvm` bundle**, `boot_a`
untouched. (`/vendor/firmware_mnt` survives reboots; remove files there by hand.)

## If the phone dead-hangs

A bad stub hangs before the bootloader watchdog; the bootloader watchdog brings
fastboot back after ~2 min. Otherwise long-press power ~15 s, then
Vol-Down+Power for fastboot.

## Full partition backup

Recipe: `journal/13` (AVF-through-vendor-path) — the same procedure used to
produce the two stock images above.

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

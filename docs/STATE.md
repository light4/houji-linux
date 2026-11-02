[English](STATE.md) | [中文](STATE.zh.md)

# Current state

Arch Linux ARM runs as a Gunyah guest on the Xiaomi 14, next to Android, on the
stock kernel. Summary of how it got here: [FINDINGS](FINDINGS.md); routes compared:
[OPTIONS](OPTIONS.md); rollback: [RESTORE](RESTORE.md).

## Host (phone)

| item | state |
|---|---|
| ROM | HyperOS `OS3.0.7.0.WNCCNXM`, bootloader unlocked |
| kernel | **stock** `boot_a` (`6.1.118-android14-11-gca0ef6d17716-ab13624819`) |
| root | KernelSU v3.3.1 LKM in `init_boot`, manager v3.3.1 |
| KSU modules | `gh-hugepage-reserve` v12 (`pool_want=1024` = 2 GiB of 2 MiB pages); `gunyah-kvcalloc-fix` disabled |
| DroidVM | `0.0.6.r242.3348830-dev`; runtime modules GH Unmovable, Gunyah Host Share, Gunyah kvcalloc Fix auto-load with its daemon |
| network | DroidVM Linux bridge `br-net0`, 192.168.233.1/24, NAT + DHCP over Wi-Fi |
| SELinux | enforcing |
| also installed | DroidDeck 0.6.9 (`com.droiddeck.launcher`, proot, Ubuntu runtime `ubuntu-r1-rc4` + KDE desktop) for GPU/gaming use; `settings_enable_monitor_phantom_procs=false`. Unmodified fork: [light4/DroidDeck](https://github.com/light4/DroidDeck) |

## Guest (VM `arch` in DroidVM)

| item | value |
|---|---|
| mode | `pseudo_unprotected` (DroidVM shim), cmdline `console=ttyS0 root=/dev/vda rw rootwait nokaslr` |
| CPU / RAM | 4 vCPU, `memory_mb 1536` → 1442 MiB usable, 2 GiB swap file |
| kernel | `/data/local/tmp/guest.Image`, ACK 6.1 `defconfig` + virtio, Landlock, PL030 RTC, no modules (`guest/build_guest.sh`) |
| disk | `/data/local/tmp/alarm.img`, 8 GiB ext4 → `/dev/vda` (`guest/mk_alarm.sh`) |
| OS | Arch Linux ARM, fully upgraded (systemd 262, pacman 7.1), `linux-aarch64`/firmware removed |
| network | `enp0s6`, DHCP `192.168.233.128`, internet via the phone's Wi-Fi |
| accounts | `root`/`root` (serial autologin), `alarm`/`alarm` (ssh) |

## Using it

* **On the phone**: DroidVM → VM → *arch*: Start / Stop / Restart; *Console →
  Text Console Serial 1* is the guest's ttyS0 (root shell, ESC/CTRL/TAB/arrow keys).
* **From a PC**: run `guest/ssh_relay.sh` on the phone (as root), then
  `adb forward tcp:2222 tcp:2222` and `ssh -p 2222 alarm@127.0.0.1`.
* **Config**: DroidVM *Edit*, or `guest/dvcfg.sh '<sed expression>'`, which stops the
  app and its daemon before editing `vms.json`.
* After a phone reboot, unlock the phone once before starting the VM — the app's
  data lives in credential-encrypted storage.

## Known limits

* guest RAM is bounded by the hugepage pool (`pool_want`); without the pool the
  ceiling is ~256 MiB after uptime (fragmentation);
* `nokaslr` is required in pseudo-unprotected mode with our kernel;
* virtio devices only; the guest kernel has no GPU, input or sound drivers yet.

Details of the closed vendor-loader route (ioctl surface, bundles, traps) are in
[reference/vendor-loader](reference/vendor-loader.md).

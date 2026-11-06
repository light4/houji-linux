[English](STATE.md) | [中文](STATE.zh.md)

# Current state

Debian sid runs as a Gunyah guest on the Xiaomi 14, next to Android, on the
stock kernel of the Global ROM. Summary of how it got here: [FINDINGS](FINDINGS.md);
routes compared: [OPTIONS](OPTIONS.md); rollback: [RESTORE](RESTORE.md). The
move to the Global ROM and the rebuild are in [journal/59](journal/59-global-rom-and-debian-sid.md).

## Host (phone)

| item | state |
|---|---|
| ROM | HyperOS **Global** `OS3.0.304.0.WNCMIXM` (China hardware, `hwc=CN`), bootloader unlocked; was `OS3.0.7.0.WNCCNXM` until journal/58 |
| kernel | **stock** `6.1.138-android14-11-g0c3d559bcd85-ab14529422`; hypervisor `gunyah-6de79709c perf` (unchanged) |
| root | KernelSU v3.3.1 LKM in `init_boot`, manager v3.3.1 |
| KSU modules | `gh-hugepage-reserve` v12 (`pool_want=1024` = 2 GiB of 2 MiB pages); also NeoZygisk, TEESimulator, PlayIntegrityFork (unrelated to the VM) |
| DroidVM | `0.0.6.r242.g3348830` (CI build, run `38251242629`); runtime modules GH Unmovable, Gunyah Host Share, Gunyah kvcalloc Fix auto-load with its daemon |
| network | DroidVM Linux bridge `br-net0`, **192.168.134.1/24**, NAT + DHCP over Wi-Fi |
| SELinux | enforcing |
| also installed | DroidDeck 0.6.9 (`com.droiddeck.launcher`); `settings_enable_monitor_phantom_procs=false`. Unmodified fork: [light4/DroidDeck](https://github.com/light4/DroidDeck) |

## Guest (VM `debian-sid` in DroidVM)

| item | value |
|---|---|
| mode | `pseudo_unprotected` (DroidVM shim), Linux boot protocol, cmdline `console=ttyS0 root=/dev/vda rw rootwait nokaslr` |
| CPU / RAM | 4 vCPU, `memory_mb 1536` → 1446 MiB usable, no swap yet |
| kernel | `/data/local/tmp/guest.Image`, ACK 6.1 `defconfig` + virtio, Landlock, PL030 RTC, no modules (`guest/build_guest.sh`) |
| disk | `/data/local/tmp/debian-sid.img`, 8 GiB ext4 → `/dev/vda` (`guest/mk_debian_sid.sh`) |
| OS | Debian sid (`forky/sid`), apt source `mirrors.tuna.tsinghua.edu.cn/debian` |
| devices | virtio block/net/rng only; the app's default `virtio_sound` (with microphone), `xhci_usb` and `simplefb` screen were removed from `vms.json` |
| network | `enp0s2` (systemd-networkd), DHCP `192.168.134.128`, internet via the phone's Wi-Fi |
| accounts | `root`/`root` (serial autologin), `debian`/`debian` (sudo, ssh) |

The Arch Linux ARM VM of journal/53–58 was wiped with `/data` when the ROM was
reflashed. Its pre-upgrade image is still at `~/kmb/alarm.img` on the build host.

## Using it

* **On the phone**: DroidVM → VM → *debian-sid*: Start / Stop / Restart; *Console →
  Text Console Serial 1* is the guest's ttyS0 (root shell, ESC/CTRL/TAB/arrow keys).
* **From a PC**: run `guest/ssh_relay.sh` on the phone (as root), then
  `adb forward tcp:2222 tcp:2222` and `ssh -p 2222 debian@127.0.0.1`. Stop it
  afterwards with `guest/kill_relay.sh 2222` and `adb forward --remove tcp:2222`.
* **Config**: DroidVM *Edit*, or `guest/dvcfg.sh '<sed expression>'`, which stops the
  app and its daemon before editing `vms.json`.
* After a phone reboot, unlock the phone once before starting the VM — the app's
  data lives in credential-encrypted storage.

## Known limits

* guest RAM is bounded by the hugepage pool (`pool_want`); without the pool the
  ceiling is ~256 MiB after uptime (fragmentation);
* `nokaslr` is required in pseudo-unprotected mode with our kernel;
* virtio devices only; the guest kernel has no GPU, input or sound drivers yet;
* no file capabilities: the guest kernel lacks `EXT4_FS_SECURITY`, so `setcap`
  fails (`ping` uses `ping_group_range` instead).

Details of the closed vendor-loader route (ioctl surface, bundles, traps) are in
[reference/vendor-loader](reference/vendor-loader.md).

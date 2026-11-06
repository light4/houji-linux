# Global ROM 3.0.304.0; DroidVM rebuilt on it; Debian sid guest

## The phone moved to the Global ROM

The device was reflashed with the official **Global** fastboot ROM
`houji_global_images_OS3.0.304.0.WNCMIXM_20260908.0000.00_16.0_global_8f1c952b2d.tgz`
(md5 verified). This is a full `flash_all.sh` and wipes `/data`, so the Arch VM
(`/data/local/tmp/alarm.img`, `guest.Image`) and DroidVM's config were lost.
The hardware is still the China unit (`ro.boot.hwc=CN`), but it now reports
`houji_global` / `23127PN0CG`.

| item | before | after |
|---|---|---|
| ROM | `OS3.0.7.0.WNCCNXM` | `OS3.0.304.0.WNCMIXM` |
| host kernel | `6.1.118-android14-11-gca0ef6d17716-ab13624819` | `6.1.138-android14-11-g0c3d559bcd85-ab14529422` |
| hypervisor | `gunyah-6de79709c perf` | `gunyah-6de79709c perf` (same) |
| anti-rollback | 1 | 1 |

The FINDINGS update checks come out the same: no `mssecapp`/`hyp_manager_launch`
in `hyp_a`, no `gunyah` in `virtualizationservice`, no `/sys/kernel/load_guestvm_*`,
no `/dev/kvm`. As expected from journal/52, the newer kernel changes nothing
about the VM route.

Flashing notes:

* After `adb reboot bootloader` the Mac did not enumerate the fastboot device at
  all (not even in `ioreg`) until the cable moved to another USB port.
* `flash_all.sh` checks anti-rollback with `grep -oP`; macOS grep has no `-P`, so
  `ver` ends up empty → 0 and the check is **silently skipped**. Compare
  `fastboot getvar anti` with `images/anti_version.txt` by hand.
* `fastboot flash init_boot_a <ksu-patched>` returned
  `Error flashing partition : 0000001B`, yet KernelSU was active after boot, and
  `sha256(init_boot_a)` equals the patched image byte for byte, so the write
  had completed and only the status was an error. Next time flash `init_boot_ab` like
  `flash_all.sh` does, and check every fastboot exit code separately.

Root is KernelSU v3.3.1 LKM again (init_boot patched on the Mac with
`ksud-aarch64-apple-darwin boot-patch --kmi android14-6.1`).

## DroidVM rebuilt

Same pieces as journal/55 and 57, reinstalled on 6.1.138:

* **App**: the release APK (0.0.6, April) is too old, so it was replaced by the same CI build as before,
  run `38251242629` (`3348830`, 2026-10-12), artifact `app-debug.apk`
  → `0.0.6.r242.g3348830`. The artifact is the APK itself; `gh run download`
  unpacks it, so fetch it raw via
  `gh api repos/Droid-VM/DroidVM/actions/artifacts/<id>/zip > app.apk`.
  It is signed differently from the release, so uninstall the release first. MIUI blocks
  `adb install` (`INSTALL_FAILED_USER_RESTRICTED`); `su -c 'pm install'` works.
* **Onboarding**: root granted in KernelSU, privacy policy declined, prebuilts
  extracted. The wizard now has a network step; picked *Linux bridge* →
  `br-net0`, **`192.168.134.1/24`** (not .233 as before).
* **Runtime modules**: GH Unmovable, Gunyah Host Share, Gunyah kvcalloc Fix
  (`*_gki_6.1`) load on 6.1.138, set to auto-load. NPROC Guard not loaded.
* **`gh-hugepage-reserve` v12**: `ksud module install`, KMI `android14-6.1`.
  Its `kapi_check` flags one mismatch (`kallsyms_lookup_name`: exp `ulong(char*)`
  got `ulong(uchar*)` → disabled) and loads otherwise. After reboot:
  `pool_avail=1024`, `total_refilled=1024`, watchdog stamp cleared.

## Debian sid root disk

`guest/mk_debian_sid.sh`: debootstrap `sid` (minbase + systemd, networkd,
resolved, timesyncd, openssh-server, sudo) from the TUNA mirror into an 8 GiB
ext4 image (`mkfs.ext4 -d`), fstab `/dev/vda`, serial autologin for root,
`systemd-networkd` DHCP on `en*`, user `debian` in `sudo`.

Rootless podman cannot `mknod`, even with `--privileged`, and debootstrap stops at
`Cannot install into target ... mounted with noexec or nodev`. Exporting
`container=lxc` makes debootstrap skip the mknod probe and bind-mount `/dev`
instead; the build then completes:

```
podman run --rm --privileged --arch arm64 -v ~/kmb:/work debian:bookworm /work/mk_debian_sid.sh
```

The guest kernel is the v2 `Image` from journal/56 (still in `~/kmb/guest`).

## VM `debian-sid`

Created with the app's *Customize* wizard: Linux boot protocol, kernel
`/data/local/tmp/guest.Image`, no initrd, cmdline
`console=ttyS0 root=/dev/vda rw rootwait nokaslr`, disk
`/data/local/tmp/debian-sid.img`, network `br-net0`, 1536 MiB, 4 vCPU,
`pseudo_unprotected` (wizard default).

This DroidVM build adds default devices our kernel has no drivers for:
`virtio_sound` (speaker **and microphone**, the app asks for mic access), an
`xhci_usb` controller and an enabled `simplefb` screen. Removed them from
`vms.json` with the app and daemon stopped (`guest/dvcfg.sh`), writing the file
in place so that owner and SELinux label stay intact.

Launch line (from `ps`):

```
crosvm --extended-status run --name debian-sid --mem 1536 --cpus 4 --hypervisor gunyah
  --protected-vm-pseudo-unprotected --no-balloon --no-usb --disable-sandbox --hugepages
  --prepare-lend-mthp-mode single --params console=ttyS0 root=/dev/vda rw rootwait nokaslr
  --block /data/local/tmp/debian-sid.img,lock=false --net tap-name=vm81b23388-0,... guest.Image
```

## Result

Pool `served=768` (= 1536 MiB), `active_vms=1`. Guest:

```
PRETTY_NAME="Debian GNU/Linux forky/sid"
6.1.138-g5929004f86ff-dirty
running                      <- systemctl is-system-running, no failed units
cpus=4
Mem:   1446 total
/dev/root  7.8G  318M  7.1G  5% /
enp0s2  UP  192.168.134.128/24
System clock synchronized: yes
```

`apt-get update` from TUNA works over the bridge NAT.

`setcap cap_net_raw+ep /usr/bin/ping` fails with `Operation not supported`: the
guest kernel has `# CONFIG_EXT4_FS_SECURITY is not set`, so there are no
`security.*` xattrs. Worked around with `net.ipv4.ping_group_range=0 2147483647`
in `/etc/sysctl.d/60-ping.conf`; the real fix belongs in `build_guest.sh`.

ssh from the Mac: `guest/ssh_relay.sh 192.168.134.128` + `adb forward tcp:2222 tcp:2222`
+ `ssh -p 2222 debian@127.0.0.1`. The relay is stopped when not in use.

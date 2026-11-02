# Arch Linux ARM under the DroidVM app, with network and pacman

## App

DroidVM `0.0.6.r242.3348830-dev` (latest CI build, 2026-10-12), stock host
kernel 6.1.118 + KernelSU 3.3.1. Onboarding: root granted in KernelSU
(Superuser → DroidVM), `appops MANAGE_EXTERNAL_STORAGE allow`, SoC detected as
SM8650, **privacy policy declined** (it only gates the image index and update
check via `api.classfun.cn`), prebuilts extracted.

Kernel modules page (KMI `android14-6.1`): loaded **GH Unmovable**, **Gunyah Host
Share**, **Gunyah kvcalloc Fix** at runtime (`*_gki_6.1`, no auto-load).
**GH-Hugepage-Reserve** (boot-time KSU module) not installed yet.
Network: Linux bridge `br-net0`, 192.168.233.1/24, IPv4 NAT + DHCP.

## VM config (`/data/data/cn.classfun.droidvm/files/vms.json`)

Created in the Customize wizard: Linux boot protocol, kernel
`/data/local/tmp/guest.Image`, no initrd, cmdline
`console=ttyS0 root=/dev/vda rw rootwait`, disk `/data/local/tmp/alarm.img`,
network `br-net0`, 256 MiB, 2 vCPU. Then edited the JSON with the app stopped:
`cpu_count 2`, `auto_up false`, `protected_vm protected_without_firmware`,
`swiotlb_mb 64`. `prepare_lend_mthp` stays `single` (upstream's SM8650 default;
the run reports "mTHP not available" and carries on).

* `pseudo_unprotected` (the wizard default): the DroidVM shim accepts the memory
  parcel (`accept handle 0x30 at 0x80600000 size 0xf600000`), jumps to our
  kernel, then `SHIM EXCEPTION 0x4 esr=0x2000000 elr=0x834d9400` — not solved yet.
* `protected_without_firmware`: boots to the systemd login, like journal/53.

## Network

Guest gets `192.168.233.128` via DHCP (`enp0s6`, virtio-net), sshd reachable.
SSH from the Mac: phone-side relay
`toybox nc -s 127.0.0.1 -p 2222 -L toybox nc 192.168.233.128 22` +
`adb forward tcp:2222 tcp:2222`.

The phone itself had no uplink (no Wi-Fi, no SIM: `Active default network: none`),
so the NAT has nowhere to go. Borrowed the Mac's: `uvx pproxy -l http://127.0.0.1:3128`
+ `adb reverse tcp:3128 tcp:3128` + relay on `192.168.233.1:3128`.

## pacman

```
OS: Arch Linux ARM aarch64
Kernel: Linux 6.1.138-g5929004f86ff-dirty
Packages: 167 (pacman)
CPU: dummy-virt (2)
Memory: 71.29 MiB / 129.82 MiB (55%)
Disk (/): 1.93 GiB / 2.88 GiB (67%) - ext4
```

Needed on the way:
* `pacman-key --init && pacman-key --populate archlinuxarm`;
* `--disable-sandbox`: pacman 7's download sandbox needs **Landlock**, absent in
  the guest kernel (add `SECURITY_LANDLOCK` + `lsm=landlock,…`);
* clock: no NTP without uplink → TLS "certificate not yet valid"; set with `date -s @…`;
* mirror: `mirror.archlinuxarm.org` geo-redirect looped through the proxy and the
  `de` mirror returns 403 for `extra.db`; `fl.us.mirror.archlinuxarm.org` works.

## Open

Landlock + RTC/NTP in the guest kernel; larger guest RAM
(`gh-hugepage-reserve`); the pseudo-unprotected shim exception.

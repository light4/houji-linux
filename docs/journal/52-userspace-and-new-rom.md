# Userspace in the 客户机; the newer ROM changes nothing that matters

## Alpine userspace

Initramfs = Alpine 3.24.3 aarch64 minirootfs + a `/init` script, guest kernel =
GKI 6.1.138 `Image`:

```
crosvm-dv run --disable-sandbox --hypervisor gunyah --mem 512 --cpus 2 --swiotlb 16 \
  --protected-vm-without-firmware \
  --serial type=stdout,hardware=serial,earlycon=true,console=true \
  -p "earlycon console=ttyS0 panic=-1 rdinit=/init" -i alpine-initrd.cpio.gz kfix_Image21
```

```
Run /init as init process
=== hello from a Gunyah guest userspace ===
NAME="Alpine Linux"
Linux (none) 6.1.1380921-SUSFS #1 SMP PREEMPT ... aarch64 Linux
cpus: 2
MemTotal:         464792 kB
reboot: Power down          ← crosvm: "exit type 1 … system shutdown", exits 0
```

The GKI kernel only has the 8250 **earlycon**: `serial port 0 not yet initialized`,
no `ttyS0` device, so userspace writes to the tty go nowhere. The output above
went out through `/dev/kmsg`. It also lacks virtio-blk/pci/mmio and `FHANDLE`,
so a real distro needs our own guest kernel (`arm64 defconfig` + virtio,
`DMA_RESTRICTED_POOL`, `FHANDLE`, `SERIAL_OF_PLATFORM`), which is being built.

## Latest ROM: OS3.0.307.0.WNCCNXM (2026-09-21)

Stream-extracted the relevant images from the fastboot tgz (9.5 GB; they sit at
the front, so the download could be stopped after ~200 MB):

| image | 3.0.7.0 → 3.0.307.0 |
|---|---|
| `hypvmperformance.mbn` | same size, **identical strings**, 99 bytes differ (signature); both `gunyah-6de79709c perf` |
| `devcfg.mbn` | same size, identical strings |
| `boot.img` kernel | `6.1.118-…-ab13624819` → `6.1.138-android14-11-g0c3d559bcd85-ab14529422` |
| vbmeta rollback index | 0 → 0 |

Same hypervisor and RM, so the lend requirement and the VM behaviour are
unchanged; only the host GKI moves to 6.1.138. Not worth flashing for this work.

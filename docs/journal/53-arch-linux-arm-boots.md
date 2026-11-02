# Arch Linux ARM boots in the Gunyah guest

Stock host (6.1.118 + KernelSU LKM + `gunyah_kvcalloc_mod`), no kernel patch.

## Pieces (all in `guest/`)

* **Guest kernel** — `build_guest.sh`: ACK 6.1 tree (`5929004f8`) `arm64 defconfig`
  + `DMA_RESTRICTED_POOL`, `FHANDLE`, `DEVTMPFS_MOUNT`, virtio blk/pci/mmio/net/
  console/vsock, `SERIAL_OF_PLATFORM`, btrfs/squashfs/overlay, `MODULES=n`,
  `KVM=n` (ACK's pKVM code references `pkvm_load_early_modules`, which does not
  link without modules). Built in an arm64 podman container; `Image` 57 MB.
* **Root disk** — `mk_alarm.sh`: `ArchLinuxARM-aarch64-latest.tar.gz` → 3 GiB ext4
  via `mkfs.ext4 -d`, fstab `/dev/vda`, root password `root`, serial autologin,
  a first-login report.
* **Run** — `run_arch.sh`:

```
crosvm-dv run --disable-sandbox --hypervisor gunyah --mem 256 --cpus 2 --swiotlb 64 \
  --protected-vm-without-firmware \
  --serial type=stdout,hardware=serial,earlycon=true,console=true,stdin=true \
  --block path=/data/local/tmp/alarm.img \
  -p "earlycon console=ttyS0 root=/dev/vda rw rootwait panic=-1" guest.Image
```

## Result

```
[  OK  ] Reached target Multi-User System.
Arch Linux ARM 6.1.138-g5929004f86ff-dirty (ttyS0)
houji-vm login: root (automatic login)
NAME="Arch Linux ARM"
Linux houji-vm 6.1.138-g5929004f86ff-dirty #3 SMP PREEMPT ... aarch64 GNU/Linux
running                                   ← systemctl is-system-running
Mem:             133          74          23 ...
/dev/root       2.9G  1.9G  848M  70% /
```

virtio-blk through the restricted-DMA swiotlb works; systemd reaches
`running` with no failed units. Log: [`logs/53-arch-linux-arm-boot.txt`](../logs/53-arch-linux-arm-boot.txt).

## Limits found

* `--mem 768` → `VM_START` `NORESOURCE` again, with or without the disk; 256 boots.
  512 is unconfirmed (that run's result got lost). crosvm-dv's
  `--prepare-lend-mthp-mode chunked` splits lends into ≤ 256 MB parcels, which
  hints at a per-parcel limit. But mTHP preparation has crashed the hyp before,
  so test it carefully.
* 64 MiB of the guest RAM goes to the swiotlb pool; 256 MiB leaves 133 MiB usable.
* A lingering crosvm keeps the disk locked (`failed to lock file`). Find it via
  `/proc/*/exe`, not `pkill -f` (that matches the calling shell).

## Next

Network (virtio-net/vsock to the host), a bigger guest (lift the memory limit),
NixOS image.

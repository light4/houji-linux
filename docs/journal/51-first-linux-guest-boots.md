# A Linux guest boots — stock kernel, no patch, guest RAM must be *lent*

## Two corrections

1. **Error 2 is `NORESOURCE`, not `NOMEM`.** Kernel `rsc_mgr.c` and the RM source
   (`include/rm_types.h`) both define `NOMEM=1, NORESOURCE=2, DENIED=3, …,
   MAP_FAILED=9`. Every decode since journal/45 was off by one.
2. **The VMID theory is irrelevant** (journal/50) — and so is the patched kernel.

## Why `VM_START` failed

The RM is open source ([quic/gunyah-resource-manager](https://github.com/quic/gunyah-resource-manager));
the device's `hypvm*.mbn` carries the same strings (`process_memparcels: ret %d`,
`VM_START: from:%d vmid:%d, ret=%d`). In `vm_mgnt_handle_start` every failure of
`vm_creation_process_resource()` is reported as `RM_ERROR_NORESOURCE`.
Inside it, `process_image_memparcel()`:

```c
bool require_exclusive = vm->mem_private;
if (require_exclusive && (trans_type != TRANS_TYPE_LEND)) {
        err = RM_ERROR_DENIED;
```

`crosvm-a16` hands guest RAM over with `GH_VM_SET_USER_MEM_REGION` → kernel
`gh_rm_mem_share` (RPC `0x51000013`). This RM wants the main parcel **lent**
(`0x51000012`), which the 6.1 GKI only does for `GH_VM_ANDROID_LEND_USER_MEM`.

## The working run

DroidVM's `crosvm-dv` with `--protected-vm-without-firmware` lends:

```
crosvm-dv run --disable-sandbox --hypervisor gunyah --mem 256 --cpus 1 --swiotlb 16 \
  --protected-vm-without-firmware \
  --serial type=stdout,hardware=serial,earlycon=true,console=true \
  -p "earlycon console=ttyS0 panic=-1" kfix_Image21
```

RPC trace: `ALLOC_VMID → MEM_LEND(0x51000012) → MEM_APPEND → VM_CONFIGURE →
VM_INIT → GET_HYP_RESOURCES → VM_START ret=0`.

Guest (GKI 6.1.138 `Image` as payload): `Booting Linux on physical CPU 0x0`,
`Machine model: linux,dummy-virt`, `PSCIv1.1`, 8250 earlycon at `0x3f8`,
restricted-DMA swiotlb 16 MiB, … → `Kernel panic - not syncing: VFS: Unable to
mount root fs` — the expected end with no disk. Full log:
[`logs/51-first-linux-guest-boot.txt`](../logs/51-first-linux-guest-boot.txt).

Host: stock `boot_a` 6.1.118, KernelSU 3.3.1 LKM, `gunyah_kvcalloc_mod` loaded,
SELinux permissive only for the run. `GH_VM_SET_BOOT_CONTEXT failed` is harmless
(not in 6.1). `tvk.Image` (Qualcomm trustedvm kernel) stays silent in the same
VM — it has no 8250, so its earlier "silence" proved nothing.

Side results: `cpusys_vm` is static VMID `0x32`; killing it leaves it unrecoverable
until reboot (`VM_STOP`/`RESET`/`DEALLOC` refused). `/proc/tzdbg/hyp_log` is the
QHEE log (one-shot stream), not the RM console.

## Next

Root filesystem: initramfs, then virtio-blk with an Arch Linux ARM / NixOS image.

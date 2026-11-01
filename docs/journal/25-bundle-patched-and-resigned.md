# Bundle patched, re-signed and delivered — guest still dies

Follow-up to [24](24-no-boot-context-ioctl.md). The bundle internals turned out
to be more interesting than the ioctls.

## What the bundle really contains

`trustedvm.b02` is referenced by the guest config as `config_cpio = <0x02>` and
the DT calls itself `boot-config = "fdt,unified"`, `vm-type = "aarch64-guest"`,
`vm-attrs = "crash-fatal"`, `image-name = "qcom,trustedvm"`. The other segments:

| segment | content |
|---|---|
| `b01` | guest kernel |
| `b02` | guest DT template (dtbo table, 22 board variants) |
| `b03` | cpio with `verity.cmdline` (`root=/dev/dm-0 init=/sbin/init dm-mod.create=…`) |

`crash-fatal` explains the hard reset: the host panics when the guest dies, which
is why nothing is logged and why a `dmesg` capture has to race it.

## Patch and delivery

`tools/patch_b02.py` replaced entry 13 (houji) with a rebuilt node set that adds
`reg` (`<0x00 0xda6f8000 0x4000>` / `<0x00 0xda6fc000 0x4000>`) and
`interrupts` (`<0x00 0x13b 0x04>` / `<0x00 0x13c 0x04>` from the DT's own
`gic-irq-ranges`) to both `virtio-mmio` nodes. `tools/rebuild_bundle.py`
re-assembles the image around it, signs it with `qtestsign` and splits the result
by the new phdrs.

Result on device: **the bundle authenticates** (`gunyah_loader: VM: 45 status 2
complete`) and the VM reaches `started running`, so the split/naming is right and
re-signing a *structurally changed* b02 is fine too.

## But the guest still dies

Same as before: gone within ~0.2 s, no device touched, no event. So the missing
`reg`/`interrupts` in the template are **not** the cause — the hyp evidently
generates them from `memory.base` / `dma_base` at boot, our copies were
redundant.

One new probe: `GH_GET_QUEUE_INFO` (`0xC010B269`) answers `EINVAL` both with
`{label, queue}` and `{label, flags, queue}` — either the layout is still wrong
or it only succeeds once the guest has a bank up. Worth finishing, because it is
the only non-blocking way to watch guest progress.

## State on device

The patched bundle is installed in `/vendor/firmware_mnt/image/` (it survives
reboots, vfat). To go back to the stock bundle, delete those files (the loader
then falls back to `/product/vm-system/…`).

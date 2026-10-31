# Adding the virtio backends via a dtbo overlay — it works

The device tree declares no virtio devices (see [14](14-virtio-needs-dt.md)).
This adds them with an overlay, and the kernel's backend driver now picks them
up at boot.

## dtbo image format gotchas

The partition is a DTBO table, not a single blob:

```
magic 0xd7b7ab1e | total_size | header_size(0x20) | entry_size(0x20) |
entry_count | entries_offset | page_size | version
entry = { dt_size, dt_offset, id, rev, custom[4] }
```

Two ways to make it not boot, both hit here:

1. **Data must start after the entries.** With one entry the stock layout works
   out to `0x40`; appending a second entry makes the entries 2×0x20 long, so the
   data area has to move to `0x60`. Overlapping them produced an instant
   boot-to-fastboot.
2. **An overlay needs the board ids or it is never selected.** The stock overlay
   carries `qcom,msm-id`, `qcom,board-id = <0x08 0x00>` and
   `xiaomi,miboard-id = <0x02 0x00>` in its root; leaving them out makes the
   bootloader skip the entry silently (the device boots, nothing appears).

## The overlay

```dts
/dts-v1/;
/plugin/;
/ {
	qcom,msm-id = <0x22d 0x20000 0x22d 0x10000 0x241 0x20000 0x241 0x10000>;
	qcom,board-id = <0x08 0x00>;
	xiaomi,miboard-id = <0x02 0x00>;

	fragment@0 {
		target-path = "/hypervisor/qcom,gunyah-vm";
		__overlay__ {
			virtio-backends = <0x10 0x11>;
		};
	};
};
```

`0x10`/`0x11` are the labels the vendor config uses for the `trustedvm` disks.

## Result

Boot succeeds and the driver reports, unprompted, at probe time:

```
gh_virtio_backend: trustedvm: Recognized VM 'trustedvm' shmem_size 108000
gh_virtio_backend: trustedvm: Recognized virtio backend device with label 11
gh_virtio_backend: trustedvm: Recognized virtio backend device with label 10
```

`shmem_size 108000` is supplied by the hypervisor, so the labels really exist on
the hyp side — the DT only had to declare them. `GH_GET_SHARED_MEMORY_SIZE` now
returns `0x108000` where it used to return `EINVAL`.

No platform devices or `/dev` nodes are created: backends are addressed by
`label` over the **VM fd** (`struct virtio_event`, `virtio_irqfd`,
`virtio_queue_info` all begin with `__u32 label`).

## Still failing

`GH_WAIT_FOR_EVENT` returns `EINVAL` even with `label` pre-filled with 10 or 11.
The driver's handler takes a per-VM lock and walks label state before blocking,
so something is still uninitialised — most likely the shared buffer must be
`mmap`ed first (`gh_virtio_backend_mmap` exists) and/or a device state machine
entered via `GH_ACK_RESET`/`GH_SET_DEVICE_FEATURES`.

## Rollback

The overlay is flashed on `dtbo_a` right now. Stock image kept at
`~/Downloads/houji_backup/dtbo_a.img`
(sha256 `6264b8e66e2e7720e9f593f93f9b7390ccbe5675c04d15c0c13abf1ff327456b`):

```bash
fastboot flash dtbo_a ~/Downloads/houji_backup/dtbo_a.img
```

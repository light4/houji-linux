# Why the guest has no devices: no virtio nodes exist in the device tree

Short version: the kernel's virtio-backend driver is DT-driven, and this device
tree declares **no** virtio nodes at all. That is why a started VM only ever
resets: the guest finds no `virtio_mmio` device, no console, no disk.

## Evidence

`grep -rl virtio /proc/device-tree` → nothing. The VM node is bare:

```
/hypervisor/qcom,gunyah-vm/{compatible,name,qcom,vendor,qcom,vmid}
```

and nothing appears under `/sys/bus/platform/devices` or `/dev` matching
`virtio`, because `gh_virtio_backend_probe` never runs.

`gunyah_loader.ko` is explicit about the mechanism:

* it parses a **`virtio-backends`** property on the VM node
  (`%s: No virtio-backends specified for vm %s`, `%s: Can't parse
  virtio-backends property %d`)
* the property is a u32 array, entries are labels (the code divides the byte
  length by 4 and iterates)
* for each label it looks up a child node named **`virtio_dev_%x`** and reads
  `qcom,vm`, `qcom,label`, `shared-buffers-size`
* `GH_VM_*`/backend ioctls are **label-addressed** (`struct virtio_irqfd`,
  `virtio_event`, `virtio_queue_info` all start with `__u32 label`), so there is
  no "create device" ioctl — the devices must already exist

Full backend ABI in `kernel-headers/linux/gunyah.h` (from the partially-OSS
dump): `GH_GET_SHARED_MEMORY_SIZE` 0x8008B261, `GH_IOEVENTFD` 0x4010B262,
`GH_IRQFD` 0x4010B263, `GH_WAIT_FOR_EVENT` 0xC010B264, `GH_GET_QUEUE_INFO`
0xC010B269, `GH_GET_DRIVER_FEATURES` 0xC010B26A, `GH_ACK_DRIVER_OK` 0xC010B26B,
`GH_ACK_RESET` 0xC010B26D.

Also confirmed by probe: `GH_WAIT_FOR_EVENT` on the **VM fd** returns `EINVAL`
(events are delivered per backend fd), and creating a VM + firmware + 2 vCPUs
works but yields no device.

## What has to be added

A dtbo overlay adding to the VM node:

```
virtio-backends = <0x10 0x11>;
virtio_dev_10 { qcom,vm = <&gunyah_vm>; qcom,label = <0x10>; shared-buffers-size = <...>; };
virtio_dev_11 { ... };
```

`dtbo` is flashable and not signature-checked, but it is also the file the
bootloader uses for the whole kernel DT, so this needs a backup and a
`fastboot boot`-style trial (or `fastboot flash dtbo` + rollback).

Open questions the next run has to answer: which labels/types the hyp actually
exposes for `trustedvm` (the vendor config has disks on labels 10 and 11, which
is what the JSON config's `label` fields point at), and the exact
`shared-buffers-size` value the backend expects.

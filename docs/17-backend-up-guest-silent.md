# Backends are fully armed — but the guest stays silent

## Host side is complete

With both labels declared (`virtio-backends = <0x10 0x11>`) and a real 1 MiB
backing file for label 16:

```
disk fd 4  size 0x100000  sectors 0x800
ack_reset 0  feat0 0  feat1 0  qmax 0
cfg 0                    (capacity via GH_SET_DEVICE_CONFIG_DATA)
shmsz 0  mmap ok  eventfd 9
irqfd lbl 0x10 -> 0      ioevfd lbl 0x10 -> 0
irqfd lbl 0x11 -> 0      ioevfd lbl 0x11 -> 0
running
wait lbl16...            <- blocks, and never returns
```

Notes worth keeping:

* `GH_SET_DEVICE_CONFIG_DATA` (0x4010B267) takes
  `{u32 label; u32 config_size; u64 user_ptr}` — the capacity has to be passed as
  a *user pointer to 8 bytes*, not inline. Wrong layout gives `EINVAL`.
* `GH_SET_DEVICE_FEATURES` (0x4010B265) is called twice, `features_sel` 0
  (`VIRTIO_BLK_F_RO`) and 1 (`VIRTIO_F_VERSION_1`).
* file size comes from `lseek(fd, 0, SEEK_END)`; `fstat` needs a buffer and is
  easy to get wrong in a freestanding binary.

## The guest does not touch the device

No `DRIVER_OK`, no `NEW_BUFFER`, nothing arrives before the host resets. So the
backend exists host-side but the guest never probes it.

Most likely explanation: the **guest's device tree** is what enumerates
`virtio_mmio` nodes, and it does not contain them. The child nodes we add
(`virtio_dev_10`) so far only carry `qcom,vm`, `qcom,label`,
`shared-buffers-size` — no `reg` (MMIO window) and no `interrupts`. Either the
kernel driver synthesises the guest nodes from additional properties, or the
guest DT comes from the hypervisor-side VM config that the vendor service would
normally supply.

Next checks, in order:

1. read the driver's child-node parsing fully and see whether it reads `reg`,
   `interrupts`, `device-type`; add whatever it wants
2. if the guest DT is hyp-provided, dump the guest DT by letting the vendor
   `trustedvm` start with the vendor service (`qvirtservice`) instead of our
   tool and comparing dmesg
3. as a fallback, boot a kernel whose DT we build ourselves — that is the crosvm
   path, currently blocked by the missing pvmfw registration ([13](13-avf-through-vendor-path.md))

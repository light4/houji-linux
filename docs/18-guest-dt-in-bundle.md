# The guest device tree ships in the bundle — and it tells us the label map

`trustedvm.b02` is not payload: it is a **dtbo table** (`0xd7b7ab1e`, 22 entries,
all `custom = 0`) of guest-side device trees, one per board variant. Decompiling
the entry that matches our SoC (`qcom,msm-id = <0x22d … 0x241 …>`,
`qcom,board-id = <0x15 0x00>`, "Pineapple SVM RCM") gives the whole picture:

```dts
chosen { bootargs = "… console=hvc0 loglevel=8 …"; };

virtio-mmio@0 {
    vdevice-type = "virtio-mmio"; vqs-num = <0x01>;
    memory { qcom,label = <0x11>; base = <0x00 0xda6f8000>; };
};
virtio-mmio@1 {
    vdevice-type = "virtio-mmio"; vqs-num = <0x02>;
    memory { qcom,label = <0x10>; base = <0x00 0xda6fc000>; };
};
```

So the mapping is the opposite of what the vendor JSON suggested:

| label | device | queues | MMIO window |
|---|---|---|---|
| `0x11` (17) | **console** (`hvc0`, the guest's boot console) | 1 | `0xda6f8000` |
| `0x10` (16) | **disk** | 2 | `0xda6fc000` |

The guest's early boot writes to `hvc0`, so the console queue is the first thing
that has to work; until then the guest is blind and the `gh_virt_wdt` resets the
host a few seconds in.

Two consequences:

1. **The guest DT is editable by us.** `b02` is inside the signed bundle, and we
   can re-sign with `qtestsign` ([09](09-custom-system.md)) — so guest windows,
   labels and even `bootargs` (drop the console requirement, point `root=` at a
   disk) are ours to set.
2. Features / queue max / reset have to be prepared **for both labels**; leaving
   the console unconfigured stalls the guest at its first print.

## Current status

Host side armed for both labels (ack_reset, features, queue max, mmap,
`irqfd`/`ioeventfd` all return 0) and both vCPUs running, yet
`GH_WAIT_FOR_EVENT` never returns on either label. So something still keeps the
guest from touching the window at `0xda6f8000`.

Next suspects, in order:

* the MMIO windows have to be backed (the driver has `gh_virtio_mmio_init`;
  our child nodes carry no `reg`/`base`, so the driver may expect the window
  address from DT rather than from the guest DT)
* `GH_SET_DEVICE_CONFIG_DATA` for the console (size 0) may be mandatory before
  the guest's probe completes
* proxy-scheduling state: `VCPU_RUN` needs the vCPU actually scheduled; the
  `gh_proxy_sched` lines only appear once a real start happens, and after a
  reset dmesg is gone — check them on the same boot instead

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

## Regression to know about

With the backends declared, the loader no longer logs `VM:45 started running`
and `VCPU_RUN` blocks forever — whereas without any backend the VM did start and
reset after a few seconds. So **start is gated on backend readiness**, and our
setup is still incomplete somewhere.

Tried without effect: per-queue `GH_SET_QUEUE_NUM_MAX`/`GH_IOEVENTFD` for both
disk queues (the disk has 2 vqs), and `GH_SET_DEVICE_CONFIG_DATA` with size 0 for
the console.

Remaining candidates, roughly in order of likelihood:

* console features: a multiplort `hvc0` needs `VIRTIO_CONSOLE_F_MULTIPORT`; we
  set only `VIRTIO_F_VERSION_1`
* ordering: maybe `GH_ACK_RESET` has to follow the `mmap`, or `queue_ready` from
  `GH_GET_QUEUE_INFO` has to be polled before start proceeds
* the host is expected to read `GH_GET_DRIVER_FEATURES`/`GH_GET_DRIVER_CONFIG_DATA`
  after the guest boots, which cannot happen before start — need to confirm the
  actual start trigger in the driver's state machine

## Per-label mappings

The shared buffer is mapped **per label**: `mmap(shm_size, RW, SHARED, vm,
offset = label << 12)` succeeds and yields a distinct address per device
(`0x71820fc000` for label 16, `0x7181ff4000` for label 17 in one run). A single
`mmap` with offset 0 only covers the first backend.

Tried without changing the outcome: `shared-buffers-size` 0x4000 instead of
0x108000.

## Where it stands

Everything returns success, both vCPUs exist, both windows are mapped — and the
guest still faults (host reset) within about a second of `VCPU_RUN`, with no
event on either label. Since declaring the backends the loader never logs
`started running`, so the VM's start is gated on something we still do not
satisfy.

Next ideas, roughly in order:

1. `GH_ACK_DRIVER_OK` for the console before start (it takes just the label as
   `__u32`, so it can be pre-set)
2. `virq` wiring: the guest's `interrupts` in its DT must correspond to what the
   hyp assigns; our host child nodes carry no interrupt information and the
   driver has no `interrupts` string, so the mapping presumably comes from the
   hyp — worth confirming by dumping the guest DT's `interrupts` for both nodes
3. temporarily strip `console=hvc0` from `chosen.bootargs` in `b02` (re-sign) so
   the guest does not depend on a working console to get anywhere

## Operating notes

* the loader's VM lifecycle lines are `dev_dbg`: re-enable
  `file *gunyah* +p` after **every** boot
* the watchdog resets ~5 s in, so dmesg has to be sampled within ~4 s; anything
  read afterwards comes from the next boot

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

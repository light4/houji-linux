# What the loader actually does at start, and why the overlay was a red herring

The earlier "the VM never starts" conclusion was wrong — it came from sampling
dmesg too late. With a proper capture the picture is different.

## Capturing the loader's log across the reset

The watchdog/panic notifier resets the phone ~0.1 s after the guest faults, so a
one-shot `dmesg` never survives. What works:

```sh
dmesg -w > /data/local/tmp/d.txt &      # follow, append as it happens
while true; do sync; done &             # keep page cache on flash
/data/local/tmp/ghj                     # the probe
```

`sync` in a loop is what makes the difference: the reset kills the writer, and
without flushes the file is empty.

## What it shows

```
gh_virtio_backend: vmid 45 vm_name trustedvm label 11 cap_id 6754.. irq 424 base 0000000000000000 size 4096
gh_virtio_backend: vmid 45 vm_name trustedvm label 10 cap_id 67a2.. irq 425 base 0000000000000000 size 4096
gh_virtio_backend: ack_reset for label 10!
gh_virtio_backend: label 16 features 0 20
gh_virtio_backend: set_features 1/1e failed ret 30
gh_virtio_backend: label 16 queue_max 0 80        (0x80 = 128, our value)
gh_virtio_backend: config_word 0 800              (0x800 = 2048 sectors, ours)
gh_virtio_backend: cmd c010b26b not supported
gunyah_loader: VM:45 started running
```

Three facts fall out of it:

* **the VM does start.** `started running` is printed after `VCPU_RUN` and after
  all backend setup. The reset that follows comes from the guest faulting.
* **`GH_ACK_DRIVER_OK` is not implemented** by this driver (`cmd ... not
  supported`), for both 0x4010B26B and 0xC010B26B — drop the idea.
* the driver **reports `base 0 size 4096`** for both devices, whatever we put in
  `reg` (2-cell, 3-cell, 4-cell) or in a `memory { base = ... }` child. So base
  is not read from the Linux DT at all, and 0 may be benign (it is the
  userspace mapping, which is unset at that point).

## The overlay is not needed

With the **stock** `dtbo_a` back in place the loader still logs both
`Recognized virtio backend device with label 11/10`. And `/proc/device-tree`
never contains any of our overlay content — no `virtio-backends`, no
`virtio_dev_*`, not even the `xiaomi,miboard-id` the vendor entry carries.

So the dtbo overlays are **not** merged into the Linux DT. The loader gets the
backend list from the signed bundle (`trustedvm.b02`), whose
`virtio-mmio@N/memory { qcom,label, base }` nodes are exactly the devices that
get "recognized". Our overlay was harmless but irrelevant.

> **Correction (`journal/36`):** the *second* half is wrong — stripping every
> virtio node out of all 22 `b02` entries (and cutting the table to one entry)
> still yields "Recognized … label 11/10". The device list comes from the
> hyp/RM, not from the bundle. The overlay conclusion still holds.

Current device state: **stock dtbo**, nothing pending.

## Remaining problem

`set_features 1/0x1e failed ret 30` (word 1 = the high feature bits) and the
guest faults right after start. Also `trustedvm.b02`'s nodes carry
`generate = "/virtio-mmio"`, `push-compatible = "virtio,mmio"`, `dma_base`,
`peer-default` — that is a *template*, not a finished guest DT: no `reg`, no
`interrupts` on those nodes. Someone has to turn it into real nodes, which on
the vendor stack is `vendor.qvirtmgr`/`qvirtservice`.

Next steps, in order:

1. find which feature bits the driver accepts for word 1 (try 0x01 VERSION_1
   alone, then 0x00) — 0x1e was rejected
2. let `qvirtmgr`/`qvirtservice` start `trustedvm` and see whether their guest
   DT ends up with `reg`/`interrupts` (they may generate it), then diff against
   ours
3. if the guest still cannot be reached, patch `trustedvm.b02` directly (it is
   inside the bundle and we can re-sign) to a static, fully specified DT

# `/proc/vmkmsg` exists; b02 entry selection confirmed; still no guest output

Three results, all negative but each closes a door.

## The guest-log channel is present but empty

`dmesg_dumper.ko` provides **`/proc/vmkmsg`** (0444, `copy_to_user` of a shared
buffer), and the *host* DT has the matching node:

```
dmesg-dump { compatible = "qcom,dmesg-dump"; qcom,primary-vm;
             gunyah-label = <0x07>; peer-name = <0x02>;
             memory-region = …; shared-buffer-size = <0x1000>; }
```

Reading it in a loop while the VM runs returns 0 bytes every time: the guest
would have to write into the buffer, and it dies before it does. The channel is
right, the guest is too short-lived. (`/proc/vmkmsg` is the thing to watch once
the guest survives longer.)

## The patch did land on the right entry

The b02 table has 22 variants; exactly one combines the host's `qcom,msm-id`
(`0x22d`) with its `qcom,board-id` (`0x15`) — entry 13, the one patched in
`journal/25`. So the earlier "no effect" is real and not a "patched the wrong
board" mistake: the hyp generates `reg`/`interrupts` itself.

## No RAM size in the DT

The guest `memory` node has `#size-cells = <0>` and only `base-address`
(`0x28800000`); the size comes from the reserved `trust_ui_vm_region` (68 MB) via
the loader. Nothing to tune there either.

## Net

Host-side channels: none left (no console, no device nodes, no queue-info ioctl,
no panic trace, `vmkmsg` empty). Every host-side and DT-template lever has been
tried. The remaining move is the one that also satisfies the real goal: bring our
own guest image, starting with something that proves execution without virtio.

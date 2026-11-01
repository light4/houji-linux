# The virtio backend sequence works — here is the exact order

Everything below is verified on-device against label 0x10 (see the label note).

## Sequence

```
GH_CREATE_VM                      0xB201            -> vm fd
GH_VM_SET_FW_NAME                 0x4010B241        ok
GH_CREATE_VCPU x vcpu_count       0xB240            ok
GH_VCPU_RUN (child process)       0xB280            blocks, guest runs
GH_ACK_RESET      {label,0}       0x4008B26D        ok
GH_SET_DEVICE_FEATURES {label,_,sel,feat}  0x4010B265  ok
GH_SET_QUEUE_NUM_MAX   {label,_,q,max}     0x4010B266  ok
GH_GET_SHARED_MEMORY_SIZE        0x8008B261         -> 0x108000
mmap(shm, size, RW, SHARED, vm, 0)                 ok
eventfd2                                           ok
GH_IRQFD      {label, flags=1, fd, 0}  0x4010B263   ok   <-- flags must be 1
GH_IOEVENTFD  {label, flags=1, q, fd}  0x4010B262   ok   <-- flags must be 1
GH_WAIT_FOR_EVENT {label,...}          0xC010B264   blocks, events arrive
```

## Three traps

1. **`flags` must be non-zero.** With `flags = 0` both `GH_IRQFD` and
   `GH_IOEVENTFD` return `EOPNOTSUPP` (-95). With `flags = 1` they succeed.
   Nothing in the header says so.
2. **Labels are hex.** The hyp labels are `0x10` and `0x11`; the driver prints
   them as hex, so "label 10/11" in dmesg means 16/17 in decimal. The
   `virtio-backends` property and `qcom,label` carry the same hex values, and
   userspace must pass the same numbers.
3. **Declare every label you intend to use.** Only the labels listed in
   `virtio-backends` get backend state; the other returns `EOPNOTSUPP`.

## Capturing output across the reset

The guest still resets the host a few seconds in, and a plain `>` redirect
leaves a correctly-sized file full of zeros because the data never reaches disk
in time. The probe opens its own output file and calls `fsync` after every write;
then the results survive ([`src/gh_virtio_probe.c`](../../src/gh_virtio_probe.c),
`w()`).

## What is left

Service the virtqueue: on `DRIVER_OK` read `GH_GET_QUEUE_INFO` (0xC010B269) for
descriptor/avail/used addresses inside the mmap'ed region, translate the
descriptor chain into the backing file, and answer with `used` entries plus an
irq. For a disk this is the whole virtio-blk data path; a console is simpler
(single queue, no device config).

The reset means something still breaks after start — most likely the first
request going unserviced — so a watchdog continue and a minimal, correct blk
path are the next two items.

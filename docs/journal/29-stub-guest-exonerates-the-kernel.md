# A one-instruction guest: the kernel is innocent, the backend setup kills the VM

`vm-attrs`/`reg`/DT changes all did nothing, so the guest kernel itself was
suspect. Replaced `b01` (the kernel segment) with four bytes of AArch64:

```
14000000        b .
```

Padded to the original length, re-signed, installed.

| setup | result |
|---|---|
| stub guest, **no** backends armed (create → firmware → vcpus → run) | host alive at 8 s, died later (~15 s) |
| stub guest **with** backend setup | host dies in ~0.5 s, exactly as with the real kernel |

> Timings corrected in [30](30-stub-timings-corrected.md): an inert stub actually
> survives ~10 s; only the vendor kernel dies in 0.5 s.

So the vendor kernel, its DT, `reg`, `interrupts` and `vm-attrs` are all
irrelevant to the fast reset. **Declaring the virtio backends is what kills the
VM**: the moment they exist, the loader/VM aborts within half a second, while
without them an inert guest survives much longer.

That reframes the whole failure: the guest never services the backend, and the
backend's presence imposes a progress requirement (probably a reset/ack timeout)
that nothing satisfies. The vendor kernel probably dies for the same reason —
because it cannot reach the device, not because it cannot boot.

## Next

* make the stub touch the window: read the virtio magic at `0xda6f8000`
  (`ldr w0, [x1]`), then loop. If that keeps the VM alive, the device *is*
  reachable and only the handshake is missing.
* try a single-label setup (console only) to see whether one backend also
  triggers it; that isolates setup order from "any backend at all".
* find the timeout in the loader (`gunyah_loader`, "Wait for VM_STATUS …") to know
  what progress it expects.

Note: the device is back on the **stock** bundle; the stub bundle is in `/tmp`
(`outl/`, `signed_loop`, `comb_loop`).

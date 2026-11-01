# The backend list does not come from `b02`

`journal/18`/`19` concluded the loader reads the backend labels from the guest DT
in the bundle, because dtbo overlays never reach the Linux DT. That is wrong.

Experiment: every one of the 22 entries in `trustedvm.b02` was stripped of its
`virtio-mmio@0`, `virtio-mmio@1` and `swiotlb-shm` nodes, re-signed and installed.
The loader still logs:

```
gh_virtio_backend: Recognized VM 'trustedvm' shmem_size 108000
gh_virtio_backend: trustedvm: Recognized virtio backend device with label 11
gh_virtio_backend: trustedvm: Recognized virtio backend device with label 10
```

Then the table was cut down to a **single** entry (the houji one) — same result.
So the device composition and `shmem_size` are decided *above* the bundle: the
hyp's VM template / resource manager hands them to the loader. `b02` is the guest
DT the guest is booted with, not the device manifest.

## Consequences

* adding, removing or renaming devices in `b02` cannot change what the host side
  offers — the "bisect the virtio path" idea has to be done elsewhere (the
  composition may simply be fixed in this firmware).
* guest-side DT edits can still matter for the *guest kernel*, but the guest dies
  before it can use them.
* this also means the earlier `reg`/`interrupts` additions were doubly
  irrelevant.

## State

Single-entry DT in place, guest still gone in ~0.2 s, no events, no fault — the
same signature as every other variant.

# `cpusys_vm` as the known-good reference — and `pinned` affinity

The one VM that works on this device ships its own bundle in
`/vendor/firmware_mnt/image/cpusys_vm.{mdt,b00..b03}` (the loader looks it up by
that name; the boot log shows `gh_secure_vm_loader@2: loading …
cpusys_vm.mdt failed with error -13` before the vendor path evidently retries).
Its config segment is a **plain FDT** — no dtbo table — and the interesting
differences to `trustedvm` are:

| property | `cpusys_vm` | `trustedvm` |
|---|---|---|
| `vcpus.affinity` | `"pinned"`, `affinity-map = <0..7>` | `"proxy"`, `affinity-map = <0x05 0x06>` |
| `vm-attrs` | `"no-shutdown", "crash-fatal", "ras-error-handler"` | `"crash-fatal", "context-dump"` |
| `vdevices` | `rm-rpc`, `chipinfo-shm`, `hw-fence-*` — **no virtio at all** | `virtio-mmio@0/1`, `swiotlb-shm` |
| memory base | `0x80e00000` (`cpusys_vm_region`) | `0x28800000` (`trust_ui_vm_region`) |

Dropping `affinity` entirely (`journal/34`) and switching to `"pinned"` with a
`<0x00 0x01>` map both leave the guest dying at the same instant, so affinity is
not the trigger either — with the caveat that the sample counts are noisy because
the probe process itself dies when the VM is torn down.

Still worth noting for the plan: the known-good VM uses **no virtio backends at
all**. Everything that makes `trustedvm` fail might live in the virtio path, and a
`trustedvm` variant with `virtio-backends` removed entirely would be the cleanest
next bisect step.

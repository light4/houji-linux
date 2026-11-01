# Removing `crash-fatal` does not stop the reset; no per-label device nodes exist

Two negative results from `journal/25`'s patched bundle:

1. **`vm-attrs` without `crash-fatal`** (`vm-attrs = "context-dump";`) still
   resets the host ~0.5 s after `started running`. So the reset is not the
   crash-fatal panic alone — likely the VM watchdog / the loader's default
   restart path. Consequence: no extra information is gained by removing it, and
   a dmesg capture still has to race the reset.
2. **No `/dev/gh_virtio_backend_<label>` nodes are ever created.** Sampling
   `/dev` five times inside the guest's lifetime shows none, although `qcrosvm`'s
   strings open exactly that path. Together with `0x4020B245` and the `0xBCxx`
   family being unimplemented (`journal/21`, `journal/23`), this is more evidence
   that the shipped launcher/bundle target a newer Gunyah driver than this
   kernel ships.

## Where the guest log could still come from

The guest DT declares a `dmesg-dump` device with `gunyah-label = <0x07>` plus a
signing key, i.e. the guest kernel can push its log into a Gunyah message queue.
The loader module parses `gunyah-label` properties, but nothing appears in `/dev`
for it either, so the msgq side is not wired up on the vendor loader path. If it
ever works, that is the guest console we want.

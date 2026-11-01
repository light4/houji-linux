# The guest never even touches its MMIO window

Follow-up measurements after `docs/21`:

* `0x8008B261` does return a value: **0x108000**. It is the VM's reserved/shared
  memory size, our `mmap` already used it, so that step was fine.
* `0xB243` (get vcpu count) → `2`; the `qcrosvm` spelling `0x4020B245` →
  `EINVAL` on this driver. Use `0xB243`.
* `create vcpu` really is just `0xB240` with the vcpu index as argument, matching
  `qcrosvm`.

## The decisive observation

With the loader's log captured across the reset (see `docs/19` for the method)
the log **ends at `VM:45 started running`** — no fault, no vcpu exit, no
`gh_panic_notifier` line, and `GH_WAIT_FOR_EVENT` never returns.

A virtio guest resets its device (status = 0) as the *first* thing its driver
does, which produces a device-reset event. We get no event of any kind, so the
guest dies **before virtio init** — not because of a feature or queue
disagreement, but in the earliest part of its boot.

Also worth dropping: the `base 0 size 4096` prints are taken *before* the
`mmap`, so they are the (still unset) userspace mapping, not a missing window.

## What that leaves

1. the guest faults in early boot (stack/entry/`fdt` handoff) — plausible, the
   vendor never boots `trustedvm` on this ROM, so the bundle may expect a boot
   mode we do not set;
2. the bundle's kernel needs a *generated* guest DT (the `b02` template) and
   dies on a bad one;
3. something in the hyp-facing VM creation arguments (`0xB201` takes a struct;
   `qcrosvm` passes one, we pass `NULL`).

Next: recover the create-vm struct (`qcrosvm` builds it from its CLI:
`main.rs` 0x1fb1c) and pass the same fields — a NULL there is the largest
remaining difference between our tool and the vendor's.

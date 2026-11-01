# Segments are placed by `p_paddr` — and we had it at zero

`cpusys_vm.mdt` (the one bundle that boots) settles the mapping convention:

```
seg 1  PT_LOAD  offset 0x200000  vaddr 0x80e00000  paddr 0x80e00000  size 0x7e000
seg 2  PT_LOAD  offset 0x400000  vaddr 0x81000000  paddr 0x81000000  size 0x16e6
```

`0x80e00000` is exactly the `memory/base-address` of that VM's DT, and the
payloads sit at 2 MB-aligned offsets. So the loader/hyp place a segment at its
**physical address** inside the guest's memory region — `p_paddr` is the guest
IPA, not decoration.

`trustedvm`'s kernel segment has `p_paddr = 0`, i.e. the guest would start
fetching at IPA 0 while its memory region begins at `0x28800000`. That looked like
the bug. Setting the kernel segment's `p_vaddr`/`p_paddr` to `0x28800000`
(matching the DT, as `cpusys_vm` does) and booting it: **the guest still dies in
~0.2 s** (2 log samples, 3 probe samples). So the address is wrong in the
original bundle *and* correcting it is not sufficient.

## Notes for the next attempt

* keep `p_paddr == DT memory base` from now on when building bundles — it is the
  correct convention regardless;
* the remaining delta to a working VM is elsewhere, and `cpusys_vm` is still the
  only reference: it uses 2 MB-aligned segments, `PT_LOAD` flags `0x6`, a
  `vdevices` set without virtio, and `vm-attrs` with `no-shutdown` and
  `ras-error-handler`.
* both of those bundles are in `/tmp` for further diffing: `cs.mdt`, `cs.b02`.

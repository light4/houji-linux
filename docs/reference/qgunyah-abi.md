# `/dev/qgunyah` ABI — implemented surface and traps

Consolidated from `journal/16`, `journal/21` and `journal/24`. Everything here
was verified on device; the numbers are the only ones that work.

## Naming convention

`_IO*` encoded: `<dir/size><0xB2><nr>`. The driver implements the **legacy**
`0xB2xx` family only. `qcrosvm` also carries a second family at `nr + 0x9a0`
(`0xBCxx`) for a newer driver; on this kernel every `0xBCxx` command answers
`EINVAL` (see `journal/21`).

## Implemented commands

| cmd | meaning | arg |
|---|---|---|
| `0xB201` | create VM | `1` (scalar; `0` also works) |
| `0xB240` | create vCPU | vcpu index |
| `0xB243` | get vCPU count | none, returns count |
| `0xB280` | run vCPU | pointer to run buffer, blocks |
| `0x4010B241` | set firmware name | `trustedvm` only |
| `0x8008B261` | reserved/shared memory size | out `u64` → `0x108000` |
| `0x4010B262` | ioeventfd | `{label, flags=1, queue, fd}` |
| `0x4010B263` | irqfd | `{label, flags=1, fd}` |
| `0xC010B264` | wait for event | `{label,…}` (never returned here) |
| `0x4010B265` | set device features | `{label, flags, features_sel, word}` |
| `0x4010B266` | set queue num max | `{label, flags, queue, max}` |
| `0x4010B267` | set device config data | `{label, size, user_ptr}` |
| `0xC010B269` | get queue info | `{label, queue,…}` |
| `0xC010B26A` | get driver features | `{label, flags, sel, …}` |
| `0x4008B26D` | ack reset | `{label, 0}` |

Not implemented: `ACK_DRIVER_OK` (`0xC004B26B`, `0xC004BC0B`) → dmesg
`cmd … not supported`; `0x4020B245` (newer get-vcpu-count) → `EINVAL`.

## Traps

* `flags` **must be 1** in `ioeventfd`/`irqfd`, otherwise `EINVAL`.
* labels are **hex** everywhere — DT `qcom,label`, `virtio-backends`, userspace.
  `0x10`/`0x11` are decimal 16/17 in dmesg (`label 16`, `label 17`).
* the shared buffer is mapped **per label**: `mmap(size, RW, SHARED, vm,
  label << 12)`; a single `mmap` with offset 0 only covers the first backend.
* `set_features` returns the device's support mask (`0x1e`) for any subset and
  `0` only for `0x1f` — request the mask plus `VIRTIO_F_VERSION_1`.
* every label used must be declared in the guest DT of the bundle, otherwise
  there is no device behind it.
* `setfw` on an unsigned/rebuilt bundle fails `VM_AUTH_IMAGE`; re-sign first
  ([bundle-format](bundle-format.md)).
* one VM per boot: a failed attempt leaks the 68 MB `trust_ui_vm_region` CMA.
* **order matters**: `GH_VM_SET_FW_NAME` must come before `0x8008B261`
  (reserved memory size). On a fresh VM that ioctl answers `EINVAL` — which is
  exactly why the shipped `qcrosvm` aborts (`journal/40`).

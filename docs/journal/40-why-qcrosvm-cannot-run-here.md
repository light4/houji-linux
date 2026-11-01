# Why the vendor launcher cannot run on this ROM

> **Wrong — see [41](41-stock-launcher-same-failure.md).** `qcrosvm` runs
> `cpusys_vm` on this ROM (pid from boot). The abort below is caused by our
> `--mem` argument and by `cpusys_vm` already running; without `--mem` it boots
> `trustedvm` up to `started running` and fails exactly like our probe.

`qcrosvm` aborts immediately on `Get reserved mem size failed (EINVAL)` — for
`trustedvm` *and* for `cpusys_vm`, i.e. also for the VM that demonstrably boots on
this device. So it is not about our bundle.

Cause found by trying the same ioctl in both orders:

| order | `0x8008B261` (reserved memory size) |
|---|---|
| right after `GH_CREATE_VM` | **EINVAL** |
| after `GH_VM_SET_FW_NAME` | **0** |

`qcrosvm` calls it *before* `set_fw_name` (its string order is `create vm` →
`reserved mem size` → `set fw name` → …), which this driver rejects. A second,
independent skew: `qcrosvm -l` needs `/sys/kernel/load_guestvm_<vm>/boot_guestvm`,
and **no module in this ROM provides that sysfs node** (grepped every `.ko`).

So the shipped launcher belongs to a different (newer) driver generation than the
kernel here. Consequences:

* `qcrosvm` cannot be used as a shortcut to boot the VM on this build.
* our hand-rolled probe is not a hobby re-implementation but the only path that
  gets as far as `started running` — with `set_fw_name` before anything else.
* if a future ROM ships the newer ABI (`0x4020B245`, the `0xBCxx` family,
  `load_guestvm`), the launcher becomes usable again and is the reference to
  diff against.

## Worked-around, not fixed

The ordering requirement is now documented behaviour of *this* driver
([reference/qgunyah-abi](../reference/qgunyah-abi.md)); the guest still does not
execute, which remains the open problem.

# `NOMEM` survives every lever — option 3 parked

Sweep of everything that can be expressed from userspace, all with the stock
`qcrosvm`, own kernel, on a freshly booted device:

| varied | result |
|---|---|
| `--mem` 16 / 64 / 128 / 256 / 512 / 1024 MB | `VM_START` → `NOMEM`, every size |
| memory `base-address` 0x80000000 / 0x28800000 / 0x40000000 / 0xc0000000 | `NOMEM` |
| `--cpus` 1 / 2 | `NOMEM` |
| `image-name` `trustedvm` / `qcom,trustedvm` / `qcom,cpusysvm` | `NOMEM` |
| `vcpus.affinity` proxy (default) | `NOMEM` |
| `vcpus.affinity` pinned / static | fails earlier, `VM_INIT` → `MAP_FAILED` |
| `vm-attrs = "crash-fatal"` | `VM_INIT` → `BUSY`, then → `NORESOURCE` |
| `qcom,pasid`, `vcpus.config`, cpu `enable-method = "psci"` | no change |
| `--no-balloon --no-rng --no-usb --no-pmu` | no change |
| all ten doorbells `status = "disabled"` | no change |
| **other VM stopped** (`kill -9` of `cpusys_vm`'s `qcrosvm`, state `STOPPED`) | no change |

So it is not memory quantity, not IPA placement, not the vCPU count, not the VM
name the hyp might key on, not the device set, and not contention with the only
running VM. The hyp simply cannot allocate for an undeclared, unauthenticated VM
on this build.

The remaining levers are all hyp-side: per-VM object/heap budgets and the
`hlos_vm_config` table live in the signed `hypvm*` image and its devcfg, and this
production hyp logs nothing.

**Verdict:** option 3 is parked until a firmware update. Re-run the tripwires in
[FINDINGS](../FINDINGS.md) first; a newer hyp may well change this.

## Repair after the sweep

`cpusys_vm` was killed during the test (display/fence go with it); a reboot brings
it back — it autostarts.

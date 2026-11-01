# The crosvm start failure is `NOMEM`, not `NORESOURCE`

Earlier entries called the Resource Manager's answer "NORESOURCE". The error
enumerators are in the kernel image (`gh_rm_drv`), so the numbers can be read off
directly:

```
GH_RM_ERROR_OK(0) UNIMPLEMENTED(1) NOMEM(2) NORESOURCE(3) DENIED(4) INVALID(5)
BUSY(6) ARGUMENT_INVALID(7) HANDLE_INVALID(8) VALIDATE_FAILED(9) MAP_FAILED(10)
MEM_INVALID(11) MEM_INUSE(12) MEM_RELEASED(13) VMID_INVALID(14) LOOKUP_FAILED(15)
IRQ_INVALID(16) IRQ_INUSE(17) IRQ_RELEASED(18)
```

So `VM_START` (`0x56000004`) → **`NOMEM`**, and `VM_INIT` (`0x5600000b`) with a
non-proxy affinity → **`MAP_FAILED`**.

## `NOMEM` details

* independent of guest RAM size: `--mem` 16 / 64 / 128 / 512 / 1024 MB all give
  `NOMEM`, so it is hyp-side allocation, not the size of the lent memory.
* happens on a freshly booted device with only `cpusys_vm` running, so it is not
  contention from our own earlier attempts either.
* unaffected by `--no-balloon --no-rng --no-usb --no-pmu`, nor by marking all ten
  doorbell vdevices `status = "disabled"`.

## Config does reach the RM, though

Overlays change the outcome, so the parser is reading our node:

| overlay | effect |
|---|---|
| none | `VM_START` → `NOMEM` |
| `affinity = "pinned"`/`"static"` | `VM_INIT` → `MAP_FAILED` |
| `vm-attrs = "crash-fatal"` | `VM_INIT` → `BUSY`, then `0x56000006` → `NORESOURCE` |
| `vm-attrs` + `qcom,pasid` | same as `crash-fatal` |
| `qcom,pasid` alone | no change |
| `vcpus.config = "/cpus"` + `enable-method = "psci"` on the cpu | no change |

`vm-attrs` being able to turn `NOMEM` into `BUSY` shows the config is not being
ignored — the resource shortfall just happens before the attributes matter.

## Interpretation

Two candidate readings, both consistent with the data:

1. the hyp's static object/heap tables are sized for the VMs listed in its devcfg
   (`cpusys_vm`, `trustedvm`, `oemvm` — the "PVM only variant" and
   `hyp_extra_heap_size` strings point at a deliberately small extra heap), and an
   *additional*, non-declared VM has no budget;
2. something in crosvm's device set still exceeds a per-VM cap that `status =
   "disabled"` does not reduce (the hyp may ignore `status`).

Both are only testable from the hyp side, which this production build does not
log.

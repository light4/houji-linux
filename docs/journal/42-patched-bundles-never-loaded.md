# Patched bundles were never loaded — and re-signed ones fail authentication

This entry invalidates the central premise of `journal/09` and every bundle
experiment in `journal/25`–`39`.

## The silent fallback

With SELinux enforcing, the loader's *direct* read of
`/vendor/firmware_mnt/image/trustedvm.*` fails, and it silently falls back to the
ueventd/`vmmgr` path, which serves the **stock** bundle from `/product`:

```
gh_secure_vm_loader …@0: loading /vendor/firmware_mnt/image/trustedvm.mdt failed with error -13
gh_secure_vm_loader …@0: Direct firmware load for trustedvm.mdt failed with error -2
ueventd: found /product/vm-system/trustedvm/boot/trustedvm.mdt …
```

(The same `-13` appears for `cpusys_vm` at boot; it is the normal state.) So
every "patched" bundle we installed — DT edits, the `b .` stub kernel, `p_paddr`
fixes, the cpusys-mimicry — ran the **stock** bundle. That is why every variant
behaved identically.

## With the direct path actually working

`tools/run_tvm.py` now sets SELinux permissive for the run (and restores it) and
checks the log for the fallback. Results, all with the stock `qcrosvm`:

| bundle in `firmware_mnt` | loaded from | result |
|---|---|---|
| stock files, byte-identical copy | `firmware_mnt` | auth OK, `started running`, host resets ~5–10 s later |
| stock content, **re-signed with qtestsign**, nothing changed | `firmware_mnt` | `gh_rm_vm_auth_image: VM_AUTH_IMAGE failed with err: -22` |
| re-signed, `iomemory-ranges`/`gic-irq-ranges` removed | `firmware_mnt` | `VM_AUTH_IMAGE failed -22` |

**`qtestsign` is not accepted.** The certificate names (`SecTools Test User`, …)
looked like test signing, but the hyp/TZ PIL check is anchored to Xiaomi's root
(`qcom,vm-signing-authority`); a dummy chain does not pass. `journal/09`'s
"accepted" result was the same fallback artifact, and its "patched byte without
re-signing fails" result was the only genuinely loaded image.

## Consequences

* **No custom code can run in `trustedvm`** (or any `secured` loader VM) without
  Xiaomi's signing key. The vendor-loader route is closed for a custom OS.
* The guest also needs a dm-verity system disk whose root hash is inside the
  signed bundle, and that disk (`/data/misc/vm-system/system.img`) is not even
  shipped — so the stock `trustedvm` cannot boot userspace on this ROM either.
* `/dev/hvc<vmid>` (hvc_gunyah) exists for guest consoles; for the stock
  `trustedvm` it stays empty — the guest dies before its console comes up.

## What is left for a custom OS

The only path that does not need a vendor signature is an **unauthenticated VM on
`/dev/gunyah`** (mainline ABI, crosvm). Re-tested without any pVM options:

```
crosvm run --disable-sandbox --hypervisor gunyah --mem 256 --cpus 1 … Image
gh_rsc_mgr …: RM rejected message 56000004. Error: 2
misc gunyah: Failed to start VM: -19
```

`0x56000004` is `VM_START`, RM error 2 is `NORESOURCE`; configure and init pass.
crosvm's generated `gunyah-vm-config` (with or without `compatible`/`vm-type`
added by overlay) does not change it. Diagnosing it needs the RM's own log, which
this production hyp does not expose (`/proc/tzdbg/hyp_log` is empty).

## First bisect of the crosvm `NORESOURCE`

The RM does parse crosvm's `gunyah-vm-config` (overlays applied with
`--device-tree-overlay` change the outcome):

| `vcpus` in the overlay | result |
|---|---|
| default (`affinity = "proxy"`, no map) | init OK, `VM_START` → `NORESOURCE` |
| `affinity = "proxy"; affinity-map = <0x05>` | same |
| `affinity = "pinned"` or `"static"`, map `<0x00>` | `VM_INIT` (`0x5600000b`) rejected, error 10 |

Dropping balloon/rng/usb/pmu (`--no-*`) changes nothing; crosvm still emits ten
doorbell vdevices. The start failure is not in the vCPU affinity and not in the
optional devices; memory lending (crosvm lends host user pages, the working VMs
use carve-outs) is the next suspect.

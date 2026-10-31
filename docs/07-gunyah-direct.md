# Driving Gunyah directly: it works, right up to the image load

Short version: an unprivileged-root helper **can** create a Gunyah VM, set its
firmware name and create a vCPU through `/dev/qgunyah`. It then fails in the
secure loader because the CMA pool is empty. So the door is open further than
expected — the blocker moved.

## The device is `/dev/qgunyah`, not `/dev/gunyah`

* `/dev/gunyah` (0666, `vendor_gunyah_dev`) accepts **no** ioctl in the
  `0xB200..0xB2FF` range at all
* `/dev/qgunyah` (0600, root) accepts them

The uapi is the Android `crosvm-gunyah` one, matching
`kernel-headers/linux/gunyah.h` in Xiaomi's partially-OSS kernel dump
(`lolipuru/device_xiaomi_houji-kernel`), with `GH_IOCTL_TYPE = 0xB2`:

| ioctl | on | meaning |
|---|---|---|
| `0xB201` | `/dev/qgunyah` | `GH_CREATE_VM`, **returns an fd** |
| `0xB240` | VM fd | `GH_CREATE_VCPU` (arg = index) |
| `0xB241` / `0x4010B241` | VM fd | `GH_VM_SET_FW_NAME`, `struct { char name[16]; }` — **the encoded form is required**, the raw one returns `EINVAL` |
| `0xB242` / `0x8010B242` | VM fd | `GH_VM_GET_FW_NAME` |
| `0xB243` | VM fd | `GH_VM_GET_VCPU_COUNT` |
| `0xB280` | vCPU fd | `GH_VCPU_RUN` |

`src/gh_probe.c` is a freestanding static binary (raw syscalls, no libc) that
walks the sequence. Build:

```bash
clang --target=aarch64-linux-gnu -ffreestanding -nostdlib -O1 -c src/gh_probe.c -o gh.o
ld.lld -m aarch64elf -static -e _start gh.o -o gh
adb push gh /data/local/tmp/ && adb shell su -c 'chmod 755 /data/local/tmp/gh && /data/local/tmp/gh'
```

## Observed

```
open:         0x3
create_vm:    0x4            <- VM fd, works
setfw:        0xfffffff4     <- -ENOMEM
vcpu_count:   0xffffffea     <- -EINVAL (not ready)
create_vcpu:  0x5            <- works
run:          0xffffffea     <- -EINVAL (VM not ready to start)
```

dmesg names the culprit:

```
gh_secure_vm_loader soc:gh-secure-vm-loader@0: Couldn't allocate cma memory for trustedvm -12
gh_secure_vm_loader soc:gh-secure-vm-loader@0: Loading secure VM trustedvm to memory failed -12
```

`set_fw_name` is what kicks off the secure-image load (firmware `%s.mdt` via
`mdt_loader`, authenticated through the RM: `gh_rm_vm_auth_image`), and that
allocation fails because:

```
CmaTotal: 589824 kB
CmaFree:       0 kB
```

The whole CMA pool is consumed by camera/display/DSP, so no room for a VM
image.

## What the name list actually accepts

Only `trustedvm` reached the loader. `oemvm` and `cpusys_vm` return `EINVAL`
from `set_fw_name` even though all three appear in the driver's
`gh_vm_names` table — and only `cpusys_vm.mdt` exists in
`/vendor/firmware_mnt/image` (`oemvm`/`trustedvm` boot blobs referenced by the
cmdline's `firmware_class.path` are **not present on this build**, and
`/data/misc/vm-system/` is empty).

## Next steps, in order of cost

1. **Free CMA**, then retry: close the camera, unload `msm_video`/`msm_eva`
   users, or drop `cma=` requests. If `CmaFree` goes to a few MB the load
   should proceed and produce a much more informative failure.
2. **Raise the pool** with a `cma=` kernel cmdline (boot.img cmdline is
   patchable with this repo's tooling) — but 576 MB total already looks like
   the DT's `linux,cma` reservation, so this may need a DT change.
3. **Supply firmware**: with only `cpusys_vm.mdt` on the box, a self-built VM
   still needs a Qualcomm-signed image (RM auth). That is the same signature
   wall as `hyp`, only for VM images instead of the hypervisor.

Nothing here needs EL2, so this route is independent of the
[Secure Launch](01-secure-launch-absent.md) dead end — it is *vendored
virtualization*, not `/dev/kvm`.

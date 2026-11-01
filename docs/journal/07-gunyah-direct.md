# Driving Gunyah directly: a VM really starts

A 1 KB freestanding binary running as root can create a Gunyah VM, get its
firmware authenticated and loaded, create the required vCPUs and **start it**.
The device then resets a few seconds later, because nothing services the
guest's virtio devices. That is the current edge.

## The device is `/dev/qgunyah`, not `/dev/gunyah`

* `/dev/gunyah` (0666, `vendor_gunyah_dev`) accepts **no** ioctl in
  `0xB200..0xB2FF` at all
* `/dev/qgunyah` (0600, root) does

uapi follows Android `crosvm-gunyah` (`kernel-headers/linux/gunyah.h` in
Xiaomi's partially-OSS dump, `lolipuru/device_xiaomi_houji-kernel`),
`GH_IOCTL_TYPE = 0xB2`:

| ioctl | on | meaning |
|---|---|---|
| `0xB201` | `/dev/qgunyah` | `GH_CREATE_VM`, **returns an fd** |
| `0xB240` | VM fd | `GH_CREATE_VCPU`, arg = index, must create `vcpu_count` of them |
| `0xB241` / `0x4010B241` | VM fd | `GH_VM_SET_FW_NAME`, `struct { char name[16]; }` — **encoded form required**, raw returns `EINVAL` |
| `0xB242` / `0x8010B242` | VM fd | `GH_VM_GET_FW_NAME` |
| `0xB243` | VM fd | `GH_VM_GET_VCPU_COUNT` |
| `0xB280` | vCPU fd | `GH_VCPU_RUN` (blocks while the guest runs) |

`src/gh_probe.c` walks it (freestanding, raw syscalls):

```bash
clang --target=aarch64-linux-gnu -ffreestanding -nostdlib -O1 -c src/gh_probe.c -o gh.o
ld.lld -m aarch64elf -static -e _start gh.o -o gh
adb push gh /data/local/tmp/ && adb shell su -c 'chmod 755 /data/local/tmp/gh && /data/local/tmp/gh'
```

## Prerequisites learned the hard way

* **CMA must be free.** `set_fw_name` allocates the whole VM image — 17408
  pages (68 MB) — from `trust_ui_vm_region@f3800000` (71 MB total). Every
  successful attempt holds it until the process dies, so it is roughly **one
  attempt per boot**; `cat /sys/kernel/debug/cma/trust_ui_vm_region@f3800000/used`
  (mount debugfs first) tells you where you stand. Stopping the camera frees
  ~35 MB in the shared pool but the dedicated region is what matters.
* **vCPU count must match exactly.** Creating one where two are allowed makes
  `GH_VCPU_RUN` fail with `EINVAL` and logs
  `VCPUs created 1 doesn't match with allowed 2`.
* **The firmware is not in `/vendor`.** `trustedvm.mdt`/`.b0x` live in
  `/product/vm-system/trustedvm/boot/`; `ueventd` bridges the load through
  `/vendor/bin/vmmgr -r`. `/vendor/vm-system/` does not exist on this build,
  which is why plain `/vendor/bin/vmmgr` finds no VMs.
* Only `trustedvm` is accepted by `set_fw_name` (`oemvm`, `cpusys_vm` return
  `EINVAL`).

## Observed

[evidence](../../evidence/gunyah-vm-start.txt): `VM: 45 status 2 complete`,
two `gh_proxy_sched` entries with VIRQs 406/407, `VM:45 started running`, and
the calling thread parked inside `GH_VCPU_RUN`.

Then the host resets within seconds. Likely candidates: the guest burns through
its boot and faults once it looks for virtio MMIO/console, or `gh_virt_wdt`
fires because nobody heartbeats. Either way the VM itself is up — the missing
piece is the device model.

## Next steps

1. **Virtio backends.** `gunyah_loader` exposes `gh_virtio_backend_ioctl`
   (`GH_GET_SHARED_MEMORY_SIZE` 0xB261, `GH_IOEVENTFD` 0xB262, `GH_IRQFD`
   0xB263, `GH_WAIT_FOR_EVENT` 0xB264, …) — a virtio-console backend on top of
   those would let the guest's output reach us and remove the blind spot.
2. **Watchdog.** Feed or disable `gh_virt_wdt` for vmid 45 so the host survives
   the experiment.
3. **Own payload.** Only Qualcomm-signed images authenticate; a self-built
   kernel still needs the vendor signature (`gh_rm_vm_auth_image`). The
   hypervisor-side config (`qcom,gh-secure-vm-loader` nodes) is not in the
   kernel DT either, so slot parameters come from XBL.

None of this needs EL2, so it is independent of the
[Secure Launch](01-secure-launch-absent.md) dead end — it is *vendored
virtualization*, not `/dev/kvm`.

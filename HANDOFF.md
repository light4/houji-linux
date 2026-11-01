# Handoff — state of the VM work on houji

Read this first, then `docs/17`, `docs/18`, `docs/16`, `docs/15`.

## Goal

Boot a *custom* guest (Arch/NixOS-class) in a Gunyah VM on the Xiaomi 14. No
proot, no `/dev/kvm` (that route is closed — `docs/01`, `docs/13`).

Two VM paths exist:

| path | state |
|---|---|
| **vendor loader** `/dev/qgunyah` (`GH_CREATE_VM` 0xB201 …) | VM creates, firmware authenticates, backends arm — **guest faults within ~1 s** |
| crosvm / pVM `/dev/gunyah` (0x47xx) | blocked: the vendor pvmfw registration step can't be satisfied (`docs/13`) |

All current work is on the **vendor loader** path.

## What is verified working

```
GH_CREATE_VM                      0xB201        -> vm fd
GH_VM_SET_FW_NAME (trustedvm)     0x4010B241    ok   (bundle must be signed; qtestsign is accepted)
GH_CREATE_VCPU x2                 0xB240        ok
GH_GET_SHARED_MEMORY_SIZE         0x8008B261    -> 0x108000
mmap(size, RW, SHARED, vm, label<<12)            ok, one mapping per label
GH_ACK_RESET      {label,0}       0x4008B26D    ok
GH_SET_DEVICE_FEATURES            0x4010B265    ok (features_sel 0 = per-device, 1 = VERSION_1)
GH_SET_QUEUE_NUM_MAX              0x4010B266    ok (per queue)
GH_SET_DEVICE_CONFIG_DATA         0x4010B267    ok (capacity as user pointer, 8 bytes)
GH_IRQFD   {label, flags=1, fd}   0x4010B263    ok   <-- flags must be 1
GH_IOEVENTFD {label, flags=1, q, fd} 0x4010B262 ok   <-- flags must be 1
GH_VCPU_RUN                       0xB280        blocks
```

`GH_WAIT_FOR_EVENT` (0xC010B264) has **never** returned an event.

## Hard facts

* labels are **hex**: `0x10` = disk (2 vqs), `0x11` = `hvc0` console (1 vq).
  Same values in `virtio-backends`, `qcom,label` and userspace.
* the guest DT ships in the bundle as `trustedvm.b02`, a 22-entry dtbo with
  `virtio-mmio` nodes at `0xda6f8000` (console) / `0xda6fc000` (disk). It is
  editable — we can re-sign.
* host-side virtio needs a **dtbo overlay** on `/hypervisor/qcom,gunyah-vm`
  declaring `virtio-backends` + `virtio_dev_<label>` children with `qcom,vm`,
  `qcom,label`, `shared-buffers-size`.
* overlay entries need the board ids (`qcom,msm-id`, `qcom/board-id`,
  `xiaomi,miboard-id`) or they are silently skipped; the DTBO table's data area
  must start after **all** entry structs (`header_size + n*entry_size`).
* loader lifecycle logs are `dev_dbg`: re-enable
  `file *gunyah* +p` after every boot. Watchdog resets ~1 s in, so sample dmesg
  within the first second.
* one VM per boot: every failed attempt leaks the 68 MB CMA allocation.
* a plain `>` redirect loses data on reset — tools open their own output file
  and `fsync` after each write.

## Next hypotheses (in order)

1. `GH_ACK_DRIVER_OK` (0xC004B26B / takes `__u32 label`) pre-set for the console
   before `VCPU_RUN`.
2. Compare the guest DT's `interrupts` for both nodes with the virqs the hyp
   assigns (dmesg `gh_proxy_sched`, interrupt allocation lines).
3. Re-sign `trustedvm.b02` with `console=hvc0` removed from
   `chosen.bootargs`, so the guest does not depend on a working console.
4. If the guest still faults: back only the console first and give it a trivial
   initramfs (our own b01/b03), i.e. take the vendor kernel out of the picture.

## Rollback / restore

```bash
fastboot flash dtbo_a ~/Downloads/houji_backup/dtbo_a.img   # stock overlay
fastboot flash boot   ~/playground/houji-kvm/baseline/boot_a_OS3.0.7.0_orig.img
```

Both stock images are byte-identical backups (sha256 in `docs/15` /
`README`). The overlay currently on `dtbo_a` is the `shared-buffers-size =
0x4000` variant and boots fine.

## Inventory

* `src/gh_probe.c`, `src/gh_virtio_probe.c`, `src/gh_virtio_blk_probe.c` — the
  probe tools (freestanding, raw syscalls, fsync'd logging)
* `~/playground/houji-kvm/` — `patch.py`, `run.sh`, stub sources, baseline
  `boot_a` dump
* on device `/data/local/tmp`: `ghc4`, `ghd2`, `ghe` (latest), `pvmfw.img`,
  `trustedvm.*` bundles, `disk1.img`, `crosvm-a16*`
* `/tmp` on the host: `rom_kernel.img` (kernel dump used for RE), `tv/`
  (extracted vendor bundle), overlay sources `a.dts`, `b.dts`, `c.dts`,
  `dtbo_*.img`, `zget*.py` (factory-image range extractor)

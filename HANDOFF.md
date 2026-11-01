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

## Corrections from `docs/19` (read it)

* the VM **does** start (`started running`); the reset is the guest faulting.
* the dtbo overlay is **not** merged into the Linux DT and the loader recognizes
  the backends from `trustedvm.b02` alone — the overlay is irrelevant, keep the
  stock dtbo.
* `GH_ACK_DRIVER_OK` is **not implemented** (`cmd not supported`).
* capture the loader log with `dmesg -w > file &` **plus**
  `while true; do sync; done`, otherwise nothing survives the reset.

## Key asset: `/system_ext/bin/qcrosvm` (see `docs/20`)

The vendor's Rust launcher is on the phone and is the reference implementation
(`/dev/qgunyah`, `/dev/gh_virtio_backend_<label>`,
`/sys/kernel/load_guestvm_<vm>/boot_guestvm`). Use it as an oracle or launcher.
`qcrosvm -l` exits 1 (no sysfs dir on this ROM); without `-l` it dies at
"Get reserved mem size" `EINVAL`. `trustedvm` is not in
`/vendor/etc/qvirtmgr-vndr.json`, so nothing auto-starts it.

## Full ioctl table: `docs/21`

`qcrosvm`'s commands, both numbering families, and which of them this driver
accepts. `ACK_DRIVER_OK` does **not** exist here; the driver only speaks the
legacy family. Two steps in its order were missing from our tool: `0xB245`
(get vcpu count) and `0x8008B261` returning the *reserved memory size* (we got
`0` and ignored it while hardcoding 2 vCPUs).

## Next hypotheses (in order)

1. find the feature bits the driver accepts for word 1 (`set_features 1/0x1e
   failed ret 30`): try 0x01 alone, then 0x00.
2. `trustedvm.b02` is a **template** (`generate = "/virtio-mmio"`, no `reg`, no
   `interrupts`). Let `vendor.qvirtmgr`/`qvirtservice` bring up `trustedvm` and
   look at the guest DT they produce, then diff.
3. patch `trustedvm.b02` into a static guest DT (we can re-sign it), drop
   `console=hvc0` from `chosen.bootargs` in the same pass.

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

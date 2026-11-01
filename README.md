# houji (Xiaomi 14) — Gunyah VM research

Getting a **custom** guest (Arch/NixOS-class, no proot) to run in a Gunyah VM on
the Xiaomi 14. `/dev/kvm` is not part of this: it is closed on this firmware
(see below), so the work targets Qualcomm's own hypervisor instead.

Status: **blocked — no route runs custom code on this firmware.** Read
[docs/FINDINGS.md](docs/FINDINGS.md) for the one-page result and its honest
boundary, [docs/STATE.md](docs/STATE.md) for exact current state, and
[docs/ROADMAP.md](docs/ROADMAP.md) for what is left.

## Where it stands

| path | state |
|---|---|
| vendor loader `/dev/qgunyah` | works for vendor-signed VMs only; `qtestsign` re-signing is **rejected** (`VM_AUTH_IMAGE` -22) |
| crosvm `/dev/gunyah` (own kernel) | RM rejects `VM_START` with `NORESOURCE`, protected or not |
| `/dev/kvm` | closed: EL2 belongs to Gunyah from XBL on |

The loader path is mapped in detail (ioctls, bundle/dtbo formats, labels), but
every VM on it is authenticated against Xiaomi's root, and the unauthenticated
crosvm path is refused by the Resource Manager. See
[docs/FINDINGS.md](docs/FINDINGS.md).

## Why `/dev/kvm` is not the answer here

The kernel is ready (`CONFIG_KVM=y`, `kvm-arm.mode=protected`) but KVM needs EL2
and EL2 is Gunyah's. Both hand-back routes are closed:

* Secure Launch (`slbounce`) — not implemented in this firmware: no `mssecapp`,
  no `hyp_manager_launch`, and the config key is parsed into a field nobody
  reads ([journal/01](docs/journal/01-secure-launch-absent.md)).
* Gunyah handing EL2 back — no such call in the ABI
  ([journal/03](docs/journal/03-gunyah-hvc-abi.md)).

AVF (Terminal app, `android.system.virtualmachine`) is additionally blocked
because this build ships no Gunyah backend at all — it is crosvm-on-KVM, so
forcing `protected_vm.supported` only moves the failure to the missing
`/dev/kvm` ([journal/06](docs/journal/06-avf-is-kvm-only.md)).

## Tripwires after a firmware update

Any hit below re-opens the work; details in
[docs/FINDINGS.md](docs/FINDINGS.md#tripwires-for-a-firmware-update).

```bash
# Secure Launch / slbounce
adb shell su -c 'strings /dev/block/by-name/hyp_a | grep -E "mssecapp|hyp_manager_launch"'
# Gunyah backend in the AVF APEX
adb shell su -c 'strings /apex/com.android.virt/bin/virtualizationservice | grep -c gunyah'
# newer driver ABI that the vendor launcher expects (0 today)
adb shell su -c 'ls /sys/kernel/load_guestvm_*'
```

## Layout

```
src/probe_stub.S      general SMCCC prober (returns to kmain on any error)
src/ident.S           `hvc #0x6000` hypervisor_identify probe
src/stub_scan2.S      64-FID bitmap scanner
src/stub_nop2.S       minimal "write a marker and return" stub
src/gh_probe.c        drives /dev/qgunyah: create VM, firmware, vCPUs
src/gh_virtio_probe.c backend sequence for one label
src/gh_virtio_blk_probe.c  same, block device, two labels
tools/qvirt_start.c   drive the stock vendor.qti.qvirt service (getVm/getState/start)
tools/run_tvm.py      install a bundle, start it with stock qcrosvm, collect logs (permissive)
patch.py, run.sh      boot.img stub harvester
tools/patch_b02.py    replace one guest-DT entry in a bundle
tools/rebuild_bundle.py  re-assemble, sign and split a bundle
docs/FINDINGS.md      one-page results and what is still unknown
docs/STATE.md         current state and verified call sequence
docs/ROADMAP.md       next moves and parked routes
docs/RESTORE.md       rollback, stock hashes, dead-hang recovery
docs/reference/       ABI, bundle/dtbo formats, harness
docs/journal/         chronological lab notebook
evidence/             raw probe output, partition greps
```

`baseline/` keeps a byte-exact `boot_a` dump (96 MB, not in git) — see
[docs/RESTORE.md](docs/RESTORE.md).

## Safety

Only `boot_a` is ever flashed by the harness, and the rollback is the same
one-line `fastboot flash boot`. A bad stub dead-hangs; the bootloader watchdog
restores fastboot in ~2 min, or power-cycle and hold Vol-Down+Power.

Read [the pitfalls](docs/journal/04-harness-pitfalls.md) before writing a stub:
MMU-off unaligned access, BTI landing pads, `x0` = DTB, rodata write alignment.

[qtestsign]: https://github.com/msm8916-mainline/qtestsign

# houji-linux — Arch Linux / NixOS on the Xiaomi 14

Research and tools for running a **custom Linux** (Arch Linux ARM, NixOS) on the
Xiaomi 14 (houji, Snapdragon 8 Gen 3 / SM8650, HyperOS 3.0.7.0), without proot.
It started as a hunt for `/dev/kvm`, went deep into Qualcomm's Gunyah hypervisor,
and now compares three practical routes.

## Where it stands

| route | state |
|---|---|
| `/dev/kvm` | **closed**: EL2 belongs to Gunyah from XBL on |
| VM on the vendor loader `/dev/qgunyah` | **closed for custom code**: only Xiaomi-signed VMs; `qtestsign` is rejected (`VM_AUTH_IMAGE` -22) |
| unauthenticated VM on `/dev/gunyah` (crosvm, own kernel) | **blocked**: the Resource Manager refuses `VM_START` with `NORESOURCE` |
| native Linux as the primary OS (LineageOS-style) | **open**: stock GKI 6.1 + vendor modules usable, kernel needs `DEVTMPFS`/`FHANDLE`/namespaces |
| real chroot on the Android kernel | **works today**, not an independent system |

Start here:

* [docs/OPTIONS.md](docs/OPTIONS.md) — the three ways forward, compared, with plans
* [docs/FINDINGS.md](docs/FINDINGS.md) — one-page results of the virtualization work
  and its honest boundary
* [docs/STATE.md](docs/STATE.md) — exact current state, verified call sequences
* [docs/RESTORE.md](docs/RESTORE.md) — rollback and stock hashes
* [docs/README.md](docs/README.md) — full index, reference docs and the lab journal

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
docs/OPTIONS.md       three ways to run Arch/NixOS, compared
docs/FINDINGS.md      one-page results of the virtualization work
docs/STATE.md         current state and verified call sequences
docs/ROADMAP.md       open question for the VM route (option 3)
docs/RESTORE.md       rollback, stock hashes, dead-hang recovery
docs/reference/       /dev/qgunyah ABI, bundle/dtbo formats, harness
docs/journal/         chronological lab notebook (01–43)

src/probe_stub.S      general SMCCC prober (returns to kmain on any error)
src/ident.S           `hvc #0x6000` hypervisor_identify probe
src/stub_scan2.S      64-FID bitmap scanner
src/stub_nop2.S       minimal "write a marker and return" stub
src/gh_probe.c        drives /dev/qgunyah: create VM, firmware, vCPUs
src/gh_virtio_probe.c backend sequence for one label
src/gh_virtio_blk_probe.c  same, block device, two labels
patch.py, run.sh      boot.img stub harness (splice a stub into boot_a, classify the boot)

tools/run_tvm.py      install a trustedvm bundle, start it with stock qcrosvm, collect logs
tools/qvirt_start.c   drive the stock vendor.qti.qvirt service (getVm/getState/start)
tools/patch_b02.py    replace one guest-DT entry in a bundle
tools/rebuild_bundle.py  re-assemble, qtestsign and split a bundle (structurally valid;
                      the signature is rejected on this device)

evidence/             raw probe output, partition greps
```

`baseline/` keeps a byte-exact `boot_a` dump (96 MB, not in git) — see
[docs/RESTORE.md](docs/RESTORE.md).

## Safety

The harness only ever flashes `boot_a`; the virtualization experiments touched
`dtbo_a` and files on `/vendor/firmware_mnt`. Every one of those has a stock backup
and a one-line rollback in [docs/RESTORE.md](docs/RESTORE.md). A bad stub dead-hangs; the bootloader watchdog
restores fastboot in ~2 min, or power-cycle and hold Vol-Down+Power.

Read [the pitfalls](docs/journal/04-harness-pitfalls.md) before writing a stub:
MMU-off unaligned access, BTI landing pads, `x0` = DTB, rodata write alignment.


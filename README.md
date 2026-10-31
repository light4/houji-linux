# Xiaomi 14 (houji) — EL2 / `/dev/kvm` research

Why this repo exists: to stop the next person (likely future me) from
re-running a week of dead ends on Android virtualization on this device.

## Conclusion first

**This firmware cannot give you `/dev/kvm`.** The kernel side is already
ready — `CONFIG_KVM=y` and `kvm-arm.mode=protected` are in the shipped
cmdline — but KVM needs the CPU at EL2, and on houji EL2 belongs to Qualcomm's
Gunyah hypervisor from XBL onwards. Both escape routes are closed:

| Route to EL2 | State on this firmware |
|---|---|
| Secure Launch handover ([slbounce](https://github.com/TravMurav/slbounce)) | **not implemented** — no `mssecapp`, no `hyp_manager_launch`; the config key is parsed into a field nobody reads ([01](docs/01-secure-launch-absent.md)) |
| Gunyah handing EL2 back | **no such call in the ABI** — Gunyah's host ABI is `hvc 0x6000+`, a VM-management API only ([03](docs/03-gunyah-hvc-abi.md)) |

The Android virtualization framework (Terminal app, `vm`, third-party
`android.system.virtualmachine` apps) is blocked too, but for a second,
independent reason: this AVF build ships **no Gunyah backend** — it is
crosvm-on-KVM, so forcing the `protected_vm.supported` property only moves the
failure to the missing `/dev/kvm` ([06](docs/06-avf-is-kvm-only.md)).

The `0x8600_xxxx` vendor SMC range is not Gunyah at all, it is SMCI to
TrustZone — probing it is a dead end ([02](docs/02-smc-is-smci.md)).

Qualcomm's own QVT stack, by contrast, does work — `vendor.qvirtmgr.cpusys_vm`
is `RUNNING` and a Gunyah guest is live right now — but it is vendor-private.

That stack can also be driven from a root tool without the vendor service —
and it **does** start a VM: `/dev/qgunyah` accepts `GH_CREATE_VM`,
`GH_VM_SET_FW_NAME`, `GH_CREATE_VCPU` and `GH_VCPU_RUN`, the firmware
`trustedvm` gets authenticated and loaded, both proxy-scheduled vCPUs come up
(`gh_proxy_sched: vmid=45 … virq_num=406/407`) and the loader logs
`VM:45 started running`. The host resets a few seconds later because no virtio
backends service the guest. See [07](docs/07-gunyah-direct.md) and
`src/gh_probe.c`.

Bootstrapping a *custom* system (own kernel, own rootfs, no proot) is **not
blocked by signing**: the SVM images are *test*-signed, and re-signing with
[qtestsign] is accepted by `gh_rm_vm_auth_image` — verified by booting a
deliberately patched kernel after re-signing it. What remains is a device model
(virtio backends), a console and a watchdog keep-alive, in that order. See
[09](docs/09-custom-system.md).

Booting our **own** distribution this way is a signature problem, not a
hypervisor one: flipping a single byte in the guest kernel makes
`gh_rm_vm_auth_image` refuse the image, the rootfs is dm-verity protected with a
signed root hash, and there is no auth-off switch. See
[08](docs/08-arch-feasibility.md).

## Tripwire

Re-check after firmware updates. One command, no root files needed beyond su:

```bash
adb shell su -c 'strings /dev/block/by-name/hyp_a | grep -E "mssecapp|hyp_manager_launch"'
```

Any hit means Secure Launch shipped, and the harness in `src/` is the right
starting point again (`src/probe_stub.S` already has the EL1→EL2 entry
skeleton).

For the framework route, watch for a Gunyah backend appearing in the APEX:

```bash
adb shell su -c 'strings /apex/com.android.virt/bin/virtualizationservice | grep -c gunyah'
```

`0` today. A non-zero count means AVF stopped depending on `/dev/kvm` and the
Terminal route re-opens independently of Secure Launch.

## Layout

```
patch.py            hijack arm64 Image entry in a dumped boot.img, splice a stub
run.sh              flash + boot + classify (BOOTED / REBOOTED-TO-FASTBOOT / DEAD)
src/probe_stub.S    general SMCCC prober, records into linux_banner, safe fallback
src/ident.S         hvc #0x6000 hypervisor_identify probe
src/stub_scan2.S    64-FID bitmap scanner
src/stub_nop2.S     minimal "write a marker and return" stub
src/gh_probe.c      freestanding tool that drives /dev/qgunyah directly
docs/               findings, harness internals, pitfalls, reproduction, AVF backend check, Arch feasibility, custom-system plan, ROM inventory
evidence/           raw probe output, partition greps
```

`baseline/` holds a byte-exact `boot_a` dump (`sha256
c88537b854911c142a920989865b663a5b7e0b3faab7025a7858b7a7adddafa9`, OS 3.0.7.0).
It is 96 MB and kept out of git; dump it yourself with
`dd if=/dev/block/by-name/boot_a` (see [reproduction](docs/05-reproduction.md)).

## Quickstart

```bash
fastboot flash boot baseline/boot_a_OS3.0.7.0_orig.img   # rollback, always works
python3 patch.py baseline/boot_a_OS3.0.7.0_orig.img build/x.img --stub probe_stub.S
./run.sh build/x.img
adb shell su -c 'dmesg | head -3'                        # stub output lands here
```

Results are reported by overwriting part of the `Linux version …` banner the
kernel prints on every boot — no framebuffer driver, no debug channel.

## Safety

Nothing is written to flash except `boot_a`, and the rollback is that same
one-line `fastboot flash boot`. A bad stub dead-hangs (bootloader watchdog
brings fastboot back after ~2 min, or power-cycle and hold Vol-Down+Power).
`hyp_a`/`xbl` are signature checked by XBL; flashing a self-built Gunyah is a
separate, riskier project.

## Read the pitfalls before writing a stub

[04-harness-pitfalls.md](docs/04-harness-pitfalls.md) — MMU-off unaligned
access, BTI landing pads, `x0` = DTB, rodata write alignment. Each one cost
a debugging cycle here.

[qtestsign]: https://github.com/msm8916-mainline/qtestsign

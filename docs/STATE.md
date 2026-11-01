# Current state

State after `journal/42`. **Read journal/42 first: bundle experiments 25–39 never loaded the patched bundle, and `qtestsign` is rejected.** Summary:
[FINDINGS](FINDINGS.md). Rollback: [RESTORE](RESTORE.md).

## Where the work stands

The goal is a *custom* guest (Arch/NixOS-class) on houji, without proot and
without `/dev/kvm`. Two Gunyah paths exist:

| path | state |
|---|---|
| vendor loader `/dev/qgunyah` | create VM, authenticate firmware, arm backends, vCPU entered and blocked — **guest never executes, VM torn down ~0.2 s later** |
| crosvm / pVM `/dev/gunyah` | blocked: no vendor pvmfw, `RM start` → `ENODEV` |

**The vendor launcher fails identically** (`journal/41`): `qcrosvm` with the
untouched bundle reaches `started running` and the phone resets — so the failure is
not in our tool. `/dev/qgunyah` itself works: `cpusys_vm` runs from boot through
`qcrosvm`. The ROM matches `~/Downloads/houji_rom` OS3.0.7.0 except KernelSU's
`init_boot`.

## Verified working

```
GH_CREATE_VM                        0xB201        -> vm fd
GH_VM_SET_FW_NAME ("trustedvm")     0x4010B241    ok  (vendor-signed bundle only; qtestsign → VM_AUTH_IMAGE -22)
GH_GET_VCPU_COUNT                   0xB243        -> 2
GH_VM_RESERVED_MEMORY_SIZE          0x8008B261    -> 0x108000
GH_CREATE_VCPU (arg = index)        0xB240        ok
mmap(size, RW, SHARED, vm, label<<12)              ok, one mapping per label
GH_ACK_RESET                        0x4008B26D    ok
GH_SET_DEVICE_FEATURES              0x4010B265    ok; only word1 0x1f is accepted
GH_SET_QUEUE_NUM_MAX                0x4010B266    ok (per queue)
GH_SET_DEVICE_CONFIG_DATA           0x4010B267    ok (capacity, 8 bytes)
GH_IRQFD           (flags must be 1) 0x4010B263   ok
GH_IOEVENTFD       (flags must be 1) 0x4010B262   ok, per queue
GH_VCPU_RUN                         0xB280        blocks; VM logs "started running"
```

`GH_WAIT_FOR_EVENT` (`0xC010B264`) has never returned an event, not even the
device-reset event the guest would produce first. Full table and traps:
[reference/qgunyah-abi](reference/qgunyah-abi.md).

## Hard facts

* labels are **hex**: `0x10` = read-only system disk, `0x11` = read-write persist
  disk — both virtio-block (`qvirtmgr-vndr.json`, `journal/41`).
* the guest DT ships in the bundle as `trustedvm.b02` — a 22-entry **dtbo
  template** (`virtio-mmio@N/memory { qcom,label, base }`, no `reg`, no
  `interrupts`), windows at `0xda6f8000` (label `0x11`) / `0xda6fc000` (label `0x10`). Entry
  13 is the only variant matching this board; the others are for other SoCs.
* neither the dtbo overlay nor the bundle's guest DT drives the device list:
  stripping every virtio node from all 22 `b02` entries (and cutting the table to
  one entry) still yields `Recognized virtio backend device with label 10/11`.
  Roof level: the hyp/RM decides the composition (`journal/36`). Stock `dtbo_a`
  is the safe state.
* there is **no boot-context ioctl**: guest entry state is hyp-side.
* one VM per boot — every failed attempt leaks the 68 MB CMA region.
* no `/dev/gh_virtio_backend_<label>` nodes exist; `qcrosvm` expects them
  (`journal/26`).
* `trustedvm` **is** in `/vendor/etc/qvirtmgr-vndr.json` (on demand, for Trusted
  UI), but its system disk `/data/misc/vm-system/system.img` does not exist, so the
  stock start fails — while the service still reports `RUNNING` (`journal/41`).
  `0x4020B245` and the `0xBCxx` family are `EINVAL` here.

## Bundle patching does NOT work

Re-signed bundles fail `VM_AUTH_IMAGE` once really loaded; the earlier "authenticates"
was the stock bundle via silent fallback (`journal/42`). The tools still build valid
MBN layouts:
`tools/patch_b02.py` + `tools/rebuild_bundle.py`, see `journal/25`. The device is
back on the **stock** bundle; deleting `/vendor/firmware_mnt/image/trustedvm.*` is
what keeps it there.

## Further facts

* `/proc/vmkmsg` (`dmesg_dumper`) is the channel a guest uses to hand its kernel
  log to the host — present, but empty because the guest dies first
  (`journal/28`).
* **the guest kernel is not the problem.** A stub guest whose kernel is a single
  `b .` instruction dies exactly like the vendor kernel (`journal/29`).
* every variant dies ~0.2 s after `VCPU_RUN`; how long the *host* survives
  depends only on `crash-fatal` (`journal/34`).
* the backend eventfds (`irqfd`, `ioeventfd`) never fire and
  `GH_WAIT_FOR_EVENT` never returns: the driver sees no guest activity at all —
  so there is no evidence the guest ever executes an instruction
  (`journal/31`).
* arm64 has no `poll(2)`; use `ppoll` or non-blocking eventfd reads.
* `VCPU_RUN` does reach the hypervisor (the `started running` print sits right
  after that call in `gh_vcpu_ioctl`), but the driver exposes **no vCPU
  state/register ioctl** — the backend event path is the only observable
  (`journal/32`).
* **segment indices shift after signing**: `b02` is the kernel in a delivered
  bundle, `b01` is the hash block (`journal/30`).
* `VCPU_RUN` **blocks** (verified with `timeout`: `RC=124`) — the vCPU is entered
  and parked in the hypervisor; the VM is torn down a few seconds later while the
  call is still blocked (`journal/37`).
* segments are placed by `p_paddr` (guest IPA): `cpusys_vm` has
  `p_paddr == memory base` for every load. `trustedvm`'s kernel has `p_paddr = 0`
  and fixing it to `0x28800000` changed nothing by itself (`journal/38`), but the
  convention should be kept in every future build.
* a variant that mimics the working VM completely (no virtio vdevices, its
  `vm-attrs`, `nokaslr`, kernel `p_paddr` = memory base) still dies in ~0.2 s
  (`journal/39`). The reachable configuration space is exhausted; the remaining
  differences are host/hyp-side or inside the vendor launcher.
* the shipped launcher `qcrosvm` **does** run here (it starts `cpusys_vm` at boot);
  `journal/40`'s "cannot run" was caused by our `--mem` argument (`journal/41`).
* proxy scheduling was a suspect (the working VM does not use it) but a
  controlled 3+3 run shows it makes no difference (`journal/33`, corrected in
  `journal/34`). The vCPU never executes in any variant, ~0.2 s after
  `VCPU_RUN` — long host survival is not guest liveness.

## The known-good VM, as a reference

`cpusys_vm`'s own bundle sits in `/vendor/firmware_mnt/image/cpusys_vm.*`; its
config segment is a plain FDT with `affinity = "pinned"`, extra `vm-attrs`
(`no-shutdown`, `ras-error-handler`) and **no virtio backends at all**
(`journal/35`). `pinned` changes nothing either; the "no virtio at all" shape was
what made a backend-free variant the obvious next step — it did not help
(`journal/39`).

## Tooling notes that save a round

* loader lifecycle logs are `dev_dbg`: re-enable with
  `echo 'file *gunyah* +p' > /sys/kernel/debug/dynamic_debug/control` after
  every boot.
* the reset kills one-shot `dmesg`; capture with `dmesg -w > file &` **plus**
  `while true; do sync; done &`, see [reference/harness](reference/harness.md).
* probe tools open their own output file and `fsync` after each write; a shell
  `>` redirect loses everything across the reset.

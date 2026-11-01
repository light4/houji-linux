# Current state

Updated after `journal/24`. Read with [ROADMAP](ROADMAP.md) and
[RESTORE](RESTORE.md).

## Where the work stands

The goal is a *custom* guest (Arch/NixOS-class) on houji, without proot and
without `/dev/kvm`. Two Gunyah paths exist:

| path | state |
|---|---|
| vendor loader `/dev/qgunyah` | create VM, authenticate firmware, arm both virtio backends, start — **guest dies in <0.5 s** (hard reset, no panic) |
| crosvm / pVM `/dev/gunyah` | blocked: no vendor pvmfw, `RM start` → `ENODEV` |

Host-side work on the vendor path is **effectively complete**; the remaining
failure is on the guest side (see [ROADMAP](ROADMAP.md)).

## Verified working

```
GH_CREATE_VM                        0xB201        -> vm fd
GH_VM_SET_FW_NAME ("trustedvm")     0x4010B241    ok  (bundle must be signed; qtestsign passes)
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

* labels are **hex**: `0x10` = disk (2 vqs), `0x11` = `hvc0` console (1 vq).
* the guest DT ships in the bundle as `trustedvm.b02` — a 22-entry **dtbo
  template** (`virtio-mmio@N/memory { qcom,label, base }`, no `reg`, no
  `interrupts`), windows at `0xda6f8000` (console) / `0xda6fc000` (disk).
* the dtbo overlay we build is **not** merged into the Linux DT; the loader reads
  the backends from the bundle. Stock `dtbo_a` is the safe state.
* there is **no boot-context ioctl**: guest entry state is hyp-side.
* one VM per boot — every failed attempt leaks the 68 MB CMA region.
* no `/dev/gh_virtio_backend_<label>` nodes exist; `qcrosvm` expects them
  (`journal/26`).
* `trustedvm` is not in `/vendor/etc/qvirtmgr-vndr.json` and nothing on this ROM
  ever starts it; the shipped launcher `/system_ext/bin/qcrosvm` targets a newer
  ABI than this kernel (`0x4020B245` and the whole `0xBCxx` family are `EINVAL`).

## Bundle patching is proven

Re-signing a structurally modified `b02` authenticates
(`VM: 45 status 2 complete`), so guest-side changes are practical:
`tools/patch_b02.py` + `tools/rebuild_bundle.py`, see `journal/25`. A patched
bundle is installed on the device right now; delete
`/vendor/firmware_mnt/image/trustedvm.*` to fall back to the stock one.

* `/proc/vmkmsg` (`dmesg_dumper`) is the channel a guest uses to hand its kernel
  log to the host — present, but empty because the guest dies first
  (`journal/28`).
* b02 entry 13 is confirmed to be the only variant matching this board, so patch
  experiments on it are meaningful (`journal/28`).

* **the host flow is not the problem.** A stub guest (single `b .` instruction)
  survives ~10 s where the vendor kernel dies in ~0.5 s — the vendor guest kills
  itself, early, on its own (`journal/29`, corrected in `journal/30`).
* an inert guest is eventually abandoned (~10 s), so something waits for
  progress; that is separate from the 0.5 s kernel death.
* the backend eventfds (`irqfd`, `ioeventfd`) never fire and
  `GH_WAIT_FOR_EVENT` never returns: the driver sees no guest activity at all —
  so there is no evidence the guest ever executes an instruction
  (`journal/31`).
* arm64 has no `poll(2)`; use `ppoll` or non-blocking eventfd reads.
* **segment indices shift after signing**: `b02` is the kernel in a delivered
  bundle, `b01` is the hash block (`journal/30`).
* the device is on the **stock** bundle again; the stub bundle lives in `/tmp`
  (`outl/`, `signed_loop`, `comb_loop`).

## Tooling notes that save a round

* loader lifecycle logs are `dev_dbg`: re-enable with
  `echo 'file *gunyah* +p' > /sys/kernel/debug/dynamic_debug/control` after
  every boot.
* the reset kills one-shot `dmesg`; capture with `dmesg -w > file &` **plus**
  `while true; do sync; done &`, see [reference/harness](reference/harness.md).
* probe tools open their own output file and `fsync` after each write; a shell
  `>` redirect loses everything across the reset.

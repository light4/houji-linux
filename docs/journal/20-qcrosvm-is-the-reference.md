# The vendor's own implementation is on the phone: `qcrosvm`

`/system_ext/bin/qcrosvm` (200 KB, Rust) is the vendor's Gunyah VM launcher and
the reference for the whole backend protocol. `vendor.qti.qvirt-service_rs`
(pid 1149, always running) shells out to it:

```
qcrosvm --disk=<img>,label=<n>[,rw] --vm=<name> --vsock=label=<n>,cid=<n> --mem=<mb>
        [-l] [-s] [--log=type=logcat|term|ftrace]
```

It opens `/dev/qgunyah`, `/dev/gh_virtio_backend_<label>` and, in `-l` mode,
`/sys/kernel/load_guestvm_<vm>/boot_guestvm`. Its strings enumerate the exact
sequence we reverse-engineered by hand (create vm, set fw name, get vcpu count,
get shared memory size, per-disk ioeventfd/irqfd, vhost-vsock, vcpu threads,
boot).

## What the ROM actually bootstraps

* `vendor.qvirtmgr.cpusys_vm.status = RUNNING`
* `vendor.qvirtmgr.oemvm.status = NOT_STARTED`
* **no `trustedvm` entry at all** (`/vendor/etc/qvirtmgr-vndr.json`) — nothing on
  this ROM ever starts the SVM, so the "vendor worked example" only exists for
  `cpusys_vm`, and that VM has no virtio backends.
* `/sys/kernel/load_guestvm*` does not exist, so `qcrosvm -l` bails out
  immediately (`rc=1`, no message).
* without `-l`, `qcrosvm` reaches
  `Error: Get reserved mem size failed (Invalid argument (os error 22))`
  at `main.rs:941` and aborts (`rc=134`) — before any backend work.
* `/vendor/bin/vmmgr` is only the firmware *reporter* on this kernel
  (invoked by ueventd with `FIRMWARE=<name>`, prints the path, exits).

## Why this matters

Reimplementing the sequence by hand got us to `started running` with a guest
that dies instantly. `qcrosvm` encodes the vendor's own ordering, feature set,
memory-region negotiation and boot handshake. Two realistic uses:

1. **as an oracle** — disassemble `main.rs` around 0x900-0xb00 (strings are
   interleaved with the call sites) and copy the exact ioctl arguments,
   especially around `Get reserved mem size` and the shared-memory mmap, where
   our hand-rolled version guesses.
2. **as the launcher** — once the `EINVAL` is understood, drive `trustedvm`
   with it and read the guest console through its logging instead of ours.

## Next steps

1. find the ioctl behind "Get reserved mem size" and why it answers `EINVAL`:
   dump the instructions around `main.rs:941` and compare with the
   `kernel-headers/linux/gunyah.h` definitions (this is the qvm ABI, not the
   generic gunyah one).
2. find what creates `/sys/kernel/load_guestvm_<vm>`: it is a *loader-side*
   directory, so the VM must be created through the kernel path (`-l`), which is
   the route `qvirtservice` uses for `cpusys_vm` at boot.
3. diff our ioctl trace against `qcrosvm`'s for the same labels before touching
   the guest DT again — the DT is probably fine and our setup order is not.

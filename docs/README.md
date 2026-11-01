# Docs

Start with [STATE](STATE.md) (current), [ROADMAP](ROADMAP.md) (next) and
[RESTORE](RESTORE.md) (rollback).

## Reference

* [qgunyah-abi](reference/qgunyah-abi.md) — implemented ioctl surface, traps
* [bundle-format](reference/bundle-format.md) — MBN bundle and dtbo containers,
  signing flow
* [harness](reference/harness.md) — toolchain, logging across a reset, stub
  pitfalls

## Journal (chronological, superseded in places)

The journal is the lab notebook: what was tried, what it showed, and what was
later corrected. Where it disagrees with `reference/`, the reference wins.

### Route analysis

| | |
|---|---|
| [01](journal/01-secure-launch-absent.md) | Secure Launch / slbounce absent from this firmware |
| [02](journal/02-smc-is-smci.md) | `0x8600_xxxx` is SMCI to TrustZone, not Gunyah |
| [03](journal/03-gunyah-hvc-abi.md) | Gunyah host ABI has no EL2 hand-back |
| [06](journal/06-avf-is-kvm-only.md) | AVF here is crosvm-on-KVM; no Gunyah backend |
| [08](journal/08-arch-feasibility.md) | what a custom distro would need |
| [11](journal/11-crosvm-pvmfw.md) | crosvm / pVM route and its pvmfw blocker |
| [12](journal/12-pvmfw-obtained.md) | pvmfw extracted from a Pixel image |
| [13](journal/13-avf-through-vendor-path.md) | forcing the AVF path, full-partition backup recipe |

### Vendor loader route

| | |
|---|---|
| [07](journal/07-gunyah-direct.md) | `/dev/qgunyah` drives a real VM |
| [09](journal/09-custom-system.md) | test-signing: `qtestsign` is accepted |
| [10](journal/10-rom-inventory.md) | ROM inventory, where the bundle lives |
| [14](journal/14-virtio-needs-dt.md) | a started guest has no devices without a DT |
| [15](journal/15-dtbo-overlay-virtio.md) | dtbo overlay declares the backends |
| [16](journal/16-virtio-backend-sequence.md) | full backend sequence and its traps |
| [17](journal/17-backend-up-guest-silent.md) | host armed, guest silent |
| [18](journal/18-guest-dt-in-bundle.md) | guest DT ships in `trustedvm.b02`; label map |
| [19](journal/19-loader-state-visibility.md) | how to see the loader log; overlay is a red herring |
| [20](journal/20-qcrosvm-is-the-reference.md) | `qcrosvm` is the vendor reference launcher |
| [21](journal/21-qcrosvm-ioctl-table.md) | its ioctl table, both ABI families |
| [22](journal/22-guest-never-touches-the-window.md) | guest dies before virtio init |
| [23](journal/23-guest-dies-in-300ms.md) | <0.5 s hard reset, ABI skew suspicion |
| [24](journal/24-no-boot-context-ioctl.md) | no boot-context ioctl exists |

### Harness

| | |
|---|---|
| [04](journal/04-harness-pitfalls.md) | stub pitfalls (read before writing one) |
| [05](journal/05-reproduction.md) | reproduce the probe setup |

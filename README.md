[English](README.md) | [中文](README.zh.md)

# houji-linux

Running Linux (Debian sid, Arch Linux ARM, NixOS) on the Xiaomi 14 (houji, SM8650;
HyperOS 3.0.7.0 China, now 3.0.304.0 Global),
without proot.

## Status

| route | state |
|---|---|
| `/dev/kvm` | closed — EL2 belongs to the Gunyah hypervisor |
| VM via vendor loader `/dev/qgunyah` | closed — only Xiaomi-signed VMs boot |
| unauthenticated VM via `/dev/gunyah` (DroidVM) | **works on the stock kernel** — Debian sid (Global ROM, [journal/59](docs/journal/59-global-rom-and-debian-sid.md); Arch Linux ARM before, 53–58), 4 vCPU, 1.4 GiB, network, `apt`/`pacman` ([STATE](docs/STATE.md)) |
| native Linux as primary OS | not started — kernel needs `DEVTMPFS`/`FHANDLE`/namespaces |
| chroot on the Android kernel | feasible, not set up — not an independent system |

## Docs

* [STATE](docs/STATE.md) — current setup and how to use it
* [OPTIONS](docs/OPTIONS.md) — the three routes compared, pros and cons
* [ROADMAP](docs/ROADMAP.md) — what is left
* [FINDINGS](docs/FINDINGS.md) — what the virtualization work established
* [RESTORE](docs/RESTORE.md) — rollback and stock hashes
* [index](docs/README.md) — reference and lab journal

## Layout

```
docs/      findings, options, reference, journal
src/       freestanding probes (SMCCC stubs, /dev/qgunyah drivers)
guest/     guest kernel build, rootfs image, run/ssh/config scripts
tools/     bundle tools, stock-launcher runner, qvirt service client
patch.py   boot.img stub harness (with run.sh)
evidence/  raw probe output
```

Everything flashed has a stock backup; see [RESTORE](docs/RESTORE.md) before
touching the device.

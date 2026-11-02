# houji-linux

Running Arch Linux / NixOS on the Xiaomi 14 (houji, SM8650, HyperOS 3.0.7.0),
without proot.

## Status

| route | state |
|---|---|
| `/dev/kvm` | closed — EL2 belongs to the Gunyah hypervisor |
| VM via vendor loader `/dev/qgunyah` | closed — only Xiaomi-signed VMs boot |
| unauthenticated VM via `/dev/gunyah` (crosvm) | **works on the stock kernel** — Arch Linux ARM boots to a systemd login ([journal/53](docs/journal/53-arch-linux-arm-boots.md)) |
| native Linux as primary OS | open — stock GKI 6.1 kernel needs `DEVTMPFS`/`FHANDLE`/namespaces |
| chroot on the Android kernel | works, not an independent system |

## Docs

* [OPTIONS](docs/OPTIONS.md) — the three ways forward
* [FINDINGS](docs/FINDINGS.md) — what the virtualization work established
* [RESTORE](docs/RESTORE.md) — rollback and stock hashes
* [index](docs/README.md) — state, reference, lab journal

## Layout

```
docs/      findings, options, reference, journal
src/       freestanding probes (SMCCC stubs, /dev/qgunyah drivers)
guest/     客户机 kernel build, rootfs image, run script
tools/     bundle tools, stock-launcher runner, qvirt service client
patch.py   boot.img stub harness (with run.sh)
evidence/  raw probe output
```

Everything flashed has a stock backup; see [RESTORE](docs/RESTORE.md) before
touching the device.

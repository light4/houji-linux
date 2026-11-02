# DroidVM: fork or not

## What upstream already does (Droid-VM/DroidVM `master`, 2026-11-01)

`daemon/vm/backend/CrosvmBackendInstance.java`:

* on Gunyah the default protection is `PROTECTED_WITHOUT_FIRMWARE` — the exact
  mode that boots Arch here (journal/51, 53); `--protected-vm`,
  `--protected-vm-pseudo-unprotected` are selectable;
* `--hugepages` on by default, `--prepare-lend-mthp-mode single|chunked`,
  `--swiotlb`, virtio-gpu, VNC, bridge/NAT networking, 9p shares, LXC image import;
* companion repos: `edk2-gunyah` (UEFI → distro kernels via GRUB/systemd-boot),
  `gh-hugepage-reserve` (2 MB page pool kernel module, KMIs android14-6.1…16-6.12,
  release v12), `netbox`/`pseudo-bridge-rs` (networking).

So the SM8650 adaptation we worked out by hand is already the app's default;
a fork would mostly re-implement upstream.

## Memory ceiling = fragmentation

`--mem 256` boots, `--mem 768` fails at `VM_START` (`NORESOURCE`) with or
without `--hugepages` (shmem THP `advise`). Tracing `gh_rm_call`: the lend goes
out as one `MEM_LEND` + ~30 `MEM_APPEND` requests of 8204 B (~512 SGL entries
each), i.e. >10k fragments averaging ~16 KiB. After 2 h uptime (`MemFree` 1.3 GB)
no 2 MB pages are available, so `--hugepages` changes nothing.
`gh-hugepage-reserve`'s README describes exactly this (84 % of 2 MB windows
pinned by unmovable pages after uptime) and reserves the pool at early boot.

## Decision

Don't fork now. Use upstream pieces (crosvm-dv already; next
`gh-hugepage-reserve` for bigger guests, optionally the app/`edk2-gunyah` for UEFI
+ the distro's own kernel). Fork only for a concrete gap found while using them.

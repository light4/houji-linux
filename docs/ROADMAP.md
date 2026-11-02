[English](ROADMAP.md) | [中文](ROADMAP.zh.md)

# Roadmap

The Gunyah VM route works ([STATE](STATE.md)). What is left, roughly in order of
value.

## Make it routine

* **ssh without the relay**: run the relay as a KernelSU `service.sh`, or use
  Termux `ssh` on the phone (`ssh alarm@192.168.233.128`), so nothing is lost
  after a reboot.
* **ssh keys** instead of passwords; drop the serial root autologin once keys work.
* **Bring up on start**: let the DroidVM daemon start the VM at boot (after the
  first unlock).

## Guest kernel

* **v3 drivers**: `DRM_VIRTIO_GPU`, `DRM_SIMPLEDRM`, `VIRTIO_INPUT`, `SND_VIRTIO`
  (were `=m` in defconfig and dropped by `MODULES=n`); needed for any display.
* **Arch's own kernel**: pseudo-unprotected mode no longer needs
  `DMA_RESTRICTED_POOL`, so a stock `linux-aarch64` might boot via DroidVM's UEFI
  (`edk2-gunyah`) and update with `pacman`.
* **KASLR**: find why the early relocation fails after the shim hand-off
  ([journal/58](journal/58-pseudo-unprotected-nokaslr.md)); report upstream.

## Resources

* **More vCPUs**: 4 now; SM8650 has 8 cores (1 + 5 + 2 of three classes) —
  measure before going higher, and consider `cpu_affinity`.
* **More RAM**: raise `pool_want` (2 MiB pages) together with `memory_mb`; the pool
  is memory taken from Android while held.

## Later

* NixOS image (same kernel, different rootfs).
* A desktop, once the v3 drivers exist.
* Option 1 (native Linux) only if replacing Android becomes the goal — see
  [OPTIONS](OPTIONS.md).

Obsolete plans for the vendor-loader route are kept in
[reference/vendor-loader](reference/vendor-loader.md).

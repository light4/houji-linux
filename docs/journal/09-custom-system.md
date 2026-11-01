# Booting a *custom* system in a VM: the signing gate is open

**Result first: `qtestsign` is accepted.** The SVM images are test-signed, so a
dummy certificate chain passes `gh_rm_vm_auth_image` — including for content we
modified ourselves. A custom guest is therefore an engineering problem, not a
key problem.

## Why this was expected to be impossible, and is not

The signature segment (`trustedvm.b04`, flags `0x2000000`) carries a chain whose
subjects read `Generated Xiaomi Root CA`, `Generated Xiaomi Attestation CA`,
`SecTools Test User`, `Xiaomi VMSigner`. "Generated" + "SecTools **Test** User"
is what Qualcomm SecTools emits for test signing, not fuse-anchored production
signing. The format is the documented MBN/ELF one (hash segment `0x8000007`,
signature segment), which [qtestsign] implements.

## Experiment and result

Two runs, both on a freshly booted device with the 68 MB CMA region free:

1. re-signed the **unmodified** bundle → `set_fw_name` returned `0`,
   `vcpu_count` 2, vCPUs created, `gh_vcpu_run` blocked (guest executing)
2. re-signed the bundle whose kernel had one byte changed
   (`Linux version` → `Linux Xersion`) → identical result: `set_fw_name` `0`

For comparison, serving the *same* patched byte without re-signing yields
`gh_rm_vm_auth_image: VM_AUTH_IMAGE failed with err: -22`
([08](08-arch-feasibility.md)). So auth is real, and it accepts our signature.

## How to produce a loadable bundle

```bash
# 1. rebuild the combined image from header + segments at their phdr offsets
# 2. sign it (v6 header, multi-image type)
qtestsign.py -v 6 -o signed.bin multi-image combined.bin
# 3. split back into .mdt + .b00 .b01 … following the new phdrs:
#    .mdt  = bytes[0 : end of the last non-PT_LOAD segment]   (header + hash + sig)
#    .b00  = bytes[0 : 0x d4]                                 (ELF header)
#    .bNN  = payload of the Nth PT_LOAD segment
```

Deliver it through the **direct** firmware path, not the ueventd fallback:

```bash
adb shell su -c 'mount -o rw,remount /vendor/firmware_mnt'
# copy the bundle into /vendor/firmware_mnt/image/  (label/context already
# firmware_file, and the kernel loads it directly)
```

The fallback path (`/product/vm-system/...` via bind mount) works too but
`ueventd` dies with `sendfile failed … Out of memory` on the 10 MB kernel
segment, and the bind mount has to be made in init's mount namespace and
relabelled `vendor_vm_qti_system_file`. The vfat partition is simpler and
survives reboots until you remove the files.

## What is still missing (engineering, in order)

1. **Our own cmdline.** Sign an initramfs of ours and drop dm-verity
   (`root=/dev/vda rw`, or an initramfs that mounts by label). This also
   removes the signed-root-hash problem for the rootfs entirely, so any ext4
   image works.
2. **Host-side `virtio-blk` backend.** Nothing backs the disk today; `vmmgr`
   does loop devices and `qcom,svm_neuron_block` grants, `gh_probe` does not.
   Needs `GH_GET_SHARED_MEMORY_SIZE` (0xB261), `GH_IOEVENTFD` (0xB262),
   `GH_IRQFD` (0xB263), `GH_WAIT_FOR_EVENT` (0xB264) plus the virtio
   feature/queue plumbing of `gh_virtio_backend_ioctl`.
3. **A console.** The vendor kernel has 0 refs to `virtio_console` and
   `virtio_gpu`; with our own kernel we enable `virtio_console` (and
   `virtio_mmio`) and get a shell.
4. **Watchdog keep-alive** for vmid 45 (`gh_virt_wdt`) so the host stops
   resetting a few seconds in.
5. **The guest itself**: arm64 kernel + initramfs + ext4 rootfs, bundled,
   signed, tested.

Steps 1–2 are the next sprint; 5 is only meaningful once a console exists.

[qtestsign]: https://github.com/msm8916-mainline/qtestsign

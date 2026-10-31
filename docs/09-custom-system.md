# Booting a *custom* system in a VM: what is actually required

Scope: no proot, no userspace sandbox — a real guest with our own kernel and
rootfs, started through Gunyah. This is the remaining big goal, and it reduces
to one gate plus one backlog.

## The gate is image authentication, and it may be weaker than it looks

`gh_rm_vm_auth_image` → TZ `mssecapp` refuses any bundle whose hash table no
longer matches ([08](08-arch-feasibility.md)). To go further we must produce a
bundle TZ accepts. Two things suggest this is not hopeless:

1. The signature segment (`trustedvm.b04`, segment flags `0x2000000`) carries a
   certificate chain whose subjects are literally
   `Generated Xiaomi Root CA`, `Generated Xiaomi Attestation CA`,
   `SecTools Test User`, `Xiaomi VMSigner`. The word **Generated** plus
   "SecTools **Test** User" is what Qualcomm's SecTools emits for *test*
   signing, not for fuse-anchored production signing.
2. The image format is the documented MBN/ELF one: an ELF with a hash segment
   (`0x8000007` flags, one SHA-256 per segment) and a signature segment. That
   format has an open implementation — [qtestsign] packs exactly this layout
   with a dummy chain and stub signature, and it is what the MSM8916 mainline
   community uses on devices that do not enforce fuses.

So the first experiment is cheap and precise: **re-sign the *unmodified*
`trustedvm` bundle with qtestsign and see whether TZ still accepts it.**

* accepted → the signature path is test-key friendly and we can sign our own
  kernel/initramfs next
* rejected → auth is fuse-anchored; the game becomes obtaining a Xiaomi/
  Qualcomm key, and everything below still has to exist for the day we do

## What a custom guest needs, independent of signing

1. **Our own cmdline.** Signing our own initramfs lets us drop dm-verity
   entirely (`root=/dev/vda rw` or a small initramfs that mounts by label), so
   the rootfs no longer needs a signed root hash.
2. **A host-side `virtio-blk` backend.** Currently nothing backs the disk:
   `vmmgr` sets up loop devices and `qcom,svm_neuron_block` grants, and our
   `gh_probe` does none of it. Needs `GH_GET_SHARED_MEMORY_SIZE` (0xB261),
   `GH_IOEVENTFD` (0xB262), `GH_IRQFD` (0xB263), `GH_WAIT_FOR_EVENT` (0xB264)
   and the virtio feature/queue plumbing from `gh_virtio_backend_ioctl`.
3. **A console.** The vendor kernel has **no** `virtio_console` and no
   `virtio_gpu`; once we can sign, our own kernel just enables them, otherwise
   the guest is a black box.
4. **Watchdog keep-alive**, so a booting guest does not reset the host after a
   few seconds (`gh_virt_wdt`, vmid 45).
5. **A guest image**: arm64 kernel + minimal initramfs + rootfs (ext4),
   assembled as an MBN bundle (`<name>.mdt` + `.b00…`), placed where `ueventd`
   resolves it (the bind-mount + `vendor_vm_qti_system_file` relabel trick from
   08 keeps `/product` untouched).

## Ordered plan

| step | goal | cost |
|---|---|---|
| 1 | qtestsign the unmodified bundle, run `gh_probe`, read `VM_AUTH_IMAGE` | minutes |
| 2 | virtio-blk + console backend in `gh_probe` (works with the vendor guest too) | days–weeks |
| 3 | if 1 succeeded: sign a trivial custom bundle (own cmdline, shell initramfs) and watch the console | days |
| 4 | real rootfs, display path, watchdog handling | project |

Steps 1 and 2 are independent and can be attacked in either order; 3 is
worthless if 1 fails.

## Fallbacks if test keys are rejected

* hunt a leaked/derived Xiaomi `VMSigner` key (nothing public found so far);
* find a firmware revision whose `gh-secure-vm-loader` node has auth disabled —
  but the auth mode comes from the hypervisor-side DT in XBL, not from
  anything writable;
* get the vendor to expose a non-secure VM slot.

[qtestsign]: https://github.com/msm8916-mainline/qtestsign

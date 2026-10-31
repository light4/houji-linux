# Could Arch Linux run in a Gunyah VM here? Mechanism yes, signatures no

Short answer: the VM machinery works (see [07](07-gunyah-direct.md)), but the
guest image is **authenticated** — kernel, initramfs and rootfs all hop through
a signature chain we cannot reproduce. Booting Arch needs Xiaomi/Qualcomm
signing keys.

## What the shipped guest actually is

`trustedvm` firmware (`/product/vm-system/trustedvm/boot/`) unpacks to:

| segment | content |
|---|---|
| `b01` | Linux `6.1.68-android14-11` arm64 Image (has `virtio_blk`, `ext4`) |
| `b03` | cpio initramfs containing only `verity.cmdline` |
| `b02`, `b04` | TUI payload / cert blob |

`verity.cmdline`:

```
root=/dev/dm-0 ro init=/sbin/init dm-mod.create="verity,,,ro,0 270336 verity 1
/dev/vda /dev/vda 4096 4096 33792 33793 sha256 <root hash> 10
use_fec_from_device /dev/vda ... root_hash_sig_key_value=<Xiaomi VMSigner cert>
```

So the root filesystem is a **dm-verity image on the virtio disk with a signed
root hash**, and the disk path the vendor config expects
(`/data/misc/vm-system/system.img`) is empty on this build.

## The trust chain

1. `trustedvm.mdt` carries a hash table over the segments plus a signature.
2. `gh_secure_vm_loader` loads the segments, then asks the resource manager:
   `gh_rm_vm_auth_image` → TZ (`mssecapp`) verifies.
3. The rootfs is protected separately by dm-verity with a Xenix-VMSigner-signed
   root hash embedded in the authenticated initramfs.

There is no auth-type property on the hypervisor side to switch this off
(the `qcom,gh-secure-vm-loader` node only knows `firmware-name`, `vmid`,
`label`, `pas-id`, `keep-running`, `no-shutdown`).

## Experiment: one byte is enough to fail

Flipped `Linux version` → `Linux Xersion` inside `trustedvm.b01`, served the
modified bundle over `/product/vm-system/trustedvm/boot/` and started the VM.

Two operational details are worth keeping:

* the bind mount must land in **init's** mount namespace, otherwise `ueventd`
  cannot see it — `nsenter -t 1 -m -- mount --bind …`
* the files must be relabeled `u:object_r:vendor_vm_qti_system_file:s0`,
  otherwise `ueventd` gets `EACCES`

Result:

```
gh_rm_vm_auth_image: VM_AUTH_IMAGE failed with err: -22
gunyah_loader: VM_AUTH_IMAGE failed for VM:45 -22
```

The loader had already mapped the whole image into the 68 MB CMA region and
only refused at auth — so the mechanism is fine, the bytes are simply not
signed by anyone the device trusts.

## Verdict

| goal | feasible today? |
|---|---|
| run the shipped TUI VM | **yes** (07) |
| swap in an Arch rootfs, keep vendor kernel | no — dm-verity root hash is signed, and its signature+cert live inside the authenticated initramfs |
| Arch kernel + own initramfs + own rootfs | no — requires re-signing the MBN bundle with a trusted key |
| Arch userland without any VM | **yes**, see below |

So "Arch in a VM" is a *signing* problem now, not a hypervisor problem. The
ways forward, cheapest first:

1. **Don't use a VM.** `proot`-based Linux userlands already work on this SoC:
   [DroidDeck](https://github.com/Droid-Deck/DroidDeck) ships exactly that
   (Steam Big Picture, Proton via FEX, no root, Adreno 730+). No `/dev/kvm`, no
   Gunyah, but it boots an Arch-ish userland today.
2. **Get keys.** Qualcomm MBN signing (SEC tools) with a Xiaomi-accepted
   identity; then the whole 07 path becomes useful for arbitrary payloads.
3. **Find a leaky loader.** Another ROM/SoC variant that ships
   `gh-secure-vm-loader` with auth disabled, or a hypervisor-side config change
   (XBL) that adds a non-secure VM slot — both are outside our write access.

## Notes for whoever retries

* every failure path leaks the 68 MB CMA allocation → **one attempt per boot**
* `set_fw_name` is the step that loads the image, so it is also the step where
  auth happens; do not expect `vcpu_run` to report signature problems

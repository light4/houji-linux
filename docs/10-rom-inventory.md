# What the stock ROM actually ships around virtualization

Checked a local dumpset (`houji_images_OS3.0.7.0.WNCCNXM_16.0`, same build as the
device). Short version: the VM *firmware* is shipped twice, the VM *rootfs* is
shipped nowhere.

## Relevant files

| file | what it is |
|---|---|
| `images/vm-bootsys.img` | Android sparse → 20 MB ext4; contains `/trustedvm/boot/trustedvm.{mdt,b00…b04}` (the vendor-signed bundle, byte-for-byte the one in `/product/vm-system/trustedvm/boot/`) plus `/trustedvm/sign_image_log.log` |
| `images/vm-persist.img` | sparse → 35 MB ext4; contains `/trustedvm/persist.img` (29 MB), matching what is mounted at `/mnt/product/vm-persist` |
| `images/hypvm.mbn`, `images/hypvmperformance.mbn` | hypervisor images; `flash_all.sh` flashes **hypvmperformance.mbn** to `hyp_ab` |

`sign_image_log.log`:

```
[07:21:04] - You need sync vendor/qcom/non-hlos/common
[07:21:04] - Need use python3.6 or above version to exe sign_imagev2
[07:21:04] - Now Your Platform is:linux
```

So the vendor signs bundles with Qualcomm's `sign_imagev2` (SecTools). That is
the production tool, while our re-signing uses `qtestsign` — which TZ accepted
([09](09-custom-system.md)), so the test-signing path of this platform is
permissive.

## Partitions on the device

```
vm-bootsys_a  vm-bootsys_b   (20 MB each, A/B)
vm-persist    (mounted rw at /mnt/product/vm-persist)
hyp_a         hyp_b
```

`vm-bootsys_a` is **byte-identical** to the ROM's unsparsed image
(`sha256 040b9d5f…`), i.e. the partition really does carry the bundle. Neither
`vmmgr` nor `qvirtservice` references the name `vm-bootsys` and nothing mounts
it, so it looks like an A/B carrier for the images rather than a runtime source
— the runtime source `ueventd` actually resolves is `/product/vm-system/…`.

## What is missing

* the guest rootfs: the authenticated cmdline wants a 270336-sector (≈138 MB)
  dm-verity image on `/dev/vda`, and no such file exists in
  `images/`, in `/product`, or in `/data/misc/vm-system/` (empty)
* `oemvm` firmware of any kind, which is consistent with `set_fw_name("oemvm")`
  returning `EINVAL`

So no shipped VM can boot to a userland here; supplying the disk is on us
either way, which is why [09](09-custom-system.md) treats our own initramfs +
rootfs as the target rather than reviving the vendor TUI VM.

## Practical value of the dump

1. a pristine vendor-signed bundle for regression-testing the signing pipeline
   (byte-compare our split output against `trustedvm.*`)
2. a real `persist.img` to exercise a future `virtio-blk` backend with
   something other than an empty file
3. the hyp images, for comparing against what is flashed if hypervisor version
   ever becomes relevant

## How to inspect

```bash
# unsparse (blk_sz = 4096, chunk types CAC1..CAC4) then
debugfs -R 'ls -l /trustedvm/boot' vm-bootsys.raw
debugfs -R 'cat /trustedvm/sign_image_log.log' vm-bootsys.raw
```

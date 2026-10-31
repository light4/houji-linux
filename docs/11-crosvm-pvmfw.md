# crosvm drives this device's Gunyah — the missing piece is pVM firmware

Prior art exists: [gunyah-on-sd-guide] (a Lenovo 8-elite device) runs Debian in a
Gunyah pVM with **upstream crosvm**, an AOSP-test-key-signed kernel and Google's
ferrochrome rootfs. This device follows the same shape, and got surprisingly
close.

## Two device-ABI facts that cost time

* The ioctl type for the *management* node here is **`0x47xx`**, not `0xB2xx`:
  `/dev/gunyah` implements `0x4700` (create VM → returns an fd) and friends.
  The `0xB2` family that our own probe tool used lives on `/dev/qgunyah` and is
  the *vendor/gunyah_loader* interface (create VM, fw name, vcpu, run).
  crosvm talks `0x47xx`, i.e. the mainline-style ABI, so `/dev/gunyah` is the
  correct node — do **not** remap it to `/dev/qgunyah` (that was my first
  mistake and it produced bare `EINVAL`).
* `/dev/gunyah` needs its SELinux label if you recreate the node:
  `chcon u:object_r:vendor_gunyah_dev:s0`.

## Where it gets to

With the stock `crosvm-a16` (and the device's own APEX crosvm behaves the same)
and the guide's artifacts (`kernel` re-signed with `avbtool`+AOSP test key,
`disk1.img`, `vm_dt_overlay.dtbo`):

```
creating hypervisor: Gunyah { device: Some("/dev/gunyah") }
Trying to attach block device: disk1.img        <- disks work
failed to initialize virtual machine: EPERM
dmesg: gh_vm_mgr: Failed to configure pVM firmware
```

`--protected-vm-without-firmware` (the 8-elite shortcut) fails with `-ENODEV`
(`Failed to start VM: -19`) — this platform needs the firmware path.

`--protected-vm-with-firmware <img>` is the right flag, and it works up to a
size limit:

```
custom pVM firmware could not be loaded: Image size too large: 12177408
```

The firmware slot is `pvmfw_region@0x824a0000`, **1 MiB**. So a real pvmfw fits;
a 12 MB microdroid kernel does not.

## What pvmfw is, and why it is missing here

`pvmfw` is the tiny first-stage in a protected VM: it verifies the payload
against keys carried in an ABL-provided config, measures the boot into a DICE
chain and only then starts the kernel. On Pixel and several Qualcomm devices it
is a physical partition. On this device the reserved region exists but **no
pvmfw image ships in the ROM** (no `pvmfw*` in `images/`, none on the
filesystem), which is consistent with `ro.boot.hypervisor.protected_vm.supported
= false` and with AVF never being able to start a pVM.

## AOSP's own knobs

The pvmfw README documents the whole prototyping flow, two pieces of which are
worth having here verbatim:

```shell
# build a usable image: pvmfw binary + a DICE chain appended by pvmfw-tool
m pvmfw-tool pvmfw_bin
pvmfw-tool custom_pvmfw ${PVMFW_BIN} ${DICE}     # DICE = tests/pvmfw/assets/dice.dat
adb push custom_pvmfw /data/local/tmp/pvmfw
adb root && adb shell setprop hypervisor.pvmfw.path /data/local/tmp/pvmfw
adb shell /apex/com.android.virt/bin/vm run-microdroid --protected
```

and a debug escape hatch: `setprop hypervisor.pvmfw.path none` runs a pVM
*without* pvmfw.

Both are consumed by `virtualizationservice`, not by crosvm, and on this build
`vm run-microdroid --protected` dies before reaching the hypervisor:

```
panicked: Device doesn't support protected or non-protected VMs
```

— that is the `ro.boot.hypervisor.*` userspace gate again
([06](06-avf-is-kvm-only.md)), so driving crosvm directly (as above) is the only
usable route, and it needs the image passed explicitly with
`--protected-vm-with-firmware`.

## How to supply one

AOSP documents this exact "iterate without flashing" flow in
`packages/modules/Virtualization/guest/pvmfw/README.md`: build `pvmfw`, append a
DICE chain and config with `pvmfw-tool`, `adb push` it, and pass it to
`--protected-vm-with-firmware`.

Problem: `pvmfw` has no `Cargo.toml`, it only builds inside the Android build
system (soong). So the practical routes, cheapest first:

1. **Take a prebuilt that trusts AOSP test keys.** Cuttlefish images
   (`aosp_cf_arm64_phone`, ci.android.com) ship `pvmfw` built with test keys,
   plus the config/DICE assets. Extract `pvmfw.bin`, append the config the same
   way `pvmfw-tool` does, push it.
2. **Build it in an AOSP tree** (`m pvmfw-vbmeta pvmfw-tool`) and assemble the
   image with `PVMFW_BUILTIN_KEY`-style test keys.
3. **Reuse another device's pvmfw** (e.g. `wuxianlin/oneplus12_pvmfw`, same
   SoC family). It will boot, but its embedded key will refuse our AOSP-test-
   signed kernel unless it happens to trust test keys.

Once a pvmfw is in place the rest of the guide applies unchanged: sign kernel and
initrd with `avbtool` + `testkey_rsa4096.pem`, pass `--rwdisk disk1.img` (the
GPT/magic trick pvmfw wants) plus a rootfs disk, and the guest boots with
`root=/dev/vdb`.

[gunyah-on-sd-guide]: https://github.com/polygraphene/gunyah-on-sd-guide

## Where this stopped

Obtaining the artifact. `ci.android.com`'s status endpoints and the legacy
`androidbuildinternal` API are both 404/rate-limited from a plain client, so the
two realistic options are (a) an authenticated client / browser download of a
Cuttlefish build, or (b) an AOSP checkout and `m pvmfw-vbmeta pvmfw-tool`.
Everything device-side is already proven up to this point.

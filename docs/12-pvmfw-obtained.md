# pvmfw obtained — and what the device does with it

## How to get a pvmfw without downloading a 4 GB factory image

Google's factory-image list is client-rendered, but the backing JSON is open:

```
https://content-flashstation-pa.googleapis.com/v1/builds?key=<39-char key from flash.android.com>&product=<codename>
```

`key` is embedded in the `data-client-config` attribute of `https://flash.android.com/`.
Each entry carries `factoryImageDownloadUrl`. `dl.google.com` honours `Range`, and
the factory zip nests a second zip (`image-<device>-<build>.zip`, stored
uncompressed), so the whole thing is two central-directory reads plus one range
read of the single entry `pvmfw.img` — ~1 MiB instead of 4 GB.

The result is an **exactly 1 MiB Android boot image** (header + payload), i.e.
the pvmfw partition content as shipped on a Pixel
(`artifacts/pvmfw-pixel-husky-cp3a.261005.005.img`, from
`husky-cp3a.261005.005-factory-4b94ad0e.zip`).

## Why a Pixel pvmfw is acceptable

`guest/pvmfw/src/main.rs`:

```rust
// Policy/Hidden ABI: If the pvmfw loader (typically ABL) didn't pass a DICE handover
// ... skip DICE, AVB, and RBP.
let (verified_boot_data, deggable, guest_page_size) = if current_dice_handover.is_none() {
    warn!("Verified boot is disabled!");
```

This device has no ABL pvmfw step, so nothing appends a DICE/config block and
**verification is skipped** — the Google signing key never matters. Our kernel
does not even need its AVB footer (though it has one).

## What happens

| attempt | result |
|---|---|
| `--protected-vm` (no firmware) | `EPERM` at init, dmesg `gh_vm_mgr: Failed to configure pVM firmware` |
| `--protected-vm-with-firmware <12 MB kernel>` | `custom pVM firmware could not be loaded: Image size too large: 12177408` |
| `--protected-vm-with-firmware pvmfw.img` (1 MiB) | **firmware accepted**, then `failed to initialize virtual machine: ENODEV` |
| same with the 4096-byte boot header stripped | same `ENODEV` |
| plain (unprotected) and `--unprotected-vm-with-firmware` | same `ENODEV` |
| all of the above after a fresh boot | same `ENODEV` |

dmesg is terse: `misc gunyah: Failed to start VM: -19`. The device-side pieces
are all in place (hypervisor found, block device attached, firmware size
accepted), so the failure is inside the vendor `gh_vm_mgr` start path, not in
crosvm.

## Hypotheses to try next

1. **A Qualcomm-built pvmfw.** Pixel's pvmfw is plain AOSP; the vendor driver may
   expect a Qualcomm-specific payload/layout. OnePlus 13 (`SM8750`) ships a
   `pvmfw` partition — extract it from that firmware and try the same flow.
2. **Config/DICE presence.** Append the AOSP test DICE chain the way
   `pvmfw-tool custom_pvmfw` does, so `current_dice_handover` is `Some` and
   pvmfw takes the verified path. Needs `pvmfw-tool` (AOSP build).
3. **Property path.** `setprop hypervisor.pvmfw.path <file>` + `vm run-microdroid
   --protected`; blocked here earlier only by the `ro.boot.hypervisor.*` gate, so
   it needs `resetprop`/`ksud` to fake those boot props first.
4. **Ask the guide's Discord** whether anyone ran a pVM on an `SM8650` ROM at
   all; the whole path may be untested outside 8-elite.

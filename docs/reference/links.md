# External references

Third-party work this project relied on its its findings. Kept here so nothing
depends on memory or a chat log. All links are read-only references unless stated
otherwise.

## Gunyah / VM tooling

* [Droid-VM/DroidVM](https://github.com/Droid-VM/DroidVM) — GPL-3.0 crosvm/QEMU VM
  manager for Android; lists SM8650 as supported; source of the finding that the
  SM8650 start failure is a kernel bug.
* [DroidVM issue #4](https://github.com/Droid-VM/DroidVM/issues/4) — Redmi K70 Pro
  (SM8650, same kernel build), same `os error 19`/`12`; maintainer reply names the
  Gunyah kernel bug and links the patches below.
* [DroidVM issue #4 comment with the houji kernel fork](https://github.com/Droid-VM/DroidVM/issues/4#issuecomment-5034887143)
* [DroidVM-Prebuilts](https://github.com/Droid-VM/DroidVM-Prebuilts) — their prebuilt
  `crosvm` (patched: `--prepare-lend-mthp-mode`, `--protected-vm-pseudo-unprotected`,
  `--extended-status`); tried here, does not fix `NOMEM` on an unpatched kernel.
* [DroidVM wiki](https://github.com/Droid-VM/DroidVM/wiki)

## The SM8650 Gunyah VMID bug and its fixes

* [android_kernel_samsung_sm8650_gunyah @3d87670](https://github.com/Andy312432/android_kernel_samsung_sm8650_gunyah/commit/3d8767092fa186b77dfcbc5fd14b53ae17b04bb2)
  — original fix by the DroidVM maintainer (Samsung SM8650 tree).
* [android_kernel_samsung_sm8650_gunyah @b2ffff2](https://github.com/Andy312432/android_kernel_samsung_sm8650_gunyah/commit/b2ffff284b1f8b6cd79c280fb746b27fb6f0cbae)
  — follow-up commit.
* [Yizhou147/Xiaomi14-8Gen3-Gunyah-Fix](https://github.com/Yizhou147/Xiaomi14-8Gen3-Gunyah-Fix)
  — **houji-specific** fork: ABK/GKI build pipeline, GPL, GitHub Actions.
  * [patch](https://github.com/Yizhou147/Xiaomi14-8Gen3-Gunyah-Fix/blob/master/patches/0001-gunyah-fix-scm-vmid-gki-compat.patch)
    — GKI-compatible version (uses `arm_smccc_1_1_invoke`), 38 lines.
  * [releases](https://github.com/Yizhou147/Xiaomi14-8Gen3-Gunyah-Fix/releases) —
    AnyKernel3 zip and boot images, `v6.1.138-GunyahFix-*`.
  * [kvcalloc kprobe module](https://github.com/Yizhou147/Xiaomi14-8Gen3-Gunyah-Fix/blob/master/modules/gunyah_kvcalloc_fix.zip)
    — lifts the ~2 GB per-VM cap.
  * field notes: disable mTHP pre-lend on SM8650 (2 MB collapse panics the hyp);
    keep `init_boot` free of Magisk patches.

## Kernel / distribution work (option 1)

* [Droidian porting guide](https://github.com/droidian/porting-guide) —
  `kernel-compilation.md`, boot image header v4 for GKI devices.
* [droidian-devices/linux-android-common-6.1-android14](https://github.com/droidian-devices/linux-android-common-6.1-android14)
  — packaging for ACK 6.1.174 `gki_defconfig` plus fragments.
* [droidian-devices/common_fragments](https://github.com/droidian-devices/common_fragments)
  — `droidian.config`, `halium.config`, `container.config` (enables `DEVTMPFS`,
  `PID_NS`, `USER_NS`, `SYSVIPC`, `VT`, …).
* [Halium](https://halium.org), [libhybris](https://github.com/libhybris/libhybris)
  — the Android-HAL compatibility stack behind Droidian.
* [qtestsign](https://github.com/msm8916-mainline/qtestsign) — MBN signing; the
  produced signatures are **rejected** by this device's PIL check.

## Prior art on the EL2 question

* [slbounce](https://github.com/TravMurav/slbounce) — Secure Launch handover; not
  implemented in this firmware.

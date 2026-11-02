# `NOMEM` is a known SM8650 kernel bug with a fix

Found by reading [Droid-VM/DroidVM] (a GPL crosvm/QEMU VM manager that lists
SM8650 as supported) and its issue tracker. Issue #4 (Redmi K70 Pro, same SOC,
same kernel `6.1.118-…ab13624819`) is the same failure we hit; the maintainer's
answer:

> 8 Gen 3 cannot start VMs due to Gunyah memory allocation in the kernel
> (8 Elite and later rewrote that code); a small chance it works. Patch:
> `android_kernel_samsung_sm8650_gunyah@3d87670`, `@b2ffff2`.

and a houji-specific fork: [Yizhou147/Xiaomi14-8Gen3-Gunyah-Fix].

## What actually goes wrong

On SM8650 the Resource Manager runs in its own VM with a VMID **above**
`QCOM_SCM_MAX_MANAGED_VMID` (0x3F). `drivers/virt/gunyah/gunyah_qcom.c`
hardcodes the source VMID:

```c
src = BIT_ULL(QCOM_SCM_VMID_HLOS);
```

so the SCM `assign_mem` for every lend is made on behalf of the wrong VMID and
is rejected; the RM reports `NOMEM`. The fix is 38 lines: resolve the real self
VMID with `gh_rm_get_vmid()` and map managed VMIDs, i.e.

```c
ret = gh_rm_get_vmid(rm, &self_vmid);
...
src = BIT_ULL(qcom_scm_map_vmid(self_vmid));
```

That explains everything we measured and could not move: it is not the guest RAM
size, the IPA base, the vCPU count, the device set, the VM name or contention —
the *lend* itself is refused, so no amount of config helps. (8 Elite/SM8750
rewrote this code and is unaffected.)

## Practical consequences

* Option 3 ([OPTIONS](../OPTIONS.md)) is **not** a firmware dead end: it needs a
  patched kernel, which an unlocked bootloader can take.
* The same repo ships ready artefacts (AnyKernel3 zip / boot images, releases
  `v6.1.138-GunyahFix-*`, GPL, built by GitHub Actions from `gki_defconfig` +
  their patch) and a `kvcalloc` kprobe module to lift a ~2 GB per-VM cap.
* Their field notes for this SOC: **disable DroidVM's mTHP pre-lend** (2 MB
  collapse breaks this firmware's hyp and reboots the phone), and keep
  `init_boot` free of Magisk patches (our `init_boot` is KernelSU-patched, which
  is fine).
* Their kernel is `6.1.138` while our stock is `6.1.118`; same GKI
  branch/Android 14, stock vendor modules expected to load, but that has to be
  verified rather than assumed.

## Boot chain caveat for our own work

Our `boot_a` has a byte-exact stock backup, so a custom kernel is reversible. Two
things to keep in mind: flashing a *kernel with KernelSU* into `boot_a` replaces
our current KernelSU install (matching Manager APK required), and the patched
kernel is also exactly what option 1 needs for a native Arch/NixOS.

[Droid-VM/DroidVM]: https://github.com/Droid-VM/DroidVM
[Yizhou147/Xiaomi14-8Gen3-Gunyah-Fix]: https://github.com/Yizhou147/Xiaomi14-8Gen3-Gunyah-Fix

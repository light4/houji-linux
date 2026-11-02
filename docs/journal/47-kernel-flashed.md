# Flashed the Gunyah-fix kernel — `NOMEM` unchanged so far

Executed the plan from [journal/46](46-nomem-is-a-known-kernel-bug.md) far enough
to learn something: the published kernel alone is not enough.

## What was done

* backed up `boot_a` (stock, sha256 `c88537b8…`, also matches the fastboot ROM) to
  `~/Downloads/houji_backup/boot_a_pre_kpatch_20261102.img`;
* rebuilt a boot image from the stock one with the release kernel
  (`Yizhou147/Xiaomi14-8Gen3-Gunyah-Fix`, tag `v6.1.138-GunyahFix-17`) via
  `magiskboot`, flashed it, device boots, `uname -r` = `6.1.138-android14-11-…`;
* the kernel carries KernelSU, but the init_boot's KernelSU-LKM is built for
  6.1.118, so root was gone → **Magisk 31.1** was installed instead (stock
  `init_boot.img` from the ROM, patched with Magisk, flashed to `init_boot`);
* the project's `gunyah_kvcalloc_fix` module is installed and loads
  (`gh_kvcalloc: installed: gh_vm_mem_alloc@… gh_vm_mem_reclaim@…`), which is the
  module for the ~2 GB cap.

New `boot_a` sha256 `ee72b00c…`, `init_boot` is Magisk-patched; both stock images
are backable (see [RESTORE](../RESTORE.md)).

## What did not change

`crosvm run --hypervisor gunyah --mem 512 --cpus 2` still fails:

```
RM rejected message 56000004. Error: 2          # VM_START → NOMEM
```

Byte-identical to before the kernel change, even with the kvcalloc module loaded
and with no SELinux interference.

## Why — the fix is in a module that was not replaced

The published patch edits `drivers/virt/gunyah/gunyah_qcom.c`
(`qcom_scm_gh_rm_pre_mem_share`: `src = BIT_ULL(QCOM_SCM_VMID_HLOS)` →
`gh_rm_get_vmid()` + managed-VMID mapping). Their workflow deliberately keeps
`GUNYAH_QCOM_PLATFORM` as a **module** (`=m`) to avoid early-boot panics, so the
fix only lands if `gunyah_qcom.ko` is rebuilt and installed — and neither the
boot image nor the AnyKernel3 zip ships modules (`do.modules=0`, no `modules/`
dir; only the kvcalloc helper is separate).

Verified on device: `qcom_scm_map_vmid` is absent from `/proc/kallsyms`, and
`gunyah_qcom` is loaded as a module. So the running pair is a new kernel with the
**old** `gunyah_qcom` — the patch has no effect yet.

## Next

1. build `gunyah_qcom.ko` from `android14-6.1` with their patch and this kernel's
   config (Android clang 17, `gki_defconfig` + their `config/`);
2. install it the Magisk way (module that overlays
   `/vendor/lib/modules/gunyah_qcom.ko`) and verify the SCM assign stops failing;
3. re-test the VM.

Alternative, if local cross-building proves painful: fork their repo and run the
`kernel-custom` workflow with module packaging enabled.

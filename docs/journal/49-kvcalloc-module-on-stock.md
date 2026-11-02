# The repo's `modules/` is the kvcalloc fix, not the VMID fix

`skczhans/copy-Xiaomi14-8Gen3-Gunyah-Fix` `master` has `modules/`:
`gunyah_kvcalloc_mod.ko` (329 KB) and `gunyah_kvcalloc_fix.zip` (KernelSU/Magisk
wrapper, `post-fs-data.sh` → `insmod`).

- `vermagic=6.1.118-g129ebe2013a8 … modversions` — built for the **stock** 6.1.118
  KMI; loads on the stock kernel (`gh_kvcalloc: installed: gh_vm_mem_alloc@… use kvcalloc/kvfree`).
- What it does (symbols): kprobe on `gh_vm_mem_alloc` / `gh_vm_mem_reclaim`,
  reimplemented with `kvcalloc`/`kvfree`; resolves `gh_rm_get_vmid`,
  `gh_rm_mem_reclaim`, `account_locked_vm` via `kallsyms_lookup_name` obtained
  through a kprobe. It fixes host-side `kcalloc` OOM for big guests.
- It does **not** touch `gunyah_qcom` / `qcom_scm_assign_mem`.

Result on stock kernel + KernelSU 3.3.1 + this module, `crosvm run --hypervisor
gunyah --mem 512 --cpus 2` (both `crosvm-a16` and `crosvm-dv`):

```
RM rejected message 56000004. Error: 2     # VM_START → NOMEM
misc gunyah: Failed to start VM: -19
```

Unchanged. Useful takeaway: the same kprobe+`kallsyms_lookup_name` trick works on
this stock kernel, so a patched `gunyah_qcom` replacement can resolve the
unexported `gh_rm_get_vmid` the same way, built against 6.1.118.

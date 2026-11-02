# The SCM-VMID patch is a no-op on this firmware; only `VM_START` fails

Stock kernel 6.1.118 + KernelSU 3.3.1 + `gunyah_kvcalloc_mod`, tracefs kprobes
on the GKI Gunyah path during `crosvm run --hypervisor gunyah --mem 512 --cpus 2`:

```
gh_rm_alloc_vmid                 → 128 (0x80, > QCOM_SCM_MAX_MANAGED_VMID)
qcom_scm_assign_mem  ×4990       src=BIT(3)  dst={0x3A, 0x3}  → all ret=0
qcom_scm_gh_rm_pre_mem_share     → 0
gh_rm_mem_share                  → 0
gh_rm_vm_configure               → 0
gh_rm_vm_init                    → 0
gh_rm_vm_start                   → -19   (RM 0x56000004, Error 2 = NOMEM)
gh_rm_get_vmid  cached_vmid      = 3     (HLOS)
```

* The vendor `gunyah_qcom.ko` **already** maps guest VMIDs > 0x3F to
  `QCOM_SCM_RM_MANAGED_VMID` (0x3A).
* The patch's only other change is `src = BIT(map(self_vmid))` instead of
  `BIT(HLOS)`; self VMID is 3 = HLOS, so it is identical.
* → Building a patched `gunyah_qcom.ko` cannot change anything. Plan dropped.
  The other referenced Samsung commit is the kvcalloc fix, already loaded.

Every host-side step succeeds; the refusal is purely the RM at `VM_START`.
Guest RAM goes out as thousands of 8 KiB `assign_mem` ranges (32 MiB → 680).
DroidVM's author calls the 8 Gen 3 problem a memory-allocation one that
"occasionally boots", which points at RM-side limits on fragmented parcels — unproven.
`THP=always` + 512 MiB hard-reset the phone (same as mTHP pre-lend, journal/47).

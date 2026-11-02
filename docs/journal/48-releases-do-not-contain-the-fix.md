# The published Gunyah-fix kernels do not contain the fix

Checked every release of
[skczhans/copy-Xiaomi14-8Gen3-Gunyah-Fix](https://github.com/skczhans/copy-Xiaomi14-8Gen3-Gunyah-Fix/releases)
(`v6.1.138-GunyahFix-10/11/19/21`, all 2026-09-20/21) and of the original
`Yizhou147` repo (`-17`). Assets are always the same five: AnyKernel3 zip, its
bundle, and `boot`/`boot-gz`/`boot-lz4` image bundles. **No modules.**

The embedded kernel config (IKCONFIG, extracted from `Image`) of both v17 and v21:

```
CONFIG_GUNYAH=y
CONFIG_GUNYAH_PLATFORM_HOOKS=y
# CONFIG_GUNYAH_QCOM_PLATFORM is not set
```

`gunyah_qcom.c` — the only file the patch touches — is built only when
`GUNYAH_QCOM_PLATFORM` is set. With it unset the patched code is compiled into
**neither** the kernel nor a module, so the releases are plain 6.1.138 GKI
kernels (+ KernelSU, SUSFS) with respect to Gunyah. The platform hooks at runtime
still come from the vendor's own `gunyah_qcom.ko` in `vendor_boot`, unpatched.
That is why `NOMEM` did not change after flashing v17 and v21.

## Side result

With the vendor `gunyah_qcom` unloaded (`rmmod`), the failure moves earlier:

```
RM rejected message 51000013. Error: 3      # memory lend → NORESOURCE
```

so the qcom hooks (SCM `assign_mem` before the lend) are mandatory, and they are
exactly where the VMID bug lives.

## What would actually apply the fix

Build `gunyah_qcom.ko` with the patch and load it in place of the vendor one.
First attempt (podman arm64 container, ACK `deprecated/android14-6.1-2025-06`,
`gki_defconfig` + `GUNYAH_QCOM_PLATFORM=m`) builds, but fails to load:
`Unknown symbol gh_rm_get_vmid` — the function exists on device but is not
exported to modules. Being reworked to resolve it at runtime; replacing the
vendor module also needs the boot-time ordering handled (it is loaded from
`vendor_boot` before userspace).

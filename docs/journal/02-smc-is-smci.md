# The vendor SMC range is SMCI to TrustZone, not Gunyah

The Radxa Dragon Q6A trick — `smc(0x2000121, 0, 0, 1)` on `ExitBootServices` to
have Gunyah step aside — does **not** work here.

## Method

`src/probe_stub.S` calls a table of FIDs at EL1, records every answer that is
not `SMCCC_NOT_SUPPORTED` into the kernel banner, then returns to the kernel.
A 64-FID bitmap scanner (`src/stub_scan2.S`) sweeps `0x86000000..0x860000FF`.

## Result

Also see [evidence](../../evidence/probe-results.txt).

* `0x02000121`, `0x82000121`, `0xC2000121`: `NOT_SUPPORTED`
* `0x8200_xxxx`, `0xC600_xxxx`, `0xC200_xxxx`: nothing
* `0x86000000..0x860000FF`: 11 functions answer, all in the first 0x1E —
  returning `0`, `1` or `0x1e`

`0x1e` behaves like a status/error code, not a handle, and none of these calls
move `CurrentEL` to EL2.

## Why SMCI and not Gunyah

`0x8600_0000` is the SMCI (secure message call) base used by Qualcomm's
`smcinvoke` kernel driver: the module `/vendor_dlkm/lib/modules/smcinvoke_dlkm.ko`
contains that constant. Under Gunyah these host SMCs are forwarded to
TrustZone (`forward SMC id {:x}, client-id {:d}` in `hyp_a`), and the hyp's
SMCCC filter only allows arch/PSCI FIDs through.

So probing `0x8600_xxxx` tells you about TZ, not about the hypervisor. The
hypervisor is reached with HVC instead — see
[03-gunyah-hvc-abi.md](03-gunyah-hvc-abi.md).

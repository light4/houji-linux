# Gunyah's host ABI is HVC `0x6000+`, and it has no EL2 handback

## How the dispatcher was found

`hyp_a`'s EL2 trap handler decodes `ESR_EL2`:

```
2408: mrs  x18, ESR_EL2
240c: lsr  w30, w18, #26          ; EC
2410: cmp  w30, #0x16             ; HVC64
2414: ubfx x18, x18, #0, #16      ; immediate
2418: sub  x18, x18, #0x6000
2420: cmp  x18, #0x69             ; bound
```

Then it saves context and jumps through a 16-byte-entry table at `0x2fc0`
(`add x9, x9, x18, lsl #4` / `br x9`). That numbering is the upstream Gunyah
hypercall ABI (`docs/api/gunyah_api.md` in `quic/gunyah-hypervisor`,
`hvc 0x6000` = `hypervisor_identify`, and so on).

## Result of `hvc #0x6000`

[evidence](../evidence/probe-results.txt):

```
x0 = 0x00008001   API version 1, bit15 set => 64-bit ABI, variant 0
x1 = 0x100107ff   feature flags: partition/CSpace, doorbell, msgq, vGIC, vPM, vCPU, memext, trace
x2 = 0
x3 = 0
```

So Gunyah answers, the ABI is real and 64-bit, and the feature set is broad for
*guest* management.

## What the ABI does not have

The API is object-oriented (partition, CSpace, address space, memory extent,
VCPU, doorbell, virtual interrupt controller…). There is **no
"relinquish EL2"/"takeover"/"deinit hypervisor"** call — Gunyah simply stays at
EL2 for the lifetime of the device.

That is the structural reason `/dev/kvm` is out of reach here: Gunyah is a
type-1 hypervisor that never gives the privilege level back, and the one
vendor-specific escape (Secure Launch) was not compiled in.

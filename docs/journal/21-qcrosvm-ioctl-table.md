# `qcrosvm`'s ioctl table (and what this driver really accepts)

`qcrosvm` (see [20](20-qcrosvm-is-the-reference.md)) builds every backend
command from a base number plus a **packing suffix**, and has **two families**:
legacy `0xB2xx` and `0xBCxx` (= base + `0x9a0`), chosen at runtime from a
variant field (`[sp+0x84] & 0xff`, values 1/2). Recovered from the call sites:

| op | legacy (`0xB2xx`) | other (`0xBCxx`) |
|---|---|---|
| create vm | `0xB201` | — |
| create vcpu | `0xB240` | — |
| get vcpu count | `0xB245` | — |
| `0xB243` (unnamed) | `0xB243` | — |
| vcpu run | `0xB280` | — |
| reserved/shared mem size | `0x8008B261` | — |
| ack reset | `0x4008B26D` | `0x4008BC0D` |
| set driver features | `0x4010B265` | `0x4010BC07` |
| set device config word | `0x4010B267` | — |
| get driver features | `0xC010B26A` | — |
| ack driver ok | `0xC004B26B` | `0xC004BC0B` |
| queue info | `0xC028B269` | `0xC028BC09` |
| ioeventfd | `0x4010B262` | `0x4010BC02` |
| irqfd | `0x4010B263` | — |
| vhost-vsock | `0xBC01`, `0xBC0C` | — |

`SET_FEATURES` carries `{label, flags, features_sel, features_word}` — the driver
echoes `features_sel`/`features_word` into dmesg, which is how the layout was
confirmed.

## Measured against this driver

| cmd | result |
|---|---|
| `0x4008B26D` ack_reset | **0** |
| `0x4008BC0D` ack_reset (BC) | `EINVAL` |
| `0xC004B26B` / `0xC004BC0B` ack driver ok | `EINVAL` — **not implemented** (`cmd … not supported` in dmesg) |
| `0xC010B26A` get driver features | **0**, both words come back `0` before the guest boots |
| `0x4010BC07` set features (BC) | `EINVAL` |
| `0x4010B265` set features | `0` only for word1 = `0x1f`; every subset returns the mask `0x1e` |

So this kernel speaks the **legacy `0xB2xx`** family only, and `ACK_DRIVER_OK`
does not exist — the guest's `DRIVER_OK` has to be picked up from
`GH_WAIT_FOR_EVENT` alone.

## Updated backend order (from `qcrosvm`)

create vm → `0xB245` vcpu count → `0x8008B261` reserved mem size →
`mmap` → per device: ack_reset, set features, queue max, config, irqfd,
ioeventfd → create vcpus → run.

Note the two steps we never did: **ask the driver for the vcpu count** and **ask
for the reserved memory size** (we took `shmsz` but ignored that it came back
`0`, then hardcoded 2 vCPUs).

# No hidden "boot context" ioctl

Probed every raw number `0xB200`..`0xB2C0` on a live VM fd (empty firmware name
set, 4 KB zeroed buffer as argument) and logged everything that did not answer
`EINVAL`/`ENOTTY`:

```
0xB243 -> 2          (get vcpu count; scalar result, buffer untouched)
```

Everything else in that range is unimplemented **in its raw form** — the backend
ops only exist in their packed forms (`0x4010B2xx` / `0xC0xxB2xx`), which is why
they look absent here. `0xB201` (create), `0xB240` (create vcpu) and `0xB280`
(run) are the only other raw ones and were excluded because they mutate or
block.

So the implemented surface is exactly:

| raw | meaning |
|---|---|
| `0xB201` | create vm |
| `0xB240` | create vcpu (`arg` = index) |
| `0xB243` | get vcpu count |
| `0xB280` | vcpu run |
| `0x4010B241` | set fw name |
| `0x8008B261` | reserved memory size |
| `0x4010B262/63` | ioeventfd / irqfd |
| `0xC010B264` | wait for event |
| `0x4010B265/67` | set features / set device config |
| `0xC010B269/6A` | queue info / driver features |
| `0x4008B26D` | ack reset |

**There is no vcpu/DT boot-context call.** The guest entry state is entirely
hyp-side, taken from the authenticated image and the bundle's own DT — userspace
cannot influence it. Combined with `23` (guest dead in <0.5 s, hard reset)
that narrows the failure to "the bundle's guest does not survive on this
hyp/kernel combination", not to our ioctl sequence.

Practical consequence: our host-side work is effectively complete; the next
meaningful moves are on the guest side (patch `trustedvm.b02` into a real DT and
re-sign, or supply our own kernel + initramfs in the bundle) rather than more
ioctl archaeology.

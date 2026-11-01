# Controlled run: proxy scheduling does not matter; the vCPU never executes

`journal/33` claimed proxy scheduling as a suspect from a single lucky run. Ran it
properly: 3 boots per variant, both with `crash-fatal` already removed, probe
sampling every 50 ms, host liveness checked at 1 s / 3 s / 6 s.

| variant | runs | host alive @1s / @3s / @6s |
|---|---|---|
| `affinity = "proxy"` kept | 3 | yes / yes / yes |
| `affinity` removed | 3 | yes / yes / yes |

So with `crash-fatal` gone the host survives in both cases and **proxy
scheduling makes no difference to the guest's fate**.

## The guest never executes — in any configuration

The probe's own sample loop stops after ~4–5 ticks (≈0.25 s) *also with the
one-instruction `b .` stub as guest*, i.e. exactly like with the vendor kernel.
A guest that executes a single branch would keep the vCPU busy; nothing changes.
Combined with `journal/31` (no events, no fault on an unbacked read) the
conclusion is that **the vCPU never starts executing**, about 0.2 s after
`VCPU_RUN` in every variant we can construct.

## Measurement caveat

The probe process itself dies at about the same moment (the output file stops
mid-write and eventfd reads never resume), so "host alive at X s" and "sample
count" measure different things: the host survives thanks to `crash-fatal` being
removed, the *VM* dies at ~0.2 s regardless. Earlier journal entries that treated
a long host survival as a live guest were reading it wrong; this entry is the
corrected interpretation.

## Where to look next

The vCPU entry state is set hyp-side, and userspace has no way to inspect it
(no register ioctl, `journal/32`). The most useful next artifact would be a
**working VM's** start sequence to diff against — `cpusys_vm` starts at boot with
the same driver, so its `gh_vcpu_ioctl`/`GH_CREATE_VM` arguments and its VM-node
DT properties are the remaining reference.

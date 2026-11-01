# `VCPU_RUN` blocks — the vCPU is entered

Ran a probe that arms the backends and then calls `GH_VCPU_RUN` on vCPU 0
**without forking**, with `timeout 3`:

```
RC=124          # timeout killed it → the ioctl never returned
```

and the internal log ends at `run0 `, i.e. inside the call. So `VCPU_RUN` does
not fail, does not return `EINVAL`, and the vCPU sits in the hypervisor. Combined
with the `started running` print site (`journal/32`) the entry is definitely
taken; what never happens is any *progress*.

Also worth recording: the VM still dies (host resets) a few seconds into that
blocked call, so the teardown does not depend on our process doing anything.

## Harness gotcha that caused wrong numbers

Every probe writes its log itself to `/data/local/tmp/o.txt` (open + `fsync`), so
a shell redirect (`tool > other.txt`) only captures stdout, which is mostly
empty. Two journal entries measured "sample counts" on redirect files that the
tool never wrote to and reported ~0 rows; the real files had the samples. Always
read `o.txt`.

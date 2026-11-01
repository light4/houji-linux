# Proxy scheduling: the one difference to the only VM that works

`VM:%d started running` is printed from `gh_vcpu_ioctl` (`GH_VCPU_RUN` branch,
`.text+0x14e8`) directly after the run hypercall, so the call does reach the
hyp. The vCPU fd range `0xB240`..`0xB2C0` has no other implemented command — no
state, no registers — so "did it execute" is not observable through the driver.

Comparing with `cpusys_vm` (vmid 50), the one guest that runs on this device:

* it is started **without** proxy scheduling — dmesg says
  `Skip populating VCPU affinity info for VM=50` /
  `Proxy Scheduling isn't supported for VM=50`;
* our VM 45 *is* proxy-scheduled (`vmid=45 … virq_num=406/407 … nr_vcpus:1`),
  because the guest DT asks for it: `vcpus { affinity = "proxy";
  affinity-map = <0x05 0x06>; }`.

So b02 was patched to drop `affinity`/`affinity-map`. With the real vendor kernel
the host then survived **8 s** in one run — previously it reset at ~0.5 s — while
a second run reset again. The guest itself still aborts in ~0.2 s (the probe's
own 50 ms sample loop stops after a handful of iterations), so this is not yet
a fix, but it is the first change that altered the *host* behaviour at all.

**Correction ([34](34-proxy-does-not-matter.md)):** a controlled 3+3 run shows
proxy scheduling makes no difference — the host survives in both variants once
`crash-fatal` is gone, and the guest dies in both. Repeating also showed the
"survived 8 s" was host survival, not guest liveness.

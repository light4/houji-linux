# Where `started running` is printed, and no vCPU state ioctl

To pin down whether the vCPU ever gets to the hypervisor, the print site was
located: `gunyah_loader: VM:%d started running` is emitted from `gh_vcpu_ioctl`
(the `GH_VCPU_RUN` branch, `.text+0x14e8`) **after** the run hypercall is
entered. So `VCPU_RUN` does reach the hyp and the vCPU is registered — that part
is not broken.

Enumerating the *vCPU fd* range `0xB240`..`0xB2C0` (skipping create/run) gives
**nothing**: no get-state, no get/set registers. So "did the PC advance?" cannot
be answered through this driver at all; the only observable is the backend event
path, which stays silent (`journal/31`).

## Remaining ideas

* compare with the one VM that *does* run on this device: `cpusys_vm` (vmid 50,
  started at boot, `vendor.qvirtmgr.cpusys_vm.status = RUNNING`). Its start lines
  in dmesg, its proxy-sched setup and its VM-create arguments are a reference we
  have not read yet.
* the guest memory for our VM comes from `trust_ui_vm_region@f3800000`; whether
  the hyp actually maps the *kernel segment* to the guest IPA the DT declares is
  the open mapping question (`journal/31`), and nothing in userspace can check it.

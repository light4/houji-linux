# The Android virtualization framework here is KVM-only, so it inherits the wall

A common shortcut on Snapdragon 8 Gen 3 is "the Terminal app only fails because
of a property; set `androidboot.hypervisor.protected_vm.supported=1` and it
works". On this device that is not true, and it is worth knowing why before
burning a day on it.

## What the property actually gates

`android.system.virtualmachine` checks two boot properties before it ever
talks to the hypervisor:

```
ro.boot.hypervisor.vm.supported            (non-protected VMs)   -> absent
ro.boot.hypervisor.protected_vm.supported  (protected VMs)       -> false
```

Both false => `VirtualMachineConfig.setProtectedVm()` throws
`UnsupportedOperationException` and the Terminal surfaces "Unrecoverable
error" before any Gunyah call is made.

## But this AVF build has no Gunyah backend

```
ls /apex/com.android.virt/lib64/                  # crosvm libs, no libgunyah
strings /apex/com.android.virt/bin/virtualizationservice | grep -c gunyah   -> 0
```

The framework is present (`/apex/com.android.virt/bin/{vm,virtualizationservice}`,
`crosvm`), but it is the crosvm-on-KVM flavour. With the properties forced on,
`VirtualizationService` would go looking for `/dev/kvm`, which does not exist
([01](01-secure-launch-absent.md)). So the property is a second gate, not the
gate.

## Verdict per layer

| layer | state |
|---|---|
| `android.system.virtualmachine` / `vm` / Terminal app | blocked by missing `/dev/kvm` |
| Qualcomm QVT (`qvirtservice_rs`, `virtmgr`, `/dev/gunyah`) | **alive** — `vendor.qvirtmgr.cpusys_vm.status=RUNNING`; a Gunyah VM is running right now |
| user access to QVT | none — vendor-private, fixed images in `/vendor/vm-system/` |

So "Android hardware virtualization" on this firmware is not zero: the vendor
runs Gunyah guests. It is just not reachable through any public API, and the
public API is KVM-only here.

# Forcing the stock AVF path: same wall, and a clearer diagnosis

## Getting `vm run-microdroid` to even try

Two gates, both bypassable, both worth recording:

```bash
# 1. the userspace property gate (see 06)
adb shell su -c 'export PATH=/data/adb/ksu/bin:$PATH
  resetprop -n ro.boot.hypervisor.vm.supported true
  resetprop -n ro.boot.hypervisor.protected_vm.supported true
  resetprop -n hypervisor.pvmfw.path /data/local/tmp/pvmfw.img'

# 2. SELinux: virtualizationservice is denied setrlimit on our su (ksu) process
adb shell su -c 'setenforce 0'
```

Without `setenforce 0` the run dies in
`virtmgr/src/main.rs: Failed to remove memlock rlimit: Permission denied`, with a
matching `avc: denied { setrlimit } scontext=u:r:virtualizationservice:s0
tcontext=u:r:ksu:s0`.

## What happens then

The stock stack runs *all the way to crosvm* — instance created, `instance.img`
written, and three block devices attached through crosvm's own gunyah backend:

```
Created debuggable VM from ".../EmptyPayloadApp.apk" ... with CID 2048, state is STARTING.
Trying to attach block device: /proc/self/fd/53
Trying to attach block device: /proc/self/fd/58
Trying to attach block device: /proc/self/fd/67
failed to initialize virtual machine No such device (os error 19)
VM ended: StartFailed
dmesg: misc gunyah: Failed to start VM: -19
```

Identical failure to our manual `crosvm --protected-vm-with-firmware`. So it is
not our harness, and it is not crosvm's age: the stock AVF cannot start a pVM on
this build either.

## What the kernel says about it

`Failed to start VM: %d` lives in the built-in `gh_vm_mgr` and is printed right
after a call to the RM start helper (`0xc39838`, taking the vmid) returns
non-zero. Two stages therefore exist and only the first was ever reached:

| stage | what it does | our result |
|---|---|---|
| configure pVM firmware | registers the pvmfw with the hypervisor via RM (`gh_vm_mgr: Failed to configure pVM firmware`) | `EPERM` |
| start | `RM_VM_START(vmid)` | `ENODEV` |

`--protected-vm-with-firmware` makes crosvm place the blob in *guest memory*;
the configuration step that tells the hypervisor about it is skipped, so the RM
start finds no firmware registered and returns `-ENODEV`.

## Consequence for the plan

This makes the pVM/crosvm route depend on a Xiaomi-provided pvmfw (or on
building one and fitting it into the vendor config path), while the **vendor
loader route needs no pvmfw at all** — `trustedvm` already starts with a
`qtestsign`-signed bundle ([09](09-custom-system.md)). The remaining work there
is the device model (virtio backends) and it is entirely under our control.

So: back to the virtio-blk/console backend, with the pVM route parked until a
pvmfw ships or someone finds the RM register call.

## Full-partition backup, since this needs one

`adb exec-out` streaming works for every named partition as root:

```bash
for p in $(adb shell su -c 'ls /dev/block/by-name'); do
  adb exec-out su -c "dd if=/dev/block/by-name/$p bs=4M" > "backup/$p.img"
done
```

Caveats: ~1.9 TiB in total (943 GiB of it `userdata`), logical partitions inside
`super` are only addressable as a whole, and `fastboot fetch` (fastbootd) covers
logical ones without root. GPT: `sgdisk --backup`.

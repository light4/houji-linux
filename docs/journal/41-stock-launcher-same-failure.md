# Stock ROM check, and the stock launcher fails the same way

Three questions: is the phone really on the stock ROM, why does `cpusys_vm`
boot, and can a `/dev/qgunyah` VM boot at all?

## The device matches `~/Downloads/houji_rom` (OS3.0.7.0.WNCCNXM)

Compared byte-for-byte against `houji_images_OS3.0.7.0.WNCCNXM_16.0/images`
(the partition prefix of the image's length, sha256):

| partition | image | result |
|---|---|---|
| `hyp_a` | `hypvmperformance.mbn` (what `flash_all.sh` writes, **not** `hypvm.mbn`) | same |
| `tz_a`, `devcfg_a`, `xbl_a`, `xbl_config_a`, `abl_a` | | same |
| `boot_a`, `vendor_boot_a`, `dtbo_a`, `vbmeta_a`, `vbmeta_system_a` | | same |
| `vm-bootsys_a` | `vm-bootsys.img` (sparse → unsparsed) | same |
| `modem_a` (vfat, `/vendor/firmware_mnt`) | `NON-HLOS.bin` | partition hash differs, **all 475 files identical** — only FAT metadata changed by our temporary bundle copies |
| `init_boot_a` | | differs: KernelSU patch (expected) |

`super` is covered by dm-verity: `vbmeta`/`vbmeta_system` are identical and
`ro.boot.veritymode=enforcing`, so `system`/`vendor`/`product` are the ROM's.

## Why `cpusys_vm` boots

It is launched by the stock stack through the **same** driver:

```
pid 1100  /system_ext/bin/qcrosvm --vm=cpusys_vm
  fd 5,6 /dev/qgunyah   fd 7 anon_inode:gunyah-vm   fd 8 anon_inode:gh-vcpu:0
  thread cpusys_vm_vcpu0  wchan gh_vcpu_ioctl
```

So `/dev/qgunyah` + `gunyah_loader` + `qcrosvm` do run a VM on this ROM.
`journal/40` was wrong: our `qcrosvm` attempts failed because we passed `--mem`,
which takes the "reserved mem" path the stock config never uses, and
`--vm=cpusys_vm` was already running.

## What the stock stack does with `trustedvm`

`/vendor/etc/qvirtmgr-vndr.json` **does** have a `trustedvm` entry (`journal/20`
only read the tail of the file):

```json
"name": "trustedvm", "legacy": true, "autostart": false, "on_demand_start_supported": true,
"disk": [ { "image": "/data/misc/vm-system/system.img", "label": 10, "read_write": false },
          { "image": "/mnt/product/vm-persist/trustedvm/persist.img", "label": 11, "read_write": true } ]
```

So label `0x10` is the guest's read-only system disk and `0x11` its persist disk
— **both virtio-block**; `0x11` is not a console (correcting `journal/18`). The
user is `vendor.xiaomi.hardware.mitrustedui-service` (Trusted UI) through
`vendor.qti.qvirt.IVirtualizationService`.

Driving that service directly (`tools/qvirt_start.c`: `getVm` = code 1,
`IVirtualMachine.getState` = 1, `start` = 3):

* the service spawns `qcrosvm` for `trustedvm`, which exits with status 1 within
  2 s — `/data/misc/vm-system/system.img` **does not exist** on this ROM;
* the service nevertheless reports `RUNNING` (`vendor.qvirtmgr.trustedvm.status`)
  — its state is bookkeeping, not a liveness check.

## Stock launcher, stock bundle, disks provided

```
qcrosvm --vm=trustedvm --disk=disk1.img,label=10 --disk=persist-copy.img,label=11,rw=true --log=type=term
```

`qcrosvm` sets up both block devices (features `0x20000064`/`0x20006244` +
`0x403`, `queue_max 0x80`, full config space), the loader logs
`VM:45 started running` — and the phone hard-resets within ~0.1 s, with nothing
after that line. **Exactly the signature of our own probe.**

## Consequence

* The failure is not in our tool: the vendor launcher with the untouched vendor
  bundle dies identically.
* `/dev/qgunyah` VMs can boot on this ROM (`cpusys_vm` does); `trustedvm`
  specifically cannot, by any launcher, in this configuration.
* Differences that remain between the two VMs: `trustedvm` lends real device
  ranges to the guest (`iomemory-ranges` incl. `0xa90000`-area QUP/SPI used by
  the touch controller, `gic-irq-ranges`), uses the CMA-backed
  `trust_ui_vm_region` and proxy scheduling, and in the real product flow is only
  started after the Trusted UI HAL has handed display/touch over. Starting it
  without that hand-off is the most plausible cause; not yet proven.

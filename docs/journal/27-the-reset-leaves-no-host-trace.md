# The reset leaves no host trace

Trying to identify *who* resets the phone after `started running`:

* no watchdog device exists (`/dev/watchdog*`, `/sys/class/watchdog` are empty),
  `gh_virt_wdt` is only the guest-side virt watchdog under `qcom_wdt_core`.
* `gh_panic_notifier` cannot be unloaded (`rmmod` → `Try again`, it is in use).
* the boot reason is the generic `reboot`; `bootinfo.pureason=0x80001`,
  `pdreason=0x2`.
* the kernel is configured for oops persistence (`mtdoops` on
  `/dev/block/by-name/oops`, `printk.always_kmsg_dump=1`,
  `mtdoops.dump_oops=1`), which is a good channel in general — but a run that
  ends in this reset writes **no new record**: the partition still holds an older
  boot's logcat tail, and none of the records contain `Kernel panic`.

So the reset is not a kernel panic and not a userspace watchdog: it looks like a
hard reset issued from the hyp/SCM side (`gh_vm_mgmt` restarting after the VM
state goes bad), which would also explain why removing `crash-fatal` changed
nothing (`journal/26`).

## Consequence

Host-side debugging has run out of channels: no console, no per-label device
nodes (`journal/26`), no queue-info ioctl (`journal/21`), and now no panic trace.
The only way forward is to give the *guest* a working early channel — a patched
`b02` with a console that does not need virtio, or our own guest.

## Useful side discovery

`/dev/block/by-name/oops` + `mtdoops` captures the tail of the kernel log across
a reset whenever a panic/oops happens, which is much better than racing `dmesg`
by hand. Keep it in mind for any *host-side* crash work.

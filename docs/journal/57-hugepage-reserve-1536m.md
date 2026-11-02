# gh-hugepage-reserve: the guest gets 1.5 GiB

* Installed `gh-hugepage-reserve` **v12** (Droid-VM release zip) with
  `ksud module install`. On boot `post-fs-data.sh` insmods
  `ko/gh_hugepage_reserve/android14-6.1.ko` before memory fragments, with a crash
  watchdog: a `stamp` written at post-fs-data and cleared at boot-completed;
  a leftover stamp → `crash` → `disable` on the next boot.
* **`pool_want` is in 2 MiB pages** (README: "pool share target (2MB pages)").
  I first set 2048 = 4 GiB; reduced live with
  `echo 1024 > /sys/module/gh_hugepage_reserve/parameters/pool_want` and in
  `/data/adb/modules/gh-hugepage-reserve/settings.prop` → 2 GiB (the default).
* Disabled the old `gunyah-kvcalloc-fix` KSU module (touch `disable`); DroidVM's
  own `gunyah_kvcalloc_gki_6.1` hooks the same function.
* DroidVM runtime modules GH Unmovable / Gunyah Host Share / Gunyah kvcalloc Fix
  now **auto-load** with the daemon (Settings → Kernel Modules).
* After a reboot, app data (`/data/data/...`) stays unreadable until the phone
  is unlocked once (CE storage), so DroidVM can't start VMs before that.

## Result

`memory_mb 1536` (via `guest/dvcfg.sh`, which stops the app and its daemon, then
edits `vms.json`). `VM_START` succeeds; the pool reports `served=768` (= 1536 MiB
in 2 MiB pages), `active_vms=1`. Guest:

```
               total        used        free      shared  buff/cache   available
Mem:            1384         201        1199           0          76        1183
Swap:           2047           0        2047
```

The 256 MiB ceiling of journal/53–54 was fragmentation; with pool pages the
whole guest RAM goes out as 2 MiB blocks.

## Managing it on the phone

DroidVM → VM → *arch*: Start/Stop/Restart/Power; **Console → Text Console
Serial 1** is the guest's ttyS0 (root autologin, ESC/CTRL/TAB/arrow keys);
Console → CrosVM stdout/stderr for the launcher log; Edit for config. From a PC:
`guest/ssh_relay.sh` on the phone + `adb forward tcp:2222 tcp:2222`.

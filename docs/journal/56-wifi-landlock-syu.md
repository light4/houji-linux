# Wi-Fi uplink, Landlock/RTC guest kernel, full `pacman -Syu`

* **Uplink**: with the phone on Wi-Fi, the DroidVM bridge NAT works directly
  (`1.1.1.1 via 192.168.80.1 dev wlan0 table wlan0`); the guest pings out,
  resolves names and reaches the mirrors. The Mac proxy/relay from journal/55 is gone.
  `systemd-timesyncd` syncs on its own.
* **Guest kernel v2** (`guest/build_guest.sh`): `RTC_DRV_PL030` (crosvm's RTC
  at `rtc@2000` is a PL030 primecell; defconfig only had PL031),
  `SECURITY_LANDLOCK`, `SECURITY_YAMA`, `LSM=landlock,lockdown,yama,integrity,bpf`.
  Result: `/sys/kernel/security/lsm` = `capability,landlock,yama`, `/dev/rtc0`.
  pacman's download sandbox now works, so `--disable-sandbox` is no longer needed. The
  default mirrorlist works too, now that there's no proxy in between.
* **Disk**: `truncate -s 8G alarm.img` with the VM stopped, `resize2fs /dev/vda`
  online → 7.8 G, 5.5 G free.
* **Memory**: the first `pacman -Syu` was OOM-killed (`anon-rss 49 MB`; the guest
  has 129 MiB usable) before touching any package. Added a 2 GiB `/swapfile`
  (in fstab) and reran: 78 packages upgraded, rc 0 → systemd 262.1, pacman 7.1,
  glibc 2.43. After `systemctl reboot` (DroidVM restarts the VM): `running`,
  no failed units.
* Helpers: `guest/ssh_relay.sh` (phone-side `nc` relay for `adb forward`),
  `guest/kill_relay.sh`.

Still open: guest RAM above 256 MiB (`gh-hugepage-reserve`), the
pseudo-unprotected shim exception, and dropping the unused `linux-aarch64`/
firmware packages (we boot an external kernel).

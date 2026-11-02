#!/bin/bash
# Build an ext4 disk image from the Arch Linux ARM aarch64 tarball for the Gunyah guest.
set -euo pipefail
command -v mkfs.ext4 >/dev/null || apt-get -qq install -y e2fsprogs >/dev/null
R=/tmp/alarm; IMG=/work/alarm.img
rm -rf "$R"; mkdir -p "$R"
tar -xpzf /work/alarm.tar.gz -C "$R" --numeric-owner 2>/dev/null || true
test -x "$R/usr/lib/systemd/systemd"

# Guest kernel is monolithic and booted externally: no /boot kernel, no modules.
cat > "$R/etc/fstab" <<'EOF'
/dev/vda  /  ext4  rw,relatime  0 1
EOF
echo houji-vm > "$R/etc/hostname"
# root password "root"; disable the default alarm user's password login.
cp /etc/resolv.conf "$R/etc/resolv.conf" 2>/dev/null || true
chroot "$R" /bin/sh -c 'echo root:root | chpasswd'
# Autologin root on the serial console so a boot test reaches a shell unattended.
mkdir -p "$R/etc/systemd/system/serial-getty@ttyS0.service.d"
cat > "$R/etc/systemd/system/serial-getty@ttyS0.service.d/autologin.conf" <<'EOF'
[Service]
ExecStart=
ExecStart=-/sbin/agetty -o '-p -f -- \\u' --autologin root --keep-baud 115200,57600,38400,9600 %I $TERM
EOF
# First-boot report written to the console.
cat > "$R/etc/profile.d/zz-report.sh" <<'EOF'
if [ "$(tty)" = /dev/ttyS0 ] && [ ! -e /run/reported ]; then
  touch /run/reported
  echo "=== Arch Linux ARM in a Gunyah guest ==="
  cat /etc/os-release | head -2; uname -a; systemctl is-system-running
  free -m | head -2; df -h / | tail -1
fi
EOF
rm -f "$IMG"; truncate -s 3G "$IMG"
mkfs.ext4 -q -F -L alarm -d "$R" "$IMG"
ls -l "$IMG"

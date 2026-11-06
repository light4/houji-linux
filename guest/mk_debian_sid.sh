#!/bin/bash
# Build a Debian sid arm64 root disk for the DroidVM Gunyah guest (external kernel, /dev/vda).
# Run (rootless podman; container=lxc makes debootstrap bind-mount /dev instead of mknod):
#   podman run --rm --privileged --arch arm64 -v ~/kmb:/work debian:bookworm /work/mk_debian_sid.sh
set -euo pipefail
MIRROR=https://mirrors.tuna.tsinghua.edu.cn/debian
R=/tmp/sid; IMG=/work/debian-sid.img; SIZE=8G
export DEBIAN_FRONTEND=noninteractive container=lxc
apt-get update -qq && apt-get install -y -qq debootstrap e2fsprogs ca-certificates >/dev/null
rm -rf "$R"; mkdir -p "$R"
debootstrap --arch=arm64 --variant=minbase \
  --include=systemd,systemd-sysv,systemd-resolved,systemd-timesyncd,dbus,udev,openssh-server,ca-certificates,iproute2,iputils-ping,sudo,less,vim-tiny,curl,apt-utils,locales,bash-completion,kmod,procps \
  sid "$R" "$MIRROR"
echo houji-sid > "$R/etc/hostname"
printf '127.0.0.1 localhost\n127.0.1.1 houji-sid\n' > "$R/etc/hosts"
echo '/dev/vda / ext4 defaults,noatime 0 1' > "$R/etc/fstab"
cat > "$R/etc/apt/sources.list.d/debian.sources" <<S
Types: deb
URIs: https://mirrors.tuna.tsinghua.edu.cn/debian
Suites: sid
Components: main contrib non-free non-free-firmware
Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg
S
rm -f "$R/etc/apt/sources.list"
mkdir -p "$R/etc/systemd/network"
printf '[Match]\nName=en* eth*\n\n[Network]\nDHCP=yes\n' > "$R/etc/systemd/network/20-wired.network"
mkdir -p "$R/etc/systemd/system/serial-getty@ttyS0.service.d"
printf '[Service]\nExecStart=\nExecStart=-/sbin/agetty --autologin root --keep-baud 115200,57600,38400,9600 %%I $TERM\n' \
  > "$R/etc/systemd/system/serial-getty@ttyS0.service.d/autologin.conf"
chroot "$R" /bin/bash -c '
  set -e
  echo root:root | chpasswd
  useradd -m -s /bin/bash -G sudo debian && echo debian:debian | chpasswd
  systemctl enable systemd-networkd systemd-resolved systemd-timesyncd ssh serial-getty@ttyS0.service
  sed -i "s/^# *en_US.UTF-8/en_US.UTF-8/" /etc/locale.gen && locale-gen >/dev/null
  apt-get clean'
ln -sf /run/systemd/resolve/stub-resolv.conf "$R/etc/resolv.conf"
rm -f "$IMG"; truncate -s "$SIZE" "$IMG"
mkfs.ext4 -q -L sidroot -d "$R" "$IMG"
ls -l "$IMG"; chroot "$R" cat /etc/os-release | head -3

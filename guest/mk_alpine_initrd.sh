#!/bin/bash
set -e
R=/tmp/rootfs; rm -rf $R; mkdir -p $R
tar -xzf /work/initrd/alpine-mini.tgz -C $R
cat > $R/init <<'INIT'
#!/bin/sh
mkdir -p /proc /sys /dev
mount -t proc proc /proc; mount -t sysfs sys /sys
mount -t devtmpfs dev /dev || echo "devtmpfs mount failed"
for t in /dev/kmsg; do [ -c $t ] && { exec >$t 2>&1; break; }; done
echo; echo "=== hello from a Gunyah guest userspace ==="
head -2 /etc/os-release
uname -a
echo "cpus: $(grep -c ^processor /proc/cpuinfo)"; grep MemTotal /proc/meminfo
cat /proc/cmdline
ls /dev | tr '\n' ' '; echo
echo "=== done, powering off ==="
sync; sleep 2; poweroff -f
INIT
chmod 755 $R/init
cd $R && find . | cpio -o -H newc --quiet | gzip -9 > /work/initrd/alpine-initrd.cpio.gz
ls -l /work/initrd/alpine-initrd.cpio.gz

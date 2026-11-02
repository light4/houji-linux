#!/bin/bash
# Build a generic arm64 guest kernel (defconfig + virtio/systemd needs) for the Gunyah VM.
set -e
export ARCH=arm64 LLVM=1
cd /work/src
O=/work/guest
mkdir -p $O
make O=$O defconfig
scripts/config --file $O/.config \
  -e DMA_RESTRICTED_POOL -e DEVTMPFS -e DEVTMPFS_MOUNT -e FHANDLE \
  -e VIRTIO_BLK -e VIRTIO_PCI -e VIRTIO_MMIO -e VIRTIO_NET -e VIRTIO_CONSOLE -e HW_RANDOM_VIRTIO \
  -e VSOCKETS -e VIRTIO_VSOCKETS -e SERIAL_8250 -e SERIAL_8250_CONSOLE -e SERIAL_OF_PLATFORM \
  -e EXT4_FS -e BTRFS_FS -e SQUASHFS -e OVERLAY_FS -e AUTOFS_FS -e TMPFS_POSIX_ACL -e CGROUPS -e BPF_SYSCALL \
  -e NAMESPACES -e USER_NS -e PID_NS -e SECCOMP -e CRYPTO_USER_API_HASH -e DMIID \
  -d DEBUG_INFO -d DEBUG_INFO_BTF -d DEBUG_INFO_DWARF_TOOLCHAIN_DEFAULT -e DEBUG_INFO_NONE -d MODULES
make O=$O olddefconfig
make O=$O -j"$(nproc)" Image 2>&1 | tail -3
ls -l $O/arch/arm64/boot/Image

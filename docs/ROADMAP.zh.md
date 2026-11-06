[English](ROADMAP.md) | [中文](ROADMAP.zh.md)

# 路线图

Gunyah VM 这条路线已经可用（[STATE](STATE.zh.md)）。剩下的工作，大致按价值排序。

## 日常化

* **不用中转也能 ssh**：把中转做成 KernelSU 的 `service.sh`；或者直接在手机上用 Termux 的 `ssh`（`ssh debian@192.168.134.128`）。这样重启后不用手动恢复。
* **ssh 改用密钥**，不再用密码；密钥可用后，去掉串口上 root 的自动登录。
* **开机自动启动 VM**：让 DroidVM 的后台进程在开机（首次解锁之后）时拉起 VM。

## 客户机内核

* **v3 驱动**：`DRM_VIRTIO_GPU`、`DRM_SIMPLEDRM`、`VIRTIO_INPUT`、`SND_VIRTIO`。这些在 defconfig 里是 `=m`，`MODULES=n` 时被一起丢掉了，任何显示输出都需要它们。
* **文件 capability**：打开 `EXT4_FS_SECURITY`（`security.*` 扩展属性）；没有它，客户机里 `setcap` 会失败（journal/59，`ping` 暂时用 `ping_group_range` 绕过）。
* **用发行版自己的内核**：pseudo-unprotected 模式不再需要 `DMA_RESTRICTED_POOL`，原版 `linux-aarch64`（Arch）或 `linux-image-arm64`（Debian）也许可以通过 DroidVM 的 UEFI（`edk2-gunyah`）启动，然后跟着包管理器更新。
* **KASLR**：查清楚 shim 交接后早期重定位为什么出错（[journal/58](journal/58-pseudo-unprotected-nokaslr.md)），并反馈给上游。

## 资源

* **更多 vCPU**：现在是 4 个。SM8650 有 8 个核，分三档（1 + 5 + 2），继续加之前先测一下，再考虑设置 `cpu_affinity`。
* **更多内存**：同时调大 `pool_want`（单位是 2 MiB 页）和 `memory_mb`。大页池占着的内存 Android 用不了。

## 以后

* NixOS 镜像（同一个内核，换根文件系统）。
* 桌面，等 v3 驱动有了再说。
* 方案 1（原生 Linux）：只有目标变成替换 Android 时才做，见 [OPTIONS](OPTIONS.zh.md)。

厂商加载器路线的旧计划保留在 [reference/vendor-loader](reference/vendor-loader.md)（英文）。

[English](OPTIONS.md) | [中文](OPTIONS.zh.md)

# 在 houji 上运行 Arch Linux / NixOS 的方案

[FINDINGS](FINDINGS.zh.md) 之后有三条路可走。只有方案 3 是硬件虚拟化；方案 1 和 2 走的是 LineageOS 和 Droidian 的路子。**方案 3 现在已经可用**：在原厂内核上，Arch Linux ARM 作为 Gunyah 客户机运行在 DroidVM 里（[journal/51](journal/51-first-linux-guest-boots.md)–[58](journal/58-pseudo-unprotected-nokaslr.md)）。内核和 Droidian 的背景见 [journal/43](journal/43-kernel-config-and-droidian.md)。

## 为什么 LineageOS 那套能用，我们的 VM 一开始不行

这台手机上的一切都跑在 Gunyah 之下，有两条校验链：

| | 主 VM（HLOS = Android） | 次级 VM（`trustedvm`、`cpusys_vm`） |
|---|---|---|
| 由谁加载 | XBL → ABL，从 `boot`/`vendor_boot`/`super` | 运行时由 `gunyah_loader` 加载 |
| 由谁校验 | AVB | hyp/TZ PIL，小米签名根 |
| 解锁 bootloader 后 | 不再强制（orange 状态），任何内核都能启动 | **仍然强制** |

LineageOS 替换的是左边一列，所有签名过的厂商固件原样复用。它同样拿不到 `/dev/kvm`，也起不了自定义的次级 VM。

## 对比

| | 1. 原生 Linux 作为主系统 | 2. Android 内核 + 真 chroot/容器 | 3. Gunyah VM（`/dev/gunyah`，crosvm/DroidVM） |
|---|---|---|---|
| 硬件虚拟化 | 否（仍是 Gunyah 的主 VM，占着 EL1） | 否（共用 Android 的内核） | **是** |
| 独立系统 | 是，有自己的内核和 init | 否，只是隔离的用户空间 | 是，有自己的内核和 systemd |
| 状态 | 未开始 | 可行，未搭建 | **可用**：Arch，4 vCPU、1.4 GiB，能联网、能 `pacman -Syu` |
| 宿主需要的改动 | 自定义 `boot`（+`vendor_boot`） | root | root（KernelSU）、`gh-hugepage-reserve` + DroidVM 运行时模块；原厂内核 |
| 性能 | 原生 | 原生 | CPU 接近原生；I/O 走 virtio |
| 硬件 | 手机自己的硬件，见下文"驱动" | 经 Android 内核使用 Android 的硬件；自己没有显示 | 只有 virtio：磁盘、网络、串口（GPU/输入/声音要等客户机内核 v3） |
| 对 Android 的影响 | 替换掉它（或双系统） | 无 | 无；Android 同时运行 |
| 风险 / 回退 | 中：要刷 `boot`（+`vendor_boot`），可用原厂备份恢复 | 低：只是 `/data` 里的文件 | 低：用户空间 + KSU 模块；禁用模块、重启即可 |
| 工作量 | 数周到数月 | 几小时 | 已完成（包括走过的弯路，用了几天） |

### 优缺点

**1. 原生 Linux 作为主系统**

* \+ 完全原生的性能，能用手机自己的硬件：显示、GPU（KGSL 上的 turnip）、触摸、电池、USB、Wi-Fi；
* \+ 是真正的手机系统，不是 Android 里的一个客户机；
* − Android 没了（或者要做双系统），基带、相机、音频、指纹也随之失去，除非再加上 Halium/libhybris；
* − 内核必须重编（`DEVTMPFS`、`FHANDLE`、命名空间），而且要和 300 多个厂商模块保持 CRC 兼容（`MODVERSIONS`、`TRIM_UNUSED_KSYMS`）；
* − houji 还没有 Linux 移植（已有一个原生 XNU 移植，见下文）；适配要数周到数月；每次刷机都可能刷到无法开机（可以用 fastboot 备份恢复）。

**2. Android 内核 + 真 chroot/容器**

* \+ 最快：几小时就能用，文件都在 `/data`，不用刷任何东西；
* \+ 原生速度（不像 proot 那样靠 ptrace），能直接用 Android 的网络和存储；
* − 不是独立系统：用的是 Android 的内核，没有自己的 init，systemd 当不了 PID 1；
* − 原厂内核缺 `PID_NS`/`USER_NS`：没有 systemd-nspawn/LXC，也没有 Nix 沙箱；
* − 自己没有显示（只能靠 VNC/X server 应用），跟着 Android 的生命周期走。

**3. Gunyah VM**

* \+ 真正的硬件虚拟化：用我们自己的内核，跑完整的 systemd 版 Arch，和 Android 隔离，宿主用的是**原厂**内核；
* \+ Android 照常可用；在 DroidVM 里启停和开终端；电脑上可以 ssh；
* \+ 回退很简单（停掉 VM，禁用 KSU 模块）；
* − 只有 virtio 设备：用不到手机的硬件（相机、基带、传感器）；GPU 加速走的是 DroidVM 的 virgl/gfxstream，不是原生；
* − 客户机内存来自开机时预留的 2 MiB 大页池（`gh-hugepage-reserve`，从 Android 那里占用 2 GiB）；没有它的话，开机一段时间后上限约 256 MiB；
* − 依赖 DroidVM 的 crosvm 分支和它的测试版，也依赖 root；
* − 手机重启后要先解锁一次才能启动 VM（CE 存储）；
* − 一些特殊处理：pseudo-unprotected 模式要加 `nokaslr`；客户机内核（不带模块）要我们自己维护。

## 方案 1：用现有内核和驱动跑原生 Linux

复用 GKI 内核（`6.1.118-android14`）和小米的厂商模块（`vendor_dlkm` 里 319 个，加上 `vendor_boot` 里的），替换掉 Android 的用户空间。

**内核缺的选项**（来自 `/proc/config.gz`），都会卡住 systemd，因此 Arch 和 NixOS 都受影响：

| 选项 | 原厂 | 用途 |
|---|---|---|
| `DEVTMPFS` | 关 | systemd/udev（硬性要求） |
| `FHANDLE` | 关 | systemd（硬性要求） |
| `PID_NS`、`USER_NS` | 关 | nspawn、容器、Nix 沙箱构建 |
| `SYSVIPC` | 关 | 不少程序要用 |
| `VT`、`AUTOFS_FS` | 关 | 可选 |

所以必须从 ACK `android14-6.1` 带上这些选项重编内核，同时保证厂商模块还能加载。限制在于：`MODVERSIONS` 和 `TRIM_UNUSED_KSYMS` 是开的，模块会检查符号的 CRC；`MODULE_SIG_FORCE` 是关的（问题不在签名，在 CRC）。`DEVTMPFS`/`FHANDLE`/`PID_NS` 大概率不影响 KMI；`SYSVIPC`/`USER_NS` 风险更大。

**驱动**：内核侧大多都有，问题在用户空间：

| 硬件 | 内核 | 普通 Linux 用户空间 |
|---|---|---|
| 显示 | `/dev/dri/card0`、DSI-1、SDE atomic KMS | DRM/KMS 合成器（weston、sway）；可能有下游驱动的怪癖 |
| GPU | `/dev/kgsl-3d0`（高通 KGSL，不是 freedreno） | Mesa turnip 有 KGSL 后端（Vulkan，GL 走 zink），或者 libhybris |
| 触摸、按键 | evdev | 可用 |
| 电池/充电 | `power_supply` sysfs | 可用 |
| USB 网络/串口 | configfs gadget | 可用，是 SSH 的第一个目标 |
| Wi-Fi | `kiwi_v2`、cfg80211/nl80211 | 大概可用，可能需要 `cnss-daemon` |
| 音频、基带、相机、指纹、传感器 | 有 | 需要 Android HAL → Halium/libhybris |

**Droidian** 已经在维护这条路线：`linux-android-common-6.1-android14`（ACK 6.1.174，加上开启 `DEVTMPFS`、`PID_NS`、`USER_NS`、`SYSVIPC`、`VT`、`CGROUP_PIDS` 等的配置片段；片段里没看到 `FHANDLE`）。它没有 houji 或 SM8650 的移植，而且是 Debian 系，不是 Arch/NixOS。能复用的是内核的配置方法和 Halium/libhybris 这套栈。

**计划**

1. 离线：把 Droidian 6.1 内核导出的符号（`Module.symvers`）和原厂厂商模块要求的 CRC 对比一遍；确认 `FHANDLE`；
2. 先按原厂配置编一个 ACK `android14-6.1` 内核，确认所有厂商模块都能加载，再一个个加选项；
3. 最小 initramfs：加载厂商模块，挂载 Arch Linux ARM 根分区，只开 USB 网络 + SSH；
4. 然后是显示 + GPU（KGSL 上的 turnip），再是 Wi-Fi；只有需要音频/基带时才上 Halium。

只刷 `boot_a`（可能还有 `vendor_boot_a`），见 [RESTORE](RESTORE.zh.md)。

**新的参考项目（2026-11）**：[UniXNU-houji](https://github.com/lonzzi/UniXNU-houji) 通过 **Mu-Silicium UEFI 的 houji 板级包**在 houji 上原生启动了 iOS 16 / XNU。这个 UEFI 只在内存里运行，打包成 boot 镜像，可以用 `fastboot boot` 测试而不刷任何分区；它和本方案一样运行在 EL1。它还引用了**小米官方的内核和设备树源码**（MiCode `shennong-u-oss`）。这对方案 1 有两点意义：(a) 用 MiCode 的源码把内核**和**厂商模块一起重编，就不用再对 CRC；(b) 或者通过这个 UEFI 启动 Linux 内核，设备树从 MiCode 的 DTS 派生。见 [reference/links](reference/links.md)。

## 方案 2：Android 内核 + 真 chroot/容器

有了 KernelSU 的 root，就可以真正 chroot 进 Arch Linux ARM，或者装 Nix，速度是原生的（不像 proot 靠 ptrace）。限制同样来自内核配置：没有 `PID_NS`/`USER_NS`，就没有 systemd-nspawn/LXC，也没有 Nix 沙箱；systemd 当不了 PID 1。适合快速、实用的场景；但不是独立系统，也不是虚拟化。

## 方案 3：`/dev/gunyah` 上的 Gunyah VM

唯一一条能用我们自己的内核、又是硬件虚拟化的路线。经过见 [journal/42–50](journal/)：厂商加载器只能启动签名过的 VM；无认证路径在 `VM_START` 失败，一开始被误读成 `NOMEM`，又被归咎于一个 VMID 内核 bug，结果那个补丁在这台机器上什么也没改。

真正可行的做法（[journal/51](journal/51-first-linux-guest-boots.md) 起）：

* **原因**：RM 要求客户机的主内存块是**借出（lend）**，而不是共享（share）。`GH_VM_SET_USER_MEM_REGION` 走的是共享，所以 `VM_START` 返回 `NORESOURCE`。DroidVM 的 crosvm 用借出（`--protected-vm-without-firmware`），或者用它的 pseudo-unprotected shim；
* **内存**：超过约 256 MiB 时，借出会因内存碎片失败；`gh-hugepage-reserve` 从开机时预留的池子里提供 2 MiB 大页（[journal/57](journal/57-hugepage-reserve-1536m.md)）；
* **客户机**：ACK 6.1 `defconfig` 内核，加上 virtio、Landlock、PL030 RTC（`guest/build_guest.sh`）；Arch Linux ARM 的 ext4 磁盘（`guest/mk_alarm.sh`）；
* **管理**：DroidVM 应用，经手机 Wi-Fi 的 NAT 网桥。

两种 VM 模式都能用：

| 模式 | 优点 | 缺点 |
|---|---|---|
| `pseudo_unprotected`（DroidVM 默认，正在用） | 不需要 swiotlb：多 58 MiB 内存，virtio I/O 不用中转；理论上能用发行版原版内核 | 用我们的内核要加 `nokaslr`（[journal/58](journal/58-pseudo-unprotected-nokaslr.md)）；依赖 DroidVM 的 shim |
| `protected_without_firmware` | 最简单，不需要 shim | 客户机内核要有 `DMA_RESTRICTED_POOL`；swiotlb（64 MiB）从客户机内存里扣，virtio I/O 要经过它中转 |

## 建议

* **用方案 3**：已经可用，Arch Linux ARM 作为 Gunyah 客户机和 Android 同时运行，由 DroidVM 管理。后续工作见 [ROADMAP](ROADMAP.zh.md)；
* 不想用 VM 时，方案 2 仍是一个快速的备选（几小时就能搭好）；
* 只有目标变成替换 Android 时才考虑方案 1，从离线检查厂商模块的 CRC 开始。

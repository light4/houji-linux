[English](STATE.md) | [中文](STATE.zh.md)

# 当前状态

Arch Linux ARM 作为 Gunyah 客户机运行在小米 14 上，与 Android 同时运行，宿主用的是原厂内核。怎么走到这一步：[FINDINGS](FINDINGS.zh.md)；路线对比：[OPTIONS](OPTIONS.zh.md)；回退：[RESTORE](RESTORE.zh.md)。

## 宿主（手机）

| 项 | 状态 |
|---|---|
| ROM | HyperOS `OS3.0.7.0.WNCCNXM`，bootloader 已解锁 |
| 内核 | **原厂** `boot_a`（`6.1.118-android14-11-gca0ef6d17716-ab13624819`） |
| root | KernelSU v3.3.1 LKM，打在 `init_boot` 里；管理器 v3.3.1 |
| KSU 模块 | `gh-hugepage-reserve` v12（`pool_want=1024`，即 2 GiB 的 2 MiB 大页）；`gunyah-kvcalloc-fix` 已禁用 |
| DroidVM | `0.0.6.r242.3348830-dev`；运行时模块 GH Unmovable、Gunyah Host Share、Gunyah kvcalloc Fix 随其后台进程自动加载 |
| 网络 | DroidVM 的 Linux bridge `br-net0`，192.168.233.1/24，NAT + DHCP，经 Wi-Fi 出网 |
| SELinux | enforcing |

## 客户机（DroidVM 里的 VM `arch`）

| 项 | 值 |
|---|---|
| 模式 | `pseudo_unprotected`（DroidVM shim），cmdline `console=ttyS0 root=/dev/vda rw rootwait nokaslr` |
| CPU / 内存 | 4 vCPU，`memory_mb 1536` → 可用 1442 MiB，另有 2 GiB swap 文件 |
| 内核 | `/data/local/tmp/guest.Image`：ACK 6.1 `defconfig` + virtio、Landlock、PL030 RTC，不带模块（`guest/build_guest.sh`） |
| 磁盘 | `/data/local/tmp/alarm.img`，8 GiB ext4 → `/dev/vda`（`guest/mk_alarm.sh`） |
| 系统 | Arch Linux ARM，已完整升级（systemd 262、pacman 7.1），`linux-aarch64` 和 firmware 包已卸载 |
| 网络 | `enp0s6`，DHCP 分到 `192.168.233.128`，经手机 Wi-Fi 上网 |
| 账号 | `root`/`root`（串口自动登录），`alarm`/`alarm`（ssh） |

## 使用

* **在手机上**：DroidVM → VM → *arch*，可以启动、停止、重启。*Console → Text Console Serial 1* 是客户机的 ttyS0，root shell，屏幕下方有 ESC/CTRL/TAB/方向键。
* **从电脑连**：在手机上以 root 运行 `guest/ssh_relay.sh`，然后执行 `adb forward tcp:2222 tcp:2222` 和 `ssh -p 2222 alarm@127.0.0.1`。
* **改配置**：用 DroidVM 的 *Edit*，或者 `guest/dvcfg.sh '<sed 表达式>'`。脚本会先停掉应用和它的后台进程，再改 `vms.json`。
* 手机重启后，要先解锁一次才能启动 VM，因为应用数据放在凭据加密存储（CE）里。

## 已知限制

* 客户机内存受大页池（`pool_want`）限制。没有大页池的话，开机一段时间后上限约 256 MiB（内存碎片）。
* 用我们的内核时，pseudo-unprotected 模式必须加 `nokaslr`。
* 只有 virtio 设备；客户机内核目前没有 GPU、输入和声音驱动。

已经放弃的厂商加载器路线的细节（ioctl 接口、bundle、各种坑）见 [reference/vendor-loader](reference/vendor-loader.md)（英文）。

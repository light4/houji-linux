[English](STATE.md) | [中文](STATE.zh.md)

# 当前状态

Debian sid 作为 Gunyah 客户机运行在小米 14 上，与 Android 同时运行，宿主用的是全球版 ROM 的原厂内核。怎么走到这一步：[FINDINGS](FINDINGS.zh.md)；路线对比：[OPTIONS](OPTIONS.zh.md)；回退：[RESTORE](RESTORE.zh.md)。换成全球版和重新搭建的过程见 [journal/59](journal/59-global-rom-and-debian-sid.md)（英文）。

## 宿主（手机）

| 项 | 状态 |
|---|---|
| ROM | HyperOS **全球版** `OS3.0.304.0.WNCMIXM`（硬件是国行，`hwc=CN`），bootloader 已解锁；journal/58 之前是 `OS3.0.7.0.WNCCNXM` |
| 内核 | **原厂** `6.1.138-android14-11-g0c3d559bcd85-ab14529422`；hypervisor 是 `gunyah-6de79709c perf`（没变） |
| root | KernelSU v3.3.1 LKM，打在 `init_boot` 里；管理器 v3.3.1 |
| KSU 模块 | `gh-hugepage-reserve` v12（`pool_want=1024`，即 2 GiB 的 2 MiB 大页）；另有 NeoZygisk、TEESimulator、PlayIntegrityFork（与虚拟机无关） |
| DroidVM | `0.0.6.r242.g3348830`（CI 构建，run `38251242629`）；运行时模块 GH Unmovable、Gunyah Host Share、Gunyah kvcalloc Fix 随其后台进程自动加载 |
| 网络 | DroidVM 的 Linux bridge `br-net0`，**192.168.134.1/24**，NAT + DHCP，经 Wi-Fi 出网 |
| SELinux | enforcing |
| 另外装了 | DroidDeck 0.6.9（`com.droiddeck.launcher`）；已设 `settings_enable_monitor_phantom_procs=false`。未修改的 fork：[light4/DroidDeck](https://github.com/light4/DroidDeck) |

## 客户机（DroidVM 里的 VM `debian-sid`）

| 项 | 值 |
|---|---|
| 模式 | `pseudo_unprotected`（DroidVM shim），Linux 启动协议，cmdline `console=ttyS0 root=/dev/vda rw rootwait nokaslr` |
| CPU / 内存 | 4 vCPU，`memory_mb 1536` → 可用 1446 MiB，暂时没有 swap |
| 内核 | `/data/local/tmp/guest.Image`：ACK 6.1 `defconfig` + virtio、Landlock、PL030 RTC，不带模块（`guest/build_guest.sh`） |
| 磁盘 | `/data/local/tmp/debian-sid.img`，8 GiB ext4 → `/dev/vda`（`guest/mk_debian_sid.sh`） |
| 系统 | Debian sid（`forky/sid`），apt 源 `mirrors.tuna.tsinghua.edu.cn/debian` |
| 设备 | 只有 virtio 块设备、网卡、rng；应用默认加的 `virtio_sound`（含麦克风）、`xhci_usb` 和 `simplefb` 屏幕已从 `vms.json` 里去掉 |
| 网络 | `enp0s2`（systemd-networkd），DHCP 分到 `192.168.134.128`，经手机 Wi-Fi 上网 |
| 账号 | `root`/`root`（串口自动登录），`debian`/`debian`（有 sudo，ssh 用这个） |

journal/53–58 的 Arch Linux ARM 虚拟机在重刷 ROM 时随 `/data` 一起被清掉了。升级前的镜像还在构建机的 `~/kmb/alarm.img`。

## 使用

* **在手机上**：DroidVM → VM → *debian-sid*，可以启动、停止、重启。*Console → Text Console Serial 1* 是客户机的 ttyS0，root shell，屏幕下方有 ESC/CTRL/TAB/方向键。
* **从电脑连**：在手机上以 root 运行 `guest/ssh_relay.sh`，然后执行 `adb forward tcp:2222 tcp:2222` 和 `ssh -p 2222 debian@127.0.0.1`。用完用 `guest/kill_relay.sh 2222` 和 `adb forward --remove tcp:2222` 关掉。
* **改配置**：用 DroidVM 的 *Edit*，或者 `guest/dvcfg.sh '<sed 表达式>'`。脚本会先停掉应用和它的后台进程，再改 `vms.json`。
* 手机重启后，要先解锁一次才能启动 VM，因为应用数据放在凭据加密存储（CE）里。

## 已知限制

* 客户机内存受大页池（`pool_want`）限制。没有大页池的话，开机一段时间后上限约 256 MiB（内存碎片）。
* 用我们的内核时，pseudo-unprotected 模式必须加 `nokaslr`。
* 只有 virtio 设备；客户机内核目前没有 GPU、输入和声音驱动。
* 不支持文件 capability：客户机内核没开 `EXT4_FS_SECURITY`，`setcap` 会失败（`ping` 改用 `ping_group_range`）。

已经放弃的厂商加载器路线的细节（ioctl 接口、bundle、各种坑）见 [reference/vendor-loader](reference/vendor-loader.md)（英文）。

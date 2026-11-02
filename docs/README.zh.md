[English](README.md) | [中文](README.zh.md)

# 文档

先看 [STATE](STATE.zh.md)（当前配置和使用方法）和 [FINDINGS](FINDINGS.zh.md)（一页纸，讲怎么走到这一步），然后是 [OPTIONS](OPTIONS.zh.md)（三条路线和优缺点）、[ROADMAP](ROADMAP.zh.md)（剩下的工作）和 [RESTORE](RESTORE.zh.md)（回退）。

下面的参考资料和实验日志只有英文版。

## 参考资料

* [vendor-loader](reference/vendor-loader.md)：已放弃的 `/dev/qgunyah` 路线的详细状态和计划
* [qgunyah-abi](reference/qgunyah-abi.md)：已实现的 ioctl 接口和坑
* [bundle-format](reference/bundle-format.md)：MBN bundle、dtbo 容器格式和签名流程
* [links](reference/links.md)：外部参考（DroidVM、VMID 修复、Droidian）
* [harness](reference/harness.md)：工具链、跨重置记录日志、stub 的坑

## 实验日志（按时间顺序，部分已被后文推翻）

实验日志就是实验记录本：试了什么、看到了什么、后来又更正了什么。与 `reference/` 冲突时，以参考资料为准。

### 路线分析

| | |
|---|---|
| [01](journal/01-secure-launch-absent.md) | 这套固件里没有 Secure Launch / slbounce |
| [02](journal/02-smc-is-smci.md) | `0x8600_xxxx` 是发给 TrustZone 的 SMCI，不是 Gunyah |
| [03](journal/03-gunyah-hvc-abi.md) | Gunyah 宿主 ABI 没有交还 EL2 的调用 |
| [06](journal/06-avf-is-kvm-only.md) | 这里的 AVF 是 crosvm-on-KVM，没有 Gunyah 后端 |
| [08](journal/08-arch-feasibility.md) | 跑自定义发行版需要什么 |
| [11](journal/11-crosvm-pvmfw.md) | crosvm / pVM 路线，卡在 pvmfw |
| [12](journal/12-pvmfw-obtained.md) | 从 Pixel 镜像里提取出 pvmfw |
| [13](journal/13-avf-through-vendor-path.md) | 强行走 AVF 路径；完整分区备份方法 |

### 厂商加载器路线

| | |
|---|---|
| [07](journal/07-gunyah-direct.md) | `/dev/qgunyah` 能驱动真正的 VM |
| [09](journal/09-custom-system.md) | ~~测试签名：`qtestsign` 被接受~~（已撤回，见 42） |
| [10](journal/10-rom-inventory.md) | ROM 清单，bundle 放在哪 |
| [14](journal/14-virtio-needs-dt.md) | 没有 DT，启动的客户机就没有设备（部分被推翻） |
| [15](journal/15-dtbo-overlay-virtio.md) | 用 dtbo overlay 声明后端（后来发现没必要） |
| [16](journal/16-virtio-backend-sequence.md) | 完整的后端初始化流程和坑 |
| [17](journal/17-backend-up-guest-silent.md) | 宿主准备就绪，客户机没动静 |
| [18](journal/18-guest-dt-in-bundle.md) | 客户机 DT 放在 `trustedvm.b02` 里（标签见 41，设备列表见 36） |
| [19](journal/19-loader-state-visibility.md) | 怎么看加载器日志；overlay 是个误导（设备列表见 36） |
| [20](journal/20-qcrosvm-is-the-reference.md) | `qcrosvm` 是厂商的参考启动器（被 41 更正） |
| [21](journal/21-qcrosvm-ioctl-table.md) | 它的 ioctl 表，两套 ABI |
| [22](journal/22-guest-never-touches-the-window.md) | 客户机在 virtio 初始化之前就挂了 |
| [23](journal/23-guest-dies-in-300ms.md) | 不到 0.5 秒就硬重置，怀疑 ABI 不一致 |
| [24](journal/24-no-boot-context-ioctl.md) | 不存在设置启动上下文的 ioctl |

### 客户机侧、二分排查、死路（25–39 跑的都是原厂 bundle，见 42）

| | |
|---|---|
| [25](journal/25-bundle-patched-and-resigned.md) | 改了 bundle 并重新签名：认证通过 |
| [26](journal/26-crash-fatal-and-device-nodes.md) | 去掉 `crash-fatal`；没有按标签区分的设备节点 |
| [27](journal/27-the-reset-leaves-no-host-trace.md) | 重置在宿主上不留痕迹 |
| [28](journal/28-vmkmsg-and-b02-entry-selection.md) | `/proc/vmkmsg` 存在但一直是空的 |
| [29](journal/29-stub-guest-exonerates-the-kernel.md) | stub 客户机：问题不在内核 |
| [30](journal/30-stub-timings-corrected.md) | 更正时序；签名后段的序号会变 |
| [31](journal/31-no-events-and-no-proof-of-execution.md) | 后端事件一次都没有 |
| [32](journal/32-where-started-running-comes-from.md) | `started running` 在哪打印；没有状态 ioctl |
| [33](journal/33-proxy-scheduling.md) | 怀疑 proxy 调度 |
| [34](journal/34-proxy-does-not-matter.md) | 对照实验：proxy 无关 |
| [35](journal/35-cpusys-vm-as-the-reference.md) | 用能跑的 VM 做参考配置 |
| [36](journal/36-backends-do-not-come-from-b02.md) | 设备列表来自 hyp/RM，不是 bundle |
| [37](journal/37-vcpu-run-blocks.md) | `VCPU_RUN` 会阻塞：vCPU 确实被调度进去了 |
| [38](journal/38-segments-are-placed-by-paddr.md) | 段按 `p_paddr` 放置 |
| [39](journal/39-mimicking-the-working-vm.md) | 完全模仿能跑的 VM，照样挂 |
| [40](journal/40-why-qcrosvm-cannot-run-here.md) | ~~为什么厂商启动器在这里跑不了~~（错了，见 41） |
| [41](journal/41-stock-launcher-same-failure.md) | ROM 与原厂一致；`cpusys_vm` 经 `qcrosvm` 能跑；厂商启动器跑 `trustedvm` 和我们的探测程序一样失败 |
| [42](journal/42-patched-bundles-never-loaded.md) | **改过的 bundle 从来没被加载过；`qtestsign` 被拒绝**，09、25–39 作废 |
| [43](journal/43-kernel-config-and-droidian.md) | 原厂内核配置缺什么、现有驱动、Droidian 的 android14-6.1 内核 |

### 无认证 VM 路线（44–58）

| | |
|---|---|
| [44](journal/44-crosvm-start-is-nomem.md) | crosvm 的 `VM_START` 失败（错误码 2，被误读成 `NOMEM`，见 51） |
| [45](journal/45-nomem-sweep.md) | 用户空间能调的参数全试了一遍，没变化 |
| [46](journal/46-nomem-is-a-known-kernel-bug.md) | ~~已知的 SM8650 VMID 内核 bug~~（在这里不成立，见 50） |
| [47](journal/47-kernel-flashed.md) | 刷了 Gunyah-fix 内核，没变化 |
| [48](journal/48-releases-do-not-contain-the-fix.md) | 发布的修复内核根本没编进打补丁的模块 |
| [49](journal/49-kvcalloc-module-on-stock.md) | 那个 fork 的 `modules/` 是 kvcalloc 修复，能在原厂内核上加载 |
| [50](journal/50-vmid-patch-is-a-noop.md) | VMID 补丁什么也没改；只有 `VM_START` 失败 |
| [51](journal/51-first-linux-guest-boots.md) | **Linux 客户机启动了**：客户机内存必须借出；错误码 2 = `NORESOURCE` |
| [52](journal/52-userspace-and-new-rom.md) | Alpine 用户空间；ROM 3.0.307.0 的 hyp/RM 没变 |
| [53](journal/53-arch-linux-arm-boots.md) | **Arch Linux ARM 启动**到 systemd 登录 |
| [54](journal/54-droidvm-fork-or-not.md) | DroidVM 已经在做这件事；内存上限来自碎片 |
| [55](journal/55-arch-in-droidvm-app.md) | 在 DroidVM 应用里跑 Arch：DHCP、ssh、pacman |
| [56](journal/56-wifi-landlock-syu.md) | Wi-Fi NAT、带 Landlock/RTC 的客户机内核、`pacman -Syu` |
| [57](journal/57-hugepage-reserve-1536m.md) | `gh-hugepage-reserve`：客户机 1.5 GiB |
| [58](journal/58-pseudo-unprotected-nokaslr.md) | pseudo-unprotected 模式加 `nokaslr` |

### 测试框架

| | |
|---|---|
| [04](journal/04-harness-pitfalls.md) | stub 的坑（写 stub 之前先看） |
| [05](journal/05-reproduction.md) | 复现探测环境 |

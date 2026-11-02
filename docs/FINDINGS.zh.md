[English](FINDINGS.md) | [中文](FINDINGS.zh.md)

# 结论（一页纸）

以下都在真机上验证过（houji / 23127PN0CC，HyperOS 3.0.7.0，KernelSU）。设备与原厂 `OS3.0.7.0.WNCCNXM` fastboot ROM 逐分区一致，只有 KernelSU 的 `init_boot` 不同（[journal/41](journal/41-stock-launcher-same-failure.md)）。数字、命令和原始日志都在实验日志里；早期日志后来被更正的地方，也附上了更正的链接。

## 1. 这套固件上拿不到 `/dev/kvm`

KVM 需要 EL2，而从 XBL 开始 EL2 就归高通的 Gunyah hypervisor 所有。两条把 EL2 交还出来的路都不通：

* Secure Launch（`slbounce` 的做法）没有实现：`hyp_a`/`xbl_a`/`tz_a`/`devcfg_a` 里都没有 `mssecapp` 和 `hyp_manager_launch`；配置字段 `0x221194` 没有任何读取方（[journal/01](journal/01-secure-launch-absent.md)）。
* Gunyah 的宿主 ABI（`hvc 0x6000+`）只管 VM 管理，没有"交还 EL2"的调用（[journal/03](journal/03-gunyah-hvc-abi.md)）。

AVF 还另有一个问题：这个版本**没有 Gunyah 后端**，用的是 crosvm-on-KVM，所以就算打开 `protected_vm.supported`，也只是让它卡在缺失的 `/dev/kvm` 上（[journal/06](journal/06-avf-is-kvm-only.md)）。

## 2. 厂商加载器这条路跑不了自定义代码

`/dev/qgunyah` 能驱动真正的 Gunyah VM（[journal/07](journal/07-gunyah-direct.md)），原厂系统就用它跑 `cpusys_vm`。但这条路上的每个 VM 都是 `secured` VM：它的 bundle 要经过小米签名根的认证。一旦真正加载了改过的 bundle，**`qtestsign` 重新签名会被拒绝**（`VM_AUTH_IMAGE` -22）。早先"被接受"的结果，是因为在 SELinux enforcing 下加载器悄悄回退到了原厂 bundle（[journal/42](journal/42-patched-bundles-never-loaded.md)，撤回了 [journal/09](journal/09-custom-system.md)）。

## 3. ……而且 `trustedvm` 在那里停住了

vCPU 被调度进去了，但**从来没有任何进展**：后端事件一次都没触发，没有报告任何异常，`GH_GET_QUEUE_INFO` 和 `ACK_DRIVER_OK` 都没实现，没有读 vCPU 状态/寄存器的 ioctl，`/proc/vmkmsg` 一直是空的，`mtdoops` 也没记录到 panic。重置来自 hyp/RM，不是宿主 panic（[journal/22](journal/22-guest-never-touches-the-window.md)、[journal/24](journal/24-no-boot-context-ioctl.md)、[journal/27](journal/27-the-reset-leaves-no-host-trace.md)、[journal/31](journal/31-no-events-and-no-proof-of-execution.md)、[journal/32](journal/32-where-started-running-comes-from.md)）。

客户机内核换成只有一条 `b .` 指令的 stub，结果也一样（[journal/29](journal/29-stub-guest-exonerates-the-kernel.md)，在 [journal/34](journal/34-proxy-does-not-matter.md) 中更正），所以问题不在厂商内核。

> 注意：第 4 节的实验在我们不知情的情况下跑的都是原厂 bundle（[journal/42](journal/42-patched-bundles-never-loaded.md)），对补丁本身说明不了什么。

## 4. 试过但没用的

客户机 DT 里加 `reg`/`interrupts`、去掉 `crash-fatal`、proxy / pinned / 不设 CPU 亲和性、`nokaslr`、把内核的 `p_paddr` 设成内存基址、只留一个条目的 DT 表、完整模仿那个能跑的 VM 的配置（[journal/25](journal/25-bundle-patched-and-resigned.md)、[journal/29](journal/29-stub-guest-exonerates-the-kernel.md)、[journal/38](journal/38-segments-are-placed-by-paddr.md)、[journal/39](journal/39-mimicking-the-working-vm.md)）。设备列表甚至不是来自 bundle，而是由 hyp/RM 决定的（[journal/36](journal/36-backends-do-not-come-from-b02.md)）。

## 5. 厂商启动器也一样失败

`/dev/qgunyah` 本身没问题：原厂系统开机就通过 `/system_ext/bin/qcrosvm --vm=cpusys_vm` 跑起 `cpusys_vm`（vCPU 线程阻塞在 `gh_vcpu_ioctl` 里）。`trustedvm` 的原厂按需启动（Trusted UI → `vendor.qti.qvirt.IVirtualizationService`）失败得更早，因为这个 ROM 上没有它的系统盘 `/data/misc/vm-system/system.img`。手动用原样的 bundle、提供两块盘运行 `qcrosvm`，会走到 `started running`，然后手机重置，**和我们的探测程序表现完全一样**（[journal/41](journal/41-stock-launcher-same-failure.md)，更正了 [journal/40](journal/40-why-qcrosvm-cannot-run-here.md)）。

## 6. `/dev/gunyah` 上的无认证 VM 可以用

只有 **`/dev/gunyah` 上的无认证 VM**（主线 ABI、crosvm、自己的内核）能绕开签名，而且它在**原厂**内核上就能跑：

* **错误码读错了。** `VM_START` 返回的 `Error: 2`，在内核的 `rsc_mgr.c` 和 RM 源码里都是 `NORESOURCE`，而不是从 journal/45 起一直误读的 `NOMEM`（[journal/51](journal/51-first-linux-guest-boots.md)）。
* **VMID 内核 bug 在这里不成立。** 跟踪显示每次 `qcom_scm_assign_mem` 都成功；厂商的 `gunyah_qcom` 本来就会映射大于 0x3F 的客户机 VMID，而宿主自己的 VMID 是 3（= HLOS），所以那个 38 行的补丁什么也没改。网上发布的"修复"内核甚至根本没编进这个补丁（[journal/48](journal/48-releases-do-not-contain-the-fix.md)、[journal/50](journal/50-vmid-patch-is-a-noop.md)）。
* **真正的原因**：资源管理器（RM，开源的 `quic/gunyah-resource-manager`，字符串和 `hypvm*.mbn` 一致）要求客户机的主内存块是**借出（lend）**。`GH_VM_SET_USER_MEM_REGION` 走的是**共享（share）**；`process_image_memparcel()` 拒绝共享，而 `VM_START` 把这类失败一律报成 `NORESOURCE`。DroidVM 的 crosvm 用借出（`--protected-vm-without-firmware`）→ `VM_START` 成功，Linux 内核启动起来（[journal/51](journal/51-first-linux-guest-boots.md)）。
* **内存上限来自碎片。** 开机一段时间后，超过约 256 MiB 的借出就会失败（被切成几万段、每段约 16 KiB）。`gh-hugepage-reserve` 从开机早期就预留 2 MiB 大页；有了它，客户机能拿到 1.5 GiB（[journal/54](journal/54-droidvm-fork-or-not.md)、[journal/57](journal/57-hugepage-reserve-1536m.md)）。
* **结果**：带 systemd、能联网、能 `pacman -Syu` 的 Arch Linux ARM，由 DroidVM 应用管理（[journal/53](journal/53-arch-linux-arm-boots.md)–[58](journal/58-pseudo-unprotected-nokaslr.md)）。

所以在原厂固件上：没有 `/dev/kvm`，secured VM 里跑不了自定义代码；但用我们自己内核的无认证 Gunyah VM 可以用，除了 root 之外不用刷任何东西。

## 接下来

当前状态：[STATE](STATE.zh.md)。三条路线（原生 Linux、真 chroot、Gunyah VM）的对比见 [OPTIONS](OPTIONS.zh.md)；剩下的工作见 [ROADMAP](ROADMAP.zh.md)。

## 固件更新时的检查项

```bash
adb shell su -c 'strings /dev/block/by-name/hyp_a | grep -E "mssecapp|hyp_manager_launch"'
adb shell su -c 'strings /apex/com.android.virt/bin/virtualizationservice | grep -c gunyah'
adb shell su -c 'ls /sys/kernel/load_guestvm_*'          # 更新的 ABI
```

只要有任何一项命中，`/dev/kvm` 和厂商加载器这两条路就值得重新看；本仓库里的测试框架、工具和 ABI 笔记就是起点。

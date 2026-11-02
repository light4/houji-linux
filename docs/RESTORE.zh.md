[English](RESTORE.md) | [中文](RESTORE.zh.md)

# 恢复 / 回退

用测试框架做实验期间，只刷过 `boot_a`；装 dtbo overlay 的那次只动过 `dtbo_a`。两个分区都有与原厂逐字节一致的备份。

```bash
fastboot flash dtbo_a ~/Downloads/houji_backup/dtbo_a.img
fastboot flash boot   ~/playground/houji-linux/baseline/boot_a_OS3.0.7.0_orig.img
```

| 分区 | 原厂 sha256 |
|---|---|
| `dtbo_a` | `6264b8e66e2e7720e9f593f93f9b7390ccbe5675c04d15c0c13abf1ff327456b` |
| `boot_a` | `c88537b854911c142a920989865b663a5b7e0b3faab7025a7858b7a7adddafa9` |

## 内核 / root

当前状态（`journal/48` 之后）：**原厂内核**（`boot_a` 就是原厂镜像，sha256 `c88537b8…`），`init_boot` 里是 **KernelSU v3.3.1 LKM**，管理器 v3.3.1。备份在 `~/Downloads/houji_backup/`：

| 文件 | 内容 |
|---|---|
| `boot_a_pre_kpatch_20261102.img` | 原厂 `boot_a`（与 ROM 里的 `boot.img` 一致） |
| `init_boot_ksu_v3.3.1_lkm.img` | 用 `ksud boot-patch` 打过 KernelSU 3.3.1 LKM 的 ROM `init_boot` |
| `boot_a_kernel_v6.1.138_GunyahFix17.img` | `journal/47` 里试过的自定义 Gunyah-fix 内核 |

```bash
fastboot flash boot      ~/Downloads/houji_backup/boot_a_pre_kpatch_20261102.img
fastboot flash init_boot ~/Downloads/houji_backup/init_boot_ksu_v3.3.1_lkm.img
```

从头重新生成 KernelSU 的 `init_boot`（在手机上以 `shell` 身份运行，用 v3.3.1 的发布文件）：

```bash
ksud-aarch64-linux-android boot-patch -b init_boot.img \
  -m lkm-aarch64-android14-6.1_kernelsu.ko -i ksuinit-aarch64 \
  --kmi android14-6.1 --partition init_boot -o . --out-name init_boot_ksu.img
```

注意：GunyahFix 内核自带的 KernelSU（驱动版本 32644）和 3.3.1 管理器不匹配；刷那些内核时，要用版本对应的 CI 管理器，或者用 Magisk。

## KernelSU 模块（journal/57）

* `gh-hugepage-reserve` v12：开机时预留 2 MiB 大页池，`pool_want=1024`（2 GiB）。停用方法：`touch /data/adb/modules/gh-hugepage-reserve/disable`（或者用 KernelSU 安全模式 / 模块页），然后重启。某次开机没走完时，它会自动停用自己。
* `gunyah-kvcalloc-fix`：已禁用（被 DroidVM 的运行时模块取代）。
* DroidVM 的运行时模块（`*_gki_6.1`）只随 DroidVM 后台进程自动加载。

## 客户机 / DroidVM

VM 完全在用户空间里，删掉它不会动到任何刷过的分区。

| 内容 | 位置 |
|---|---|
| 客户机内核 | `/data/local/tmp/guest.Image`（编译：`guest/build_guest.sh`） |
| 客户机磁盘 | `/data/local/tmp/alarm.img`（制作：`guest/mk_alarm.sh`） |
| VM / 网络配置 | `/data/data/cn.classfun.droidvm/files/{vms,networks,disks}.json` |
| 应用 | `cn.classfun.droidvm`：卸载会同时删掉配置和运行时模块 |

备份 `alarm.img` 时先停掉 VM（`adb pull`），它是唯一有状态的部分。

## 固件 bundle

手机上是**原厂** `trustedvm` bundle（没有装任何改过的）。如果实验留下了什么，用下面的命令删掉：

```bash
adb shell su -c 'mount -o rw,remount /vendor/firmware_mnt; rm /vendor/firmware_mnt/image/trustedvm.*'
```

之后加载器会回退到 `/product/vm-system/trustedvm/boot/trustedvm`。

当前设备状态：**原厂 `dtbo_a`、原厂 `trustedvm` bundle**，`boot_a` 未改动。（`/vendor/firmware_mnt` 重启后还在，里面的文件要手动删。）

## 如果手机卡死

坏的 stub 会在 bootloader 看门狗生效前卡住；bootloader 看门狗大约 2 分钟后会把手机带回 fastboot。否则长按电源键约 15 秒，再按音量下 + 电源进 fastboot。

## 完整分区备份

方法见 `journal/13`（AVF-through-vendor-path），上面两个原厂镜像就是用这个方法备份的。

[English](README.md) | [中文](README.zh.md)

# houji-linux

在小米 14（houji，SM8650，HyperOS 3.0.7.0）上运行 Arch Linux / NixOS，不用 proot。

## 现状

| 路线 | 状态 |
|---|---|
| `/dev/kvm` | 不通：EL2 归 Gunyah hypervisor 所有 |
| 厂商加载器 `/dev/qgunyah` 起 VM | 不通：只能启动小米签名的 VM |
| 经 `/dev/gunyah` 的无认证 VM（DroidVM） | **原厂内核上可用**：Arch Linux ARM，4 vCPU、1.4 GiB 内存，能联网、能 `pacman -Syu`（[STATE](docs/STATE.zh.md)） |
| 原生 Linux 作为主系统 | 未开始：内核缺 `DEVTMPFS`/`FHANDLE`/命名空间 |
| 在 Android 内核上 chroot | 可行，未搭建；不是独立系统 |

## 文档

* [STATE](docs/STATE.zh.md)：当前配置和使用方法
* [OPTIONS](docs/OPTIONS.zh.md)：三条路线的对比和优缺点
* [ROADMAP](docs/ROADMAP.zh.md)：剩下的工作
* [FINDINGS](docs/FINDINGS.zh.md)：虚拟化探索得出的结论
* [RESTORE](docs/RESTORE.zh.md)：回退方法和原厂哈希
* [索引](docs/README.zh.md)：参考资料和实验日志

参考资料（`docs/reference/`）和实验日志（`docs/journal/`）只有英文版。

## 目录结构

```
docs/      结论、路线对比、参考资料、实验日志
src/       独立运行的探测程序（SMCCC stub、/dev/qgunyah 驱动）
guest/     客户机内核编译、根文件系统镜像、启动/ssh/配置脚本
tools/     bundle 工具、原厂启动器运行脚本、qvirt 服务客户端
patch.py   boot.img stub 测试框架（配合 run.sh）
evidence/  原始探测输出
```

刷过的每个分区都有原厂备份；动手前先看 [RESTORE](docs/RESTORE.zh.md)。

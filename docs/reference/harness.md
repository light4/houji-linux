# Harness: toolchain, logging, pitfalls

## Toolchain

```bash
/opt/homebrew/opt/llvm/bin/clang --target=aarch64-linux-gnu -ffreestanding \
    -nostdlib -O1 -c tool.c -o tool.o
/opt/homebrew/bin/ld.lld -m aarch64elf -static -e _start tool.o -o tool
adb push tool /data/local/tmp/tool && adb shell su -c 'chmod 755 /data/local/tmp/tool'
```

Programs are freestanding: raw syscalls via inline `svc`, no libc.

## Logging across a reset

The VM crash resets the phone in under half a second. A shell `>` redirect and a
one-shot `dmesg` both lose everything. Working pattern:

* the tool opens `/data/local/tmp/o.txt` itself and `fsync`s after every write —
  **a shell redirect captures only stdout and is nearly empty, so always read
  `o.txt`**;
* kernel log: `dmesg -w > /data/local/tmp/d.txt &` plus
  `while true; do sync; done &` — the `sync` loop is what makes it survive;
* loader logs are `dev_dbg`:

```bash
mount -t debugfs none /sys/kernel/debug
echo 'file *gunyah* +p' > /sys/kernel/debug/dynamic_debug/control
```

Re-enable after **every** boot.

## Persisting a host crash

`mtdoops` is enabled (`printk.always_kmsg_dump=1`, oops on
`/dev/block/by-name/oops`). After a panic/oops the tail of the kernel log can be
read back with:

```bash
adb shell su -c 'dd if=/dev/block/by-name/oops bs=1M count=2 | strings -n 6 | less'
```

This is better than racing `dmesg`, and it works for any host-side crash.

## Kernel-side probes (boot stubs)

`patch.py` hijacks the arm64 `Image` entry and splices a stub; results are
written over the `Linux version …` banner and read back with `dmesg | head -3`.

Pitfalls — each cost a cycle (`journal/04` is the full list): MMU-off unaligned
access, BTI landing pads, `x0` = DTB on entry, rodata write alignment.

## Device inventory used for RE

* device: houji / 23127PN0CC, KernelSU root, Hypervisor 3.0.7.0
* `/tmp` on the host: `rom_kernel.img`, `qc.dis` (qcrosvm disassembly), `tv/`
  (extracted vendor bundle), overlay sources `a.dts`/`c.dts`, `dtbo_*.img`,
  `g*.dtbo` (extracted guest DTs), `zget*.py` (factory-image range extractor)
* on device `/data/local/tmp`: `gh*` probe binaries, `disk1.img`, `qcrosvm`,
  `pvmfw.img`, bundle copies

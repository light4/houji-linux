# Reproduction

## Toolchain

macOS is enough — no cross gcc needed:

```bash
brew install llvm        # llvm-mc, llvm-objcopy
```

`patch.py` assembles `src/*.S` with `llvm-mc -triple=aarch64`, extracts `.text`
with `llvm-objcopy -O binary`, and splices it into a copy of the dump.

## Baseline dump

```bash
adb shell su -c 'dd if=/dev/block/by-name/boot_a of=/data/local/tmp/boot_a.img bs=1M'
adb pull /data/local/tmp/boot_a.img baseline/
sha256sum baseline/boot_a.img   # expect c88537b8… for OS 3.0.7.0
```

Keep it. Every experiment is one `fastboot flash boot baseline/...` away from
a working phone.

## Running a probe

```bash
python3 patch.py baseline/boot_a.img build/x.img --stub probe_stub.S
./run.sh build/x.img
adb shell su -c 'dmesg | head -3'
```

`run.sh` classifies as `BOOTED`, `REBOOTED-TO-FASTBOOT` (stub crashed, fault
handler reset) or `DEAD` (power-cycle needed). All three are recoverable.

## Writing a new probe

Copy `src/stub_nop2.S` for the minimal skeleton and keep the prologue,
`_save`/`_restore`, `_exit` slot and `_exc` handler. Replace only the body
between `_loop:` and `_after_probe:`, keep every record a multiple of 4 bytes,
and make the fallback path take `_back_to_el1` so a failing probe still boots
the phone.

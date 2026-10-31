#!/usr/bin/env python3
"""Patch a houji boot.img with the EL1->EL2 probe stub.

Only touches a copy; nothing is flashed. Boot with: fastboot boot patched.img
"""
import argparse
import struct
import subprocess
import sys
from pathlib import Path

ENTRY = 0x1000          # arm64 Image start inside boot.img
IMAGE_SIZE_OFF = 0x1000 + 0x10
KERNEL_SIZE_FIELD = 8   # boot header v4, bytes from ENTRY
BANNER = 0x12B8660      # live "Linux version " in Image (.rodata, 2nd copy)
CMDLINE_KVM = 0x1536557  # "kvm-arm.mode=protected" in Image
STUB_AT = 0x1024000     # inside the Image, in a zero run (~16 MB, below any copy limit)
CODE_OFF = 0x800        # code offset inside stub blob (after vector table)

HERE = Path(__file__).resolve().parent


def build_stub(src: str = "probe_stub.S", chunk: int | None = None, start: int | None = None, count: int | None = None) -> bytes:
    obj = HERE / "build" / "stub.o"
    bin_ = HERE / "build" / "stub.bin"
    obj.parent.mkdir(exist_ok=True)
    subprocess.run(
        [
            "/opt/homebrew/opt/llvm/bin/llvm-mc",
            "-triple=aarch64",
            "-mattr=+v8.2a",
            f"-defsym=STUB_AT={STUB_AT}",
            f"-defsym=BANNER={BANNER}",
            f"-defsym=ENTRY={ENTRY}",
            *([f"-defsym=CHUNK={chunk}"] if chunk is not None else []),
            *([f"-defsym=START={start}"] if start is not None else []),
            *([f"-defsym=COUNT={count}"] if count is not None else []),
            "--filetype=obj",
            str(HERE / "src" / src),
            "-o",
            str(obj),
        ],
        check=True,
    )
    subprocess.run(
        [
            "/opt/homebrew/opt/llvm/bin/llvm-objcopy",
            "-O",
            "binary",
            "--only-section=.text",
            str(obj),
            str(bin_),
        ],
        check=True,
    )
    return bin_.read_bytes()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("src", type=Path)
    ap.add_argument("dst", type=Path)
    ap.add_argument("--stub", default="probe_stub.S")
    ap.add_argument("--chunk", type=int)
    ap.add_argument("--start", type=int)
    ap.add_argument("--count", type=int)
    args = ap.parse_args()

    d = bytearray(args.src.read_bytes())
    stub = build_stub(args.stub, args.chunk, args.start, args.count)
    print(f"stub size {len(stub):#x}")

    assert d[ENTRY + 0x38 : ENTRY + 0x3C] == b"ARM\x64", "not an arm64 Image"
    image_size = struct.unpack_from("<Q", d, IMAGE_SIZE_OFF)[0]
    print(f"image_size {image_size:#x}")
    assert STUB_AT + len(stub) <= ENTRY + image_size, "stub does not fit"
    assert set(d[STUB_AT:STUB_AT + len(stub)]) == {0}, "stub area is not all zero (would clobber data)"

    orig_w0 = bytes(d[ENTRY : ENTRY + 4])
    orig_ksize = struct.unpack_from("<I", d, KERNEL_SIZE_FIELD)[0]
    orig_isize = image_size

    # 1. hijack entry with a branch to stub code
    target = STUB_AT + CODE_OFF
    delta = target - ENTRY
    assert delta % 4 == 0 and -(1 << 27) <= delta < (1 << 27), "branch out of range"
    d[ENTRY : ENTRY + 4] = struct.pack("<I", 0x14000000 | ((delta >> 2) & 0x3FFFFFF))

    # 2. place stub, patch its replay slot with the hijacked instruction
    end = STUB_AT + len(stub)
    assert end <= len(d), "stub does not fit in the file"
    d[STUB_AT:end] = stub
    raw = stub.find(struct.pack("<I", 0xB0000000))
    while raw >= 0 and (STUB_AT + raw) % 4:
        raw = stub.find(struct.pack("<I", 0xB0000000), raw + 1)
    assert raw >= 0, "aligned exit slot marker missing"
    at = STUB_AT + raw
    d[at : at + 4] = struct.pack("<I", 0x14000000 | (((ENTRY + 4 - at) >> 2) & 0x3FFFFFF))

    # 3. grow both size fields so the bootloader copies the stub too
    struct.pack_into("<I", d, KERNEL_SIZE_FIELD, max(orig_ksize, end - ENTRY))
    struct.pack_into("<Q", d, IMAGE_SIZE_OFF, max(orig_isize, (end - ENTRY + 0xF) & ~0xF))

    # 4. ask for nVHE instead of pKVM
    assert d[CMDLINE_KVM:CMDLINE_KVM + 22] == b"kvm-arm.mode=protected"
    d[CMDLINE_KVM + 13 : CMDLINE_KVM + 22] = b"nvhe     "

    args.dst.write_bytes(d)
    print(f"wrote {args.dst} ({len(d)} bytes), kernel_size={end - ENTRY:#x}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

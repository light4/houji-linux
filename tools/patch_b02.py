#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# ///
"""Replace one entry of the guest-DT template (b02, a dtbo table) with a new blob.

  patch_b02.py --b02 trustedvm.b02 --index 13 --blob patched.dtbo --out new.b02

Edit the extracted entry with `dtc -I dtb -O dts` / `dtc -I dts -O dtb` first.
The table is rebuilt with 4-byte aligned, tightly packed entries and a fixed
total size, which is what the loader expects.
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

MAGIC = 0xD7B7AB1E


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--b02", type=Path, required=True)
    ap.add_argument("--index", type=int, required=True)
    ap.add_argument("--blob", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    a = ap.parse_args()

    d = bytearray(a.b02.read_bytes())
    magic, _total, hdr, esz, cnt, eoff, psz, ver = struct.unpack_from(">8I", d, 0)
    if magic != MAGIC:
        raise SystemExit("not a dtbo table")

    entries = []
    for i in range(cnt):
        e = bytearray(d[eoff + i * esz : eoff + (i + 1) * esz])
        size, off = struct.unpack_from(">2I", e, 0)
        entries.append([e, bytes(d[off : off + size])])
    entries[a.index][1] = a.blob.read_bytes()

    out = bytearray(struct.pack(">8I", magic, 0, hdr, esz, cnt, eoff, psz, ver))
    cur = eoff + cnt * esz
    offs = []
    for _, b in entries:
        cur = (cur + 3) // 4 * 4
        offs.append(cur)
        cur += len(b)
    for i, (e, b) in enumerate(entries):
        struct.pack_into(">2I", e, 0, len(b), offs[i])
        out += e
    for i, (e, b) in enumerate(entries):
        out += b"\x00" * (offs[i] - len(out))
        out += b
    struct.pack_into(">I", out, 4, len(out))
    a.out.write_bytes(bytes(out))
    print(f"{len(d)} -> {len(out)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

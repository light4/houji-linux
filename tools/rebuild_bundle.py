#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["cryptography"]
# ///
"""Re-assemble, re-sign and split the trustedvm MBN bundle around a patched b02.

Usage:
  rebuild_bundle.py --src tv/ --patch-b02 new.b02 --out out/ --qtestsign /path/to/qtestsign

`--src` must hold trustedvm.mdt + trustedvm.b00..b04 (extracted from the ROM).
The output set authenticates on device (verified: `VM: 45 status 2 complete`).
"""
from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from pathlib import Path

EHDR = 0xD4  # ELF header segment size in this bundle


def phdrs(img: bytes) -> list[tuple[int, int, int, int]]:
    phoff = struct.unpack_from("<I", img, 0x1C)[0]
    num = struct.unpack_from("<H", img, 0x2C)[0]
    return [struct.unpack_from("<8I", img, phoff + i * 32) for i in range(num)]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", type=Path, required=True)
    ap.add_argument("--patch-b02", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--qtestsign", type=Path, required=True)
    a = ap.parse_args()

    mdt = (a.src / "trustedvm.mdt").read_bytes()
    segs = phdrs(mdt)
    if len(segs) != 5:
        sys.exit(f"expected 5 segments, got {len(segs)}")

    blobs = [mdt[:EHDR]]
    blobs += [(a.src / f"trustedvm.b{i:02d}").read_bytes() for i in range(1, len(segs))]
    blobs[2] = a.patch_b02.read_bytes()  # segment 2 is the guest DT template

    size = max(s[1] + s[4] for s in segs)
    img = bytearray(size)
    phoff = struct.unpack_from("<I", mdt, 0x1C)[0]
    for i, (_, off, _, _, _fsz, _msz, _fl, _al) in enumerate(segs):
        data = blobs[i]
        img[off : off + len(data)] = data
        struct.pack_into("<II", img, phoff + i * 32 + 16, len(data), len(data))

    combined = a.out / "combined.bin"
    signed = a.out / "signed.bin"
    a.out.mkdir(parents=True, exist_ok=True)
    combined.write_bytes(img)
    subprocess.run(
        [sys.executable, str(a.qtestsign / "qtestsign.py"), "-v", "6", "-o", str(signed),
         "multi-image", str(combined)],
        check=True, stdout=subprocess.DEVNULL,
    )

    # split back by the *new* phdrs: non-PT_LOAD segments go into the mdt
    data = signed.read_bytes()
    new = phdrs(data)
    mdt_end = max(off + fsz for t, off, _, _, fsz, _m, _f, _a in new if t != 1)
    (a.out / "trustedvm.mdt").write_bytes(data[:mdt_end])
    for i, (t, off, _, _, fsz, _m, _f, _a) in enumerate(new):
        (a.out / f"trustedvm.b{i:02d}").write_bytes(data[off : off + fsz])
    print("wrote", sorted(p.name for p in a.out.glob("trustedvm.*")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

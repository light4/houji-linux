#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# ///
"""Install a trustedvm bundle (or the stock one), reboot, start it with the stock
qcrosvm and collect qcrosvm output + kernel log across a possible reset.

  run_tvm.py --bundle /tmp/out_s          # patched bundle
  run_tvm.py --stock                      # remove overrides, use ROM bundle
"""
from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

FILES = ["trustedvm.mdt"] + [f"trustedvm.b{i:02d}" for i in range(5)]
T = "/data/local/tmp"


def adb(dev: str, *args: str, timeout: int = 120, check: bool = False) -> subprocess.CompletedProcess:
    return subprocess.run(["adb", "-s", dev, *args], capture_output=True, text=True, errors="replace",
                          timeout=timeout, check=check)


def su(dev: str, cmd: str, timeout: int = 120) -> str:
    return adb(dev, "shell", f"su -c '{cmd}'", timeout=timeout).stdout


def wait(dev: str, timeout: int = 180) -> None:
    t0 = time.time()
    while time.time() - t0 < timeout:
        out = subprocess.run(["adb", "devices"], capture_output=True, text=True, timeout=10).stdout
        if f"{dev}\tdevice" in out and su(dev, "getprop sys.boot_completed").strip() == "1":
            return
        time.sleep(3)
    sys.exit("device did not come back")


def reboot(dev: str) -> None:
    adb(dev, "reboot")
    time.sleep(8)
    wait(dev)
    time.sleep(10)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dev", default="c116d21f")
    ap.add_argument("--bundle", type=Path)
    ap.add_argument("--stock", action="store_true")
    ap.add_argument("--seconds", type=int, default=25)
    ap.add_argument("--out", type=Path, default=Path("/tmp/tvm_run"))
    ap.add_argument("--extra", default="", help="extra qcrosvm args")
    ap.add_argument("--enforcing", action="store_true",
                    help="keep SELinux enforcing (then firmware_mnt loads fail with -13 and the "
                         "loader silently falls back to the stock bundle in /product)")
    a = ap.parse_args()

    dev = a.dev
    wait(dev)
    rm = "mount -o rw,remount /vendor/firmware_mnt; rm -f /vendor/firmware_mnt/image/trustedvm.*"
    if a.bundle:
        for f in FILES:
            adb(dev, "push", str(a.bundle / f), f"{T}/{f}", check=True)
        su(dev, rm + "; " + "; ".join(f"cp {T}/{f} /vendor/firmware_mnt/image/{f}" for f in FILES) + "; sync")
    elif a.stock:
        su(dev, rm + "; sync")
    reboot(dev)

    su(dev, f"cp /mnt/product/vm-persist/trustedvm/persist.img {T}/p.img; rm -f {T}/q.txt {T}/d.txt {T}/h45.txt")
    run = (f"cd {T}; nohup sh -c \"dmesg -w > d.txt 2>&1 & (while true; do cat /dev/hvc45 >> h45.txt 2>/dev/null; sleep 0.02; done) & while true; do sync; sleep 0.2; done & "
           f"timeout {a.seconds} /system_ext/bin/qcrosvm --vm=trustedvm "
           f"--disk={T}/disk1.img,label=10 --disk={T}/p.img,label=11,rw=true {a.extra} "
           f"--log=type=term > q.txt 2>&1; echo RC=\\$? >> q.txt\" >/dev/null 2>&1 &")
    su(dev, "mount -t debugfs none /sys/kernel/debug 2>/dev/null; "
            "echo \"file *gunyah* +p\" > /sys/kernel/debug/dynamic_debug/control")
    if not a.enforcing:
        su(dev, "setenforce 0")
    up0 = su(dev, "cat /proc/uptime").split()[0]
    su(dev, run)
    alive = []
    for t in (2, 5, 10, a.seconds + 3):
        time.sleep(t - (alive[-1][0] if alive else 0))
        out = subprocess.run(["adb", "devices"], capture_output=True, text=True).stdout
        alive.append((t, f"{dev}\tdevice" in out))
    wait(dev)
    up1 = su(dev, "cat /proc/uptime").split()[0]
    reset = float(up1) < float(up0)
    a.out.mkdir(parents=True, exist_ok=True)
    (a.out / "q.txt").write_text(su(dev, f"cat {T}/q.txt"))
    (a.out / "h45.txt").write_text(su(dev, f"cat {T}/h45.txt"))
    (a.out / "d.txt").write_text(su(dev, f"cat {T}/d.txt", timeout=300))
    if not reset:
        su(dev, "setenforce 1")
    d = (a.out / "d.txt").read_text(errors="replace")
    src = "PATCHED (firmware_mnt)" if "trustedvm.mdt failed with error -13" not in d else "STOCK (fallback to /product)"
    print("bundle actually loaded:", src)
    print("alive:", alive, "reset:", reset)
    h = (a.out / "h45.txt").read_text(errors="replace")
    print("guest console (hvc45) bytes:", len(h))
    for line in h.splitlines()[-40:]:
        print("  g|", line[:200])
    q = (a.out / "q.txt").read_text().splitlines()
    print("qcrosvm lines:", len(q))
    for line in [l for l in q if "unsupported read" not in l][:25]:
        print("  q|", line)
    for line in (a.out / "d.txt").read_text().splitlines():
        if any(k in line for k in ("VM:45", "VM: 45", "vmid=45", "gh_virtio_backend: cmd", "panic", "Failed to")):
            if "kiwi" not in line:
                print("  d|", line[:180])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

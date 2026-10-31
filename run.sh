#!/bin/bash
# usage: ./run.sh <image> — flash, boot, classify: BOOTED / REBOOTED (crashed) / DEAD
set -u
img=$1
adb devices | grep -q c116d21f && { adb reboot bootloader; sleep 8; }
for i in $(seq 1 25); do [ -n "$(fastboot devices)" ] && break; sleep 3; done
[ -z "$(fastboot devices)" ] && { echo "no fastboot"; exit 1; }
timeout 120 fastboot flash boot "$img" 2>&1 | tail -1 || exit 1
timeout 20 fastboot reboot >/dev/null 2>&1
for i in $(seq 1 16); do
  sleep 5
  if adb devices | grep -q c116d21f; then echo "BOOTED"; exit 0; fi
done
for i in $(seq 1 24); do
  sleep 5
  if [ -n "$(fastboot devices)" ]; then echo "REBOOTED-TO-FASTBOOT (crashed)"; exit 2; fi
  if adb devices | grep -q c116d21f; then echo "BOOTED-LATE"; exit 0; fi
done
echo "DEAD"

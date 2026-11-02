cd /data/local/tmp; setenforce 0
LD_LIBRARY_PATH=/data/local/tmp/dvlib timeout ${T:-90} ./crosvm-dv run --disable-sandbox --hypervisor gunyah \
  --mem ${MEM:-768} --cpus ${CPUS:-2} --swiotlb 64 --protected-vm-without-firmware \
  --serial type=stdout,hardware=serial,earlycon=true,console=true,stdin=true \
  ${NOBLK:---block path=/data/local/tmp/alarm.img} \
  -p "earlycon console=ttyS0 root=/dev/vda rw rootwait panic=-1 $EXTRA" guest.Image > arch.txt 2>&1 < /dev/null
echo rc=$?; setenforce 1

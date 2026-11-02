# usage: dvcfg.sh 'sed-expr'  -- stop DroidVM + daemon, edit vms.json, show keys
am force-stop cn.classfun.droidvm
for p in /proc/[0-9]*; do
  case "$(readlink $p/exe 2>/dev/null)" in *app_process*) ;; *) continue;; esac
  tr '\0' ' ' < $p/cmdline 2>/dev/null | grep -q 'cn.classfun.droidvm.daemon.Daemon' && kill ${p#/proc/}
done
sleep 2
F=/data/data/cn.classfun.droidvm/files/vms.json
[ -n "$1" ] && sed -i "$1" $F
grep -E '"(memory_mb|cpu_count|protected_vm|swiotlb_mb|prepare_lend_mthp)"' $F

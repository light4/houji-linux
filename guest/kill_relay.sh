for p in /proc/[0-9]*; do
  e=$(readlink $p/exe 2>/dev/null); case "$e" in *toybox*) ;; *) continue;; esac
  c=$(tr '\0' ' ' < $p/cmdline 2>/dev/null)
  case "$c" in *"-p $1 -L"*) kill ${p#/proc/} && echo "killed ${p#/proc/}: $c";; esac
done

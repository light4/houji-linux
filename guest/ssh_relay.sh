#!/system/bin/sh
# Run on the phone as root: expose the guest's sshd on 127.0.0.1:2222,
# then on the host: adb forward tcp:2222 tcp:2222 && ssh -p 2222 debian@127.0.0.1
GUEST=${1:-192.168.134.128}
nohup toybox nc -s 127.0.0.1 -p 2222 -L toybox nc "$GUEST" 22 >/dev/null 2>&1 &

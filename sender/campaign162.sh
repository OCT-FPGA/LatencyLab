#!/bin/bash
# usage: sudo bash campaign162.sh "FT_s2 FT_s3 FT_s4 FT_s5" /users/Pavani/fiveTuple_20k.pcap
set -e
SESSIONS=$1; PCAP=$2
MAC=$(ip link show enp134s0np0 | awk '/ether/{print $2}')
wait_token(){  # blocks until expected token arrives
  while true; do
    T=$(nc -l 9999)
    [ "$T" = "ABORT" ] && { echo "ABORT from pc163"; pkill tcpdump 2>/dev/null; exit 1; }
    [ "$T" = "$1" ] && return 0
    echo "  (ignoring token: $T)"
  done
}
for NAME in $SESSIONS; do
  echo "########## $NAME : waiting for READY ##########"
  wait_token "READY_$NAME"
  pkill tcpdump 2>/dev/null || true; sleep 1
  rm -f /users/Pavani/refl_$NAME.pcap
  tcpdump -i enp134s0np0 -j adapter_unsynced --time-stamp-precision=nano \
    -w /users/Pavani/refl_$NAME.pcap "ether dst $MAC" &
  sleep 3
  for i in $(seq 1 10); do tcpreplay -i enp134s0np0 --pps 500 $PCAP; sleep 10; done
  echo "sends done, waiting for DONE token..."
  wait_token "DONE_$NAME"
  sleep 3; pkill tcpdump; sleep 1
  cd /users/Pavani && python3 cx5_perid_all.py refl_$NAME.pcap
  mv silicon_perid_all.csv silicon_$NAME.csv
  echo "########## $NAME done ##########"
done
echo "########## SENDER COMPLETE ##########"

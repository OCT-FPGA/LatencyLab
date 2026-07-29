#!/bin/bash
# usage: sudo bash campaign163.sh "FT_s2 FT_s3 FT_s4 FT_s5" /users/Pavani/au280_fiveTuple
set -e
SESSIONS=$1; TDIR=$2
PEER=198.22.255.173   # pc162 control net
send_token(){
  for i in $(seq 1 30); do
    echo "$1" | nc -q 1 $PEER 9999 2>/dev/null && return 0
    sleep 2
  done
  echo "token send failed after retries: $1"; exit 1
}
DB=/users/Pavani/dpdk-20.11/usertools/dpdk-devbind.py
P=/sys/devices/pci0000:3a/0000:3a:00.0/0000:3b:00.0/resource2
PC=/users/Pavani/opennic/opennic-scripts/pcimem/pcimem
fatal(){ echo "FATAL: $1"; send_token "ABORT" || true; exit 1; }

for NAME in $SESSIONS; do
  echo "########## $NAME ##########"
  rm -rf /users/Pavani/p4lat/$NAME
  pkill -9 p4lat_r2 2>/dev/null || true; sleep 2
  rm -f /dev/hugepages/rtemap_* 2>/dev/null || true
  echo "[stage] unbind + rescan"
  timeout 30 $DB -u 3b:00.0 3b:00.1 2>/dev/null || true
  echo 1 > /sys/bus/pci/devices/0000:3b:00.0/remove
  echo 1 > /sys/bus/pci/devices/0000:3b:00.1/remove
  sleep 2; echo 1 > /sys/bus/pci/rescan; sleep 3
  echo "[stage] onic bind"
  $DB -b onic 3b:00.0 3b:00.1 2>/dev/null || true
  sleep 2
  ip link show | grep -q enp59s0f0 || fatal "netdevs missing"
  if [ "$TDIR" != "none" ]; then
    echo "[stage] tables"
    ( cd $TDIR/drivers/install && ./driver > /tmp/tables.log 2>&1 ) || { tail -5 /tmp/tables.log; fatal "tables"; }
  else
    echo "[stage] tables skipped (null build)"
  fi
  echo "" > /sys/bus/pci/devices/0000:3b:00.0/reset_method
  $DB -b vfio-pci 3b:00.0 3b:00.1
  echo "[stage] app launch"
  cd /users/Pavani/p4lat && mkdir -p $NAME
  nohup stdbuf -oL ./p4lat_r2 -l 2,4,6 -n 4 -a 3b:00.0 -a 3b:00.1 -a 86:00.0 \
    -d librte_net_qdma.so -d librte_common_mlx5.so -d librte_net_mlx5.so \
    -- --prefix $NAME/f --runs 10 --dst-mac b8:83:03:7a:e0:b8 > $NAME/console.log 2>&1 &
  APP=$!
  OK=0
  for t in $(seq 1 30); do
    sleep 1
    grep -q "waiting for run 1" $NAME/console.log && { OK=1; break; }
    kill -0 $APP 2>/dev/null || break
  done
  [ $OK = 1 ] || { tail $NAME/console.log; fatal "app dead"; }
  echo "[stage] T2 (fmap+RSS+CMAC, ~90s)"
  $PC $P 0x1000 w 0x1 >/dev/null; $PC $P 0x2000 w 0x00010001 >/dev/null
  for k in $(seq 0 127); do
    $PC $P $(printf '0x%X' $((0x1400+k*4))) w 0x0 >/dev/null
    $PC $P $(printf '0x%X' $((0x2400+k*4))) w 0x0 >/dev/null
  done
  $PC $P 0x8014 w 0x1 >/dev/null; $PC $P 0x800c w 0x1 >/dev/null
  $PC $P 0xC014 w 0x1 >/dev/null; $PC $P 0xC00c w 0x1 >/dev/null
  $PC $P 0x8204 | grep -q ": 0x00000003" || fatal "CMAC0 down"
  $PC $P 0xC204 | grep -q ": 0x00000003" || fatal "CMAC1 down"
  send_token "READY_$NAME"
  echo "READY_$NAME sent -> waiting for 10 runs..."
  wait $APP
  grep "== run" $NAME/console.log | tail -3
  send_token "DONE_$NAME"
done
echo "########## CAMPAIGN COMPLETE ##########"

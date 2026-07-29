#!/bin/bash
# usage: sudo bash master163_v2.sh [PROGRAMS...]   default: checksum rmvhdr
set -e
cd /users/Pavani
PROGS=${@:-checksum rmvhdr}
for PROG in $PROGS; do
  case $PROG in
    checksum) TAG=CS;;
    rmvhdr)   TAG=RH;;
    fiveTuple) TAG=FT;;
    forward)  TAG=FWD;;
  esac
  echo "#################### FLASH $PROG ####################"
  OK=0
  for TRY in 1 2 3; do
    echo "[flash attempt $TRY]"
    timeout 900 python3 Scripts/flash.py $PROG && { OK=1; break; }
    echo "[flash attempt $TRY failed/hung - retrying]"
    sleep 10
  done
  [ $OK = 1 ] || { echo "FATAL: flash failed 3x for $PROG"; exit 1; }
  echo "#################### CAMPAIGN $PROG ####################"
  bash /users/Pavani/p4lat/campaign163.sh \
    "${TAG}_s1 ${TAG}_s2 ${TAG}_s3 ${TAG}_s4 ${TAG}_s5" \
    /users/Pavani/au280_$PROG
done
echo "#################### ALL PROGRAMS COMPLETE ####################"

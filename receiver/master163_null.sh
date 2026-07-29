#!/bin/bash
# usage: sudo bash master163_null.sh
set -e
cd /users/Pavani
echo "#################### FLASH golden (null) ####################"
OK=0
for TRY in 1 2 3; do
  echo "[flash attempt $TRY]"
  timeout 900 python3 Scripts/flash.py golden && { OK=1; break; }
  echo "[flash attempt $TRY failed/hung - retrying]"; sleep 10
done
[ $OK = 1 ] || { echo "FATAL: golden flash failed 3x"; exit 1; }
for TAG in NF NT NC NR; do
  echo "#################### NULL CAMPAIGN $TAG ####################"
  bash /users/Pavani/p4lat/campaign163.sh \
    "${TAG}_s1 ${TAG}_s2 ${TAG}_s3 ${TAG}_s4 ${TAG}_s5" none
done
echo "#################### NULL ALL COMPLETE ####################"

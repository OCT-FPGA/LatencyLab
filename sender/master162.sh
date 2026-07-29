#!/bin/bash
# usage: sudo bash master162.sh
set -e
declare -A PCAPS=( [CS]=/users/Pavani/checksum_20k.pcap [RH]=/users/Pavani/rmvhdr_20k.pcap )
for TAG in CS RH; do
  echo "#################### SENDER: $TAG ####################"
  bash /users/Pavani/campaign162.sh \
    "${TAG}_s1 ${TAG}_s2 ${TAG}_s3 ${TAG}_s4 ${TAG}_s5" \
    "${PCAPS[$TAG]}"
done
echo "#################### SENDER: ALL COMPLETE ####################"

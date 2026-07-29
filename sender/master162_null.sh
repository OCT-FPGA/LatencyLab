#!/bin/bash
# usage: sudo bash master162_null.sh
set -e
declare -A PCAPS=( [NF]=/users/Pavani/forward_20k_untagged.pcap \
                   [NT]=/users/Pavani/fiveTuple_20k.pcap \
                   [NC]=/users/Pavani/checksum_20k.pcap \
                   [NR]=/users/Pavani/rmvhdr_20k.pcap )
for TAG in NF NT NC NR; do
  echo "#################### SENDER NULL: $TAG ####################"
  bash /users/Pavani/campaign162.sh \
    "${TAG}_s1 ${TAG}_s2 ${TAG}_s3 ${TAG}_s4 ${TAG}_s5" \
    "${PCAPS[$TAG]}"
done
echo "#################### SENDER NULL: ALL COMPLETE ####################"

# Vendor Worst-Case Latency Model

Scripts that compute the vendor-derived worst-case latency bound reported in
the paper (VitisNetP4 "calculated latency" plus insertion penalty). These run
on a development machine with Vivado, not on the testbed nodes.

## Requirements

- Vivado 2023.2 with a VitisNetP4 IP license
- python3 with scapy (pip install scapy)
- tshark (Wireshark CLI)

## Files and flow

- **`mpps.py`** — reads a probe pcap (see `../data/pcap/probes/`) with tshark,
  computes frame-length statistics, and suggests the PKT_RATE (Mpps) value
  to configure the VitisNetP4 IP with.
- **`calc_latency.tcl`** — batch-mode Vivado script. Instantiates the
  VitisNetP4 IP for the target part (xcu280) with the given P4 source,
  packet rate, and bus width, and prints the IP's TOTAL_LATENCY
  (the vendor "calculated latency", in pipeline cycles):

      vivado -mode batch -source calc_latency.tcl -tclargs /abs/path/prog.p4 <PKT_RATE> 512

- **`estimateA.py`** — parses the P4 source and computes the insertion
  penalty: for each non-standard header the program modifies or inserts, the
  extra bus beats needed, ceil(header bits / bus width):

      python3 estimateA.py prog.p4 512

- **`estimateB.py`** — bounds worst-case packets in flight from the
  calculated latency and the minimum packet size in a pcap:

      python3 estimateB.py --pcap probe.pcap --lat <cycles> --bus 512

- **`run_latency.py`** — driver that chains the steps: runs mpps.py for the
  suggested PKT_RATE, then Vivado with calc_latency.tcl for TOTAL_LATENCY.

## Producing the paper's bound

The worst-case bound for a program is the VitisNetP4 TOTAL_LATENCY plus the
insertion penalty from estimateA.py, in cycles of the 250 MHz user-plugin
clock (4 ns per cycle). The P4 sources for the four measured programs are
referenced in [`../p4/README.md`](../p4/README.md).

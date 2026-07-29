# LatencyLab

A DPDK-Based P4 Pipeline Latency Measurement Framework for FPGA SmartNICs

## Overview

LatencyLab measures per-packet processing latency of P4 pipelines running on
FPGA SmartNICs, using two independent techniques: a TSC-based differential
estimator (corrected via null-bitstream calibration) and an independent
hardware-clock validation, obtained by kernel-free reflection of every probe
packet to a ConnectX-5 NIC. Four P4 programs — Forward, FiveTuple,
RemoveHeader, Checksum — were measured across 5 sessions x 10 runs x 20,000
packets each, on the Open Cloud Testbed (OCT) using Xilinx Alveo U280 cards
running Open-NIC-Shell.

## Repository layout

| Path          | Contents                                                       |
|---------------|-----------------------------------------------------------------|
| `p4/`         | Reference to the P4 source (maintained in a separate repo)      |
| `bitstreams/` | Compiled FPGA bitstreams, `.mcs` (Git LFS)                       |
| `receiver/`   | DPDK measurement engine + automation — runs on the FPGA host     |
| `sender/`     | Traffic generation + hardware-clock capture/analysis — sender    |
| `data/`       | Raw and processed measurement data (`tsc/`, `pcap/`, `silicon/`) |

## Testbed

Two nodes on the Open Cloud Testbed (OCT):

- **Receiver node** — hosts the AU280 FPGA SmartNIC; runs the DPDK
  receiver/reflector (see `receiver/README.md`).
- **Sender node** — sends probe traffic via `tcpreplay` and independently
  captures reflected packets with hardware timestamps (see `sender/README.md`).

## Reproducing the measurements

1. **Get the P4 source and bitstreams** — see [`p4/README.md`](p4/README.md)
   and [`bitstreams/README.md`](bitstreams/README.md).
2. **Run a measurement campaign** — see [`receiver/README.md`](receiver/README.md)
   and [`sender/README.md`](sender/README.md). The two sides run in
   coordination: the sender waits for a per-session ready signal from the
   receiver before capturing.
3. **Resulting data** lands under `data/` — see [`data/README.md`](data/README.md)
   for the exact layout and file formats.

## Citation

TODO: add the paper citation once published.

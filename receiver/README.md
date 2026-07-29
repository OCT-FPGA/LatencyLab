# Receiver (pc163 — FPGA / DPDK measurement node)

This directory holds the DPDK measurement engine and the automation that
flashes bitstreams and drives measurement sessions on the FPGA host.

## Files

- **`p4lat_r2.c`** — the measurement engine. A DPDK application that
  TSC-timestamps every packet arriving on the two FPGA physical functions
  (P4-pipeline port and bypass port), tags each one 'P' or 'B', and reflects
  a copy out through a third port (the host's ConnectX-5) back to the sender
  — this is what feeds the independent hardware-clock validation.
- **`Makefile`** — builds `p4lat_r2.c` into the `p4lat_r2` binary (gitignored;
  rebuild locally, don't expect it to be in the repo).
- **`campaign163.sh`** — top-level orchestrator; loops over all four programs
  and all 5 sessions per program.
- **`master163_v2.sh`** — per-program driver: flashes the given bitstream
  (retries up to 3 times, since flashing can hang) then runs the sessions.
  Usage: `sudo bash master163_v2.sh [PROGRAMS...]` (default: `checksum rmvhdr`
  if no arguments given; also accepts `fiveTuple`, `forward`).
- **`master163_null.sh`** — same idea, but for the null/golden bitstream,
  producing the `NC`/`NF`/`NR`/`NT` calibration session data.

## Dependencies (not vendored in this repo)

- DPDK 20.11
- Xilinx DMA IP drivers, NICs bound to `vfio-pci`
- Open-NIC-Shell driver stack (QDMA + CMAC subsystems)

## Build

```bash
cd receiver
make
```

## Run

Requires root, hugepages configured, and 3 ports bound to DPDK (2 FPGA
physical functions + the host's ConnectX-5), across at least 3 lcores:

```bash
sudo ./p4lat_r2 -p <prefix> -r <runs> --dst-mac <pc162's ConnectX-5 MAC address>
```

`-r` defaults to 10 runs per session. In practice this binary is not run by
hand — `campaign163.sh` / `master163_v2.sh` / `master163_null.sh` launch it
automatically as part of a session.

Output: one CSV per run, written into the matching session directory under
[`../data/tsc/`](../data/tsc/).

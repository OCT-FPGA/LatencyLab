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
- **`Scripts/flash.py`** — flashes a bitstream onto the AU280 and brings it
  up for measurement. See Flashing below.

## Dependencies (not vendored in this repo)

See [`SETUP.md`](SETUP.md) for the full from-scratch bring-up (kernel prep, DPDK build, "the dance").

- DPDK 20.11
- Xilinx DMA IP drivers, NICs bound to `vfio-pci`
- Xilinx Runtime (XRT) — provides `config-fpga` and `xbflash2`, used by
  `Scripts/flash.py`
- Open-NIC driver + shell source —
  https://github.com/OCT-FPGA/P4OpenNIC_Public

## Build

    cd receiver
    make

## Flashing (Scripts/flash.py)

`sudo python3 Scripts/flash.py <name>` performs the full bring-up sequence
for one program:

1. Resets the FPGA (`config-fpga reset`), authenticated with
   `private_key.pem` — not included in this repo (see root `.gitignore`);
   generate/obtain your own key for your FPGA card.
2. Flashes the bitstream via `xbflash2 program --spi` at a fixed PCIe
   address.
3. Boots the FPGA (`config-fpga boot`).
4. Rebuilds and reloads the ONIC kernel driver from
   `opennic/opennic-scripts/open-nic-driver`.
5. Brings the FPGA's two kernel-visible interfaces up.
6. Builds and runs the per-program `driver` binary from
   `au280_<name>/drivers/install` to load the P4 match-action tables
   (skipped in `golden` mode, used for the null/calibration bitstream).

Expected directory layout for a program named NAME:

    ~/au280_NAME/NAME.mcs
    ~/au280_NAME/drivers/install/

`sudo python3 Scripts/flash.py golden` flashes `golden_nic.mcs` (found via
upward search from the current directory) and brings interfaces up, but
skips table loading.

## Configuration required on new hardware

This code was written for one specific testbed pairing, with some values
hardcoded that must change on different hardware:

- FPGA PCIe address — set in `Scripts/flash.py` (`xbflash2 -d`), currently
  `3b:00.0`. Find yours with `lspci | grep Xilinx`.
- FPGA kernel interfaces — set in `Scripts/flash.py`, currently
  `enp59s0f0` and `enp59s0f1`. Find yours with `ip link` after the ONIC
  driver loads.
- ConnectX-5 sender interface — set in `../sender/campaign162.sh`,
  currently `enp134s0np0`. Find yours with `ip link` on the sender node.
- Reflection target MAC — passed to `p4lat_r2 --dst-mac`; this is the
  sender's ConnectX-5 MAC address. Find it with `ip link show <iface>` on
  the sender node.

## Run

Requires root, hugepages configured, and 3 ports bound to DPDK (2 FPGA
physical functions + the host's ConnectX-5), across at least 3 lcores:

    sudo ./p4lat_r2 -p <prefix> -r <runs> --dst-mac <sender's ConnectX-5 MAC address>

`-r` defaults to 10 runs per session. In practice this binary is not run by
hand — `campaign163.sh` / `master163_v2.sh` / `master163_null.sh` launch it
automatically as part of a session.

Output: one CSV per run, written into the matching session directory under
../data/tsc/.

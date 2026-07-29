# Sender (pc162 — sender / hardware-timestamp capture node)

This directory holds the scripts that fire probe traffic at the FPGA and
capture the reflected packets with hardware timestamps, plus the analysis
that turns those captures into latency numbers.

## Files

- **`campaign162.sh`** — per-session capture driver. Waits for a
  `READY_<session>` signal from pc163 over a raw socket (keeps both machines
  in lockstep), then starts `tcpdump` on the ConnectX-5 with nanosecond
  hardware-clock precision (`--time-stamp-precision=nano`), capturing only
  packets reflected back to its own MAC, saved as `refl_<session>.pcap`.
  Usage: `sudo bash campaign162.sh "<session names>" <probe pcap path>`.
- **`master162.sh`** — runs `campaign162.sh` for Checksum and RemoveHeader
  (`CS`, `RH`), 5 sessions each.
- **`master162_null.sh`** — same pattern for the null/calibration conditions
  (`NF`, `NT`, `NC`, `NR`).
- **`cx5_analyze.py`** — parses one `refl_*.pcap`, matches the 'P'/'B' tagged
  packet copies by embedded ID, and computes the hardware-clock latency
  difference between them.
- **`cx5_perid_all.py`** — same extraction, but writes every individual
  packet's diff to `silicon_perid_all.csv` instead of aggregating (used for
  the FiveTuple per-packet case study).

## Dependencies (not vendored in this repo)

- `tcpdump` built with hardware-timestamp support (`adapter_unsynced` mode)
- `tcpreplay`
- ConnectX-5 (or equivalent hardware-timestamping NIC) with `PTP`/`HWTSTAMP`
  support enabled

## Output

`refl_*.pcap` → [`../data/pcap/reflected/`](../data/pcap/reflected/)
`silicon_*.csv` → [`../data/silicon/`](../data/silicon/)

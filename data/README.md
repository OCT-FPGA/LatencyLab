# Data

Three categories of measurement data, from the two testbed nodes.

## `tsc/` — FPGA-side TSC timestamps (pc163)

- **`tsc/programs/`** — the four measured programs (`CS`, `FT`, `FWD`, `RH`),
  5 sessions each (`_s1`–`_s5`), 10 runs per session.
- **`tsc/null/`** — the matching null/calibration condition for each program
  (`NC`=null-Checksum, `NT`=null-FiveTuple, `NF`=null-Forward,
  `NR`=null-RemoveHeader), same 5-session/10-run structure. Used to compute
  the port-asymmetry offset (δ) that the differential estimator corrects for.

Each `f_run##.csv` has the header:
`pkt_id,tx_ts_ns,rx1_ts_ns,rx2_ts_ns,latency_rx1_ns,latency_rx2_ns,diff_latency_ns`
(written by [`../receiver/p4lat_r2.c`](../receiver/p4lat_r2.c)).

## `pcap/` — packet captures (pc162)

- **`pcap/probes/`** — the 5 input probe traces replayed via `tcpreplay`
  (one per program, 20k packets each).
- **`pcap/reflected/`** — `refl_<session>.pcap`, the hardware-clock captures
  of packets reflected back from pc163, one per program-session (40 files).
  Known gap: `refl_FT_s5.pcap` is missing (confirmed, not a data-loss bug).

## `silicon/` — hardware-clock latency results (pc162)

`silicon_<program>_s<n>.csv`, computed from the matching `reflected/` pcap by
[`../sender/cx5_analyze.py`](../sender/cx5_analyze.py) — this is the
independent hardware-clock validation of the TSC-based `tsc/` measurements.
Known gap: `silicon_FT_s5.csv` is missing (same known issue as above).

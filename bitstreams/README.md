# Bitstreams

Five FPGA configuration files for the Xilinx Alveo U280 SmartNIC, in Intel HEX
(`.mcs`) format. Tracked via Git LFS — clone with `git lfs pull` if these come
down as pointer files.

| File             | Program / Condition       |
|------------------|----------------------------|
| `forward.mcs`    | Forward                    |
| `fiveTuple.mcs`  | FiveTuple                   |
| `checksum.mcs`   | Checksum                    |
| `rmvhdr.mcs`     | RemoveHeader                |
| `golden_nic.mcs` | Null / plain passthrough shell — no P4 program loaded, used to measure port asymmetry (δ) for calibration |

Each program bitstream was built from the P4 source referenced in
[`../p4/README.md`](../p4/README.md), on top of the Open-NIC-Shell platform
(QDMA subsystem, CMAC subsystem, two physical functions). Open-NIC-Shell
itself is not vendored in this repo — see the upstream project for the base
shell source.


To flash a bitstream onto the AU280, see [`../receiver/README.md`](../receiver/README.md).
